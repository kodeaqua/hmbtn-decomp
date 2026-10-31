# AGENTS.md — VanillaBTN — Native Port (Harvest Moon: Back to Nature)

Instructions for Codex (and any other coding agent) working in this repository.
Read this whole file before making changes. Nested `AGENTS.md` files (e.g. `port/AGENTS.md`)
override this one for their subtree.

## 0. Mission

### Current owner overrides (2026-10-25 to 2026-10-31)

- **Pure Vanilla Essence & Anti-Bloat Policy**: This project strictly rejects out-of-place,
  bloated, or game-breaking mods that ruin the authentic nostalgic essence of Harvest Moon:
  Back to Nature. Any feature additions must remain tasteful, elegant, and 100% faithful to
  the original game spirit. Enhancements default OFF at runtime to preserve pure vanilla parity;
  options that preserve a vanilla behavior retain that vanilla default.
- Executable binary is named `VanillaBTN.exe`. Standalone release archive is produced at
  `dist/VanillaBTN-windows-x64-v0.9.0.zip`.
- Use 16 parallel build jobs (`cmake --build ... -j16`), as requested by the owner on 2026-10-30.
- The owner paused multilingual work on 2026-10-28. Its WIP is preserved in the
  named stash for `port/indonesian-language`; current runtime fixes belong on
  `fix` from main.
- Android scaling rollback (2026-10-31): after the owner reported severe
  slowdown on tablets, especially Adreno, Android Native PS1, Enhance PS1, and
  Modern profiles must keep native internal scene resolution. Modern may retain
  its 16:9 viewport and bilinear presentation; do not enable internal viewport
  scaling in Android profiles. Android GPU scene filtering must follow the
  CPU mobile path: nearest texture sampling for Bilinear mode, while the final
  presentation may remain bilinear (ADR-165). These decisions supersede ADR-163.
  Windows scaling and filtering remain unchanged.

- Prioritize Windows x86-64 native bring-up before Linux, macOS, and Android.
  Phase 1 targets Windows; retain the portable architecture for the other targets.
- Do not add, modify, run, or provision workflows/CI for the current work.
  Existing workflow files remain untouched. CI requirements elsewhere in this
  document are deferred until the owner explicitly requests CI work.
- Use native Windows tools only for the current port work. Do not install,
  invoke, or require WSL.
- The owner performs interactive gameplay testing. Agents handle implementation,
  builds and isolated technical/unit checks; do not launch gameplay or replay
  sessions unless the owner explicitly requests them.
- Windows user data belongs in the active user's Documents/VanillaBTN folder,
  resolved through the OS user-folder API (including redirected Documents).
  Store saves under saves/ and configuration at settings.json. Do not hardcode
  a username or use AppData for these files. This overrides the default pref-path
  location below for Windows.
- The owner explicitly requests an ESC pause interface available across scenes,
  using the original game's font and dialog presentation. Enable its compile
  option in the Windows -Game script; its runtime default is available, with
  enhance.pause_menu=false as the explicit OFF path. This requested interface
  overrides section 9's runtime-OFF default for the pause interface only.
  Graphics enhancements (scaling, widescreen, filtering, AA) still default OFF.
  Their renderer paths are implemented with partial coverage and safe fallback;
  do not advertise complete coverage or unverified gameplay/device behavior.

- The owner also requests VSync/FPS caps and visual interpolation while keeping
  game logic/time native. Windows -Game compiles interpolation; its runtime
  enhance.interpolation defaults OFF. This does not authorize accelerating
  simulation or claiming pending renderer enhancements are complete.

- Graphics work remains the active validation priority. The owner has since
  confirmed sleep/save is safe; treat the earlier crash report as historical
  unless it reproduces again. Windows -Game also compiles Enhance Scaling;
  enhance.scaling defaults OFF. Widescreen now has an opt-in expanded farm
  world viewport (ADR-056);
  OFF retains 4:3. ADR-057 extends ON presentation to all screens and keeps
  native/CPU fallback aspect stable. GPU Auto shares native and enhanced scene
  output, with Software retained as the reference and a safe CPU fallback.
  Scaling, widescreen, filtering, PGXP, dithering bypass, FXAA and MSAA have
  implementation paths, but coverage is partial and gameplay/device validation
  remains owner-tested. Report CPU raster performance and untested behavior
  plainly.
