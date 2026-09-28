# Museum and post-office siding repair — 2026-09-20

The museum and post office still used the earlier reflected-model fill. That
replayed existing facades and roof detail across the missing sides instead of
giving these buildings fitted rear surfaces.

## Changes

- Museum: six fitted surfaces close its rear wall, foundation, shaped gable,
  roof underside, and narrow seams behind the front columns. The wall and
  foundation use opaque crops of the original stonework, with repeated detail
  and the museum's original fixed summer/winter body palettes.
- Post office: six patches close the two missing walls, four eave strips, and
  awning end/underside. The absent corner follows the original footprint and
  wall slope. Materials reuse its original plaster, timber, and brown backing
  with the actor's live seasonal palette.
- The post office's original window-light panel had one corner outside the
  newly completed wall. Only the submitted copy clips that corner inside;
  original assets, texture coordinates, other corners, commands, and tint
  callbacks remain intact.
- The shared fitted-surface builder now lives in
  `src/pc_structure_back_builder.c_inc`, with an optional fixed palette.
  Police and tailor repairs continue through the same builder and material
  definitions. Museum and post-office metadata live in separate data includes.

Museum originals draw once under the actor's original matrix. Post-office
originals and callbacks draw once through the existing owner-scoped joint-1
path. Repairs are gated by the existing building-completion setting and
VR/first-person mode. Neither repair uses `vr_solid_shell` scaling. No entrance,
door animation, clock, collision, movement, camera, or save behavior is changed.

## Verification

The 32-bit native builds succeed (`pc/build32/civic-palette-build.log` and
`pc/build32/civic-capture-build.log`).
Disc-backed tests execute the production builders against the user's original
vertices, indexed textures, and palettes. They check independently derived
contours, outward normals, coverage, opacity, deferred loading, cache reuse,
and byte-unchanged original assets. Live palette commands are also checked
for the exact untagged segment-8 address; fixed museum palettes retain native
pointer tagging. This catches a pointer-type error found and corrected during
the first native render pass before installation. Post-office tests also check
the actual lookup and light-corner helper, including malformed/unloaded guards.

| Test | Passed checks |
| --- | ---: |
| `python pc/tests/run_vr_museum_tests.py` | 25,286 |
| `python pc/tests/run_vr_post_office_tests.py` | 12,869 |
| `python pc/tests/run_vr_structure_draw_tests.py` | 123 |
| `python pc/tests/run_vr_structure_tests.py` | 12,728 |
| `python pc/tests/run_vr_house_tests.py` | 3,237 |
| `python pc/tests/run_vr_shrine_tests.py` | 4,225 |
| `python pc/tests/run_vr_presentation_tests.py` | 22,276 |

The draw-routing test covers one original draw/callback pass, deferred assets,
owner scoping, nested restoration, and both tailor/post-office light-list
substitution after the original tint callback. Postrender callbacks still see
the original list identity. Flat-view and unrelated-model paths are preserved.

Native model-viewer screenshots are in `pc/build32/civic-followup/`. Museum
entries 83/84 are summer/winter repairs; 85/86 retain the previous mirror only
for diagnosis. Post-office indexes remain 32/33. Capture mode expands its far
plane when a longer camera distance is requested, keeping broad museum roofs
inside the frame; interactive defaults are unchanged.

Model-viewer lighting differs from town gameplay. Physical headset checks for
day/dusk appearance and normal building interaction remain in `VR_PLAYTEST.md`.

The completed native regression covers 48 views: eight angles each for both
museum/post-office seasons, eight legacy museum comparison views, and eight
police/tailor checks. All exited successfully with empty stderr. This suite
uses candidate `0E24A6CF08D7A9D25CF2F0412AFB91989532982712F7904133613ABF23CDCF6C`.
The final executable changes only the diagnostic camera's maximum distance
from 50,000 to 100,000 model units; normal gameplay rendering and camera
defaults are identical. Some diagnostic views crop the widest roof edges
because the original fixed-point camera matrix limits effective zoom-out.
Invalid/non-finite diagnostic camera values still return exit code 2.

Ten further captures on the final executable verify both seasonal museums,
their legacy comparisons, both post offices, both police stations, and both
tailors. All pass with empty stderr. The final executable also reaches the
title screen in an isolated fixture with empty stderr and no crash report;
the test process was stopped afterward. No user save was used for these runs.

## Installation

Installed `../AnimalCrossing-VR/AnimalCrossing.exe` with SHA256
`9112748101931F50B66BE2D52419CD0809490FEE6D01A51CF4958DFF54299562`.
The prior executable, SHA256
`AE3CA13A77C6191152E18D45D0A4D9AE8283B02130AA95B48E418A2BE7BAB73F`,
is backed up with an installation receipt in
`../Backups/museum-post-office-20260920/`.

All 23 other installed files, including settings and saves, have unchanged
hashes. Close the game before restoring the backup. At the time of this local
verification, no commit or push had been made. Physical headset verification
was not performed.
