# Building the Quest version

These instructions are for developers building on **Windows x64**. To play the release, use [Install and play](INSTALL.md). The Quest source lives on the `quest/standalone` branch; `master` is the PC version. Keep a separate checkout and build directory for each.

The full game currently builds for **ARM32 (`armeabi-v7a`)**. ARM64 is supported only by the separate OpenXR test app. Building the APK does not require or include a ROM or save; running the game requires your own supported game dump.

## 1. Create the isolated workspace

Use this layout; the scripts resolve tools and dependencies relative to the source checkout's parent directory:

```text
C:/Quest/AnimalCrossing/
  source/                    this repository, quest/standalone branch
  third_party/
    SDL/
    OpenXR-SDK/
  toolchain/
    jdk-21/
    sdk/
    ninja-1.13.2/
    paths.json
  build/                     generated automatically
  research/                  local diagnostic output
```

With Git, Python 3.11, and CMake installed, run from `C:/Quest/AnimalCrossing`:

```powershell
git clone --branch quest/standalone https://github.com/LiquidAzir/animal-crossing-vr.git source
git clone --branch release-2.30.10 --depth 1 https://github.com/libsdl-org/SDL.git third_party/SDL
git clone --branch release-1.1.63 --depth 1 https://github.com/KhronosGroup/OpenXR-SDK.git third_party/OpenXR-SDK
```

The dependency revisions used for the tested build are:

| Dependency | Tag | Commit |
| --- | --- | --- |
| SDL2 | `release-2.30.10` | `9c821dc21ccbd69b2bda421fdb35cb4ae2da8f5e` |
| OpenXR SDK | `release-1.1.63` | `f2448a8797c85814aa892efc1ab8707900fbcc78` |

The build entrypoint verifies these tags and rejects modified tracked dependency files. It does not download dependencies itself.

## 2. Set up the local toolchain

Install or extract these tools into the workspace layout above:

| Tool | Tested version / location |
| --- | --- |
| Portable Eclipse Temurin JDK | `21.0.12.1+1`, with `bin/java.exe` under `toolchain/jdk-21` |
| Android command-line tools | 22.0, with `bin/sdkmanager.bat` under `toolchain/sdk/cmdline-tools/latest` |
| Android NDK | `27.3.13750724` (r27d), under `toolchain/sdk/ndk` |
| Android SDK platform | `android-34` |
| Android Build Tools | `35.0.0` |
| Android platform tools | `adb.exe` under `toolchain/sdk/platform-tools` |
| Ninja | `1.13.2`, with `ninja.exe` under `toolchain/ninja-1.13.2` |
| CMake | `4.4.2` was tested; the project declares a minimum of 3.22 |

Use the official Android SDK Manager to install the NDK, platform, Build Tools, and platform tools into this local SDK. For example, from the workspace root, after extracting the JDK and command-line tools:

```powershell
$env:JAVA_HOME = (Resolve-Path ./toolchain/jdk-21).Path
& ./toolchain/sdk/cmdline-tools/latest/bin/sdkmanager.bat --sdk_root="$PWD/toolchain/sdk" "platform-tools" "platforms;android-34" "build-tools;35.0.0" "ndk;27.3.13750724"
```

Review the SDK Manager license prompts. These environment settings apply only to the current shell. Build Tools 35.0.0 is required by the packager; its D8 successfully processes the JDK 21 SDL classes used here.

## 3. Write `toolchain/paths.json`

Create the following UTF-8 JSON file, replacing `C:/Quest/AnimalCrossing` and the CMake location with your actual absolute paths. Use forward slashes. The workspace paths are intentional checks in the build script, not arbitrary overrides.

```json
{
  "java_home": "C:/Quest/AnimalCrossing/toolchain/jdk-21",
  "sdk_root": "C:/Quest/AnimalCrossing/toolchain/sdk",
  "ndk_root": "C:/Quest/AnimalCrossing/toolchain/sdk/ndk/27.3.13750724",
  "ndk_cmake_toolchain": "C:/Quest/AnimalCrossing/toolchain/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake",
  "android_jar": "C:/Quest/AnimalCrossing/toolchain/sdk/platforms/android-34/android.jar",
  "build_tools": "C:/Quest/AnimalCrossing/toolchain/sdk/build-tools/35.0.0",
  "cmake": "C:/Program Files/CMake/bin/cmake.exe",
  "ninja": "C:/Quest/AnimalCrossing/toolchain/ninja-1.13.2/ninja.exe",
  "adb": "C:/Quest/AnimalCrossing/toolchain/sdk/platform-tools/adb.exe",
  "clang_bin": "C:/Quest/AnimalCrossing/toolchain/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/windows-x86_64/bin",
  "clang_armv7_api24": "C:/Quest/AnimalCrossing/toolchain/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/windows-x86_64/bin/armv7a-linux-androideabi24-clang.cmd",
  "clangxx_armv7_api24": "C:/Quest/AnimalCrossing/toolchain/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/windows-x86_64/bin/armv7a-linux-androideabi24-clang++.cmd"
}
```

The final three entries are used by optional native test runners. The APK itself targets native API 29; the API 24 compiler wrappers above belong to standalone diagnostic executables.

## 4. Build and package

Run from `source`:

```powershell
python quest/tools/build_quest.py --mode game --abi armeabi-v7a --dry-run
python quest/tools/build_quest.py --mode game --abi armeabi-v7a --jobs 4 --version 14
```

For an update, choose a `--version` code greater than the version installed on the headset. The example uses the current source build's version 14. The dry run validates inputs and prints commands without building. Additional options are `--configure-only` and `--no-package`; `--build-dir` must be a child of the workspace's `build` directory.

The finished APK is:

```text
../build/game-arm32/apk/AnimalCrossingQuest-armeabi-v7a.apk
```

The packager checks the native ABI, package identity, version, app label, signature, and ZIP alignment. `package-receipt.json` next to the APK records hashes and tool versions. A successful build does not establish headset gameplay correctness; use the device checks below.

For the separate OpenXR probe, use `--mode probe` with either ABI. Its package ends in `.probe` and its app label is **Animal Crossing Quest XR Test**, so it can coexist with the game.

## 5. Install and verify

Follow [manual installation](MANUAL.md) to install your APK and import your own ROM or a copy of a save. Always select the Quest explicitly with ADB's `-s SERIAL` when more than one device is connected. PC and Quest saves and settings stay separate.

The packager creates a local signing key at `quest/.local/debug.keystore`. Keep this ignored file private and backed up: Android accepts an in-place update only when its signature matches the installed app. A fresh checkout generates a different key and cannot overwrite an existing release installation. Export saves before changing installations; uninstalling deletes app data.

The current sideload build is deliberately debuggable because the verified ROM/save transfer helper uses Android `run-as` to access private app storage. Changing that manifest setting also requires a replacement import/export path. The APK contains no ROM or personal saves.

Useful checks from `source` include:

```powershell
python quest/tests/device_data_test.py
python quest/tests/run_openxr_contract_tests.py --cxx C:/msys64/mingw32/bin/g++.exe
```

The first uses simulated transfers without a headset. Native host tests require the indicated MinGW compiler; several older diagnostic runners assume `C:/msys64/mingw32`. Device graphics tests under `quest/tests` also require a local ROM fixture and an idle headset. Read each runner before using it; offscreen component checks do not replace testing boat travel, save/reload, and sleep/resume inside the actual app.

Keep generated APKs, signing keys, local toolchains, ROMs, saves, logs, and captures out of source control. Include the files in `quest/licenses` when distributing the Quest build.
