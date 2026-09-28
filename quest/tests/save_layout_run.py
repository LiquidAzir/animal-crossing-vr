"""Compare actual save headers on MinGW32 and Android ARM32; never opens saves."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess

SOURCE = Path(__file__).resolve().parents[2]
QUEST = SOURCE.parent
PC_REFERENCE = QUEST.parent / 'Animal Crossing VR' / 'animal-crossing-vr-rewrite'
OUT = QUEST / 'research' / 'save-layout'
TESTS = Path(__file__).resolve().parent
PATHS = json.loads((QUEST / 'toolchain' / 'paths.json').read_text(encoding='utf-8-sig'))
DEFINES = ['-DTARGET_PC', '-DVERSION=0', '-DF3DEX_GBI_2', '-D_LANGUAGE_C', '-DNDEBUG']
INCLUDES = ['-I' + str(SOURCE / p) for p in ('include', 'pc/include', 'src', '.')]

def run(command, name, env=None):
    if env is None:
        env = dict(os.environ)
        env['PATH'] = r'C:\msys64\mingw32\bin' + os.pathsep + env.get('PATH', '')
    completed = subprocess.run([str(p) for p in command], capture_output=True, timeout=90, env=env, cwd=SOURCE)
    (OUT / (name + '.stdout')).write_bytes(completed.stdout)
    (OUT / (name + '.stderr')).write_bytes(completed.stderr)
    if completed.returncode:
        raise RuntimeError(f'{name}: exit {completed.returncode}\n' + completed.stderr.decode(errors='replace')[:6000])
    return completed.stdout

def generate_fields(ast):
    records = {}
    records_by_id = {}
    typedefs = {}
    def index(node):
        if node.get('kind') == 'RecordDecl' and node.get('completeDefinition'):
            records_by_id[node['id']] = node
            name = node.get('name')
            if name:
                records[node['tagUsed'] + ' ' + name] = node
        if node.get('kind') == 'TypedefDecl':
            typedefs[node['name']] = node['type']['qualType']
        for child in node.get('inner', []): index(child)
    index(ast)
    def associate_anonymous(node):
        if node.get('kind') == 'TypedefDecl':
            def find_record(child):
                if child.get('kind') == 'RecordType':
                    return records_by_id.get(child.get('decl', {}).get('id'))
                for part in child.get('inner', []):
                    found = find_record(part)
                    if found is not None: return found
                return None
            record = find_record(node)
            if record is not None:
                records[node['name']] = record
        for child in node.get('inner', []): associate_anonymous(child)
    associate_anonymous(ast)

    lines = ['/* Generated from actual Quest headers by save_layout_run.py. */']
    seen_types = set()
    skipped = []
    def resolve(name):
        if name in records: return records[name]
        for _ in range(16):
            if name not in typedefs: break
            name = typedefs[name]
        return records.get(name)

    def walk(record, top_type, path, scratch):
        last_anon = None
        for field in record.get('inner', []):
            if field['kind'] == 'RecordDecl':
                last_anon = field
                continue
            if field['kind'] != 'FieldDecl': continue
            name = field.get('name')
            ftype = field['type']['qualType']
            if name is None:
                if field.get('isBitfield'): continue  # Unnamed alignment/padding field.
                if last_anon:
                    walk(last_anon, top_type, path, scratch)
                else:
                    skipped.append((top_type, path, ftype))
                continue
            current = path + name
            if field.get('isBitfield'):
                lines.append(f'BITS({top_type}, {scratch}, {current});')
                continue
            lines.append(f'FIELD({top_type}, {current});')
            base = re.sub(r'\[[^]]*\]', '', ftype).strip()
            dims = len(re.findall(r'\[[^]]*\]', ftype))
            nested = resolve(base)
            if nested is None and ('(unnamed ' in base or '(anonymous ' in base):
                nested = last_anon
            if nested is not None:
                if base in typedefs and base not in seen_types:
                    lines.append(f'TYPE({base});')
                    seen_types.add(base)
                walk(nested, top_type, current + '[0]' * dims + '.', scratch)
            elif base.startswith(('struct ', 'union ')):
                skipped.append((top_type, current, base))
    for top_type, scratch in [('Save_t', 'save_layout_scratch'), ('CARDDir', 'save_layout_card'),
                              ('mCD_keep_mail_c', 'save_layout_mail'), ('mCD_keep_original_c', 'save_layout_original'),
                              ('mCD_keep_diary_c', 'save_layout_diary'), ('mCD_foreigner_c', 'save_layout_foreigner')]:
        lines.append(f'TYPE({top_type});')
        seen_types.add(top_type)
        walk(resolve(top_type), top_type, '', scratch)
    (TESTS / 'save_layout_fields.inc').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return {'generated_checks': len(lines) - 1, 'named_types': sorted(seen_types), 'skipped': skipped}

def parse(data):
    result = {}
    for line in data.decode().splitlines():
        key, value = line.split('=', 1)
        result[key] = value
    return result

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('--no-device', action='store_true')
    parser.add_argument('--label', default='current')
    parser.add_argument('--unfixed-arm32', action='store_true', help='Reconstruct the pre-fix ARM32 header in probe output only.')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.label): parser.error('Label must contain letters, numbers, underscores or hyphens.')
    OUT = OUT / args.label
    OUT.mkdir(parents=True, exist_ok=True)
    clang = PATHS['clang_armv7_api24']
    common = [*DEFINES, *INCLUDES, '-std=gnu11', '-Wno-everything']
    arm_only = []
    if args.unfixed_arm32:
        header = (SOURCE / 'include' / 'm_quest.h').read_text()
        block = '#if defined(__ANDROID__) && defined(__arm__)\n    /* Preserve the PC save\'s full u32 bitfield slot before its timestamp. */\n    u32 : 0;\n#endif\n'
        if header.count(block) != 1: raise RuntimeError('Expected exact guarded alignment block once.')
        unfixed_header = OUT / 'm_quest_unfixed.h'
        unfixed_header.write_text(header.replace(block, ''))
        arm_only = ['-include', str(unfixed_header)]
    ast = run([clang, *common, *arm_only, '-fsyntax-only', '-Xclang', '-ast-dump=json', TESTS / 'save_layout_seed.c'], 'ast')
    coverage = generate_fields(json.loads(ast))
    (OUT / 'coverage.json').write_text(json.dumps(coverage, indent=2))
    if coverage['skipped']: print('Skipped records:', coverage['skipped'])
    windows = OUT / 'save-layout-windows.exe'
    android = OUT / 'save-layout-arm32'
    sources = [TESTS / 'save_layout_probe.c', SOURCE / 'pc' / 'src' / 'pc_save_bswap.c']
    windows_common = [*DEFINES, *['-I' + str(PC_REFERENCE / p) for p in ('include', 'pc/include', 'src', '.')], '-std=gnu11', '-w']
    windows_sources = [TESTS / 'save_layout_probe.c', PC_REFERENCE / 'pc' / 'src' / 'pc_save_bswap.c']
    run([r'C:\msys64\mingw32\bin\gcc.exe', *windows_common, '-O2', '-fno-strict-aliasing', '-fwrapv', *windows_sources, '-o', windows], 'compile-windows')
    run([clang, *common, *arm_only, '-O2', '-fno-strict-aliasing', '-fwrapv', '-fPIE', '-pie', *sources, '-o', android], 'compile-arm32')
    windows_result = parse(run([windows], 'windows'))
    print(f'Compiled both probes; {len(windows_result)} metrics, {len(coverage["named_types"])} named types.')
    if args.no_device: return
    adb = PATHS['adb']
    remote = '/data/local/tmp/acquest-layout'
    run([adb, 'push', android, remote], 'push')
    run([adb, 'shell', 'chmod', '700', remote], 'chmod')
    android_result = parse(run([adb, 'shell', remote], 'arm32'))
    differences = {key: {'windows': windows_result.get(key), 'arm32': android_result.get(key)}
                   for key in sorted(windows_result.keys() | android_result.keys()) if windows_result.get(key) != android_result.get(key)}
    report = {'metrics': len(windows_result), 'differences': differences, 'coverage': coverage}
    report['source_roots'] = {'windows_read_only': str(PC_REFERENCE), 'android': str(SOURCE)}
    report['source_sha256'] = {
        label + '/' + name: hashlib.sha256((root / name).read_bytes()).hexdigest()
        for label, root in [('windows', PC_REFERENCE), ('android', SOURCE)]
        for name in ['include/m_common_data.h', 'include/m_quest.h', 'pc/src/pc_save_bswap.c']
    }
    report['binary_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in [windows, android]}
    (OUT / 'comparison.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({'metrics': len(windows_result), 'difference_count': len(differences), 'first_differences': dict(list(differences.items())[:30])}, indent=2))
    if differences and not args.unfixed_arm32:
        raise SystemExit('FAIL: ARM32 save ABI differs from the working PC reference.')
    if args.unfixed_arm32 and not differences:
        raise SystemExit('FAIL: reconstructed pre-fix header did not reproduce the incompatibility.')

if __name__ == '__main__': main()
