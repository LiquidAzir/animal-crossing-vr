# Island controller transition integration probe

Run `python pc/tests/run_island_transition_native.py` after a normal PC build.
The runner reuses the built `objects.a`, links a separate diagnostic executable,
and opens a hidden SDL window. It copies the ROM from the existing
`pc/build32/smoke-fp-dialogue` fixture into a new timestamped directory with empty
save folders. It does not replace the production executable or use personal
saves. A 35-second process timeout bounds the run.

For a preserved pre-fix archive, add `--archive PATH --expect-crash`. This mode
requires a native exception after observing a missing controller, before that
draw completes; a timeout is not counted as a reproduced crash.

The diagnostic enables full-world residency in actual title scenery. After 90
rendered world frames, it calls the real climate/summer/background replacement
operations used by the boat. Normal actor updates and the actual GX renderer
then execute. Ten frames later it calls the corresponding town-season return
operations. It verifies resident houses render through both controller gaps,
the controller returns, and the captured frames have no GL errors.

This isolates the reported controller lifetime failure. It does **not** navigate
boarding/dialogue, load the island field, or verify return-trip interaction.

## September 28 verification

- Preserved archive `318d1029848beda2c5ed69f27ced9420f15a69a2ba403dbbe5661ef1ff7be3e0`:
  14 houses present, then native access violation in `aRSV_actor_draw+0x153`
  while loading the shadow callback through a null provider. Actor ordering in
  this title fixture reaches the reserve sign before the house seen in the
  headset crash; both use the same provider.
- Fixed archive `d33ec26422b5c21547b550b129787d483a5d19f01d43080008de17adfe565c5a`:
  **29 checks passed**. Both directions contain two rendered frames without the
  provider, with all 14 houses still drawn, followed by eight recovered frames.
  Three 640×480 native captures were written, with GL error zero.
- Baseline receipt: `pc/build32/island-transition-native-20260928-184424`.
  Fixed receipt: `pc/build32/island-transition-native-20260928-184714`.
- Production executable remained unchanged by the diagnostic; fixed executable
  SHA256 `05ba79e5cc8901c20a372cf7a36efc57e2cbc15be53cb9796a476e014472cbe3`.
