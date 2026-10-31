# Windows SDL_GPU presentation spike

## Current game renderer status

This file documents synthetic GPU checks and bounded runtime samples, not a
claim of complete game-scene coverage. GPU Auto attempts shared GPU scene output for every captured desktop
and Android frame, while Software remains the reference option. Captured
render-producing GP0 commands are represented on the compute path: polygons,
lines, rectangles, clear-image, load-image, VRAM-to-VRAM copies, and ordered
VRAM feedback. Direct `LoadImage`, `ClearImage`, and `MoveImage` calls are
recorded as ordered GP0 transfers while a frame is captured. A native
`DrawOTag` call without the normal frame wrapper creates its own bypass capture,
covering the loading screen. The scene unit test sweeps every polygon, line and
rectangle opcode encoding plus documented non-rendering GP0 NOP mirrors. The
CPU command and scene tests sweep every opcode alias in the `0x80..0x9F` copy
and `0xA0..0xBF` upload families; the offscreen GPU probe also renders
noncanonical aliases and compares them with CPU output. The
frame replay tests also cover capture streams larger than the previous
65,536-word and 64-group limits. The GPU probe
then renders all 96 primitive encodings with nondegenerate geometry and checks
GPU readback against CPU pixels, including applicable texture depths and
semi-transparency modes. Each case is also checked to ensure the CPU reference
actually changed at least one output pixel. Mask-set and
mask-check behavior is preserved for load and VRAM-copy transfers. VRAM-to-CPU
readback, IRQ requests, NOP/cache commands, and draw-environment state do not
produce scene pixels. The scene builder rejects malformed streams, unsupported
scene operations, and reserved texture depth; the host then attempts a
zero-primitive GPU scanout of the current visible VRAM page. CPU reference
presentation remains the safety path when that scanout or another GPU resource
operation fails. Explicit Software mode and diagnostic frame capture also use
the CPU reference intentionally.
Gameplay parity, broader device coverage, and frame-time budgets remain
unverified beyond the recorded samples; see PLATFORMS.md and ENHANCEMENTS.md.

The SDL-enabled build provides `hmbtn_gpu_renderer_probe`, an offscreen GPU
readback comparison against the CPU reference. It covers the primitive classes,
texture depths and blend modes, clear/load/copy ordering, wrapped and overlapping
VRAM copies, mask-set/check transfers, VRAM texture feedback,
high-density tile bins, scene payloads above the former fixed buffer size,
clipping, filtering, interpolation, multisampling, and expanded presentation.
It starts no game window and requires no disc. The probe includes CPU/GPU
readback comparisons for captured direct VRAM API operations and an unwrapped
`DrawOTag`. On 2026-10-31, the expanded probe and full primitive opcode sweep
passed with SDL GPU's Vulkan and Direct3D 12 drivers. This is technical
coverage of synthetic scenes only; owner gameplay testing remains necessary.
The isolated opcode sweep can also be run with
`hmbtn_gpu_renderer_probe.exe <vulkan|direct3d12> --opcode-sweep`.

The core `gpu.vram` test also checks overlapping `MoveImage` copies, including
wrap at the VRAM edge and mask-bit behavior. These transfers now snapshot their
source before writing, so overlapping areas do not terminate the game.

Android GPU Auto attempts compute rendering for each captured frame. Every
600 attempts, the host logs frame coverage, direct GPU VRAM scanouts, fallback
count, and the last fallback reason. Explicit Software mode remains available.
The coverage counter measures the main game-loop path while GPU Auto is
selected. It excludes frames in explicit Software mode and the separate pause
menu loop; the pause canvas is composed on the CPU, then uploaded and presented
through the GPU. Diagnostic frame capture is counted as a CPU fallback. Empty
display areas and temporarily unavailable swapchains are reported as skipped,
outside the renderable-frame percentage. Therefore a 100% report means every
renderable frame observed by that GPU Auto game-loop counter was submitted
through the GPU scene or direct-VRAM-scanout path; it does not mean all
application drawing or all engine work ran on the GPU.
When the PS1 display is disabled, GPU Auto clears the swapchain directly through
a GPU render pass instead of converting a black frame through the CPU upload
path. CPU scanout remains available if that render pass cannot be created.
The Windows Release and ASan/UBSan game builds each pass all 125 CTest cases;
the Android Release APK builds successfully. This blank-frame path has not yet
been exercised in an interactive game session.
Android's SDL GPU
device uses SPIR-V only and disables optional Vulkan features
unused by this compute renderer. The connected MediaTek G100 tablet reports a
Mali-G57 MC2 Vulkan device with `shaderClipDistance` unavailable; SDL 3.2.28
rejected it before device creation. SDL 3.4.18 exposes the optional-feature
properties needed to accept that device. After updating the SDL Java glue to
3.4.18 as well, the rebuilt APK passed JNI startup and logged `GPU Device
created and claimed successfully (Driver: vulkan)` on 2026-10-31. The app was
stopped immediately after GPU initialization; rendering, frame rate and
gameplay were not tested on the tablet. A prior Adreno 610-class measurement
showed a 42.57 ms compute/blit submit; the all-frame Android policy is an
explicit owner request and has not been profiled on the G100.

