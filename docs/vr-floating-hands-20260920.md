# Optional floating hands — 2026-09-20

Adds two simple cream-colored mitten hands for empty-handed first-person VR.
They follow controller grip position and orientation, with mirrored thumbs,
no arms, no finger tracking, and no additional input or collision behavior.

## Scope and safeguards

- `vr_empty_hands` defaults to `0` as of September 28, 2026. Settings → Gameplay → VR empty hands
  changes it through the existing Apply flow without restarting.
- Separate suggested `empty_hand_left` / `empty_hand_right` pose actions use
  `/pose/grip`. All previous buttons, haptics, and the tool's `/pose/tip`
  action retain their original mappings.
- Both hands hide while any item is equipped, in menus/pause/dialogue,
  during pickups, catches and scripted actions, and outside first-person VR.
  Ordinary walking, running, dash turns, and acre crossings remain eligible.
- CPU player drawing reports both empty item fields and allowed current/
  pending movement states for the exact current frame. Reports expire when
  the player is absent. They do not depend on a stale last-drawn tool index.
- Failed action updates, invalid/disconnected controllers, and invalid headset
  tracking suppress the corresponding poses. Invalid numerical transforms
  skip the affected draw and can recover next frame.
- The small static mesh is allocated lazily and drawn into both world depth
  buffers before UI composition. World surfaces occlude it. Drawing restores
  GL state, including the first resource initialization. Panel-only and NES
  submissions do not draw hands. Renderer failure disables only this feature.

Existing tool positioning, swinging, movement, camera, building geometry,
sky, and save-game logic are unchanged.

## Verification

- `python pc/tests/run_vr_empty_hands_tests.py`: 1,940 checks of production
  player eligibility and VR helpers. Covers all action/item states, pending
  actions, menus, stale frame stamps, independent tracking loss/recovery,
  malformed transforms, stereo matrix composition, and render-target restoration.
- `python pc/tests/run_vr_hands_render_tests.py`: 41 native OpenGL checks.
  Covers mirrored geometry, translation, GX depth conversion, clipping,
  world occlusion, depth writes, GL state restoration, lazy initialization,
  and injected shader failure/recovery. The deliberate shader-error log in
  this test is expected.
- `python pc/tests/run_vr_empty_hands_settings_tests.py`: 39 native settings/
  menu checks plus 11 JSON checks, including config round trips, Apply/discard,
  and preservation of every existing action/binding after removing the two
  additions.
- Existing tool targeting, gesture, net orientation, and first-person
  presentation suites: 34,510 checks passed. The net harness now includes the
  new read-only player eligibility helper; its original assertions remain intact.

Native preview artifacts are in `pc/build32/vr-hands-tests/`. They exercise
the production renderer with simulated poses, not a physical headset.
Actual controller fit, perceived size, and comfort still need a headset pass;
the focused route is recorded in `VR_PLAYTEST.md`.

The full 32-bit game build passed (`pc/build32/floating-hands-build.log`).
The final executable reached the title screen in an isolated fixture with
empty stderr and no crash report; its test process was then stopped. A native
post-office capture with the hand setting enabled in flat mode is pixel-identical
to the previous executable's capture. Comparison artifacts are in
`pc/build32/smoke-civic/floating-hands-{before,after}.bmp`.

## Local trial installation

Installed `../AnimalCrossing-VR/AnimalCrossing.exe`, SHA256
`E873F11E96BCD525B3C2F8113AC567FCDF3244E8187632A686D1BDD8C3261A27`, with
the updated three `vr_actions` JSON files and `vr_empty_hands = 1` appended
to the existing settings. The previous configuration's bytes are preserved.
Existing installed action mappings were compared before replacement; the only
semantic additions are the two optional grip poses.

The previous executable (`07D2DC818D41A7D77E4696B01F5A2C1DFE7E206FD3A7DAED0D365EF397CFFD69`),
settings, and bindings are backed up in `../Backups/floating-hands-20260920/`,
alongside `installation-receipt.json`. All 19 other installed files, including
saves, retain their prior hashes. At the time of this local verification,
no commit or push had been made.

### Default-on follow-up

At the user's request, the compiled default and newly generated settings now
enable floating hands. Existing configurations without the key inherit ON;
an explicit OFF preference and the Gameplay toggle still work. The settings
suite passes its updated 39 checks plus 11 unchanged binding checks. The full
build passed (`pc/build32/floating-hands-default-on-build.log`).

The updated executable is installed, SHA256
`09187871D4440D4551F15832586B4D032778C290A1ED61DF18FBEA9B6123CC16`.
Its predecessor and installation receipt are in
`../Backups/floating-hands-default-on-20260920/`. The local setting remains ON;
all 23 other installed files, including settings and saves, are unchanged.


## Empty hands disabled again — September 28, 2026

At the tester's request, empty-hand markers are now off by default in both
versions (`vr_empty_hands = 0`), including the Quest first-install settings.
The installed PC and Quest settings were also changed from 1 to 0; all other
settings and saves were preserved. Controller input, aim/tool tracking, motion
swings, and the rod direction correction are unchanged. The existing optional
Gameplay toggle remains available. No hand rendering code was removed.

Both native builds passed, as did the existing focused settings/menu, runtime,
player-eligibility, and Quest OpenXR checks. The PC build reached the title screen
in an isolated startup test without a crash or stderr. The tested executable
and Quest development APK version 10 are installed locally. PC receipts and
backups use `vr-hands-off-20260928`; Quest receipts are `research/install-v10.json`
and `research/disable-hands-v10.json` in its separate workspace. Previous binaries,
settings, and a fresh Quest save export are retained there.
