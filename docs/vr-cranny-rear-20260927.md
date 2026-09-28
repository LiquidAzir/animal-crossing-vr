# Nook's Cranny rear geometry repair — 2026-09-27

The original summer and winter Cranny models (`shop1`) contain the entrance
wall and adjacent window/gable wall, but omit the other two exterior walls.
The previous VR fallback reflected the entire skeleton to hide those openings,
which repeated the entrance, animated door, sign, and window details.

Both Cranny models now use fitted rear surfaces through the existing structure
repair path. The original skeleton and its actor callbacks draw once. Two new
timber walls meet the authored footprint and gable; six small roof underside
strips join the original eave and wall-top boundaries. There are seventeen new
triangles, with no reflected building, new doorway, or replayed actor callback.

The timber uses an opaque crop of the original seasonal front-wall atlas,
excluding doors, lettering, and window cutouts. Its UV direction and repeat
density follow the existing walls. Dark original trim supplies the undersides.
All patches use the live structure palette in segment 8. The original vertices,
display lists, roof/snow artwork, door animation, lighting callbacks, and
collision remain unchanged.

This is limited to Nook's Cranny in both seasons. Later shop upgrades have
different authored geometry and retain their prior rendering. The usual solid
building setting and VR/first-person gates still apply; ordinary flat rendering
does not receive these patches.

## Local verification

- `python pc/tests/run_vr_shop_tests.py`: 12,935 checks passed against actual
  disc assets. Checks cover independently derived contours, triangle coverage
  and winding, all material texels' seasonal opacity, live palette encoding,
  original asset preservation, bounded display lists, stable stereo caches,
  and recognized-but-unloaded behavior.
- `python pc/tests/run_vr_structure_draw_tests.py`: 133 checks passed, including
  shop-specific routing that draws the original model/callbacks once, with no
  legacy reflection while assets are loaded or deferred.
- Baseline native model-viewer images were captured from installed executable
  SHA256 `09187871D4440D4551F15832586B4D032778C290A1ED61DF18FBEA9B6123CC16`,
  using an isolated fixture with no player saves. Matching candidate captures
  use models 0/1 (summer/winter), angles 45/135/225/315, distance 35,000, and
  target height 6,500. Capture manifests and images are local under
  `pc/build32/shop-followup`.
- The matching candidate captures used SHA256
  `007AA02BE5A758252C1AAB69FC7B860D6CC02F6E2DFE4BCC0C27BCE981E0FEA6`.
  Both seasonal rear views show closed timber walls with no second entrance;
  winter snow remains on the original roof. The original entrance and window
  sides at 315/45 degrees are pixel-identical to baseline for both seasons.
  Eight further oblique views (0/90/180/270 degrees in both seasons) show
  closed corners and roof joins. All sixteen candidate captures exited
  successfully with empty stderr and the same verified executable hash.
  The native model viewer uses generic lighting callbacks, so its window/light
  appearance should not be treated as an in-game lighting reference.
- Final combined executable SHA256
  `1C6FE146040EF536D1339234B94DDE8A7EED83520F9AA26D92BDD491495EC4A3`
  was checked after the separate caught-item rendering correction. Four final
  native captures (summer/winter, rear 225 and entrance 315 degrees) exited
  successfully with empty stderr and are pixel-identical to the reviewed shop
  candidate. `pc/build32/shop-followup/final-captures.json` records the final
  executable hash; `verify-final.py` repeats those four image comparisons.

Physical headset confirmation of the shop's rear and functioning entrance is
still needed; native geometry and renderer checks do not replace that playtest.
