# v0.9.3-vr19 — VR hands, building repairs, sky, and first-person polish

This release brings the current VR improvements into one Windows download.
It is based on **ACGC-PC-Port v0.9.3**. Start with a working installation of
that recompiled PC version, then apply this update to its game folder.

## Download

Under **Assets**, choose **`AnimalCrossing-VR-v0.9.3-vr19-win32.zip`**.
GitHub's **Source code** downloads are for developers and do not contain the
playable executable. A separate `.sha256` file is provided to check the zip.
The archive contains the VR executable, supporting DLLs, shaders, controller
bindings, documentation, and license notices. It contains no disc image,
game assets, saves, texture packs, or personal settings.

## First-time installation

1. Download **`ACGC-PC-Port0.9.3.zip`** from the upstream
   [v0.9.3 PC-port release](https://github.com/flyngmt/ACGC-PC-Port/releases/tag/v0.9.3-playtest).
2. Extract the PC port to a writable folder. Put your own **USA Rev 0
   (GAFE01_00)** Animal Crossing disc image (`.iso`, `.gcm`, or `.ciso`) in
   `rom` next to `AnimalCrossing.exe`.
3. Run the original PC executable to confirm it reaches the title screen.
   Close it and back up the entire game folder before applying VR.
4. Download and extract the VR19 zip below. Open its `AnimalCrossing-VR`
   folder and copy **everything inside it** into the working PC game folder.
   Accept replacement of the supplied files. Do not create a nested game folder.
5. Keep `AnimalCrossing.exe`, `SDL2.dll`, `openvr_api.dll`, `shaders`, and
   `vr_actions` together. Preserve your existing `rom`, `save`, `texture_pack`,
   `settings.ini`, and `keybindings.ini`; these are not included in the update.
6. Start SteamVR and connect the headset and controllers, then run the updated
   `AnimalCrossing.exe`. Quest users must first connect to the PC through
   Steam Link, Virtual Desktop, or Quest Link/Air Link.
7. Press **F5** or **left grip + Y** for first person. **X + Y together**
   recenters the view. To start in first person every time, close the game and
   set `fp_mode = 1` under `[FirstPerson]` in `settings.ini`.

X/Y above are Touch button names. On Index, use **left grip + left B** for
first person and **left A + left B** to recenter. Desktop shortcuts must use
the game folder as their **Start in** location.

The [complete README walkthrough](https://github.com/LiquidAzir/animal-crossing-vr/blob/v0.9.3-vr19/README.md#installation)
includes a folder example. The
[VR setup guide](https://github.com/LiquidAzir/animal-crossing-vr/blob/v0.9.3-vr19/VR_README.md)
covers controls, optional settings, and troubleshooting.

## Updating from an older VR release

Close the game, back up the folder, and overlay the **complete** VR19 package
as in steps 4–5. You do not need to reinstall the PC port for an already
working VR installation. Keep your saves and settings.

**Replace `vr_actions` along with the executable.** VR19 corrects the optional
hand pose bindings for Touch and Index controllers. Updating only the exe can
leave the hands invisible. Custom SteamVR bindings may need the new empty-hand
actions added, or you can select the updated defaults. An existing setting
that turns hands off remains respected.

## Changes since VR18

- Outdoor gradient sky, clouds, and horizon that follow game time and weather.
- Improved tool/net alignment, controller tool targeting, and gaze-directed
  running and tool actions.
- Fitted seasonal backs for houses and several civic buildings, Able Sisters,
  Nook's Cranny, and the fountain, plus repaired ramp-side cliff gaps.
- Controller-tracked floating hands, shown during ordinary first-person VR
  play with no item equipped; no arms or finger tracking.
- First-person catch/discovery sequences, with presented items in front of
  the headset and dialogue placed higher.
- Small CPU scenery and GPU sky optimizations that preserve the existing
  appearance and gameplay defaults.

## Verification and remaining limits

The complete Windows build and isolated startup checks passed. Focused
regressions cover geometry, bindings, hand visibility, tool placement, and
item presentation. The performance changes passed 1,344 scenery checks and
1,176 sky image comparisons with no pixel differences. These component results
are not a claim of a particular game framerate or minimum hardware requirement.

No new physical-headset or lower-end-PC test was performed for this packaging
update. Some building types and later Nook shop upgrades still use approximate
rear fill. For slower GPUs, start with a lower SteamVR per-application render
resolution and restart the game; see the setup guide for the tradeoffs.
