# Fishing, hand depth, and shop rug fixes — 2026-09-28

This follow-up addresses three problems reported during headset playtesting.

## Fishing camera

Landing the lure requests the game's SIMPLE camera, which was excluded from
the first-person view override. Fishing wait/bite and return requests now use
a distinct SIMPLE mode that remains eligible for first person. The original
camera calculations, priorities, and transition durations still run, so
toggling first person during a cast continues to work. Other scripted SIMPLE
cameras retain their original behavior.

The presentation regression suite passes 23,247 checks, including actual
player fishing callbacks and camera requests. The old implementation fails
336 of the new checks.

## Hands against scenery

The native hand shader converted GX clip depth to conventional OpenGL depth,
but world geometry uses the GX convention directly. This placed the hands
behind distant scenery in the depth buffer despite their correct positions.
The difference explains why they appeared against empty black backgrounds.

Hands now use the lower half of each eye's recorded world viewport depth
range. The shader's clip conversion remains, preserving near/far clipping.
Capturing the world range before UI drawing also preserves the game's actual
0..1022/1023 viewport range. The hand pass restores framebuffer, viewport,
and depth state afterward, including failure paths. Tracking, mesh shape,
default-on settings, and tool/menu visibility rules are unchanged.

- Runtime/player eligibility suite: 2,119 checks passed.
- Existing native hand-rendering suite: 41 checks passed.
- New integration with the real world shaders: 118 checks passed, versus 14
  failures with the old renderer. It covers distant scenery, near occluders,
  partial occlusion, draw ordering, asymmetric stereo projections, scale,
  clipping distances, and viewport depth ranges.

## Shop rug shimmer

The displayed rug contains separate front and back sheets at identical
positions, with different UVs. The general two-sided rendering override drew
both sheets together, making the two patterns fight for the same depth pixels.
The original rug texture itself has no checker pattern. The recent performance
changes did not alter texture filtering or depth rendering.

Only the displayed rug's draw list now preserves its authored face culling.
The exception ends immediately after the rug's triangles and is also cleared
at the end of an aborted display-list task. Ordinary world geometry keeps its
two-sided rendering, including the building-back repairs. No additional
render pass, resolution change, filtering change, or antialiasing cost is added.

The actual-source culling/scope/lifecycle suite passes 371 checks; the old
implementation fails 52 of those checks. A native OpenGL test using the
original rug mesh and texture passes 28 checks over five front/back angles.
Its ten comparison images retain the complete silhouette while removing the
competing surface artwork. The tests also cover ordinary unmarked geometry,
model-viewer overrides, wireframe behavior, and interrupted task cleanup.

## Verification scope

The native tests use real OpenGL rendering; controller input is simulated.
Final physical headset confirmation remains on the focused route in
[VR_PLAYTEST](../VR_PLAYTEST.md). Test artifacts remain in ignored directories
under `pc/build32/` and are not release assets.

The combined MinGW game build passed. The final executable reached the title
screen in the isolated flat startup fixture, with empty stderr and no crash
report. Only that owned test process was stopped; no real user save was opened.

## Local installation

Installed the tested executable in `../AnimalCrossing-VR/AnimalCrossing.exe`.
SHA256: `05B528DD92F87BD89FF5E8FA88EC8986E6F3EFEC8C1A6CAE705FCE598A5AB480`.
All 23 other installed files retain their original hashes, including settings,
saves, controller bindings, and shader files. The previous executable is in
`../Backups/vr-fishing-hands-20260928/`, with an installation receipt.

Build, startup, and installation receipts are in
`pc/build32/vr-fishing-hands-20260928/`. This follow-up is locally installed;
the published VR19 release has not been replaced.
