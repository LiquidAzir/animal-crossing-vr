# Animal Crossing PC Port — Developer Documentation

PC port of Animal Crossing GameCube built on top of a 99.52% complete C decompilation.

## Architecture Overview

The game's rendering has 3 tiers:

```
Game code (N64 display lists) → emu64 (DL interpreter) → GX (GameCube GPU API) → [OpenGL]
```

We replace **only tier 3** (GX → OpenGL 3.3). The emu64 layer and game code stay as-is, with `#ifdef TARGET_PC` guards where platform differences require it.

### Boot Chain

```
main() [pc_main.c]
  → pc_settings_load()          # load settings.ini
  → pc_platform_init()          # SDL2, GL 3.3, GLAD, VEH crash handler
  → pc_texture_pack_init()      # scan texture_pack/ for HD replacements
  → ac_entry()                  # game's main.c: sets HotStartEntry = &entry
  → boot_main()                 # boot.c: OSInit, DVD, archives
    → entry() → mainproc() → graph_proc()   # THE MAIN LOOP

graph_proc() loops over scenes via game_dlftbls[]:
  first_game → second_game → trademark → select (title demo)
  → player_select (scene 19) → play (gameplay)
  OR: --model-viewer → model_viewer_init (scene 10)

Each frame: graph_main()
  → game_main() → scene->exec()         # builds N64 display lists
  → graph_task_set00() → emu64_taskstart()  # processes DLs → GX → GL
  → VIWaitForRetrace()                   # SDL swap + event pump + frame pacing
```

## File Reference

### PC Port Layer (what we wrote)

#### Core

| File | Purpose |
|------|---------|
| `pc/src/pc_main.c` | Entry point, SDL2/GL init, VEH crash protection, CLI flags |
| `pc/src/pc_gx.c` | GX → OpenGL: all GX API functions, vertex submission, state, draw dispatch |
| `pc/src/pc_gx_tev.c` | TEV shader: GLSL program loading, uniform upload |
| `pc/src/pc_gx_texture.c` | 10 GC texture format decoders, 2048-entry cache with FNV-1a |
| `pc/src/pc_os.c` | Dolphin OS: memory arena, timers, calendar time, message queues, thread stubs |
| `pc/src/pc_mtx.c` | C matrix math replacing PPC paired-singles assembly |
| `pc/src/pc_misc.c` | HW register arrays, EXI/SI/PPC stubs, malloc wrappers, trig |

#### I/O and Storage

| File | Purpose |
|------|---------|
| `pc/src/pc_dvd.c` | DVD filesystem → file I/O from extracted assets |
| `pc/src/pc_card.c` | Memory card API → local file save/load |
| `pc/src/pc_m_card.c` | Memory card manager: GCI save/load, village generation, ARAM data blocks |
| `pc/src/pc_save_bswap.c` | GCI save file bidirectional LE↔BE byte-swap (Dolphin-compatible) |
| `pc/src/pc_pad.c` | Keyboard + SDL2 gamepad input (GC button format) |
| `pc/src/pc_audio.c` | SDL2 audio: 32kHz s16 stereo ring buffer bridging jaudio DAC output |
| `pc/src/pc_vi.c` | Video interface → SDL swap, frame pacing (audio-clock or timer-based) |
| `pc/src/pc_aram.c` | 16MB ARAM buffer, bump allocator, DMA → memcpy |

#### Enhancements

| File | Purpose |
|------|---------|
| `pc/src/pc_settings.c` | Runtime `settings.ini` parser/writer (resolution, fullscreen, vsync, MSAA) |
| `pc/src/pc_texture_pack.c` | Dolphin-compatible HD texture pack loader (XXHash64 matching, DDS) |
| `pc/src/pc_model_viewer.c` | Debug model viewer: 75 building/structure models, orbit camera |

#### Support

| File | Purpose |
|------|---------|
| `pc/src/pc_stubs.c` | Remaining link stubs (GBA, famicom, libultra, threads) |
| `pc/src/pc_stubs_cpp.cpp` | JSystem C++ vtable stubs |
| `pc/src/pc_fontdata.c` | Embedded font (byte-swapped for LE) |
| `pc/shaders/default.vert` | GLSL vertex shader (runtime-loaded) |
| `pc/shaders/default.frag` | GLSL fragment shader (runtime-loaded, uniform-driven TEV stages) |

#### Headers

| File | Purpose |
|------|---------|
| `pc/include/pc_platform.h` | Platform config, 32-bit guard, SDL2/GL includes, crash API, widescreen defs |
| `pc/include/pc_gx_internal.h` | PCGXState, PCGXVertex, PCGXTevStage, indirect texture structs |
| `pc/include/pc_save_bswap.h` | GCI save byte-swap API |
| `pc/include/pc_model_viewer.h` | Model viewer struct and init/cleanup |
| `pc/include/pc_bswap.h` | `pc_bswap16/32/64` macros + array swap helpers |
| `pc/include/pc_settings.h` | Settings struct and load/save/apply API |
| `pc/include/pc_texture_pack.h` | Texture pack init/lookup/shutdown API |

