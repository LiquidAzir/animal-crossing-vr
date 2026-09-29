# Animal Crossing VR — Setup Guide

Native SteamVR support for the Animal Crossing PC port. The village renders in
true stereo 3D with full 6-DOF head tracking — lean in and look around your
town like a living diorama. Game UI (dialogue, menus, inventory) floats on a
panel in front of you, and your VR controllers act as the GameCube pad
(bindings ship for Touch and Index controllers; everything is rebindable).

## What you need

1. **A working recompiled PC installation first:** use
   [ACGC-PC-Port v0.9.3](https://github.com/flyngmt/ACGC-PC-Port/releases/tag/v0.9.3-playtest),
   launch it successfully, close it, and back up its folder. Then apply the
   [latest VR update](https://github.com/LiquidAzir/animal-crossing-vr/releases/latest)
   to that folder using the [step-by-step installation walkthrough](README.md#installation).
   Copy all supplied files, including `SDL2.dll`, `openvr_api.dll`, `shaders/`,
   and `vr_actions/`, alongside the replacement `AnimalCrossing.exe`.
   Preserve your existing saves and settings; the update contains neither.
2. **Your Animal Crossing (USA, Rev 0 / GAFE01_00) disc image**
   (`.iso`, `.gcm`, or `.ciso`) in `rom/` next to the exe. No assets are bundled.
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

1. Finish the PC-port setup and VR file update above.
2. Put on the headset, get SteamVR running, and wake both controllers.
3. Launch `AnimalCrossing.exe` from the updated game folder.
4. Press **F5** or **left grip + Y** for first person; **X + Y** recenters.
   Set `fp_mode = 1` under `[FirstPerson]` in `settings.ini` while the game is
   closed if you want first person enabled on future launches.

The X/Y button names in this guide refer to Touch controllers. On Index,
left A provides X and left B provides Y: use **left grip + left B** to toggle
first person and **left A + left B** to recenter. Launch from the game folder;
a shortcut's **Start in** field must point there too.

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
  VR). Dialogue, its nameplate, and choices sit 32 UI pixels higher in VR
  first person. Menus and desktop dialogue keep their normal placement.
- **Catches and discoveries stay first person** — fish, bugs, fossils,
  and other item presentations keep your current gaze and world scale.
  Their messages still advance with A; motion swings cannot skip the text.
  Scripted events and demo cutscenes retain their normal camera.
- `[FirstPerson]` in `settings.ini`: `fp_mode` (start enabled),
  `fp_eye_height` (game units above the feet, default 52),
  `fp_snap_degrees` (0 = smooth VR turning).

## Floating hands (optional)

First-person VR can show two small rounded white hand markers following your
controllers while empty-handed. They are off by default. There are no arms, finger tracking, or new controls. Both hands
hide when an item is equipped and during dialogue, pickups, menus, and scripted
actions. A controller that loses tracking disappears until tracking returns.
Hands are depth-tested against the world and do not change tool behavior.

The default setting is `vr_empty_hands = 0` under `[FirstPerson]` in `settings.ini`.
Use **Settings → Gameplay → VR empty hands → On → Apply**
to enable them without restarting. Existing explicit preferences are preserved.
The updated `vr_actions` folder must accompany the executable. Custom SteamVR
bindings can bind **Left Empty Hand** and **Right Empty Hand** to each
controller's handgrip pose (`/pose/handgrip`); the existing tool pose remains separate.
The September 27 bindings correct an unsupported pose name on Touch controllers.
If hands remain absent with awake controllers, check `vr_log.txt` for the
missing-pose diagnostic and update the `vr_actions` folder with the executable.

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
  trigger, or vice versa). With `vr_tool_on_hand` on, the tool is displayed
  at your controller. The net's catch area follows your hand for both input
  styles; axe, shovel, and fishing-cast targets follow your horizontal gaze.
- **The tool rides your real hand**: it's rendered at your controller's
  pose, and for the net that includes the catch area — you catch bugs
  where *you* swing. If the grip angle feels off, tune `vr_tool_pitch`
  (degrees) in `settings.ini`.
- The net hoop is rolled around its handle to face forward when held upright.
  Both regular and golden nets retain the controller's shaft direction;
  their VR catch probes follow that same direction.
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

## Building and terrain completion

All five villager house styles and four player home sizes have rear walls
joined to their original mesh edges, in summer and winter. These replace the
older reflected copies that overlapped roofs, doors, and porches. Rear surfaces
reuse opaque portions of the existing wall textures and each home's live palette.
Rear detail repeats at a scale matching the building, with sampled timber trim
on plaster homes and blended edge lighting instead of a stretched plain swatch.
Their appearance is reconstructed; the original game has no rear-wall artwork.
Black window panes are the original unlit windows and still light up normally.

The police station has a fitted rear wall; Able Sisters has fitted left/rear
walls and rear eaves. Each retains one entrance and sign. The fountain uses
small bark and stone closures, preserving its single tree, basin, and water.
The museum and post office also have fitted walls and roof undersides using
their original seasonal stonework and plaster/timber textures. Their original
entrances and details draw once, without reflected siding or extra facades.
Nook's Cranny (`shop1`) has fitted summer/winter timber backs and roof underside
strips, keeping one entrance and sign. Later Nook shop upgrades retain their
older model-specific rear fill.

Three omitted ramp-side cliff triangles are also filled using the original
terrain vertices and materials. Collision shapes are unchanged. Remaining
structure types still use the earlier approximate rear fill; `vr_solid_shell`
adjusts only that fallback. `vr_solid_buildings = 0`
disables building completion.

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
- SteamVR paces rendering toward the headset's refresh rate. Actual delivery
  depends on the PC, render resolution, and runtime settings; a smooth view
  can also include reprojected frames. The port uses delta time for game speed.
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
- **Performance** → see the headset-specific settings below.

## Performance on slower PCs

Start with the normal defaults. The fitted building repairs are cached and
should stay enabled; turning them off brings back missing surfaces. Floating
hands and the sky measured cheaply on the development PC, but that does not
establish a minimum GPU requirement.

For a GPU bottleneck, lower the game's per-application render resolution in
SteamVR, then restart the game so it recreates its eye buffers. If the headset
offers a lower refresh rate, that gives the application a longer frame budget.
Check motion and text readability after each change. The desktop window's
resolution, MSAA, and Max FPS settings do not lower the headset's eye resolution;
normal VR gameplay also bypasses the desktop frame limiter.

`vr_draw_radius = 2` is an optional terrain tradeoff for CPU-limited machines.
It draws at most 25 terrain acres instead of the full grid, but can expose
distant gaps/pop-in and does not reduce resident buildings, trees, or actor
updates. Keep `vr_town_residency = 1` for the intended free-look experience.
Full-world state remains active until restart, so use separate launches when
comparing settings. Current defaults are unchanged.

Before reporting performance, record the CPU/GPU, headset, connection method,
refresh rate, per-eye resolution from `vr_log.txt`, and whether SteamVR is
reprojecting frames. Compare the same busy outdoor route, rain/night, interiors,
menus, and catching items. Minimum hardware requirements have not yet been
validated on a slower PC. Details: [performance review](docs/vr-performance-review-20260927.md).
