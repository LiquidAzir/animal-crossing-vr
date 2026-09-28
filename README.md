# Animal Crossing VR

> This isolated checkout is the **standalone Quest development branch**. See
> [Quest build, installation, and test status](quest/README.md) for its APK and
> Android instructions. The Windows documentation below describes the PC
> baseline; the Quest port has its own build, installation, settings, and saves.

**A native VR fork of [ACGC-PC-Port](https://github.com/flyngmt/ACGC-PC-Port)** —
the Animal Crossing (GameCube) PC port built on the
[ac-decomp](https://github.com/ACreTeam/ac-decomp) decompilation.

This fork adds a full SteamVR mode on top of the port: true stereo rendering
with 6-DOF head tracking, a **first-person mode** that puts you on the ground
in your village at life size, **motion-controlled tools** (swing your
controller to catch bugs, chop trees, cast the fishing rod), snap turning,
a floating panel for the game's 2D UI, and controller haptics. The same
build runs **completely flat** with keyboard or gamepad when no headset is
present — VR simply activates when one is.

Latest release: **[v0.9.3-vr19](https://github.com/LiquidAzir/animal-crossing-vr/releases/tag/v0.9.3-vr19)**.
This Windows VR build is based on the recompiled PC port **v0.9.3**.

| | |
|---|---|
| Third person (VR) | Your village as a living diorama — lean in, look around |
| First person (F5 / grip+Y) | Stand in the village at eye level; villagers talk to your face |
| Motion tools | Swing the net/axe/shovel/rod with your right controller — or just press A; both always work |
| Flat mode | The unmodified game experience, plus the first-person camera if you want it |

## Installation

**Start with a working installation of the recompiled PC version, then
apply this VR update to it.** Use the steps below in order. Downloading
the prebuilt PC version does not require compiling code or installing
an emulator.

### 1. Install the recompiled PC version first

1. Open the upstream **[ACGC-PC-Port v0.9.3 download page](https://github.com/flyngmt/ACGC-PC-Port/releases/tag/v0.9.3-playtest)**.
   Under **Assets**, download **`ACGC-PC-Port0.9.3.zip`**. Do not choose
   GitHub's **Source code** archives. This is the base version used by this
   VR fork; newer upstream versions have not been validated with it.
2. Right-click the downloaded zip and choose **Extract All**. Put the
   extracted game in a writable folder, such as `C:\Games\AnimalCrossing-PC`.
   Find the folder that directly contains **`AnimalCrossing.exe`**; this is
   your game folder for every step below. Do not run the game from inside a zip.
3. Put your own **Animal Crossing (USA, Rev 0 / GAFE01_00)** disc image in
   the **`rom`** folder next to that executable. Create `rom` if it is missing.
   Supported formats are **`.iso`, `.gcm`, and `.ciso`**; the filename can be
   anything. Neither download includes a disc image or game assets.
4. Run the PC port's **`AnimalCrossing.exe`** and check that it reaches the
   title screen. If it cannot find the game or will not start, finish fixing
   the base PC installation before adding VR. Then close the game.
5. **Back up the entire game folder** to a separate location. This keeps a
   working copy of the PC version and your saves before you replace program
   files. Existing saves live under `save`; keep the whole folder, including
   `card_a` and `card_b` if present.

### 2. Apply the latest VR update

6. Open **[Animal Crossing VR releases](https://github.com/LiquidAzir/animal-crossing-vr/releases/latest)**.
   Under **Assets**, download **`AnimalCrossing-VR-v0.9.3-vr19-win32.zip`**.
   Do not download the **Source code** archives.
7. Extract that zip to a temporary location. Open its **`AnimalCrossing-VR`**
   folder and **copy everything inside it into your existing game folder**
   from step 2. Choose **Replace the files in the destination** when asked.
   Copy the contents, rather than putting another `AnimalCrossing-VR` folder
   inside your game folder.
8. Check that **`AnimalCrossing.exe`**, **`SDL2.dll`**, **`openvr_api.dll`**,
   **`shaders`**, and **`vr_actions`** are together. The VR update replaces
   the executable and supplies its supporting files; updating only the
   executable can leave hands or controller inputs missing.

Your folder should now look like this (additional documentation is normal):

```text
AnimalCrossing-PC/
  AnimalCrossing.exe
  SDL2.dll
  openvr_api.dll
  BUILD_INFO.txt
  shaders/
    default.vert
    default.frag
  vr_actions/
    actionmanifest.json
    bindings_oculus_touch.json
    bindings_knuckles.json
  rom/
    YourGame.iso                 (or .gcm / .ciso)
  save/                         (your existing saves)
  settings.ini                  (created by the game)
  keybindings.ini                (created by the game)
```

The VR update does **not** include or replace `rom`, `save`, `texture_pack`,
`settings.ini`, or `keybindings.ini`. Keep yours. `BUILD_INFO.txt` identifies
the version and executable included in the download.

### 3. Connect the headset and play

9. Install **[SteamVR](https://store.steampowered.com/app/250820/SteamVR/)**
   on your PC. Connect your PCVR headset and wake both controllers. For a
   Quest headset, connect to the PC through Steam Link, Virtual Desktop,
   or Quest Link/Air Link, then start SteamVR. Wait until SteamVR detects
   the headset and controllers before launching the game.
10. Run **`AnimalCrossing.exe` from the updated game folder**. VR activates
    automatically when SteamVR has a connected headset. Without one, the
    same executable runs as a desktop game.
11. Press **F5** on the keyboard, or **left grip + Y** on the VR controllers,
    to enter first person. Press **X + Y together** to recenter. Move with
    the left stick; use the right trigger or A to talk, confirm, or use tools.
    Empty hands appear while no item is equipped during normal first-person
    VR play; they hide during dialogue and menus.

Those button labels are for Touch controllers. On **Valve Index**, use
**left grip + left B** for first person and **left A + left B** to recenter;
use the **right** trigger or **right A** to confirm. If you create a desktop
shortcut, set its **Start in** field to the game folder so saves and the disc
image are found in the correct place.

First person is a toggle by default. To start in first person every time,
close the game and set **`fp_mode = 1`** under **`[FirstPerson]`** in
`settings.ini`. VR auto-detection uses **`vr_mode = 1`** under **`[VR]`**.
Floating hands default to **`vr_empty_hands = 1`** under **`[FirstPerson]`**;
an existing off setting is preserved. No build tools are needed for these steps.

### Updating an existing VR installation

1. Close the game and back up its entire folder, especially `save`.
2. Download and extract the latest VR zip, then copy the **contents** of its
   `AnimalCrossing-VR` folder over your existing game folder.
3. Replace all supplied program files, including **both DLLs, `shaders`, and
   the complete `vr_actions` folder**. Preserve your saves, disc image,
   texture packs, settings, and keybindings. You do not need to reinstall
   the base PC port when updating an already working VR installation.
4. Restart the game with SteamVR ready. If you use custom SteamVR bindings,
   select the updated default bindings or add the two empty-hand pose actions
   described in [the hand setup guide](VR_README.md#floating-hands-optional).

Setup details, the full VR control table, comfort tuning, and
troubleshooting: **[VR_README.md](VR_README.md)**. A guided test route for
verifying a VR session end to end: [VR_PLAYTEST.md](VR_PLAYTEST.md).

## What's new in VR19

- Outdoor sky that follows time and weather, including nights and rain.
- Corrected tool/net alignment and gaze-directed movement and tool actions.
- Improved seasonal building backs, including houses, Able Sisters, police,
  museum, post office, Nook's Cranny, and the fountain; repaired ramp-side gaps.
- Controller-tracked floating hands when empty-handed, enabled by default,
  with corrected Touch and Index hand bindings.
- First-person catches and discoveries, with the presented item in front of
  your view, plus raised dialogue placement.
- Small scenery and sky performance improvements, with unchanged sky images
  in 1,176 comparison views. See [performance guidance](VR_README.md#performance-on-slower-pcs).

Building repairs follow the building models, so a different town layout does
not require manual placement. Some remaining building types and later Nook
shop upgrades still use the older approximate rear fill. Minimum PC requirements
have not yet been established through lower-end hardware testing.

## Building from source

Only needed if you want to modify the code.

- **MSYS2** (https://www.msys2.org/), then from **MSYS2 MINGW32**:

```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-SDL2 mingw-w64-i686-make
./build_pc.sh
```

Output lands in `pc/build32/bin/`. The build is strictly 32-bit (the
decompiled code depends on 32-bit pointers); the VR layer uses OpenVR's
FnTable C API for exactly this reason. Architecture notes:
[pc/VR_ARCHITECTURE.md](pc/VR_ARCHITECTURE.md) and
[pc/DOCUMENTATION.md](pc/DOCUMENTATION.md).

## Keyboard controls (flat)

| Key | Action |
|-----|--------|
| WASD | Move (left stick) |
| Arrow Keys | Camera (C-stick) — look, in first person |
| Space | A button |
| Left Shift | B button |
| Enter | Start |
| X / Y | X / Y buttons |
| Q / E | L / R triggers |
| Z | Z trigger |
| I / J / K / L | D-pad |
| F5 | Toggle first person |

Rebindable via `keybindings.ini`. SDL2 gamepads are supported with hotplug;
VR controller bindings are rebindable in SteamVR's controller settings.

## Credits & lineage

- **[ACreTeam](https://github.com/ACreTeam)** — the complete Animal
  Crossing decompilation ([ac-decomp](https://github.com/ACreTeam/ac-decomp))
  that makes all of this possible.
- **[flyngmt](https://github.com/flyngmt)** — the
  [PC port](https://github.com/flyngmt/ACGC-PC-Port): the GX→OpenGL
  translation layer, asset pipeline, audio, save system, and everything
  else that turned the decompilation into a runnable native game. This
  fork's VR layer stands entirely on that work.
- **[FIX94](https://github.com/FIX94)** — fixNES, powering the in-game NES
  titles.
- **Valve** — the [OpenVR SDK](https://github.com/ValveSoftware/openvr)
  (BSD-3-Clause, vendored in `pc/lib/openvr/`).
- VR fork (stereo renderer, first person, motion tools) by
  [LiquidAzir](https://github.com/LiquidAzir).

## License

Dual/multi-licensed — see [LICENSE](LICENSE):
- Decompiled game code: **CC0 1.0** (ACreTeam/ac-decomp)
- PC port layer: **MIT** © FlyingMeta
- VR fork additions: **MIT** © LiquidAzir
- fixNES: **MIT** © FIX94 · OpenVR: **BSD-3-Clause** © Valve

This project distributes no Nintendo game assets or disc images. A legally obtained
disc image is required to play.

## FAQ

See upstream's [FAQ](FAQ.md) for port questions (Deluxe-mod compatibility,
Linux/Steam Deck, online play). VR-specific troubleshooting is in
[VR_README.md](VR_README.md) — start with `vr_log.txt`, which explains
itself.