### Critical Decomp Modifications

These are the most-modified files from the upstream decompilation:

| File | Changes | Why |
|------|---------|-----|
| `src/static/libforest/emu64/emu64.c` | ~850 lines | Texture cache routing, TEXEL1, vertex colors, crash protection, fog guard, per-stage texture binding, widescreen NOOPTag handling |
| `include/libforest/gbi_extensions.h` | ~660 lines | 30 GBI bitfield structs reversed for LE x86 |
| `src/static/libforest/emu64/emu64_utility.c` | ~50 lines | seg2k0 proximity heuristic, N64Mtx byte-swap |
| `src/static/boot.c` | ~50 lines | Arena init, REL skip, actable endian swap |
| `src/graph.c` | ~100 lines | Frame loop diagnostics, model viewer routing |
| `src/padmgr.c` | ~75 lines | GC→N64 button conversion, once-per-frame guard |
| `src/game/m_play.c` | ~65 lines | Scene transition diagnostics, fog BG fix, widescreen stretch markers |
| `src/game/m_player_lib.c` | ~50 lines | Player palette byte-swap from ARAM |
| `src/game/m_field_make.c` | ~40 lines | FG data u16 byte-swap (3 swap sites) |
| `src/game/m_room_type.c` | ~24 lines | Room wall/floor palette u16 byte-swap |
| `src/game/m_scene.c` | ~21 lines | Scene_Word_u endianness fix |
| `src/sys_matrix.c` | ~70 lines | Matrix_MtxtoMtxF endian swap, suMtxMakeTS/SRT/SRT_ZXY fixes |

About ~95 decomp files total are modified. Most changes are small `#ifdef TARGET_PC` blocks for byte-swapping or platform adaptation.

## Rendering Pipeline

### Vertex Submission

Deferred commit model. A position call commits the *previous* vertex. Auto-flush via `pc_gx_flush_if_begin_complete()` when expected vertex count is reached (handles missing GXEnd). Explicit GXEnd calls added at end of dl_G_TRIN/dl_G_QUADN/dl_G_TRI2 to prevent batches from flushing after viewport changes.

### SHARED vs NONSHARED Vertices

- **SHARED** (GX_PNMTX0, slot 0): pre-transformed at load time for seamless character joints
- **NONSHARED** (GX_PNMTX1, slot 1): transformed by GX matrix each frame

Do NOT force all vertices to NONSHARED — it breaks character joint seams.

### TEV Pipeline

Up to 3 stages, KONST colors, swap tables, per-stage texture binding. Single GLSL program with uniform-driven stages (bias, scale, clamp, output register selection all via uniforms). Shaders loaded from `pc/shaders/` at runtime with embedded fallback.

### Texture Cache

2048-entry cache keyed by (ptr, w, h, fmt, tlut_name, content_hash). ~100% hit rate at steady state. 10 GC texture formats decoded: I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, CI4, CI8, CI14x2, CMPR (S3TC).

### Widescreen (3-state system)

Controlled by `g_pc_widescreen_stretch`:
- **0 (hor+)**: full-window viewport, FOV-corrected projection. Default, resets each frame.
- **1 (stretch)**: full-window, no correction. For fullscreen transitions/inventory backgrounds.
- **2 (pillarbox)**: centered 4:3 viewport with black bars. For inventory UI alignment.

m_play.c inserts NOOPTag markers in POLY_OPA display lists to toggle between states. emu64 reads these during DL processing.

## Endianness

All ROM/ARAM data is big-endian. Multi-byte fields must be byte-swapped after loading.

Pattern: `#ifdef TARGET_PC` byte-swap block right after `_JW_GetResourceAram` call.

Known swap sites:
- RARC archives (JKRAramArchive.cpp)
- FG data: 3 sites in m_field_make.c
- Messages: mMsg_Get_BodyParam
- Player palettes: m_player_lib.c
- Room wall/floor palettes: m_room_type.c
- N64Mtx s16 pairs: emu64_utility.c
- Scene_Word_u: m_scene.c
- Billboard matrices: sys_matrix.c (Matrix_MtxtoMtxF, suMtxMakeTS/SRT/SRT_ZXY)
- NPC clothing: ac_npc_cloth.c_inc (both DMA paths)
- Raw binary actables: 6 files swapped once at boot via mFM_InitActableEndian()

**Cannot centralize**: ARAM data has mixed layouts (u8 textures, u16 palettes, u32 offsets). A bulk swap at the `_JW_GetResourceAram` layer would corrupt byte-level data.

## Audio

