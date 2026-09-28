# VR playtest fixes — 2026-09-27

This follow-up addresses invisible empty hands, Nook's Cranny's repeated door,
and catch/discovery items remaining beside the hidden player body.

## Empty hands

The installed settings already had `vr_empty_hands = 1`. SteamVR's September 27
log identified Oculus Touch controllers and successful loading of the installed
default bindings. Those bindings pointed the two new hand actions at
`/pose/grip`, which is absent from the installed Touch driver's input profile.
Loading the JSON successfully did not mean that those pose actions could work.

Both shipped Touch and Index bindings now use `/pose/handgrip`, supported by
their actual controller profiles. The existing tool-tip pose, input buttons,
haptics, and hand visibility rules are preserved. The binding test now validates
every shipped pose source against real SteamVR driver profiles, as well as
checking that existing mappings are unchanged. The old Touch binding fails
this new check; its failure is saved in
`pc/build32/vr-empty-hands-settings-tests/profile-baseline.txt`.

During eligible gameplay, the renderer also logs once if either optional hand
pose has never arrived after the startup grace period. This diagnoses a stale
or custom binding without suppressing the other working hand. Temporary
tracking loss after both hands have been seen does not trigger this warning.
No user binding cache or SteamVR preference was reset.

## Nook's Cranny

Summer and winter `shop1` models now use fitted timber rear surfaces and roof
underside strips instead of reflected copies of their facades. There is one
original entrance, animated door, sign, and set of actor callbacks. Texture
crops use original seasonal artwork and the building's live palette. See
[Cranny geometry notes](vr-cranny-rear-20260927.md) for asset and capture details.
Later shop upgrades retain their existing model-specific behavior.

## Catch/discovery display

In active first-person VR, the presented object is drawn approximately 90 cm
ahead and 14 cm below the headset's gaze. Its position follows head rotation
and room-scale movement. Fish, firefly/spirit sprites, and tiny-catch arrows
use the headset-facing billboard orientation; regular three-dimensional
insects retain their own rotation and animation.

The change starts only after the find leaves the shovel or the insect leaves
the net, and covers their presentation/putaway stages plus caught fish. Only
the player's actual caught actor qualifies. Ordinary pickups, loose world
insects/fish, gifts, submenus, and desktop/diorama rendering keep their original
paths. Invalid tracking also falls back to the original drawing.

These are temporary draw transforms. The code does not replace actor positions,
the animated hand/rod transforms, net probes, collision targets, inventory state,
or item-exchange destinations. Existing scales, fish sway, effects, and lifetimes
remain. The CPU uses the most recent completed tracking sample, as tools already
do; perceived latency and placement still require a physical headset check.

## Local verification

- `run_vr_item_anchor_tests.py`: 3,699 checks of the actual backend function
  across world scales, player orientation/position, head yaw/pitch/roll,
  room-scale offsets, stereo visibility, and invalid-state fallback.
- `run_vr_item_display_tests.py`: 686 checks executing the actual find, insect,
  fish, and arrow draw code against instrumented graphics services. Includes
  transfer-frame boundaries, actor identity, menu fallback, putaway scaling,
  original state preservation, and headset-facing sprite submission.
- `run_vr_empty_hands_tests.py`: 1,979 runtime/player checks, including the new
  missing-pose diagnostic and unchanged tracking/visibility behavior.
- `run_vr_empty_hands_settings_tests.py`: 58 settings, mapping-preservation,
  and installed controller-profile checks.
- `run_vr_hands_render_tests.py`: 41 native OpenGL checks. Its intentionally
  injected shader failure is expected and confirms graceful fallback.
- `run_vr_shop_tests.py`: 12,935 checks of real seasonal Cranny geometry/materials.
  `run_vr_structure_draw_tests.py`: 133 callback/routing checks.
- Existing targeting, gesture, net orientation, and first-person camera/dialogue
  suites: 34,510 checks passed.

Actual headset visibility, catch comfort, and walking through the repaired
shop entrance remain in the focused route in [VR_PLAYTEST](../VR_PLAYTEST.md).

The full game build passed (`pc/build32/vr-followup-20260927-build.log`). The
final executable reached the title screen in an isolated fixture, with empty
stderr and no crash report. That test process was stopped; the installed game
was never launched against the user's save. Sixteen native shop views were
inspected; four repeated views using the final executable are pixel-identical
to the reviewed candidate. Original entrance/window views are also identical
to the previous installed build.

## Local installation

Installed the executable and the two corrected default binding files in
`../AnimalCrossing-VR/`. Final executable SHA256:
`1C6FE146040EF536D1339234B94DDE8A7EED83520F9AA26D92BDD491495EC4A3`.

The prior executable and both bindings are backed up in
`../Backups/vr-followup-20260927/`, with `installation-receipt.json`. All 21
other installed files, including settings and saves, retain their original
hashes. `vr_empty_hands = 1` remains enabled. Installed control mappings were
compared before replacement: only the two optional hand pose paths change;
the manifest, tool pose, buttons, and haptics are unchanged. At the time of this
local verification, no commit or push had been made for this follow-up.
