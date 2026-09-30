# Island houses and crossing surfaces — September 29, 2026

The user confirmed that the repaired boat trip reaches the island, then reported
open backs on both island houses and missing surfaces on docks and bridges.
These models had faces omitted for the original fixed camera. Texture filtering
or disabling backface culling cannot fill geometry that was never authored.

## Island houses

`src/pc_island_house_back.c_inc` recognizes the cottage and islander's skeletons
in the existing owner-scoped building repair route. Their original wall contours
receive fitted rear walls; their thatched roofs receive undersides. The cottage's
missing fourth corner follows its diagonal footprint. The islander's rear wall
follows the original taper. The two houses add 30 triangles in total.

The added faces sample the original bamboo, plaster, and thatch textures and use
the building's live palette. Material coordinates repeat the samples rather than
stretching a single color over the building. The cached display lists append in
the correct skeleton frame. Original entrances, windows, fences, animations,
callbacks, and collision data remain in place. An unloaded recognized model
waits for its assets instead of falling back to a reflected entrance.

Both seasonal cottage actor configurations use these same two island models.
The repairs depend on model identity, not a particular save or town location.

## Docks and bridges

The crossing repair targets an explicit list of 31 authored dock and bridge acre
models. The audit covers all 35 variants; four stone bridges already have both
sides and need no additions. Supplemental faces use the original vertices and
live wood/stone materials. Original ground display lists and collision data
remain unchanged. The added surfaces are cached and submitted under the acre's
existing matrix in first-person/VR. Ordinary flat-camera rendering keeps the
original mesh. Mainland docks add 30 triangles and island docks add 32; the
largest addition is 48 triangles in one acre.

The separate animated wooden bridge receives only its four missing stationary
post backs, eight triangles per seasonal model. Its original suspended deck,
deformation callback, and joint matrix allocations are retained.

## Verification

The island-house tests load the user's original disc geometry and palettes;
they verify the missing contours, roof closure, nondegenerate triangles,
material opacity and variation, source-array preservation, and deferred loading.
The structure-routing tests check single submission, callback preservation,
owner state, and the absence of reflected entrances. Existing mainland building
regressions also pass after adding the island lookup.

- Island houses: 7,463 geometry/material checks in each source tree.
- Animated bridge posts: 8,399 checks per tree, with 153 shared structure-routing
  checks. Eight seasonal renderer comparisons preserve the sampled deck/rope
  pixels exactly while closing the posts.
- Fixed crossings: 4,630 geometry checks cover all 35 variants and 752 complete
  wooden faces; 214 checks exercise the real ground submission function in flat,
  first-person, and VR modes, including unloaded and unknown models.
- Mainland residence, police/tailor, museum, post-office, and Cranny geometry
  regressions pass.
- The PC game renderer passed 29 climate/controller transition checks, including
  the provider gap that caused the previous island-travel crash. Evidence:
  `pc/build32/island-transition-native-20260929-233001`.
- Native model-viewer comparisons cover the two island houses, all seven dock
  variants, and representative wooden/stone bridge layouts in summer/winter.
  Low-angle captures check the undersides. The private diagnostic executable
  changes only the viewer pitch; production geometry and rendering are used.
  Evidence: `pc/build32/island-surface-review` in the PC source workspace.

Model-viewer captures check surfaces and materials; they do not replace a
physical headset walkthrough or the complete boat round trip. The viewer's white
window lighting placeholders and single-sided fence details are original
diagnostic behavior, not part of this geometry repair.

The final Quest ARM32 library passed 23 actual GLES/actor climate-transition
checks on the headset through an isolated offscreen fixture. Three rendered
captures had no GL errors; 14 houses continued drawing through two absent-provider
frames in each direction. Receipt:
`../research/island-transition-offscreen-20260929-232853` in the Quest workspace.
An earlier attempt exited before the test report and was inconclusive. A separate
Windows test-log encoding failure was corrected with explicit UTF-8; subsequent
runs completed. No production runtime changes were needed for the test adapter.

## Installed builds

- PC executable SHA256:
  `734c32707c2d24738f0b20f8084a0fc06fb61c63c5ef88b960476fe5d9628f16`.
  The other 23 installed files retained their hashes. The previous executable is
  in `Backups/island-surfaces-20260929` under the PC workspace. Receipt:
  `pc/build32/island-surfaces-install-20260929.json`.
- Quest APK build **14**, same **Animal Crossing** app and signing identity:
  `78ac5edbd91b385c6c23f6619d59d53d26c971cf0e42b33744afef9d7af07086`.
  Native library SHA256:
  `ddea7d107c34f49b66e266c34639efa95524d442319efc8d333321e465afe276`.
  Installed ROM, save, and settings hashes are unchanged. Fresh save export and
  previous APK are in the Quest workspace's `backups` directory. Receipt:
  `../research/install-v14.json`.

The installed games were not launched or their personal saves loaded by this
update. Physical headset inspection of the repaired island remains pending.
