# Animal Crossing PC Port

A native PC port of Animal Crossing (GameCube) built on top of the [ac-decomp](https://github.com/ACreTeam/ac-decomp) decompilation project.

The game's original C code runs natively on x86, with a custom translation layer replacing the GameCube's GX graphics API with OpenGL 3.3.

This repository does not contain any game assets or assembly whatsoever. An existing copy of the game is required.

Supported versions: GAFE01_00: Rev 0 (USA)

## Requirements

- **MSYS2** (https://www.msys2.org/)
- **Animal Crossing (USA) disc image** (ISO, GCM, RVZ, WIA, WBFS, CISO, or GCZ format)

### MSYS2 Packages

Open **MSYS2 MINGW32** from your Start menu and install:

```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-SDL2 mingw-w64-i686-ninja
```

You also need **Python 3** installed on your system. The build script will auto-detect it from common Windows install paths if it's not in your MSYS2 environment.

## Building

1. Clone the repository:
   ```bash
   git clone <repo-url>
   cd ac-decomp
   ```

2. Place your disc image in the `orig/GAFE01_00/` directory

3. Build (from **MSYS2 MINGW32** shell):
   ```bash
   ./build_pc.sh
   ```

   This single command handles the entire pipeline:
   - Runs the decomp toolchain (`configure.py` + `ninja`) to extract assets from the ROM
   - Extracts runtime files (archives, audio) via dtk
   - Configures and builds the PC port with CMake

   The first build takes several minutes. Subsequent builds are incremental.

4. Run:
   ```bash
   pc/build32/bin/AnimalCrossing.exe
   ```

## Controls

Keybinding support is planned.

### Keyboard

| Key | Action |
|-----|--------|
| WASD | Move (left stick) |
| Arrow Keys | Camera (C-stick) |
| Space | A button |
| Left Shift | B button |
| Enter | Start |
| X | X button |
| Y | Y button |
| Q / E | L / R triggers |
| Z | Z trigger |
| I / J / K / L | D-pad (up/left/down/right) |

### Gamepad

SDL2 game controllers are supported with automatic hotplug detection. Button mapping follows the standard GameCube layout. Rumble is supported.

## Command Line Options

| Flag | Description |
|------|-------------|
| `--verbose` | Enable diagnostic logging |
| `--no-framelimit` | Disable frame limiter (unlocked FPS) |
| `--model-viewer [index]` | Launch debug model viewer (75 building/structure models) |
| `--time HOUR` | Override in-game hour (0-23) |

## Settings

Graphics settings are stored in `pc/build32/bin/settings.ini` and can be edited manually or through the in-game options menu:

- Resolution (up to 4K)
- Fullscreen toggle
- VSync
- MSAA (anti-aliasing)

## Texture Packs

Custom textures can be placed in `pc/build32/bin/texture_pack/`.

## Save Data

Save files are stored in `pc/build32/bin/save/` using the standard GCI format, compatible with Dolphin emulator saves. Place a Dolphin GCI export in the save directory to import an existing save. Slot A and B support is planned.

## Credits

This project would not be possible without the work of the [ACreTeam](https://github.com/ACreTeam) decompilation team. Their near-complete C decompilation of Animal Crossing is the foundation this port is built on. Huge thanks to them.