- Graphics Mode offers Native PS1, Enhance PS1, and Modern. Android exposes the
  same three choices, but its Modern/Enhance profiles bypass the desktop PGXP
  compute path for mobile performance (ADR-131/132); do not claim PGXP is active
  on Android.
- In-game mouse/touch clicks and mouse-to-gamepad injection are temporarily
  disabled by ADR-136. Mouse navigation in the pause menu remains available.
  Keep the controls model; do not describe inventory/dialogue taps as working.

### Current state and agent workflow (as of 2026-10-30)

- Active branch workflow: Always work on `fix` branched from `main`. Commit changes,
  push to `origin/fix`, open PR to `main` via `gh pr create`, squash-merge via `gh pr merge --squash`,
  and sync both branches (`git checkout main; git pull; git checkout fix; git reset --hard origin/main`).
- Build command for playable Windows game executable and tests:
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game`
- Decomp Overlay Manifest Integrity Rule: Whenever any file in `src/overlay/11_GF_FARM/`
  is edited (e.g. `calendar.c`, `status_pages.c`), the SHA-256 hash of the normalized content
  (`\r\n` replaced with `\n`) MUST be recalculated and updated in `port/overlay/farm_state/manifest.cmake`.
  Failure to do so causes CMake configuration failure.
- Active Executable Lock: If `VanillaBTN.exe` or `hmbtn_native.exe` is currently running, linking will fail with
  `Permission denied`. Never kill the user's running process without consent; check `Get-Process`
  first and ask the owner to close the game before compiling `-Game`.
- Implemented Features:
  1. ESC Pause Menu with Settings (Graphics, Controls, Gameplay) and Resume / Exit Game.
  2. Controls Options (ADR-081): Gamepad, Keyboard (Preset A/B/C/Custom), and
     Mouse settings. In-game mouse/touch interaction is disabled by ADR-136;
     pause-menu mouse navigation remains available.
  3. Gameplay Enhancements (ADR-080): Walk through crops, 99 item stacking in rucksack with
     quantity selector dialog, durable fences, connected wooden rails, wild dog attacks toggle,
     smart animal bell, optional 10-heart stamina HUD with fatigue gauge,
     calendar NPC birthday/milestone dialogue info on X press, and roaming horse.
  4. Latest recorded Windows unit suite: 124 tests passing (ADR-136; not
     rerun for this documentation update).
  5. Fast Text page-break fixes remain in source, but the option was removed
     from the menu and disabled pending a better multi-line pagination design
     (ADR-133).
  6. Rucksack reorganize screen strict category protection preventing tool/item cross-swapping (ADR-107).
  7. Android arm64-v8a native support with native internal resolution in all
     graphics profiles (ADR-164).

These explicit owner decisions override conflicting roadmap, CI skeleton,
workflow, and definition-of-done requirements below.

This repo is a **matching decompilation** of *Harvest Moon: Back to Nature*
(PlayStation, USA, `SLUS_011.15`). Goal of this effort:

> Turn the decompiled C source into a **native game executable** for
> **Windows, Linux, macOS, and Android** — fast, faithful to the original by default,
> and extensible with optional "enhanced" features added **only when the owner asks**.

This is a *port of the decompiled source*, **not** an emulator and **not** a static recompiler.
We compile the game's own C code natively and provide our own implementation of the PS1
hardware/SDK surface it calls (GPU, GTE, SPU, CD-ROM, pads, memory card, BIOS/kernel).

### Priorities (in order, when they conflict)
1. **Never break the matching build.** (Section 3)
2. **Vanilla fidelity**: with all enhancements OFF, game logic behaves like the original.
3. **Correctness and determinism.**
4. **Performance** on low/mid-range hardware (Section 8).
5. **Portability**: no platform-specific code outside the platform layer.
6. Enhancements (Section 9) — never implemented speculatively.

### Communication
- The owner writes in **Indonesian (informal)**. Reply to the owner in Indonesian.
- Code, comments, commit messages, docs, and this file stay in **English**.
- Be direct. State what you ran, what passed, what you did not verify.

## 1. Known facts (from README) vs. things to discover

Verified from the README:
- Matching decomp of the PS1 USA release; builds the EXE and **13 overlays** byte-for-byte,
  plus the full disc image if the user supplies their own disc.
- Build system: **CMake + Ninja**, upstream wraps it in a `Dockerfile`. SDK is **PSY-Q 4.7**,
  assembler wrapper in `tools/maspsx`.
- **Owner decision: this project does NOT use Docker** (no containers for building, testing, CI, or packaging).
  The `Dockerfile` is kept only as a *reference* for which tools/versions the matching build needs (Section 12).
- Two non-source exceptions: one patched SDK word (`sdk/patches.txt`) and 13 `GAP` padding entries in overlay `CMakeLists.txt`.
- Layout: `.github/workflows`, `cmake/`, `disc/`, `sdk/`, `src/`, `tools/`, `CMakeLists.txt`, `Dockerfile`.
- License: CC0-1.0 for this repo's own work; `tools/maspsx` has its own license. Game data and PSY-Q are **not** included and must never be added.

**Not yet known — discover in Phase 0 and write findings to `docs/port/ANALYSIS.md`:**
- Exact `src/` structure, how overlays are organized and loaded, global symbol collisions between overlays.
- Which PSY-Q libraries and which functions/macros are actually used (libgte, libgpu, libgs, libspu, libsnd, libcd, libpad, libcard, libapi, libetc, libpress/MDEC, etc.).
- Whether the game uses FMV (STR/MDEC), XA-ADPCM streaming, CD-DA, or only SPU samples/sequences.
- Any inline assembly, `INCLUDE_ASM`-style files, GTE inline macros, hardcoded hardware addresses (`0x1F80xxxx`, `0x8xxxxxxx`).
- Binary asset formats read into structs, and code that stores pointers in 32-bit integers.

Do not guess. If something here turns out wrong, fix this file in the same PR.

## 2. Repository layout (target)

Existing directories are untouched. The port lives in **new** directories:

```
src/ sdk/ tools/ disc/ cmake/        # matching decomp — see Section 3
port/
  CMakeLists.txt                      # port build, separate from the matching build
  AGENTS.md                           # optional, port-specific details
  compat/                             # force-included headers: fixed-width types, macros, PSY-Q-header shims
  psx/                                # clean-room runtime: gte, gpu, spu, cd, pad, card, bios/kernel, events
  gfx/                                # backend interface + backends + shaders
  platform/                           # SDL layer: window, input, audio out, filesystem, timing, lifecycle
  overlay/                            # overlay linking / symbol namespacing
  enhance/                            # on-demand enhancements (Section 9); empty at start
  packaging/{windows,linux,macos,android}/
  tests/                              # unit, replay, golden-frame
