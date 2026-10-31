# Native build setup

## Current native game builds (2026-11-01)

The playable port is built separately from the matching decompilation:

- Windows x86-64: use the Windows build script below. The latest recorded
  Windows -Game Release build produced the standalone package and passed
  125/125 tests on 2026-11-01. The Windows ASan/UBSan preset passed 108/108.
- Android arm64-v8a: the Gradle/NDK build script produces the native APK.
  NDK 30.0.16248370, Gradle 9.8.2 and AGP 9.5.0 built the Release APK on
  2026-11-01. That APK was not installed or launched in this build run.
- Linux x86-64: Fedora 44 Release and ASan/UBSan builds pass 111/111 tests.
  SDL's offscreen Vulkan probes also pass on the Intel iGPU. The executable was
  linked, but not launched; game boot and interactive use remain unverified.
- macOS universal: presets exist, but no build has been validated on macOS.

From the repository root:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/android/build.ps1 -BuildType Release

The owner supplies and interactively tests game data. No WSL or Docker is used.
The legacy MIPS toolchain setup notes follow.

### Linux x86-64 native port

Install Clang, CMake 3.24 or newer, Ninja, and the development/runtime
dependencies for the SDL3 Linux video, audio and Vulkan backends. From
`port/`, configure and build the playable native target and test suite:

```sh
cmake --preset linux-release -DHMBTN_BUILD_NATIVE_WINDOW=ON
cmake --build --preset linux-release -j16
ctest --preset linux-release
cmake --preset linux-asan-ubsan -DHMBTN_BUILD_NATIVE_WINDOW=ON
cmake --build --preset linux-asan-ubsan -j16
ctest --preset linux-asan-ubsan
```

The executable is `build/port/linux-release/bin/VanillaBTN`. On the Fedora 44
validation host, the base image lacked a compiler and development headers, so
the build used Fedora RPMs extracted into an ignored project-local sysroot under
`build/host-tools/`; no system packages were installed. That local sysroot is
machine-specific and is not part of the repository.

## Legacy MIPS toolchain setup

The supported host for this legacy toolchain is a separate Linux x86-64 machine. The owner excludes
WSL from the current work; do not install, invoke, or require it.
The historical GCC archive contains Linux executables, not Windows programs.
Windows/macOS native matching support has not been established. This restriction
does not apply to the future native game executable.

Requirements derived from `Dockerfile`, `cmake/`, `tools/`, and the old workflow:

| Component | Required upstream selection | Source / verification |
| --- | --- | --- |
| GCC cpp/cc1 | 2.95.2 PSX, old-gcc release 0.17 | GitHub decompals/old-gcc; SHA-256 below |
| PSY-Q runtime | 4.7 | Owner-provided local zip; SHA-256 below |
| MIPS binutils | `mips-linux-gnu-{as,ld,objcopy}`; Fedora `binutils-mips64-linux-gnu` is adapted by the setup script to O32/ELF32 little-endian | Host distribution package; upstream did not pin a version |
| Python | minimum 3.10; reference image used 3.14 | Standard library only; no pip packages found |
| CMake | minimum 3.20 for this toolchain, 3.24 for the port | Host distribution package; reference did not pin patch version |
| Ninja | reference distribution package | Upstream did not pin version |
| maspsx | vendored revision | `tools/maspsx/UPSTREAM` |
| curl, tar, sha256sum | host tools | Setup download, extraction, verification |
| objdiff-cli | 3.8.2, optional report only | Previous workflow; optional report tool |

Archive hashes, preserved from the reference Dockerfile:

```
932ed3669710a82b12570c29c46ff42989b6505b2af10db84d4551d55dfc0b1c  gcc-2.95.2-psx.tar.gz
e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745  psyq-4.7.zip
```

On a native Linux x86-64 host, install Python 3.10+, CMake 3.20+, Ninja,
curl, tar, `sha256sum`, and MIPS binutils. Ubuntu/Debian package names:

```sh
sudo apt install python3 cmake ninja-build curl tar coreutils binutils-mips-linux-gnu
```

Fedora package names:

```sh
sudo dnf install python3 cmake ninja-build curl tar coreutils binutils-mips64-linux-gnu
```

The Fedora cross-binutils use a `mips64-linux-gnu-` command prefix. Setup verifies that the installed linker supports `elf32ltsmip` and generates local wrappers in
the ignored `build/` tree, forcing the assembler to ABI32 and the linker to
little-endian ELF32. Standard `mips-linux-gnu-` tools are used directly.

From the repository root, provide your local PSY-Q 4.7 archive:

```sh
PSYQ_ARCHIVE=/path/to/your/runtime-4.7.zip bash tools/setup-native.sh
```

Setup verifies the local SDK archive and downloads only the checksum-pinned GCC
archive from the release URL in the script. It prepares ignored `sdk/` directories,
normalizes header names/line endings, and configures the standard matching build
directory with absolute tool paths. Repeating it re-verifies both inputs and
refreshes their contents. It does not install system packages, invoke sudo, or
download proprietary SDK files. A partial download is never used as a compiler.

