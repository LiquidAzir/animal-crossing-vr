# Sky and motion-tool bug-fix round — 2026-09-19

This pass reviews the local skybox and motion-tool work. It leaves the game's
collision dimensions, controller bindings, save format, and cloud artwork intact.

## Reproduced and repaired

1. **Invalid controller velocity retained virtual A.** A NaN, infinity, or
   negative speed cleared gesture qualification but left the second frame of
   a fired press active. Invalid speed now cancels the remaining press at
   once, preserving the cooldown and the requirement to slow before rearming.
2. **A gesture could use a tool while selecting another.** Holding left grip
   redirected the stick to the D-pad, but did not suppress pending gestures.
   Both gesture qualification and pad injection now exclude that grip.
   Physical A and D-pad selection remain usable.
3. **Refused axe/shovel actions still rotated the player.** Gaze alignment
   ran before target selection, but requests can subsequently be refused by
   the existing action-priority/cancellation rules. Both original angles are
   now restored on failure. Accepted tree, reflect, air, broken-axe, dig,
   fill, and pickup requests retain gaze alignment.
4. **Pausing could revive stale outdoor sky state.** The old pause guard
   bypassed environment-age checks entirely. It now freezes the last valid
   answer; expiry is also evaluated at pass start, including all-2D scenes.
   Pausing an active outdoor scene still keeps the sky visible.

## Verification

The added regression cases were run against the pre-fix source first:
13 targeting/input failures, four gesture failures, and one sky failure.
Those logs are `pc/build32/bugfix-before-tools.log` and
`pc/build32/bugfix-before-sky.log`. The four original production files are
archived in `pc/build32/bugfix-before-source.zip`.

The expanded final suite passes **375 checks**:

- 307 targeting/input checks execute extracted production functions with
  real player structs and simulated world/runtime services.
- 39 gesture checks cover timing, cooldown, rearming, tracking interruptions,
  timer wraparound, and invalid velocity during an active press.
- 29 native OpenGL checks exercise the actual sky shader, per-eye projection,
  GL state/depth preservation, weather/time colors, and pause/scene lifecycle.

Run from the checkout:

```powershell
python pc/tests/run_vr_tool_tests.py
python pc/tests/run_sky_tests.py
```

Final test logs are `pc/build32/bugfix-tools.log` and
`pc/build32/bugfix-sky.log`. The sky-only GPU measurement on the RTX 5090
was 0.048 ms per 2048x2048 pass (32-pass average). That excludes the rest
of the game and SteamVR. The runner now prints gesture stderr on failure.

The complete 32-bit game build passed using the existing CMake dependency
graph. It rebuilt `pc_vr.cpp`, `pc_sky.cpp`, and the player translation unit;
the build log is `pc/build32/bugfix-build.log`. `git diff --check` also passed.

These checks do not replace an in-game or physical headset playtest. In
particular, test tool switching while moving the right hand, interrupted or
refused actions, and pause/resume around outdoor-to-menu scene transitions.

## Installation and rollback

The tested executable is installed at `../AnimalCrossing-VR/AnimalCrossing.exe`.
SHA-256:
`0919d44ba63b01c328a01a3af2d7ef2d345dedd06d77a8b4a9d09ea1a1b6fe3f`.

The previous sky-and-motion-tools executable is backed up at
`../Backups/bugfix-20260919/AnimalCrossing.exe`. Its adjacent
`installation-receipt.json` records both hashes and confirms that all 23
other installed files, including settings and four save files, were unchanged.
Close the game before copying that backup over the installed executable.

The final candidate reached the title screen in `pc/build32/smoke-bugfix/`
with no stderr or crash report. SteamVR reported `Hmd Not Found (108)` and
fell back to desktop mode. This startup result is based on logs, not a visual
gameplay test. The test process is closed. Work is local and uncommitted;
no push or release was performed.
