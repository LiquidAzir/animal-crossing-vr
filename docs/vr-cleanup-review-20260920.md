# Conservative bug review — 2026-09-20

Reviewed the recent building repairs, rendering state, sky/weather, running,
controller tools, net orientation, first-person catch camera, dialogue offsets,
and dock lifecycle. The current gameplay and appearance are the baseline.
Only two demonstrated defects required production changes.

## Fixes

1. **Fountain bubble rendering state.** The original bubble model calls a
   scrolling display list through segment B in both opaque and translucent
   passes. The actor only bound that segment in the later translucent pass,
   so the opaque bubble could inherit another actor's palette or display list.
   The PC actor now binds the existing scroll list before its opaque draw as
   well. Geometry, texture artwork, animation speed, order, and shadow callbacks
   are unchanged. A test using the actual actor source and original model lists
   reproduced 12 failures before the fix and passes all 312 checks afterward.

2. **Diagnostic model-viewer zoom.** Large distances and heights overflowed
   the signed 16.16 view-matrix translation. Native museum captures at 50,000
   and 75,000 units had identical cropped silhouettes. The viewer now rescales
   camera-space units only when translation exceeds the fixed-point range,
   and scales projection near/far planes with it. This preserves the intended
   projection without changing the game's camera or shared matrix conversion.
   The viewer's far plane also accommodates its long-distance interactive view.
   Normal-range matrices remain byte-identical.

No speculative tool, movement, camera, material, geometry, or sky adjustments
were made. No additional concrete regression was found in the reviewed paths.

## Verification

- The 32-bit build succeeds; log: `pc/build32/cleanup-audit/build.log`.
- New fountain actor test: `python pc/tests/run_vr_shrine_draw_tests.py` —
  312 checks, both seasons, building repairs on/off, three animation frames,
  and an unrelated earlier actor's segment binding.
- New viewer matrix test: `python pc/tests/run_model_viewer_matrix_tests.py` —
  2,048 camera cases and 443,377 checks. The `--unfitted` failing control
  reproduces 86,456 failures in the old packing path. Maximum tested structure
  projection deviation from the floating reference is 0.269 pixels.
- Existing residence, police/tailor, museum, post-office, fountain-geometry,
  and structure-routing suites pass: 58,468 checks.
- Tool targeting, gesture, net, and presentation suites pass: 34,510 checks.
- Native sky/weather regression passes 29 checks. Dock lifecycle (26),
  rock geometry (988), and cliff geometry (47) checks also pass.
- Six native captures on the final candidate succeed with empty stderr,
  covering normal viewer range, distant summer/winter museums, and seasonal
  fountain geometry. The normal-range post-office image is pixel-identical
  to the previous executable. The museum now fits fully into the image at
  50,000 units and becomes smaller at 75,000 as requested.

Local diagnostic artifacts and measurements are in `pc/build32/cleanup-audit/`.
Model-viewer captures validate geometry and camera behavior; the fountain's
actor-state correction is exercised separately by the actual-source actor test.
No physical headset playtest was performed.

The final executable reached the title screen in an isolated startup fixture
with empty stderr and no crash report. The test process was stopped afterward.

## Installation

Installed `../AnimalCrossing-VR/AnimalCrossing.exe`, SHA256
`07D2DC818D41A7D77E4696B01F5A2C1DFE7E206FD3A7DAED0D365EF397CFFD69`.
The previous executable, SHA256
`9112748101931F50B66BE2D52419CD0809490FEE6D01A51CF4958DFF54299562`,
and a verification receipt are in `../Backups/cleanup-review-20260920/`.
All 23 other installed files, including settings and saves, have unchanged
hashes. At the time of this local verification, no commit or push had been made.
