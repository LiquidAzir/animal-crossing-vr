# Animal Crossing VR

Play Animal Crossing in first-person VR on **Windows with SteamVR** or **standalone on Quest 3**.
Includes tracked tools, weather-aware skies, repaired scenery, and an in-headset settings menu.
Based on [ACGC-PC-Port](https://github.com/flyngmt/ACGC-PC-Port) and [ac-decomp](https://github.com/ACreTeam/ac-decomp).

**[Download the latest release](https://github.com/LiquidAzir/animal-crossing-vr/releases/latest)** — choose the package for your headset below, not GitHub's Source code archives.

| Version | Download | Requirements |
|---|---|---|
| Windows PC VR | `AnimalCrossing-VR-v0.9.3-vr20-win32.zip` | Working recompiled PC version + SteamVR |
| Standalone Quest 3 | `AnimalCrossing-Quest-v0.9.3-vr20.zip` | Quest 3 + USB connection to a computer for setup |

Both require your own **Animal Crossing USA Rev 0 (GAFE01_00)** disc image: `.iso`, `.gcm`, or `.ciso`. No ROMs or saves are included. Quest runs on the headset after setup; it does not need the PC game or streaming.

**Quick links:** [VR controls](#vr-controls) · [PC installation](#windows-pc-vr) · [Quest installation](#standalone-quest-3)

## Installation

### Windows PC VR

1. **Install the recompiled PC version first.** Download `ACGC-PC-Port0.9.3.zip` from [ACGC-PC-Port v0.9.3](https://github.com/flyngmt/ACGC-PC-Port/releases/tag/v0.9.3-playtest) and extract it to a writable folder.
2. Put your own disc image in `rom` beside `AnimalCrossing.exe`. Run the PC version once to confirm it works, then close it and **back up the game folder**.
3. Download and extract **`AnimalCrossing-VR-v0.9.3-vr20-win32.zip`** from the release above. Copy everything **inside** its `AnimalCrossing-VR` folder into your working PC game folder and replace the supplied files.
4. Keep the executable, both DLLs, `shaders`, and `vr_actions` together. Your existing `rom`, `save`, `settings.ini`, and keybindings stay in place.
5. Start SteamVR with the headset and controllers connected, then run `AnimalCrossing.exe`. For Quest PC VR, connect through Steam Link, Virtual Desktop, or Quest Link/Air Link first.
6. Press **F5** or **left grip + Y** to enter first person. **X + Y** recenters. Without a connected headset, the Windows build also runs flat.

**Updating:** close the game, back up `save`, then repeat steps 3–4 with the complete new package. [Full PC controls and troubleshooting](VR_README.md).

### Standalone Quest 3

1. On a Windows computer, install [SideQuest's Advanced Installer](https://sidequestvr.com/setup-howto). Follow its setup to enable Developer Mode, connect Quest 3 by USB, and accept **Allow USB debugging** inside the headset.
2. Download **`AnimalCrossing-Quest-v0.9.3-vr20.zip`** from the release above and extract it completely.
3. Close Animal Crossing on the headset if it is running. Open **`Install-Quest.cmd`** in the extracted folder and select your own supported disc image when asked. It installs the APK and copies the ROM for you.
4. Unplug the USB cable and open **Animal Crossing** from the headset's **Unknown Sources** apps.

**Updating:** run the installer from the new package again. Existing ROMs, saves, and settings are kept; do not uninstall or clear app data. [Quest setup help and manual installation](quest/INSTALL.md).

## VR controls

These are the default bindings. **Touch controls are the same in PC VR and standalone Quest 3.** Grip means the side squeeze button; a stick click means pressing the stick down.

| Action | Quest / Touch (PC or standalone) | Valve Index (PC) |
|---|---|---|
| Move | Left stick | Left stick |
| Turn / camera | Right stick | Right stick |
| Talk, confirm, use equipped tool | Right **A** or right trigger | Right **A** or right trigger |
| Pick up, cancel / back | Right **B** or left trigger | Right **B** or left trigger |
| Run | Hold right **B**, left trigger, or right grip while moving | Hold right **B**, left trigger, or right grip while moving |
| Open inventory | Left **Y** or left stick click | Left **B** or left stick click |
| Rearrange items in inventory | Left grip to pick up / drop the selected item | Left grip to pick up / drop the selected item |
| Next inventory / collection page | Right grip | Right grip |
| Open town map (once obtained) | Left **X** | Left **A** |
| Previous / next pocket tool | Hold left grip + left stick **left / right** | Hold left grip + left stick **left / right** |
| Put tool away | Hold left grip + left stick **down** | Hold left grip + left stick **down** |
| Toggle lights in your house / island cottage | Right stick click | Right stick click |
| Toggle first person / third person | Hold left grip, then press **Y** | Hold left grip, then press left **B** |
| Recenter view | Press **X + Y** together | Press left **A + B** together |
| Open VR Settings / pause | Click **both sticks together** | Click **both sticks together** |

Tool shortcuts work during normal outdoor movement and use tools in your pockets. Putting a tool away needs an empty pocket slot. Holding left grip changes the left stick to D-pad input, so release it to walk again.

**Motion tools:** in first person, equip a net, axe, shovel, or fishing rod and swing your **right controller** to use it. **A/right trigger** also works. Look toward your target with the axe, shovel, or rod; the net follows your hand. Let your hand slow between swings. Gestures are disabled in menus and dialogue, and can be turned off in VR Settings.

**VR Settings:** left stick **up/down** selects a row, **left/right** changes its value, **A/right trigger** confirms, and **B/left trigger** goes back or resumes. Choose **Apply** to save changes, then resume; unapplied edits prompt before being discarded. Settings include hand visuals (**off by default**), smooth/snap turning, motion swings, and volume. Open this menu during gameplay; it is unavailable on the title screen or inside NES games.

On PC, **F5** also toggles first person. Custom SteamVR bindings can differ; change them in **SteamVR → Settings → Controllers → Manage Controller Bindings**. [PC setup and troubleshooting](VR_README.md).

## This release

VR20 includes the standalone Quest build (APK build 14), first-person fishing and catches, fishing-rod alignment, optional hand markers, the VR settings menu, aquarium visibility fixes, and island-travel, house, dock, and bridge repairs.

Scenery fixes follow models, so they carry over to new towns and different residents. Some structures and later Nook shop upgrades still use approximate rear fills. Quest 3 has been playtested; other standalone headsets are unverified. Complete island return/save-reload and sleep/resume testing remain outstanding. [Release notes](docs/release-v0.9.3-vr20.md).

## Source code

Both versions live in this repository, with separate branches/builds to keep platform changes isolated:

- **PC:** [`master`](https://github.com/LiquidAzir/animal-crossing-vr/tree/master). Build with MSYS2 MINGW32 using `./build_pc.sh`; see [PC documentation](pc/DOCUMENTATION.md).
- **Quest:** [`quest/standalone`](https://github.com/LiquidAzir/animal-crossing-vr/tree/quest/standalone). See [Quest build instructions](https://github.com/LiquidAzir/animal-crossing-vr/blob/quest/standalone/quest/BUILDING.md).

## Credits & lineage

- **[ACreTeam](https://github.com/ACreTeam)** — the complete Animal
  Crossing decompilation ([ac-decomp](https://github.com/ACreTeam/ac-decomp))
  that makes all of this possible.
- **[flyngmt](https://github.com/flyngmt)** — the
  [PC port](https://github.com/flyngmt/ACGC-PC-Port): the GX→OpenGL
  translation layer, asset pipeline, audio, save system, and everything
  else that turned the decompilation into a runnable native game. This
  fork's VR layer stands entirely on that work.
- **[FIX94](https://github.com/FIX94)** — fixNES, powering the in-game NES
  titles.
- **Valve** — the [OpenVR SDK](https://github.com/ValveSoftware/openvr)
  (BSD-3-Clause, vendored in `pc/lib/openvr/`).
- **Khronos Group** — [OpenXR SDK](https://github.com/KhronosGroup/OpenXR-SDK) for standalone Quest VR.
- **SDL contributors** — [SDL2](https://www.libsdl.org/) for platform support.
- VR fork (stereo renderer, first person, motion tools) by
  [LiquidAzir](https://github.com/LiquidAzir).

This VR fork was developed with AI assistance, with human direction and hands-on playtesting by LiquidAzir.

## License

Dual/multi-licensed — see [LICENSE](LICENSE):
- Decompiled game code: **CC0 1.0** (ACreTeam/ac-decomp)
- PC port layer: **MIT** © FlyingMeta
- VR fork additions: **MIT** © LiquidAzir
- fixNES: **MIT** © FIX94 · OpenVR: **BSD-3-Clause** © Valve

This project distributes no Nintendo game assets or disc images. A legally obtained
disc image is required to play.
