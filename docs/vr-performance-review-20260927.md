# VR performance review — September 27, 2026

The current measurements identify useful priorities, but do not establish
minimum hardware requirements. The development machine is an RTX 5090 with
an Intel Core i9-13900K. No slower PC or new physical-headset session was used
for this review, and no user save or gameplay settings were changed.

## What was measured

### Baseline build: isolated flat title/attract scene

Executable SHA256 `1C6FE146040EF536D1339234B94DDE8A7EED83520F9AA26D92BDD491495EC4A3`
ran with `--no-vr --profile 600 --framelimit 0` in the existing isolated test
fixture at 640x480. The first mixed startup window was discarded. Thirteen
subsequent 600-frame windows (7,800 frames) produced these window medians:

| Measurement | Result |
| --- | ---: |
| Frame time, including presentation and profiler overhead | 6.627 ms |
| Range of window-average frame times | 6.170–6.950 ms |
| Draw calls per frame | 1,851 |
| Submitted vertices per frame | 54,840 |
| GX flush CPU time | 3.120 ms |
| Vertex-buffer upload API CPU time | 1.433 ms |
| Texture-object processing CPU time | 0.496 ms |
| Game-logic CPU time | 0.210 ms |

Timers overlap: buffer upload is inside GX flush, which is inside command
interpretation. They must not be summed. Upload timing can include driver
work or waiting, rather than just a memory copy. The animated title/attract
scene is neither a controlled whole-town benchmark nor a VR framerate claim.
Results point to CPU command submission/driver overhead as a useful follow-up
area, especially on slower CPUs. They do not show a problem caused by the new
building walls.

The renderer already batches identical adjacent triangle/quad state, caches
shaders and uniform locations, tracks changed uniforms, and caches textures.
Every actual draw currently uploads a separate vertex buffer with `glBufferData`.
A bounded streaming-buffer experiment could reduce driver allocation overhead,
but should be compared using identical rendered images and actual headset
timings before replacing this established path. No such rewrite was shipped.

Local artifacts: `pc/build32/performance-audit/attract-profile-before.stdout.log`
and `flat-profile-summary.json`.

### Current production sky and hands: isolated native OpenGL

The standalone benchmark links the actual renderers, uses a hidden native
OpenGL context, warms the programs/buffers, and collects seven batches of
timer queries. GPU batches run after the separate application profiler stops.
It checks framebuffer coverage and GL errors. There is no game simulation,
SteamVR compositor, headset transport, or lower-end GPU in these measurements.

Before optimization, the fully exposed sky measured a median 0.108 ms per
2048x2048 eye and 0.246 ms per 3072x3072 eye. Full occlusion was approximately
0.0013 ms; the existing depth test avoids almost all fragment work behind
geometry. Two hands in a 3072x3072 eye measured approximately 0.014 ms GPU and
0.038 ms CPU submission. Tiny GPU measurements have substantial scheduling
noise. These figures are isolated component costs, not whole-frame times.

The hand mesh/program are persistent, with no per-frame allocation. There was
no measured reason to remove hands or rewrite their renderer for this task.
Results and benchmark sources are under `pc/build32/performance-audit/`.

## Conservative optimizations

- Background scenery: full-world mode already makes the original visibility
  test return true. The caller now returns before projecting the position
  through the old mono-camera matrix. Ordinary culling, dialogue-specific
  tree masking, linked draw loops, and matrix state are preserved. This removes
  work per scenery test without hiding or unloading anything; no whole-game
  FPS improvement is claimed for this small CPU change.
- Sky: each cloud row has a bounded possible elevation. A conservative row
  test now skips its hash/shape calculations when the ray cannot meet the
  original per-cloud vertical bound. The original cloud test remains in place,
  with a small extra margin on the row bound. Clouds, colors, weather, time,
  stars, and visibility rules are unchanged.

For the sky experiment, 1,176 native image pairs across seven times (including
night/dawn/dusk), three weather blends, seven pitches, and eight yaws were
byte-identical. Alternating seven 256-draw GPU batches at each view gave:

| Per-eye size and pitch | Original median | Optimized median |
| --- | ---: | ---: |
| 2048², level | 0.0884 ms | 0.0752 ms |
| 2048², 0.25 radians up | 0.1004 ms | 0.0741 ms |
| 2048², 0.65 radians up | 0.1101 ms | 0.0763 ms |
| 3072², level | 0.236 ms | 0.196 ms |
| 3072², 0.25 radians up | 0.272 ms | 0.197 ms |
| 3072², 0.65 radians up | 0.258 ms | 0.181 ms |

