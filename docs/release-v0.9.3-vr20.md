# VR20 — PC VR and standalone Quest 3

The latest Windows fixes and the standalone Quest 3 version are now available together. **Choose one installation path:**

| Download | Use it for |
|---|---|
| **AnimalCrossing-VR-v0.9.3-vr20-win32.zip** | Windows + SteamVR; install the recompiled PC version first |
| **AnimalCrossing-Quest-v0.9.3-vr20.zip** | Standalone Quest 3; includes APK and Windows ROM installer |
| AnimalCrossing-Quest-v0.9.3-vr20.apk | Manual sideloading; ROM import is still required |

Both versions require your own **Animal Crossing USA Rev 0 (GAFE01_00)** `.iso`, `.gcm`, or `.ciso`. Downloads contain no ROMs, extracted game assets, personal saves, settings, or signing keys. GitHub's Source code archives are for developers.

## Windows PC VR

1. Install and test [ACGC-PC-Port v0.9.3](https://github.com/flyngmt/ACGC-PC-Port/releases/tag/v0.9.3-playtest) with your own ROM in `rom` beside the executable. Close the game and back up its folder.
2. Extract the VR zip. Copy everything **inside** its `AnimalCrossing-VR` folder into the working PC game folder, replacing the supplied program files.
3. Start SteamVR, connect your headset/controllers, and launch `AnimalCrossing.exe`. **F5** or **left grip + Y** enables first person.

Already using PC VR? Back up your save and apply the complete zip over the existing installation. Keep your ROM, saves, and settings.

## Standalone Quest 3

1. Set up [SideQuest Advanced Installer](https://sidequestvr.com/setup-howto) on Windows, enable Developer Mode, connect Quest by USB, and approve USB debugging in the headset.
2. Extract the **Quest zip**, close Animal Crossing on the headset, then run **Install-Quest.cmd**. Select your own supported ROM when asked.
3. After setup succeeds, unplug USB and launch **Animal Crossing** from Unknown Sources. No PC game installation or streaming is needed.

For updates, run the new installer again; it preserves existing ROMs, saves, and settings. **Do not uninstall or clear app data.** [Quest setup/manual import](https://github.com/LiquidAzir/animal-crossing-vr/blob/quest/standalone/quest/INSTALL.md).

## Changes since VR19

- Standalone Quest 3 runtime with OpenXR tracking, motion tools, and separate local saves.
- VR Settings opens with both stick clicks: hand visuals, turning, motion swings, and volume. Hand markers default off.
- First-person fishing remains first person after casting; rod bend points downward.
- Corrected hand depth and shop-carpet overlap; fish-room aquariums stay visible.
- Island scenery-transition crash and return-boat lifecycle fixes.
- Matching island-house backs, roof undersides, dock/bridge faces, and wooden bridge post backs.

The Quest package uses the existing tested **APK build 14**; this release does not lower resolution or change gameplay/rendering settings.

## Verification and limits

Native PC and Quest builds and focused geometry, island-transition, menu, and rendering checks passed. The Windows setup helper and release contents are checked separately; checksums are included. Quest playtesting confirmed playable performance, the settings menu, and arrival on the island. A complete island return/save-reload cycle and sleep/resume remain unverified. Some structures and later Nook shop upgrades retain approximate rear fills. Standalone testing covers Quest 3 only.

Source is kept in this same repo on **master (PC)** and **quest/standalone (Quest)**. The companion **quest-v0.1.0-build14** tag records the source for the Quest download; the VR20 tag records the PC source.
