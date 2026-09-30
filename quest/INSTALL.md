# Install Animal Crossing on Quest 3

**Download [AnimalCrossing-Quest-v0.9.3-vr20.zip](https://github.com/LiquidAzir/animal-crossing-vr/releases/latest).** This includes the APK and a Windows setup helper. You only need the computer for installation; the game runs entirely on Quest 3.

1. Install [SideQuest's Advanced Installer](https://sidequestvr.com/setup-howto) on Windows. Follow its setup to enable Developer Mode, connect Quest by USB, and accept **Allow USB debugging** in the headset. [Meta's device setup guide](https://developers.meta.com/horizon/documentation/native/android/mobile-device-setup/) has additional help.
2. Extract the downloaded zip completely. Have your own **Animal Crossing USA Rev 0 (GAFE01_00)** disc image ready (`.iso`, `.gcm`, or `.ciso`; not RVZ or a zip).
3. Close Animal Crossing on the headset. Double-click **Install-Quest.cmd** and choose your disc image when asked. Wait for the success message; a large ROM can take several minutes.
4. Disconnect USB and open **Animal Crossing** in the headset's **Unknown Sources** app list.

No recompiled PC installation, SteamVR, or PC streaming is needed for this version. No ROM or save is included.

## Updating

Extract the new Quest package and run its installer again. It updates the app in place and keeps existing ROMs, saves, and settings. **Do not uninstall or clear app data** to update: that removes the stored ROM and progress.

## Playing

Move with the left stick, turn with the right, and use **A/right trigger** to confirm or use tools. **X + Y** recenters. **Left grip + Y** toggles first person. Click **both sticks together** for VR Settings; choose **Apply** to save changes. Hand markers start off.

## If setup stops

- **No headset / unauthorized:** wake Quest, reconnect USB, and accept its USB-debugging prompt. Disconnect any second Quest during setup.
- **ADB not found:** open SideQuest once to finish setup, or select `adb.exe` from Google's [platform-tools](https://developer.android.com/tools/releases/platform-tools) when asked.
- **App is running:** quit Animal Crossing in the headset and retry; the installer will not interrupt your game.
- **Game data error:** confirm your dump is USA Rev 0 in one of the supported formats. Renaming RVZ to ISO does not convert it.

The separate `.apk` download is for manual installation; installing the APK alone does not supply the ROM. Mac/Linux users, save backups, and manual import: [manual setup](https://github.com/LiquidAzir/animal-crossing-vr/blob/quest/standalone/quest/MANUAL.md) (also included as MANUAL.md).