docs/port/
  ANALYSIS.md  DECISIONS.md  PLATFORMS.md  ENHANCEMENTS.md  THIRD_PARTY.md
```

## 3. Hard rules for the matching decomp (do not violate)

1. Preserve the matching source and build inputs. Keep port behavior in the port layer,
   and make only narrow, owner-requested decompilation edits under the guards below.
2. Prefer **zero edits to `src/`**. Port behavior goes in `port/compat/` via force-included headers
   (`-include`), macro/type shims, and link-time replacement of library functions.
3. If `src/` must change for the port, use a narrow guard and document it:
   ```c
   #ifdef HMBTN_PORT
   /* PORT: <why> — see docs/port/DECISIONS.md#adr-NNN */
   #else
   /* original code, unchanged */
   #endif
   ```
   The `#else` branch must remain the exact original so the matching build is unaffected.
   Keep a running list of every such guard in `docs/port/ANALYSIS.md`.
4. Never "clean up", reformat, rename, or refactor decomp source "while you're there".
5. Do not edit `sdk/patches.txt` or overlay `GAP` entries unless the task is specifically about them.
6. State clearly when a build or runtime behavior could not be verified.

## 4. Legal and licensing rules

- **Never commit**: disc images, extracted game assets, PS1 BIOS, PSY-Q SDK files/headers/libs, or anything derived verbatim from them.
- Port library headers/implementations are **clean-room**: write them from public hardware documentation
  (e.g. psx-spx/nocash docs) and from how `src/` uses the API. Do not copy Sony SDK code or leaked material.
