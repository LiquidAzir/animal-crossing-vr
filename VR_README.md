# Animal Crossing VR — Setup Guide

Native SteamVR support for the Animal Crossing PC port. The village renders in
true stereo 3D with full 6-DOF head tracking — lean in and look around your
town like a living diorama. Game UI (dialogue, menus, inventory) floats on a
panel in front of you, and your VR controllers act as the GameCube pad
(bindings ship for Touch and Index controllers; everything is rebindable).

## What you need

1. **This software** — the release zip has everything: `AnimalCrossing.exe`
   with `openvr_api.dll` and `vr_actions/` next to it. (Building from
   source produces all of it in `pc/build32/bin/`.)
2. **Your Animal Crossing (USA) disc image** (`.iso`, `.gcm`, or `.ciso`)
   in the `rom/` folder next to the exe.
3. **SteamVR** installed on the PC (free, on Steam). You do NOT need to own
   any Steam game.
4. **A SteamVR-compatible headset.** Native PCVR headsets (Index, Vive, and
   friends) just work once SteamVR is running. A Quest 2/3/Pro needs one of
   these to reach the PC — any of them works:
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

## First-person mode

Toggle any time with **F5** (keyboard) or **left grip + Y** (VR). Works flat
and in VR:

- The camera moves to your villager's eyes; the character model hides (its
  shadow stays) but **held tools stay visible and animated in front of
  you** — fishing rod (with its bend), net, shovel, axe, umbrella. Left
  stick walks **where you're looking — headset included**: turn your head
  and stick-forward follows your gaze. The right stick turns smoothly by
  default (`fp_snap_degrees = 45` in settings.ini if you prefer snap
  turns); pitch comes from your head in VR.
- **Doors keep you in first person** — walking into your house or a shop no
  longer cuts to third person. Scripted event cameras still take over
  (deliberately — including short "look at this" pans like the wallpaper
  preview when decorating).
- The corner **clock/address widget** only appears in VR after standing
  still for ~10 seconds (it was constant otherwise). Stand still when you
  want to check the time.
- In VR the world switches to **life-size scale** (`vr_fp_world_scale`,
  default 25 = one tile ≈ 1 m) — villagers stand in front of you at eye
  level. Toggling back returns to the diorama.
- **Talking to villagers stays first person** — the view turns to face them
  as the chat starts, so they're right in front of you (at eye level in
  VR). Doors, events, and demo cutscenes still use the normal game camera,
  then first person resumes.
- `[FirstPerson]` in `settings.ini`: `fp_mode` (start enabled),
  `fp_eye_height` (game units above the feet, default 52),
  `fp_snap_degrees` (0 = smooth VR turning).

## Outdoor sky

Outdoor views now have a blue gradient, soft rounded clouds, and a hazy distant
horizon. The sky follows the game's time and weather and stays at infinity as
you turn or lean, in both first person and diorama. Desktop views use it too.
To restore the original background, set `skybox = 0` under `[Graphics]` in
`settings.ini` (default: `1`). See [skybox notes](docs/skybox.md) for validation.

## Motion tools (VR first person)

With a tool out in first person:

- **Swing your right controller** (a firm, quick motion) to use it — swing
  the net, chop with the axe, dig with the shovel, cast the rod. One swing
  = one use.
- **Swing and button are fully interchangeable, always.** The swing simply
  presses A for you — the A button (right trigger) works at all times, and
  you can mix them freely (cast with a flick, hook the bite with the
  trigger, or vice versa). With `vr_tool_on_hand` on, the tool acts where
  your hand is for both input styles — point the net at the bug whether
  you swing or press.
- **The tool rides your real hand**: it's rendered at your controller's
  pose, and for the net that includes the catch area — you catch bugs
  where *you* swing. If the grip angle feels off, tune `vr_tool_pitch`
  (degrees) in `settings.ini`.
- **Look toward your target before using a tool.** In VR first person,
  axe and shovel targeting and the fishing cast use your horizontal gaze,
  even when your character last walked in another direction. The axe's
  collision probe follows that same gaze. The net updates facing when
  readied and released; its catch area still follows your actual hand.
  Looking elsewhere after starting a chop, dig, or cast does not redirect it.
- Motion gestures are ignored in dialogue, pause, inventory, while holding
  the tool-selection grip, and when tracking or velocity data is unavailable.
  Let your hand slow between swings to rearm; a single
  continuous fast movement no longer repeats tool use. Buttons keep working
  in menus and during normal play.
- Both features have switches under `[FirstPerson]`-adjacent keys in
  `settings.ini`: `vr_motion_swing` and `vr_tool_on_hand` (1 = on).

`vr_log.txt` now also records a performance summary every 30 seconds
(fps, GPU frame time, dropped frames) — see [VR_PLAYTEST.md](VR_PLAYTEST.md)
for the guided test route that uses it.

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