An existing matching build directory configured for different tool paths must be
reconfigured by setup first. Do not put SDK archives in tracked directories.
The compatibility script `tools/install_host_sdk.sh` now forwards to native setup.
No container command is required.

The reference did not pin host package versions. A fully reproducible host-tool
manifest remains pending a verified native baseline; no versions have been
invented here. Workflows/CI are excluded by owner instruction. Existing upstream
workflow files are unchanged and are not part of this setup.

## Historical port foundation snapshot (2026-10-25)

The foundation notes in this section describe the early bring-up milestone and
are superseded by the current native game-build status above.

Presets live in the separate `port/` project. From that directory:

```sh
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
cmake --preset linux-asan-ubsan
cmake --build --preset linux-asan-ubsan
ctest --preset linux-asan-ubsan
```

Desktop presets also exist for `windows-release` (LLVM-mingw/UCRT, without MSVC)
and `macos-universal-release`.
These build the compatibility layer and eleven original source units.
At that early milestone Android presets had not been configured. Android
arm64-v8a build support was added later; see the current status above.

No SDL dependency, SDK, network, or game data is required by the port foundation.
It is not a playable game. Only `math_util.c`, `farm_data_size.c`, `horse_names.c`,
`picture_codes.c`, `event_flags.c`, `farm_tables.c`, `farm_state.c`, `main.c`,
`graphics/frame_buffers.c`, `graphics/prim.c`, and `cd/afile.c`
from `src/` are compiled so far. The game entry point is renamed to hm_game_main
in the port build and archived, not linked as an executable or called by tests.
The work-memory fixture includes main.c separately, makes its unused entry point
local, and calls only the original frame setup/region helper functions. GPU/GS
dependencies there are explicit test observations, not runtime implementations.

## Windows first

From the repository root, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1
```

The policy override applies only to this process, not to machine/user settings.
The script downloads checksum-pinned LLVM-mingw 20250709, CMake 3.31.6 and Ninja
1.12.1 into ignored `build/host-tools/`, verifies cached archives, then configures,
builds and tests `windows-release`. It does not require administrator access,
WSL, MSVC or the PSX SDK. Subsequent runs reuse local downloads. Network is
required for initial tool provisioning, never for the foundation tests themselves.

To run the preset commands individually in a PowerShell session that permits scripts:

```powershell
& ./port/packaging/windows/setup-toolchain.ps1
Set-Location port
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

For native Windows ASan/UBSan tests:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-asan-ubsan
```

The sanitizer preset uses a dynamic ASan runtime from the toolchain directory
added to PATH by setup; release builds retain static linkage. The wrapper uses
`cmake --fresh` so compiler upgrades cannot drop cached preset options. When
running CMake directly after upgrading a compiler, also use `--fresh` on configure.

The optional source audit requires any local Python 3.10+ interpreter, independently
of the port build. It does not need an SDK, a network connection, or game assets:

```powershell
python -m unittest discover -s port/analysis -p "test_*.py"
python port/analysis/source_audit.py --output build/port/source-audit.json
```

It reports lexical candidates, not a complete compiler/linker inventory. See
`ANALYSIS.md` for limitations. Python is not required by the Windows build wrapper.

For a syntax-only compiler inventory of all 113 original translation units:

```powershell
python port/analysis/compile_probe.py --compiler build/host-tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe --output build/port/compile-probe.json
```

This writes each file's diagnostics to ignored build output. Uncompilable units
are expected at this stage: the command reports counts rather than treating all
source errors as a script failure. It neither links nor runs the game. A syntax
pass is not proof that MIPS inline assembly can be assembled by the native backend.

## Validation on 2026-10-25

An earlier Ubuntu WSL launch failed (`HCS_E_HYPERV_NOT_INSTALLED`); the owner
subsequently excluded WSL, so no further WSL attempts are required. Matching
validation is deferred to a separate Linux host. CMake, Ninja and Clang were not found on the
Windows PATH initially. After provisioning project-local tools, Windows release
configure/build and all 25 tests passed. The Windows ASan/UBSan preset also
built and passed 25 tests with Clang 20.1.8; Clang 19's ASan had failed during
startup (ADR-005). The matching build, Linux/macOS builds and full game execution
remain unverified. Eleven native guards in five original files are listed in
ANALYSIS.md. Their matching branches reconstruct the preceding source text
exactly; this does not prove byte-for-byte output parity.

For the optional synthetic OT-construction benchmark, from a repository-root
session with the local toolchain on PATH and windows-release already built:

```powershell
cmake --build build/port/windows-release --target hmbtn_ot_bench
& ./build/port/windows-release/hmbtn_ot_bench.exe
```

This target is excluded from default builds and is not a CTest or a rendering
benchmark. It times repeated construction of one 2048-word OT with 1000 packets,
after the persistent addresses have been registered. See PLATFORMS.md for scope
and measurements; no frame budget or full-game performance result is implied.