- The port reads the game data from **the user's own disc image** at runtime (see Section 7). Verify it against the known SHA-1 and fail with a clear message on mismatch.
- Port code is CC0-1.0 like the rest of the repo unless the owner says otherwise.
- Third-party dependencies: permissive only (zlib, MIT, BSD, Apache-2.0, ISC, public domain). Avoid GPL/LGPL/AGPL in anything linked into the game binary (Android and macOS distribution make this a problem).
  Pin versions, and record name/version/license/URL in `docs/port/THIRD_PARTY.md`.
  If evaluating an existing PSX-API reimplementation (for example the one used by the REDRIVER2 project), verify its license first and record the decision as an ADR — do not copy code in without that.

## 5. Architecture decisions (defaults; change only via ADR)

Record every deviation or new decision in `docs/port/DECISIONS.md` as a short ADR
(context, decision, consequences, date).

| Area | Default decision |
|---|---|
| Language | C11 for decomp code and `psx/` runtime; C++17 allowed in `platform/`, `gfx/`, `enhance/` if clearly beneficial. No exceptions/RTTI in hot paths. |
| Toolchains | Clang everywhere (Linux: clang/gcc; Windows: clang-cl or LLVM-mingw — MSVC not required; macOS: Apple clang; Android: NDK clang). |
| Build | CMake ≥ 3.24 + presets (`CMakePresets.json`), Ninja. **No Docker, anywhere.** Separate from the matching build. Third-party via `FetchContent`/vendored with pinned versions. |
| Platform layer | **SDL3** (window, input, gamepad, audio device, filesystem/pref paths, timing, Android lifecycle, file dialogs). Nothing outside `port/platform/` includes SDL headers. |
| Graphics | Behind a small `gfx_backend` interface. First backend: **SDL_GPU** if it meets needs on all four platforms (Vulkan / D3D12 / Metal); otherwise OpenGL 3.3 core + GLES 3.0 sharing one GLSL subset. Decide with a short spike and write the ADR. |
| Execution model | **Single-threaded deterministic game thread.** PS1 interrupts/callbacks (VSync, DMA, CD, SPU) are simulated at defined points in the main loop, never from real threads. Only audio output uses a separate thread, fed through a lock-free ring buffer. |
| Overlays | Statically link all 13 overlays into one executable with per-overlay symbol namespacing (e.g. via generated `-D`/`objcopy --prefix-symbols`/linker scripts), with a table-driven loader replacing disc-based overlay loading. No `dlopen` on the critical path (Android/macOS friendliness). |
| Memory | Use real C globals/heap, **not** an emulated 2 MB RAM image, unless an ADR proves it necessary for specific data. Scratchpad (`0x1F800000`) → a normal static 1 KB buffer. |
| 64-bit | Targets are 64-bit only (x86-64, arm64). Code must be 64-bit clean (Section 6). |

## 6. Porting rules for the decomp code

