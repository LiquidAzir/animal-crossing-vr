#!/usr/bin/env python3
"""gen_runtime_assets.py - Convert .inc #includes to runtime-loaded binary assets.

Scans source files for #include "assets/*.inc" patterns, rewrites them with
#ifdef TARGET_PC sized-array declarations, and generates pc/src/pc_assets.c
loader + pc/include/pc_assets.h header.

Supports two loading modes:
  1. ROM-direct: reads from DOL + REL files (orig/GAFE01_00/)
  2. .bin fallback: reads from extracted .bin files (assets/)

Binary data from GC ROM is big-endian. After loading on LE PC,
multi-byte fields need byte-swapping:
  - u8/unsigned char: no swap
  - u16/unsigned short: swap each u16
  - Vtx: swap s16/u16 fields (first 12 bytes of each 16-byte vertex)

Usage:
    python gen_runtime_assets.py             # full run
    python gen_runtime_assets.py --scan-only # just report stats
    python gen_runtime_assets.py --dry-run   # show changes without writing
"""

import re
import os
import sys
import struct
import shutil
from pathlib import Path
from collections import defaultdict

DECOMP_ROOT = Path(__file__).resolve().parent.parent.parent  # ac-decomp/
BIN_DIR = DECOMP_ROOT / "build" / "GAFE01_00" / "bin"
DOL_PATH = DECOMP_ROOT / "orig" / "GAFE01_00" / "sys" / "main.dol"
REL_PATH = DECOMP_ROOT / "orig" / "GAFE01_00" / "files" / "foresta.rel.szs"
FORESTA_SYMBOLS = DECOMP_ROOT / "config" / "GAFE01_00" / "foresta" / "symbols.txt"
DOL_SYMBOLS = DECOMP_ROOT / "config" / "GAFE01_00" / "symbols.txt"
OUTPUT_ASSETS_C = DECOMP_ROOT / "pc" / "src" / "pc_assets.c"
OUTPUT_ASSETS_H = DECOMP_ROOT / "pc" / "include" / "pc_assets.h"
RUNTIME_BIN_DIR = DECOMP_ROOT / "pc" / "build32" / "bin" / "assets"

SCAN_DIRS = [DECOMP_ROOT / "src"]

ARRAY_INC_RE = re.compile(
    r'([ \t]*)'
    r'((?:static|extern)\s+)?'
    r'((?:unsigned\s+(?:char|short|int|long))|(?:[A-Za-z_]\w*))'
    r'(\s+)'
    r'(\w+)'
    r'(\s*\[\s*(?:\w+)?\s*\])'
    r'(\s*(?:ATTRIBUTE_ALIGN\(\d+\)|__attribute__\s*\(\(aligned\(\d+\)\)\))\s*)?'
    r'(\s*=\s*\{[^\n]*)\n'
    r'([ \t]*)#include\s+"(assets/[^"]+\.inc)"\s*\n'
    r'([ \t]*\};)',
    re.MULTILINE
)

TYPE_NORMALIZE = {
    'unsigned char': 'u8', 'unsigned short': 'u16',
    'unsigned int': 'u32', 'unsigned long': 'u32', 'char': 'u8',
}

# Asset swap types
SWAP_NONE = 0
SWAP_U16 = 1
SWAP_VTX = 2
SWAP_U32 = 3

# ROM sources
SRC_REL = 0
SRC_DOL = 1
SRC_NONE = 2  # .bin fallback only


def get_swap_type(elem_type):
    norm = TYPE_NORMALIZE.get(elem_type, elem_type)
    if norm in ('u8', 's8', 'char'): return SWAP_NONE
    elif norm in ('u16', 's16'): return SWAP_U16
    elif norm == 'Vtx': return SWAP_VTX
    elif norm in ('u32', 's32'): return SWAP_U32
    return SWAP_NONE


def is_function_local(indent_str):
    return len(indent_str.replace('\t', '    ')) >= 4


# --- DOL/REL offset computation ---

