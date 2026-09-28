# Standalone Quest development port

This is an isolated Android/OpenXR development branch of the Animal Crossing PC VR port. It runs native game code rather than streaming the PC app. Keep it in its own workspace; the existing PC source, installation, ROM, settings, and saves remain separate and unchanged.

This is **not a public standalone release or a store-ready build**. As of September 28, 2026, development APK version 7 is installed on the test Quest 3. The native game verifies all 14,495 assets, runs a focused OpenXR session, loads the copied PC town, and renders its inventory in both eyes. The actual game also logged a successful write to the separate Quest save. Observed outdoor rendering remains around 36-40 FPS against a 72 Hz headset target, so performance is still a limitation. Fishing, motion tools, hands, audio quality, sleep/resume, and a complete save/quit/reload cycle still need a physical playtest; successful startup does not establish release readiness.

Quest eye targets have an **app-local maximum edge of 1,760 pixels**. The source scales each runtime recommendation down uniformly, with pixel rounding, while respecting the runtime's maximum width/height and never enlarging a smaller recommendation. The version 7 headset session selected 1,680 x 1,760 from a 2,800 x 2,933 recommendation; an existing 1,680 x 1,760 recommendation stays that size. Runtime recommendations varied between those sizes during development, so this ceiling bounds rendering cost independently of that variation. Startup logs record recommended and selected dimensions, and reject zero dimensions. The cap changes only eye target resolution; field of view, UI target size, PC settings, and global headset properties are unaffected.

## Workspace and dependencies

The development tools expect this layout:

```text
Animal Crossing Quest 3/
  source/                  this separate checkout, branch quest/standalone
    quest/
    pc/
  build/                   generated native builds and APKs
  research/                local verification logs and captures
  third_party/
    SDL/                   git tag release-2.30.10
    OpenXR-SDK/            git tag release-1.1.63
  toolchain/
    paths.json
    toolchain-env.ps1
    jdk-21/
    sdk/
    ninja-1.13.2/
```

The known working Windows toolchain uses portable Temurin JDK 21.0.12.1, Android NDK **27.3.13750724**, SDK platform **34**, SDK Build Tools **35.0.0**, Ninja **1.13.2**, and CMake **4.4.2**. The native target is Android API **29**, with static C++ runtime linkage. The manifest currently targets API 32; this is a development compatibility choice, not a claim of store-policy compliance.

`../toolchain/paths.json`, relative to `source`, records the installed tool paths. `../toolchain/README.md` describes their provenance. These local files are outside the source checkout. On another computer, prepare the same isolated layout and update the absolute paths. The build script does not download dependencies or change Git remotes. Obtain SDL and OpenXR from their official repositories, then check out the tags above; modified tracked dependency files or a different commit are rejected.

Build Tools 34.0.0 contains a D8 version that crashes on SDL anonymous classes compiled by this JDK, even with Java 8 bytecode targeting. Build Tools 35.0.0 converts those same classes successfully. Keep the packager and `paths.json` on the verified version.

## Build and package

Run these commands from `source` with Python 3.9 or newer. Python 3.11 was used for verification.

```powershell
# Inspect validated commands without building or writing outputs.
python quest/tools/build_quest.py --mode probe --abi armeabi-v7a --dry-run

# Build and sign the ARM32 runtime probe (no ROM or saves needed).
python quest/tools/build_quest.py --mode probe --abi armeabi-v7a

# Optional ARM64 runtime probe; this is not an ARM64 game port.
python quest/tools/build_quest.py --mode probe --abi arm64-v8a

# Full game development build, currently ARM32 only.
python quest/tools/build_quest.py --mode game --abi armeabi-v7a
```

The entrypoint validates dependency tags, uses the NDK CMake toolchain, configures Ninja/Release, builds, then runs `package_apk.py`. Default output directories are `../build/probe-arm32`, `../build/probe-arm64`, and `../build/game-arm32`. It rejects directories outside the sibling `build` folder and refuses to reuse a cache for a different source tree, ABI, or probe/game mode. It never deletes an existing build.

Useful options are `--jobs 4`, `--configure-only`, `--no-package`, and `--version 2` for the APK version code. `--build-dir ../build/game-arm32-test` selects another isolated build directory. Relative build paths are resolved from the source root, regardless of the shell's working directory. Do not build into the same directory concurrently.

The signed outputs are:

```text
../build/probe-arm32/apk/AnimalCrossingQuest-probe-armeabi-v7a.apk
../build/probe-arm64/apk/AnimalCrossingQuest-probe-arm64-v8a.apk
../build/game-arm32/apk/AnimalCrossingQuest-armeabi-v7a.apk
```

