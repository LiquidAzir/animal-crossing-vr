# Animal Crossing VR

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

Based on upstream **v0.9.3**. Upstream improvements can be merged in as they
land.

| | |
|---|---|
| Third person (VR) | Your village as a living diorama — lean in, look around |
| First person (F5 / grip+Y) | Stand in the village at eye level; villagers talk to your face |
| Motion tools | Swing the net/axe/shovel/rod with your right controller — or just press A; both always work |
| Flat mode | The unmodified game experience, plus the first-person camera if you want it |

## What you need

1. **This software** — grab the latest zip from
   [Releases](../../releases) (no build tools required).
2. **Your own Animal Crossing (USA) disc image** (`.iso`, `.gcm`, or
   `.ciso`), dumped from a cartridge/disc you own, placed in the `rom/`
   folder next to the exe.
   **This project contains no game assets and never will. Do not ask for
   ROMs and do not link to them in issues or discussions.**
3. **For VR:** [SteamVR](https://store.steampowered.com/app/250820/SteamVR/)
   (free) and any SteamVR-compatible headset. Quest 2/3/Pro work over Steam
   Link, Virtual Desktop, or Quest Link; Index, Vive, and other native PCVR
   headsets work directly. Any reasonably VR-capable GPU is enough — the
   game is light.
4. **For flat play:** nothing else. SteamVR is not required; the game
   detects the absence of a headset and runs as a normal window.

Setup details, the full VR control table, comfort tuning, and
troubleshooting: **[VR_README.md](VR_README.md)**. A guided test route for
verifying a VR session end to end: [VR_PLAYTEST.md](VR_PLAYTEST.md).

## Quick start

1. Extract the release zip anywhere.
2. Put your disc image in `rom/`.
3. (VR) Start SteamVR with your headset connected.
4. Run `AnimalCrossing.exe`.

Settings live in `settings.ini` (created on first run) — resolution,
world scale, first-person options, motion-tool toggles. `F5` toggles
first person any time; in VR, left grip + Y does the same.

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

## AI notice

Upstream discloses that AI tools were used for the PC port layer. In the
same spirit: this fork's VR/first-person layer was developed with
substantial assistance from Claude (Anthropic). All of it was
human-directed, reviewed, and play-tested.

## License

Dual/multi-licensed — see [LICENSE](LICENSE):
- Decompiled game code: **CC0 1.0** (ACreTeam/ac-decomp)
- PC port layer: **MIT** © FlyingMeta
- VR fork additions: **MIT** © LiquidAzir
- fixNES: **MIT** © FIX94 · OpenVR: **BSD-3-Clause** © Valve

This project distributes no Nintendo assets or code. A legally obtained
disc image is required to play.

## FAQ

See upstream's [FAQ](FAQ.md) for port questions (Deluxe-mod compatibility,
Linux/Steam Deck, online play). VR-specific troubleshooting is in
[VR_README.md](VR_README.md) — start with `vr_log.txt`, which explains
itself.