The owner later ran the GPU Auto build on the connected G100. Read-only logcat
reported repeated 600/600 GPU scene windows, but only 14-16 presents/s and
62.7-73.2 ms average present time; guest-game time averaged 3.5-4.0 ms and the
host reported 0% idle wait. The owner then installed the 8x8 packed-bin build
and reported continued lag on both Android and Windows. The connected G100's
later logcat sample showed 8 presents/s, 143-150 ms average present time,
503-547 ms maximum, 3.4-3.7 ms guest-game time, all observed presents on GPU,
and 0% idle wait. Thermal status was normal (status 0; skin 42.1 C). The bin
change therefore improves the synthetic desktop workload but has not resolved
the real farm workload on this tablet.

A read-only Windows sample during the owner's run showed the game process at
98-101% of one logical CPU and the GPU 3D engine at 11% in one snapshot. This
points toward CPU-side scene construction or driver submission, but one GPU
sample is not a complete profile. The first diagnostic APK was installed and
logged 8 presents/s at 384x240 with 619-624 primitives, 8.0 average/27 maximum
bin references, and 155-157 KiB scene payload. Per-scene build time averaged
204-219 ms (peaks 360-412 ms); encode averaged 6-9 ms and GPU submit 17-19 ms.
Guest-game time remained about 4.1-4.4 ms. A second diagnostic APK split replay,
VRAM materialization, and scene packing. In its farm run, scene preparation
averaged 48.7-144.0 ms, materialization replay 97.5-142.9 ms, and packing
rounded to 0.0-0.1 ms; scenes contained 41-55 VRAM versions. The sampled
windows presented at 12-14 FPS, with 6.3-40.2 ms average GPU submit. This
identifies the second CPU reference replay and its full-image snapshot lookup
as the dominant measured cost. Thermal status remained normal.

The private materialization replay tracks 16x16 dirty-tile fingerprints and
filters CPU VRAM writes to tiles read by the captured scene. Polygon and
rectangle raster loops skip unneeded horizontal tile spans and reject primitive
bounds that cannot touch selected materialization tiles. Earlier G100 APKs
reduced replay from 97.5-142.9 ms to 20.3-26.6 ms, but the owner still reported
lag and audio crackle. The tile-span-only APK did not improve farm timing.