**Types and layout**
- Provide `port/compat/types.h` mapping `s8/u8/s16/u16/s32/u32/s64/u64` to `<stdint.h>` types. PS1 `int`/`long` are 32-bit — never rely on host `long` (differs on LP64 vs LLP64).
- Every struct that is read from disc/save data or shared with assets: **explicit layout** (fixed-width fields, `_Static_assert(sizeof(...))` and `offsetof` checks). Pointers inside such structs become 32-bit offsets/handles with accessor helpers.
- Find and fix pointers stored in 32-bit integers, `(u32)ptr` casts, and arithmetic on addresses.
- Assume little-endian (all four targets are), but isolate byte reads of asset data behind helpers so big-endian never silently breaks.

**Undefined behavior and old-compiler habits**
- Build the port with `-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks` by default; the original GCC 2.x-era code relies on these semantics.
- Provide ASan/UBSan dev presets and run the game under them regularly; fix findings in the port layer first, `src/` guard last.
- Do not "fix" suspicious original behavior (bugs are part of vanilla). Flag it in `ANALYSIS.md` instead.

**Hardware/SDK surface**
- Implement the library API the game *actually uses* (found by grep/link analysis), not the whole PSY-Q SDK. Unused functions get no implementation and a link error is the signal to add one.
- Unimplemented-but-called functions must `abort()` with the function name in dev builds — never silently return 0.
- GTE: implement as `static inline` C with **bit-exact** fixed-point behavior (rounding, saturation, flag registers) and unit tests against known vectors from public documentation. Optional SIMD only after profiling, with the scalar version kept as the reference.
- GPU: emulate the command/primitive/ordering-table interface (OT linking, DMA chain walk, DrawSync/ClearOTag, texpages, CLUTs, semi-transparency modes, dithering) at the API level; keep a 1024×512 16-bit VRAM model where the game reads/writes VRAM (LoadImage/StoreImage/MoveImage).
- SPU: ADPCM (VAG) decode, ADSR, pitch, 24 voices, reverb, 44.1 kHz mix. Make the mixer deterministic and independent of the host audio callback size.
- CD: ISO-9660 + Mode 2 Form 1/2 sector access over user disc image, with a timing model that does not exceed original latency assumptions but never makes the game wait *longer* than necessary. If XA-ADPCM/CDDA/STR are used, implement their decoders in Phase 5+.
- Pads/memcard: map to normal save files in the per-user data directory; keep the original memory card file format only if the game's code requires it.

## 7. Game data handling

- Runtime input: user's disc image (`.bin/.cue`, optionally `.iso`/`.chd` later) or a folder of extracted files. First run: file picker (desktop) / Storage Access Framework (Android); remember the choice in the config.
- On Android, do **not** require broad storage permissions. Use SAF + file descriptors, or copy/import into app-private storage with progress UI.
- Validate by SHA-1 of the known tracks/files; show a human-readable error if wrong region/revision.
- Config and saves: `SDL_GetPrefPath`-style per-user directories on every platform. Never write next to the executable.
- Never log file contents or full user paths in release builds' crash reports.

## 8. Performance requirements and optimization policy

**Initial budget (validate and adjust with measurements, record in `docs/port/PLATFORMS.md`):**
- Native 60 fps equivalent pacing at 1× internal resolution on low-end desktop and a mid-range Android (Snapdragon 6/7-class) with comfortable headroom.
- Port overhead (GTE + GPU submission + SPU mix + platform) target: < 2 ms/frame desktop, < 4 ms/frame mid-range Android.
- Cold start to title < 3 s on desktop (after first-run shader/pipeline cache creation); no per-frame heap allocations in steady state; no hitches from shader compilation.

**Policy: measure first.** No speculative micro-optimization. For any optimization PR include before/after numbers and the tool used
(`perf`/VTune/Tracy, Instruments, Android Studio profiler/Perfetto/simpleperf, RenderDoc/Xcode GPU capture).

