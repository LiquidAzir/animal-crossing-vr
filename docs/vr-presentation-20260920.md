# First-person catches and dialogue — 2026-09-20

## Changes

`Camera2_SetView` now includes ITEM among its first-person camera modes. Fish
catches, bug catches, shovel discoveries, and other item presentations use this
mode. Their stock animation, inventory updates, messages, and camera bookkeeping
continue, while the rendered view stays at the player's eye position with the
existing gaze. This also avoids toggling first-person world scale or showing the
player's body during the presentation. Desktop first person gets the same fix;
with first person disabled, the original item camera remains.

ITEM also sets the existing dialogue input guard. Synthetic motion-tool presses
cannot advance its messages; physical A remains available. Other scripted camera
modes, including preview pans and the title attract sequence, retain their stock
behavior.

`mMsg_Draw_Window` raises dialogue by 32 pixels in the original 320x240 UI space
only during VR first person, excluding flat menu scenes. The body, nameplate,
continue indicator, message text, and choices move together. The message and
choice centers are restored exactly after drawing so their animation state
cannot accumulate the offset. Font/voice side effects still operate on the
original message object. The overall UI panel and desktop dialogue are unchanged.
This is a fixed vertical adjustment, not a world-space bubble attached to an NPC.

## Verification

- Full 32-bit build passed: `pc/build32/fp-dialogue-build.log`.
- `python pc/tests/run_vr_presentation_tests.py` passed. The harness extracts
  production camera, view, gesture gate, and dialogue draw functions and uses
  instrumented rendering/runtime seams. It covers NORMAL → ITEM → NORMAL at
  sixteen headings in VR and desktop first person, sustained presentations,
  player movement, supported/excluded camera modes, title/disabled/missing-player
  guards, and repeated dialogue draws across VR/FP/menu combinations. It checks
  restored layout and retained font side effects. Running against the previous
  commit (`--revision 26d58b6`) reproduces the regressions.
- Existing tool/input/movement and gesture suites passed: 1,077 + 39 checks.
- Native OpenGL sky suite passed: 29 checks.
- An isolated `--no-vr --verbose --time 12` run reached the title and attract
  sequence with empty stderr and no crash report. It used no user saves.
  Logs are in `pc/build32/smoke-fp-dialogue/`.
- `git diff --check` passed.

These checks do not verify catch artwork or dialogue clearance on a physical
headset. `VR_PLAYTEST.md` includes fish/bug/fossil catches, full pockets, deliberate
A versus gestures, repeated conversations and shop choices, and flat/diorama
regressions. The item presentation still uses its original artwork/animation.

## Installation and rollback

Installed executable SHA-256:
`228b1e1d1223d8e8e802498fa4cc5695004204790c689893c2681826e1ac343f`.

Previous executable: `../Backups/fp-dialogue-20260920/AnimalCrossing.exe`, SHA-256
`b7747f751577a83058e5b9c1d1113ce284e58c97e0db4c644f7578000ff74c7d`.
The adjacent `installation-receipt.json` confirms the candidate and installed
hashes match and all 23 other files, including saves/settings, are unchanged.
Close the game before restoring the backup. The older source checkout was not
modified. These changes followed commit `26d58b6`. At the time of this local
verification, no commit or push had been made for this follow-up.