The 09:31 Release APK adds exact changed-tile tracking for VRAM snapshots. It
avoids repeated 1 MiB comparisons when feedback reads match the baseline, while
hash matches for non-baseline versions are confirmed by exact pixel comparisons
over tiles that differ from the baseline. On the connected G100 at 384x240,
the 09:33 farm sample measured 60-61 game ticks and presents per second.
Materialization replay averaged 1.1-1.7 ms (observed maximum 4.6 ms), down from
20.3-24.1 ms in the prior farm sample; scene preparation averaged 2.0-3.2 ms.
The game audio ring reported zero underruns and drops, with 2,056-2,791 queued
frames. The owner reported that the result felt smoother. This measurement
covers the tested farm scene at native scene size. Android viewport scaling has
since been disabled by ADR-164; performance after that rollback and other scenes
remain unmeasured. One earlier sample reported 598/600 scene attempts through the GPU;
the two fallbacks were missing complete captures during loading or invalidated
frames. Coverage reporting separates those true fallback frames from temporarily
unavailable swapchains, where no renderer can present. Android's audio service
also reported zero hardware mixer underruns in a prior run. The ROM rejects
`simpleperf` events (`cpu-cycles`, `cpu-clock`, and `task-clock`), preventing a
function-level profile. A diagnostic update now reports why replay selection
failed (no complete capture, no matching loading/bypass capture, mismatched
draw/display areas, or a capture newer than the live tick). Windows unit tests
cover no-capture and area-mismatch cases. A no-capture or unrenderable-capture
frame now tries a zero-primitive GPU scene over the currently visible VRAM page
before CPU safety fallback. A GPU scene test verifies recovery after an
unsupported GP0 opcode. The connected
G100's 09:52 farm sample reported 589/589 renderable frames through the GPU
(100%), five direct VRAM scanouts, zero fallbacks, and 11 skipped attempts with
an empty display area during startup. The next two windows each reported
600/600 renderable frames, zero fallbacks, and two then zero direct scanouts.
At 384x240, those steady farm windows held 60-61 presents/s; average game time
was 3.6-3.7 ms, average present time 3.7-4.1 ms, and the audio ring reported
zero underruns or drops. The VRAM scanout also passed a 2x pixel-exact readback
probe on Windows Vulkan and Direct3D 12.

On 2026-10-31, the Windows Release game window completed the authored
`new_game_farm.replay` at `HMBTN_BOOT_TICKS=39000` on SDL_GPU Direct3D 12 and
exited with code 0 after 39,000 VBlanks. With diagnostic CPU frame capture
disabled, its 81 coverage reports recorded 48,599/48,599 renderable frames on
the GPU (100%), zero CPU fallbacks, eight direct VRAM scanouts, and one skipped
attempt for an empty display area during startup. Audio reported zero
underflows and drops. The complete captured output is kept locally under
`build/port/windows-release/gpu-window-full.*.log`; it is build output and is
not committed. This verifies the authored Windows farm replay, not all game
areas or output sizes. Android runtime evidence remains the connected G100
farm at 384x240; other devices and Windows Vulkan gameplay remain unverified.
Android enhanced profiles now stay at native internal resolution (ADR-164).

After the ADR-164 APK was installed, the connected Adreno 610 device
(SM6225, 1080x2340 display) still showed 12-13 presents/s with a 520x240 scene.
Across 15 recent log windows, presentation averaged 63.9-69.8 ms and GPU submit
60.3-66.6 ms; scene construction was 1.4-1.6 ms and encoding 1.7-1.9 ms.
`/sys/class/kgsl/kgsl-3d0/gpubusy` reported about 99.9% GPU utilization in two
samples. Thermal status was normal with a 46.7 C GPU reading and 42.6 C skin
reading. This indicates a GPU workload bottleneck after internal scaling was
disabled. ADR-165 aligns Android GPU texture sampling with its CPU mobile path
by skipping per-primitive bilinear taps while keeping bilinear final
presentation. The new APK's Adreno performance has not yet been measured.

Audit rerun on 2026-10-31: the Windows Release and ASan/UBSan suites each pass
125/125 tests; the Python source-audit fixtures pass 8/8. Clang-Tidy core and
dead-code checks pass on all 77 runtime C files. The excluded Windows
`hmbtn_boot_probe` target also builds after fixing its Win32/PSY-Q `RECT` and
`LoadImage` header collisions and adding its missing crash-handler/version
objects. A focused analyzer pass found one dead frame-deadline store, which was
removed; no other non-generic findings remained. The full offscreen renderer
probe, primitive opcode sweep, 2x VRAM scanout, display-area protection, texture
precision, interpolation seams, and FXAA/MSAA probe all pass on both D3D12 and
Vulkan with pixel comparisons against the CPU reference. The 8,192-rectangle
farm benchmark measured 1.784/1.688 ms at 320x240 and 2.525/2.711 ms at
960x720 on D3D12/Vulkan, respectively, on the local Radeon RX Vega. These are
synthetic desktop measurements and do not establish Android performance.
Android Release also builds, but ADB reported no connected device during this
rerun; the ADR-165 APK has not been measured on Adreno, and rendering remains
unverified on Mali.