**Defaults to implement from the start (cheap, architectural):**
- Release flags: `-O2` (selective `-O3` on proven hot files), ThinLTO/LTO, `-fvisibility=hidden`, dead-code stripping. Optional PGO/BOLT later as a separate task.
- Conservative CPU baseline: x86-64-v2 and armv8-a. Any AVX2/NEON-specific path needs runtime detection and a scalar fallback.
- GPU: batch primitives by state (texpage/CLUT/blend), persistent/ring buffers, upload only dirty VRAM regions, CLUT decode in shader, pipeline cache persisted to disk, 2-frame-in-flight max to keep input latency low.
- Frame pacing: fixed 60 Hz (or 50 Hz for PAL if ever supported) game tick decoupled from display refresh; correct behavior on 90/120/144 Hz and VRR displays; no drift.
- Audio: pull-based callback, lock-free ring, no allocation or locks in the callback.
- Input: sample at the start of each game tick; keep input-to-photon latency minimal.
- Android: handle pause/resume/surface loss, thermal/battery friendliness (optional fps cap), and consider Android Frame Pacing (Swappy) after the basic loop works.

## 9. Enhanced features — ON DEMAND ONLY

**Do not implement any enhancement unless the owner explicitly asks for it in the current task.**
Do, however, keep the architecture ready for them.

Rules when the owner requests an enhancement `X`:
1. Create `port/enhance/<x>/` (self-contained module). Keep hooks in core code minimal and generic (callbacks / extension points), not feature-specific `if`s scattered around.
2. Gate with a CMake option `HMBTN_ENH_<X>` (default **ON for builds only if the owner says so; the runtime setting defaults OFF**) and a runtime config key `enhance.<x>`.
3. **Vanilla parity mode must stay available and untouched**: with all `enhance.*` OFF, logic and rendering stay equivalent to the baseline. Enhancements must not alter game-logic state or RNG unless that is the explicit point of the feature.
4. Add a test or a replay check proving the OFF path is unchanged, and document the feature in `docs/port/ENHANCEMENTS.md`.
5. Add it to the settings UI/config schema; never hardcode.

Backlog of *ideas* (not tasks — do not build unprompted): internal resolution scaling, widescreen, texture filtering, PGXP-style subpixel precision / perspective-correct textures, 60 fps interpolation, save states, rewind, fast-forward, controller remapping/touch layout editor, UI scaling, CRT/scanline shaders, texture replacement packs, audio upgrades (resampling/reverb quality), cheat/debug menu, localization hooks, QoL tweaks (skip cutscenes, faster text), achievements/telemetry (opt-in only).

## 10. Platform requirements

| Platform | Arch | Notes |
|---|---|---|
| Windows | x86-64 (arm64 optional later) | Windows 10+. Static CRT, no installer required (zip). D3D12/Vulkan/GL per ADR. |
| Linux | x86-64, aarch64 | glibc baseline pinned by building on the oldest supported Ubuntu LTS runner/host (no containers). Ship AppImage and/or Flatpak. Wayland + X11 via SDL. |
| macOS | arm64 + x86-64 (universal) | Metal path. `.app` bundle, hardened runtime, ad-hoc sign for dev; notarization is a later, owner-requested task. |
| Android | arm64-v8a (x86-64 for emulator only) | NDK (current LTS-level), Gradle + CMake `externalNativeBuild`, SDL3 Java glue, minSdk chosen by ADR, 16 KB page-size compatible native libs, AAB + APK outputs. Touch overlay + gamepad support. |

Design for all four from day one (no desktop-only assumptions: blocking dialogs, absolute paths, `fopen` on user-picked files, exit-on-error).
**Bring-up order:** Linux → Windows/macOS (same SDL code) → Android. Keep CI green on every platform that already works.

## 11. Roadmap / phases

Work through phases in order; do not start enhancements before Phase 6 unless the owner says so.