jaudio_NES engine compiled and linked (59 source files, ~23K lines). SDL2 backend at 32kHz s16 stereo with SPSC ring buffer. rspsim software DSP processes ADPCM/RESAMP/ENVMIX.

All effects enabled: reverb, comb filter, Haas effect, Dolby surround.

### Known audio issue

Subtle bass distortion in specific rooms (museum dinosaur room). Present since early audio implementation. Root cause likely in A_CMD_UNK3 implementation accuracy (reverse-engineered from table data, no original microcode reference).

## Save System

GCI format only (64-byte CARDDir header + 0x72000 raw data). Bidirectional LE↔BE byte-swap for all ~300+ multi-byte fields.

- Save file: `save/DobutsunomoriP_MURA.gci`
- Also scans for Dolphin naming format (`8P-GAFE-...`)
- Backup rotation: up to 3 `.bak` files on each save
- Recovery: tries temp file, then backups if main save is missing
- Compatible with Dolphin emulator (can import/export saves)

## Enhancement Features

Compiled under `PC_ENHANCEMENTS` define (enabled by default in CMakeLists.txt).

### Settings (`settings.ini`)

```ini
[Graphics]
window_width = 1280
window_height = 720
fullscreen = 0          # 0=windowed, 1=fullscreen, 2=borderless
vsync = 0
msaa = 4                # 0/2/4/8
```

Auto-generated with defaults on first run. Edit by hand.

### HD Texture Packs

Drop Dolphin-compatible HD texture packs into `texture_pack/` directory. Uses XXHash64 for matching (identical algorithm to Dolphin). Supports DDS files with BC7, BC1, BC3, or uncompressed RGBA.

Filename format: `tex1_{W}x{H}_{hash}[_{tlut_hash}]_{fmt}.dds`

Wildcard palette support: `tex1_WxH_DATAHASH_$_FMT.dds` matches any palette variant.

## Input

Keyboard mapping:
- **WASD** = analog stick
- **Arrow keys** = C-stick
- **Space** = A, **LShift** = B, **Enter** = Start
- **IJKL** = D-pad
- **Q/E** = L/R triggers, **Z** = Z trigger
- **F3** = toggle frame limiter
- **ESC** = quit

SDL2 gamepad with hotplug, analog sticks, triggers, and rumble.

PADRead returns GC button format. Conversion to N64 format happens in `padmgr_UpdatePC()`.

## Crash Protection

Three layers:
1. **VEH/signal handler** (pc_main.c): catches ACCESS_VIOLATION, ILLEGAL_INSTRUCTION, INT_DIVIDE_BY_ZERO, PRIV_INSTRUCTION. Recovers via longjmp.
2. **game_main wrapper** (game.c): setjmp/longjmp around scene exec.
3. **Actor profile guard** (m_actor.c): skips NULL/invalid profiles.

## Build System

32-bit MinGW GCC 15.1 (i686) + CMake + SDL2 2.30.10 + GLAD2 (GL 3.3 Core).

**Must compile as 32-bit** — decomp code casts pointers to u32 everywhere.

```bash
# Build
cd /ac-decomp/pc/build32
MSYSTEM=MINGW32 mingw32-make -j8

# Run
cd bin/
./AnimalCrossing.exe --verbose
```

### CLI Flags

| Flag | Effect |
|------|--------|
| `--verbose` / `-v` | Enable diagnostic output |
| `--no-framelimit` | Disable frame pacing |
| `--model-viewer [N]` | Launch model viewer (optional start index) |
| `--time HOUR` | Override in-game hour (0-23) |
| `--help` / `-h` | Show help |

## Common Pitfalls

- **32-bit required**: 64-bit builds crash in JKRHeap (pointer→u32 casts).
- **`__attribute__((weak))`** doesn't work on MinGW/PE. Use regular definitions.
- **libc64/malloc.c** is excluded — it redefines system malloc and crashes the CRT.
- **NDEBUG must always be defined**: decomp asserts have side effects. Without NDEBUG, assert macros run and cause texture corruption.
- **Optimization must be -O0**: any optimization (-O1+) exposes UB in decomp code (infinite spawn loops, crashes).
- **windows.h macros**: always `#undef near` / `#undef far` after including.
- **GC address space**: emu64 uses 0x80000000-0x83000000 range. Guard with TARGET_PC.
- **glClear respects write masks**: must set glDepthMask(GL_TRUE) + glColorMask(all TRUE) before glClear.
- **seg2k0 collision**: PC heap pointers can collide with N64 segment addresses. Fixed with proximity heuristic + VirtualAlloc arena at >=0x10000000.
- **`#included .c` files**: emu64_utility.c, emu64_print.cpp, jsyswrapper_ext.cpp, jsyswrapper_main.cpp, ac_animal_logo_misc.c, m_item_debug.c, ac_npc_shop_common.c — these are compiled as part of their parent file, not standalone.