The packager includes only the native libraries, Java host, shaders, and initial Quest settings. It verifies library ABI, APK signing, ZIP alignment, and the signed APK’s actual package, version code, ABI, and app label before replacing the output. Each run uses fresh staging and writes `package-receipt.json` with executable/library hashes and tool versions. APKs are signed with a local development key in `quest/.local/debug.keystore`; keep that key private and retain it for updates. The script does not install or launch an app.

## Device installation and personal game data

Enable Quest developer mode, authorize the computer's USB debugging connection, and verify `adb devices`. Use the ADB path recorded in the isolated toolchain. Install a chosen APK manually, for example:

```powershell
adb install -r "../build/probe-arm32/apk/AnimalCrossingQuest-probe-armeabi-v7a.apk"
```

The runtime probe is labeled **Animal Crossing Quest XR Test** and uses package `com.liquidazir.animalcrossingquest.probe`. The full game keeps the label **Animal Crossing Quest (Development)** and uses `com.liquidazir.animalcrossingquest`. They have separate app data. The two probe ABIs share the same probe package; they are alternative builds rather than side-by-side apps. The probe renders simple stereo colors and exits after its bounded test; it does not load game data.

The current full game uses **internal private app storage**, obtained through Android `getFilesDir` / `SDL_AndroidGetInternalStoragePath`:

```text
/data/user/0/com.liquidazir.animalcrossingquest/files/
  rom/                     your own supported US Animal Crossing disc image
  save/card_a/             optional copied Slot A .gci files
  save/card_b/             Slot B .gci files
  settings.ini             Quest-specific settings
  shaders/                 installed by the Java host
  quest-stdout.log
  quest-stderr.log
```

`/data/data/com.liquidazir.animalcrossingquest/files` may refer to the same location. Use the helper below rather than addressing either path directly. Earlier external-storage test copies are unused by the current build and are not automatically migrated or deleted.

Launch the installed full game once so the Java host prepares its private settings and shaders, then close it before transferring data. The debug APK permits the helper to use `run-as` as the app's own user. Do **not** use ordinary `adb shell mkdir` or `adb push` to populate Android app-storage directories: device tests found shell-owned external directories unreadable by the app, while direct pushes to app-created external directories failed ownership checks. The helper creates internal directories under the correct app user and transfers binary bytes with `exec-in` / `exec-out`.

From `source`, substitute your authorized device serial from `adb devices` and your own staged files:

```powershell
# Close the full game before import/export. This does not clear its data.
adb -s "YOUR_QUEST_SERIAL" shell am force-stop com.liquidazir.animalcrossingquest

# Import your own compatible disc image; its bytes/hash are verified.
python quest/tools/device_data.py --serial "YOUR_QUEST_SERIAL" --rom "../personal-data/AnimalCrossing.iso"

# Optional: import a COPY of a PC .gci save into Quest Slot A.
python quest/tools/device_data.py --serial "YOUR_QUEST_SERIAL" --save "../personal-data/MyTown.gci"

# Export Quest Slot A/B .gci files to a separate local backup directory.
python quest/tools/device_data.py --serial "YOUR_QUEST_SERIAL" --export-save "../backups/quest-save-20260928"
```

The helper takes its ADB executable from `../toolchain/paths.json`; `--adb` can select another executable. It requires the full game to be stopped. `--rom` accepts `.iso`, `.gcm`, or `.ciso` and imports as `AnimalCrossing` with the same extension. Repeat `--save` for multiple staged `.gci` files; imports currently target Slot A, while exports include both slots. Each transfer verifies byte count and SHA-256. Identical existing files are retained; different existing Quest files or local exports are refused rather than overwritten. Changing an existing Quest save requires a deliberate separate backup/replacement procedure, not another import over it.

Use your own compatible GAFE01 game dump. No Nintendo ROM or assets are distributed. Keep original PC ROMs/saves separate, and import only copies of saves for testing. Loading the copied town and a subsequent write have been observed on Quest; a complete save/quit/reload cycle remains unverified. Preserve independent PC and Quest backups.

Existing Quest `settings.ini` is preserved on app launch; shaders are refreshed from the APK. APK updates signed with the same key using `adb install -r` preserve app data. Uninstalling or clearing app data removes private files, so export wanted saves first. The helper uses `run-as` for these **debug development APKs**; it is not a consumer release import mechanism. Do not overwrite Quest settings with PC defaults because the render backend and performance settings differ.

## Controller mapping in the current source

These are the implemented Touch controller bindings, pending a complete gameplay playtest:

