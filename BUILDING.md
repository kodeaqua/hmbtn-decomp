# Building VanillaBTN

Instructions for building **VanillaBTN** from source code on supported platforms.

VanillaBTN uses a modern native architecture compiling directly from decompiled C source and clean-room runtime modules—no emulators, no static recompilers, and no Docker containers required.

---

## Table of Contents
1. [Windows (Native x86-64)](#windows-native-x86-64)
   - [Prerequisites](#windows-prerequisites)
   - [Build Command](#windows-build-command)
   - [Build Presets & Options](#windows-build-presets--options)
   - [Outputs](#windows-outputs)
2. [Android (Native arm64-v8a)](#android-native-arm64-v8a)
   - [Prerequisites](#android-prerequisites)
   - [Build Command](#android-build-command)
   - [Outputs](#android-outputs)
3. [Linux (Native x86-64)](#linux-native-x86-64)
4. [Running Tests](#running-tests)
5. [Troubleshooting & Tips](#troubleshooting--tips)

---

## Windows (Native x86-64)

### Windows Prerequisites
- **Operating System**: Windows 10 or Windows 11 (64-bit).
- **Shell**: PowerShell 5.1 or newer.
- **Git**: Installed and accessible.

> [!NOTE]
> You do **not** need to install Visual Studio, MSVC, PSY-Q, or WSL. The build script automatically provisions an isolated, self-contained LLVM-MinGW (Clang), Ninja, and CMake toolchain under `build/host-tools/` without altering system PATH or polluting your environment.

### Windows Build Command

Open PowerShell in the repository root and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
```

### Windows Build Presets & Options
- `-Preset windows-release`: Release build optimized with `-O3` and release settings.
- `-Game`: Compiles the playable game executable (`VanillaBTN.exe`) alongside unit tests and creates a standalone release archive.
- To build only the technical unit test suite without packaging:
  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release
  ```

### Windows Outputs
- **Playable Game Binary**: `build/port/windows-release/bin/VanillaBTN.exe`
- **Release ZIP Archive**: `dist/VanillaBTN-windows-x64-v0.9.0.zip`
- **User Data & Saves**: Stored in your user's `Documents\VanillaBTN\` folder (`saves/` and `settings.json`).

---

## Android (Native arm64-v8a)

VanillaBTN compiles natively for Android devices (`arm64-v8a`), featuring a dedicated touch overlay, controller support, and full hardware rendering.

### Android Prerequisites
- **JDK / Java**: Java 17 or Java 21 (e.g. from Android Studio JBR, OpenJDK, or `C:\DevTools\jdk`).
- **Android SDK**: Android SDK (API 26+ minimum, API 35 compileSdk) with platform tools.
- **Android NDK**: NDK r27c or newer (e.g. `27.x`, `28.x`, `29.x`, or `30.x`).
- **Gradle**: Gradle 8.10+ (or will use downloaded Gradle from wrapper).

> [!TIP]
> The script automatically detects environment variables (`JAVA_HOME`, `ANDROID_HOME`, `ANDROID_NDK_HOME`) as well as standard install locations in `%LOCALAPPDATA%\Android\Sdk`.

### Android Build Command

From PowerShell in the repository root:

```powershell
# Build Release APK (signed with debug keystore for easy installation)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/android/build.ps1 -BuildType Release

# Or build Debug APK
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/android/build.ps1 -BuildType Debug
```

Alternatively, build directly using Gradle from the Android packaging directory:
```powershell
cd port/packaging/android
gradle.bat assembleRelease
```

### Android Outputs
- **Release APK**: `dist/VanillaBTN-android-v0.9.0.apk`
- **Gradle Build Output**: `port/packaging/android/app/build/outputs/apk/release/app-release.apk`

---

## Linux (Native x86-64)

Linux x86-64 is validated on Fedora 44. Install Clang, CMake 3.24 or newer,
Ninja, and the development/runtime dependencies for SDL3's Linux video, audio
and Vulkan backends. From the repository root:

```sh
cd port
cmake --preset linux-release -DHMBTN_BUILD_NATIVE_WINDOW=ON
cmake --build --preset linux-release -j16
ctest --preset linux-release
```

The executable is `build/port/linux-release/bin/VanillaBTN`. For memory and
undefined-behavior checks, use the `linux-asan-ubsan` preset with the same
commands. The owner supplies game data and handles interactive gameplay tests.

---

## Running Tests

VanillaBTN includes a native unit test suite for math, GTE fixed-point vectors,
VRAM/GPU commands, memory card operations, layouts, and gameplay logic.

To run the test suite:
```powershell
ctest --test-dir build/port/windows-release --output-on-failure
```

---

## Troubleshooting & Tips

- **Executable In Use (`Permission Denied`)**:
  If `VanillaBTN.exe` or `hmbtn_native.exe` is currently running, the linker will fail with permission denied. Close the running game before compiling `-Game`.
- **Game Disc Image**:
  VanillaBTN requires the user's legally owned *Harvest Moon: Back to Nature (USA)* disc image (`.bin`/`.cue` or `.iso`). On first launch, a file picker lets you select the disc file once, and the path is remembered in `settings.json`.
