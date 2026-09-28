# Hand markers, aquariums, and rod alignment — September 28, 2026

These three changes apply to both the PC VR tree and the separate Quest tree.
The preceding first-person fishing, hand-depth, and shop-rug fixes remain intact.

## Changes

- Empty hands use one small rounded white oval per tracked grip. The extra thumb
  bump is removed. Visibility rules, grip offsets, occlusion, and controller
  tracking are unchanged. The static mesh now uses half as many triangles.
- Museum fish-room tanks, plants, and donated fish bypass the original 320×240
  camera's draw-only visibility check when full-world rendering is active. The
  actual eye views decide what is visible. Donation checks and fish behavior are
  unchanged. The large tank's reflection also uses the correct position entry
  instead of reading beyond its five-entry array.
- Controller-held ordinary and golden fishing rods receive a 90-degree roll
  around their shaft. Their existing casting bend now lies in the vertical plane.
  The straight-ahead cast target, animated line attachment, other tools, and
  ordinary avatar-held rendering are unchanged.

## Verification

Both trees passed the focused native suites:

- 42 hand-render/state checks and 118 world-depth checks.
- 22,994 aquarium checks covering full-world and original-camera modes, actual
  tank submissions, plants, donation-gated fish, and the reflection position.
- 82,360 rod geometry checks using original ordinary/golden skeletons and cast
  animations, plus 11,118 net, 1,077 tool-targeting, and 39 gesture checks.

The aquarium and rod regressions fail against their preceding implementations.
Quest's actual GLES device passed 108 hand/world-depth checks. Its complete native
game build passed a two-eye offscreen startup with distinct eye images and no GL
errors. These checks simulate controller poses; they are not physical playtests.

## Quest draw-distance decision

Kept `vr_draw_radius = 0` (whole town). No PC performance settings were changed.

An isolated Quest comparison of the current whole-town setting against radius 2
(a 5×5 acre window) reduced render time in the synthetic title scene. Two north
views measured about 22–25 ms for the whole town versus 17 ms for radius 2; a south
view measured 33 ms versus 25 ms. These are short, uncapped 640×640 stereo tests
without OpenXR, not headset frame-rate claims or a controlled performance target.

The south-facing screenshots show why radius 2 was rejected as a default: distant
trees and buildings remain above missing ground. The setting suppresses terrain
and water independently of actor residency, and whole strips change immediately
at acre boundaries. VR fog does not conceal the gaps. Preserving the currently
playable experience is preferable to this visual regression.

The Quest-only test runner now accepts `--draw-radius` and `--yaw` to reproduce
this comparison in disposable fixtures. No game setting was changed by the tests.
Receipts and both-eye captures are under the separate Quest workspace's
`research/game-offscreen-20260928-*` directories.

## Focused headset route

1. Put the tool away indoors and outdoors. Both white markers should follow the
   grips, appear against scenery, and remain occluded by nearby solid objects.
2. In the museum fish room, turn fully around and walk between small and large
   exhibits. Tanks, donated fish, and plants should remain visible when looking
   back; ordinary fish animation and glass/water should continue.
3. Hold the rod level and cast using both the button and motion gesture. Its bend
   should be vertical with the line attached to the visible tip. Wait for a bite
   and catch a fish; the first-person camera and presentation should persist.

Repeat on PC and Quest. Physical confirmation of these new changes remains open.
No GitHub release is created by this local follow-up.

## Local installation

The combined MinGW PC build passed and reached the title screen in an isolated
fixture with empty stderr and no crash. Installed only the tested executable in
`../AnimalCrossing-VR/`; all 23 other installation files retain their hashes,
including settings and saves. Its SHA256 is
`0AA362BE24C11B53ABD3131A2C15CB6C99EF024B56A4E5B95D37758C24E130E8`.
The previous executable is in `../Backups/vr-comfort-20260928/`; receipts are in
`pc/build32/vr-comfort-20260928/` of the PC source tree.

Quest development APK version 8 is installed under the existing package with
`install -r`. Its SHA256 is
`839a1b11576ba4edaa9d4208bdfdb1341415fa873121a6a9ecb795777992d6ec`.
The current Quest town, settings, and ROM match their pre-update hashes. The
separate Quest workspace retains a save export, previous APK, package receipt,
and `research/install-v8.json`. The game was stopped during both installations.