- **Phase 0 - Recon & guardrails.** Produce `ANALYSIS.md` (libs/functions used, overlay map, symbol collisions, hardware addresses, asm/GTE macro usage, pointer/layout hazards, asset formats). Create `port/` skeleton, CMake presets, CI skeleton.
- **Phase 1 — Native boot (Linux).** Compile `src/` natively with compat headers + stubbed runtime. Reach the main loop and title init (black screen OK). Overlay linking solved.
- **Phase 2 — Video.** GTE, GPU API, VRAM model, first backend, rendering of the first screens.
- **Phase 3 — Disc & data.** CD/file layer over user disc image, overlay loading, assets, first-run flow.
- **Phase 4 — Audio.** SPU mixer, music/SFX, then XA/CDDA/FMV if used.
- **Phase 5 — Input & saves.** Keyboard/gamepad mapping, memory card → save files, settings persistence.
- **Phase 6 — Full playthrough parity.** All overlays reachable; replay tests; fix timing/logic divergences.
- **Phase 7 — Platforms.** Windows, macOS, Android builds, packaging, CI matrix.
- **Phase 8 — Performance pass.** Profile on target devices, fix hotspots, cache pipelines, frame pacing polish.
- **Phase 9 — Release hardening.** Crash handling, logging, docs, reproducible builds, versioning.
- **Afterwards — Enhancements on request** (Section 9).

## 12. Build and test commands

**No Docker.** Everything runs directly on the host. Do not add Dockerfiles, compose files, devcontainers, or container-based CI steps.

**Matching decompilation toolchain:** Legacy setup details are documented in docs/port/NATIVE_BUILD.md; the native game port does not require those tools.

**Port build (to be created in Phase 0 — keep these names stable once they exist):**
```bash
cmake --preset linux-release        && cmake --build --preset linux-release
cmake --preset linux-asan-ubsan     && cmake --build --preset linux-asan-ubsan
ctest --preset linux-release        # unit tests, no game data needed
# similar presets: windows-release, macos-universal-release, android-arm64-release
```
If a command here does not exist yet, create it as part of the relevant task rather than inventing a different one.

## 13. Testing strategy

- **CI never has game data and never uses Docker.** CI runs natively on GitHub-hosted runners (or equivalent): port builds for every supported platform, unit tests, sanitizer builds, headless smoke tests (SDL dummy video/audio drivers, software or null gfx backend).
- **Unit tests:** GTE operations (bit-exact vectors), GPU primitive rasterization/state decode, SPU ADPCM decode/ADSR, CD sector parsing, struct layout `_Static_assert`s, overlay symbol table.
- **Local-only data tests** (skipped when no disc is configured): deterministic **input replay** files → per-frame hash of VRAM/audio buffer; golden-frame comparisons with tolerance rules documented per backend.
- **Determinism:** same disc + same replay + same build = same hashes on every platform for the vanilla path. Treat divergence as a bug.
- Add a regression test with every bug fix when feasible.

## 14. Code quality and conventions

- Port code style: `.clang-format` (add it) and `.clang-tidy` for `port/` only. **Never run formatters on `src/`.**
- Warnings: `-Wall -Wextra` on `port/`; warnings in `src/` under the port build are tracked in `ANALYSIS.md`, not mass-silenced.
- Naming: runtime layer mirrors PSY-Q function names where the decomp calls them (so no edits to `src/`); internal helpers use a `psx_`/`hm_`/`plat_`/`gfx_` prefix.
- Logging: single logging API with levels; compile out verbose logs in release. No `printf` scattered in hot paths.
- Errors: user-facing failures show a clear message (and on Android, a UI dialog) — no silent exits.
- Small, reviewable commits. Branch per task: `port/<topic>`. Never force-push or rewrite `main`.

### Commit messages

**Always in English**, following [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<optional scope>): <short summary>

<optional body: what and why, wrapped at ~72 cols>

