# Isolated island controller transition probe

Coordinate an idle GPU slot before running:

```text
python quest/tests/run_island_transition_offscreen.py --serial DEVICE_SERIAL
```

Add `--compile-only` to build without any device access. To reproduce the
pre-fix failure, select a preserved library with `--libmain PATH --expect-crash`.
The expected-crash mode requires exit 139 after the missing-controller draw
begins and before it completes. Other failures are not accepted as reproduction.

This test reuses `game_offscreen_profile.c` for the native ARM32 boot/audio/EGL
host and interposes only diagnostic callbacks. It loads the actual built game
library and its assets in a fresh `/data/local/tmp/acquest-island-transition`
fixture. It uses empty save folders and does not install an APK, launch an
Activity, read/write installed app data, or simulate physical XR input. It
checks the game is stopped and refuses a non-Home resumed activity before
transfers or execution. The process has a 50-second alarm.

With actual resident town houses, the test invokes the boat's climate/summer/
background-controller replacement operations, lets normal game actor updates
and the renderer execute, and then reverses the season transition. It checks
both missing-provider intervals, continued house body drawing, controller
recovery, and nonempty native captures with no GL errors.

The probe exercises controller lifetime in title scenery. It does **not**
navigate boarding/dialogue, load the island field, or verify return boarding.
Those remain separate playtest requirements.

On September 28 the harness compiled successfully using NDK27 ARM32. The device
run was skipped during preflight because another game was active; no diagnostic
was pushed or executed. The user subsequently requested finishing the update
and testing later. Accordingly no Quest native-render pass is claimed.
The corresponding PC actual-render test reproduced the old null-shadow crash
and passed 29 checks with the fix in both transition directions.
