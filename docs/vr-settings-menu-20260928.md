# In-headset VR settings — September 28, 2026

Both controller stick clicks together open a compact VR settings page during
play on SteamVR and native Quest. It uses the existing pause/font-panel system.
Left stick selects/edits, A/right trigger selects, B/left trigger resumes, and
Apply persists the four available options: hand visuals, first-person turning
(Smooth / Snap 30 / 45 / 90), motion swings, and master volume. Resume/B asks
before discarding unapplied edits. Hands remain off until explicitly enabled.

The page copies only those four fields on Apply. It bypasses desktop window and
graphics application; current resolution, quality, draw radius, and other settings
are preserved. Existing Gameplay/settings screens and bindings remain available.
Custom turning angles remain intact until that row is edited.

Start and Z retain their individual stick-click mappings. Both edges are delayed
180 ms to distinguish the combined shortcut; quick taps remain visible across the
game's two PADRead calls. The chord fires once until released. Pausing, closing,
and focus/tracking interruptions suppress gameplay input and pending tool gestures;
returning to gameplay requires neutral controls. Title and NES scenes retain the
existing pause restrictions. Existing recenter and first-person shortcuts remain.

## Verification

- Both native builds passed; PC reached the title screen in an isolated fixture
  with no crash or stderr.
- Each tree passed 598 settings/persistence checks and 69 pause-host checks.
- Input sequences passed 180 checks for desktop and 180 for native Quest,
  including duplicate polling, quick taps, focus recovery, and post-menu drain.
- Existing targeting/gesture checks passed (1,077 + 39 per tree), as did existing
  hand-settings/menu and binding/profile checks (39 + 19 per tree).
- Quest OpenXR lifecycle checks passed (4,068 plus 17 action binding checks).
- Native PC rendering passed 159 checks and produced eight actual menu captures
  with no GL errors. Rows and hints fit without overlap; gameplay stayed paused
  while editing/applying and resumed afterward. Captures are under
  `pc/build32/vr-menu-native-20260928-181116/fixture` in the PC source tree.
- The PC and Quest menu, font, and layout sources are byte-identical; Quest
  GLES/OpenXR panel placement still needs a headset check.
- Shared menu/input implementations and tests match between the separate trees.

A bounded Quest offscreen render mode was compiled for future menu testing.
It was not run because another headset app was active; that app was left running.
Physical controller/VR panel confirmation remains pending.

## Local installation

Installed the tested PC executable, SHA256
`97E48F3EF36492BAF751C61A9C3A2CCF06B6E91F272E80AC87233B875B249CAB`.
All 23 other installed files, including settings and saves, retained their hashes.
Receipts and executable backup use `vr-menu-20260928` in the PC build and backup
folders.

Installed Quest development APK version 11 using `install -r`, SHA256
`985f65d1ae6c5ba4861f0bfb339f3f4949fe4b9861611ba59f8cbb42ad64809d`.
The current Quest town was exported first; settings, ROM, and save match their
pre-install hashes. Previous APK and save export are in the separate workspace's
`backups`; the receipt is `research/install-v11.json`. The Quest app was not launched and
no public GitHub release was created by this update.