The 15–31% figure at 2048² describes only sky GPU time in this experiment.
Absolute savings on the 5090 are small, and cannot be translated into the
same percentage improvement in game FPS or a guaranteed gain on another GPU.

## Existing headset session

The September 27 17:15–17:41 session predates the current executable's 17:58
build. SteamVR recorded a 120 Hz headset, approximately half of presents
reprojected, no dropped presents, and low application CPU/GPU times on the
5090. The game reported approximately 60 new frames per second. The original
startup submit error 101 was a focus event; the subsequent session did not
remain in failed-submission fallback.

This is evidence that comfortable-looking motion alone does not prove native
120 FPS. It is not evidence that the GPU is too slow, or that a game-side
60 FPS cap is active. Normal VR explicitly disables the desktop limiter and
window VSync; NES intentionally keeps its separate 60 Hz pacing. The eye
buffers were 3072x3264 each: about 20.05 million pixels per stereo frame, even
though the original game has modest geometry. The buffers are single-sampled;
the desktop MSAA setting does not add MSAA to the eyes.

Existing telemetry is insufficient to explain half-rate delivery. Before
changing pacing, collect headset refresh, client-frame intervals, submission
success/focus, wait durations, and runtime reprojection settings. The vendored
OpenVR documentation describes `PostPresentHandoff` for apps doing CPU work
between companion-window presentation and the next `WaitGetPoses`; this
application has that ordering but does not call it. That is a candidate for
a headset A/B experiment, not a demonstrated fix. Pacing remains unchanged.

## World and feature costs

The fitted house/shop/civic repairs generate their meshes and small textures
once, then reuse them. They add a few surfaces instead of redrawing an entire
reflected facade. The original model and callbacks draw once. Keeping these
repairs is the appropriate default.

Full-world mode deliberately bypasses the original narrow camera cull to
prevent scenery popping when the headset turns. The residency sweep checks
one acre's 256 foreground cells per frame. Ordinary villagers retain their
original pool/range; this does not run every villager's AI town-wide. Full-world
state is sticky until restart, so switching first person off is not a clean
performance baseline.

The terrain `vr_draw_radius` setting only limits terrain display-list acres.
Radius 2 covers at most 25, radius 3 at most 49, and the full grid at most 70.
Buildings, trees, BG items, and actor updates are not covered by that switch.
Reducing it can expose distant terrain gaps and cannot be advertised as a
measured universal speedup. Disabling residency would compromise the intended
free-look experience and is not recommended as a release default.

## Recommendations before publishing hardware requirements

1. Keep current visual/world defaults. Test at least one genuinely slower CPU
   and GPU using the same route and headset resolution, with native and
   reprojected frame delivery recorded separately.
2. Document SteamVR per-application resolution and supported lower headset
   refresh rates as the first GPU tuning options. Restart the game after
   changing resolution; it reads recommended eye dimensions only at startup.
   Do not direct users to desktop Max FPS/MSAA as headset-quality controls.
3. Profile busy outdoor scenes, rain/night, interiors, menus, catches, and
   loading on that hardware. Distinguish shader warm-up/loading spikes from
   sustained frame time; include controller tracking and readable dialogue.
4. If CPU submission remains the bottleneck, evaluate streaming vertex uploads
   before changing visibility, simulation, or town residency. Changes to
   culling/pacing require headset validation because they affect head turns
   and frame delivery.

For reference, a native frame has about 13.9 ms at 72 Hz, 11.1 ms at 90 Hz,
or 8.3 ms at 120 Hz. A single 5090 result cannot certify those budgets for
another machine, driver, headset, or wireless connection.

## Verification and local installation

- Background scenery regression: 1,344 checks passed across PC, widescreen,
  and original-source paths. The prior helper fails only the 37 expected
  projection-elimination checks; visual decisions and dialogue masking agree.
- Sky regression: 29 existing checks passed. The durable native comparison
  in `pc/tests/run_sky_row_tests.py` reconstructs the baseline by removing only
  the new bound block; all 1,176 views matched exactly, with no GL errors.
- The complete MinGW build passed. The resulting executable reached the title
  screen in the isolated flat startup fixture, with empty stderr and no crash
  report. No fresh headset session or lower-end machine was tested.
- Installed executable SHA256:
  `9A6A75857A78ABD5B88AA6164B77CCF72BE128E76018F6724C4574538ADB23AA`.
  Only `AnimalCrossing-VR/AnimalCrossing.exe` was replaced; all 23 other
  installation files retained their hashes, including settings, saves, and
  controller bindings. The previous executable is backed up in the sibling
  `Backups/performance-20260927/` directory.

Build, startup, installation, and measurement receipts remain under
`pc/build32/performance-audit/`. At the time of this local verification, no
commit or push had been made.
