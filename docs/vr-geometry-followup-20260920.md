# Geometry and net follow-up — 2026-09-20

The headset screenshots exposed faults that the previous build's checks did
not establish: the residence rear fill still overlapped original geometry, and
the net's visible hoop was not aligned by the old catch-proxy yaw correction.
This follow-up replaces those approaches and repairs the photographed cliff gap.

## Residence geometry

The old fill reflected an entire building around a measured bounding-box center.
That box includes porches, fences, and decorations. In house 1, for example,
the front wall is at local X=2000 and the open rear edge at X=-5000, while the
porch reaches X=6000. Reflecting the full model therefore puts copied surfaces
inside and across the original building, and duplicates doors and roof details.

All five villager house styles and four player-home sizes, in both seasons,
now use a small set of new faces sharing vertices with their actual rear edges.
House 5 also closes the underside of its overhanging rear eave and upper gable.
Original vertices, roofs, doors, porches, fences, decorations, and collisions
are not moved. The skeleton and actor callbacks run once; rear geometry uses
joint 1's frame after its child transforms unwind, with a separate frame matrix.
The temporary draw state is scoped to the owning keyframe and restored afterward.

Each cap uses a cached 32x32 CI4 crop from an opaque wall region of the original
atlas, with the home's live segment-8 palette. This avoids the atlas's transparent
door/window cutouts. Crops are checked under every palette, clamped for filtering,
and selected per season where necessary. In particular, winter house 3 uses its
unsnowed wall section instead of the summer bark region that becomes snow.

Rear-wall artwork is reconstructed from the existing textures; some rear walls
are plain. Other structure types still use the earlier approximate measured
shell, and their `vr_solid_shell` setting remains effective. That size setting
does not apply to the residence caps. The previous rock caps are unchanged.

The black panes in the user's screenshots are original unlit windows, not missing
texture data. House actor callbacks deliberately set their primitive color to
black when lamps are off. That lighting behavior is preserved.

## Net alignment

Both original net meshes place the shaft along tool +Z, with the hoop opening
along +X. The former -3000 Y trim canceled a bias belonging only to the stock
catch proxy; it did not fix the visible hoop and yawed the shaft off the hand.

Controller-held nets now roll -90 degrees around the shaft in the net draw.
The shaft keeps the controller's direction, while the hoop faces forward when
held upright. VR catch samples use the shaft direction. The stock catch-proxy
yaw remains for the animated-hand fallback. Regular/golden meshes, all seven
animations, and other tool paths are preserved.

## Cliff gap

The two ramp-acre meshes `grd_s_c4_s_1` and `grd_s_c4_s_2` omitted three triangles
on their cliff sides. Those triangles now use the original adjoining vertices,
UVs, normals, materials, and seasonal palettes. They close the gap shown under
the ledge without changing collision or global culling.

## Validation

```powershell
python pc/tests/run_vr_house_tests.py
python pc/tests/run_vr_net_tests.py
python pc/tests/run_vr_cliff_tests.py
python pc/tests/run_vr_presentation_tests.py
python pc/tests/run_vr_tool_tests.py
python pc/tests/run_sky_tests.py
```

- Residence tests exercise the production helper with original disc assets:
  rear contours, triangulation, winding/normals, bounded lists, opacity under
  all palettes, cache/deferred lookup, and unchanged source assets.
- Net tests execute the real item wrapper, matrix/keyframe functions, and net
  draw against both original meshes through all animation frames and varied
  controller poses. The earlier committed code fails the alignment regression.
- Cliff tests decode the compiled production lists and show that each repair
  closes three previously unmatched edges with opposite winding. The earlier
  revision fails the missing-face checks.
- Existing camera/dialogue, sprint/tool input, motion gesture, and OpenGL sky
  regressions pass.
- The 32-bit build passes. Logs: `pc/build32/geometry-fix-build.log` and
  `pc/build32/geometry-final-build.log`.
- The final executable reaches the title screen in an isolated flat-screen
  startup test with empty stderr and no crash report. The temporary test process
  was stopped afterward; no user saves were copied into the test directory.

The native model viewer now supports fixed-angle hidden-window BMP captures.
Its real display lists, textures, and renderer were used for 35 views across
the 18 seasonal models, with front/side and rear angles. Captures have successful
exit codes and empty stderr. The winter house-3 material correction was found
through these renders and rechecked in the final build. Local images/logs are
under `pc/build32/geometry-audit/gallery/`; the aligned old/new house comparison
is `pc/build32/geometry-audit/house1-rear-comparison.png`.

Example (run with a disc available under `rom/` in the working directory):

```text
AnimalCrossing.exe --no-vr --model-viewer 8 --model-viewer-solid
  --model-viewer-angle 225 --model-viewer-distance 30000
  --model-viewer-height 6500 --model-viewer-shot C:\absolute\house.bmp
```

Model-viewer windows use diagnostic lighting and show optional decorations;
these captures verify geometry/material placement, not full actor-driven
gameplay. Physical headset comfort, net feel, and in-town appearance remained
pending headset checks at the time of this review. No user saves are used in automated captures.

## Installation

The verified executable is installed in `../AnimalCrossing-VR/AnimalCrossing.exe`.
The previous version and `installation-receipt.json` are preserved in
`../Backups/geometry-followup-20260920/`. All 23 other installed files, including
settings and saves, have unchanged SHA256 hashes. Close the game before restoring
the backup.

- Installed SHA256: `4A22858E5A7FD693632DC273B08222B8DF78871C4737A54B1BAA00B15B662B17`
- Backup SHA256: `228B1E1D1223D8E8E802498FA4CC5695004204790C689893C2681826E1AC343F`

Earlier camera/dialogue changes remain included. At the time of this local
verification, no commit or push had been made. The older source checkout was
unchanged.
