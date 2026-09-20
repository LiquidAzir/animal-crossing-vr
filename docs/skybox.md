# Outdoor sky

The PC renderer now draws a sky inspired by the supplied village reference:
a blue gradient, pale horizon, rounded white clouds with blue undersides, and
subtle distant island silhouettes. The silhouettes are background artwork,
not new terrain. No textures or ROM changes are needed.

The sky is enabled by default. Add `skybox = 0` under `[Graphics]` in
`settings.ini` to restore the original clear-color background. Missing keys
keep the new default. Settings saves retain this option.

## Integration

- `pc/src/pc_sky.cpp` owns the embedded OpenGL 3.3 shaders and a single
  fullscreen-triangle draw per world pass. Its depth test preserves nearer
  content; it never writes depth and restores all GL state it changes.
- The existing emu64 lookAt notification now supplies the sky's current view
  in both desktop and VR. The sky is drawn on the first perspective batch
  after framebuffer routing, using the same projection as the world.
- In VR, the view rotation comes from `view_correction * game_view` and each
  eye's asymmetric projection. Translation and world scale are removed: the
  sky stays at infinity and follows head rotation without eye parallax.
- `Global_kankyo_set` supplies the game's time and interpolated rain/snow
  state. Day, twilight, night, and overcast colors use those values. Cloud
  motion is continuous across midnight and identical for both eyes.
- Only the outdoor field with no submenu requests the sky. Environment
  stamps expire after leaving gameplay; paused outdoor views retain it.
  Each pass requires a fresh lookAt. Indoor scenes and submenu previews use
  their original background. Shader failure retains the stock clear color.

## Automated verification

Run from the source checkout:

```powershell
python pc/tests/run_sky_tests.py
python pc/tests/run_vr_tool_tests.py
```

The sky harness compiles and draws the actual production shader in a hidden
SDL/OpenGL context on the local GPU. It checks scene/setting guards, day and
night colors, weather, midnight continuity, asymmetric stereo overlap,
translation/scale invariance, depth preservation, and GL state restoration.
It writes `sky-day.bmp`, `sky-sunset.bmp`, `sky-night.bmp`, `sky-rain.bmp`, and
`results.txt` under `pc/build32/sky-tests/`. These are isolated renderer
previews, not in-game screenshots or evidence of a physical headset test.

Local verification on 2026-09-19: all 26 sky checks and the existing 278
motion-tool checks passed. The shaders compiled on the NVIDIA RTX 5090;
the sky-only GPU measurement averaged 0.054 ms per 2048x2048 pass over 32
passes. This excludes the rest of the game and SteamVR. The complete 32-bit
game built successfully; logs are `pc/build32/sky-build.log` and
`pc/build32/sky-final-build.log` (the final cloud-shape adjustment).

## Local installation

Update: this initial sky build is superseded by the
[bug-fix round](bugfix-round-20260919.md), which preserves the sky artwork.
The hashes below describe the initial sky installation.

The tested build is installed at `../AnimalCrossing-VR/AnimalCrossing.exe`.
Its SHA-256 is
`83f089edd1322488a39e080769995fda3c5be8feb0fcac97252250c9eaef03e8`.
The previous executable, including the motion-tool fixes, is preserved at
`../Backups/skybox-20260919/AnimalCrossing.exe`. The adjacent
`installation-receipt.json` records both hashes and verifies all 23 other
installation files, including settings and all four save files, are unchanged.
Close the game before copying that backup executable back for rollback.

The final executable also reached the title screen in an isolated startup
check, with no stderr or crash report. SteamVR reported `Hmd Not Found (108)`
and fell back to desktop mode. This is a log-based startup check, not a visual
gameplay or headset validation. Test processes are closed. Changes remain
local and uncommitted; no release was published.

## Remaining playtest

- Outdoors, look up, down, and all the way around; clouds should remain fixed
  in world directions without a panorama seam. Lean and recenter in VR.
- Check first person, diorama, desktop, and toggling between them.
- Enter/exit buildings; open inventory, map, dialogue, and the pause menu.
  Check that foreground scenery, transparent foliage, UI, and fades remain
  intact. Pausing must keep the sky visible without cloud drift.
- Check day, twilight, night, rain, and snow. For a temporary desktop launch,
  the existing `--no-vr --time 12` or `--time 18:30` options aid comparison;
  the existing `--rain heavy` option exercises overcast weather.
- Check ocean/cliff sightlines, headset horizon alignment, and GPU timing
  during normal play. Physical stereo comfort is not established by the
  synthetic asymmetric-projection check.