Live Adreno 610 baseline on 2026-10-31 (SM6225/`bengal`, 520x240 scene), before
the native-coordinate shader optimization: the latest 12 one-second windows
presented 15 frames each, with 48.0-53.4 ms average presentation and 44.5-49.9
ms in `SDL_SubmitGPUCommandBuffer`; game CPU time was 7.8-9.8 ms. Scene build,
replay, materialization and encode each remained around 1-2 ms. A 600-attempt
coverage report showed 596/596 renderable GPU frames, zero fallback and four
empty-display skips. Audio underflow reached 1,169,240 source frames (about
26.5 seconds at 44.1 kHz), with zero dropped frames and a typical 735-frame
queue; thermal status was normal. The scene shader now bypasses coordinate
division for native-sized output. Its pixel output passes the D3D12 and Vulkan
CPU-reference probes, and a new Android Release APK builds. The APK is not yet
installed or measured on Adreno because the owner is actively using the current
game session; target-device improvement remains unverified.

The probe's `--farm-benchmark` option renders 8,192 fixed 8x8 rectangles at
320x240 and 960x720, with a fresh builder and an explicit GPU fence per frame.
On the local Windows Radeon RX Vega, D3D12 capture/upload/compute/fence time
changed from 1.926 to 1.830 ms at 320x240 and from 3.976 to 2.714 ms at
960x720 after switching to 8x8 packed bins. Vulkan changed from 1.841 to
1.849 ms and from 4.283 to 2.713 ms, respectively. Native-size differences
are within run-level variation; the larger-output results improve by 32-37%.
Scene payload fell 16.1%,
from 1,651,952 to 1,385,920 bytes. The baseline is a detached build of the
previous 16x16 fixed-bin revision, using this exact rectangle workload. Full
CPU-reference comparisons and the opcode sweep pass on both D3D12 and Vulkan.
The synthetic desktop result does not prove Android farm performance is fixed.

This optional local diagnostic validates GPU presentation without game data.
It is not a game screen or a primitive rasterizer. Default unit builds neither
download SDL nor require a GPU. The SDL release and SHA-256 are pinned in the
port CMake project; see THIRD_PARTY.md.

From the repository root, use a PowerShell session with the local tools:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass
& ./port/packaging/windows/setup-toolchain.ps1
cmake -S port -B build/port/windows-gpu-spike -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DHMBTN_BUILD_GPU_SPIKE=ON
cmake --build build/port/windows-gpu-spike --target hmbtn_gpu_spike -j 4
& ./build/port/windows-gpu-spike/hmbtn_gpu_spike.exe
```

The program uploads a synthetic RGBA gradient/checker pattern, performs a
nearest GPU blit into a second texture, verifies byte-exact CPU readback, then
presents eight frames in a window named HMBTN native graphics validation. It
limits frames in flight to two and closes automatically. No assets are read,
no screenshot is saved, and no config/save files are written.

Validated on 2026-10-25 with SDL3 3.2.28, Windows x86-64, Radeon RX Vega and
driver 31.0.21925.1001. Output reported the Vulkan backend and successful upload,
blit, readback and presentation. The offscreen scene probe separately passed on
Vulkan and Direct3D 12 on 2026-10-31. Metal, other devices, suspend/resume and
game-frame performance remain untested. This synthetic diagnostic stays separate
from the game-window target. See ADR-018 for scope and backend status.

The optional HMBTN_BUILD_NATIVE_WINDOW=ON target hmbtn_native links the compiled
game, reference DrawOTag rasterizer and SDL_GPU scanout. It accepts the original
USA BIN argument described in DISC_TESTING.md. Local Vulkan captures now show
the title/menu and initial New Game dialogue. A 1200-VBlank ASan/UBSan window
run also exercised the SDL audio callback. No full-game, golden-frame parity or
frame-time/latency budget is established. ASan and reference CPU rasterization
are expected to affect pacing; observed audio underflows remain an open issue.

Build the Windows release game window (also runs the unit suite):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/run.ps1 -DiscPath 'D:/Games/Harvest Moon USA.bin'
```

The launcher builds the release window if missing and clears diagnostic input
replays/tick limits for the child game, restoring the caller's environment on
exit. It does not copy disc data into the repository. Keyboard: arrows move,
Enter is Start, X is Cross/confirm, Z is Square, D is Circle, W is Triangle,
A/S are L1/R1, Q/E are L2/R2 and Backspace is Select. A 31000-tick replay
validates free movement in the house (WINDOWS_PLAYTEST.md). Physical keyboard
input remains unverified by automation. Native file-backed card containers are
implemented; the complete in-game sleep/save/load lifecycle remains owner-unverified.

