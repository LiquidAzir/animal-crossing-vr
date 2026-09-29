# Isolated native Quest game diagnostics

`game_offscreen_profile_run.py` builds a small ARM32 shell host, copies an existing
game library and the personal staged ROM into its own `/data/local/tmp` directory,
then calls the real asset loader, `ac_entry`, `boot_main`, game logic and GX renderer.
It does not rebuild the game, install an APK, launch an Activity, or open an XR session.
It refuses to run while the installed Quest game has a process.

The default mode enables the same full-world flag as Quest and renders one flat
640x480 view. `--stereo` instead runs the real `graph.c` two-pass replay with exact
production matrix, eye-target, UI-routing and panel helpers extracted at build time.
The host supplies fixed test poses (64 mm IPD, 90 degree FOV, 20 degree downward
pitch), two square targets and no runtime pacing. The generated source is saved in
the receipt. These measurements exclude runtime/compositor/tracking work and are
not headset frame-rate claims.

Examples from the source directory:

```powershell
python quest/tests/game_offscreen_profile_run.py --stereo --no-profile
python quest/tests/game_offscreen_profile_run.py --stereo --eye-size 1760 --no-profile
python quest/tests/game_offscreen_profile_run.py --stereo --libmain ../research/profile-v6/libmain.so
python quest/tests/game_offscreen_profile_run.py --stereo --compile-only
python quest/tests/game_offscreen_profile_run.py --stereo --no-profile --draw-radius 2 --yaw 180
```

`--no-profile` disables production per-draw timers and reports wall time for 90
world frames after 30 warmup frames. A world frame has over 100 actual GX draws.
Use `--warmup-frames 60 --sample-frames 300` for a longer comparison. The receipt
records both requested counts; the log reports actual samples and average draw,
command, and vertex-load counts so unequal workloads are visible. A run that hits
the time limit before collecting the requested samples is not a complete sample.
The test exits within 40 seconds overall, with a 50-second failsafe. Inspect the
reported sample count before comparing runs. Initial shader compilation is excluded
from the steady aggregate. `--stock-world` provides the smaller flat-world control.

Timing comparisons should use identical eye sizes, view directions, draw radii
and sample windows. Keep diagnostic verification work outside timed runs, compare
workload counts, and alternate ordering when confirming a promising result.

`--draw-radius 0` retains the whole town; values 1–10 select a terrain acre radius
in the disposable test settings only. `--yaw -180..180` rotates the synthetic
headset view in stereo mode. Inspect both forward and backward views: radius 2
can leave distant actors above missing terrain even when the forward screenshot
looks intact. The installed game settings and production defaults are untouched.

For save navigation, `--save-copy PATH.gci` copies only that explicitly selected file
into disposable test Slot A and records the source hash before and after the run.
The game may write its temporary copy; the original source and app-private saves
are never opened for writing. `--pad-script PATH.json` supplies synthetic input and
intermediate mirror captures. Script frames begin at the first world draw and
continue through menus/transitions. Navigation uses a minimum 60 Hz host pace so
real-time fades can finish; it is marked as navigation, not a performance benchmark.

```json
{
  "stop_frame": 600,
  "events": [
    {"frame": 120, "duration": 4, "buttons": ["START"]},
    {"frame": 320, "duration": 4, "buttons": ["A"]}
  ],
  "captures": [299, 420, 599]
}
```

Buttons are `A`, `B`, `X`, `Y`, `START`, `L`, `R`, `Z`, and the four directions.
Events can also specify `stick_x`/`stick_y` in -100..100. Up to 128 events and captures
are supported. Scripts select normal game choices; they do not bypass game state.
Always inspect intermediate screenshots to verify the reached scene.

Receipts are in the parent workspace's `research/game-offscreen-<timestamp>`:
binary hashes, settings, logs, generated adapter source, final mirror, stereo eyes
and requested intermediate captures. Audio sample generation runs on a native
thread, but device playback is discarded because Android SDL audio requires JNI.
No physical controller input is consumed. GPU/CPU test slots should be coordinated
with other diagnostics and the user's headset session.

The dedicated settings check uses no save file:

```powershell
python quest/tests/game_offscreen_profile_run.py --stereo --vr-menu-test
```

It opens the actual VR settings API during `PADRead` in the disposable title
play scene. The harness alone clears the title-menu guard to reuse that scenery;
production still blocks this shortcut at the title menu. It does not test the
physical two-stick chord, which has separate input tests. Synthetic navigation
edits all four settings, checks that Apply commits them, and selects Resume.
Only the temporary fixture's settings file is written.

The test checks that the actual gameplay frame remains frozen, then changes
synthetic head yaw from 0 to 35 degrees and back while paused. The raw UI must
stay identical, both eye images must change with yaw, and they must recover
exactly when yaw returns. Captures include the production 1280x960 UI texture,
320x240 inspection thumbnails, and both eye targets at every scripted checkpoint.
This checks native menu rendering and continued view updates, not live OpenXR
tracking or controller hardware. A test failure retains earlier captures for
inspection. Use `--compile-only` to prepare the harness without accessing a device.