<optional footer: BREAKING CHANGE: ..., Refs: #123>
```

- **Types:** `feat`, `fix`, `chore`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `style`, `revert`.
  - `feat` = new capability · `fix` = bug fix · `chore` = maintenance with no behavior change (deps, tooling, repo housekeeping)
  - `build` = build system/CMake/toolchain · `ci` = workflows · `perf` = measured performance improvement (put numbers in the body)
  - `refactor` = restructure without behavior change (port code only; **never** for decomp `src/`)
- **Scopes** (examples): `gte`, `gpu`, `spu`, `cd`, `pad`, `card`, `gfx`, `platform`, `compat`, `overlay`, `enhance/<name>`, `android`, `macos`, `windows`, `linux`, `matching`, `docs`.
- **Summary line:** imperative mood, lowercase after the colon, no trailing period, ≤ 72 characters.
  Good: `feat(gpu): add semi-transparency blend modes`. Bad: `Added stuff to GPU.`
- Breaking changes: `!` after the type/scope (`feat(platform)!: ...`) plus a `BREAKING CHANGE:` footer.
- One logical change per commit. Do not mix `src/` guard edits with port-layer work; keep them separate so the matching impact is easy to review.
- Do not add `Co-authored-by` or tool-attribution lines unless the owner asks.
- Examples:
  - `fix(compat): use fixed-width types for overlay table entries`
  - `chore: pin SDL3 to a tagged release`
  - `feat(enhance/widescreen): add runtime aspect ratio override`
  - `build(android): enable 16 KB page size alignment`
  - `docs: record renderer backend decision as ADR-003`
- Update docs (`ANALYSIS.md`, `DECISIONS.md`, `PLATFORMS.md`) in the same change that makes them stale.

## 15. How to work (agent workflow)

1. Read this file and the relevant nested `AGENTS.md`/docs. Look at existing code before adding new code.
2. State a short plan; keep changes scoped to the task. One concern per PR.
3. If a choice affects architecture, public interfaces, licensing, or platform support and is not decided here: **ask the owner**; otherwise pick the conservative default, record an ADR, and continue.
4. Implement, build, and test. Run the relevant presets for the touched areas.
5. In your final message report: what changed, commands run with results, what you could **not** verify (missing host tools, no disc, no device), and follow-ups.
6. **Never** claim something works if you did not run it. **Never** fabricate benchmark numbers.

## 16. Definition of done

A change is done when:
- [ ] Port builds on all platforms that currently have presets; sanitizer preset passes for touched areas.
- [ ] Tests added/updated and passing; vanilla parity unaffected (enhancements OFF).
- [ ] No game data, BIOS, SDK files, or secrets committed; `THIRD_PARTY.md` updated for new deps.
- [ ] Docs/ADRs updated; any `HMBTN_PORT` guard added is listed in `ANALYSIS.md`.
- [ ] Performance-sensitive changes include measurements.

## 17. Do-not list

- Do not modify decomp source for style, readability, or refactoring.
- Do not add emulator cores or embed a PS1 BIOS.
- Do not add enhancements, telemetry, or network features unprompted.
- Do not introduce GPL/LGPL/AGPL dependencies or non-pinned dependencies.
- Do not use `dlopen`-style overlays, threads for game logic, or platform `#ifdef`s outside `port/platform/` and `port/packaging/`.
- Do not use or add Docker/containers in any form (build, test, CI, packaging, dev environment).
- Do not require network or game data for the port's unit tests/CI.
- Do not write non-English or non-conventional commit messages.
- Do not suppress sanitizer/UB findings without understanding them.

## Current shared-GPU owner direction

The owner requests shared GPU scene output for native and enhanced resolutions,
not only scaling. graphics.renderer defaults to GPU Auto (0); Software (1)
remains selectable. Keep native VRAM/readback compatibility and safe CPU
fallback while coverage is validated. Synthetic offscreen GPU probes are
technical checks; interactive gameplay testing remains with the owner. Never
present partial GPU coverage as complete or remove the reference prematurely.

## Historical owner follow-up (2026-10-26; superseded)

The all-screen widescreen and aspect/fallback work was completed in ADR-057.
FXAA/MSAA were later implemented in ADR-076. Current renderer coverage and
remaining owner validation are summarized above and in docs/port/PLATFORMS.md.
Profile actual timing and waits before attributing low aggregate CPU/GPU use to
a cause; interactive gameplay testing stays with the owner.