def parse_dol_header(dol_path):
    """Parse DOL header → list of (file_offset, load_addr, size) for all sections."""
    with open(dol_path, 'rb') as f:
        data = f.read(0x100)
    text_offs = struct.unpack('>7I', data[0x00:0x1C])
    data_offs = struct.unpack('>11I', data[0x1C:0x48])
    text_addrs = struct.unpack('>7I', data[0x48:0x64])
    data_addrs = struct.unpack('>11I', data[0x64:0x90])
    text_sizes = struct.unpack('>7I', data[0x90:0xAC])
    data_sizes = struct.unpack('>11I', data[0xAC:0xD8])

    sections = []
    for i in range(7):
        if text_sizes[i] > 0:
            sections.append((text_offs[i], text_addrs[i], text_sizes[i], 'text'))
    for i in range(11):
        if data_sizes[i] > 0:
            sections.append((data_offs[i], data_addrs[i], data_sizes[i], 'data'))
    return sections


def parse_rel_header(rel_path):
    """Parse REL header → list of (file_offset, size) per section index."""
    with open(rel_path, 'rb') as f:
        data = f.read(0x200)  # enough for header + section table
    num_sections = struct.unpack('>I', data[0x0C:0x10])[0]
    sec_info_off = struct.unpack('>I', data[0x10:0x14])[0]

    sections = []
    for i in range(num_sections):
        off = sec_info_off + i * 8
        sec_offset = struct.unpack('>I', data[off:off+4])[0]
        sec_size = struct.unpack('>I', data[off+4:off+8])[0]
        sec_offset &= ~1  # clear exec flag
        sections.append((sec_offset, sec_size))
    return sections


def parse_symbols_with_section(path):
    """Parse symbols.txt → {name: (section_name, offset, size)}."""
    symbols = {}
    if not path.exists():
        return symbols
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = re.match(
                r'(\w+)\s*=\s*\.(\w+):(0x[0-9a-fA-F]+);\s*//.*?size:(0x[0-9a-fA-F]+)',
                line
            )
            if m:
                symbols[m.group(1)] = (
                    m.group(2),  # section name
                    int(m.group(3), 16),  # offset/address
                    int(m.group(4), 16),  # size
                )
    return symbols


def compute_dol_file_offset(va, dol_sections):
    """Convert a virtual address to DOL file offset."""
    for file_off, load_addr, size, kind in dol_sections:
        if load_addr <= va < load_addr + size:
            return file_off + (va - load_addr)
    return None


def build_rom_offset_table(foresta_syms, dol_syms, rel_sections, dol_sections):
    """Build {symbol_name: (rom_source, file_offset)} mapping."""
    # REL section mapping: .text=1, .ctors=2, .dtors=3, .rodata=4, .data=5, .bss=6
    REL_SECTION_MAP = {'text': 1, 'ctors': 2, 'dtors': 3, 'rodata': 4, 'data': 5, 'bss': 6}

    offsets = {}

    # REL symbols (section-relative offsets)
    for name, (sec_name, sec_offset, size) in foresta_syms.items():
        sec_idx = REL_SECTION_MAP.get(sec_name)
        if sec_idx is not None and sec_idx < len(rel_sections):
            rel_sec_file_off, rel_sec_size = rel_sections[sec_idx]
            if rel_sec_file_off > 0:
                offsets[name] = (SRC_REL, rel_sec_file_off + sec_offset)

    # DOL symbols (virtual addresses)
    for name, (sec_name, va, size) in dol_syms.items():
        if name not in offsets:  # REL takes priority
            file_off = compute_dol_file_offset(va, dol_sections)
            if file_off is not None:
                offsets[name] = (SRC_DOL, file_off)

    return offsets


# --- Source scanning and rewriting ---

