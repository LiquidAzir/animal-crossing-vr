# Animal Crossing VR — Quest 3 Setup Guide

Native SteamVR support for the Animal Crossing PC port. The village renders in
true stereo 3D with full 6-DOF head tracking — lean in and look around your
town like a living diorama. Game UI (dialogue, menus, inventory) floats on a
panel in front of you, and Touch controllers act as the GameCube pad.

## What you need

1. **This build** (`AnimalCrossing.exe` + `openvr_api.dll` + `vr_actions/`
   next to it — the build produces all of it in `pc/build32/bin/`).
2. **Your Animal Crossing (USA) disc image** (`.iso`, `.gcm`, or `.ciso`)
   in the `rom/` folder next to the exe.
3. **SteamVR** installed on the PC (free, on Steam). You do NOT need to own
   any Steam game.
4. **A way to connect the Quest 3 to the PC** — any of these works:
   - **Steam Link** (Quest app, easiest): install Steam Link inside the
     headset, connect to your PC, SteamVR starts automatically.
   - **Virtual Desktop**: launch SteamVR from within VD.
   - **Quest Link / Air Link**: Meta Quest Link PC app + Link; launch SteamVR.

## Running

1. Put on the headset, get SteamVR running (see above).
2. Launch `AnimalCrossing.exe`.

That's it. With `vr_mode = 1` (the default) the game detects the headset and
starts in VR; without a headset it runs flat as before. Force with `--vr` /
`--no-vr`, or set `vr_mode` under `[VR]` in `settings.ini` (0 off, 1 auto,
2 force).

The desktop window shows a mirror of the left eye.

## Controls (Touch)

| Input | Action |
|---|---|
| Left stick | Move |
| Right stick | Camera (C-stick) |
| Right trigger or A | A (talk / use tool / confirm) |
| Left trigger or B | B (run / pick up / cancel) |
| X / Y | GC X / Y |
| Left grip | GC L — **hold it and the left stick becomes the D-pad** (tool switching; you can't walk while it's held) |
| Right grip | GC R |
| Right stick click | Z |
| Left stick click | Start (menu) |
| X + Y together | Recenter the view |

Rebind anything in SteamVR → Settings → Controllers → Manage Controller
Bindings. Keyboard and a normal gamepad keep working in VR too.

## Tuning (`settings.ini`, `[VR]` section)

| Key | Default | Meaning |
|---|---|---|
| `vr_mode` | 1 | 0 = off, 1 = auto, 2 = force |
| `vr_world_scale` | 10 | Millimeters per game unit. 10 → one ground tile is 40 cm (cute miniature). 25 → life-size-ish. Lower = smaller village, stronger diorama depth. |
| `vr_ui_distance` | 200 | UI panel distance in cm |
| `vr_ui_size` | 240 | UI panel width in cm |
| `vr_height_offset` | 0 | Raise (+) / lower (−) your viewpoint in cm |

## Comfort notes

- The camera is anchored to the game's camera but **the horizon is always
  level** with real gravity, and camera cuts snap instantly (no swooping).
- The village slides by as you walk (the game's camera follows the player).
  At the default miniature scale this reads as a diorama moving past —
  gentle for most people. If you're sensitive, try a smaller
  `vr_world_scale` (e.g. 6-8).
- The game runs at your headset's refresh rate (72-120 Hz) — the port's
  delta-time system keeps game speed correct.
- NES games appear on the floating panel, locked at their native 60 Hz.

## Troubleshooting

Two files next to the exe make problems diagnosable without a terminal:

- **`vr_log.txt`** — written every VR launch: runtime detection, init steps,
  render target size, first-frame submit result, and any fallback reason.
- **`crash.txt`** — written only if the game crashes: the faulting module
  and offset.

If something goes wrong, send both files.

- **"SteamVR is not installed" box** → install SteamVR from Steam, relaunch.
- **Game runs flat with headset on** → make sure SteamVR is running first;
  check `vr_mode` isn't 0; try `--vr`; read `vr_log.txt` for the reason.
- **Black view but game audible** → check `vr_log.txt` for submit errors.
- **Wrong seated position** → look straight ahead and press X + Y.
- **Performance** → lower SteamVR's render resolution, or reduce `msaa` in
  `settings.ini` (applies to the mirror window only).
