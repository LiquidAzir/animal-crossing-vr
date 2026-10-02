# VR tool targeting repair

## Scope

Preserve the working v0.9.3-vr18 game, save format, renderer, controller bindings,
and flat/third-person controls. Repair the mismatch between headset direction
and the facing used to choose a tool target in VR first person.

## Confirmed cause

`pc_fp_camera_yaw()` already includes headset yaw for walking. Axe and shovel
target selection instead read the player's body rotation. The fishing rod
also projects its destination from body rotation. Turning the head without
walking therefore leaves those tools aimed in the previous walking direction.
The existing hand-pose override alone cannot repair these game-logic targets.

## Intended behavior

- Align body facing to the current horizontal gaze immediately before an axe
  or shovel target is chosen, when a rod cast is accepted, and when a net is
  readied/released. Both physical A and motion gestures use these same paths.
- Do not continuously rotate the player during an action. Keep the game's
  target selection, reach, collision, fishing, damage, and catch rules.
- Keep the net's existing hand-following catch geometry.
- Do not turn the player during desktop play, third-person VR, dialogue,
  menus, scripted events, or invalid headset tracking.
- Suppress synthetic swing presses during menus, pauses, and tracking loss.
  A continuous fast movement must not repeatedly fire; separate swings retain
  the existing 22 ms qualification and 350 ms cooldown.

## Validation plan

1. Build the untouched baseline with the installed 32-bit toolchain.
2. Exercise actual source functions with simulated headset/input state:
   compass headings, wraparound, accepted/rejected tool requests, held net
   release, cast direction, and flat/third-person/menu exclusions.
3. Exercise gestures at 72/90/120 Hz, tracking interruptions, repeated/held
   motion, physical-button coexistence, and the millisecond timer wrap.
4. Build the complete modified game. Preserve the old playable executable
   before installing the tested replacement; leave settings and saves alone.
5. Document the remaining headset playtest: look sideways/backward while
   stationary, use all four tools, then check menus, fishing, and flat mode.

Headset comfort, real controller calibration, and physical catch behavior
require a headset session; automated checks do not establish those results.

## Results — 2026-09-19

- The unchanged checkout at `7aa64bc` built successfully with the installed
  MSYS2 i686 GCC 16.2.0 and CMake 4.4 toolchain. Its executable was retained
  as `pc/build32/AnimalCrossing-baseline.exe`.
- The same targeting harness ran against functions extracted from that
  baseline: 170 of 251 assertions failed, reproducing gaze/probe direction,
  synthetic input leaking into invalid contexts, and partial stick movement
  while the tool-selection grip was held.
- The repaired source passes all 251 targeting/input/probe assertions and
  27 gesture assertions. Tests use real player structs, actual extracted
  production functions and simulated runtime/world-query seams. They are
  not a complete asset-dependent collision or fishing playthrough.
- The full changed game built successfully. The final collision-probe change
  rebuilt the affected player translation unit and relinked the game using
  CMake's existing dependency graph (`ac_pc/fast`).
- Isolated baseline and repaired executables started with the existing
  playable build's DLLs/assets. The repaired build initialized graphics and
  audio and continued rendering the title sequence. SteamVR reported
  `Hmd Not Found (108)` and correctly continued in desktop mode.
- Windows UI capture did not expose a targetable test window, so the native
  smoke result is based on process/runtime logs, not a visual menu playtest.
  The automated harness does separately exercise the actual pad merger's
  menu guards and preservation of physical A/B.
- The axe's draw-time collision probe now uses gaze yaw too. This retains the
  game's existing previous-frame collision pipeline, 35-unit reach, triangle
  width and 31-unit height. The net's hand-following geometry is unchanged.

Reproduce automated checks from the checkout:

```powershell
python pc/tests/run_vr_tool_tests.py
python pc/tests/run_vr_tool_tests.py --revision 7aa64bc
```

The second command is expected to fail: it is the regression baseline.
Build and test logs are in `pc/build32/`; no ROM or save is required for
these automated checks. Physical headset verification remained pending at the
time of this review. No GitHub release is published by
this local repair.

## Local installation

Update: the playable executable below has since been superseded by the
[outdoor sky build](skybox.md), which retains these motion-tool fixes.
The hashes and receipt below describe the original motion-tool installation.

The final executable was installed into the existing sibling playable folder
`../AnimalCrossing-VR/AnimalCrossing.exe`. Its SHA-256 is
`83d0aa89fa24bad7583772f0981215cc37562ad57e4318410cfd2688d4770e04`.
The final candidate ran through 5,821 logged audio frames and title rendering
in an isolated directory with no crash file or stderr output before the test
process was stopped. The installed file matches that tested executable.

The original executable is preserved at
`../Backups/motion-tools-20260919/AnimalCrossing.exe`. The adjacent
`installation-receipt.json` records both hashes and confirms all 23 other
files in the playable folder (including settings, saves, runtime libraries,
and assets) were unchanged. To roll back, close the game and copy that backup
executable over the installed `AnimalCrossing.exe`. Test processes are closed.

Source edits are in this current GitHub-matching checkout; the older
`../animal-crossing-vr` checkout remains untouched.