def scan_files(scan_dirs):
    files = []
    for scan_dir in scan_dirs:
        for root, dirs, fnames in os.walk(scan_dir):
            for fname in fnames:
                if fname.endswith(('.c', '.cpp', '.c_inc')):
                    fpath = Path(root) / fname
                    try:
                        content = fpath.read_text(encoding='utf-8', errors='replace')
                    except Exception:
                        continue
                    if '.inc"' in content:
                        matches = list(ARRAY_INC_RE.finditer(content))
                        if matches:
                            files.append((fpath, content, matches))
    return files


def get_bin_path(inc_path):
    return BIN_DIR / inc_path.replace('.inc', '.bin')


def get_runtime_bin_path(inc_path):
    return inc_path.replace('.inc', '.bin')


def sanitize_func_name(path):
    rel = path.relative_to(DECOMP_ROOT)
    name = str(rel).replace('\\', '_').replace('/', '_').replace('.', '_').replace('-', '_')
    return f'_pc_load_{name}'


def process_all(scan_dirs, dry_run=False, scan_only=False):
    # Parse symbol tables
    foresta_syms = parse_symbols_with_section(FORESTA_SYMBOLS)
    dol_syms = parse_symbols_with_section(DOL_SYMBOLS)
    print(f"Loaded {len(foresta_syms)} foresta symbols, {len(dol_syms)} DOL symbols")

    # Parse DOL/REL headers for file offsets
    rom_offsets = {}
    if DOL_PATH.exists() and REL_PATH.exists():
        dol_sections = parse_dol_header(DOL_PATH)
        rel_sections = parse_rel_header(REL_PATH)
        rom_offsets = build_rom_offset_table(foresta_syms, dol_syms, rel_sections, dol_sections)
        print(f"Computed {len(rom_offsets)} ROM file offsets")
    else:
        print("WARNING: DOL/REL not found, ROM-direct loading disabled")

    # Build unified size table
    all_syms = {}
    for name, (sec, off, size) in foresta_syms.items():
        all_syms[name] = size
    for name, (sec, off, size) in dol_syms.items():
        if name not in all_syms:
            all_syms[name] = size

    # Scan source files
    files = scan_files(scan_dirs)

    all_entries = []
    static_files = {}
    nonstatic_entries = []
    func_local_entries = []
    skipped = []
    name_counts = defaultdict(int)

    for fpath, content, matches in files:
        for m in matches:
            storage = (m.group(2) or '').strip()
            elem_type = m.group(3).strip()
            name = m.group(5)
            align_str = (m.group(7) or '').strip()
            inc_path = m.group(10)
            is_static = (storage == 'static')
            is_func_local = is_static and is_function_local(m.group(1))

            byte_size = all_syms.get(name)
            if byte_size is None:
                bin_path = get_bin_path(inc_path)
                if bin_path.exists():
                    byte_size = bin_path.stat().st_size
            if byte_size is None or byte_size == 0:
                skipped.append((fpath, name, inc_path, 'no size'))
                continue

            rom_src, rom_off = rom_offsets.get(name, (SRC_NONE, 0))

            entry = {
                'file': fpath, 'name': name, 'type': elem_type,
                'byte_size': byte_size, 'storage': storage,
                'inc_path': inc_path, 'align': align_str,
                'is_static': is_static, 'is_func_local': is_func_local,
                'match': m, 'swap_type': get_swap_type(elem_type),
                'rom_src': rom_src, 'rom_off': rom_off,
            }
            all_entries.append(entry)

            if is_func_local:
                func_local_entries.append(entry)
            elif is_static:
                static_files.setdefault(str(fpath), []).append(entry)
            else:
                nonstatic_entries.append(entry)
                name_counts[name] += 1

    dupes = {n: c for n, c in name_counts.items() if c > 1}

    # Report
    rom_count = sum(1 for e in all_entries if e['rom_src'] != SRC_NONE)
    bin_only = sum(1 for e in all_entries if e['rom_src'] == SRC_NONE)
    type_counts = defaultdict(int)
    for e in all_entries:
        type_counts[e['swap_type']] += 1

    print(f"\n=== Runtime Asset Scan ===")
    print(f"Files with .inc includes: {len(files)}")
    print(f"Total .inc arrays found:  {len(all_entries)}")
    print(f"  Non-static:             {len(nonstatic_entries)}")
    print(f"  File-scope static:      {len(all_entries) - len(nonstatic_entries) - len(func_local_entries)}")
    print(f"  Function-local static:  {len(func_local_entries)}")
    print(f"  Skipped (no size):      {len(skipped)}")
    print(f"ROM-direct capable:       {rom_count}")
    print(f".bin fallback only:       {bin_only}")
    print(f"By swap type: u8={type_counts[0]} u16={type_counts[1]} Vtx={type_counts[2]} u32={type_counts[3]}")

    total_bytes = sum(e['byte_size'] for e in all_entries)
    print(f"Total asset data: {total_bytes:,} bytes ({total_bytes/1048576:.1f} MB)")

    if scan_only:
        return

    # --- Modify source files ---
    modified_files = set()
    for fpath, content, matches in files:
        file_entries = [e for e in all_entries if e['file'] == fpath]
        if not file_entries:
            continue

        new_content = content
        for entry in sorted(file_entries, key=lambda e: e['match'].start(), reverse=True):
            m = entry['match']
            old_text = m.group(0)
            indent = m.group(1)
            elem_type = entry['type']
            name = entry['name']
            align = entry['align']
            byte_size = entry['byte_size']

            norm_type = TYPE_NORMALIZE.get(elem_type, elem_type)
            if norm_type in ('u8', 'char', 's8'):
                size_expr = f'0x{byte_size:X}'
            else:
                size_expr = f'0x{byte_size:X} / sizeof({elem_type})'

            pc_storage = 'static ' if entry['is_static'] else ''
            align_str = f' {align}' if align else ''

            if entry['is_func_local']:
                bin_path = get_runtime_bin_path(entry['inc_path'])
                pc_decl = f'{pc_storage}{elem_type} {name}[{size_expr}]{align_str};'
                lazy_lines = [
                    f'{indent}#ifdef TARGET_PC',
                    f'{indent}{pc_decl}',
                    f'{indent}static int {name}_loaded = 0;',
                    f'{indent}if (!{name}_loaded) {{',
                    f'{indent}    extern void pc_load_asset(const char*, void*, unsigned int, unsigned int, int, int);',
                    f'{indent}    pc_load_asset("{bin_path}", {name}, 0x{byte_size:X}, 0x{entry["rom_off"]:X}, {entry["rom_src"]}, {entry["swap_type"]});',
                    f'{indent}    {name}_loaded = 1;',
                    f'{indent}}}',
                    f'{indent}#else',
                    f'{indent}{old_text}',
                    f'{indent}#endif',
                ]
                replacement = '\n'.join(lazy_lines)
            else:
                pc_decl = f'{pc_storage}{elem_type} {name}[{size_expr}]{align_str};'
                replacement = (
                    f'{indent}#ifdef TARGET_PC\n'
                    f'{indent}{pc_decl}\n'
                    f'{indent}#else\n'
                    f'{indent}{old_text}\n'
                    f'{indent}#endif'
                )

            new_content = new_content[:m.start()] + replacement + new_content[m.end():]

        # Per-file init function for file-scope statics
        file_statics = [e for e in file_entries if e['is_static'] and not e['is_func_local']]
        if file_statics:
            func_name = sanitize_func_name(fpath)
            init_lines = [f'\n#ifdef TARGET_PC']
            init_lines.append(f'extern void pc_load_asset(const char*, void*, unsigned int, unsigned int, int, int);')
            init_lines.append(f'void {func_name}(void) {{')
            for entry in file_statics:
                bin_path = get_runtime_bin_path(entry['inc_path'])
                init_lines.append(
                    f'    pc_load_asset("{bin_path}", {entry["name"]}, '
                    f'0x{entry["byte_size"]:X}, 0x{entry["rom_off"]:X}, {entry["rom_src"]}, {entry["swap_type"]});'
                )
            init_lines.append(f'}}')
            init_lines.append(f'#endif')
            init_lines.append('')
            new_content += '\n'.join(init_lines)

        if new_content != content:
            modified_files.add(fpath)
            if not dry_run:
                fpath.write_text(new_content, encoding='utf-8')
            else:
                print(f"  [DRY-RUN] Would modify: {fpath.relative_to(DECOMP_ROOT)}")

    # Deduplicate central entries
    seen = set()
    central_entries = []
    for entry in nonstatic_entries:
        if entry['name'] not in seen:
            central_entries.append(entry)
            seen.add(entry['name'])

    init_func_names = [sanitize_func_name(Path(fp)) for fp in static_files]

    generate_pc_assets_c(central_entries, init_func_names, dry_run)
    generate_pc_assets_h(dry_run)

    # Copy .bin files
    copy_count = 0
    for entry in all_entries:
        src_bin = get_bin_path(entry['inc_path'])
        dst_bin = RUNTIME_BIN_DIR / get_runtime_bin_path(entry['inc_path']).replace('assets/', '')
        if src_bin.exists():
            if not dry_run:
                dst_bin.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src_bin, dst_bin)
            copy_count += 1

    print(f"\n=== Results ===")
    print(f"Source files modified: {len(modified_files)}")
    print(f"Central loader entries: {len(central_entries)}")
    print(f"Per-file init functions: {len(init_func_names)}")
    print(f"Function-local lazy-loads: {len(func_local_entries)}")
    print(f"Bin files {'would copy' if dry_run else 'copied'}: {copy_count}")