| Quest input | Game action |
| --- | --- |
| Left stick | Movement |
| Right stick | Camera/C-stick; first-person turning uses the existing camera rules |
| Right A or right trigger | GameCube A: confirm/use tool |
| Right B or left trigger | GameCube B: cancel/hold to run |
| Left X / Y | GameCube X / Y |
| Left grip | GameCube L and tool-selection modifier |
| Left grip + left stick | D-pad/tool selection |
| Right grip | GameCube R |
| Right stick click | GameCube Z |
| Left stick click | Start |
| X + Y together | Recenter |
| Hold left grip, then press Y | Toggle first person (X released, outside pause menu) |

The right tool uses the controller aim pose; floating empty hands use each grip pose. Motion swings can trigger tool use when enabled. These bindings are in `quest/src/quest_vr_openxr.c_inc` and `quest/src/quest_vr.cpp`.

## Verification and current limits

`python quest/tests/gles_device_run.py` builds standalone ARM32 EGL diagnostics and runs them through `adb shell` in a unique `/data/local/tmp/acquest-gles` subdirectory. It does not install or launch the game app. On the tested Quest 3 (Adreno 740, GLES 3.2 driver V@0837.0.9), **299 checks passed**: all 139 specialized GX shaders plus the default shader linked, world/hand depth behaved correctly, sky day/night/weather checks passed, NES colors matched, and sRGB write control preserved encoded color values. An initial one-pixel daytime-sky assertion hit a cloud; the diagnostic now checks a sky region. Production appearance was not changed to satisfy the test.

Logs, captures, source hashes, and device details are retained in `../research/gles-device-*`. The initial 139-program driver compilation took about 6.9 seconds; this measures shader startup work, not gameplay frame rate. OpenXR lifecycle/input contract tests are available in `quest/tests/run_openxr_contract_tests.py`.

`python quest/tests/run_eye_size_tests.py` checks the production eye-size selector without a device, including runtime bounds, portrait/landscape sizes, no upscaling, invalid zero dimensions, pixel rounding, and extreme integer values.

Performance measurements on the same Quest 3 show the following progress. These are development build labels, not public releases:

| Diagnostic | Earlier v5 | Streaming v6 |
| --- | --- | --- |
| Native flat full-world game, 640 x 480 | 92.75 ms/frame; 69.8 ms vertex upload | Reaches the 60 FPS cap; about 13 ms game rendering and 1.5 ms vertex upload |
| Native stereo game path, fixed 640 x 640 per eye | 223.10 ms/frame; 173.30 ms vertex upload | 41.44 ms/frame; 14.95 ms vertex upload |

Both game diagnostics execute real game assets and render code offscreen without an Android Activity or the OpenXR compositor. The stereo test uses the production two-eye routing and produced distinct left/right images with no GL errors; its reduced, fixed eye size differs from the headset runtime's targets. These numbers do **not** establish headset frame rate or playable performance. The separate synthetic 3,750-draw upload test passed 238 correctness checks, including pixel parity and buffer wrapping. Receipts are under `../research/game-offscreen-*` and `../research/gles-device-*`.

The next optimization, included in version 7, rejects triangle/quad batches only when every vertex is outside the same actual eye clip plane. It preserves all display-list state, the sky, flat menus, actor residency, and full-town drawing. At the same fixed 640 x 640 eye size, the title scene falls to about 438 draws and 20.59 ms/frame with profiling, or 19.30 ms without profiling. The actual Quest session at 1,680 x 1,760 still showed roughly 36-40 outdoor FPS; menus can reach 72 FPS. These are different workloads and must not be compared as identical benchmarks. Runtime logs and the stereo inventory capture are in `../research/live-v7`.

`python quest/tests/run_batch_clip_tests.py` exercises the production classifier and dispatch ordering; device checks cover 768 triangle/quad cases with exact color and depth parity. `python quest/tests/run_card_io_tests.py` and `python quest/tests/run_village_save_io_tests.py` exercise real I/O code against disposable fixtures, including failed seek/write/flush/close and preservation of existing town backups. They passed 72 and 159 checks respectively on both Windows32 and Quest ARM32. The village writer now rejects failed flush/close before rotating any backup. [Offscreen game diagnostics](tests/OFFSCREEN_GAME.md) documents synthetic stereo, profiling, and disposable-save navigation. Those tests refuse to start while the installed game is running.

The current full game is ARM32 because original game/runtime structures still depend on 32-bit pointers and layout. An ARM64 probe demonstrates the runtime ABI only. Further gameplay, controller, performance, sleep/resume, audio, and save tests are required before calling this a playable release.