The headless boot probe also accepts HMBTN_CAPTURE_FRAME to export its final
15-bit scanout to BMP at HMBTN_BOOT_TICKS, and HMBTN_TRACE_GAME_STATE to print
area/player state every 600 ticks. These diagnostics require the owner's disc
for game frames. Store captures only in ignored build output. A 6000-tick
Vulkan/ASan window capture shows farm terrain/player/HUD; a release headless
capture shows the original grandparent dialogue. Longer headless replays now
complete the opening and house movement; broader gameplay remains under test.

### Android native-scene tile packet cache (2026-10-31)

The scene compute shader now stages each eligible 8x8 tile's primitive IDs and
up to 16 complete primitive records in workgroup-shared memory. This applies
only when scene output matches logical size and is aligned to the tile grid;
scaled/partial groups and denser tiles retain the global-buffer path. Windows
Release and ASan/UBSan builds pass 125/125 tests, and the full offscreen scene
readback/probe suites pass on both D3D12 and Vulkan. The Android Release APK
builds and was installed on the owner's Helio G100 tablet. One farm log sample
at 384x240 reported 3.57 ms average game time, 3.99 ms average presentation,
and 1.3 ms average GPU submit; the owner reports smooth play. This is a single
device sample, not a controlled before/after benchmark. Adreno results from a
later build are recorded below; other Mali devices remain unverified.

### Empty-bin workgroup synchronization and cache diagnostics (2026-10-31)

The shader no longer enables packet caching for empty bins. Bins with zero or
more than 16 primitives now take only the initial workgroup barrier; cached
bins retain the ID and packet-data barriers. Android perf output reports
`cache`, `empty`, and `uncached` tile counts so a device capture can show how
often each shader path runs. In the log, `uncached` means non-empty bins that
do not qualify for the cache, including dense bins and scaled/misaligned scenes.

Windows Release and ASan/UBSan CTest each pass 125/125, and the D3D12 and
Vulkan offscreen scene readback probes pass. The Android arm64 Release build
succeeds with NDK 30.0.16248370, Gradle 9.8.2, and AGP 9.5.0. Its APK is at
`dist/VanillaBTN-android-v0.9.0.apk`. It was installed on the connected Adreno
610 after the G100 session; the preceding cache-enabled APK was reported smooth
on G100. The barrier change has not been runtime-measured on G100. Its Adreno
farm capture follows.

### Adreno 610 farm capture after tile caching (2026-10-31)

The newly built APK with the workgroup cache and cache diagnostics was
installed on the connected Redmi device (`SM6225`, `Adreno610v2`, Vulkan).
The owner reached the farm. Across 20 consecutive one-second performance
windows at 520x240, the scene contained 665-756 primitives in 1,950 bins,
averaging 6.5-7.1 references per bin with a maximum of 26. The shader reported
1,936 cache-eligible bins, zero empty bins, and 14 uncached bins. `gpu` matched
`presents` in each window.

Average GPU submit ranged from 36.4 to 43.5 ms and presentation from 40.6 to
48.3 ms; game time was 6.6-10.8 ms, scene construction 5.2-7.8 ms, and CPU VRAM
materialization 3.8-5.3 ms. The run stayed around 17-21 presents/s. Cumulative
audio underflow reached 2,680,694 frames with zero dropped frames and a
735-frame queue. The GPU remains too slow for 60 fps. This sample is not a
controlled before/after comparison because the earlier farm capture did not
record primitive counts. KGSL counter reads reset/increased inconsistently and
GPU clock sysfs access was denied, so this capture does not establish GPU
utilization or clock frequency.


### Current verification (2026-11-01)

The Windows -Game Release build passed 125/125 CTest cases; the Windows
ASan/UBSan preset passed 108/108. The offscreen GPU renderer probe and
primitive opcode sweep passed with Vulkan and Direct3D 12, including expanded
scene readback, whole-canvas presentation, and native/fallback aspect checks.
Android Release built with NDK 30.0.16248370, Gradle 9.8.2, and AGP 9.5.0.
No game window or interactive gameplay session was started. Android device
performance after the current shader changes remains unmeasured.