def generate_pc_assets_c(central_entries, init_func_names, dry_run):
    lines = []
    lines.append('/* pc_assets.c - Runtime binary asset loader (auto-generated) */')
    lines.append('#ifdef TARGET_PC')
    lines.append('#include <stdio.h>')
    lines.append('#include <stdlib.h>')
    lines.append('#include <string.h>')
    lines.append('#include "types.h"')
    lines.append('#include "PR/gbi.h"')
    lines.append('#include "pc_assets.h"')
    lines.append('')
    lines.append('extern int g_pc_verbose;')
    lines.append('')

    # ROM source enum
    lines.append('enum { SRC_REL = 0, SRC_DOL = 1, SRC_NONE = 2 };')
    lines.append('enum { SWAP_NONE = 0, SWAP_U16 = 1, SWAP_VTX = 2, SWAP_U32 = 3 };')
    lines.append('')

    # Global ROM data buffers
    lines.append('static u8* g_rel_data = NULL;')
    lines.append('static u8* g_dol_data = NULL;')
    lines.append('')

    # Byte-swap functions
    lines.append('void pc_bswap_asset_u16(void* data, unsigned int size) {')
    lines.append('    u16* p = (u16*)data;')
    lines.append('    unsigned int i, count = size / 2;')
    lines.append('    for (i = 0; i < count; i++) p[i] = (u16)((p[i] >> 8) | (p[i] << 8));')
    lines.append('}')
    lines.append('')
    lines.append('void pc_bswap_asset_u32(void* data, unsigned int size) {')
    lines.append('    u32* p = (u32*)data;')
    lines.append('    unsigned int i, count = size / 4;')
    lines.append('    for (i = 0; i < count; i++)')
    lines.append('        p[i] = ((p[i]>>24)&0xFF)|((p[i]>>8)&0xFF00)|((p[i]<<8)&0xFF0000)|((p[i]<<24)&0xFF000000);')
    lines.append('}')
    lines.append('')
    lines.append('void pc_bswap_asset_vtx(void* data, unsigned int size) {')
    lines.append('    u8* p = (u8*)data;')
    lines.append('    unsigned int i, count = size / 16;')
    lines.append('    for (i = 0; i < count; i++) {')
    lines.append('        int j;')
    lines.append('        for (j = 0; j < 12; j += 2) { u8 t = p[j]; p[j] = p[j+1]; p[j+1] = t; }')
    lines.append('        p += 16;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # Swap dispatch
    lines.append('static void do_swap(void* data, unsigned int size, int type) {')
    lines.append('    switch (type) {')
    lines.append('        case SWAP_U16: pc_bswap_asset_u16(data, size); break;')
    lines.append('        case SWAP_VTX: pc_bswap_asset_vtx(data, size); break;')
    lines.append('        case SWAP_U32: pc_bswap_asset_u32(data, size); break;')
    lines.append('        default: break;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # Load a file into malloc'd buffer
    lines.append('static u8* load_file(const char* path, unsigned int* out_size) {')
    lines.append('    FILE* f = fopen(path, "rb");')
    lines.append('    unsigned int sz;')
    lines.append('    u8* buf;')
    lines.append('    if (!f) return NULL;')
    lines.append('    fseek(f, 0, SEEK_END); sz = (unsigned int)ftell(f); fseek(f, 0, SEEK_SET);')
    lines.append('    buf = (u8*)malloc(sz);')
    lines.append('    if (buf) fread(buf, 1, sz, f);')
    lines.append('    fclose(f);')
    lines.append('    if (out_size) *out_size = sz;')
    lines.append('    return buf;')
    lines.append('}')
    lines.append('')

    # Unified load function (ROM-direct + .bin fallback)
    lines.append('void pc_load_asset(const char* bin_path, void* dest, unsigned int size,')
    lines.append('                   unsigned int rom_off, int rom_src, int swap_type) {')
    lines.append('    int loaded = 0;')
    lines.append('    /* Try ROM-direct first */')
    lines.append('    if (rom_src != SRC_NONE) {')
    lines.append('        u8* rom = (rom_src == SRC_REL) ? g_rel_data : g_dol_data;')
    lines.append('        if (rom) { memcpy(dest, rom + rom_off, size); loaded = 1; }')
    lines.append('    }')
    lines.append('    /* Fallback to .bin file */')
    lines.append('    if (!loaded && bin_path) {')
    lines.append('        FILE* f = fopen(bin_path, "rb");')
    lines.append('        if (f) { fread(dest, 1, size, f); fclose(f); loaded = 1; }')
    lines.append('    }')
    lines.append('    if (!loaded) fprintf(stderr, "[PC] ASSET MISSING: %s\\n", bin_path ? bin_path : "(rom)");')
    lines.append('    if (loaded) do_swap(dest, size, swap_type);')
    lines.append('}')
    lines.append('')

    # Extern declarations
    by_type = defaultdict(list)
    for entry in central_entries:
        norm_type = TYPE_NORMALIZE.get(entry['type'], entry['type'])
        by_type[norm_type].append(entry['name'])

    lines.append('/* Extern declarations */')
    for typ in sorted(by_type.keys()):
        for name in sorted(by_type[typ]):
            lines.append(f'extern {typ} {name}[];')
    lines.append('')

    # Asset table with ROM offsets
    lines.append('typedef struct { const char* path; void* dest; unsigned int size; unsigned int rom_off; int rom_src; int swap; } PCAsset;')
    lines.append('static const PCAsset s_assets[] = {')
    for entry in central_entries:
        bin_path = get_runtime_bin_path(entry['inc_path'])
        lines.append(
            f'    {{"{bin_path}", {entry["name"]}, 0x{entry["byte_size"]:X}, '
            f'0x{entry["rom_off"]:X}, {entry["rom_src"]}, {entry["swap_type"]}}},')
    lines.append('};')
    lines.append('')

    # Per-file init declarations
    if init_func_names:
        lines.append('/* Per-file init functions for static arrays */')
        for func_name in sorted(init_func_names):
            lines.append(f'extern void {func_name}(void);')
        lines.append('')

    # pc_assets_init
    lines.append('void pc_assets_init(void) {')
    lines.append('    int i, loaded = 0, failed = 0, rom_mode = 0;')
    lines.append('    int total = (int)(sizeof(s_assets) / sizeof(s_assets[0]));')
    lines.append('')
    lines.append('    /* Try loading ROM files into memory */')
    lines.append('    g_rel_data = load_file("orig/GAFE01_00/files/foresta.rel.szs", NULL);')
    lines.append('    g_dol_data = load_file("orig/GAFE01_00/sys/main.dol", NULL);')
    lines.append('    if (g_rel_data && g_dol_data) {')
    lines.append('        rom_mode = 1;')
    lines.append('        if (g_pc_verbose) printf("[PC] ROM-direct mode: loaded DOL + REL\\n");')
    lines.append('    } else {')
    lines.append('        if (g_pc_verbose) printf("[PC] .bin fallback mode\\n");')
    lines.append('        if (g_rel_data) { free(g_rel_data); g_rel_data = NULL; }')
    lines.append('        if (g_dol_data) { free(g_dol_data); g_dol_data = NULL; }')
    lines.append('    }')
    lines.append('')
    lines.append('    /* Load all central-table assets */')
    lines.append('    for (i = 0; i < total; i++) {')
    lines.append('        pc_load_asset(s_assets[i].path, s_assets[i].dest, s_assets[i].size,')
    lines.append('                      s_assets[i].rom_off, s_assets[i].rom_src, s_assets[i].swap);')
    lines.append('        loaded++;')
    lines.append('    }')
    lines.append('')

    if init_func_names:
        lines.append('    /* Load static arrays via per-file init functions */')
        for func_name in sorted(init_func_names):
            lines.append(f'    {func_name}();')
        lines.append('')

    lines.append('    /* Free ROM data */')
    lines.append('    if (g_rel_data) { free(g_rel_data); g_rel_data = NULL; }')
    lines.append('    if (g_dol_data) { free(g_dol_data); g_dol_data = NULL; }')
    lines.append('')
    lines.append('    if (g_pc_verbose)')
    lines.append('        printf("[PC] Assets: %d loaded (%s)\\n", loaded, rom_mode ? "ROM-direct" : ".bin fallback");')
    lines.append('}')
    lines.append('')
    lines.append('#endif /* TARGET_PC */')
    lines.append('')

    content = '\n'.join(lines)
    if not dry_run:
        OUTPUT_ASSETS_C.write_text(content, encoding='utf-8')
        print(f"Generated: {OUTPUT_ASSETS_C.relative_to(DECOMP_ROOT)}")
    else:
        print(f"[DRY-RUN] Would generate: {OUTPUT_ASSETS_C.relative_to(DECOMP_ROOT)} ({len(central_entries)} entries)")


def generate_pc_assets_h(dry_run):
    content = (
        '/* pc_assets.h - Runtime binary asset loader (auto-generated) */\n'
        '#ifndef PC_ASSETS_H\n'
        '#define PC_ASSETS_H\n'
        '\n'
        'void pc_load_asset(const char* bin_path, void* dest, unsigned int size,\n'
        '                   unsigned int rom_off, int rom_src, int swap_type);\n'
        'void pc_bswap_asset_u16(void* data, unsigned int size);\n'
        'void pc_bswap_asset_u32(void* data, unsigned int size);\n'
        'void pc_bswap_asset_vtx(void* data, unsigned int size);\n'
        'void pc_assets_init(void);\n'
        '\n'
        '#endif /* PC_ASSETS_H */\n'
    )
    if not dry_run:
        OUTPUT_ASSETS_H.write_text(content, encoding='utf-8')
        print(f"Generated: {OUTPUT_ASSETS_H.relative_to(DECOMP_ROOT)}")
    else:
        print(f"[DRY-RUN] Would generate: {OUTPUT_ASSETS_H.relative_to(DECOMP_ROOT)}")


def main():
    dry_run = '--dry-run' in sys.argv
    scan_only = '--scan-only' in sys.argv

    print(f"Decomp root: {DECOMP_ROOT}")
    print()
    process_all(SCAN_DIRS, dry_run=dry_run, scan_only=scan_only)


if __name__ == '__main__':
    main()
