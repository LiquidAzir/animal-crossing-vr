# Small vertex-preparation experiment — September 28, 2026

**Decision: not adopted. The installed Quest development build remains version 8.**

The user requested a small performance improvement without reducing resolution
or changing graphics. The experiment reused prepared CPU vertices from the left
eye when the right eye encountered identical source data and conversion state
in the same frame. Eye transforms, visibility, materials and draw order still ran
normally. It did not change simulation, resolution, textures or draw distance.

The fixed cache used 1,982,528 bytes with no dynamic allocation. It checked exact
source contents, matrices, flags, command identity, owner, frame and successful
task completion. Mismatches, capacity limits and unavailable input used the
original path. Only defined output fields were copied, retaining destination
padding and independent mutable interpreter vertices.

## Correctness

- 11,272 host checks passed, using the actual vertex conversion code in both
  reuse and verification modes. They cover input/state changes, mutations,
  skipped loads, overflow and task boundaries.
- The actual ARM32 interpreter compiled with the experiment off, on and in
  verification mode.
- An isolated Quest game run compared **732,160 cache hits** against the original
  conversion in the same frames, with **zero mismatched vertices**.
- Stereo rendering completed with distinct eye images and no GL errors.

## Initial performance comparison

Both runs used the same full-town title scene, fixed headset pose, 1,760×1,760
pixels per eye, 60 warmup world frames and 300 timed world frames. Detailed
per-draw profiling and correctness verification were disabled during timing.

| Mode | Average frame time | Average draws | Average commands |
| --- | ---: | ---: | ---: |
| Original conversion | 23.573 ms | 501.983 | 40,160.233 |
| Vertex reuse | 23.939 ms | 504.777 | 40,171.913 |

The candidate showed no useful gain in this initial pair. The 0.366 ms difference
is small enough to include ordinary run variation; this does not prove a general
slowdown. It provides no reason to add a two-megabyte cache and more rendering
code to the working build. Further comparisons and rollout were therefore
cancelled. These measurements exclude OpenXR/compositor/tracking overhead and
are not headset FPS claims.

## Final state and retained evidence

The production experiment and its diagnostic entry point were removed. The
restored native game library exactly matches the version 8 library SHA256:
`af1b5313e763fce1da5ec1231a19c0f2af74a81c1f85f6119a53bd5369b78214`.
No APK was installed and no app-private data was touched. The 54 snapshotted PC
source/installation files retain their hashes.

Only useful test-harness improvements remain: configurable warmup/sample counts
and workload reporting. The current game and graphics behavior are unchanged.

Local evidence in the separate Quest workspace:

- `research/vtx-comparison-20260928-170237/results.json`: timings and receipts.
- `research/game-offscreen-20260928-170237`: on-device conversion verification.
- `research/game-offscreen-20260928-170259`: original-conversion timing/captures.
- `research/game-offscreen-20260928-170321`: reuse timing/captures.
- `research/vtx-{disabled,verify,enabled}-built`: frozen experimental libraries,
  source snapshots and build hashes.
- `research/vtx-rejected-source`: archived helper/tests and integration/diagnostic
  patches, kept outside the game source rather than left as dormant production code.
- `pc/build32/quest-vtx-prep-tests`: host correctness and ARM compilation receipts.

Reproducing the archived comparison requires its archived source/diagnostic
patches or frozen libraries. The standard current game has no reuse switch.
