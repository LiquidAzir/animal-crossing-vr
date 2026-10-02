# Building materials and fountain repair — 2026-09-20

The follow-up headset screenshots showed that the earlier residence repairs
closed the holes but stretched small wall samples into plain panels. The police
station, Able Sisters, and fountain still used the older reflected-model fill,
duplicating entrances, signs, roofs, and tree branches.

## Repairs

- Residence rear walls keep their fitted geometry and now use cached 64x64
  indexed materials assembled from the original wall artwork. Detail repeats
  in model units. Plaster homes receive sampled timber outline, center, and
  eave trim; player homes no longer stretch a whole atlas strip into giant
  blocks. Blended boundary normals soften the lighting seam with the original
  walls. Original seasonal and house-color palettes still drive the colors.
- The police station adds one rear rectangle using its original rear vertices
  and wall texture. The original entrance, signage, and lamp draw once.
- Able Sisters adds its missing left/rear walls, rear gable, and two eaves.
  Its one missing floor-plan corner is derived from the other corners of its
  diagonal footprint. New surfaces use repeated red wall detail. The original
  door, sign, roof, windows, and actor callbacks are not duplicated. A copy of
  the original window-light quad moves only its hidden wall-touching edge
  slightly inward during the repaired draw, avoiding depth fighting at the new
  side wall; the original disc data and ordinary flat view are unchanged.
- The fountain removes its whole-fixture reflected pass. Small bark and stone
  closures follow the tree and stone backing's original open edges. The basin,
  foliage, figure, statue, and animated water retain their original draws.

These are render-only changes, active with building completion in VR or first
person. Door interaction, collision, saves, camera behavior, running, and net
alignment are not changed. The remaining legacy structures still respond to
`vr_solid_shell`; the fitted repairs do not use that scale setting.

## Verification

```powershell
python pc/tests/run_vr_house_tests.py
python pc/tests/run_vr_structure_tests.py
python pc/tests/run_vr_structure_draw_tests.py
python pc/tests/run_vr_shrine_tests.py
python pc/tests/run_vr_tool_tests.py
python pc/tests/run_vr_net_tests.py
python pc/tests/run_vr_presentation_tests.py
```

The disc-backed tests execute the production helpers against original vertices,
textures, and seasonal palettes. They check contour coverage, winding, bounds,
deferred loading, unchanged source data, cached reuse, and opaque materials.
The draw-routing regression verifies one original draw/callback pass for fitted
structures, including deferred assets and nested draws, and restoration of the
caller-owned temporary state. Existing tool, net, and presentation tests pass.

Results: 3,237 residence checks, 12,712 police/tailor geometry and material
checks, 76 draw-routing checks, and 4,225 fountain checks passed. Existing
tool targeting (1,077), gestures (39), net alignment (11,118), and presentation
(22,276) checks also pass. The 32-bit build succeeds; logs are
`pc/build32/structure-followup-final-build.log` and
`pc/build32/structure-followup-seam-build.log`.

Native model-viewer captures compare the original renderer's output at fixed
angles. Police entries 75/76 and fountain entries 77/78 cover summer/winter;
79–82 reproduce the previous police/fountain reflected fill for diagnosis.
Existing model indexes are preserved. These captures use diagnostic lighting
and omit gameplay callbacks, so window glow and the in-town time/weather state
are not a headset playtest. Local captures are under
`pc/build32/structure-followup/` and the relevant material/fountain test folders.

Physical checks for daylight/dusk colors, entrances, and fountain animation
remained pending at the time of this review.

The reviewed rendering pass covers all 18 seasonal homes, 22 police/tailor
views, and 12 fountain views. Six further summer/winter tailor captures verify
the final hidden light-edge inset at both 32,000- and 38,000-unit camera
distances. All captures exited successfully with empty stderr. The final build
also reached the title screen in an isolated startup test without a crash
report or stderr output; its test process was stopped afterward.

## Installation

Installed `../AnimalCrossing-VR/AnimalCrossing.exe` with SHA256
`AE3CA13A77C6191152E18D45D0A4D9AE8283B02130AA95B48E418A2BE7BAB73F`.
The prior executable (SHA256
`4A22858E5A7FD693632DC273B08222B8DF78871C4737A54B1BAA00B15B662B17`)
and an installation receipt are in `../Backups/structure-materials-20260920/`.
All 23 other installed files, including settings and saves, have unchanged
hashes. Close the game before restoring a backup. At the time of this local
verification, no commit or push had been made.
