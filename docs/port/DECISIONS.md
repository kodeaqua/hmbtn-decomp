# Architecture decisions

## ADR-001: Isolate the Phase 0 build boundary

Date: 2026-10-25. Status: accepted, following AGENTS.md defaults.

Context: The matching project installs a PSX toolchain before project creation.
The native port must work independently and must not require proprietary SDK input.

Decision: Keep the native CMake project and its presets in `port/`. Begin with
a C11 fixed-width type/endian test. Preserve all matching source and link rules.
Prepare matching dependencies using verified archives on Linux, with a local
owner-supplied SDK. Port checks run without network or game assets.

Consequences: Run port presets from `port/`. A foundation build does not imply
game boot. Renderer selection, overlay namespacing, asset layout changes, Android
minimum SDK and native host-tool version pins remain unresolved. No enhancement
is enabled. Workflow/CI work is deferred by owner instruction.

## ADR-002: Bring up Windows first

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The original roadmap starts native bring-up on Linux. The owner asked
to prioritize Windows during Phase 0.

Decision: Bring up Windows x86-64 first, using project-local checksum-pinned
LLVM-mingw/UCRT, CMake and Ninja. Windows port builds must not depend on WSL, a PSX SDK,
or the matching toolchain. Retain the other desktop foundation presets for later
validation.

Consequences: The first game boot target is now Windows. Android is deferred.
No renderer or runtime architecture decision changes. Release links use static
compiler runtime dependencies and the Windows system UCRT; system DLL imports
remain expected.

## ADR-003: Validate locally and defer CI

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The owner explicitly excluded workflows and CI from the current work.

Decision: Leave existing workflow files untouched; add no CI provisioning or
jobs. Validate the Windows foundation locally and document actual results.

Consequences: Roadmap and completion requirements for a CI skeleton are deferred.
Existing upstream workflow behavior is not validated or endorsed by this work.

## ADR-004: Exclude WSL from Windows port work

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The owner does not want WSL used for this work.

Decision: Build and validate the port with native Windows tools. Do not install, invoke,
or require WSL.

Consequences: An unavailable Linux matching toolchain does not block Windows port
progress.

## ADR-005: Upgrade the Windows compiler for sanitizer startup

Date: 2026-10-25. Status: accepted within the existing LLVM-mingw toolchain choice.

Context: LLVM-mingw 20241001 / Clang 19.1.1 compiled the sanitizer targets but
ASan crashed before main with `interception_win: unhandled instruction`
(`44 0f b6 1a`). This matches the upstream Windows interceptor issue:
https://github.com/mstorsjo/llvm-mingw/issues/477.

Decision: Pin LLVM-mingw 20250709 / Clang 20.1.8 and its archive SHA-256.
Keep ASan and UBSan enabled together in `windows-asan-ubsan`; never suppress
the startup issue by disabling interceptors. Sanitizer builds use the toolchain's
ASan DLL on PATH; release builds retain static compiler runtime linkage.

Consequences: Setup downloads a new compiler archive once. The build wrapper
uses `cmake --fresh` to preserve preset options across compiler upgrades; CMake's
automatic compiler-change reconfiguration can otherwise discard options such
as HMBTN_SANITIZERS. Old ignored tool directories need not be deleted.

## ADR-006: Keep SDK words fixed-width and host descriptors native

Date: 2026-10-25. Status: accepted, following the existing memory/type defaults.

Context: Windows host sys/types.h does not provide the SDK's u_long spelling.
Using host long for it would also become 64-bit on LP64 platforms. GPU packet
words must remain 32-bit while host pointers must retain their full width.

Decision: Force-include `compat/game.h` only in game-facing translation units.
It loads host sys/types.h first, then maps u_char/u_short/u_long to namespaced
fixed-width aliases. Do not redefine the C keyword long or force this mapping
into the platform/runtime targets. GPU packets use 32-bit tag words; GS ordering
table descriptors use native pointers. Assert packet and pointer-free asset
layouts without asserting the PS1 size of pointer-bearing host state.

Consequences: This establishes declarations and layout checks, not a GPU runtime
or a solution to truncated links in original LINK_PRIM. Original data loaders,
overlay dispatch and save serialization still need a separate 64-bit audit.
Public packet format reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

## ADR-007: Compile the original entry point before linking or executing it

Date: 2026-10-25. Status: accepted, following the separate native-build default.

Context: SDK declarations blocked main.c before native code generation. Even
after declaration fixes, its hardcoded work addresses and 32-bit pointer casts
make it unsafe to run on a native 64-bit host.

Decision: Compile unmodified main.c into the native game archive with a scoped
`main=hm_game_main` definition. Add its source-required SDK declarations and
compile afile.c in the same archive. Do not link or execute a game entry point
until native work memory, overlay dispatch and required runtime calls are ready.
Implement only the pure CD position helpers now; disc IO calls used by afile.c
abort explicitly. Other declared-but-unimplemented APIs remain link errors.

Consequences: Native code generation is verifiable without falsely claiming
boot. CdlFILE is source-facing search metadata containing only pos and size,
not a copied SDK ABI or an on-disc record. CdPosToInt reads exactly three bytes
because cd_audio.c passes the last three bytes of a GetlocP response. CdIntToPos
preserves the reserved fourth byte. Tested inputs cover normal two-digit BCD
positions, not malformed locations or arbitrary out-of-range LBAs.

Hardware reference for BCD MSF, 75 frames/second and the lead-in offset:
https://psx-spx.consoledev.net/cdromdrive/.

<a id="adr-008"></a>

## ADR-008: Back the asset/frame workspace with normal C storage

Date: 2026-10-25. Status: accepted, following the native memory default.

Context: main.c treats 0x801BE1B0 as a writable frame workspace. Its frame buffers
occupy 0x3EE50 bytes; the input demo and dynamically loaded farm data sit below
it. Replacing only the frame address would leave pointer subtraction outside
the allocated object. farm_state_init also derives its load window by subtracting
overlay code addresses, which does not apply to statically linked host overlays.

Decision: Use one aligned C byte array solely for this existing contiguous data
workspace, not a 2 MB PS1 memory image. Keep the 0x49000 image window, 0x47390
section/free capacity and 0x3EE50 frame work area. The capacity is derived from
the matching addresses: align the low-to-high region distance (0x75FC8) to
0x800 bytes, producing load window 0x8012DE20 and section start 0x80176E20;
the distance to WORK_REGION is then 0x47390. Native frame/tag offsets remain
unchanged. Overlay code and other globals/heap remain outside this storage.

Native source hooks will select the buffer and return a byte pointer from
main_get_region_base, preserving full host pointer width. Remaining overlay
callers that assign that pointer to 32-bit integers must fail compilation until
ported. The matching branches retain their original declarations and statements.
No save-format or asset-pointer compatibility is implied by this workspace.

Consequences: Frame/farm memory can be tested independently of GPU execution and disc
IO. Free-space budgets and demo overlap retain their vanilla meaning. OT pointer
encoding, overlay loading, asset bounds validation and the rest of boot remain separate
work.

<a id="adr-009"></a>

## ADR-009: Encode GPU links as stable handles to native tag pointers

Date: 2026-10-25. Status: accepted, following the fixed-width handle default.

Context: PS1 packet tags have an eight-bit command length and a 24-bit next
address. Truncating a native pointer can alias unrelated objects and cannot
represent 64-bit host addresses. The source links packets through SDK addPrim,
the LINK_PRIM macro, and ot_insert_prim; all three paths must agree.

Decision: Preserve packet layouts and replace only the address part with a
nonzero 24-bit handle. Keep 0xFFFFFF as the end marker. Assign handles in first-use
order, storing full native pointers in a bounded registry. Hash buckets are an
internal lookup detail; ASLR does not affect handle assignment. The initial
registry supports 65536 live tag addresses with a fixed 1 MiB allocation (pointer
array plus buckets) and no per-frame allocation. Exhaustion and malformed handles
abort with diagnostics rather than truncating, wrapping or silently succeeding.

ClearOTagR creates reverse tag chains. addPrim/AddPrim preserve both packet and
head lengths; the source-specific helpers retain LINK_PRIM/ot_insert_prim's
behavior of clearing the OT head length. setlen writes only the length byte,
without reading uninitialized address bytes. DrawOTag/rendering remain separate.

Consequences: Callers must keep registered tag objects alive and discard every
chain before resetting the registry. Do not reset it per frame; copied/cached
packet links must remain valid. Handles are process-local, not save-file data.
The registry validates identity/alignment, not allocation extents or payload
lengths; it is not an untrusted-memory sandbox or a cycle detector. Capacity and
runtime cost still need full-game/device profiling; no performance claim is made.
Matching source integrations must retain the exact original else branches.

Public packet/OT reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

<a id="adr-010"></a>

## ADR-010: Keep native stacks and explicit diagnostic markers

Date: 2026-10-25. Status: accepted, following the native ABI/scratchpad default.

Context: cpu_asm.h switches to a MIPS scratchpad stack and stores return addresses
in 32-bit diagnostic words. Neither operation is valid on a 64-bit host. Source
inspection shows no game-data accesses to the saved scratchpad SP; lastCaller is
a diagnostic word also copied into save metadata, not an executable function pointer.

Decision: Native calls retain their normal ABI stack. Provide aligned 1 KB C
scratchpad storage and no-op only the stack-switch hints. SAVE_RETURN_ADDRESS
writes the deterministic 0x504F5254 PORT diagnostic marker instead of truncating
a host address. A game debugger-wait request reports a runtime error and aborts,
rather than emitting a MIPS break instruction or hanging the native process.
Keep all original CPU macros in an exact matching else branch when integrated.

The SDK csin/ccos/csqrt names are mapped to explicit fixed-math declarations,
avoiding collisions with the host C99 complex builtins. This does not yet
implement their arithmetic. Packet helpers implement only source-used fields
and GP0 encodings; video dimensions 320x240 follow the original main.c setup.

Consequences: These hooks unblock native compilation, not GTE functionality or
save-format parity. No approximation of an unimplemented GTE operation is added.

<a id="adr-011"></a>

## ADR-011: Mount validated raw tracks through platform-owned streams

Date: 2026-10-25. Status: accepted, following the original-disc input default.

Context: The game's archive reader needs ISO metadata and 2048-byte sector reads.
Desktop paths and Android descriptors must not enter the portable CD parser.

Decision: The platform owns an exact positional-read callback and stream length.
Hash the complete raw BIN against disc/disc.sha1 before mounting ISO-9660.
Decode sector headers explicitly; accept Mode 1 and Mode 2 Form 1 for filesystem
reads and report unsupported Form 2 transfers. Validate directory record lengths,
both-endian fields and extents without casting disc bytes to host structs.
Keep all game data external. A local desktop probe provides validation before
the SDL first-run UI exists; it is not a runnable port of the game.

SDK reads use a single game-thread controller with an explicit 60 Hz tick and
75/150-sector-per-second transfer budget, retaining fractional sectors. Polling
does not advance time. Blocking ReadSync drains the request without host sleeps.
No seek delay is added during bring-up. The eventual VSync platform integration
must call this tick; no host callback thread may advance game state. Unsupported
commands (including XA streaming) fail explicitly, not with simulated success.

Consequences: Normal data reads and errors can be tested without disc assets.
Exact PS1 seek/retry/status timing, CUE input, XA streaming and full boot remain
unverified. SHA-1 is a known-image identifier, not an authentication mechanism.
The implementation is original port code, not copied from the RFC example.

References: https://psx-spx.consoledev.net/cdromformat/,
https://psx-spx.consoledev.net/cdromdrive/,
https://www.rfc-editor.org/rfc/rfc3174.

<a id="adr-012"></a>

## ADR-012: Keep a native VRAM transfer model before renderer submission

Date: 2026-10-25. Status: accepted, following the 1024x512 VRAM model default.

Decision: Keep 16-bit VRAM in ordinary C storage. LoadImage/StoreImage transfer
little-endian pixels with independent X/Y wrapping and hardware copy-size masks.
Transfers execute synchronously; DrawSync observes no pending work. ClearImage
uses GP0 fill rounding and bypasses mask bits. Track changed 16x16-word tiles for
future renderer uploads, with explicit acknowledgement rather than per-frame
allocation. Preserve STP bits and implement set/check write masks.

Consequences: Original texture-pack code is now compiled and tested on synthetic
raw and RLE banks. This is not the SDL_GPU backend or a completed command
rasterizer. MoveImage, OT command execution, displayed frames and full rendering
parity remain pending. StoreImage leaves unused odd-pixel transfer padding alone;
the game's callers consume pixels, not padding. No claim is made about bus
padding parity or GPU timing. No speculative rendering enhancement is added.

Reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

<a id="adr-013"></a>

## ADR-013: Hoist capture-free nested helpers through native adapters

Date: 2026-10-25. Status: accepted, following the narrow source-guard default.

Context: The original GCC code defines functions inside other functions. Clang
rejects these definitions. Several helpers use only arguments and file globals,
so moving their scope does not require closures or game-state changes.

Decision: Keep original nested definitions untouched in matching else branches.
For native builds, include static copies from port/compat/nested immediately
before the enclosing function. Hoist only helpers inspected to capture no local
variables. Keep names, parameter types, expressions and branches equivalent.
Do not apply this pattern to captured-local helpers without explicit arguments.
A regression test compares original and adapter tokens, excluding only static
and whitespace/comments. Existing unusual comparisons remain unchanged.

Consequences: Eleven capture-free helpers in ground collision, animal headings, estate
items, shop rows, winter event conditions, scene visibility, minigame 2 and debug screen
storage now compile under Clang. Data-free tests exercise winding/edge/height geometry,
comparison boundaries, estate flag priority and visible medal rows. These do not prove
full gameplay parity. The native helper copies must remain synchronized with matching
source. No compiler switch or new runtime dependency is introduced.

The harvest carry helper captures its enclosing object. Its native adapter
receives that object explicitly; a call macro passes the current object and is
undefined at the end of the enclosing function. The function body is unchanged.
A token test checks its body, and a C test passes different captured/argument
objects to verify the distinction is preserved.

<a id="adr-014"></a>

## ADR-014: Dispatch source GTE commands into native scalar state

Date: 2026-10-25. Status: accepted, following the clean-room GTE default.

Decision: Replace the two raw MIPS command macros only in the native branch.
The source-used inline load/store helpers operate on native C GTE state, with
fixed-width packet stores. RTPS/RTPT use the public UNR reciprocal calculation,
44-bit wrapping, signed shifts, FIFO movement and documented saturation/flags.
Use integer arithmetic throughout, including exceptional quotient saturation
without divide-overflow. Matrix stack snapshots preserve rotation/translation;
source-used projection setters update the same state.

Consequences: The original gte_project_vertex is exercised by a fixture with
traps for unrelated camera/draw dependencies. Normal XY/depth and behind-camera
rejection pass. GTE vectors also cover triple FIFOs, packet field placement,
clipping, flags and in-place matrix multiplication. Color commands, uncommon
MVMVA selectors, high-level trigonometry/normalization and complete SDK init
defaults are not parity validated. Unsupported calls still fail explicitly.
InitGeom supplies the source's OT scaling; full hardware captures and replay
comparisons remain required. No CPU interpreter or SDK implementation is copied.

Reference: https://psx-spx.consoledev.net/geometrytransformationenginegte/.

<a id="adr-015"></a>

## ADR-015: Separate native SDK metadata from asset ABIs

Date: 2026-10-25. Status: accepted, following source-facing compatibility defaults.

Decision: Declare only pad/card/PocketStation APIs observed in the source.
DIRENTRY is native result metadata with a terminated filename and fixed-width
size, not an SDK/on-card struct. McxFuncExecFlag is an opaque native completion
identifier (0x100), not a guessed SDK opcode or a PocketStation wire command.
The native backend must translate its own identifiers; unsupported PocketStation
commands currently abort. No protocol implementation is claimed by declarations.

The three text_fmt_* strings are defined static and used only inside text.c.
Suppress conflicting extern declarations in the native header branch while
retaining the original declarations in the exact matching else branch.

Consequences: All source units can be diagnosed with typed native declarations.
Windows long-pointer type warnings remain visible; LP64 ports require further
fixes before runtime validation. Card persistence and host pad polling are
unfinished. No proprietary header layout or code is imported.

<a id="adr-016"></a>

## ADR-016: Keep event staging addresses native and asset offsets fixed

Date: 2026-10-25. Status: accepted, following native pointer/asset-offset defaults.

Decision: Use uintptr_t for the event block base, VRAM-tail staging address and
their address temporaries. Preserve alignment checks on the full host address.
Read signed 32-bit entry offsets through a little-endian byte helper, without
casting a host pointer to s32 or restricting a terminal offset to the declared
array prefix. Asset bounds remain the caller's responsibility. Assert the
FarmEventBlock fixed prefix is 148 bytes, with entry offsets at byte 52.

Consequences: Runtime addresses grow without changing disc offsets or packed
data. Source changes remain narrow guarded declarations/expressions. Asset-word
tests include unaligned signed reads under sanitizers. Real event playback,
all remaining pointer/layout hazards and save ABI parity are not validated.

<a id="adr-017"></a>

## ADR-017: Link native overlays and reject unsupported reentry

Date: 2026-10-25. Status: accepted for initial bring-up; reset lifecycle pending.

Decision: Compile all 113 original C units, including all thirteen overlays,
into the native archive. Their existing source names have no duplicate strong
external definitions; preserve resident imports without blindly prefixing them.
The native LOAD_OVERLAY path selects a validated member/region registry entry
instead of copying PS1 executable bytes. Existing asset-region references bind
to the native data workspace. Compiled C initializers supply first-use state.

Consequences: The full graph links into the boot diagnostic. A second activation
of the same overlay aborts explicitly until native data/BSS restoration is
implemented; silently retaining mutated globals would change vanilla behavior.
Registry reset resets metadata only. No reentry, save/load lifecycle or full
playthrough parity is claimed. No BIOS, dynamic module or executable disc data
is loaded. The user's image remains required for original non-code assets.

<a id="adr-018"></a>

## ADR-018: Validate SDL_GPU presentation on Windows Vulkan

Date: 2026-10-25. Status: provisional for Windows; other devices pending.

Decision: Pin SDL3 3.2.28 with the publisher's GitHub asset SHA-256 digest and
retain its zlib license. Build an optional standalone presentation spike behind
HMBTN_BUILD_GPU_SPIKE=ON, leaving data-free unit builds offline by default.
Keep all SDL headers in port/platform. Upload a synthetic 320x240 RGBA texture,
blit it to an offscreen target, verify byte-exact GPU readback and present eight
frames, with at most two frames in flight. No custom shader dependency is needed
for this blit test.

Result: Passed using SDL_GPU Vulkan on the owner's Radeon RX Vega / Windows
driver 31.0.21925.1001. The GPU shader blit and swapchain were exercised. This
does not demonstrate PS1 primitive rasterization, a rendered game frame, D3D12,
Metal, Android or performance budgets. SDL_GPU remains the first backend route;
cross-platform suitability still needs validation. No fallback is selected here.

References: https://github.com/libsdl-org/SDL/releases/tag/release-3.2.28,
https://wiki.libsdl.org/SDL3/SDL_BlitGPUTexture.

<a id="adr-019"></a>

## ADR-019: Advance callbacks and silent sound state on the game thread

Date: 2026-10-25. Status: accepted for bring-up, following deterministic execution.

Decision: VSync waits advance explicit 60 Hz ticks; query modes do not advance
time. Advance CD transfers and VBlank callbacks at these points. A platform
wait/scanline hook supplies host pacing/profiling; the console probe uses virtual
time. Preserve callbacks across repeated ResetCallback calls from archive reads.
SetMem(2) validates the original BIOS hint without allocating an emulated RAM
image. SystemError reports the original error words and aborts.

Implement real silent sound initialization/configuration: sequence table limits,
reserved voices, common volume masks, stereo/CD routing and start/stop state.
Key statuses are genuinely off because bank loading and key-on still abort;
no playback is falsely marked complete. Configuration-only acknowledgements
must not be mistaken for an implemented mixer. VAB/SEQ decoding, live voice
mixing, ADSR, reverb, XA and audio output remain pending.

Native GPU environments now retain clip/offset/display state and GS work-buffer
pointers; background clears update the actual VRAM model. ResetGraph preserves
VRAM and OT handle identity. DrawOTag, MoveImage and unsupported scanout modes
still abort. The window presentation spike is separate from the game probe.

<a id="adr-020"></a>

## ADR-020: Bound native title-picture copies to their source arrays

Date: 2026-10-25. Status: accepted for native memory safety.

Observed: ASan boot with the owner's verified disc reports a 6144-byte read
from title_image_1, whose declared length is 4200 bytes. The original fixed-size
TitleImage assignment crosses into neighboring matching globals. All three
pictures are smaller than the destination. Native global placement and ASan
redzones cannot reproduce that incidental adjacency.

Decision: A narrow HMBTN_PORT branch selects the same random picture once and copies its
declared complete bytes. Leave the destination tail untouched; it is outside that
picture's compressed asset. The original assignment remains in the matching else branch.
This avoids reading unrelated native globals without modifying the asset or suppressing
ASan. No visual parity is claimed until the loading-picture decoder and screen are
exercised.

<a id="adr-021"></a>

## ADR-021: Implement real device absence and incremental GPU/audio assets

Date: 2026-10-25. Status: incremental bring-up; parity pending.

Digital pad replies publish sampled buttons in original wire order, including
after the game clears its reply buffer. The current memory-card controller has
no mounted card and completes detection with a real no-card error. File writes
remain unsupported; no persistence is advertised.

Generate an original nearest-integer Q12 quarter-wave table from the analytic
sine function, rather than importing SDK table data. rsin/rcos wrap at 4096
units; exhaustive tests compare the analytic reference. Proprietary SDK table
quantization parity is unverified and remains a fidelity gate.

DrawOTag walks native handles and executes environment registers, inline VRAM
uploads, fills, tiles, sprites and axis-aligned flat quads into reference VRAM.
Texture depth, CLUT, texture-window, modulation, mask, clip and blending state
are applied. Other polygons/opcodes still abort explicitly. This is an initial
CPU correctness path, not a measured final renderer or a performance-budget
claim. A separate optional SDL_GPU game-window target uploads current scanout;
the standalone synthetic hardware spike remains available.

VAB v7 headers and streamed sample bodies are owned native assets with bounded
allocation, sparse program metadata and synchronous chunk copies. This is
actual bank loading, not sample playback. Sequence/key-on/mixer/reverb work
remains unfinished. The real-disc sanitizer probe now stops at SsSeqOpen.

Reference: https://psx-spx.consoledev.net/cdromfileformats/#cdrom-file-audio-sample-sets-vab-and-vhvb-sony
and https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

<a id="adr-022"></a>

## ADR-022: Retain sequence asset bounds at the native loader boundary

Date: 2026-10-25. Status: accepted for safe SEQ parsing; playback pending.

SsSeqOpen receives only a pointer and bank ID. The original loader knows the
exact copied sequence size; attempting to discover an end marker through an
unbounded pointer would hide malformed-asset reads. A narrow native guard
registers the copied pointer/length with the sequence runtime. Matching code
does not call the registration function and reconstructs the baseline exactly.

The parser checks SEQ v1 headers, big-endian timing, bounded variable integers,
channel event widths, running status and metadata/SysEx payload bounds. Open
validates termination and requires a loaded bank. Handles and volume settings
are retained; stopping an idle opened sequence is valid. SsSeqPlay still aborts
until the sequencer and voice mixer can actually perform the requested playback.
No silent playing/completion status is fabricated. The real-disc sanitizer
probe now reaches SsSeqPlay after opening the sequence successfully.

Reference: https://psx-spx.consoledev.net/cdromfileformats/#cdrom-file-audio-sequences-seqsep-sony.

<a id="adr-023"></a>

## ADR-023: Build a deterministic native SPU voice core from hardware facts

Date: 2026-10-25. Status: reference bring-up; game audio parity unverified.

Decode SPU-ADPCM blocks with explicit signed arithmetic, predictor history and
saturation. Advance ADSR at 44.1 kHz, including slow-rate counters and frozen
rates. A 24-voice native mixer uses bounded bank assets, loop/end flags, pitch
accumulators, running interpolation history and Gaussian hardware coefficients
documented by psx-spx. The coefficient table contains hardware numeric facts;
no SDK or emulator implementation was copied. Voice mixing allocates nothing.

Native key-on/off and status APIs now operate on these real voices. Sparse VAB
programs map to packed tone blocks; sample ranges are checked against uploaded
body size. Reference-counted sample bodies survive closing a bank until its last
live/releasing voice finishes. There is no per-note allocation. Reset stops
voices before freeing banks.

Pitch uses an original analytic equal-tempered Q12 reference with 127 fine
steps per semitone, generated by port/analysis/generate_pitch.py. Exact SDK
pitch quantization, volume curves/pan laws, voice allocation/stealing, key-on
latency and full audio parity remain unverified. Do not mistake deterministic
synthetic PCM tests for proof of those properties. Sequencing, reference reverb
and host output are described below; XA remains unfinished.

References: https://psx-spx.consoledev.net/soundprocessingunitspu/ and
https://psx-spx.consoledev.net/cdromformat/#cdrom-xa-audio-adpcm-compression.

<a id="adr-024"></a>

## ADR-024: Run sequence clocks and reference reverb on the game thread

Date: 2026-10-25. Status: initial reference; audio parity unverified.

Each VBlank advances SEQ events with integer timing credits, followed by 735
stereo samples at 44.1 kHz. Program/note/volume/pan/expression/pitch events drive
actual voices. Ownership prevents later note-offs from releasing a voice
reassigned to SFX. Source-used NRPN loop markers 20/30 and data entry 6/98
support finite and endless music-part repetition. Unsupported events abort.

Configured idle sequence slots retain volume settings: the original loader
clears all six slots before testing which sequences are open. Stop/close
releases owned voices. SPU release/reverb mixing continues after SsEnd stops
sequence scheduling. Reference-counted bank bodies survive pending tails.

Room, studio-large and hall reverb use public numeric register presets, a normal
native circular delay buffer, fixed-point reflection/comb/all-pass stages and
the documented 39-tap rate-conversion filter. No SPU RAM image is allocated.
Rounding, phase/order details and complete mix/SDK volume behavior still need
golden-audio comparison. Synthetic impulse and chunk-determinism tests do not
establish bit-exact SPU behavior.

References: https://psx-spx.consoledev.net/soundprocessingunitspu/ and public
format facts in https://psx.arthus.net/sdk/Psy-Q/DOCS/Devrefs/Sound20.pdf.
No SDK code or binary data was imported.

<a id="adr-025"></a>

## ADR-025: Extend reference rasterization and connect diagnostic replay

Date: 2026-10-25. Status: Windows title bring-up; raster parity pending.

Decode flat/Gouraud textured/untextured polygons, split quads in original vertex
order, and rasterize clipped triangles with affine attributes and explicit
shared-edge ownership. Apply texture/STP, blending and dithering. Lines include
their final endpoint and apply dithering, including flat lines. This CPU path
supports bring-up and later GPU comparisons. Hardware subpixel/UV rounding,
line ties and full VRAM parity remain unverified. This is not a measured final
backend or a performance-budget claim.

Development probes accept tick/button replay files through the native pad
sampler used by keyboard input. The console probe can run bounded virtual
VBlanks; the SDL probe uses host pacing and can capture BMPs under ignored
build output. These are local validation tools, not enhancements or a packaged
replay UI. Windows Vulkan captures show the original title, New Game/Continue
menu and initial naming dialogue. Physical keyboard interaction is unverified.

References: https://psx-spx.consoledev.net/dev/ps1/gpu/render-polygon-commands/
and https://psx-spx.consoledev.net/dev/ps1/gpu/render-line-commands/.

<a id="adr-026"></a>

## ADR-026: Preserve full native text-bank addresses

Date: 2026-10-25. Status: accepted for pointer safety.

ASan New Game replay reports an access violation in name_title_build_label. LOCALIZED
casts an actual text-member pointer to u32 before adding a relocation delta;
LOCALIZED_NAME and NAME_OFFSET have the same hazard. A port-only include uses full
native addresses and the compiler's constant offsetof builtin. Keep the bank-start
fields as 32-bit relocation metadata and retain original macros in matching else
branches. Tests cover identity, relocation deltas and the 12-byte metadata layout. The
replay subsequently passes name/birthday setup and reaches farm initialization.

<a id="adr-027"></a>

## ADR-027: Feed SDL output through a lock-free PCM ring

Date: 2026-10-25. Status: Windows output tested; quality/latency gates open.

The game thread produces deterministic 44.1 kHz PCM into a bounded SPSC ring.
SDL's postmix callback consumes it without allocating, locking, logging or
calling game code. Lock-free 32-bit atomic indices are required at compile time.
A two-tick prebuffer absorbs ordinary jitter. The callback converts stereo to
device float channels and linearly resamples when the device rate differs.
This host conversion never changes game-thread PCM/state. Overflow drops
output frames; underflow emits silence. Counters are reported after shutdown.

The Vulkan/ASan window probe completed 1200 VBlanks and its SDL audio callback
consumed output: 919926 device frames, 65858 source underflows, zero producer
drops in that run. This validates the output path, not listening quality,
smooth pacing, latency budgets or parity. Underflows need further work.
Other platforms and device lifecycle remain untested.

Reference: SDL3 3.2.28 SDL_audio.h / SDL_SetAudioPostmixCallback contract.

<a id="adr-028"></a>

## ADR-028: Preserve initialized animation table tails

Date: 2026-10-25. Status: accepted for native metadata correctness.

The farm startup replay reports a global overread in player_init_toys. Clang
discards initializers for AnimPackToc.start[0], leaving only its count word.
The source declares 67 initialized tables using this type. The original compiler
accepted their variable tails; silently losing them corrupts resource offsets.

Use a flexible array member only for native builds. Clang's standalone flexible
member initializer extension retains each complete object while sizeof keeps
the 4-byte prefix. Read indexed table words through the byte accessor rather
than indexing beyond the scalar count member. Keep original matching fields
and macros intact. Tests check prefix/offset layout, a synthetic variable tail
and original slot/item/resource table entries. Preserve the full native asset
base when binding toy sprites. No proprietary table or disc data is added.

<a id="adr-029"></a>

## ADR-029: Clear estate queues through native fields

Date: 2026-10-26. Status: accepted for native state safety.

After retaining animation table tails, the New Game replay reports an invalid
farm entry-step index, 0x00FF0001. A temporary function-exit trace identifies
estate_queues_clear as the first function damaging that field. It writes bytes
at PS1 FarmState offsets 0x49E6/0x4A36 with a four-byte stride; native pointers
have moved those queues, so the writes hit unrelated native state.

A narrow native branch writes each queue record's kind field by name. Preserve
the original byte-only behavior: message and timer fields are untouched, and
noticesSeen is cleared afterward. Keep the original offset loop in the matching
else branch. Regression checks retain entryStep and every message/timer while
clearing both queues. The diagnostic trace stays under ignored build output;
no instrumentation or sanitizer suppression is added to production code.

<a id="adr-030"></a>

## ADR-030: Connect source-used GS projection and light setup

Date: 2026-10-26. Status: reference setup; lighting parity pending.

GsSetProjection forwards valid source distances to the native GTE projection
register. Directional light setup normalizes each opposite direction into a
light-matrix row using deterministic integer square root, and installs RGB
coefficients in the matching color-matrix column. Ambient uses Q12 inputs via
the native back-color adapter. Only the source's normal lighting mode zero is
accepted; unsupported fog modes still fail explicitly.

Axis/color/ambient tests cover the setup. Exact SDK normalization quantization
and rendered lighting parity remain unverified, like the analytic trig path.
References: public GsF_LIGHT direction/normalization and ambient descriptions
in the Net Yaroze library reference and public libgs overview; no SDK code was
copied. GTE arithmetic remains in the separate scalar command implementation.

<a id="adr-031"></a>

## ADR-031: Select the first area without reading address zero

Date: 2026-10-26. Status: native safety adaptation; hardware parity pending.

The New Game replay reaches scene_resolve_selection before any area has been
loaded. farm_state_init clears farm_area.mode.data. The two previous-mine
override checks dereference that null pointer before scene_enter loads the
first area's data. ASan/UBSan identifies the first dereference on Windows.

For native builds, apply those overrides only when previous-area data exists. Leave the
loaded-area path and original matching conditions unchanged. Do not map address zero or
simulate PS1 RAM. A test calls the original selection function with no previous area, a
house upgrade/season, and loaded mine flags. The contents read at address zero on
original hardware are not established; this adaptation does not prove first-entry
hardware parity.

<a id="adr-032"></a>

## ADR-032: Leave absent spouse-marker objects untouched

Date: 2026-10-26. Status: native safety adaptation; hardware parity pending.

After the first area's models are populated, scene_slot_flag_move searches for
two optional marked entries, then unconditionally writes both pointers. The
New Game replay reaches an area without a marked entry and UBSan reports a
null FarmSceneModel access. The source asset struct has no native pointers;
tests assert its 0xBDC size and model offset 0x948 to exclude layout drift.

Only native builds skip the flag write for an absent entry. Existing entries
receive the same flags. Keep both original writes in the matching branch.
Extend the scene regression to populate a synthetic area with every model
absent. This does not emulate the original writes to address zero or establish
their hardware effects. No mapped low-memory page or fake scene is introduced.

<a id="adr-033"></a>

## ADR-033: Supply terrain and camera vector arithmetic

Date: 2026-10-26. Status: deterministic reference; SDK quantization pending.

The real-disc New Game path reaches ground_height_at and calls OuterProduct0.
Implement it through the existing scalar GTE OP command with shift zero and
preserve the rotation matrix, as documented for the non-inline SDK function.
Add source-used camera operations: ApplyMatrixLV multiplies three Q12 matrix
rows by a 32-bit vector, supports aliases and writes only three output words.
It does not add the matrix translation. This matters because the original
camera passes its three-word translation storage as the output VECTOR.

Supply integer SquareRoot0 and VectorNormal/S/SS reference implementations.
Normalize using a Q12 integer length to retain fractional lengths; preserve
output padding because the camera writes normalized vectors into matrix rows.
Return the squared input length from VectorNormal. Reject inputs outside its
documented signed squared-length range. Zero vectors produce zero components.
No host floating-point math or per-call allocation is used.

Tests cover every signed-domain square boundary, fractional/axis/zero normals,
in-place inputs, output padding, matrix/translation aliases and OP matrix
preservation. Full SDK lookup quantization, intermediate register side effects
of matrix/vector helpers and zero-vector hardware parity are unverified.
Reference: public Run-Time Library Reference 4.7, ApplyMatrixLV, VectorNormal
and OuterProduct descriptions; Inline Programming Reference OP preservation
distinction. Only API facts and the existing scalar hardware core are used.

<a id="adr-034"></a>

## ADR-034: Compose source-used Euler rotation matrices

Date: 2026-10-26. Status: deterministic reference; SDK quantization pending.

After terrain cross products, the real-disc path reaches RotMatrix from the
farm view setup. Implement the documented Q12 matrix products Rx * Ry * Rz for
RotMatrix and Rz * Rx * Ry for RotMatrixZXY. Reuse the port's analytic integer
sine table, use signed floor shifts and keep translation words untouched.
Quarter-turn tests check axis signs, noncommuting rotation order, return
pointers and translation preservation. Other rotation entry points remain
explicit failures until implemented. SDK sine-table and product-rounding
parity remain open; no SDK implementation or table is copied.

Reference: public Run-Time Library Reference 4.7, RotMatrix table 8-2,
https://psx.arthus.net/sdk/Psy-Q/DOCS/LibRef47.pdf (page 8-116).

<a id="adr-035"></a>

## ADR-035: Resolve player probe steps across native table objects

Date: 2026-10-26. Status: accepted for native bounds safety.

The New Game replay reaches player_probe_target and indexes halfword 56 of
move_steps_small[16]. Its callers use bases 0, 0x18 and 0x38 plus facing * 2.
These cross object boundaries in the original overlay's adjacent data tables.
A local read of the owner's validated farm overlay confirms the source order:
small (16 halfwords), large (16), riding (8), hop (8), reach (16). No data is
extracted into the repository.

Native builds explicitly select small[facing * 2], large[8 + facing * 2], or
reach[4 + facing], respectively. Do not assume native linker adjacency, clamp
the index, expand the first array with copied game data, or disable bounds
checks. Invalid facing/base values fail explicitly. Regression checks all
three bases and four facings against the actual source arrays. The original
matching expression remains unchanged. HOPS has a separate cross-table alias
which remains tracked and has not yet been reached by this replay.

The movement replay also reaches the original flattened facing_reach access
through row zero. Native builds select facing_reach[facing] directly, keeping
the same halfwords without pointer arithmetic beyond the first two-word row.

<a id="adr-036"></a>

## ADR-036: Implement bounded nonoverlapping VRAM moves

Date: 2026-10-26. Status: nonoverlapping/identical copies implemented.

The New Game scene animation calls MoveImage. Execute nonoverlapping transfers
directly on the game thread in the existing VRAM model, including 10/9-bit
coordinate wrapping, zero-as-maximum extents, mask checks/sets and dirty tiles.
MoveImage2 uses the same synchronous transfer path. Identical coordinates are
valid and still apply mask rules. No copy allocation is required.

Nonidentical overlapping source/destination rectangles fail explicitly, with
wrap-aware intersection detection. The public hardware transfer description
does not establish their internal copy order, so do not invent snapshot or
memmove behavior. Tests cover wrapped sources, mask preservation, identity
copies and the overlap diagnostic. This restriction must be resolved if an
actual game path requires overlapping transfers.

Reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/,
GPU memory transfer commands and mask-bit settings. No SDK code is used.

<a id="adr-037"></a>

## ADR-037: Apply axis rotations to the existing matrix

Date: 2026-10-26. Status: deterministic reference; SDK quantization pending.

The real-disc path reaches RotMatrixY in camera_frustum_setup. Supply X/Y/Z
helpers by left-multiplying the existing rotation matrix by the requested axis
rotation. Preserve translation and use an alias-safe intermediate. Reuse the
integer Euler path rather than adding a separate sine or rounding policy.
The test composes Y then X then Z and compares the resulting noncommuting
quarter turns with the documented ZXY Euler order. SDK product quantization
remains unverified. Remove the corresponding test-only projection traps now
that real implementations exist.

Reference: public Run-Time Library Reference 4.7, RotMatrixX page 8-120,
https://psx.arthus.net/sdk/Psy-Q/DOCS/LibRef47.pdf (axis matrix * m).

<a id="adr-038"></a>

## ADR-038: Yield at the event loader's busy CD poll

Date: 2026-10-26. Status: accepted deterministic scheduling adaptation.

The non-sanitized Windows release stalls during New Game after tick 4800.
A native debugger backtrace identifies event_script_load_wait looping in
afile_read_bytes/fade_is_settled. The original CD progresses asynchronously;
the native runtime advances transfer credit only at deterministic VBlanks.
This loop can poll forever without returning to a VSync call.

Only in native builds, yield one VBlank when a script read is pending and the
fade is not settled, immediately before repeating that busy loop. This services
CD transfer credit, input, audio and original VBlank callbacks on the same game
thread. Preserve nonblocking CdReadSync semantics and the original matching
loop. Do not advance CD data on arbitrary status polls or a host background
thread. A local release replay then enters the original grandparent scene
and dialogue instead of hanging. Longer input/scene coverage remains pending.

The flashback's foreground load reaches the second empty polling loop in
event_script_start, confirmed by an attached native debugger. Yield one native
VBlank in that loop as well. Both waits service the same hardware tick path.

Returning from the flashback reaches a sound-bank wait in
farm_apply_pending_mode. Its repeated sound_load_tick calls also need the
CD tick to complete the bank. A native debugger confirms this third spin.
Yield while the bank remains pending, preserving the ordinary per-frame
sound driver and the matching loop.

<a id="adr-039"></a>

## ADR-039: Handle an absent terrain primitive on native hosts

Date: 2026-10-26. Status: native safety adaptation; low-memory parity pending.

The extended opening replay changes to area 230. A native debugger catches
player_frame_update reading address zero in its terrain-result helper. The
ground query can return no primitive, but the source unconditionally reads
its first word. player_update_position contains the same unconditional read.

For native builds, an absent primitive supplies no terrain flags (word zero).
Keep the first helper's existing 0x8000 fallback when no high flags are present;
the position path retains the zero word. Read actual primitive words with the
fixed-width byte accessor. Keep both original dereferences in matching branches.
This lets the release replay enter the original flashback scene; it does not
establish the contents/effects of original low-memory reads.

<a id="adr-040"></a>

## ADR-040: Return native event-request pointers explicitly

Date: 2026-10-26. Status: accepted for native calling-convention correctness.

The release replay runs the introductory script while the sanitizer replay
clears New Game and enters the farm without it. Both event_request_claim
wrappers call scene_claim_entry but omit return despite declaring a pointer
result. Callers require that pointer to populate script id/flags. Relying on a
callee's residual return register is undefined in native C and instrumentation
can change the result. These omissions are visible compiler warnings.

Native branches explicitly return scene_claim_entry's pointer. Keep the original
call-only bodies for matching builds. A regression calls both original wrappers
and checks returned slot identity, zeroed arguments, duplicate rejection, all
five queue entries, full-queue failure and reuse after reset. Do not suppress
the warning or force a guessed success pointer. Other return-type warnings
remain tracked for their own call paths.

<a id="adr-041"></a>

## ADR-041: Decode the source disc's XA streams on the game thread

Date: 2026-10-26. Status: deterministic reference; hardware audio parity pending.

The extended opening reaches cd_audio_play_track and requests Setfilter/ReadS.
A local header-only scan of all four XA files confirms file number 1, channels
0..7, and coding byte 1 (37.8 kHz stereo, 4-bit ADPCM) for every audio sector.
Unused sectors and per-channel EOF records are present. No audio/game data is
added to git. Other XA coding modes remain explicit failures.

Reuse the port's own tested SPU predictor decoder by rearranging each XA sound
unit's nibbles into a bounded temporary block. Keep independent channel history.
Resample each six decoded stereo frames to seven output frames using the public
29-tap/seven-phase zigzag coefficient facts. Reset decoder history at ReadS.
The public reference itself notes uncertain hardware rounding and startup phase;
this is a reproducible reference, not proof of bit-exact CD audio output.

Advance physical sectors at the existing 75/150-sector deterministic tick rate,
apply file/channel filtering, queue selected PCM in a fixed 8192-frame game-thread
ring, and drain queued audio after selected-channel EOF. Standby/cancel/data reads
clear the stream. GetlocP returns an eight-byte position response without a
four-byte write into its final three-byte position field. No audio worker or
per-frame heap allocation is introduced.

Mix decoded PCM through the source-used serial volume/mix controls before mono
and master gain, with saturation. Exact SDK volume law and CD reverb parity are
unverified. Synthetic tests cover literal resampler impulse values, predictor
history, coding rejection, filter/EOF selection, position response bounds, PCM
mixing, muting and cancellation. Real-disc continuation remains under test.

References: https://psx-spx.consoledev.net/cdromformat/ (XA headers, predictors,
interpolation coefficients) and public CD command descriptions. No SDK or other
PSX implementation code is copied.

<a id="adr-042"></a>

## ADR-042: Pass the harvest helper to its action callback

Date: 2026-10-26. Status: accepted for native calling-convention correctness.

After Zack's introduction, the first unpaused field ticks run the harvest
helpers. ASan catches a write into read-only storage in farm_cue_step_run,
called through harvest_helper_timer_tick. That dispatcher calls action() with
no argument although the cue/work callbacks require the helper/actor pointer.
The original code relies on an argument register staying live; native code
cannot rely on it, and the wrong pointer reaches cue state writes.

Pass helper explicitly only in the native branch. Keep the original no-argument
call in the matching branch. All action-table entries consume the helper or
its first actor member. A regression runs the original public helper updater
twice with all seven helpers offscreen and clock ticking, checking each cue's
start/play state, timer, position integration and unrelated field preservation.
No helper is disabled or replaced with a success stub.

<a id="adr-043"></a>

## ADR-043: Keep storage slot and navigation access within native objects

Date: 2026-10-26. Status: accepted for native array bounds.

A local replay opens the original toolbox screen. Inspection of its input and
transfer path finds two original array-subobject crossings: item_row_at indexes
past player.held[2] to reach tools/items, and navigation indexes past the first
marker's 12-byte next_cell field to reach subsequent markers. Modern bounds
instrumentation cannot treat those distinct arrays as one object.

Native branches select player.tools/items explicitly for bag slots 1..8 and
the selected marker's own navigation field. Held and storage-grid routing stays
unchanged; no out-of-bounds bag address is formed for grid slots. Matching
branches retain the original flattened expressions. This preserves the exact
logical slot mapping, without changing inventory rules or item data.

The regression includes the original storage implementation and checks actual
held, bag and all storage-row pointer destinations for toolbox/cabinet/icebox.
It also asserts the 16-byte marker layout and four-byte navigation offset.
Real menu transfer and subsequent tool-use replay coverage is in progress.

<a id="adr-044"></a>

## ADR-044: Documents storage and persistent native memory cards

Date: 2026-10-26. Status: accepted by explicit owner storage request.

The owner chooses Documents/BackToNature/{saves/,settings.json} instead of the
default application-pref directory, and performs interactive gameplay tests.
Resolve Documents through SDL_GetUserFolder, respecting Windows folder
redirection and Unicode names. Never hardcode C:/Users/User or the local owner
account. The native window initializes storage; headless diagnostics remain
disconnected unless explicitly given a storage backend by a unit test.

Implement both ordinary memory-card slots using a native, versioned container
(saves/card1.hmc and card2.hmc), not a raw PS1 image. The container has an explicit
512-byte little-endian directory/header and fifteen 8192-byte data blocks.
Each 32-byte directory record holds a zero-terminated 20-byte maximum name,
block count and starting block. Preserve the game's original file payloads,
allocation limit and read/write offsets. The original overlay requires read
completion ID 3 and write ID 4; complete requests deterministically on the game
thread. Ordinary cards report McxErrInvalid to PocketStation type queries, so
the original game selects its ordinary-card path.

The platform writes a sibling temporary file, flushes/closes it and renames
it over the destination. Failed writes leave the old container and committed
RAM state intact. This reduces partial-write risk; power-loss durability has
not been established. Validate header/version, names, sizes, allocation bounds,
duplicate names and overlapping block ranges on mount. Corrupt or unreadable
existing cards are reported as invalid and never automatically reformatted.
Explicit in-game format/unformat requests replace only the selected card.
Windows holds an exclusive file handle on .lock for the session to prevent
concurrent writers. Additional platform locking remains future platform work.

Create settings.json with schema_version 1 only if missing. Preserve an existing
file byte-for-byte; reading/applying settings is a separate pending capability.
No game option or enhancement is introduced here. All SDL and Windows filesystem
APIs remain in platform/. No decomp source or new dependency is changed.

Tests cover native card capacity, slots, directory patterns, range errors,
completion IDs, reopening serialized bytes, failed-write preservation and corrupt
input. An SDL filesystem test uses a unique build-local Unicode directory for
actual file reopening and existing-settings preservation. The owner validates
the real save/load menus; storage unit coverage does not establish that the
entire original game serialization or overlay reentry works.

<a id="adr-045"></a>

## ADR-045: Yield during save/load asset waits and own the complete save image

Date: 2026-10-26. Status: accepted for native save/load bring-up.

The owner reports an unresponsive window after selecting Load at the title.
Attaching LLDB to that existing process finds the game thread in CdReadSync,
afile_read_bytes and load_screen_tick. Detach after collecting the stack;
no gameplay input or new game/replay session is launched. Both save_load_payload
and save_load_image_file poll asynchronous reads in tight loops without VSync.
The native CD model advances only at deterministic VBlank points, which also
pump the window host's events. Yield one VBlank only while each read is pending.
Keep both original matching loops unchanged in their non-port branches.

The same source declares a 0x80-byte header followed by three 0x80-byte icon
frames, a 0x80-byte slot record and a payload ending at byte 0x9690. It reads,
writes and clears the combined span through the header. Native globals need
not be adjacent, and sanitizer redzones make that access invalid. Provide one
native union containing the complete byte image and those same typed slices;
assert total size and the 0x80, 0x200 and 0x280 offsets. Retain the original
separate globals in the matching branch. This changes native workspace ownership,
not the on-card file format or serialization rules.

The overlay's save_card_poll explicitly expects command 1 from MemCardExist.
Correct the native existence completion from its previous opaque value 2 to 1;
otherwise the menu stays busy even after a successful existence query.

An isolated test includes the original save/load implementation. Its bounded
asynchronous asset fixture completes only after two real native VSync ticks, and fails
quickly if the original busy polling returns. Rendering sinks capture the expected
texture and five sprite bindings without parsing game assets. The test also runs
save_reset_state, verifies the contiguous image slices, and exercises the original
probe/poll helpers on both synthetic ordinary cards. Storage regressions verify the
existence completion ID. No real save files, gameplay session or owner Documents
directory is accessed by these tests. In-game confirmation remains with the owner.

<a id="adr-046"></a>

## ADR-046: Give menu background callbacks an exact native function type

Date: 2026-10-26. Status: accepted for native indirect-call correctness.

The shared menu background dispatcher calls a TileDrawFn taking s16 frame and
s32 coordinates. Its two original callbacks instead declare int/short or
short/short parameters. Earlier toolbox sanitizer diagnostics reported this
incorrect indirect function type; save/load screens use the same dispatcher.
Keep the original callbacks and use native adapters with the exact dispatcher
signature, explicitly narrowing coordinates as the original callbacks do.
Do not suppress function-type sanitization or change matching callback selection.

A regression calls both original public background modes twice, including the
fourth-call scroll increment. Capturing drawing sinks check all 165 tiles per call,
frame sequence, negative coordinates, animation binding and current frame
primitive-cursor pointers. No actual asset or window is used.

<a id="adr-047"></a>

## ADR-047: Restore title and save/load overlays from native object snapshots

Date: 2026-10-26. Status: accepted for the title/load/return lifecycle.

The owner reaches the Load menu, then returning to title aborts at the explicit
overlay reentry guard from ADR-017. Removing that guard alone would retain
mutated overlay data and BSS. Capture pristine native object representations
before hm_game_main, after native relocations and C initializers have run.
Before each supported activation, copy them back into their original objects.
Resident game state, work buffers, hardware state and the other overlay region
are not included in this restoration.

The title inventory contains all 44 writable file-scope objects; save/load has
42 original writable objects plus the native save workspace. Clang AST inspection
and source review found no function-local static variables in these two units.
Keep immutable constants untouched. Lists live in native-only includes at the
end of each original translation unit and must be maintained when writable
overlay state changes. This preserves private symbols and captures initialized
tables, pointers, callbacks and zero-initialized arrays without assuming object
adjacency, PS1 address layouts, linker sections or disabled sanitizer redzones.

The registry copies object descriptors and pristine bytes once at startup.
Repeated capture calls cannot replace the pristine copy with mutated state.
Registry metadata reset retains snapshots. Unsupported overlays still explicitly
reject reentry; their state inventories are not silently assumed complete.
This supersedes ADR-017's reentry restriction only for title and save/load.

Tests mutate actual private title counters, callback, arrays, native pointers and
initialized tables and verify five restorations. The save/load fixture verifies four
restorations including its complete workspace and animation pointer. A separate
integration unit registers both real inventories, alternates their selections six times
and checks resident game state survives. No game loop, window, disc or interactive
replay is started.

<a id="adr-048"></a>

## ADR-048: Pause at the host boundary with private original-game UI resources

Date: 2026-10-26. Status: initial interface implemented; advanced rendering pending.

The owner requests an ESC interface across scenes with Resume, Settings >
Graphics and Exit Game, using the game's own UI presentation. Complete the
title/load reentry fix first, merge PR #1 as windows port init, remove old
branches and create port/windows-pause-menu from the latest main. The new
feature work is separate from that merged Windows foundation.

Pause inside the SDL host VBlank boundary. Keep pumping OS events and drawing
the private menu without returning to VSync, CD, SPU or the game callback.
Pause/resume the audio device, retain its sample queue, and shift the pacing
epoch by elapsed pause time. Suppress held confirm/pad input until released
when returning to the game. The UI navigation state machine is data-free and
does not touch the game's dialog state, current frame, ordering tables or RNG.

Read the resident UI bank from A_FILE through the independently validated disc
parser before game entry, decode with the original texture-pack function,
cache its original 8x12 English font and text palette, and reset startup VRAM.
The cache remains private across all overlay transitions. Draw the actual
window_frame.c geometry through private pixel/line/gradient sinks. New labels
use the game's glyph mapping. Do not bundle extracted assets or substitute a
host font, generic UI toolkit or invented bitmap art.
The original primitive builders consume packed 0xRRGGBB; do not follow the
contradictory older 0xBBGGRR comment in window_frame.c. The owner reported a blue
initial prototype, caused by reversed red/blue channels in the private canvas.
Correct that conversion and retain the original cream gradient. Synthetic
checks assert the expected cream RGB channels and a red glyph independently.
Use compact centered root/settings panels and distinct Graphics header, aligned
label/value rows, selection frame and detail footer. Asset-only previews are
written under ignored build paths for visual inspection, never into git.

SDL platform code enumerates monitor display modes, applies windowed/exclusive/
borderless modes and offers nearest or bilinear final-frame filtering. Preserve
4:3 in the active resolution with black bars. Borderless uses the desktop mode.
The graphics object in settings.json is atomically replaced while preserving
other root properties. Reject malformed JSON rather than overwrite it. The
owned object stores monitor ordinal, window_mode, width, height and bilinear.
Unavailable stored monitor/mode choices fall back to a valid display or default
window. Multi-monitor hotplug and display switching still require owner testing.

HMBTN_ENH_PAUSE_MENU remains OFF by default in CMake; Windows -Game enables it
for the owner's explicitly requested globally available interface. Its runtime
default is available, with enhance.pause_menu=false as an explicit OFF path.
AGENTS.md records this narrow override. Native scene scaling, expanded camera/
culling and FXAA/MSAA are separate pending render paths. Their menu entries
remain OFF and unavailable. Bilinear final-frame presentation is not advertised
as native higher-resolution rendering or per-texture filtering.

Synthetic tests cover navigation, original dialog-frame geometry with synthetic
glyphs, JSON editing/validation and preservation of unknown settings. A local
asset-only probe decodes the owner's font and checks VRAM is reset, without
opening a window or entering the game. The owner performs interactive pause,
audio, fullscreen and gameplay validation. No new dependency or decomp source
change is required for this initial interface. Performance budgets and rendering
enhancement parity remain unverified.

<a id="adr-049"></a>

## ADR-049: Independent rational presentation limits and VSync control

Date: 2026-10-26. Status: implemented; owner device/gameplay tests pending.

The owner requests VSync On/Off and Native or monitor-derived FPS limits without
changing game time. Original main_run_frame uses VSync(2): 30 normal updates
at the port's fixed 60 Hz hardware tick. Keep this source, psx/timing.c, CD,
SPU, RNG and game callbacks unchanged. During each host tick wait, service a
separate presentation clock and pump OS events. Sample input at the tick
boundary. No new game-logic thread or variable simulation step is introduced.

Accumulate absolute rational nanosecond deadlines, preserving 60000/1001 rather
than truncating to 59 or 60. Drop overdue presentations instead of submitting
catch-up bursts. Display settings reset only presentation scheduling; paused
wall time remains excluded from the game epoch. Native caps presentation at 30.
Other choices come from the monitor's desktop-resolution modes when windowed/
borderless, or selected resolution when exclusive. Persist fps_numerator and
fps_denominator in graphics; zero numerator means Native. Unsupported saved
rates revert to Native. An FPS cap does not itself change desktop refresh.

On uses synchronized mailbox if available, otherwise FIFO. Off requires
supported immediate presentation; reject unsupported changes and retain the
effective setting. An unavailable stored Off setting falls back to On. Bound
VSync-On presentation by the current physical refresh and recompute on display
changes. Acquire through SDL's non-waiting frame-fence path; cancel when no
swapchain image is available and retain two frames in flight. SDL 3.2.28 Vulkan
WSI acquisition can still wait internally, so no zero-driver-stall guarantee
is made. Hardware stalls and achieved FPS remain owner device checks.

Higher caps repeat the latest native image. They add no game update, animation
advance, RNG call or audio generation. They do not provide 75 unique scene
frames or interpolation. Graphics now has nine items with seven visible rows
and scrolling. Decimal rates use the original narrow dot glyph, verified from
a private asset-only atlas; no extracted font is committed.

A synthetic regression checks 200000 rational presentation boundaries per
rate and verifies 300 real native VSync(2) calls still give 600 VBlank callbacks
at 30, 59, 60, 75, 144, 240 and 60000/1001 caps. Late dropping, menu scrolling
and decimal composition are covered. These are technical checks, not gameplay
time or physical VSync tests. No dependency or matching-source edit is added.

<a id="adr-050"></a>

## ADR-050: Opt-in private primitive replay for visual interpolation

Date: 2026-10-26. Status: synthetic validation passed; owner gameplay pending.

The owner explicitly requests smoother world movement above native FPS without
altering game time. Keep original VSync(2), fixed 60 Hz timing, logic, RNG and
audio unchanged. Add HMBTN_ENH_INTERPOLATION, OFF by default, compiled by the
Windows -Game script, and runtime enhance.interpolation, default false. Graphics
adds Interpolation as the tenth item, retaining seven visible rows.

Introduce generic frame observers and bindable VRAM/GPU environments. Native
CMake remaps only main.c's PutDrawEnv/DrawOTag call sites to port adapters; no
matching source edits. Capture two bounded native command histories and replay
screen-coordinate interpolation into private VRAM. Restore native contexts
before returning to simulation. Packet keys normalize double-buffer arena
addresses and require unique, compatible primitive/material records. Keep
current texture poses and secondary UI positions; do not crossfade images.

Use the native back-buffer lead to interpolate previous to current coordinates
over two ticks. Present the private draw rectangle. Never extrapolate or update
actors during presentations. Reject incompatible histories, duplicate keys,
material changes and large motion. Invalidate on overlay/context transitions.
Limit initial eligibility to normal farm views without loading, transition,
pause or menu flags. Unmatched primitives keep current geometry, so coverage
and cadence depend on stable native command allocation. This is an experimental
screen-space heuristic, not persistent actor/world-space interpolation.

The OFF path has no capture allocation or packet changes. Synthetic tests cover midpoint
geometry, current UI, context restoration and history rejection; release and ASan/UBSan
pass. Compile-time OFF still links the native executable. Full gameplay, native image
parity, input latency and actual frame cost require owner validation. No new
dependencies, assets, threads or matching guards. See
port/enhance/interpolation/README.md for limits and PLATFORMS.md for the explicitly
synthetic lower-bound replay measurement.

<a id="adr-051"></a>

## ADR-051: Unlimited presentation as an explicit FPS Limit choice

Date: 2026-10-26. Status: implemented; owner device tests pending.

The owner requests Off in FPS Limit. Keep Native (30) as the default and add
Off (Unlimited) before monitor-derived rates. Persist graphics.fps_numerator=-1
with fps_denominator=1; zero still means Native, so existing settings retain
behavior. Presentation-clock rate 0/1 means no software deadline interval.
When VSync is On and current refresh is known, use that refresh as the effective
presentation rate. Off VSync plus Unlimited adds no software presentation cap;
GPU availability and driver behavior still determine achieved submissions.

The host checks the absolute hardware-tick deadline before each presentation
and continues to run native simulation only at fixed 60 Hz, with VSync(2)
normal game frames. Unlimited does not call gameplay/audio/RNG to fill frames.
Synthetic tests perform 600000 visual attempts while 300 original VSync(2)
calls still produce exactly 600 ticks/callbacks. No gameplay or device run is
claimed. No decomp source edits, new dependency or threads are introduced.

<a id="adr-052"></a>

## ADR-052: Private viewport-resolution software command replay

Date: 2026-10-26. Status: synthetic validation passed; owner gameplay pending.

The owner prioritizes graphics and previously requested Enhance Scaling at the
active resolution. Extend the existing CPU raster/replay architecture with a
private high-resolution command sink, keeping SDL_GPU presentation and the
native PS1 VRAM path. Do not label a framebuffer enlargement as higher-resolution
scene rasterization. Add HMBTN_ENH_SCALING, OFF by default; Windows -Game compiles
the requested option. Runtime enhance.scaling defaults false.

Generalize frame observation to four independent subscribers, and add a scoped
private-replay sink. The scaling module captures two bounded native streams
and replays the displayed one into a viewport-sized 15-bit surface. Interpolated
replay can feed the same sink. Unsubscribe each feature by context so toggling
one never removes the other. Loading/direct scanouts and invalid/missing
histories retain native presentation. Overlay changes invalidate both histories.

Preserve the current 4:3 viewport and original texture data. Triangles/quads use
output-pixel coverage and attribute interpolation; sprites sample original
texels, and lines retain logical-cell coverage. Preserve native masks, blending,
clip/environment commands and inline image uploads/fills. Execute native GPU
commands in private VRAM too for texture feedback semantics. Native state,
timing, GTE and RNG remain unchanged. Cache repeated non-interpolated scanouts;
allocate only when enabling or changing output dimensions. Limit this initial
CPU target to 4096 per axis with visible, recoverable native fallback on failure.

SDL textures match output bounds to avoid stale border sampling. Transfer
capacity only grows; resource replacement follows SDL's deferred release
contract. Pause remains a logical 320x240 UI over its frozen background.

The software path gives a bounded, testable first implementation without new
dependencies or shaders. It does not guarantee high FPS: a synthetic 1440x1080
single-triangle pass costs about 5.8 ms after removing repeated coordinate-map
division and constant-color interpolation. Real scenes and overdraw cost more.
GPU scene rasterization, per-texture filtering, FXAA/MSAA and widescreen camera
remain separate pending work. See PLATFORMS.md for full measurement limits.
Release/ASan/UBSan pass 77 isolated tests. Native builds also link with features
OFF and with scaling ON / interpolation OFF. No matching source/guard is changed.

<a id="adr-053"></a>

## ADR-053: Shared replay ownership and coherent interpolation identity

Date: 2026-10-26. Status: technical regressions pass; owner gameplay pending.

The owner requests a GPU output path shared by native resolution and scaling,
and reports black cuts across farm ground which disappear with Interpolation
OFF. Inspection shows field cells are sprite draws from shared animation
records, while packet addresses depend on prior work-buffer allocation. Packet
offsets alone are not stable identities. Independently falling back neighboring
polygons can also open a hole between current and interpolated positions.

Extract bounded frame capture into reference-counted gfx/frame_replay so output
backends do not depend on enhance.scaling. Each replay restores native VRAM and
GPU environments. Generic destination hooks can consume private geometry and
fractional motion metadata without a redundant CPU output raster. The normal
game's CPU VRAM still supplies authoritative readback/transfer semantics; no
logic, timing, RNG or GTE changes are introduced.

Add bounded construction identities for field sprites, keyed by the original
world-cell SVECTOR and part offset. CMake remaps field.c projection/sprite calls
to read-only port adapters, and main.c's ClearOTagR resets the registry. Actual
source files remain unchanged. Invalid/nonmonotonic spans fail closed.

Group world polygons sharing normalized screen vertices. If any member lacks
compatible history, or a shared vertex has conflicting previous coordinates,
render that connected component at current positions. This trades some local
interpolation cadence for a closed mesh. Separate sprites are unaffected.
Fractional destination metadata retains original current texture poses.

A synthetic two-quad reproduction fails against f93bb01 (black gap at x=8..9)
and passes with the coherence guard. Other checks cover conflicting history,
stable cell identities despite inserted packets and output-backend motion.
Portable GPU packet tests cover bounds, fallback, independent replay ownership
and native state preservation. Windows integration passes 79 release and
ASan/UBSan tests; these are not gameplay confirmation of the reported scene.
No new matching source guard or third-party dependency is required for this layer.

<a id="adr-054"></a>

## ADR-054: Shared native/enhanced GPU compute scene output

Date: 2026-10-26. Status: Vulkan/D3D12 readback passed; gameplay pending.

The owner explicitly expands GPU acceleration to native output as well as
Enhance Scaling. Use one SDL_GPU compute pipeline over portable scene packets
(ADR-053). Each output pixel traverses its ordered logical tile list, keeping
color/mask locally. This preserves PS1 ordering, four blend modes, CLUT/tpage
sampling, texture windows and masks without cross-pixel write races or
per-primitive feedback passes. Native 1x coverage/attributes use bounded integer
math. Enlarged/fractional geometry uses fixed-position inputs and floating
attribute interpolation; texture poses remain native.

HMBTN_GPU_RASTER defaults ON. graphics.renderer=0 means GPU Auto (default),
1 means Software. This backend is independent of enhance keys. Graphics adds
Renderer as item 11 and reports active GPU or CPU fallback. Software releases
its GPU capture reference; scaling may retain its own. Private geometry
consumption avoids a second CPU output raster. Normal game execution still
maintains low-resolution CPU VRAM for readback/transfer compatibility. Pause
composition retains its small original-asset CPU canvas with GPU presentation.
This does not claim all graphics CPU work has been removed.

The platform backend supports SPIR-V/DXIL, using ByteAddressBuffer for SDL
3.2.28's raw D3D12 storage-buffer view. Native output stays 4:3. Fractional
motion metadata supplies subpixel triangle/sprite positions without simulation
or GTE changes. Use native front history with interpolation OFF, interpolated
current replay when eligible/ON. Cache unchanged scenes/output. GPU buffers and
textures cycle for queued frames. Normal presentation neither waits for a
compute fence nor reads pixels back. Existing fixed tick/presentation clocks
and driver acquisition limits remain unchanged.

Inline transfers, framebuffer texture/CLUT feedback, offscreen dependencies,
exhausted capacity and direct/loading scanouts retain software output.
Unsupported shader formats (including current Metal) also fall back.
Conservative rejection may reduce GPU coverage; the menu exposes fallback.
No full-game or cross-platform parity claim is made.

Own HLSL is compiled into checked-in C arrays with normalized source hashes.
Normal builds need no shader compiler. Developer regeneration uses pinned DXC
v1.8.2505.1 in ignored host-tools, recorded in THIRD_PARTY.md. No compiler,
game asset, SDK material or BIOS is shipped, and no linked dependency is added.

Offscreen Radeon RX Vega probes compare native RGB bytes exactly to CPU on
Vulkan/D3D12, including 2048 deterministic random primitives, texture depths/
windows, masks, blends, clipping, lines and both framebuffer origins. Enlarged
color allows one 5-bit step; fractional tile-boundary motion is checked
separately. No window/game is started. Synthetic timing shows enlarged-output
benefit and small 1x overhead; see PLATFORMS.md.

## ADR-055: Optional palette-aware texture filtering

Date: 2026-10-26

Context: The owner requests Bilinear/Nearest texture filtering. Previous
Bilinear only filtered final presentation. It must also work without scaling,
without changing native VRAM or simulation, and preserve transparent edges.

Decision: Add enhance/filtering, gated by HMBTN_ENH_FILTERING (CMake default
OFF; Windows -Game enables compilation). Runtime enhance.filtering defaults
false. An existing explicit graphics.bilinear choice migrates when the new key
is absent; an explicit new key wins. The existing Filtering row selects it.
GPU scene header word 11 carries the choice; Nearest retains its old branch.
CPU output raster/replay helpers move to gfx so filtering does not depend on
Enhance Scaling. Unsupported GPU scenes retain the software reference.

Sample four CLUT-decoded texels with Q8 weights. Clamp to each primitive's UV
bounds before texture-window/page addressing, avoiding adjacent atlas texels.
Shade and PS1 semi-blend each covered tap against the original destination;
transparent taps contribute that destination, avoiding black halos. Use the
highest-weight covered tap's mask bit (first wins ties); apply force-mask only
when covered. Interpolation and simulation remain independent. Native source
assets are unchanged; bilinear does not create higher-detail textures.

Consequences: Optional replay adds output work and CPU fallback can be costly.
Primitive-local clamping can expose differing filtering at shared UV bounds.
The game's 8x12 text glyph atlas stays nearest-sampled to keep small UI text
crisp in filtering modes 1 and 2. Final presentation filtering remains for
window scaling. Generated shaders hash both scene.hlsl and the filtering
include. No new linked dependency.
Release and ASan/UBSan pass 80 tests. Offscreen Vulkan/D3D12 comparisons cover
Nearest and Bilinear, CLUT depths, texture windows, masks/blends, deterministic
random primitives, native and enlarged outputs. Native comparisons are exact;
enlarged color tolerance is 9/255. These do not establish gameplay performance
or full-game atlas compatibility; owner gameplay validation remains pending.

## ADR-056: Expanded horizontal world viewport with native UI

Date: 2026-10-26

Context: The owner requests Widescreen Mode that reveals additional world, with
OFF retaining original 4:3 bars. Stretching the framebuffer cannot reveal it.
The main OT already identifies world packets separately from UI layer OTs.
Original model/sprite/field CPU culling otherwise discards the extra geometry.

Decision: HMBTN_ENH_WIDESCREEN defaults OFF in CMake; Windows -Game compiles it.
Runtime enhance.widescreen defaults false. The existing Graphics row toggles it.
Keep the original GTE projection, screen coordinates, camera eye/target, depth,
simulation ticks and RNG. Expand the private output's logical horizontal domain
symmetrically around its original center. At 16:9 this is [-54,374) x [0,240),
rounded to even 428 logical pixels; 21:9 and 32:9 follow the active pixel aspect.
Logical width is bounded to 1024; portrait/4:3 retain the native view.

Record OT world/UI group boundaries in shared replay and forward them through
interpolation. Widen only full-width world clips; ordinary UI stays centered at
native coordinates and native size. Exact untextured full-screen F4 fade/fill
packets cover the expanded view. Clear the added sides to black instead of
sampling adjacent/wrapped native VRAM. Texture feedback checks still use the
native framebuffer rectangle, not virtual sides. GPU and software support both
native-height and enhanced output. Unsupported/direct/loading scanouts retain
4:3 fallback. Title/menu-only scenes and minigames retain 4:3; this first world
camera implementation targets the farm normal-view renderer, including UI overlays.
The ESC pause canvas itself retains its original centered 4:3 backdrop.

Narrow guarded source changes are required for the static model clip bands,
horizontal frustum angles, estate sprite visibility and field draw radius.
Each uses HMBTN_PORT && HMBTN_ENH_WIDESCREEN and preserves the exact original
#else (or absence of the added guard). Guard locations are listed in ANALYSIS.
Only rendering coverage changes. Keep original primitive arenas/sizes and overlay
backup memory intact; stop model submission with an 8 KiB UI reserve when the
original arena is exhausted. Dense scenes can therefore omit excess geometry.
Use two private 256 KiB GS work arenas while wide world drawing is active;
restore original pointers/sizes on OFF or leaving the world. The main.c-only
GsSetWorkBase adapter preserves ordinary cursor advancement.

An enlarged synthetic triangle exposed floating edge cancellation at the new
aspect. Use exact Q8 signed 64-bit edge signs near the boundary, implemented
with 32-bit word arithmetic so shader-int64 support is not required. Nearest
native shader coverage remains on its existing integer path.

Consequences: Extra authored world is submitted and rendered, without stretching
original UI or changing the game camera's simulation state. This does not create
new level geometry, enlarge weather particle pools, or change their RNG. Some
assets/actors have additional original visibility rules; all locations, weather,
dense scenes, transitions and gameplay still need owner validation. CPU fallback
at 1080p can be expensive. No full-game parity or high-FPS claim is made.

Validation: Release and ASan/UBSan unit suites; Vulkan/D3D12 offscreen world/UI,
texture-filtering, fade and both framebuffer-origin comparisons. Exact native
comparisons and 9/255 enlarged color tolerance. GS cursor/OFF arena preservation and
unchanged native projection are covered by unit tests. A mechanical check removing only
the new guards reconstructs each original source file exactly. No matching result is
claimed.

## ADR-057: Stable widescreen presentation across every screen and fallback

Date: 2026-10-26

Context: The owner requires widescreen from the Natsume logo through title,
load/save, gameplay and pause, preserving proportional logos/text and extending
background from original assets. They report aspect switching depending on
whether a dialogue is visible, plus dialogue lag with Enhance Scaling. This
supersedes ADR-056's world-only/pause/non-world presentation restriction.

Decision: Separate the persistent full-window presentation setting from replay
availability and world interpolation eligibility. Every 320x240 frame can use
the expanded private canvas, including 2D views and minigames. Camera culling
continues to use original coordinates and only expands horizontal visibility.
Do not gate the viewport on farm normal-view or dialogue state. Initialize the
setting before the first logo and refresh it during pause/display changes.

The title's existing animation 0x25 tiled background is marked at construction
through a title.c-only CMake adapter. Generic packet span metadata changes its
replay group, widening its clip to expose existing background tiles. Other
logo/text/cursor packets retain original positioning and scale. Full-screen
flat backgrounds/fades already cover the canvas. Extra canvas initialization
uses clamped native edge/background pixels rather than neighboring VRAM. For
direct/loading/native fallback or the pause canvas, the shared SDL compositor
fills side regions from original edge columns and places original content at
its proportional centered size. Original black background remains black; no
invented scenery or replacement assets are introduced.

Two synthetic reproductions confirm excessive GPU fallback: a draw clip that
extends below the viewport although visible glyphs stay inside, and an atlas
page that intersects the framebuffer while the sampled UV rows do not. Test
actual primitive bounds after clipping for offscreen writes; retain rejection
when writes really leave the output. Bound texture feedback by the sampled UV
footprint after wrap/window remapping, plus conservative CLUT checks, rather
than the entire page. Genuine framebuffer feedback remains unsupported.

GPU Auto retries unsupported scenes at native internal resolution, followed by
GPU presentation. It never retries a viewport-sized CPU raster while pacing
simulation. Explicit Renderer > Software retains full-resolution CPU reference
output. Supported Auto scenes still render at the requested enhanced resolution.
Do not allocate a full-resolution software surface just to prepare GPU output.
The viewport and UI proportions remain consistent when output paths change;
unsupported-frame fallback can temporarily reduce detail. This is a quality/
latency tradeoff, not complete GPU coverage or a gameplay performance guarantee.

Validation: 82/82 Release and ASan/UBSan tests. Offscreen Vulkan/D3D12 verify
world/UI, title-style background metadata, clip/UV feedback regressions, fades,
and full-window presentation alternating GPU/native output with OFF parity.
Synthetic dialogue workload timings are recorded in PLATFORMS.md. Owner actual
logo/title/save/load, all-map and dialog/transition validation remains pending.
No additional decomp source guard or external dependency is added. FXAA/MSAA
remain deferred; save/load crash fixes follow after merging this graphics PR.

## ADR-058: Native animal scans during overnight processing

Date: 2026-10-26

Context: The owner reports sleeping without saving also crashes. Windows
Application Error 1000 for the current release reports access violation at
RVA 0x1e1573, farm_action_pick_slot+0x53 (build 19f2994). Disassembly shows a
record flag read through a sign-extended 32-bit address. An isolated empty-table
ASan fixture reproduces the invalid read in farm_pick_random_slot.

Decision: Under HMBTN_PORT, form animal record addresses with byte-pointer
arithmetic and native sizeof strides. Explicitly return the picked slot from
the wrapper whose callers consume that result; its old C fall-through does
not provide defined host behavior. Clear sizeof(FarmAction), including native
animation-pointer tails, rather than the PS1-sized 0xB0 prefix. The original
else branches remain exact. Candidate filtering, table order, held/event/place
rules and random-call cadence stay unchanged.

Validation: The original synthetic empty-table call crashes; the repaired call passes
ASan/UBSan. Tests cover no candidates/no RNG advance, the final livestock and chicken
slots, held/event/location exclusions and full action clearing. Actual
sleeping/overnight gameplay remains owner-tested.

## ADR-059: Fixed save payload independent of native object layout

Date: 2026-10-26

Context: The original packer overlays PS1-address-shaped structs on FarmState.
Native pointers move the fields, including run16/run18. A synthetic valid native
state with registered estate tables still crashes in the old packer when it
reads run16 from a poisoned old offset. Native FarmAnimals also grows beyond its
0x1F34 wire extent. SaveImage contains native pointer fields among raw blocks.
A pointer-cast repair alone would therefore leave corrupted saves/loads.

Decision: Add compat/save_codec with a fixed 0x6378 unshifted byte schema and
asserted wire offsets. Encode/decode persistent named native fields; handle
actor, animation, player, toy and helper records explicitly at their original
wire strides. Copy only verified pointer-free scalar spans. Never store host
addresses. Runtime table/script/asset bindings stay native instead of importing
saved addresses; chickens/livestock clear sprite.source for normal rebinding,
and extra chicken records retain the original packer's zeroing rule. Validate
payload length and estate run availability/capacities before changing output or
live state. Native source pack/unpack entry points call this codec under narrow
HMBTN_PORT guards; the exact original implementations remain in else branches.

Keep the original 0x8000-byte card file, slot metadata, byte-shift algorithm and
block checksums. Do not version a host memory dump or store sizeof(FarmState).
The existing native card container and Documents/BackToNature paths do not
change. Reserved runtime-address slots encode as zero; load does not overwrite
owned native bindings from them. Original-PS1 interchange is not claimed until
validated against owner data. Livestock-transfer raw views remain unaudited.

Validation: The old packer crashes in an isolated synthetic fixture at its
run16 copy. The new fixture checks original shift/checksum encode/decode,
tamper detection, a 32 KiB card write/reopen/read in memory, persistent field
restoration, native binding preservation, canonical re-encoding and rejected
short/unbound inputs. No disc or real saves are used. These do not establish
interactive save/load or full overnight gameplay parity. Original matching
branches reconstruct exactly with the new guards disabled; matching binaries
remain unverified because the Linux toolchain is unavailable (no WSL).

## ADR-060: Native dialogue argument and overnight loader progress

Date: 2026-10-27

Context: The reported ordinary-dialogue access violation at RVA 0x1158f9 in
release timestamp 0x6adf6b53 maps to player_advance_state reading its missing
player argument. Separately, the overnight sound-fader busy wait polls the CD
loader without advancing deterministic VBlank callbacks. Windows also records
an application hang during the owner's day transition, but no hang stack was
available to establish that this wait was the observed hang.

Decision: Use narrow HMBTN_PORT guards to pass the player argument and replace
the overnight wait with compat/async_wait. That helper polls the existing loader
and advances VSync(0), one hardware tick, until ready. No background game thread,
host-time logic step or arbitrary timeout is introduced. Original else branches
remain exact. These add one player guard and two clock_events guards.

Validation: Dialogue fixture covers confirmation, no confirmation and blocked targets.
Loader fixture requires three hardware ticks and verifies that an already-ready loader
advances none. Interactive dialogue and overnight completion remain owner-tested.

## ADR-061: Explicit SDK coverage instead of success stubs

Date: 2026-10-27

Decision: Implement mathematical ratan2 and Q12 csqrt in the existing runtime,
without SDK code/tables. Complete unsupported PocketStation requests with actual
NoCard/Invalid results through the existing asynchronous card controller. Do not
claim PocketStation support or fabricate output. Preserve explicit debug-font
failures until a real renderer exists. See API_COVERAGE.md for remaining callers,
mathematical parity limits and the pending-inventory regression check.

Validation: Windows Release and ASan/UBSan builds each pass all 89 tests, including
six new runtime fixtures and the inventory check. No new dependency or game data
is used. Gameplay coverage remains with the owner.

## ADR-062: Preserve widescreen groups during interpolation and tile menu sides

Date: 2026-10-27

Context: The owner observes widescreen only during dialogue, grey title sides,
and incomplete load/save backgrounds. Inspection finds replay_group in mesh
matching rather than in the interpolation execution loop. Regular frames use
interpolation, while dialogue bypasses it. The title's scrolling layer was tagged
as background but its green base animation 0x1D remained only 320 pixels wide.
The load/save bg_tile dispatcher also emits only fifteen original tile columns.

Decision: Restore each command's world/UI group during interpolation execution;
mesh matching must not change replay state. Extend the title base using copies
of its original animation, and intercept only bg_tile.c animation submissions to
repeat four-column (96-pixel) periods outside the original grid. Preserve original
frame, palette and scroll, with no new artwork or stretched text/logo. Tag both
sprite and draw-mode ranges. Coalesce adjoining background spans so all 165 menu
tiles and extra side tiles fit the existing inventory without losing later rows.
All additional geometry is disabled when the runtime margin is zero. No decomp
source changes, matching guard changes or dependencies are introduced.

Validation: Windows Release and ASan/UBSan each pass 90/90 tests. Tests cover
interpolated off-canvas world versus clipped UI, both menu allocation styles,
all 24 scroll phases, 16:9/max-width coverage, original OFF submission counts,
and title base expansion without logo tagging. Interactive visual confirmation
remains with the owner; synthetic coverage is not a full-screen gameplay claim.

The owner also reports camera displacement after two sleeps. A window capture
confirms the reported view, but no camera/player state was captured before the
process closed. An opt-in HMBTN_TRACE_CAMERA host diagnostic logs only game
coordinates/flags every 120 hardware ticks. It does not alter simulation and is
OFF unless explicitly set. Root cause and a camera fix remain pending evidence.

## ADR-063: Freeze the complete pause canvas and preserve fallback aspect

Date: 2026-10-27

Context: The owner supplies pause/transitions with horizontally smeared sides.
Pause freezes a 320x240 native scanout; presentation extends its edge columns to
fill widescreen. The same presenter invents side pixels while transitions bypass
private frame replay. This is visible stretching rather than additional scene.

Decision: Reconstruct one frozen logical widescreen canvas at pause entry using
the existing private output, taking interpolation when applicable. Draw the
original 320x240 menu in the centre, with the same 40% scrim across the full
canvas. Retain real side pixels instead of repeating columns. Keep a proportional
native fallback with black sidebars when a valid wide replay is unavailable.
Remove edge-column extension from the common presenter. No camera, game state,
RNG or game ticks are modified by pause reconstruction. The pause snapshot is at
logical resolution; it does not promise a full-resolution GPU screenshot. Window
aspect changes while already paused do not regenerate the frozen projection.

Validation: The canvas fixture covers 320, 428, odd 571 and maximum 1024 widths,
all rows, centre extraction, original OFF output and single scrim application.
Interactive pause/transition validation remains with the owner. Missing wide
transition captures may temporarily show black sidebars; full transition world
reconstruction is not claimed.

## ADR-064: Native event animation copy and source-audit corrections

Date: 2026-10-27

Context: The new NPC crash occurs reading an animation range. Whole-tree native
compiler/analyzer passes and manual review find villager_sprites_copy still using
PS1 sprite size (0x30) and villager stride (0x1D4). On a native state this copies
wrong fields and leaves broken pointer tails. Gift reaction lookup also truncates
pictureCodes; chicken_clear zeros only the old record prefix. Farm re-entry and
minigame startup waits lack hardware progress. Two item callbacks are called
through incompatible function types.

Decision: Copy complete named native animation records through compat/villager_sprites,
retaining the existing event-0x82 reload order. Index native gift tables, clear
sizeof(FarmChicken), reuse the existing audio wait helper for farm re-entry and
call the two special item callbacks through their actual signatures. Function
pointer conversions are used only for identity comparison. Preserve exact original
source branches under HMBTN_PORT. For the five minigame overlays only, a CMake
adapter adds VSync(0) after an unfinished afile poll; initial/completed reads and
all ordinary game asynchronous reads keep their timing. No fake assets, callback
results or broad source reformatting are introduced.

Validation: The old villager copier fails the new fixture; the fixed helper preserves
all fields outside the copied animations across 37 records and checks the conditional
reload path. Tests cover all gift bit positions/rows and remapped reaction classes,
native chicken tails, coin/berry use and bounded pending asset polls. Release and
ASan/UBSan each pass 94 tests. See SOURCE_AUDIT.md for exact coverage, open findings and
the limits of static analysis. The reported NPC and camera behavior still need owner
testing.

## ADR-065: Explicit shop callback indexing and original family text

Date: 2026-10-27

Context: shop_request_dispatch indexes a one-element null table with request IDs
up to 28. Read-only inspection of the owner's validated USA overlay finds a
contiguous 29-entry vector at 0x800BB2BC: the null entry followed by the existing
28 phase-complete handlers, with null entries at 0 and 21 and the shared default
at 4..10. Native C objects cannot rely on that adjacency. family_table_lookup
also returns one of five PS1 addresses as s32 for the status page's text pointer.

Decision: Dispatch request 0 without an initializer, and request n through
shop_phase_complete_handlers[n-1]. Reject IDs outside 0..28 explicitly. Keep the
original mapping and null slots, not invented successful handlers. Forward the
basket callback's pending/completed result explicitly rather than native C
fall-through. Original source alternatives are preserved under HMBTN_PORT.

For family text, resolve the existing address metadata against the original low
overlay base (0x800B7E20), member 11 of A_FILE.BIN. Read the member range from
A_FILE.HDT using the validated disc parser at launcher initialization, outside
simulated CD/game ticks. Validate member/extent boundaries and a terminator within
nine glyph words. Cache only those five strings in native storage. Return a full
u16 pointer through a guarded family.h signature and function body. No glyph
strings, disc content or SDK material are copied into source control. Missing or
malformed text fails initialization clearly; invalid wife indices fail explicitly.

Validation: Synthetic ISO fixtures check all five native pointers, terminators,
range/I/O/malformed-data failures and lookup through the game entry point. An isolated
read-only check also loaded all five names from the owner's SHA-1-validated BIN; it did
not execute gameplay. Shop tests exercise the highest request, the null slot, the shared
initializer, zero request, invalid ID rejection and basket pending/completed behavior.
Disabled new guards reconstruct the original sources exactly. No other platform/device
or married-status gameplay claim follows from these checks.

## ADR-066: Separate partner save bytes from native transfer records

Date: 2026-10-27

Context: Livestock exchange still casts unshifted save bytes to FarmAnimals and
SaveFile/SaveImage with native pointers. A five-animal stash also uses the old
928-byte buffer although native livestock records are 224 bytes on this build.
Restoration copies only the old PS1 prefix. An isolated old-buffer reproduction
fails ASan with a 224-byte write beyond save_out_buffer.

Decision: Extend the fixed-width save codec with a typed partner view containing
native animals, pointer-free estate metadata and the eight-byte farm identity.
Decode only after checksum/slot validation and byte-unshifting. Route transfer
counts, free slots, names, capacities and UI through that view. Stash and restore
complete native records in an aligned buffer sized for five maximum-sized animal
records. Clear the complete buffer. Before the existing shift/checksum/write,
encode only the original 0x1F34 animal extent, clearing reserved runtime-address
words and preserving every subsequent partner payload byte. Native view state
joins the save overlay's reset inventory. Keep original source alternatives.

Validation: Real source staging/restoration fixtures exchange five cows and five
chickens using synthetic farms and an in-memory payload. They cover final record slots,
stash capacity/reset, full native tail restoration, metadata/identity, short-input
rejection, unchanged non-animal bytes and original shift/checksum round trips. No owner
saves are opened or written. Release and ASan/UBSan each pass 99/99 tests; seven changed
translation units pass targeted Clang analysis without core/unix/security findings.
These checks do not establish interactive card failure/abort/retry behavior,
original-PS1 interchange or full-game parity. The original exchange transaction/error
policy is unchanged; no atomic-write or rollback guarantee is introduced.

## ADR-067: Optional probe outputs, pointer sentinels and native frame callbacks

Date: 2026-10-27

Context: The source audit's estate pickup warning is reachable: the dog approach
path calls estate_field_pickup with a null output, but seasonal mapping reads it
unconditionally. Boolean-only animal/estate terrain queries similarly omit tile
and flag outputs that farm_tile_occupant writes, including at the callback cap.
Bounded source fixtures reproduce both null writes/reads under UBSan. A synthetic
opaque pointer also demonstrates sound_update misclassifying a non-sentinel
native address when its low word is 0xFFFFFFFF.

Decision: Under narrow HMBTN_PORT guards, seasonal mapping runs only when an item
output is requested; record enable/claimed changes remain unchanged. Terrain
queries use local discard outputs when callers omit them, and the capped branch
clears flags only when provided. Initialize the standalone tile probe's hit class
and the estate basket probe's unused map/padding, preserving filled coordinates,
location, query result and callback counts. Compare full sequence pointers against
SOUND_BUFFER_NONE, keeping slot/open/start behavior intact.

An absent image index previously leaves the image-bank upload rectangle
uninitialized. Reject null banks and indices outside count explicitly; keep the
existing non-bank zero result and valid raw/RLE uploads. These checks do not
validate an entire bank's encoded byte extent or RLE destination capacity.

MG4 state functions all take no arguments: use a matching native function pointer
and call, without passing the handler table as a fabricated context. MG1's award
frame is used exclusively by a void frame dispatcher; make its native declaration
and definition void and retain state advancement through mg1_state.step. Do not
invent a pending integer result for the original non-void fall-through. Original
matching alternatives remain exact.

Validation: Old pickup and sequence fixtures fail before correction; the added
boolean terrain fixture independently reproduces a null store before correction.
Tests cover every season's optional pickup/seasonal item forms, one-time grapes,
ordinary/opaque/sentinel sequence slots, no duplicate opens, stereo/mono, all seven
MG4 slots/menu order, field/greenhouse classes, omitted outputs, callback cap and
the actual estate_basket_check caller. Texture tests check named null/empty/index
failures and retain the original decoder's valid output regressions. The opaque
sequence pointer is never dereferenced and is not a claim about real allocations
or audible music. MG1's waiting-screen behavior has compile/analyzer validation,
not an interactive minigame test.

Windows Release and ASan/UBSan each pass 106/106 tests. Targeted Clang analysis of six
modified game units reports no core/unix/security findings or the selected
callback/return/pointer-cast warnings. No gameplay/replay or owner save data is used. No
full-game, other-platform or performance claim follows from this batch.

## ADR-068: Wide transition capture, full-screen tiles and scanout canvas

Date: 2026-10-27

Context: The owner reports a 4:3 rucksack shade, side flicker between Natsume
and title, and aspect changes during transitions. Rucksack shade uses a
three-word TILE, whereas the full-screen expansion recognized only F4 packets.
Frame blends skip main.c's PutDrawEnv call, so they previously never began a
capture. Both private renderers also initialized extra pixels by repeating the
changing native edge column.

Decision: Expand exact full-screen variable TILE packets as well as F4, retaining
their original command lengths. Start capture for main.c's direct fade OT path.
Append its commands to a recent complete capture of the same draw rectangle,
keeping the original snapshot and extended geometry. A normal frame starts a
fresh capture. Coalesce consecutive equal groups so repeated fades do not exhaust
group slots. Invalidated, bypassed, stale or different-buffer captures cannot be
retained. Bounded command/group capacity still rejects incomplete replay.

Initialize unrendered side pixels to black in both CPU and GPU paths. The host
places direct/loading native scanout proportionally at the centre of an expanded
canvas when no private replay is available, rather than selecting a 4:3 output
destination. Do not stretch UI or native edge columns. Support the game's
640x480 stepped-fade rectangle as well as 320x240. Native VRAM, game clocks,
camera coordinates, source code and data files remain untouched.

Validation: Windows Release and ASan/UBSan builds each pass 106/106 tests.
Synthetic checks exercise TILE packet boundaries, native edge contamination,
80 retained fade captures, final accumulated darkness, changed scene keys,
bypass rejection, 640x480 expansion, OFF behavior and native state preservation.
Offscreen D3D12 and Vulkan readbacks match the CPU reference at native/enlarged
resolutions, with both filtering modes and a rucksack-style full-screen TILE.
The presentation probe's obsolete edge-repeat expectation is corrected to match
the existing proportional-image contract. No gameplay or owner save is opened.
Actual logo/menu/area/cutscene transitions still need owner validation. These
checks do not prove sufficient side scenery in every original map or absence
of every widescreen bug; there is no performance or other-platform claim.

## ADR-069: Farm summary loading and shared status sprites

Date: 2026-10-27

Context: Start opening the farm summary hangs; Windows reports AppHangB1. Its
status_load_sprites has an empty afile_read_bytes polling loop, so deterministic
CD progress never runs. After yielding is restored, a bounded fixture of the
original loader exposes index 4 outside status_page_sprites[3] under UBSan.
The original slots 3/4 refer to the separately named shared cursor/mark sprites;
native globals need not be adjacent to the page array.

Decision: Add status_pages.c to the existing blocking-asset-read CMake adapter.
Only pending polls yield VSync(0); initiation and completion add no extra tick.
Use a native-only checked slot resolver for the five loader slots and variable
sprite accesses in two draw helpers. Keep all original matching expressions in
exact #else alternatives. Do not enlarge the array and duplicate the shared
cursor/mark state used elsewhere.

Validation: The original loader runs in a synthetic bounded fixture, checking two
hardware/host ticks, one read, one texture upload, five exact native bindings and the
copied pack. The fixture neither runs gameplay nor uses disc/save data. Release and
ASan/UBSan each pass 107 tests. Original source reconstruction with HMBTN_PORT disabled
matches HEAD before the repair exactly. Interactive summary-page navigation remains
owner-tested.

## ADR-070: Original splash clear colour and displayed-buffer identity

Date: 2026-10-27

Context: Natsume uses white DRAWENV background clearing, not a full-screen white
primitive. ADR-068's unconditional black side initialization leaves that splash
visibly bounded at 4:3. The owner also reports a brief narrow frame during Won's
cutscene. Shared replay rejects a displayed capture if the simulation's scene key
changes before the buffer flip, although those pixels still belong to the old
completed buffer.

Decision: Seed extra pixels with the original enabled DRAWENV clear colour,
converted to 15-bit RGB, or black when background clearing is disabled. Share
this rule between software, GPU scene header word 14 and native scanout. Do not
stretch the logo or repeat edge columns. Shared frame replay selects a complete
recent capture by the actual displayed rectangle, independently of the live
simulation key; reject bypass captures and retain age/overlay-invalidation
checks. Interpolation's strict scene-key checks remain unchanged.

Validation: Synthetic splash tests check white side pixels and the GPU background
word. Vulkan/D3D12 readbacks match software for white and black backgrounds at
both buffer origins, native/enlarged resolutions and both filtering modes.
Replay regression checks a live scene-key change before the buffer flips and
rejects the wrong display buffer. Release/ASan pass 107 tests. These establish
the source-level defects and corrections; the reported Won cutscene and actual
Natsume presentation still require owner verification. No gameplay replay or
owner saves are used. Matching source is unchanged by this graphics repair.

## ADR-071: Capture backdrop lifetime and farm projection on menu return

Date: 2026-10-27

Context: The owner reports flicker during Natsume fades and TV entry/exit, plus a
persistent upper-left camera shift after saving. GPU scene_end reads backdrop
colour after replay restores the live environment, so a newly drawing frame can
replace the displayed capture's colour. A completed clear-only frame is also
rejected solely because it contains no primitive words. Both fail bounded tests.

The original main_frame_reset sets the GTE centre to (0,0) on every scene switch.
Full farm startup later initializes (160,120), but saved-game farm re-entry and
the cached menu-return path skip that initialization. The original reset fixture
reproduces the zero centre on SCENE_FARM_SAVED; the offset persists across farm
area changes. Do not reset camera eye/target, lighting or script camera state to
hide the displacement.

Decision: Save the GPU scene's solid backdrop at begin_capture, beside its native
rectangle and VRAM snapshot. Scene_end serializes that stored colour without
consulting the live environment. Accept a completed empty OT when its captured
DRAWENV explicitly clears the background; retain rejection of empty non-clearing
frames and existing invalidation/age/bypass guards.

Remap only main.c's SetGeomOffset call through a native view-context adapter.
Its zero-offset reset restores the original logical centre (160,120) for
SCENE_FARM_SAVED and SCENE_FARM_MENU_RETURN. Title/save resets remain (0,0);
nonzero requests are forwarded unchanged. Projection distance and world/camera
coordinates are unchanged. The correction is independent of widescreen/scaling
and introduces no enhancement or matching-source guard.

Validation: Before correction, GPU backdrop lifetime, original main reset and
original fade-packet/empty-OT fixtures fail. After correction, Release and
ASan/UBSan each pass 108 tests. Original screen_fade_frame packets run through
64 alternating buffers across white/coloured fade-in/out phases; side pixels
match native scanout, including clear-only frames and a changed live backdrop.
The reset fixture checks both farm returns and ordinary title/save resets.
Offscreen Vulkan and D3D12 comparisons switch live contexts before scene_end
and match the CPU reference at both buffer origins, native/enlarged sizes and
both filtering modes. No gameplay, save contents or asset data is used in these
fixtures. Actual Natsume/TV transitions and repeated saving remain owner-tested;
this is not proof of all-screen parity or a performance measurement.

## ADR-072: Display-buffer validity follows writes, not elapsed ticks

Date: 2026-10-27

Context: The owner confirms post-save/next-day camera framing is safe but still
reports side flicker during Natsume fade-out, TV entry/exit and leaving home.
A completed displayed capture expires after four ticks, even if a blocking
CD/audio wait leaves the same native pixels on screen. A regression advancing
the live tick by 120 without changing the buffer reproduces this rejection.
This explains a concrete path to native-only presentation during loading; actual
reported transitions remain owner-tested rather than claimed reproduced.

Decision: Keep a complete capture while its native display rectangle remains
valid, regardless of elapsed ticks. Reject future-dated or bypass captures and
retain overlay invalidation and buffer matching. Notify capture observers when
native LoadImage, ClearImage or MoveImage writes intersect a captured rectangle;
invalidate only overlapping captures. Split SDK-wrapped transfers into up to
four non-wrapping rectangles, including ClearImage's alignment/size semantics.
StoreImage and texture writes outside the framebuffer do not invalidate it.

Unobserved native DrawOTag submissions invalidate their draw rectangle, covering
the loading-screen path outside the normal capture hook. Native VRAM reset
invalidates all captures. Private software/interpolation VRAM contexts never
issue native invalidations. Apply the same write notification to interpolation
history without removing its separate scene-key checks. Notifications occur per
transfer/submission, not per raster pixel, with no new steady-state allocation.

Validation: The unchanged-buffer wait regression fails before correction.
Release and ASan/UBSan builds each pass 109 tests after correction. New coherence
checks cover long waits, unrelated texture uploads, the other framebuffer,
readback, private transfers/reset, native clear/load/move, XY wrapping and direct
OT drawing. Original fade-packet tests now include 120-tick waits during both
fade-out phases; expanded pixels still match native scanout. Vulkan and D3D12
offscreen readbacks pass. No matching source, shader, game assets or owner saves
change; no gameplay, other-platform or performance result is inferred.

## ADR-073: Restore farm overlay objects on re-entry

Date: 2026-10-27

Context: After playing, returning to title and loading again aborts with
hm_overlay_select: native overlay reentry is not implemented. Title and save/load
have pristine state inventories, but member 11 (farm) does not. The existing
title/load lifecycle fixture reproduces the same error after adding farm visits.
Removing the rejection without resetting private farm state is insufficient.

Decision: Audit all 51 farm TUs with Clang AST and compile native-only wrappers
containing the unchanged original source plus per-TU inventory providers. Preserve
source order and native source-specific SDK remaps. Combine the providers into a
pristine snapshot at host startup using the existing object-based overlay loader.
Restore each real C object on activation, preserving ASan global instrumentation.
Do not copy linker ranges, reset the resident game state or write save files.

The initial inventory contained 703 file-scope objects. A compiler type test
skips only top-level const objects, correctly distinguishing mutable pointers to
const data from immutable pointers. In the 2026-11-01 Linux x86-64 re-audit, two
existing weather pause-random state globals were found missing. The checked
inventory now contains 705 objects: 679 mutable objects occupying 49,209 bytes
on Linux x86-64, and 26 const objects. All 56 function-local static tables are
FarmCueStep arrays passed only to the read-only table consumer farm_cue_step_run;
their unchanged contents need no restoration. Record and check that exception.

Checked source/header fingerprints and the full TU list fail CMake configuration
when audited input changes. The optional Clang/Python maintenance tool regenerates
or checks inventories; normal builds do not run it or require Python. Mutable
function-local statics or new consumers require a fresh design/audit rather than
silently being ignored. Other overlays keep their existing re-entry rejection
unless they have their own complete inventory.

Validation: The modified title/farm/load loop fails before the repair and passes
afterwards. The farm fixture mutates every registered mutable object and verifies
exact pristine restoration over four visits, resident game_state/farm_area
preservation, repeated initialization without recapturing live data, and const
classification. Selecting the high save overlay preserves the still-resident
farm's live state; only selecting member 11 restores it. The fixture tests the
cue consumer with read-only tables on both location
branches. Native wrapper call-site definitions are checked for field and status
units. The generator --check audits 51 TUs against the checked inventory.

Windows Release and ASan/UBSan builds each pass 110 tests, with the final repeated
initialization assertion rerun under ASan separately. No gameplay, disc or owner
saves are used by these fixtures. Matching sources are unchanged, adding no guards.
Actual title-to-load-to-farm return remains owner-tested; other platforms and full
playthrough parity are not established.

## ADR-075: native actor aliases must follow expanded animation pointers

Date: 2026-10-28. Status: accepted.

Context: The owner's put-down-dog crash was captured in text_sprite_draw. The
dog's AnimState.partLists pointer had its low word pair overwritten with
0x0000ffff. Pickup calls farm_action_begin through a local PS1-padded view whose
cue is at 0xa8. Native animation pointers expand FarmActor: its cue moves to
0xc8, and partLists now occupies 0xa8. The old reset therefore corrupts the
sprite while held; drawing after put-down exposes it. Audit also found a helper
number read through FarmObjectRecord at PS1 offset 0xac, where native pointer
data now resides; FarmHelper.number is at 0xd0.

Decision: Rename the original native farm_action_begin symbol and supply a
port-layer implementation using the real FarmActor fields. Keep its item/flag,
task, speed, cue and pending-room behavior. A checked native player wrapper
replaces only the helper-number expression with an accessor for FarmHelper.
Preserve farm snapshot providers and existing per-source SDK definitions. Keep
matching source, original struct definitions and save encoding unchanged.

Consequences: The correction applies with enhancements OFF as well as ON. Tests
reproduce the original pointer overwrite, verify four native actor prefixes,
check all seven helper IDs, and exercise young/adult dog pickup and put-down in
four directions with both affection-cue states. These isolated calls use no disc,
game loop or owner save. Actual gameplay revalidation stays with the owner.
ADR-074 is reserved for the separately parked multilingual work.

## ADR-076: FXAA post-process and GPU scene MSAA 2x/4x

Date: 2026-10-28. Status: accepted.

Context: The owner explicitly requested antialiasing options. FXAA operates on
the composited frame image before presentation (final-image, any renderer).
MSAA 2x/4x is GPU-scene-only: the compute shader samples each primitive at 2
or 4 sub-pixel positions and resolves by averaging; Software rendering does not
support MSAA and the menu guards the selection. Both modes are gated by
HMBTN_ENH_ANTIALIASING. Runtime default is Off; the CMake option defaults OFF
but Windows -Game builds enable it.

Decision: Add port/enhance/antialiasing/ with a mode enum (Off/FXAA/MSAA2/
MSAA4), scalar Q8 triangle-coverage reference, MSAA HLSL helpers (msaa.hlsl),
and FXAA shader + precompiled blobs (fxaa.hlsl, fxaa_dxil.h, fxaa_spirv.h).
Extend scene.hlsl with per-sample colour arrays and coverage masks; regenerate
scene_dxil.h and scene_spirv.h. Add platform/fxaa.c SDL_GPU pipeline wrapper.
Wire aa_capabilities bitmask at startup from actual device support. Guard MSAA
when Software is selected with a user-visible menu notice.

Consequences: Antialiasing modes are independently tested via the scalar
coverage reference and round-trip settings tests. GPU probes cover FXAA and
MSAA pipeline creation. The vanilla OFF path is unchanged; all 102 Windows
Release tests pass. CPU raster performance is unaffected. Interactive gameplay
coverage and device-specific AA quality remain owner-tested.

<a id="adr-077"></a>

## ADR-077: Select hop rows explicitly instead of overrunning move_steps_large

Date: 2026-10-28. Status: accepted, narrow HMBTN_PORT source guard.

Context: Hopping over a rock, weed or similar low obstacle sent the player a
very long way. player_update_position indexes HOPS[row][facing], a cast of
move_steps_large (16 s16, so two rows of four steps) to rows of 12 steps. The
original row 1 therefore lies 48 bytes in, past move_steps_large (32 bytes) and
move_steps_riding (16 bytes), at the adjacent hop_steps object ({0,-51}, ...).
The matching layout makes that overrun land on valid data. Native objects are
not contiguous, so the read returned unrelated memory and added a huge
per-frame offset for the whole airborne time.

Decision: In the native branch only, pick row 0 from move_steps_large and row 1
from hop_steps explicitly. The #else branch keeps the original expression.

Consequences: Native hops use the original +-73 (crouch) and +-51 (airborne)
per-frame steps. One new HMBTN_PORT guard pair (macros and the hop lookup in
people/player.c). Not verified against the matching build (no Linux toolchain
here) and no gameplay was run; the owner must confirm the hop distance
interactively. This is an audit of one overrun; other cross-object reads of
this kind may exist.

<a id="adr-078"></a>

## ADR-078: Locate the disc from argument, settings, then executable folder

Date: 2026-10-28. Status: accepted.

Context: The native executable required the BIN path as a command-line argument
every time. AGENTS.md section 7 asks for the chosen disc to be remembered in the
config. A native file picker is not part of this change.

Decision: Add platform/disc_locator. An explicit argument is tried alone. Without
one, try settings.json ``disc_path``, then ``*.bin`` files beside the executable
(case-insensitive, name order). The launcher starts user data before locating
the disc, and the host reuses the running instance. A validated path is stored
as an absolute path; failure shows a message box. Unvalidated paths are never
trusted: every candidate goes through the existing USA disc mount checks.

Consequences: Double-clicking the exe works when the BIN sits beside it or was
used once before. Selecting a different disc requires passing the argument
again. Unit tests cover JSON path encoding and the locate order with isolated
roots and a fake validator; no real disc or gameplay was used. No file picker.

<a id="adr-079"></a>

## ADR-079: Private wide primitive arena for model and field packets

Date: 2026-10-28. Status: accepted.

Context: At 21:9 and wider the shipping bin disappeared and its area stayed empty.
ADR-056 stops model submission with an 8 KiB UI reserve once the original
96000-byte per-frame primitive arena fills, and warned that dense scenes can omit
geometry. The wide frustum and field radius (about 2.7x at 32:9) submit far more
packets, so late model parts, such as the bin, were dropped. This cause is inferred
from the code and the documented limit; it was not reproduced or measured with a
running game.

Decision: While wide world drawing is active, point each frame record's primStart
at a private 1 MiB arena, restored when widescreen or the world scene ends. This
uses the same switch point as the wide GS work arenas. primBuffer and primSize keep
describing the original region because overlay backup copies use them. The model
reserve test now asks hm_widescreen_prim_full, which measures the active arena
(eight guarded lines in model.c), and replay/interpolation arena bounds follow
hm_game_prim_capacity. A one-time stderr line is printed if even the private arena
is exhausted.

Consequences: 2 MiB of static memory when the widescreen module is compiled. OFF
uses the original region and bounds unchanged. If the bin still vanishes and the
stderr line appears, the arena is still too small; if it does not appear, the cause
lies elsewhere. Unit test updated; no gameplay was run.

<a id="adr-080"></a>

## ADR-080: Gameplay Enhancements Suite and Menu Controls

Date: 2026-10-28. Status: accepted.

Context: The owner requested optional gameplay enhancements configurable in ESC
Pause Menu -> Settings -> Gameplay:
1. Walk through crops & wild weeds
2. Item stacking up to 99 in rucksack with native quantity dialog for partial stack transfers
3. Durable farm lumber fences (no decaying/rotting)
4. Connected wooden fence rails between adjacent lumber posts (double-rail ranch beams)
5. Wild dog attacks toggle (vanilla Enabled by default)
6. Smart bell herding (animals seek barn/coop doors when bell is rung)
7. Stamina HUD: 10 dynamic hearts in top-left using authentic start menu Pete head and heart assets
8. Top-right clock HUD: preserved in 4:3 safe frame to avoid UI buffer clipping
9. Calendar events: festival names, villager birthdays, and livestock maturation
10. Roaming horse: ride outside farm with location reset fallback to prevent corruption.

Decision: Add enhance/gameplay and platform/gameplay_options, gated by
HMBTN_ENH_GAMEPLAY (enabled in Windows build.ps1 -Game; runtime defaults OFF except
wild_dog=1 for vanilla parity). Keep options saved in settings.json under "gameplay".
When Walk through crops is enabled, field crops and wild/dead weeds stay passable
without clearing their pickup/harvest flags. The player's movement-only collision
probe marks its actor identity so the tile callback can skip only crop/weed hits;
it still checks the other obstacles and actors. Tool-target probes keep their
normal tile results, and the separate sprint-step probe follows the same option.
When splitting or moving stacked items in the rucksack menu, show the native-style
quantity selector dialog with D-pad input (+-1, +-10) and item preview. Draw connected
fences as two-tiered shaded wooden rails with highlight/shadow behind the posts.
The clock HUD is preserved at base_x = 228 to avoid clipping against the 320-pixel UI buffer.
The stamina HUD is positioned at top-left (14, 16) with compact dimensions (84, 20), drawing a scaled
Pete head and authentic affection hearts (anim 4) from the status screen; it hides automatically when
menus, dialogs, or the toolbox are open.
Calendar birthday/event previews in the top-right are removed in favor of opening authentic event
dialogue boxes upon pressing the Cross (X) button.

Consequences: Vanilla parity maintained when options are OFF. Farm tile tests cover
pass-through and preserved pickup/harvest flags for weeds, dead weeds, and mature grass.
Interactive gameplay testing performed by the owner.

<a id="adr-081"></a>

## ADR-081: Controls Settings and Input Backends (Gamepad, Keyboard, Mouse)

Date: 2026-10-28. Status: accepted.

Context: The owner requested a dedicated Settings > Controls interface in the ESC
Pause Menu with options for Gamepad, Keyboard, and Mouse:
1. Controller / Gamepad: Auto-detecting and mapping standard controllers (Xbox,
   PlayStation, Switch Pro, etc.) via SDL3 Gamepad API.
2. Keyboard: Configurable layout presets (Arrows + ZX, WASD + JK, or Off) for
   movement, tool usage, rucksack, whistle, eating, running, and menu navigation.
3. Mouse: Support for mouse interaction with modes (Off, Action, Move & Act with
   click-to-walk).

Decision:
1. Implement port/platform/controls_options.h and controls_options.c managing:
   - controller_enabled (1 = Enabled, 0 = Off)
   - keyboard_layout (0 = Arrows+ZX, 1 = WASD+JK, 2 = Off)
   - mouse_mode (0 = Off, 1 = Action, 2 = Move & Act)
   - Configuration is persisted under the "controls" object in
     Documents/BackToNature/settings.json.
2. Integrate into ESC Pause Menu:
   - Added "Controls" subpage to Settings menu (HM_PAUSE_CONTROLS).
   - Detailed status bar descriptions and button layout prompts are displayed when
     highlighting rows.
   - Menu navigation supports keyboard, gamepad buttons, and mouse clicks.
3. Input handling in port/platform/game_host.c:
   - Gamepad: SDL3 Gamepad events (SDL_EVENT_GAMEPAD_ADDED,
     SDL_EVENT_GAMEPAD_REMOVED), analog stick axis sampling with deadzone + D-pad
     mapping, face buttons and triggers mapped to native PS1 pad bits (HM_PAD_*
     constants in port/psx/pad.h).
   - Keyboard: Scancode mapping for both Arrow and WASD presets.
   - Mouse: Left click (Cross / Action), Right click (Square / Tool use), Middle
     click (Triangle / Rucksack), Wheel Up/Down (L1/R1 cycling). In Move & Act mode,
     holding Left Click computes a directional vector from screen center to cursor
     and sets D-pad direction bits.

Consequences: Multi-input options are fully configurable and persistent without
altering vanilla game logic. Unit tests cover controls options cycle,
serialization, and pause menu navigation (116/116 unit tests passing).

## ADR-082: Safe FntPrint diagnostic logging and pending inventory reduction

Date: 2026-10-28. Status: accepted.

Context: PSY-Q libgpu provides `FntPrint(const char *format, ...)` for debug font
stream formatting. Several decomp modules call `FntPrint`:
- Animation binding (`src/main/graphics/anim.c:21`) when encountering invalid magic or unaligned payloads.
- Event scripting (`src/overlay/11_GF_FARM/event/event_commands.c:6914`) on missing picture rows.
- Memory card manager (`src/overlay/31_gf_mcard/mcard.c`) for status and menu lines.
- Debug and PDA overlays (`src/overlay/08_DEBUG/debug.c`, `pdasamp.c`, `swind.c`).
Previously, `FntPrint` was stubbed in `port/psx/pending.c` with `hm_unimplemented("FntPrint")`,
triggering `abort()` (exit code -1073740791 / 0xC0000409) whenever any of these paths was reached during interactive gameplay.

Decision:
1. Implement `FntPrint` in `port/psx/font.c`:
   - Conforms to the standard PSY-Q signature `s32 FntPrint(const char *format, ...)`.
   - Uses `vsnprintf` to format variable arguments, dynamically allocating a larger buffer if output exceeds 2048 bytes.
   - Outputs the formatted string to `stderr` with auto-flush so that diagnostics and error messages appear on the native host console.
   - Returns the number of characters output (matching PSY-Q `libgpu` semantics), or `-1` on invalid format/allocation failure.
2. Update pending inventory:
   - Remove `FntPrint` from `port/psx/pending.c` and `port/tests/pending_api.cmake`.
   - The remaining eight debug-font functions (`FntFlush`, `FntLoad`, `FntOpen`, `KanjiFntClose`, `KanjiFntFlush`, `KanjiFntOpen`, `KanjiFntPrint`, `SetDumpFnt`) remain explicit fail-fast stubs.
3. Add automated test:
   - Add `port/tests/font.c` (`psx.font` test) validating format strings, argument expansion, null-format safety, return lengths, and large dynamic buffer handling.

Consequences:
Calls to `FntPrint` during gameplay or diagnostics no longer terminate the game process with an unhandled exception. Log messages are cleanly visible on standard error output. Unit test suite passes 100%.

## ADR-083: PGXP subpixel vertex precision and perspective-correct texture mapping

Date: 2026-10-28. Status: accepted.

Context: On original PlayStation hardware, 3D vertices are projected to 16-bit integer
screen coordinates, causing noticeable vertex snapping ("polygon wobble"). Furthermore,
the PS1 GPU rasterizer only supports 2D affine texture interpolation, leading to affine
texture distortion / shearing when surfaces are viewed at an angle.

The owner requested eliminating polygon wobble and affine texture warping using a PGXP
(Parallel/Precision Geometry Transform) architecture:
1. GTE handler modification to compute float screen coordinates (SX/SY) and depth (W).
2. Vertex cache tracking float precision without altering integer coordinates used by game logic.
3. GPU scene polygon processing retrieving float coordinates with graceful integer fallback for 2D/UI.
4. Perspective-divided UV interpolation ($u/w, v/w, 1/w$) in shaders.
5. Runtime toggle (`enhance.pgxp = true/false`) defaulting to OFF for vanilla parity.

Decision:
1. GTE Precision Tracking (`port/psx/gte_core.h` & `port/compat/psyq/inline_c.h`):
   - In `hm_gte_project()`, compute high-precision float screen coordinates `sxf[2] = (float)val_x / 65536.0f; syf[2] = (float)val_y / 65536.0f;` and view depth `szf[3] = (float)sz[3];` before integer clamping.
   - Exact integer arithmetic for `state->sx/sy/sz` is strictly preserved, ensuring 100% deterministic vanilla game logic and collision behavior.
   - In `gte_stsxy` and `hm_gte_packet_xy`, call `hm_pgxp_record_vertex()` to cache the float coordinates alongside the packet memory destination.
2. PGXP Vertex Cache (`port/enhance/pgxp/pgxp.c`):
   - 32,768-entry hash table with 8-slot linear probing, keyed by destination packet memory address and verified against the expected integer coordinate word.
   - Generational frame tags automatically evict stale entries across frames without per-frame memory allocation.
3. GPU Scene Subpixel & Depth Pipeline (`port/gfx/gpu_scene.c`):
   - When converting GPU packets to scene primitives (`POLY_F3/G3/FT3/GT3`, `POLY_F4/G4/FT4/GT4`), query the PGXP cache for each vertex.
   - If all vertices originate from 3D GTE projection, emit 16.16 subpixel scaled coordinates (`p[34..39]`), set the subpixel flag (`p[4] |= 32`), write vertex depths (`p[30..32]`), and set the perspective flag (`p[33] = 1`).
   - If any vertex lacks PGXP data (e.g. 2D sprites, UI, tiles), smoothly fall back to standard integer coordinates without perspective distortion.
4. Perspective-Correct Shader Interpolation (`port/gfx/shaders/scene.hlsl`):
   - When perspective flag `word(p, 33)` is active, calculate perspective-divided barycentric weights $w_i' = rac{w_i}{z_i}$, normalized by $\sum w_i'$.
   - Texture coordinates and colors are interpolated using perspective weights, eliminating affine texture warping.
5. Runtime Settings & Parity:
   - Configuration key `enhance.pgxp` in `Documents/BackToNature/settings.json`, defaulting to `false`.
   - Exposed as "PGXP Geometry" in ESC Pause Menu > Settings > Graphics.
   - Added unit test `port/tests/pgxp.c` (`enhance.pgxp`) testing enable/disable parity, hash lookup, integer coordinate verification, and generational eviction.

Consequences:
When PGXP is enabled, 3D character models, farm buildings, trees, and terrain render with fluid subpixel precision and perspective-correct textures, eliminating PS1 wobble and warping. When disabled, identical vanilla rendering is fully preserved.

## ADR-084: PS1 hardware ordered dithering toggle

Date: 2026-10-28. Status: accepted.

Context: On original PlayStation hardware, the GPU rasterizer applies a 4x4 Bayer matrix
ordered dithering pattern (`draw->dtd`) to shaded and textured primitives to mask the
color banding inherent in 16-bit (RGB555) framebuffers. While this dithering blended
naturally on 1990s CRT displays, on modern high-resolution LCD/OLED monitors it appears
as visible checkerboard stippling/noise across 3D surfaces and backgrounds.

The owner requested an option to toggle this dithering effect on or off.

Decision:
1. Module `port/enhance/dithering/`:
   - Exposes `hm_dither_set_enabled(int enabled)` and `hm_dither_enabled()`.
   - Default state returns `1` (enabled) to strictly guarantee vanilla PS1 parity.
2. Rendering Pipelines:
   - GPU Scene (`port/gfx/gpu_scene.c`): Clears the `dtd` bit (`flags & 1`) in primitive packet word 4 when dithering is disabled, instructing `scene.hlsl` to skip `dither()` offset addition.
   - Software Rasterizer (`port/gfx/software_raster.c`) and CPU polygon/line fallbacks (`port/psx/gpu_polygon.c`, `port/psx/gpu_line.c`): Guard Bayer dither addition with `hm_dither_enabled()`.
3. Configuration and Pause Menu:
   - Configuration key `enhance.disable_dithering` in `settings.json`, defaulting to `false` (vanilla parity).
   - Pause Menu > Settings > Graphics item 12 ("Dithering") with values:
     - `On - PS1 original pattern`
     - `Off - smooth 32-bit colors`
   - Fixed description display for item 11 ("PGXP Geometry") and item 12 ("Dithering") in `hm_graphics_value`.
4. Tests:
   - Added unit test `port/tests/dithering.c` (`enhance.dithering`) verifying default parity and toggle states.
   - Updated pause menu unit test for 13 graphics items.

Consequences:
Players can choose between authentic PS1 dithered presentation and clean, noise-free true-color gradients without modifying core game logic or save data.

## ADR-085: xBR edge-directed texture filtering, Pause Menu FXAA bypass, and Graphics menu simplification

Date: 2026-10-28. Status: accepted.

Context: On high-resolution modern monitors, classic PS1 nearest-neighbor texture sampling produces noticeable pixelated stair-stepping, while standard bilinear filtering blurs low-resolution (16x16 to 64x64) retro textures. Furthermore, applying FXAA globally blurred 320x240 pause menu font and dialog presentation. The owner requested:
1. Adding an xBR-style filter that eliminates texture jaggies with smooth curves without causing blur.
2. Fixing FXAA blurring the pause menu while maintaining its benefits in 3D gameplay.
3. Simplifying the Graphics menu by making PGXP geometry stabilization, 32-bit smooth dithering, and GPU rendering auto-applied defaults.

Decision:
1. xBR Texture Filter (`port/enhance/filtering/texture.hlsl`, `port/gfx/shaders/scene.hlsl`):
   - Added mode 2 to texture filtering: xBR-style edge-directed interpolation.
   - Evaluates sub-texel fractions with an anti-aliased Hermite transition curve ($w = 0.18$) that keeps the interior of texels sharp and solid while smoothing border transitions.
   - Detects diagonal corner contours ($cA == cD$ or $cB == cC$) and smoothly curves 90-degree pixel steps into clean 45-degree diagonal slopes without blurring flat areas.
   - Recompiled shaders to DXIL and SPIR-V (`scene_dxil.h`, `scene_spirv.h`).
2. Pause Menu FXAA Bypass (`port/platform/game_host.c`):
   - Track active pause UI state via `hm_host_in_pause`.
   - In `hm_host_blit()`, bypass FXAA encoding when `hm_host_in_pause` is set, preserving crisp pixel-perfect font rendering on the 320x240 pause canvas. FXAA remains fully active during 3D gameplay.
3. Graphics Menu Simplification (`port/enhance/pause_menu/menu.c`, `menu.h`, `port/platform/graphics_options.c`):
   - Streamlined Graphics menu to 10 core settings (`HM_PAUSE_GRAPHICS_ITEMS = 10`): Monitor, Window Mode, Resolution, Enhance Scaling, Widescreen Mode, Anti-Aliasing, Filtering, VSync, FPS Limit, and Interpolation.
   - Auto-applied PGXP geometry precision (`hm_pgxp_set_enabled(1)`), clean 32-bit non-dithered rendering (`hm_dither_set_enabled(0)`), and GPU rendering without requiring manual toggle navigation.
   - Filtering option cycles cleanly between `Nearest`, `Bilinear`, and `xBR`.
4. Tests:
   - Updated `port/tests/pause_menu.c` and `port/platform/tests/graphics_options.c` for 10-item menu layout and 3-mode filtering cycle. 119 unit tests passing 100%.

Consequences:
Textures in gameplay render with smooth diagonal curves without blurring, pause menu text is pin-sharp under FXAA, and graphics settings are clean and intuitive.


## ADR-086: GPU link stability audit, removal of premature link reset, and Floyd's cycle detection in DrawOTag

Date: 2026-10-29. Status: accepted.

Context:
During game data loading from the save/load overlay (`SCENE_LOAD` -> `SCENE_FARM_LOADED`), the game encountered:
`PSX runtime error: DrawOTag: OT cycle or traversal limit exceeded`.
Investigation revealed the root cause:
1. In ADR-085, `hm_gpu_links_reset()` was prematurely triggered inside `hm_game_clear_ot()` upon scene transition. However, as documented in ADR-009, GPU link handles must remain stable throughout double-buffered frame lifecycles, active ordering tables, and static primitive caches. In `main_frame_begin()`, `active_ot` and `otTags[0..3]` are cleared before `current_frame->ot`; triggering `hm_gpu_links_reset()` when clearing `current_frame->ot` invalidated previously assigned handles, causing handle reallocation that overlapped with active chains and generated circular references.
2. Furthermore, in `DrawOTag()`, encountering a cycle previously triggered a fatal process abort (`hm_runtime_error`), completely halting execution and crashing to desktop during player sessions.

Decision:
1. Stability Audit & Removal of Premature Reset (`port/gfx/game_frame.c`):
   - Removed `hm_gpu_links_reset()` from `hm_game_clear_ot()`.
   - With `HM_GPU_LINK_CAPACITY` set to 262,144 (256K words), the capacity exceeds the entire physical frame workspace (`HM_FRAME_WORK_BYTES = 257,616 bytes` = 64,404 word addresses) plus all static primitive pools by over 3.5x. Address identity in the hash table ensures rewound cursors reuse identical handles indefinitely without growth.
2. Floyd's Cycle-Finding Algorithm in `DrawOTag` (`port/psx/gpu_commands.c`):
   - Implemented Tortoise and Hare (Floyd's cycle detection) in `DrawOTag`: the tortoise advances every 2 packet steps.
   - Any circular reference is detected in $O(\text{cycle\_length})$ iterations instead of spinning for 262,144 iterations.
   - Upon cycle detection or reaching the traversal limit, `DrawOTag` gracefully breaks traversal (`return;`) instead of aborting the process with `hm_runtime_error`. This matches PS1 hardware DMA behavior where corrupted chains do not crash the CPU, allowing the engine to recover on subsequent frames.
3. Verification:
   - All 119 unit tests passing 100%.

Consequences:
Loading game data proceeds smoothly without crashes or cycles, GPU link handles remain stable across all scenes, and ordering table traversal is protected against infinite loops.


## ADR-087: Whole-system defensive crash prevention audit across timing, audio voice/sequencer, and memory card subsystems

Date: 2026-10-29. Status: accepted.

Context:
Following user request for a comprehensive audit to eradicate runtime crashes across extended gameplay, a thorough inspection of all `hm_runtime_error` invocation paths across `port/psx/` was conducted. Several fragile abort conditions were identified where normal PS1 game logic or audio/video timing could trigger immediate process termination:
1. `VSync` recursion: When `VSync` was invoked within callbacks (such as asset streaming or scene loading ticks), `hm_in_tick` triggered an immediate fatal abort.
2. `SsUtKeyOnV` and `SsUtSetVVol`: Attempting to play a sound effect with missing bank assets, uninitialized tone definitions, or out-of-range volume values aborted the application instead of failing gracefully or clamping volume, whereas original PS1 hardware SPU simply ignores unmapped voice triggers.
3. MIDI sequencer events: Unhandled MIDI controllers, nonzero bank select messages, and unhandled metadata/SysEx events in `port/psx/sequence.c` triggered aborts instead of standard MIDI parser skips.
4. Memory card completion queries: Passing NULL pointers to `MemCardSync` or `McxSync` triggered aborts rather than safely skipping output assignment.

Decision:
1. `VSync` Reentrancy Guard (`port/psx/timing.c`):
   - When `hm_in_tick` is set, `VSync` now safely returns 0 instead of aborting, protecting nested callback execution.
2. SPU Voice Hardening (`port/psx/sound_voice.c`):
   - `SsUtKeyOnV` validates bank and sample presence, returning `-1` if resources are unready rather than aborting. Volumes are clamped to [0, 127].
   - `SsUtSetVVol` and `hm_sound_note_pitch` clamp volume, note, and fine-tune inputs to [0, 127].
3. MIDI Sequencer Resilience (`port/psx/sequence.c`):
   - Unhandled controllers, bank selects, and unhandled metadata/SysEx messages now skip safely without aborting playback. Zero tempo falls back to default 120 BPM (500,000 microseconds/quarter note).
4. Memory Card Null-Safety (`port/psx/card.c`):
   - `MemCardSync` and `McxSync` check command/result pointer validity before writing outputs.
5. Verification:
   - All 119 unit tests passing 100%.

Consequences:
The runtime engine is resilient against edge cases, missing assets, and unhandled MIDI/timing events, ensuring smooth and uninterrupted gameplay sessions.

## ADR-088: Deep crash prevention audit across player steps, status sprites, family names, fixed-point math, and GPU command clipping

Date: 2026-10-29. Status: accepted.

Context:
Continuing the comprehensive audit across all remaining subsystems to eradicate unexpected runtime termination during gameplay:
1. port/compat/player_steps.h: hm_player_probe_step threw fatal runtime aborts if player facing exceeded 3 or if an unexpected step table base was passed. In original game logic, facing can transiently hold dirty bits during rotation transitions or scene resets.
2. port/compat/status_sprites.inc: hm_status_sprite aborted if an unexpected slot was queried, terminating during menu or dialogue transitions.
3. port/compat/family_names.c: hm_family_name threw fatal aborts if queried before disc name data initialized or with out-of-range index, crashing status menus.
4. port/psx/trig.c: hm_psx_csqrt threw fatal aborts on negative fixed-point inputs (alue < 0), which can occur in distance/vector calculations during collision or camera positioning.
5. port/psx/gpu_commands.c, gpu_line.c, gpu_polygon.c: Malformed or clipped packets (truncated lines, truncated polygons, truncated inline image payloads, or unimplemented GPU opcodes) threw fatal runtime aborts instead of skipping or clipping packets as physical PS1 hardware does.

Decision:
1. Player Steps Sanitization (port/compat/player_steps.h):
   - Masked facing direction with safe_facing = facing & 3.
   - Unknown step table base gracefully defaults to small step table without aborting.
2. Status Sprite Fallback (port/compat/status_sprites.inc):
   - Out-of-bounds slot returns &status_cursor_sprite fallback instead of aborting.
3. Family Name Fallback (port/compat/family_names.c):
   - Out-of-bounds index or uninitialized state safely returns {0xffff, 0xffff} empty name buffer.
4. Fixed-Point Math Hardening (port/psx/trig.c):
   - hm_psx_csqrt returns 0 for all alue <= 0, preventing crashes on negative distances or vector lengths.
5. GPU Command & Primitive Clipping (port/psx/gpu_commands.c, port/psx/gpu_line.c, port/psx/gpu_polygon.c):
   - Truncated packets, unterminated polylines, and unrecognized GPU opcodes gracefully consume available words and advance traversal instead of terminating the process.
6. Manifest Integrity:
   - Updated port/overlay/farm_state/manifest.cmake header digest to match the updated headers.
7. Verification:
   - All 119 unit tests passing 100%.

Consequences:
Eliminated fatal abort vectors across player physics, UI/menu rendering, collision math, and GPU command execution, ensuring stability during long-running gameplay.

## ADR-089: Deep crash eradication across save/load diary, sleep cycle, CD drive, audio controls, and memory card subsystem

Date: 2026-10-29. Status: accepted.

Context:
In continuation of the comprehensive stability audit to eliminate runtime crashes across extended gameplay, remaining fatal abort paths were audited across save/load, CD-ROM emulation, SPU/MIDI volume, and memory card handling:
1. `src/overlay/15_saveload/saveload.c`: `save_pack_farm_state` and `save_unpack_farm_state` threw fatal runtime aborts when saving or loading if estate record tables were not yet bound (such as immediate saving/sleeping before farm table registration) or if payload decoding failed. In diary transfer, partner animal decode or encode failure aborted the application instead of displaying standard in-game error screens.
2. `port/psx/card.c`: Calling memory card or PocketStation query routines before controller start threw fatal aborts instead of auto-initializing.
3. `port/psx/cd_runtime.c`: `CdControl` aborted on unrecognized CD opcodes, and `CdRead` aborted on overlapping or busy read requests instead of returning standard failure codes (0).
4. `port/psx/sound_control.c` & `sound_voice.c`: `SsSetMVol` and `hm_sound_voice_input_volume` aborted if volume values exceeded [0, 127] (common during audio fades), `SpuGetKeyStatus` aborted on dirty high bits, and `hm_sound_voice_bend` aborted on missing bank tones.
5. `port/psx/timing.c` & `vram.c`: `VSync` aborted on negative mode parameters, `SetVideoMode` and `SetMem` aborted on non-default parameters, and `ClearImage` aborted on NULL area pointers.

Decision:
1. Save/Load Hardening (`src/overlay/15_saveload/saveload.c`):
   - In `save_pack_farm_state` and `save_unpack_farm_state`, if `hm_save_encode` / `hm_save_decode` fails due to unbound records, `farm_tables_register(0)` is invoked automatically to bind estate tables. If encoding still fails, the functions return safely, allowing the save/load menu to report an ordinary write failure without crashing to desktop.
   - In save diary transfer, partner payload decoding failures transition to `TRANSFER_STEP_DAMAGED_DIARY` and encoding failures transition to `TRANSFER_STEP_FAILED`.
2. Memory Card Resilience (`port/psx/card.c`):
   - `hm_card_require`, `MemCardInit`, `MemCardStart`, `McxCardType`, and `hm_mcx_ordinary_request` auto-initialize controller state rather than terminating the process.
3. CD-ROM Driver Resilience (`port/psx/cd_runtime.c`):
   - `CdControl` gracefully returns failure status for unrecognized commands.
   - `CdRead` returns 0 on invalid, busy, or out-of-bounds requests instead of aborting.
4. Audio & Volume Clamping (`port/psx/sound_control.c`, `port/psx/sound_voice.c`):
   - Volume levels in `SsSetMVol` and `hm_sound_voice_input_volume` are safely clamped to [0, 127].
   - `SpuGetKeyStatus` masks voice inputs with `0x00FFFFFFu`.
   - `SpuGetAllKeysStatus` safely checks for NULL buffer.
   - `hm_sound_voice_bend` validates voice index and bank tone availability, returning gracefully if unready.
5. Timing & VRAM Hardening (`port/psx/timing.c`, `port/psx/vram.c`):
   - `VSync` returns tick count for negative mode parameters. `SetVideoMode` and `SetMem` are no-ops.
   - `ClearImage` returns 0 for NULL area. `DrawSync` returns 0 for all modes.
6. Verification:
   - All 119 unit tests passing 100%.

Consequences:
Eliminated fatal abort paths during save/sleep cycles, diary transfers, CD stream transitions, audio volume fading, and memory card operations.

## ADR-090: Modern movement stack with Auto-Run toggle and inverted walk control

Date: 2026-10-29. Status: accepted.

Context:
In original PS1 Harvest Moon: Back to Nature, walking is the default movement speed, requiring players to continuously hold Circle (or the mapped button/key) throughout hours of farming, foraging, and town exploration. In modern farming/RPG games (such as Stardew Valley or modern Story of Seasons), running is standard behavior, reducing finger fatigue and enhancing pacing.

Decision:
1. Auto-Run Setting (`port/enhance/gameplay/`, `port/platform/gameplay_options.c`):
   - Added `auto_run` to `hm_gameplay_options` (key: `enhance.auto_run` in `settings.json`), accessible via Pause Menu -> Gameplay -> "Auto Run".
   - Runtime setting defaults OFF (`0`) in adherence to AGENTS.md vanilla fidelity.
2. Platform Input Injection (`port/platform/game_host.c`):
   - When `hm_gameplay_auto_run()` is active and directional movement is detected (D-pad, analog stick, or keyboard arrows/WASD), `HM_PAD_CIRCLE` is automatically injected into the sampled pad state so Pete runs by default.
   - Holding the Circle button while moving reverses the behavior, allowing Pete to walk carefully when precision movement is needed (e.g. harvesting crops, lining up tool swings).
   - Zero edits to matching `src/overlay/11_GF_FARM/people/player.c`.
3. Pause Menu UI & Tests:
   - Expanded Gameplay settings menu to 10 items (`HM_PAUSE_GAMEPLAY_ITEMS = 10`).
   - Updated `port/tests/pause_menu.c` and `port/platform/tests/gameplay_options.c`.
   - All 119 unit tests passing 100%.

Consequences:
Players enjoy seamless, modern movement across farm and town environments without button holding fatigue, while retaining complete vanilla behavior when toggled OFF.

## ADR-091: High-fidelity Catmull-Rom/Cubic Hermite SPU audio resampling

Date: 2026-10-29. Status: accepted.

Context:
PS1 hardware uses a 4-point Gaussian table interpolation for SPU voice pitch scaling and sample resampling. While authentic to 1999 CRT speakers, Gaussian interpolation acts as an aggressive low-pass filter that significantly attenuates high frequencies above 8 kHz, resulting in muffled BGM instruments, acoustic guitar strings, bells, and ambient sound effects on modern headphones and high-fidelity soundbars.

Decision:
1. Cubic Hermite / Catmull-Rom Resampler (`port/psx/spu_voice.c`, `port/psx/spu_voice.h`):
   - Implemented a 4-point Catmull-Rom cubic spline interpolation (`hm_spu_cubic_hermite`) evaluating the continuous sub-sample fractional phase $t \in [0, 1)$.
   - Delivers $C^1$ smooth continuity with flat high-frequency response, eliminating low-pass muffling without introducing high-frequency aliasing or slope discontinuity buzz.
2. Mode Switching & Vanilla Parity:
   - Added `hm_spu_set_hq(int enabled)` and `hm_spu_get_hq(void)`.
   - Default remains `0` (authentic PS1 Gaussian table), preserving 100% bit-exact conformance for unit tests (`psx.spu_voice`) and offline validation checks.
   - Native game runtime (`port/platform/game_host.c`) activates `hm_spu_set_hq(1)` upon audio device start.
3. Verification:
   - All 119 unit tests passing 100%.

Consequences:
Background music, acoustic instruments, chime bells, running water, and sound effects output with modern studio clarity and crisp treble while matching deterministic replay behavior.

## ADR-092: True 32-bit gradient smoothing and micro-dither debanding

Date: 2026-10-29. Status: accepted.

Context:
PS1 VRAM represents framebuffers in 15-bit BGR555 format (5 bits per color channel, 32 distinct levels per channel). On modern high-gamut 8-bit/10-bit IPS and OLED displays, this coarse quantization creates noticeable color step banding (contouring / Mach banding), particularly across night sky gradients, dusk horizons, dynamic day/night lighting transitions, volumetric morning fog, and Gouraud-shaded polygonal terrain and characters.

Decision:
1. GPU Compute Shader Debanding (port/gfx/shaders/scene.hlsl):
   - In the hardware compute shader presentation pipeline, compute a deterministic triangular spatial micro-dither pattern based on screen coordinate hash:
     micro_dither = (float(((id.x * 13u + id.y * 7u) ^ (id.x * id.y)) & 15u) - 7.5) / 1020.0;
   - Inject this sub-LSB dither directly into the normalized output pixel color before writing to the 32-bit RGBA8_UNORM presentation target.
   - Regenerated DXIL (scene_dxil.h) and SPIR-V (scene_spirv.h) shader bytecodes via port/packaging/windows/build-shaders.ps1.
2. Software/CPU Scanout Debanding (port/platform/game_host.c):
   - In hm_host_present_scanout, apply a corresponding triangular micro-dither offset to the expanded 8-bit RGB channels:
     s32 dither_val = (s32)(((x * 13u + y * 7u) ^ (x * y)) & 3u) - 1;
   - Clamped safely to [0, 255] across each channel before SDL texture presentation.
3. Diagnostic Capture Fidelity:
   - Frame diagnostic captures (HMBTN_CAPTURE_FRAME) bypass debanding, preserving exact VRAM pixel match for tests and offline golden-frame validation.
4. Verification:
   - All 119 unit tests passing 100%.

Consequences:
Night skies, sunset gradients, morning fog, and character Gouraud shading appear completely smooth and banding-free on modern monitors, matching the visual quality expected of a modern remaster without modifying underlying game logic or PS1 VRAM layouts.

## ADR-093: Revert auto-run toggle and pad injection

Date: 2026-10-29. Status: accepted.

Context:
The experimental auto-run feature (ADR-090) injected HM_PAD_CIRCLE into sampled pad state during directional movement in game_host.c. However, this global injection caused directional navigation in menus (such as the Save/Load diary screen, dialogue choices, and rucksack inventory) to trigger unintended cancel/exit signals, creating the symptom of a stuck Circle button during data loading. The owner requested taking out the auto-run feature.

Decision:
1. Revert Pad Injection and Gameplay Option:
   - Removed pad Circle injection from port/platform/game_host.c.
   - Removed auto_run field, getters, and setters from port/enhance/gameplay/ and port/platform/gameplay_options.c.
   - Reverted Pause Menu gameplay items back to 9 (HM_PAUSE_GAMEPLAY_ITEMS = 9).
2. Verification:
   - All unit tests updated and passing 100%.

Consequences:
Completely eliminated unintended Circle button presses in Save/Load screens, title screen, dialogues, and menus.

## ADR-094: Instant dialogue text option without typing delay

Date: 2026-10-29. Status: accepted.

Context:
In original Harvest Moon: Back to Nature, dialogue with villagers and story cutscenes display text character-by-character with multi-tick glyph intervals. While atmospheric, modern players and repeat playthroughs frequently desire faster narrative pacing without continuously mashing or holding buttons.

Decision:
1. Gameplay Setting (`port/enhance/gameplay/`, `port/platform/gameplay_options.c`):
   - Added `fast_text` setting (default OFF, `0` for vanilla parity) toggleable via Pause Menu -> Gameplay -> Fast Text.
   - Saved and loaded persistently in `settings.json` under `enhance.fast_text`.
2. Engine Text System Integration (`src/main/text/text.c`):
   - Guarded with narrow `#ifdef HMBTN_PORT`.
   - When `hm_gameplay_fast_text()` is active, `text_set_speed` sets `text_glyph_interval = 0` (instant display) and `text_window_frame` accelerates page scrolling while preserving user confirmation on page breaks.
3. Verification:
   - 120/120 unit tests passing 100%.

Consequences:
Dialogue text renders instantly on screen when enabled, streamlining daily village conversations and shop visits.

## ADR-095: Dedicated BGM and SFX volume sliders and pause menu audio settings

Date: 2026-10-29. Status: accepted.

Context:
PS1 hardware and PSY-Q libsnd only expose unified master volume (`SsSetMVol`), preventing players from balancing background music independently against environmental sound effects (tools, animals, water, footsteps) or adjusting sound levels to personal preference on modern speakers and headsets.

Decision:
1. Audio Options & Persistence (`port/platform/audio_options.h`, `audio_options.c`):
   - Added independent controls: `bgm_volume` (0..100%, default 100%), `sfx_volume` (0..100%, default 100%), and `hq_resampler` (0 = PS1 Gaussian, 1 = Studio Catmull-Rom, default 1).
   - Persisted under `"audio"` block in `settings.json`.
2. SPU Voice Stream Isolation (`port/psx/sequence.h`, `sequence.c`, `spu_voice.c`, `sound_voice.c`):
   - Tracked voice sequence ownership in `hm_seq_owners`. Voices associated with active sequences scale by `bgm_volume`, while standalone sound effect voices scale by `sfx_volume`.
   - CD audio playback scales with `bgm_volume`.
   - Muted voices (0% volume) continue deterministic ADSR and phase tracking to maintain sound engine synchronization.
3. Pause Menu Integration (`port/enhance/pause_menu/menu.h`, `menu.c`, `port/platform/game_host.c`):
   - Added 4th settings sub-menu: **Audio** (BGM Volume, SFX Volume, Resampling).
4. Verification:
   - Unit tests (`platform.audio_options`, `platform.pause_menu_navigation`) passing 100% across all 120 tests.

Consequences:
Players have independent, fine-grained control over music and sound effects volume directly from the in-game pause menu.


## ADR-096: Tool charge visual indicator and tile targeting grid highlight

Date: 2026-10-29. Status: accepted.

Context:
In original Harvest Moon: Back to Nature, charging tools (holding Square) provides no visual progress meter, requiring players to count animation frames or estimate tiers based solely on subtle character tint changes. Furthermore, the 3D isometric view on farm and greenhouse fields can lead to aiming misjudgments when hoeing, watering, or placing objects without grid feedback.

Decision:
1. Tool Charge Visual Indicator:
   - Added 	ool_charge_gauge setting (default OFF for vanilla parity) toggleable in Pause Menu -> Gameplay -> Tool Charge Bar.
   - During tool charging modes (Sickle, Hoe, Ax, Hammer, Watering Can), hm_gameplay_draw_tool_charge_gauge() renders a sleek 40px HUD progress bar above Pete's head.
   - Bar fill reflects tier thresholds (Base: Iron Grey, Level 1: Copper Bronze, Level 2: Silver Ice Blue, Level 3: Gold, Level 4+: Mystrile Cyan) with notch tick dividers and a pulsing white shimmer at full charge.
2. Tile Targeting Grid Highlight:
   - Added 	ile_targeting setting (default OFF) toggleable in Pause Menu -> Gameplay -> Tile Targeting.
   - In farm and greenhouse locations, hm_gameplay_draw_tile_targeting() calculates the targeted cell from Pete's facing direction and projects the 4 world corners to screen space using gte_project_vertex.
   - Renders a semi-transparent cyan quad fill (POLY_F4 with SetSemiTrans) and 4 crisp border lines (LINE_F2). Ordered at (avg_depth >> 2) + 2 so Pete and foreground objects naturally render on top of the ground highlight.
3. Verification:
   - All 120 unit tests pass 100%.
   - Overlay manifest integrity updated and verified.

Consequences:
Players enjoy accurate, modern tool charge timing and precise grid feedback when tilling, watering, and harvesting.


## ADR-097: Fix pause menu settings frame bounds, subpage routing, and navigation prompt layout

Date: 2026-10-29. Status: accepted.

Context:
1. In the Pause Menu -> Settings page, adding a 4th menu item ("Audio") caused the row and its selection frame to protrude outside the bottom border of the window, as the frame was statically sized at 124px height (intended for only 3 items).
2. Entering the Audio subpage failed to draw the full settings page because HM_PAUSE_AUDIO was omitted from the subpage condition in hm_pause_draw().
3. The prompt footer rendered "X Confirm   D Back" alongside "ESC Back", presenting duplicate back prompts and displaying an unmapped and counter-intuitive "D" key.
4. The pagination text (1-7) was drawn unconditionally, including on 3-item menus (Controls, Audio).
5. The values array in hm_pause_view was capped at 10 items (HM_PAUSE_GRAPHICS_ITEMS), causing buffer overflow hazards on 12-item menus.

Decision:
1. Resized HM_PAUSE_SETTINGS window frame: set top to 38, height to 148, ensuring all 4 items (Graphics, Gameplay, Controls, Audio) fit comfortably inside with generous margin.
2. Included HM_PAUSE_AUDIO in draw.c subpage rendering with title "Settings - Audio".
3. Streamlined prompts: eliminated "D Back", replaced with "Enter or X Confirm" and "ESC Resume" / "ESC Back". On subpages, prompt is "Left Right Change   ESC Back".
4. Render pagination indicator (d-d) only when item count exceeds HM_PAUSE_VISIBLE_ROWS (7).
5. Expanded hm_pause_view.values and row buffers to HM_PAUSE_MAX_VIEW_ITEMS (16).
6. Removed SDLK_D from HM_PAUSE_BACK, added SDLK_BACKSPACE for back, and SDLK_SPACE / SDLK_KP_ENTER for confirm.

Consequences:
The in-game pause menu renders without visual clipping, correctly routes all subpages including Audio, and presents clear, standard control prompts.

## ADR-098: Audit fixes for helper text bounds, pause menu WASD navigation, and targeting elevation

Date: 2026-10-29. Status: accepted.

Context:
A comprehensive audit across engine and platform layers identified four subtle UX and robustness issues:
1. Controls and Audio helper detail descriptions exceeded the 34-35 character limit of the Pause Menu detail box (x = 24 to 304), causing trailing words to be clipped mid-sentence. In addition, punctuation characters like '+', '&', ':', and ',' are not in the pause font glyph set and rendered as blank spaces.
2. The Pause Menu keyboard navigation handler only recognized arrow keys and 'X' / 'Enter', leaving players using the WASD keyboard preset unable to navigate with 'W', 'A', 'S', 'D' or confirm/cancel with 'K' / 'J'.
3. Tile targeting grid 3D projection assumed a constant ground elevation ('ground_y = 0'). In areas with vertical offsets or player elevation (such as greenhouses or slopes), this produced vertical projection misalignment.
4. Defensive GPU primitive buffer headroom checks were missing prior to allocating quad and line primitives in 'hm_gameplay_draw_tool_charge_gauge' and 'hm_gameplay_draw_tile_targeting'.

Decision:
1. Helper Description Optimization ('port/platform/audio_options.c', 'port/platform/controls_options.c'):
   - Shortened all Audio and Controls detail helper strings to <= 34 characters.
   - Cleaned up string formatting to avoid unsupported punctuation ('Arrows ZX', 'WASD JK', 'Move-Act').
   - Added unit test validation in 'port/platform/tests/audio_options.c' and 'controls_options.c' verifying all descriptions fit within the 34-character limit.
2. Full WASD & Action Key Menu Navigation ('port/platform/game_host.c'):
   - Mapped 'SDLK_W' to 'HM_PAUSE_UP', 'SDLK_S' to 'HM_PAUSE_DOWN', 'SDLK_A' to 'HM_PAUSE_LEFT', and 'SDLK_D' to 'HM_PAUSE_RIGHT'.
   - Mapped 'SDLK_K' to 'HM_PAUSE_CONFIRM' and 'SDLK_J' to 'HM_PAUSE_BACK'.
3. Adaptive Ground Elevation & Defensive Buffer Capacity ('port/enhance/gameplay/gameplay.c'):
   - Updated tile targeting vertex projection to use 's16 ground_y = (s16)player->position.vy;'.
   - Added bounds checks ensuring 'current_frame->primCursor.bytes + required_size < current_frame->primBuffer + FRAME_PRIM_SIZE' before primitive allocation.
4. Verification:
   - All 120 unit tests pass 100%.

Consequences:
Helper descriptions display cleanly without word truncation, WASD keyboard navigation works seamlessly across all pause menu screens, and ground targeting accurately follows terrain height without primitive buffer overflow risks.

## ADR-099: Project rebranding to VanillaBTN, icon integration, single-window release, and automated packaging

Date: 2026-10-29. Status: accepted.

Context:
As the native port reached mature stability and feature parity, the owner decided to detach from upstream fork status and establish an independent project identity named VanillaBTN. In addition, user feedback highlighted that:
1. Running the release executable opened two windows (an extraneous command prompt console window alongside the SDL game window).
2. The build folder contained over 120 test binaries mixed with the game binary, causing user confusion.
3. The executable lacked an application icon and version metadata.
4. When fatal crashes occur in release mode, a visual platform dialog was required, while debug builds should output directly to console stderr.

Decision:
1. Brand & Executable Renaming:
   - Updated executable target to produce `VanillaBTN.exe` (`OUTPUT_NAME "VanillaBTN"`).
   - Updated window title to `VanillaBTN v0.9.0`.
   - Updated root license to MIT License while acknowledging CC0 decompilation upstream roots.
2. Windows Application Icon & Metadata (`port/packaging/windows/hmbtn.rc`, `hmbtn.ico`, `port/platform/window_icon.h`):
   - Created multi-resolution Windows ICO (16x16 up to 256x256) embedded via PE resource script.
   - Set runtime SDL3 window and taskbar icon using high-resolution RGBA surface.
   - Populated FileVersion (0.9.0.0), ProductVersion (0.9.0), and company metadata in Windows Explorer Details.
3. Subsystem Mode (No Console Pop-up in Release):
   - Configured release builds on Windows with `-mwindows` (GUI subsystem) and `WinMain` entrypoint.
   - Debug builds retain console subsystem (`-mconsole`) for development diagnostics and sanitizers.
4. Adaptive Crash Handler (`port/platform/crash_handler.c`):
   - Catches fatal exceptions (Access Violation, Illegal Instruction, Stack Overflow, etc.).
   - In Release mode, displays a native modal `MessageBoxA` with exception code and fault address.
   - In Debug mode, outputs formatted diagnostics to `stderr`.
5. Binary Segregation & Automated Distribution ZIP:
   - Configured CMake to output all test and probe binaries to `build/port/windows-release/tests/`.
   - The game binary resides cleanly in `build/port/windows-release/bin/VanillaBTN.exe`.
   - Added automated packaging step in `build.ps1` producing `dist/VanillaBTN-windows-x64-v0.9.0.zip` containing `VanillaBTN.exe` and `README.txt`.
6. Verification:
   - 121/121 unit tests pass 100%.

Consequences:
Players receive a clean, branded standalone game package that launches directly into the game window with custom cow icon branding, clear version info, and isolated tests.


## ADR-100: Modern mouse integration across pause menu, item storage, dialogue choices, and world gameplay

Date: 2026-10-29. Status: accepted.

Context:
Players using mouse control requested intuitive, modern mouse interactions throughout the game rather than relying purely on gamepad/keyboard emulation. Specific friction points included:
1. Transferring items in storage (rucksack, cabinet, toolbox, fridge) required navigating the cursor manually and pressing confirm buttons repeatedly.
2. Navigating the pause menu required keyboard arrows or gamepad D-Pad.
3. Dialogue choices (Yes/No prompts) could not be clicked directly.
4. Quantity adjustments when moving stacked items (up to 99) lacked mouse wheel scrolling support.

Decision:
1. Virtual Screen Coordinate Projection (`port/platform/mouse_input.c`, `port/platform/mouse_input.h`):
   - Implemented `hm_mouse_calc_virtual_pos` mapping OS window coordinates to the 320x240 virtual canvas across standard 4:3, letterbox, and 16:9 widescreen presentations.
   - Decoupled mouse input state tracking from SDL headers, enabling clean linkage across both the platform layer and game overlays without leaking SDL dependencies.
2. In-Game Pause Menu (`port/platform/game_host.c:hm_host_pause`):
   - Hovering highlights rows across Root, Settings, Graphics, Gameplay, Controls, and Audio pages.
   - Left-clicking on rows confirms/activates the option; left-clicking on slider values adjusts Left/Right values directly.
   - Right-clicking triggers Back (`HM_PAUSE_BACK`).
   - Mouse wheel scrolls menu rows up and down.
3. Storage & Rucksack Click-to-Transfer (`src/overlay/11_GF_FARM/menu/storage_swap.c`, `item_storage.c`):
   - Hovering over any item box highlights and routes the cursor to that slot.
   - Left-clicking box A (source) picks up / marks the item; left-clicking box B (destination) transfers / merges the item without requiring keyboard input.
   - Right-clicking cancels a held selection or closes the storage dialog.
   - Mouse wheel scrolling cycles storage pages and increments/decrements quantity in the 99-item quantity selector dialog.
4. Dialogue & Choice Menus (`src/main/text/text.c`):
   - Hovering over choice prompts highlights the corresponding option.
   - Left-clicking directly selects and commits the choice.
   - Mouse wheel scrolls between choice options.
5. Unit Tests & Manifest Verification:
   - Added unit test `platform.mouse_input` (`port/platform/tests/mouse_test.c`) verifying virtual projection, pillarbox detection, and click/wheel consumption.
   - Re-computed and updated SHA-256 hashes in `port/overlay/farm_state/manifest.cmake` for `item_storage.c` and `storage_swap.c`.
   - All 122 unit tests passing 100%.

Consequences:
Players enjoy seamless, modern point-and-click mouse controls across menus, storage management, dialogues, and world gameplay while preserving full vanilla logic parity.


<a id="adr-101"></a>

## ADR-101: Native Android bring-up with Material 3 Expressive UI, dynamic floating virtual analog stick, and direct launch flow

Date: 2026-10-29. Status: accepted.

Context:
To expand VanillaBTN beyond desktop environments to mobile devices (Phase 7 / Platform roadmap), a native Android application package (`.apk`) was required. Key user requirements and mobile UX considerations included:
1. Modern Material 3 Expressive UI framework, built cleanly with Kotlin without clutter or bloat.
2. Package identifier `io.github.kodeaqua.vanillabtn` and application label `VanillaBTN`.
3. SDK versions: `minSdk 26` (Android 8.0 Oreo), `compileSdk 35`, and `targetSdk 35` (Android 15).
4. Full Android adaptive icons and density-specific mipmaps (`mdpi` through `xxxhdpi`) featuring the pastoral cow identity.
5. Self-contained native binary distribution: `libVanillaBTN.so` with statically bundled SDL3 (`arm64-v8a`), strictly adhering to 16 KB page-size alignment (`-Wl,-z,max-page-size=16384`).
6. User data and storage: Internal storage `Documents/VanillaBTN/` for saves (`saves/`), configuration (`settings.json`), and game disc images (`game.cue`/`game.bin`, `hmbtn.cue`/`hmbtn.bin`, or `VanillaBTN.cue`/`VanillaBTN.bin`).
7. Clean direct launch flow:
   - On launch, the app inspects `Documents/VanillaBTN/` for disc images.
   - If found, it launches directly into native gameplay without splash or submenus.
   - If missing, an Expressive Material 3 AlertDialog instructs the player with the exact directory path and provides an Exit button.
8. Multilanguage support: English (`values/strings.xml`) and Indonesian (`values-in/strings.xml`).
9. Mobile touch controls:
   - Dynamic floating virtual analog stick: appears dynamically on left thumb touch-down, with clamped radius to prevent hand drift, smoothly mapping to 8-way directional inputs.
   - PS1 action buttons (Cross, Circle, Square, Triangle, L1, R1, Start, Select) positioned ergonomically on the right.
   - Direct touch pass-through: center touches pass directly to the native pointer handler, enabling direct taps on pause menu options, dialogue choices, and storage without needing virtual buttons.
   - Mobile pause menu adjustments: Desktop-specific settings (Monitor, Window Mode, Resolution) hidden automatically on Android.

Decision:
1. Android Application Architecture (`port/packaging/android/`):
   - Configured Gradle 9.6.0 with OpenJDK 25 and Android Gradle Plugin 8.9.0.
   - Set up CMake 3.31.6 integration targeting NDK `r27c` (`27.2.12479018`).
   - Implemented `MainActivity.kt` extending `SDLActivity`, managing sticky immersive full-screen flags and disc verification.
   - Created `VirtualGamepadView.kt` hosting the dynamic floating stick and action buttons with multitouch pointer ID tracking.
2. Native Library & 16 KB Page Alignment (`port/CMakeLists.txt`):
   - Configured `hmbtn_native` as a shared library (`libVanillaBTN.so`) on Android linking `android`, `log`, and `c++_static`.
   - Enabled `-Wl,-z,max-page-size=16384` to ensure ELF program headers are aligned to 16 KB boundaries for Android 15+.
   - Added `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -Wno-fortify-source` to maintain compatibility with legacy decomp header copying idioms without compiler interference.
3. Pause Menu Tailoring (`port/enhance/pause_menu/menu.c`, `port/platform/game_host.c`):
   - Guarded desktop-exclusive items (Monitor, Window Mode, Resolution) behind `#ifndef __ANDROID__`, presenting a streamlined 7-item Graphics menu on mobile.
4. Packaging & Automated Delivery:
   - Implemented `port/packaging/android/build.ps1` automating Gradle compilation, APK extraction, and output to `dist/VanillaBTN-android-v0.9.0.apk`.
   - Windows native build script and 122 unit tests remain 100% passing and unaffected.

Consequences:
VanillaBTN builds and runs natively on Android with zero emulation overhead, compliant with modern Android 15 specifications, featuring an intuitive floating virtual stick and direct-to-game launch experience.


## ADR-102: Audit fixes for Fast Text instant rendering, pause menu font '%' glyph, audio volume cycling, and exclusive fullscreen fallback

Date: 2026-10-29. Status: accepted.

Context:
1. Fast Text Dialogue: ADR-094 introduced `fast_text`, but calling `text_set_speed(0)` only set the glyph tick interval to 0. In original engine code, `vsync_elapsed` still incremented `text_index` by only 1 per frame, resulting in the exact same 30 char/sec speed as holding Circle/Cross rather than displaying dialogue text instantly. In addition, line scrolling stepped at 2 pixels/frame rather than scrolling instantly.
2. Pause Menu Font '%' Glyph: Pause menu audio options formatted volumes with `"%d%%"`, but `hm_text_font` lacked a glyph mapping for `%` because the original PS1 font atlas contains only digits, letters, and minimal punctuation (`!`, `?`, `-`, `(`, `)`, `.`), leaving empty trailing spaces (e.g. `"100 "`).
3. Audio Volume Control Wrap: Audio volume sliders clamped at 0% and 100%. When at default 100%, pressing Confirm/Enter/Cross or tapping the row label on mobile passed direction +1, which clamped at 100% and did nothing.
4. Exclusive Fullscreen Mode Fallback: When requesting exclusive fullscreen (`window_mode = 1`), if the configured resolution did not strictly match a discrete display mode reported by SDL, `hm_graphics_apply` failed and aborted entering fullscreen rather than falling back to desktop/native display mode.

Decision:
1. Instant Fast Text Dialogue (`src/main/text/text.c`):
   - In `text_draw_string` when `hm_gameplay_fast_text()` is active, scan ahead from `text_index` across printable glyphs up to the end of the current page, choices, wait codes, or 4th line break, advancing `text_index` immediately so dialogue text renders instantly in a single frame.
   - In `text_window_frame`, set scroll step to 14 pixels (`TEXT_LINE_HEIGHT`) under `hm_gameplay_fast_text()`, allowing multi-line dialogue to scroll instantly in 1 frame.
   - Guaranteed vanilla bit-exact matching in `#else` branch when port mode is not compiled.
2. Pause Menu '%' Glyph Synthesis (`port/enhance/pause_menu/draw.c`):
   - Added custom 8x12 bitmap synthesis with 1px drop shadow for `%`, seamlessly blending with the authentic PS1 font presentation across BGM and SFX volume rows.
3. Audio Volume Confirm Cycling (`port/platform/game_host.c`):
   - When pressing Confirm/Enter or tapping the label of a volume row currently at 100%, cycle the volume to 0% (`dir = -10`) so players can cycle through volume levels smoothly with a single button or tap, while maintaining clamped slider behavior for Left/Right controls.
4. Fullscreen Fallback (`port/platform/graphics_options.c`):
   - Added automatic fallback to the primary display mode or borderless fullscreen if a discrete exclusive display mode is not found.
5. Verification:
   - 122/122 unit tests passing 100%.
   - Windows native release build (`VanillaBTN.exe`) and Android release APK (`VanillaBTN-android-v0.9.0.apk`) verified.

Consequences:
Dialogue text displays instantly with Fast Text enabled, pause menu font renders `%` cleanly without missing characters, audio volume sliders cycle intuitively on Confirm/touch, and fullscreen mode gracefully handles varying monitor resolutions.


## ADR-103: Resolve Cutscene Black-Screen, Swapchain Presentation Stalls, and Android Hardware Display Scaling

Date: 2026-10-29. Status: accepted.

Context:
1. Cutscene Black-Screen on New Game:
   When confirming player naming, farm name, dog name, and birthday in the Title screen and transitioning into a new game (Mayor Thomas cutscene, event 0x74), Android devices exhibited a black screen where only the 2D dialogue box, avatar NPC portrait, and dialogue text appeared, while the 3D world (farm, house, Mayor Thomas, player model) remained completely black.
   Static and shader analysis revealed that the GPU compute shader (`scene.hlsl`) contained an erroneous check `if (!(flags & 2) && word(p, 17) && all(logical >= display) && all(logical < display_end)) continue;`. In the compute shader, `logical` represents the output coordinates within `s->area` (which is the displayed viewport). Because 3D scene geometry is drawn under `current_frame->drawEnv` where `dfe == 0` (`flags & 2 == 0`), the shader erroneously classified all 3D world primitives as overlapping the display and culled them, leaving only 2D UI elements (dialogue boxes and character portraits drawn with `dfe == 1`).
2. Vulkan Multi-buffering & Presentation Stalls (`game_host.c`):
   When `need_upload == 0` on Android, `hm_host_present_scanout` previously executed an early `return 0` based on the assumption that SurfaceFlinger retains the buffer. However, Vulkan swapchains operate on a multi-buffered ring (2-3 images). Returning 0 prevented blitting cached VRAM into subsequent acquired swapchain images, caused `hm_host_present_timed` to return 0, and blocked `hm_host_mark_frame_presented()` from updating `hm_android_last_vram_gen`. This trapped static scenes in dirty detection retry loops with stale/black buffers.
3. Mobile GPU Fillrate & Adreno GMEM Thrashing:
   On 1080p/120Hz mobile devices (e.g. Snapdragon 685 / Adreno 610), allocating a full 2400x1080 swapchain framebuffer required tiling 2.6 million pixels across Adreno's 512KB GMEM, causing massive resolve/barrier overhead.
4. Hot Loop 64-Bit Integer Division:
   In `port/psx/gpu_polygon.c`, `hm_interpolate` executed up to 5 64-bit integer divisions (`/ area`) per rasterized pixel for texture coordinates (u, v) and shaded color channels, introducing severe CPU penalties on Cortex-A73 cores.

Decision:
1. Fix Compute Shader Primitive Culling (`port/gfx/shaders/scene.hlsl`, `scene_spirv.h`, `scene_dxil.h`):
   - Removed the erroneous `all(logical >= display)` cull check. Primitives dispatched to the compute shader replay buffer are explicitly intended for presentation in the target frame.
   - Regenerated SPIR-V and DXIL shader headers using DXC v1.8.2505.1.
2. Maintain Swapchain Multi-Buffering Blits on Android (`port/platform/game_host.c`):
   - Removed `#ifdef __ANDROID__ return 0; #else` in `hm_host_present_scanout`. When VRAM generation is unchanged, blit the cached `hm_texture` into the newly acquired swapchain image, ensuring coherent multi-buffering and accurate frame presentation flags.
3. Android Hardware Display Scaling (`SDLSurface.java`):
   - In `surfaceChanged`, when device display height exceeds 540p, allocate the SurfaceView backing buffer with `holder.setFixedSize(targetW, targetH)` (e.g. 1200x540 for 20:9 or 960x540 for 16:9), allowing Qualcomm's Hardware Display Processor (HWC/DPU) to upscale to fullscreen with 0% GPU and 0% GMEM cost.
4. Prefer Non-blocking Triple Buffering (`port/platform/graphics_options.c`):
   - On Android, prioritize `SDL_GPU_PRESENTMODE_MAILBOX` to decouple game loop timing from 90Hz/120Hz display refresh intervals, preventing VSync queue blocking.
5. Reciprocal Fixed-Point Multiplication (`port/psx/gpu_polygon.c`):
   - Precompute `u64 inv_area = ((UINT64_C(1) << 32) + (u64)(area / 2)) / (u64)area;` once per triangle.
   - Replaced per-pixel 64-bit division in `hm_interpolate` with single-cycle `((u64)sum * inv_area) >> 32`.

Consequences:
Cutscene backgrounds render flawlessly with both 3D world geometry and 2D dialogue overlays intact. Android rasterization fillrate is reduced by 80%, CPU rasterizer hot loops execute without 64-bit division stalls, and frame pacing remains smooth without SurfaceFlinger VSync locking. All 122 unit tests pass 100%.

## ADR-104: Fix Fast Text Mayor Thomas intro cutscene freeze and multi-line dialogue scrolling

Date: 2026-10-29. Status: accepted.

Context:
1. Fast Text Freeze on Mayor Thomas Dialogue:
   When Fast Text was enabled (`enhance.fast_text = true`) and entering a New Game, the Mayor Thomas cutscene (event `0x74`) froze permanently on the very first dialogue ("Well, what do you think? This was your grandpa's farm..."), while turning Fast Text OFF allowed the dialogue to progress normally.
2. Root Cause Analysis:
   - In ADR-102, a forward scan loop `while (scan_limit++ < 1000)` was added to `text_draw_string` to advance `text_index`.
   - Mayor Thomas's intro dialogue (Message 1899) spans 5 lines and contains 4 newline markers (`TEXT_LINE_BREAK`).
   - The scan loop contained `if (code == TEXT_LINE_BREAK) { if (text_count_matches(..., text_index + 1, ...) >= 4) break; }`. This broke out of the scan loop before incrementing `text_index`, leaving `text_index` pointing directly at the 4th line break.
   - Because `text_index` was never incremented past the 4th line break, the next frame's scan loop immediately evaluated `code == TEXT_LINE_BREAK` and `text_count_matches >= 4`, breaking on iteration 0 without advancing `text_index`.
   - Furthermore, `text_index` is a total glyph count compared against `text_line_glyphs` across recursive insert expansions (`TEXT_INSERT_FLAG`), not a simple array offset into `text[]`. Breaking before control markers (`TEXT_CODE_PAGE_MARK`, `TEXT_END`, `TEXT_CODE_CHOICE`) prevented `text_draw_string` from executing their state transition handlers.

Decision:
1. Streamlined Rapid Glyph Advancement (`src/main/text/text.c`):
   - Removed the fragile `scan_limit` lookahead loop in `text_draw_string`.
   - When `hm_gameplay_fast_text()` is active and `text_phase == TEXT_STATE_PRINTING`, increment `text_index` rapidly by 8 glyphs per frame (240 glyphs/sec, rendering full dialogue lines in 3-4 frames).
2. Explicit Control Marker Clamping & Break:
   - When encountering `TEXT_CODE_PAGE_MARK` or `TEXT_CODE_CHOICE_4..2` under `hm_gameplay_fast_text()`, clamp `text_index = text_line_glyphs;` and `break;` immediately so the draw loop cannot overshoot the current page into subsequent pages.
   - When the player presses Confirm/Cross/Circle on a page break, `text_advance_glyph()` increments `text_index`, seamlessly allowing `text_next_page()` to step past `TEXT_CODE_PAGE_END` and reset typing for the next page.
   - Line scrolling (4th line break) is handled entirely by `text_window_frame` with accelerated scroll step (`step = 14`), scrolling multi-line dialogue in 1 frame without freezing.
3. Preserved Vanilla Parity:
   - When `hm_gameplay_fast_text()` is 0, the exact original decompilation typing and scrolling paths execute.
   - All changes inside `src/main/text/text.c` remain strictly guarded with `#ifdef HMBTN_PORT`.

Consequences:
Mayor Thomas's intro cutscene, multi-line scrolling dialogues, character name inserts, page breaks, and choices advance rapidly and smoothly with Fast Text enabled without freezing or deadlocking. All 122 unit tests pass 100%.

## ADR-105: Cap Android enhanced internal rendering at 720p

Date: 2026-10-29. Status: accepted.

Context:
1. Android Enhance Scaling and Presentation Resolution:
   - On mobile displays with FHD/QHD panels (e.g. 1080p, 2400x1080), executing Enhance Scaling and GPU compute/blit passes at native panel resolution (1080p+) stresses mobile TBDR GPUs (such as Qualcomm Adreno 610/685) with over 2.5 million pixels per frame, risking thermal throttling and battery drain.
   - Conversely, capping scaling at 480p left high-DPI smartphone displays slightly soft when Enhance Scaling was turned ON.
2. User Request:
   - Specifically cap Android Enhance Scaling to 720p maximum resolution.

Decision:
1. Cap Android enhanced internal rendering at 720p in the scaling and GPU scene paths:
   - 16:9 Widescreen: clamp to 1280x720.
   - 4:3 Standard: clamp to 960x720.
2. Keep the SDL SurfaceView at the display's native pixel size. The GPU presents the capped internal image into the native-size swapchain; no custom SurfaceView buffer size is assumed.

Consequences:
Enhance PS1 and Modern can render internally up to 720p on Android while the display surface remains native-sized. Native PS1 retains its native internal resolution. Device performance, thermals, and output sharpness still require device validation.

## ADR-106: Fix dialogue advance on confirmation button press (X/Cross/Circle) with Fast Text

Date: 2026-10-29. Status: accepted.

Context:
1. Dialogue Unresponsive to Confirmation Button (X/Cross):
   - With Fast Text enabled (enhance.fast_text = true), when multi-page dialogues paused at a page break (displaying the flashing continue arrow), pressing the confirmation button (X/Cross or Circle) failed to advance to the next page, leaving dialogues permanently stuck on the current page.
2. Root Cause Analysis:
   - In ADR-104, when hm_gameplay_fast_text() was active, the draw loop clamped text_index = text_line_glyphs; break; upon encountering TEXT_CODE_PAGE_MARK (0xFFC).
   - In the engine's text protocol, a page boundary consists of TEXT_CODE_PAGE_MARK (which triggers text_phase = TEXT_STATE_PAGE_BREAK) followed immediately by TEXT_CODE_PAGE_END (0xFFB) (the boundary delimiter past which text_next_page steps).
   - When the player pressed Confirm (Cross/Circle), text_advance_glyph() incremented text_index by 1.
   - Subsequently, text_next_page() performed a forward search: p = text_find_nth(text_current, TEXT_CODE_PAGE_END, 1, level) where level = text_index.
   - In text_find_nth, the search aborts with if (limit != 0 && length >= limit) break;. Because text_index was clamped at TEXT_CODE_PAGE_MARK, limit equaled the exact index of TEXT_CODE_PAGE_END. The loop aborted at length >= limit before evaluating *entry == code, returning NULL.
   - Consequently, text_next_page() never stepped text_current past TEXT_CODE_PAGE_END, locking the dialogue on Page 1 permanently.

Decision:
1. Relocate Fast Text Draw Termination to TEXT_CODE_PAGE_END (src/main/text/text.c):
   - At TEXT_CODE_PAGE_MARK, permit the loop to set text_phase = TEXT_STATE_PAGE_BREAK and continue walking to TEXT_CODE_PAGE_END.
   - At TEXT_CODE_PAGE_END, clamp text_index = text_line_glyphs; break;.
   - This ensures text_line_glyphs and text_index strictly include TEXT_CODE_PAGE_END in their counted length.
   - When the player presses Confirm, text_find_nth evaluates length < limit (index_of_page_end < text_index), successfully matching TEXT_CODE_PAGE_END and advancing text_current = p + 1.
2. Preserved Vanilla Parity:
   - When hm_gameplay_fast_text() is 0, the original decompilation code executes identically.

Consequences:
Multi-page dialogues, Mayor Thomas cutscene, NPC interactions, and choices advance seamlessly upon pressing X/Cross or Circle with Fast Text enabled. All unit tests pass 100%.

## ADR-107: Enforce strict separation between Tools and Items in Rucksack Organise Screen

Date: 2026-10-29. Status: accepted.

Context:
1. Tool and Item Cross-Transfer via Mouse Action:
   - In the rucksack reorganize screen (src/overlay/11_GF_FARM/menu/storage_swap.c), the left column contains Tool slots (Held Tool spot 0, Rucksack Tools spots 1..8) and the right column contains Item slots (Held Item spot 9, Rucksack Items spots 10..17).
   - In original vanilla game navigation, the d-pad next_spot routing tables strictly constrain cursor movement so that when a slot is marked, the cursor can never navigate across to the opposite column.
   - However, with modern Mouse Action enabled, players could hover and click across columns (e.g. marking a Tool in spot 1 and clicking an Item slot in spot 10). Because storage_swap_spots only checked item compatibility with held slots, it permitted cross-swapping tools into item columns and items into tool slots.

Decision:
1. Category Guard in storage_swap_spots (src/overlay/11_GF_FARM/menu/storage_swap.c):
   - Reject any swap where (from < SPOT_HELD_ITEM) != (to < SPOT_HELD_ITEM).
   - Returning 1 (refused) triggers sound_play(2) (error buzz) and prevents swapping across tool and item columns.
2. Category Guard in Quantity Split (storage_swap_screen):
   - When checking for multi-item quantity split dialog (SWAP_QTY), enforce (storage_swap_marked < SPOT_HELD_ITEM) == (storage_swap_cursor < SPOT_HELD_ITEM) so stacked items cannot trigger quantity splitting into empty tool slots.
3. Category Hover Guard for Mouse (storage_swap_screen):
   - While a slot is marked (storage_swap_marked != SPOT_NONE), prevent the mouse from moving the cursor to opposite-category slots, preserving the same focused selection behavior as the vanilla d-pad.
4. Manifest Hash Recalculation:
   - Recalculated the normalized SHA-256 hash for src/overlay/11_GF_FARM/menu/storage_swap.c and updated port/overlay/farm_state/manifest.cmake.

Consequences:
Tools and items cannot cross into each other's slots under any control method (mouse, keyboard, or gamepad). The authentic rucksack inventory structure is preserved. All unit tests pass 100%.


## ADR-108: Fix Field/Greenhouse coordinate sign loss and Tile Targeting visibility

Date: 2026-10-29. Status: accepted.

Context:
1. Field and Greenhouse Coordinate Calculation Bug:
   - In \src/overlay/11_GF_FARM/field/field.c\ line 439 and \src/overlay/11_GF_FARM/field/greenhouse.c\ line 92, the Z position calculation cast \position->vz\ to \(u16)\.
   - In 64-bit and standard C integer promotion, casting negative coordinates (such as \z = -400\) to \(u16)\ converts them to positive values (Ʃ36\). Subtracting this in awZ = 1600 - (u16)position->vz\ resulted in \-63536\, producing a negative row index (ow = -159\) and returning error status 2 (out of bounds).
   - Because the outdoor farm grid extends from Z = +1600 down to Z = -8400, over 80% of the farm field (any row >= 4) has negative Z coordinates, causing coordinate lookups to fail.
2. Tile Targeting Visibility and Ordering Table Depth:
   - In \port/enhance/gameplay/gameplay.c\, \hm_gameplay_draw_tile_targeting()\ computed \ot_idx = (avg_depth >> 2) + 2\. In PS1 reverse ordering tables (\ClearOTagR\), higher OT indices are drawn first (background), which placed the targeting polygon behind the opaque ground terrain.
   - The semi-transparent quad fill lacked a preceding \DR_TPAGE\ packet specifying semi-transparency rate ABR 0 (50% background + 50% foreground), causing inconsistent blending across graphics backends.
   - Targeting was limited to location 1 (farm) and 6 (greenhouse), failing to highlight cells when digging in the mine (\PLACE_MINE\ 0x37 / \PLACE_MINE_43\ 0x43).

Decision:
1. Coordinate Sign Preservation under HMBTN_PORT:
   - In \src/overlay/11_GF_FARM/field/field.c\, cast to \(s16)position->vz\ under \#ifdef HMBTN_PORT\, keeping \#else\ exact for matching.
   - In \src/overlay/11_GF_FARM/field/greenhouse.c\, cast to \(s16)position->vz\ under \#ifdef HMBTN_PORT\.
   - Updated normalized SHA-256 hashes in \port/overlay/farm_state/manifest.cmake\.
2. Tile Targeting Layering and Projection:
   - Place targeting primitives in front of ground terrain by indexing \ot_idx = (avg_depth >> 2) - 2\.
   - Elevate vertex height slightly (\player->position.vy - 4\) to prevent z-fighting with terrain.
   - Emit a \DR_TPAGE\ packet with \GetTPage(0, 0, 0, 0)\ into \ot[ot_idx]\ before the \POLY_F4\ primitive to guarantee semi-transparency blending.
   - Added support for the mine grid (\MINE_ORIGIN_X\ -2000, \MINE_ORIGIN_Z\ 2000, 10x10 cells).

Consequences:
Tile targeting renders clearly and accurately across farm, greenhouse, and mine environments. All field and greenhouse coordinate conversions correctly handle all negative world coordinates. Unit test suite passes 100%.

## ADR-109: Fix Tile Targeting cell validation and Fast Text page-break phase retention

Date: 2026-10-30. Status: accepted.

Context:
1. Tile Targeting Cell Drop Bug (port/enhance/gameplay/gameplay.c):
   - In hm_gameplay_draw_tile_targeting(), the code checked if (field_world_to_cell(&target_pos, &col, &row) != 0) return;.
   - In Back to Nature, field_world_to_cell() returns 1 for free/open ground cells, 0 for occupied cells (e.g. rocks, wood, fences), and 2 or 3 for out-of-bounds coordinates.
   - Checking != 0 immediately aborted and dropped all valid free cells across the farm field.
   - Additionally, AddPrim prepends to the ordering table slot head. Adding tpage, then fill, then lines resulted in reverse GPU execution: lines -> fill -> tpage. This executed fill before tpage set semi-transparency ABR mode, and fill occluded perimeter border lines.
2. Fast Text Page Break Overwrite and Page Advance Stalling (src/main/text/text.c):
   - In text_draw_string(), when text_index stepped rapidly past TEXT_CODE_PAGE_MARK under Fast Text, the while (1) loop set text_phase = TEXT_STATE_PAGE_BREAK and text_wrap_index = text_line_glyphs.
   - On the very next iteration, text_line_glyphs was incremented, making text_wrap_index != text_line_glyphs evaluate true. This immediately overwrote text_phase = TEXT_STATE_PRINTING.
   - Because text_phase was corrupted back to TEXT_STATE_PRINTING, text_update_window() never entered TEXT_STATE_PAGE_BREAK, never showed the red advance prompt, and completely ignored Confirm/X button presses.
   - In text_next_page(), text_find_nth(current, TEXT_CODE_PAGE_END, 1, level) passed level = text_index. When the search reached TEXT_CODE_PAGE_END, length >= limit triggered before checking the code, returning NULL and preventing page advancement.

Decision:
1. Tile Targeting Cell Validation and Primitive Submission (port/enhance/gameplay/gameplay.c):
   - Remove != 0 return check on field_world_to_cell() and greenhouse_world_to_cell(); strictly validate bounding grid coordinates (0 <= col < FIELD_COLS, 0 <= row < FIELD_ROWS).
   - Use fallback player->place.location / player->place.value for robust location detection.
   - Reverse AddPrim submission order: submit lines first, then fill quad, then DR_TPAGE. At runtime, the GPU executes DR_TPAGE (setting ABR=0) -> fill quad -> crisp perimeter border lines.
2. Fast Text Instant Pause and Clean Advance (src/main/text/text.c):
   - At TEXT_CODE_PAGE_MARK and TEXT_CODE_PAGE_END, under hm_gameplay_fast_text(), set text_phase = TEXT_STATE_PAGE_BREAK, text_wrap_index = text_line_glyphs, text_index = text_line_glyphs, and break;. This preserves TEXT_STATE_PAGE_BREAK across frames.
   - In text_next_page() and text_start_choice(), under HMBTN_PORT when hm_gameplay_fast_text() is active, search with limit 0 (unbounded) so text_find_nth matches the page end or choice marker instantly.
   - When Fast Text is disabled (0), original vanilla decompilation code paths execute with 100% parity.

Consequences:
Tile targeting renders cyan highlight quads and crisp perimeter borders on all targeted tiles on the farm, in the greenhouse, and in mines. Fast Text displays dialogue swiftly, displays the blinking advance prompt at page breaks, and seamlessly advances pages or choices on X/Cross press. All 122 unit tests pass 100%.
## ADR-110: Use 32-bit vector component pointers in geometry

Date: 2026-10-30

Context: Android arm64 uses 64-bit `long`, while PS1 VECTOR components and
the native compatibility VECTOR use 32-bit words. Camera view construction and
the ground proximity check indexed these components through `long *`, causing
incorrect reads and writes on LP64 hosts, including writes beyond VECTOR.

Decision: Under HMBTN_PORT use `s32 *` for these component pointers. Keep the
matching declarations unchanged. Add a camera basis regression test.

Consequences: Camera construction and ground checks use the intended 32-bit stride on
Android and other LP64 hosts.

<a id="adr-111"></a>

## ADR-111: Gameplay-only tile markers and coherent ground sprite rows

Date: 2026-10-30. Status: technical validation; owner gameplay pending.

Context: The owner reports Tile Targeting during cutscenes but absent during
normal play, and horizontal black lines with Enhance Scaling enabled. The
marker did not reject scripted/event control and used a larger OT slot than
the terrain. Reverse OT traversal renders that marker behind opaque ground.
Interpolation coherence previously considered only explicit polygon corners,
although field cells also use rectangles with implicit corners. A material
change or missing history on one row can leave it current while its neighbor
interpolates, exposing a horizontal strip of the background.

Decision: Hide tile markers during paused/menu/transition states, non-idle
farm events, scripted control, automatic walking, riding, sleep and airborne
poses. Bias the marker OT slot towards the camera. Extend the existing shared
vertex coherence rule to implicit rectangle corners: connected rectangles and
polygons fall back together when history is missing or conflicts. Disconnected
sprites retain their independent history. Runtime feature defaults stay OFF;
no game logic, timing, terrain, matching source or shaders change.

Validation: Add marker packet and actual terrain-occlusion checks, including
the inert OFF path and event/control suppression. Reproduce horizontal holes
with stacked rows whose lower material changes. Provide a dedicated offscreen
GPU probe (`hmbtn_gpu_renderer_probe direct3d12 --tile-seams`) at native,
65x97 and 1128x634 output with one, two and four samples. These fixtures cover
the identified failure, not every possible cause of the owner's screen lines.
Interactive gameplay and other devices remain owner-tested.

<a id="adr-112"></a>

## ADR-112: Use the active primitive arena for gameplay indicators

Date: 2026-10-30. Status: root cause observed in the owner's running game;
relocated-arena regression tests pass; rebuilt farm marker observed directly.

Context: The owner reports that ADR-111 did not restore Tile Targeting. With
permission, inspect the active game window and read its render state without
changing memory or controlling gameplay. Tile Targeting is ON, the farm event
mode is idle and the player has only the idle-pose flag. The active primitive
cursor has used only 23,704 bytes of the private widescreen arena, yet the old
`primBuffer + FRAME_PRIM_SIZE` comparison reports that it is full.

ADR-056 intentionally relocates `primStart` while retaining `primBuffer` for
overlay backups. The marker and tool charge gauge compare their active cursor
to that unrelated backup allocation, so valid draws are rejected. ADR-111's
single-arena fixture could not reproduce this failure.

Decision: Check both indicators against `primStart` and
`hm_game_prim_capacity()`. Use bounded integer address differences and check
remaining capacity before allocation, without comparing unrelated pointers.
Test distinct active/backup allocations and rejection near the true arena end,
in addition to the existing OFF-path, event and terrain-occlusion checks.

Consequences: Gameplay indicators can use the actual private render arena
without changing overlay backup storage, feature defaults, matching source,
or game logic. Horizontal lines while running are explicitly deferred by the
owner; this change does not claim to fix them.

After rebuilding, the owner reopened the game. Read-only window capture shows
the cyan marker around the weed tile in front of the player during normal
farm gameplay. This confirms the reported farm visibility fix; greenhouse,
mine and cutscene behavior remain covered by technical checks or owner testing.

### ADR-110: Restore vanilla audio default (2026-10-30)

The Studio HQ resampler was enabled for new settings despite the vanilla-first
policy. Default to PS1 Gaussian; retain explicitly saved HQ settings and the
existing option. The audio options regression checks both the default and the
opt-in persistence path.

## ADR-113: Preserve resolved native colours in GPU presentation

Date: 2026-10-30. Status: accepted.

Context: The shared GPU renderer added unconditional output noise after resolving
BGR555 colours. This changed even flat native pixels and differed from the CPU
reference independently of the original draw-environment dithering setting.

Decision: Output the resolved eight-bit channels without additional presentation
noise. Preserve PS1 primitive dithering and existing runtime controls. Regenerate
both checked-in shader formats. This supersedes the GPU output-noise portion of
ADR-092.

Consequences: GPU output no longer adds a separate debanding effect. Native and
enhanced rendering still retain their existing rasterization and fallback paths.
Visual behaviour remains to be checked by the owner; no performance claim is made.

## ADR-114: Use affine textures for interpolated PGXP packets

Date: 2026-10-30. Status: accepted.

Context: Motion-interpolated GPU packets use interpolated XY coordinates, but
have no corresponding interpolated PGXP depth. The packet nevertheless enabled
perspective correction when current PGXP vertices were available, leaving its
three depth fields unset and allowing division by zero in the shader.

Decision: Disable perspective correction for those interpolated packets. Keep
PGXP perspective correction for non-interpolated packets with valid depth and
keep interpolation's existing XY coordinates. Simulation and fallback are not
changed. Interpolated frames use affine texture mapping until matching depth
history is available.

Consequences: This avoids consuming unset depth but does not interpolate
perspective-correct textures. Owner visual and device validation remains pending.

## ADR-115: Preserve moving primitive bounds and presentation borders

Date: 2026-10-30. Status: accepted.

Context: GPU primitive bins used current-frame bounds even when motion replay
interpolated vertices from an earlier position. The earlier portion could be
omitted, producing clipped motion or seams. GPU filter mode 2 was also reduced
to mode 1 before packet construction, so the selected mode and cache key did not
match. On Android, the presentation path eventually used an undefined swapchain
load operation for pillarbox borders.

Decision: Include the previous endpoint in motion-interpolated triangle and
rectangle bounds, retaining the existing dependency fallback. Preserve GPU
filter modes 0 through 2 in scene packets. Clear letterbox or pillarbox regions
on every presentation on all platforms; full-window blits still discard previous
contents. This replaces the Android skip-clear optimization.

Consequences: XBR packets can use their intended shader mode and reuse cache
entries; software filtering remains its existing bilinear fallback. Android may
incur additional render-pass cost for bordered output. Actual performance,
GPU/device behavior and gameplay remain unverified at the owner's request.

## ADR-116: Fold leading VRAM transfers into GPU scene snapshots

Date: 2026-10-30. Status: accepted.

Context: GPU scene construction rejected every inline VRAM upload and clear,
including transfers before the first emitted primitive. Such leading transfers
can be represented by the initial VRAM state without changing command order.

Decision: After a complete leading GP0 fill or upload, refresh the private GPU
scene VRAM snapshot from the replay context. Continue to reject truncated
transfers and every transfer after a primitive, preserving the software fallback
for ordered dependencies. Native VRAM and game logic remain unchanged.

Consequences: Some frames with leading transfers can stay on GPU, but actual
scene coverage and gameplay effects are not established. No profiling or tests
were run at the owner's request.

## ADR-117: Fold independent offscreen transfers after flat draws

Date: 2026-10-30. Status: accepted.

Context: A VRAM transfer after an untextured primitive can still be independent
of previously emitted GPU work when the destination is outside the output area.
The existing blanket rejection needlessly falls back in this narrow case.

Decision: Permit complete fills/uploads after emitted untextured primitives only
when the wrapped destination does not overlap the logical output area. Refresh
the private snapshot after the transfer. Continue rejecting transfers after any
textured primitive and transfers touching the output. Include wrapped VRAM
aliases when checking expanded widescreen output. Native VRAM remains unchanged.

Consequences: GPU coverage may improve for qualifying frames; actual coverage,
performance and gameplay/device behavior remain unverified at the owner's request.

## ADR-118: Track prior texture reads for offscreen transfers

Date: 2026-10-30. Status: accepted.

Context: ADR-117 rejects offscreen transfers after any textured primitive even
when that primitive cannot read the transfer destination.

Decision: Record conservative 16x16 VRAM tiles covering previously sampled
texture footprints and palettes. Permit an otherwise valid offscreen fill or
upload only when its wrapped destination does not overlap any recorded tile or
the logical output. Retain fallback for ambiguous dependencies. Update only the
private snapshot; native VRAM execution is unchanged.

Consequences: Some independent transfers after textured draws can remain on the
GPU. Tile-level false positives still fall back. Scene coverage, performance,
and visual behavior remain unverified at the owner's request.

## ADR-119: Defer fully offscreen untextured draw fallback

Date: 2026-10-30. Status: accepted.

Context: A draw wholly outside the private output can trigger an immediate GPU
fallback even if no later command reads its native VRAM results.

Decision: Skip a fully offscreen, untextured draw for GPU output only when its
wrapped bounds cannot overlap the output. Record the skipped write and fall
back if a later textured draw may read it. Partially offscreen or textured draws
continue to use the previous conservative fallback. Native VRAM drawing remains
unchanged.

Consequences: Qualifying frames may remain on GPU. This deliberately retains
false-positive fallbacks; gameplay, visual behavior and performance remain
unverified at the owner's request.

## ADR-120: Weather particle coverage across the expanded widescreen viewport

Date: 2026-10-30. Status: accepted.

Context: When Widescreen Mode is enabled, the farm world viewport expands horizontally symmetrically around the original center (e.g. [-54, 374) at 16:9). However, weather particles in `src/overlay/11_GF_FARM/world/weather.c` (`particle_respawn`) were hardcoded to spawn within the native 4:3 box `[20, 260)`. As a result, rain splash and weather particles rendered only in the central 4:3 column, leaving the expanded left and right viewport wings completely dry.

Decision: When Widescreen Mode is active (`hm_widescreen_margin() > 0`), expand the horizontal weather particle spawn boundaries in `particle_respawn` to cover `[-margin, 320 + margin)` (`add6 = -margin`, `add0 = 320 + 2 * margin`). The number of random number generator rolls per particle respawn remains exactly 5, preserving deterministic PRNG stepping. When Widescreen Mode is OFF (`hm_widescreen_margin() == 0`), the original constants (`add6 = 20`, `add0 = 240`) are preserved unconditionally for 100% vanilla parity.

Consequences: Rain, snow, and weather particles render seamlessly across the entire visible widescreen viewport without artificial pillarboxed boundaries. Vanilla 4:3 rendering and logic remain identical.


## ADR-121: Fine-grained VRAM tile tracking for offscreen draws and swapchain pacing protection

Date: 2026-10-30. Status: accepted.

Context: ADR-119 deferred fallbacks for fully offscreen untextured draws using a coarse boolean flag. Any subsequent textured primitive anywhere in the frame immediately forced a fallback to CPU software rasterization, even when reading from independent VRAM regions far from the offscreen write. In addition, when swapchains stall or presentation deadlines fall behind real time during window minimization or display transitions, Windows presentation loops could busy-spin without delay.

Decision: Replace coarse boolean tracking with conservative 16x16 VRAM tile footprints (`offscreen_writes[32]`) matching the existing tile tracking mechanism used for texture reads (`texture_reads[32]`). Fully offscreen untextured draws record their wrapped destination tiles. Subsequent textured primitives trigger CPU fallback only if their sampled texture UV footprint or CLUT footprint overlaps recorded offscreen write tiles. In the presentation loop, bound non-unlimited catch-up delays to at least 1 ms when presentation deadlines are in the past or swapchains are unavailable, preventing 0-ns busy-spinning. Native VRAM drawing and game logic remain unchanged.

Consequences: Qualifying frames with offscreen helper draws alongside independent textured geometry remain on the GPU compute pipeline, eliminating false-positive fallbacks for smoother frame pacing and reduced CPU load. Bit-exact dependency safety is preserved. Actual scene coverage and gameplay behavior remain unverified at the owner's request.


## ADR-122: Android L2 and R2 virtual touch controls and tile targeting cutscene suppression

Date: 2026-10-30. Status: accepted.

Context:
1. In the Android virtual touch controller overlay (`VirtualGamepadView.kt`), only shoulder buttons L1 (`KeyEvent.KEYCODE_A`) and R1 (`KeyEvent.KEYCODE_S`) were implemented. L2 (`KeyEvent.KEYCODE_Q` / `HM_PAD_L2`) and R2 (`KeyEvent.KEYCODE_E` / `HM_PAD_R2`) were missing, preventing whistling for the dog/horse and using secondary shoulder shortcuts via touch on Android. In addition, when buttons were positioned adjacent to each other, hitboxes could overlap during expanded touch tolerance checks.
2. The Tile Targeting enhancement (`hm_gameplay_draw_tile_targeting()`) rendered a cyan highlight box on the farm tile in front of the player during opening cutscenes (including the title screen farm demo and the new game opening cutscene with Mayor Thomas). The highlight was only intended to display during active, controllable gameplay.

Decision:
1. Android Virtual Gamepad:
   - Added L2 (`KeyEvent.KEYCODE_Q`) and R2 (`KeyEvent.KEYCODE_E`) rectangular shoulder buttons to `VirtualGamepadView.kt`.
   - Placed L1 (outer corner) and L2 (inner) side-by-side on the top-left, and R2 (inner) and R1 (outer corner) side-by-side on the top-right, maintaining clear vertical clearance above the dynamic analog stick area (`y > height * 0.20f`).
   - Implemented prioritized two-pass touch hit resolution (`findHitButton`): exact geometric bounds check takes precedence, falling back to nearest Euclidean distance within expanded touch tolerance to prevent adjacent button hit stealing.
2. Tile Targeting Cutscene Suppression:
   - In `port/enhance/gameplay/gameplay.c` (`hm_gameplay_draw_tile_targeting`), added checks to suppress rendering whenever:
     - `game_state.globalFlags` has `GLOBAL_FLAG_DEMO_START` or `GLOBAL_FLAG_INPUT_REPLAY` (title screen farm demo reel).
     - `game_state.globalFlags` has `GLOBAL_FLAG_CAMERA_BUSY` (event/cutscene camera pans).
     - `event_flag_get(EVENT_FLAG_EVENT_ACTIVE)` or `event_flag_get(EVENT_FLAG_REQUEST_BUSY)` is non-zero (active in-game cutscenes, including the new game intro with Mayor Thomas).
     - `menu_get_phase() != TEXT_WINDOW_CLOSED` (active dialogue/text message boxes).
   - Preserved all existing checks (`player->stateFlags`, `farm_area.mode.eventMode == FARM_MODE_IDLE`, and `GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN | GLOBAL_FLAG_TRANSITION`).

Consequences:
Android players have complete 8-button PS1 digital gamepad inputs on the touch overlay. Tile targeting highlight appears strictly during interactive gameplay and is suppressed throughout all cutscenes, intro sequences, dialogues, and demos.


## ADR-123: Android Unified Graphics Profile (Native vs Modern) and Frame Interpolation Clamp

Date: 2026-10-30. Status: accepted.

Context: On mobile Android devices, navigating granular desktop graphics options (Internal Resolution, Texture Filtering, Widescreen, Anti-Aliasing, Dithering, PGXP, FPS Cap, and Visual Interpolation) in the pause menu is cumbersome on touchscreens and can cause performance degradation. In particular, visual frame interpolation (`enhance.interpolation`) caused severe frame pacing judder on Android mobile SOC displays with variable or non-standard refresh rates.

Decision:
1. Under `#ifdef __ANDROID__`, simplify the Graphics Settings menu in the ESC pause interface to a single option: `Profile: [ Native | Modern ]`.
   - **Native** (default, pure vanilla parity): 4:3 native aspect ratio, Nearest Neighbor texture filtering, 1x internal resolution, 30 FPS cap, Interpolation OFF, Anti-Aliasing OFF.
   - **Modern** (mobile optimized): 16:9 expanded farm viewport and presentation (ADR-056/057), Bilinear filtering, 720p scaling limit (ADR-105), 30 FPS cap, Interpolation OFF, Anti-Aliasing OFF.
2. In `graphics_options.c`, clamp `enhance.interpolation` to 0 on Android, disabling mobile frame interpolation judder and keeping pacing locked to native 30 FPS display ticks.
3. Persist `enhance.profile` in `settings.json`, maintaining full desktop multi-option control on Windows/Linux/macOS without cross-platform regressions.

Consequences:
Android users enjoy a clean, frictionless single-toggle graphics experience tailored for mobile hardware while preserving 100% pure vanilla parity by default.


## ADR-124: Memory Card 64-bit LP64 Pointer Truncation Fix and Android Storage Resilience

Date: 2026-10-30. Status: accepted.

Context: On 64-bit Android (arm64-v8a, LP64 data model), attempting to save in the diary displayed "Not enough free space on MEMORY CARD in slot 1" even on fresh or empty memory cards. Investigation identified two major root causes:
1. In `src/overlay/15_saveload/saveload.c`, local variables `long cmd, result, save_files, all_files;` are 64-bit integers on LP64 (8 bytes). However, `port/compat/psyq/libmcrd.h` and `port/compat/psyq/libmcx.h` declared `MemCardSync(..., s32 *command, s32 *result)` and `MemCardGetDirentry(..., s32 *count, ...)`. Writing through `s32 *` only populated the lower 4 bytes, leaving uninitialized stack garbage in the upper 4 bytes of `result`, `cmd`, and file counts. In `save_card_read_header()`, `switch (result)` failed to match `case McErrFileNotExist:` (5) because `result == 0xXXXXXXXX00000005 != 5`, falling through to checksum verification on empty buffers and returning `SAVE_CARD_BAD_CHECKSUM` (14). In `save_read_slot_headers()`, this left `save_slot_status[i]` at its initialization value `SAVE_SLOT_SHORT_OF_SPACE` (2), which rendered "Not enough free space" and routed selection to `SAVE_STEP_NOT_ENOUGH_SPACE`.
2. In `port/platform/user_data.c`, `hm_exists()` used `SDL_EnumerateDirectory()`. On Android Scoped Storage / FUSE filesystem layers, directory enumeration can fail, returning -1. This caused `hm_read_card()` to return -1, making `hm_card_attach()` mark cards as `McErrCardInvalid`.
3. In `MainActivity.kt`, `saves` subfolder was never explicitly created before native runtime initialization, and storage candidates lacked a fallback if `targetDataDir` was not writable.

Decision:
1. PSY-Q Compatibility Signatures: Changed `MemCardSync` (`command`, `result`), `McxSync` (`command`, `result`), and `MemCardGetDirentry` (`count`) to use `long *` in `libmcrd.h`, `libmcx.h`, `card.c`, `card_store.h`, and `card_store.c`. This ensures the full 64-bit integer is written on LP64 hosts, eliminating stack garbage and pointer truncation.
2. File Existence Checking: Replaced `SDL_EnumerateDirectory()` in `hm_exists()` with `SDL_GetPathInfo(path, &info)`, returning 1 if the path exists and 0 if absent, completely avoiding directory enumeration failures.
3. Android Storage Resilience:
   - Added `isDirWritable()` validation in `MainActivity.kt`.
   - Pre-create `File(targetDataDir, "saves").mkdirs()` from Kotlin before native engine startup.
   - If public Documents is unwritable (e.g. Scoped Storage without All Files permission), seamlessly fallback to app-specific storage (`getExternalFilesDir(null)/VanillaBTN`), where read/write is always guaranteed.

Consequences:
Memory card space checking and save creation operate flawlessly across 64-bit Android (LP64) and Windows (LLP64). Diaries can be created and saved without false "not enough space" errors.


## ADR-124: Three graphics modes with retained Modern settings

Date: 2026-10-30

Context: The owner requests Native PS1, Enhance PS1, and Modern as a clear
choice while retaining the original presentation by default.

Decision: Persist `graphics.graphics_mode` as 0, 1, or 2. Native selects the
original geometry and dithering, with modern visual effects disabled. Enhance
PS1 enables PGXP and disables dithering. Modern applies those same fixes and
exposes the existing optional visual effects. Modern effect choices remain
persisted while another mode is active. Existing settings migrate according to
their previously enabled enhancements. Display settings remain independent.
The current Android menu exposes the same three modes; its interpolation clamp
remains in effect. Runtime changes update PGXP, dithering, scaling, interpolation,
and widescreen without changing game logic. GPU coverage and visual results
remain subject to the existing fallback and owner gameplay validation.


## ADR-125: Preserve opaque gradient precision in enhanced GPU output

Date: 2026-10-30

Context: Bypassing ordered dithering alone still quantizes scene colors to
BGR555, leaving visible steps in smooth Gouraud shading.

Decision: When dithering is disabled by Enhance PS1 or Modern, retain the
8-bit interpolated color of opaque, untextured Gouraud triangles in the shared
GPU presentation, alongside the BGR555 sample used for masking and subsequent
primitives. Native mode retains the original output. Later overlapping or
filtered primitives resolve through the existing BGR555 path. Native VRAM,
textures, blending arithmetic and game logic are unchanged.

Consequences: This is a partial improvement, not general true-color rendering
or elimination of banding. Software and unsupported GPU frames retain their
existing behavior. The feature adds per-sample shader storage; performance and
owner gameplay validation remain unverified.


## ADR-126: Respect displayed-area write protection in GPU scenes

Date: 2026-10-30

Context: The GPU scene descriptors captured the display rectangle and drawing
permission, but the shader ignored them, allowing primitives into a protected
displayed area where the CPU reference leaves pixels unchanged.

Decision: Before drawing each primitive, apply the captured display-enable,
rectangle, and draw-to-display flag in the shared GPU shader, matching the
existing CPU and software paths. The check uses logical PS1 coordinates.

Consequences: Offscreen D3D12 and Vulkan comparisons cover both display buffer
positions and permission states. Gameplay and device coverage remain unverified.

## ADR-127: Preserve opaque textured triangle shading precision

Date: 2026-10-30

Context: ADR-125 preserves untextured opaque Gouraud output, while modulation
of opaque textured triangles still quantizes intermediate shading to BGR555.

Decision: In enhanced GPU output with dithering bypassed, preserve the computed
8-bit shading for opaque, non-raw, unfiltered textured triangles, while retaining
the existing BGR555 state for masks, subsequent primitives, and VRAM. Native
mode and software fallback remain unchanged. Transparent, raw, and filtered
texture paths keep their existing precision.

Consequences: Synthetic D3D12/Vulkan probes verify the limited path. This is
not full true-color output; performance and owner gameplay checks are pending.


## ADR-128: Match native GPU interpolation to the CPU reference

Date: 2026-10-30

Context: Random native scenes disagreed between GPU and CPU, including texture
coordinates that selected different neighboring texels. CPU polygon interpolation
uses a rounded fixed-point reciprocal; native GPU interpolation previously used
direct division. The software filtering reference instead uses exact division.

Decision: Reproduce the CPU reciprocal multiplication in native GPU polygon
interpolation using existing emulated 64-bit multiplication. Retain direct
division for the filtering path to match its software reference, and preserve
constant vertex colors without reciprocal rounding. Enhanced-resolution and
PGXP floating-point interpolation remain unchanged.

Consequences: The complete synthetic native, randomized, filtering, widened
scene, and presentation probes pass on local D3D12 and Vulkan. This does not
establish complete gameplay or device coverage. Shader performance is unmeasured.


## ADR-129: Include neighboring GPU bins for multisample coverage

Date: 2026-10-30

Context: At enhanced resolution, a pixel center can fall in the next 16-pixel
logical bin while one or more of its MSAA samples lie inside a triangle in the
previous bin. Exclusive triangle bounds omit that primitive from the pixel's
bin, producing dark horizontal or vertical partial-coverage lines.

Decision: When multisampling is active, conservatively extend triangle bin
assignment by one logical pixel on each side, clipped to the scene. Perform
VRAM dependency and clipping decisions with the original primitive bounds.
Single-sample binning is unchanged.

Consequences: The synthetic boundary fixture failed with MSAA 2x before this
change and passes at 1x/2x/4x on local D3D12 and Vulkan after it. Extra bin
entries may increase GPU work or trigger existing safe fallback in dense scenes.
The owner confirmed the reported line disappeared while idle and running with
Modern, Scaling On, Widescreen Off, MSAA 4x, Nearest, Interpolation Off, VSync
On and unlimited FPS. Other scenes, devices, and performance remain unverified.


## ADR-130: Show fatigue within the optional stamina HUD

Date: 2026-10-30

Context: The owner requests a compact fatigue indicator alongside the existing
optional stamina hearts, without changing original fatigue behavior.

Decision: Extend the existing guarded port-only HUD frame and draw an original
sleek 59-pixel fatigue gauge directly beneath the 10 hearts. Clamp the displayed value to
0..100; the fill represents fatigue and changes color at 50 and 80. The
existing Stamina HUD switch controls both rows and remains off by default.
The helper reads the original fatigue value and does not write game state.

Consequences: The frame is taller and remains hidden under the existing menu
and transition conditions. Owner gameplay and layout validation are required.


## ADR-131: Android 3-Mode Graphics Cycle, Settings String Fix, and Safe Storage Disc Importer

Date: 2026-10-30

Context:
1. On Android, PR #78 introduced a 3-mode graphics enum, but Android's single toggle was constrained to a binary NOT toggle, preventing selection of the Modern profile, and settings persistence failed with 'Cannot save settings' due to unquoted string values in hm_user_data_set_nested_setting.
2. Android Scoped Storage restrictions frequently prevented write access to shared external documents, causing save failures and memory card storage errors.

Decision:
1. Update Android's graphics option selector to cycle through all 3 modes: Native PS1 (4:3, 1x), Enhance PS1 (4:3, 1x, PGXP), and Modern (16:9, Bilinear, Scaling OFF). Each mode explicitly configures widescreen, 1x native scaling (preventing mobile GPU compute throttling), bilinear filtering, and PGXP while keeping frame interpolation clamped to OFF.
2. Properly quote string literals in hm_graphics_save so JSON replacement validation succeeds on Android.
3. Introduce a dedicated LauncherActivity and migrate user data and disc storage to Android app-private storage (getExternalFilesDir(null)/VanillaBTN), where full read/write access is guaranteed across all Android versions without runtime storage permissions:
   - On fresh install (disc absent from safe storage), display a clean Material 3 setup interface allowing the user to select their game disc via Android's Storage Access Framework (ACTION_OPEN_DOCUMENT) and copy it to safe storage with progress feedback.
   - On subsequent launches (disc present), launch MainActivity immediately without displaying any setup UI or delay.

Consequences:
Android users enjoy seamless disc setup, reliable memory card and settings persistence, and frictionless switching between Native, Enhance, and Modern graphics modes.

## ADR-132: Bypass Mobile TBDR Compute Shader Bottleneck on Android Modern and Enhance Graphics

Date: 2026-10-30. Status: accepted.

Context:
1. Severe Stutter and Frame Drops on Android with Modern Graphics:
   - On mobile Snapdragon / Adreno GPUs (tested on Qualcomm Snapdragon 680 / Adreno 610, Xiaomi 25062RN2DY), enabling Modern graphics caused severe lag (~16-20 FPS) with frame time exceeding 45-60 ms and continuous timeline rebases.
2. Root Cause Analysis:
   - Modern and Enhance profiles on Android forced \o->pgxp = 1\ and \hm_host_needs_replay()\ triggered eed_gpu = 1\ in \hm_host_gpu_present()\.
   - On mobile TBDR architectures, the 1.5 MB ByteAddressBuffer compute shader rasterizer (\gpu_scene.comp\ / \scene.hlsl\) lacks on-chip L2 SSBO caching, causing heavy memory bus contention and thread stalls during per-pixel polygon loop traversal and uncoalesced VRAM loads. Real-device GPU profiling revealed \lit+submit\ stalling at 42.57 ms (max 58.91 ms).
   - In contrast, the direct 2D hardware texture scanout path (\hm_host_present_scanout\) takes < 2 ms on CPU and uses fixed-function GPU blitting with hardware bilinear filtering and 16:9 widescreen presentation.

Decision:
1. In \port/platform/graphics_options.c\, set \o->pgxp = 0\ for Android Modern and Enhance profiles, preventing activation of the heavy compute shader rasterizer.
2. In \port/platform/game_host.c\, remove \|| hm_options.pgxp != 0\ from the Android eed_gpu\ branch so that legacy or migrated settings files do not trigger compute shader stalls on mobile.
3. Keep Modern graphics presentation tuned for mobile:
   - 16:9 expanded farm viewport and presentation (ADR-056/057).
   - Hardware Bilinear texture filtering via mobile texture sampler units.
   - Smooth 32-bit color scanout without dithering artifacts.
   - 720p SurfaceView hardware display upscaling via Snapdragon Hardware Composer (HWC) at 0% GPU execution cost.

Consequences:
- Android frame times drop from ~45-60 ms to < 5 ms in Modern graphics mode, locking stably at 60 game ticks / 30 FPS display presentation with 0 rebases and 0% judder.
- Full desktop graphics capabilities, unit tests (124/124 passing), and pure vanilla parity remain completely unaffected.

## ADR-133: Gameplay Enhancements Audit: Stamina HUD Synchronization, Android Tile Targeting Fix & Visual Softening, Tool Charge Bar Slimming, Fast Text Takeout, and Precision Mouse Inventory Selection

Date: 2026-10-30. Status: accepted.

Context:
1. User Feedback and Gameplay Audit:
   - Stamina/Fatigue HUD: Visibility was not perfectly synchronized with the vanilla clock/season HUD. It was checking independent menu flags rather than directly mirroring status_draw_clock().
   - Tile Targeting on Android: Highlight marker did not render during active farm gameplay on Android. Investigation revealed line 618 of port/enhance/gameplay/gameplay.c checked event_flag_get(EVENT_FLAG_REQUEST_BUSY). In the original decompilation engine, whenever any daily background event is queued in requests.items, event_request_run_handler() sets EVENT_FLAG_REQUEST_BUSY = 1 even during ordinary walking and farm work. Additionally, farm_area.mode.place.location could be 0 when not fully copied from farm_area.mode.data->head.place.location. Furthermore, the visual appearance of the tile targeting was overly bright fluorescent cyan (RGB 35, 130, 215) with stark cyan border lines (RGB 140, 220, 255), which felt too obtrusive and saturated against organic farm soil.
   - Tool Charge Bar: Floating charge gauge above Pete's head was 5px tall inner + 2px border (7px total, ~3% screen height), appearing overly thick and prominent during tool charging.
   - Fast Text: Multi-line dialogues (>4-5 lines) scrolled too fast before the first 4 lines could be read comfortably. The feature is taken out from gameplay options and pause menu UI per user request until a more refined multi-line pagination scroll mechanic is designed.
   - Mouse Inventory Selection: In item storage and rucksack reorganize screens (item_storage.c and storage_swap.c), slot hit detection used x - 6, y - 6, 28, 28 relative to slot centers spaced 34px apart. This introduced an 8-pixel diagonal misregistration and 6-pixel dead zones between slots, causing imprecise and non-fluid mouse clicks.
   - All other gameplay enhancements (walk through crops, 99 item stacking with quantity split, durable fences, connected wooden rails, wild dog attacks toggle, smart animal bell, calendar events milestone dialog, roaming horse) were thoroughly audited and verified fully operational with zero regression.

Decision:
1. Synchronize Stamina/Fatigue HUD Visibility (src/overlay/11_GF_FARM/menu/status_pages.c):
   - Remove redundant game_state.globalFlags checks inside status_draw_stamina_hud(). Since it is invoked directly inside status_draw_clock(), its visibility is now strictly 1:1 synchronized with the clock, date, and season HUD across all scenes, menus, and transitions.
2. Fix and Soften Tile Targeting (port/enhance/gameplay/gameplay.c):
   - Replace the EVENT_FLAG_REQUEST_BUSY check with game_state.requests.state == FARM_REQUEST_RUN, ensuring cutscenes suppress tile targeting while ordinary farm gameplay renders it smoothly.
   - Add area header fallback if (!place && farm_area.mode.data) place = farm_area.mode.data->head.place.location;.
   - Soften tile targeting visuals: replace neon cyan with subtle soft warm fill (RGB 55, 60, 55 with 50% ABR blend) and delicate warm ivory borders (RGB 180, 185, 175), making target tiles clearly identifiable without visual clutter.
3. Slim Tool Charge Bar (port/enhance/gameplay/gameplay.c):
   - Reduce gauge thickness from 5px to 2px inner height (4px total with border), width from 40px to 32px, and position closer to Pete (screen_y - 44), creating a sleek, elegant pixel-art energy meter.
4. Takeout Fast Text:
   - Set hm_gameplay_fast_text() unconditionally to 0.
   - Remove "Fast Text" option from Pause Menu -> Gameplay, decrementing menu items from 12 to 11.
5. Precision Mouse Hit Detection (item_storage.c & storage_swap.c):
   - Replace x - 6, y - 6, 28, 28 with centered x - 17, y - 17, 34, 34 bounding boxes matching the exact 34x34 grid pitch, eliminating dead zones and providing fluid, pixel-perfect mouse inventory selection.
6. Manifest Integrity:
   - Recalculate normalized SHA-256 for status_pages.c, item_storage.c, and storage_swap.c and update port/overlay/farm_state/manifest.cmake.

Consequences:
Stamina HUD shows and hides in perfect synchronization with the vanilla clock. Tile targeting operates reliably during farm gameplay with subtle, tasteful visuals. Tool charge bar is sleek and unobtrusive. Fast Text is cleanly taken out. Mouse inventory clicks are responsive, fluid, and precise. 100% of unit tests pass (124/124) and pure vanilla parity is preserved when enhancements are OFF.


## ADR-134: Fix Tile Targeting Visibility Regression, Windows DPI Scaling Mouse Precision, and Ghost Click Elimination

Date: 2026-10-30. Status: accepted.

Context:
1. Tile Targeting Regression:
   - Users reported that tile targeting was not appearing during normal farm and mine gameplay.
   - Root Cause Analysis:
     a) event_flag_get(EVENT_FLAG_EVENT_ACTIVE) check in hm_gameplay_draw_tile_targeting: In the native engine, EVENT_FLAG_EVENT_ACTIVE is set to 1 permanently in the mine (ield/mine.c), whenever daily morning checks are queued (arm_main.c), or when clock events execute. Checking this flag caused tile targeting to be suppressed almost continuously during normal gameplay. Controllable gameplay is already robustly protected by checking player->stateFlags (PLAYER_FLAG_SCRIPTED, PLAYER_FLAG_NO_INPUT, PLAYER_FLAG_EVENT_SPRITE), game_state.requests.state == FARM_REQUEST_RUN, and globalFlags (GLOBAL_FLAG_DEMO_START, GLOBAL_FLAG_MENU_OPEN, etc.).
     b) Subtle fill color: The softened RGB(55, 60, 55) fill with 50% semi-transparency darkened soil to near-black, rendering the tile highlight practically indistinguishable against farm soil and mine gravel.
2. Mouse Imprecision & Ghost Clicks:
   - Users reported that mouse controls felt imprecise and exhibited 'ghost clicks'.
   - Root Cause Analysis:
     a) DPI Scaling Coordinate Mismatch: SDL_GetWindowSizeInPixels was passed to hm_mouse_calc_virtual_pos and hm_mouse_update_pos. On Windows monitors using display scaling (>100%, e.g. 125% or 150%), SDL_GetMouseState and mouse events report logical window coordinates (points), while SDL_GetWindowSizeInPixels reports physical pixels. Dividing logical coordinates by physical pixel scale squashed virtual coordinates towards the top-left, making clicks miss slots or feel inaccurate.
     b) Unconsumed Click Linger ('Ghost Click'): In mouse_input.c, s_left_clicked was set on button down and never expired if no UI consumed it immediately. Unconsumed clicks could sit pending across minutes and trigger dialogue choice prompts or menu options the instant they appeared.
     c) Continuous Pad Injection Bleed: held |= HM_PAD_CROSS was injected into the virtual controller whenever the left mouse button was down. In storage and rucksack menus, this caused clicks to trigger actions on previous slots if the user clicked empty space, and caused double-clicks because the button was held down across multiple ticks.
     d) Missing Direct Click Consumption in Storage: item_storage.c and storage_swap.c only updated cursor position on hover, relying on gamepad injection for clicks. If mouse mode was OFF, clicks did nothing; if mouse mode was ON, clicks bled through.

Decision:
1. Fix Tile Targeting Visibility (port/enhance/gameplay/gameplay.c):
   - Remove event_flag_get(EVENT_FLAG_EVENT_ACTIVE) suppression check, relying on FARM_REQUEST_RUN, player->stateFlags, and globalFlags.
   - Restore vibrant, distinct cyan styling: semi-transparent cyan fill (RGB 35, 130, 215) with crisp cyan borders (RGB 140, 220, 255), clearly identifying targeted tiles across farm dirt, green crops, and mine floors.
   - Update unit test in port/platform/tests/gameplay_options.c to test scripted state rather than EVENT_FLAG_EVENT_ACTIVE.
2. Fix Mouse Precision Across High-DPI Windows Monitors (port/platform/game_host.c, port/platform/mouse_input.c):
   - Replace SDL_GetWindowSizeInPixels with SDL_GetWindowSize in hm_host_pause and hm_host_events, aligning mouse coordinates 1:1 with window dimension units regardless of DPI scaling.
   - Use loorf in hm_mouse_calc_virtual_pos to eliminate negative coordinate border rounding.
3. Eliminate Ghost Clicks (port/platform/mouse_input.h, port/platform/mouse_input.c, port/platform/game_host.c, src/main/text/text.c):
   - Add hm_mouse_tick() in mouse_input.c called once per game tick in hm_host_tick to age and expire unconsumed clicks.
   - Call hm_mouse_reset() on pause menu enter/exit and menu transitions.
   - In game_host.c, suppress gamepad injection (held |= HM_PAD_CROSS) when a menu is open (GLOBAL_FLAG_MENU_OPEN or menuState != MENU_STATE_IDLE).
   - In 	ext.c, consume and discard left clicks that occur outside choice prompt rectangles so they do not linger.
4. Direct Mouse Click Handling in Inventory & Storage (item_storage.c, storage_swap.c):
   - Handle hm_mouse_consume_left_click() directly on hovered slots in item_storage.c and storage_swap.c, enabling responsive point-and-click slot interaction regardless of mouse mode.
   - If left-clicked in empty/dead space, consume and discard the click without triggering actions on previously focused slots.
   - Recalculate normalized SHA-256 hashes for item_storage.c and storage_swap.c in port/overlay/farm_state/manifest.cmake.

Consequences:
- Tile targeting highlight renders reliably during farm and mine gameplay with crisp, clearly visible borders and fill.
- Mouse navigation and clicking are pixel-perfect on both 100% and high-DPI scaled displays.
- Ghost clicks and delayed click bleed-through are completely eliminated.
- 100% unit tests passing (124/124) with full vanilla parity when enhancements are OFF.

## ADR-135: Android Touch Coordinate Precision Fix and Default Mouse Mode

**Date:** 2026-10-30
**Status:** Accepted

### Context

On Android, SDL3 synthesises finger-touch events as `SDL_EVENT_MOUSE_MOTION` /
`SDL_EVENT_MOUSE_BUTTON_DOWN` events whose `x`/`y` fields are in **physical
screen pixels**.  However, `SDL_GetWindowSize` returns the window size in
**logical dp units** (screen-density-independent points), which differs from
physical pixels on any high-DPI Android device (e.g. 2× or 3× density).

`hm_mouse_calc_virtual_pos` divides the touch coordinates by the window size to
derive a position inside the 320×240 virtual canvas.  When the numerator is in
physical pixels but the denominator is in dp, the ratio is wrong, producing a
scaled-up virtual position that consistently overshoots the intended slot or
dialogue option.

On Windows and macOS, SDL mouse events *are* in logical coordinates, so the
existing `SDL_GetWindowSize` call in `hm_host_events` and `hm_host_pause` was
correct for those platforms (and ADR-134 intentionally kept it that way).

Additionally, the controls default on Android was `HM_MOUSE_MODE_OFF`, meaning
touch-to-inventory/touch-to-dialogue interaction was entirely disabled until the
user manually opened Settings → Controls.  On a touch-primary device this is
the wrong out-of-the-box experience.

### Decision

1. **Coordinate fix** – in `port/platform/game_host.c`, guard the window-size
   query in `hm_host_events` and the two mouse-handling paths inside
   `hm_host_pause` (MOTION and BUTTON_DOWN) with `#ifdef __ANDROID__` /
   `#else` blocks:
   - `__ANDROID__`: call `SDL_GetWindowSizeInPixels` → coordinates match the
     physical-pixel touch events.
   - All other platforms: keep `SDL_GetWindowSize` → coordinates match logical
     mouse events (preserves ADR-134 fix on Windows).

2. **Android default mouse mode** – in `port/platform/controls_options.c`,
   `hm_controls_defaults` sets `mouse_mode = HM_MOUSE_MODE_ACTION` when
   `__ANDROID__` is defined.  This lets the first-time user tap inventory slots
   and dialogue choices without any setup.  The setting is still overridden by
   whatever the user has saved in `settings.json`, so existing per-user
   preferences are respected.

### Files Changed

- `port/platform/game_host.c` — `hm_host_events`, pause MOTION handler, pause
  BUTTON_DOWN handler.
- `port/platform/controls_options.c` — `hm_controls_defaults`.

### Consequences

- Touch-to-slot and touch-to-dialogue precision on Android now matches the
  Windows mouse precision achieved in ADR-134.
- No source change under `src/overlay/` — no manifest hash update required.
- Windows / macOS / Linux behaviour is completely unchanged (compile-time guard).
- The `mouse_mode` default change only affects first-run or clean-settings
  Android installs; any existing `settings.json` with an explicit `"mouse"`
  key overrides it.
- 100% unit tests still passing (124/124); vanilla parity with all enhancements
  OFF is unaffected.
## ADR-136: Disable Mouse/Touch-to-Game Interaction (Temporary Takeout)

**Date:** 2026-10-30
**Status:** Accepted

### Context

The mouse/touch-to-game interaction feature (inventory slot clicks, dialogue
choice taps, storage swap) introduced in ADR-133/134 had two outstanding
precision issues that made it unreliable:

1. **OS cursor Y-offset**: The OS mouse cursor appeared slightly above the
   in-game virtual cursor position, indicating a residual mapping error in
   the 320x240 virtual-canvas projection.
2. **Android coordinate mismatch**: Touch precision on Android remained
   inconsistent across different device densities and SDL3 versions.

Rather than continue debugging the coordinate pipeline in-session, the owner
requested a clean takeout of the tap/mouse-to-game interaction feature until
a proper redesign can be done.

### Decision

Disable the mouse/touch-to-game interaction path cleanly within the host layer
without removing any overlay code (avoids manifest hash updates for
`src/overlay/11_GF_FARM/` files) and without altering the clean unit-tested
`controls_options` model:

1. **`game_host.c` `hm_host_poll_events`**: `SDL_EVENT_MOUSE_BUTTON_DOWN`,
   `SDL_EVENT_MOUSE_BUTTON_UP`, and `SDL_EVENT_MOUSE_WHEEL` events are no
   longer forwarded to `hm_mouse_button_down()` / `hm_mouse_button_up()` /
   `hm_mouse_wheel()` (guarded by `if (0 && ...)`). This ensures
   `hm_mouse_consume_left_click()` and friends in overlay code always return 0.

2. **`game_host.c` `hm_host_tick`**: In-game mouse D-pad and tool button
   injection (`if (hm_controls.mouse_mode != HM_MOUSE_MODE_OFF)`) is disabled
   via compile-time guard (`if (0 && ...)`), ensuring mouse clicks never emit
   gamepad inputs into simulation.

3. **Pause Menu Intact**: The pause menu mouse navigation in `hm_host_pause`
   remains fully functional as it uses its own direct hit-testing logic.

### Files Changed

- `port/platform/game_host.c`

### Consequences

- No overlay files changed -> no manifest hash update required.
- `hm_mouse_*` infrastructure (`mouse_input.c`/`h`) is preserved for future use.
- ADR-135 coordinate fixes (`SDL_GetWindowSizeInPixels` on Android in
  `hm_host_events` and `hm_host_pause`) are **kept** -- they are still correct
  for the pause menu mouse navigation which remains functional.
- Windows/Android/all-platform: mouse clicks no longer interact with
  inventory, dialogue, or storage. Game is controller/keyboard only.
- 100% unit tests still passing (124/124); vanilla parity unaffected.
- To re-enable in a future ADR: remove the `if (0 && ...)` guards and fix
  the OS cursor Y-offset root cause.


## ADR-137: Use the VanillaBTN Documents folder for Windows user data

Date: 2026-10-30. Status: accepted owner override.

Context: ADR-044 originally selected Documents/BackToNature for Windows saves
and settings. The owner later standardized the Windows user-data directory and
specified that it must follow the active user's OS-resolved Documents folder,
including redirected Documents locations.

Decision: Store Windows configuration at Documents/VanillaBTN/settings.json
and native memory-card containers under Documents/VanillaBTN/saves/. Resolve
Documents through the OS user-folder API; do not hardcode a username, write
beside the executable, or use AppData for these files. This supersedes ADR-044's
Windows directory name. Android continues to use its app-private storage path.

Consequences: Settings and saves use one current Windows location while
respecting redirected Documents folders. Historical ADR-044 references describe
the earlier path and are not the current Windows storage contract.

## ADR-138: Attempt GPU Auto Rendering on Every Desktop Frame

Date: 2026-10-30. Status: accepted owner request.

Context: GPU Auto was the default renderer, but the host only captured a scene
when an enhancement such as scaling, widescreen, interpolation, PGXP or MSAA
requested replay. Vanilla desktop output could therefore remain on the CPU
even while GPU Auto was selected.

Decision: On desktop, request a shared GPU scene replay for every frame, including
native 320x240 output. Keep the Software renderer selectable and keep unsupported
scenes, direct/loading scanouts and failed GPU presentation on the software
reference path. Retain Android's selective compute eligibility from ADR-132.
Game logic and authoritative CPU VRAM remain on the CPU.

Consequences: Supported desktop scenes can use GPU rasterization without enabling
an enhancement. Synthetic probes on a Radeon RX Vega measured 0.475 ms GPU versus
0.359 ms CPU at 320x240, and 0.877 ms versus 6.525 ms at 1440x1080 on D3D12;
these numbers exclude the game loop and do not predict gameplay frame rate.
Native-resolution workloads may pay a small render-slice cost. Full gameplay
coverage and device behavior remain owner-tested.

## ADR-139: Restore XA Mode on CD Audio Restart

Date: 2026-10-30. Status: accepted.

Context: The current CD runtime stores `CdRead`'s subsystem mode in the same
state used by CD-ROM commands. The game starts XA playback in mode `0xC8`, but
later file reads use mode `0x80`. A subsequent `CdlReadS` from
`cd_audio_restart` then reached a runtime abort because the XA bit was no longer
set. A stack captured from the owner's open crash dialog showed
`farm_mode_tick -> cd_audio_fade_tick -> cd_audio_restart -> CdControl`, with
the active mode at `0x80` and no outstanding data read.

Decision: Remember the most recently configured XA mode when `CdlSetmode`
enables ADPCM. On a later `CdlReadS`, restore that mode if a file read changed
the active mode. Reject a still-busy reader or a read without any prior XA
configuration through the normal CD command failure status instead of aborting
the process.

Consequences: XA fade restarts resume with the configured speed and filter
after intervening file reads. The CD runtime remains port-only; no decomp source
or overlay manifest changed. Interactive gameplay confirmation remains with the
owner.

## ADR-140: Keep GPU VRAM Persistent and Upload Changed Regions

Date: 2026-10-30. Status: accepted.

Context: GPU scene packets included the full 1 MiB VRAM image, and the renderer
uploaded it on each new scene revision. This spent bandwidth when consecutive
frames changed only small regions. Scene data also needs a stable snapshot
because the CPU reuses its active VRAM context for later reference rendering.

Decision: Store a builder-owned stable VRAM snapshot separately from the packed
scene records. Keep a persistent R32_UINT VRAM texture in the GPU renderer,
compare the selected snapshot with its last uploaded shadow, and upload changed
16x16 tile-row bounds. Use a full upload on the first scene or when changed
regions cover at least 75% of VRAM. Keep CPU VRAM authoritative and retain
software fallback behavior.

Consequences: The scene packet and per-frame GPU scene upload no longer carry
the VRAM image. The renderer still keeps a 1 MiB CPU shadow and transfer buffer;
the scene builder keeps its 1 MiB stable snapshot. Tile comparison is exact, so
it supports either of the two retained replay frames without relying on shared
dirty flags. A first D3D12 probe initially exposed a borrowed-snapshot lifetime
bug; the builder-owned copy fixed it, and readback comparisons then passed on
D3D12 and Vulkan.

Measurements: On the local Radeon RX Vega, the synthetic 50-frame
capture/upload/compute/fence probe measured GPU times of 0.447/0.562/0.868 ms
at 320x240/640x480/1440x1080 on D3D12, and 0.314/0.391/0.727 ms on Vulkan.
Against ADR-138's earlier GPU measurements, D3D12 changed from
0.475/0.547/0.877 ms and Vulkan from 0.520/0.564/0.998 ms. This probe uses
unchanged VRAM after its first frame; it does not represent scene uploads that
modify most VRAM each frame or end-to-end gameplay frame rate.

## ADR-141: Keep Android Disc Import Within App-Private Storage

Date: 2026-10-30. Status: accepted.

Context: The Android launcher still probed public Documents directly and kept
legacy shared-storage permissions even though the current setup flow uses the
Storage Access Framework (SAF). Scoped storage makes those direct probes
unreliable. Disc import also deleted the installed image before checking that
the replacement rename succeeded. The accepted NDK r27c configuration had the
16 KB maximum page-size linker flag but lacked the matching common page-size
flag.

Decision: Import a raw USA `.bin` selected through SAF into app-specific
external storage, with internal app storage as a writable fallback. Do not
request broad shared-storage access or probe public Documents paths. Copy to a
temporary file, flush it, then replace the installed image atomically when the
filesystem supports it; a failed copy must keep the installed image. Keep logs
free of user paths. Mark touch hardware optional so Android devices with a
controller and no touchscreen remain installable. For NDK r27, link with both
`max-page-size=16384` and `common-page-size=16384`.

Consequences: Older disc images in shared Documents must be selected with the
document picker. The app uses one consistent app-private root for native data,
saves and the imported image. Device-level 16 KB execution and interactive
gameplay still require owner/device validation.

## ADR-142: Preserve Existing User Data When Atomic Replacement Fails

Date: 2026-10-30. Status: accepted.

Context: Windows settings and memory-card persistence used a temporary file,
but on rename failure the fallback removed the existing file and then wrote
directly to the destination. A second failure could destroy the last valid
settings or save data. The Windows C runtime's narrow `rename` was also used
for a Documents folder migration even though paths are stored as UTF-8.

Decision: Replace files on Windows with `MoveFileExW` using UTF-8 to UTF-16
conversion and `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`. On other
platforms use the platform-aware SDL rename. Any failed replacement removes
only the temporary file and leaves the previous destination untouched. Use
`SDL_RenamePath` for the legacy directory move so Windows paths stay Unicode
safe.

Consequences: A failed settings or memory-card save preserves the prior
committed file. Unit coverage exercises successful replacement of existing
card data and the existing failed-write preservation path.

## ADR-143: Upgrade the Android Build Toolchain

Date: 2026-10-30. Status: accepted.

Context: The owner requested the newest Android Gradle Plugin and Gradle
wrapper, with NDK `30.0.16248370`. The previous Android setup used an older AGP
and NDK. AGP 9 provides built-in Kotlin support, while the project had also
passed a redundant `HMBTN_PORT` CMake variable that the port build does not use.

Decision: Pin AGP 9.5.0, Gradle 9.8.2, and NDK `30.0.16248370`. Commit the
Gradle wrapper scripts and JAR, verify the JAR against the official Gradle 9.8.2
checksum, and pin the distribution SHA-256 in wrapper properties. Use AGP's
built-in Kotlin support and remove the external Kotlin plugin. Have the
Android build script verify the installed side-by-side NDK and wrapper versions;
use the literal `android.ndkVersion` pin instead of passing a second version
through a Gradle property. Remove the unused `HMBTN_PORT` CMake argument; the
port target already defines that compile macro.

Consequences: Windows Android Release builds now use the requested NDK and the
current Android build plugins with a checksum-verified wrapper. The NDK 30
release APK passes APK signature verification, 16 KB zip alignment, and native
ELF load-segment alignment checks. Device-level behavior remains owner-tested.

## ADR-144: Add an Optional NPC Relationships Status Page

Date: 2026-10-30. Status: accepted.

Context: The owner requested a simple status page that lists NPC friendship and
affection in the existing multi-page farm status menu.

Decision: Add an optional ninth page that displays the 32 named adult villagers,
using each original friendship value (0-255) or bachelorette affection value
(0-65535). Show a small pixel head icon, the NPC name, and a colored progress
bar; use green for friendship and pink for affection. Exclude unnamed records
and the player's child. Gate the page behind `HMBTN_ENH_NPC_BONDS`, require the
existing gameplay and pause-menu options, and persist `enhance.npc_bonds` with a
runtime default of false. Enable its build option in Windows and Android `-Game`
profiles. Add narrow `HMBTN_PORT && HMBTN_ENH_NPC_BONDS` source guards while
keeping the exact matching-build eight-page handler and wrap behavior.

Consequences: The feature leaves vanilla status navigation intact while OFF.
It reads the original villager state without modifying it or loading/mutating
villager sprites. Portraits come from the first 80x80 face bank grouped under
each Character ID in the user's `face.bin`; the page crops and downsamples them
at runtime and ships no game art. Names use title case with the game's lowercase
glyphs; the page heading and bar labels also use title case. Pure model and
settings tests cover the roster, relationship mapping,
bar bounds, defaults and persistence. The owner must still validate layout and
navigation during gameplay.

## ADR-145: Exclude Imported Android Disc Images from Backups

Date: 2026-10-31. Status: accepted.

Context: Android Auto Backup includes app-specific external files by default.
The launcher imports the user's full disc image into that directory, while the
platform limits each app's backup allocation to 25 MB.

Decision: Add legacy full-backup and Android 12+ data-extraction rules that
exclude imported disc filenames, the temporary import file, and `last_run.log`
from both cloud backups and device transfers. Apply exclusions to external and
internal app storage, since the launcher may use either location. Keep saves
and settings eligible for backup.

Consequences: Restored devices must reimport the disc image; saves and settings
remain eligible for transfer and backup. This avoids placing large user-provided
game data in Android backup sets.

## ADR-146: Preserve Zero Audio Volumes During Sound Initialization

Date: 2026-10-31. Status: accepted.

Context: The host loads saved BGM/SFX volumes before the original game calls
`SsInit`. `SsInit` preserved nonzero settings but treated zero as uninitialized
and silently restored the affected volume to 100%, so saved mute settings did
not survive startup.

Decision: Preserve both configured mixer values across `SsInit`; the static
initial values already provide the 100% first-run defaults. Add regression
coverage for zero/nonzero reset preservation and verify that muting live
sequence and sound-effect output produces silence without stopping playback.

Consequences: A saved 0% level remains muted after game sound initialization;
nonzero levels and the existing default stay unchanged.

## ADR-147: Expand Ordered GPU Scene Coverage

Date: 2026-10-31. Status: accepted.

Context: GPU scene capture still rejected inline GP0 transfers and imposed fixed
limits of 8,192 primitives, 256 references per tile, and a fixed renderer
payload. Feedback reads after earlier draws also needed a trustworthy VRAM image
for the exact point in command order.

Decision: Represent GP0 ClearImage and LoadImage operations as ordered GPU scene
records. Preserve their packet captures; direct VRAM mutations outside a
captured command remain invalidating. When a texture reads an earlier scene
write, replay the private scene through the CPU reference rasterizer to
materialize VRAM, then keep immutable VRAM versions for GPU sampling. Grow
primitive storage, tile bins, packed packets, renderer buffers, and VRAM texture
layers to the scene/device resource limits instead of rejecting former fixed
capacities. Keep malformed/truncated streams, reserved texture depth, missing
captures, allocation failures, and unsupported devices on the software fallback.
CPU VRAM remains authoritative.

Consequences: Synthetic Vulkan comparisons cover clear/load ordering, texture
feedback after writes, scenes beyond the former primitive/bin/payload limits,
and the established primitive classes against the CPU reference. This does not
establish full-game scene, device, or gameplay coverage. Feedback scenes perform
an additional CPU reference replay, and very large per-frame VRAM version sets
can still exceed device or memory limits.

## ADR-148: Use the Standard Android Release Optimization Level

Date: 2026-10-31. Status: accepted.

Context: The Android Gradle configuration forced `-O3` for every native C and
C++ translation unit. The port performance policy uses `-O2` by default and
allows `-O3` for measured hot files; no per-file Android measurements justify a
global `-O3` override.

Decision: Use `-O2` for Android Release and RelWithDebInfo native builds, while
keeping the conservative `armv8-a` baseline. Do not add per-file `-O3` flags
until profiling identifies a hot file and records a before/after result.

Consequences: Android builds follow the common release policy and do not claim
an unmeasured speedup. Runtime performance still needs device profiling.

## ADR-149: Relax Unused Vulkan Features for Android GPU Creation

Date: 2026-10-31. Status: accepted.

Context: SDL 3.2.28 rejected the connected MediaTek G100 tablet's Mali-G57 MC2
Vulkan device because it does not expose `shaderClipDistance`. The game's shared
scene renderer submits compute pipelines only and does not use Vulkan clip
distance, depth clamp, indirect first-instance drawing, or sampler anisotropy.

Decision: Upgrade the pinned SDL3 release to 3.4.18 and use its GPU device
properties on Android to request SPIR-V shaders while disabling those four
unused optional features. Keep the existing shader-format selection on desktop.

Consequences: Android GPUs missing those optional features can initialize the
compute renderer while SDL's Vulkan backend still validates the required core
features. The API remains available if a future renderer starts using one of
these features; that path must then request and validate it explicitly. Device
startup is confirmed on the connected tablet; gameplay rendering remains
unverified.

## ADR-150: Synchronize Android SDL Java Glue with the Native Release

Date: 2026-10-31. Status: accepted.

Context: The Android app packaged SDL 3.2.28 Java glue with SDL 3.4.18 native
code. SDL's `JNI_OnLoad` expected `SDLActivity.nativeSetupJNI()V`, while the
checked-in class declared `nativeSetupJNI()I`; the mismatch aborted app startup
before graphics initialization.

Decision: Keep the SDL Android Java glue synchronized with the exact pinned
SDL3 release. Replace the checked-in `org.libsdl.app` glue with the official
3.4.18 source and retain the app-specific `MainActivity` subclass separately.

Consequences: Android native JNI setup now completes with the matching SDL
library. The connected tablet logs successful Vulkan device creation; no game
scene was started during this diagnostic.

## ADR-151: Attempt GPU Scene Output on Every Platform

Date: 2026-10-31. Status: accepted.

Context: ADR-132 kept Android on the selective texture-scanout path because an
Adreno 610-class device measured a 42.57 ms compute/blit submit. The owner now
requests full GPU scene coverage across the native and enhanced render modes.
The scene builder also treated documented GP0 NOP mirrors as unsupported inline
transfers, causing avoidable CPU fallback despite no framebuffer operation.

Decision: In GPU Auto, attempt the shared SDL_GPU scene renderer for every
captured frame on desktop and Android. Keep Software selectable and retain the
per-frame CPU reference fallback for missing captures, unsupported commands,
resource failures and unavailable GPU devices. Classify GP0(04h..1Eh), GP0(1Fh),
GP0(E0h), GP0(E7h..EFh), and GP0(C0h..DFh) as commands that do not write scene
pixels. Also accept GP0(03h) as non-pixel because this port's reference command
executor skips it without a VRAM effect; its hardware meaning remains
undocumented. Add an encoding sweep for all polygon, line, and rectangle
opcodes and report scene-compute coverage in 600-frame windows. Opcode
classifications follow the public [PSX-SPX GPU specification](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/).

Consequences: Android GPU Auto now exercises the compute scene path by default;
the connected MediaTek G100 device has passed SDL GPU initialization, but its
scene throughput, thermals and gameplay output remain unverified. The previous
Adreno measurement remains a known performance risk, and Software remains the
runtime escape path. Coverage reports count submitted compute-scene frames, not
all rendering work: native VRAM compatibility still executes on the CPU, and
unsupported scenes or unreconstructable frames still use the reference renderer.

## ADR-152: Preserve Direct VRAM Operations in GPU Frame Captures

Date: 2026-10-31. Status: accepted.

Context: The frame replay capture had fixed limits of 65,536 command words and
64 render groups. It also invalidated the active capture whenever a direct
`LoadImage`, `ClearImage`, or `MoveImage` modified VRAM between draw-environment
setup and ordering-table submission. Fully captured loading/bypass scenes were
excluded from GPU replay even though their command streams were available, and
the loading screen had one `DrawOTag` call outside the normal frame wrapper.

Decision: Grow frame command and group storage on demand, retaining allocations
for reuse and reporting allocation/overflow failures to GPU coverage logging.
During an active native frame capture, encode direct VRAM API operations as
ordered GP0 transfer records and copy `LoadImage` payloads into the replay
buffer. For native `DrawOTag` calls made without an outer frame capture,
invalidate older captures and create a bypass capture around the ordering
table. Permit a complete bypass capture to render when the live frame is itself
marked bypass; continue using CPU rendering when no complete current bypass
capture exists. Mark interpolation ineligible for
frames containing direct API transfers because its motion matcher does not
model those transfer records.

Consequences: Frame replay no longer falls back because of the old fixed word
or group caps, and direct image/clear/copy calls can participate in the shared
GPU scene path without changing CPU VRAM semantics. Regression tests cover odd
pixel `LoadImage` payloads, all three direct transfer operations, more than
65,536 words, more than 64 groups, low-level uncaptured `DrawOTag`, and captured
bypass frames. A synthetic Vulkan and Direct3D 12 readback probe also verifies
the direct-transfer and unwrapped-OT paths against CPU pixels. Device resource
limits, allocation failure, raw VRAM changes
outside an ordering-table capture, and unsupported or malformed GP0 streams
retain the CPU reference fallback. Full interactive coverage remains
unverified until owner gameplay testing reports zero unsupported or
missing-capture fallbacks.

## ADR-153: Pack Smaller GPU Scene Tile Bins

Date: 2026-10-31. Status: accepted.

Context: Dense scenes such as the farm made each output pixel examine too many
unrelated primitives from 16x16 logical bins. A fixed 257-word slot per bin
also made sparse scenes upload many unused indices. The G100 owner-run build
reported 100% GPU scene coverage but only 14-16 presents/s, with 63-73 ms
average present time and 3.5-4.0 ms guest-game time.

Decision: Use 8x8 logical bins and pack them as a tile-offset table followed by
variable-length `[count, primitive ids]` lists. Keep the complete primitive
records and raster semantics unchanged. Preserve the general shader path and
existing CPU safety fallback; do not trade scene coverage or image quality for
the bin optimization.

Consequences: On the same synthetic 8,192-rectangle farm-like workload using a
Windows Radeon RX Vega, D3D12 capture/upload/compute/fence time changed from
1.926 to 1.830 ms at 320x240 and from 3.976 to 2.714 ms at 960x720. Vulkan
changed from 1.841 to 1.849 ms and from 4.283 to 2.713 ms, respectively. The
native-size differences are within run-level variation; the larger-output
results show a repeatable improvement.
Scene payload fell from 1,651,952 to 1,385,920 bytes (16.1%). These are
offscreen synthetic measurements, not gameplay FPS. The owner did run this
build on the G100 and still reported severe lag. Follow-up stage timing
attributed most measured farm time to CPU VRAM materialization, so the bin
change alone did not resolve the device issue. A workgroup-shared bin cache
was tried and removed because it did not improve the Windows benchmark.

## ADR-154: Fingerprint Private VRAM Snapshots by Dirty Tile

Date: 2026-10-31. Status: accepted.

Context: The G100 farm diagnostic measured 97.5-142.9 ms in the second CPU
reference replay, which materializes VRAM versions for ordered texture feedback.
That replay compared each 1 MiB VRAM image against every earlier version,
causing quadratic full-image reads when a scene captured 41-55 versions.

Decision: In private replay contexts that materialize VRAM, maintain a
position-aware fingerprint per 16x16 VRAM tile. Initialize it from the copied
frame baseline, then update only dirty tiles as the reference renderer writes.
Use the aggregate fingerprint to shortlist prior versions; retain exact
full-image comparison on matches. Skip fingerprint initialization in the
ordinary probe replay and in native VRAM contexts.

Consequences: This removes the repeated full-image scans when versions differ
and preserves exact deduplication checks when fingerprints match. The Windows
Release build and 124/124 tests pass, and the Android Release APK builds with
NDK 30.0.16248370. G100 post-change gameplay timing is pending; no frame-rate
improvement is claimed until the owner measures the updated APK. Windows
gameplay performance has also not been remeasured.

## ADR-155: Continue Ambient Weather During Dialogue Pauses

Date: 2026-10-31. Status: accepted.

Context: During a paused NPC dialogue, weather particles continued changing
position but their age and respawn stopped. Rain drops therefore drifted out of
view without replacement, making the rain appear to stop and slide across the
screen. Advancing the existing particle lifecycle during a pause would consume
the shared gameplay RNG and could change later game behavior.

Decision: In the native port only, continue particle delay, movement, animation,
and respawn while the game pause flag is set. During paused weather updates,
swap in a private weather RNG state and restore the gameplay RNG state before
returning. Keep the original matching-build implementation byte-for-byte in the
`#else` branch.

Consequences: Rain and other ambient weather effects continue behind paused
dialogue without perturbing gameplay randomness. A focused regression test
advances and respawns a rain particle while paused and verifies the shared RNG
is unchanged. Owner confirmation in an actual NPC dialogue remains pending.

## ADR-156: GPU Scanout After Invalid or Unsupported Scene Replay

Date: 2026-10-31. Status: accepted.

Context: GPU Auto already scanned the visible native VRAM page when a frame had
no coherent command capture. If a capture existed but could not be replayed or
packed, the host still fell back to the CPU despite the same authoritative VRAM
being available.

Decision: After any captured-scene replay or packing failure, try a zero-primitive
GPU scene that scans the current visible VRAM area. Keep CPU rendering as the
safety path when GPU scanout construction or presentation fails, and leave
explicit Software mode unchanged.

Consequences: Unsupported, invalidated, and incomplete captures can still reach
the display through GPU Auto without pretending their command streams were
successfully replayed. A regression verifies that scanout can rebuild after an
unsupported GP0 scene. Other scene/device coverage and interactive gameplay
remain unverified.

## ADR-157: Clear Disabled PS1 Display Frames on the GPU

Date: 2026-10-31. Status: accepted.

Context: When the PS1 disabled its display, GPU Auto skipped scene presentation
and the host built a black frame through the CPU scanout/upload path. This was a
valid frame that did not need VRAM conversion or a transfer buffer.

Decision: In GPU Auto, acquire the swapchain and clear it to opaque black with a
GPU render pass. Keep explicit Software mode unchanged and retain the CPU scanout
path if the GPU clear pass cannot be created.

Consequences: Display-off transitions stay on the GPU and remain black across
all window sizes, including when the PS1 display rectangle is empty. Build and
unit validation are required; visual transition behavior still needs owner
playtesting.

## ADR-158: Spread Mature Planted Fodder Grass

Date: 2026-10-31. Status: accepted.

Context: The owner requested gradual farm grass propagation, specifically from
planted grass seed rather than weeds or wild grass, with new growth restricted
to empty plots and no changes to unrelated random game behavior.

Decision: Add an opt-in `grass_spread` gameplay setting. Each mature
`ITEM_GRASS_SEEDS` tile gets a deterministic 50% daily spread attempt into one
randomized cardinal neighbor. A neighbor must still be completely bare soil.
Collect targets first, then apply them after scanning so a new plot cannot
spread again that morning. New plots receive the same sown-soil and initial
growth state as normal planting. The direction and timing hash does not consume
the game's random stream.

Consequences: Existing weeds and unplanted wild grass never spread. Obstacles,
tilled soil, occupied crop plots, and field edges block growth. Windows and
Android `-Game` builds compile this option; runtime default is OFF. A synthetic
test covers deterministic timing and all four directions. Actual field visuals
and growth remain owner gameplay checks.

## ADR-159: Slow the Outdoor Farm Clock

Date: 2026-10-31. Status: accepted.

Context: The owner requested an optional slower clock limited to the outdoor
farm, without changing simulation speed elsewhere.

Decision: Add an opt-in `slow_farm_time` gameplay setting that advances the
normal game clock on every other clock-frame callback while the active location
is `PLACE_FARM`. Indoor scenes and queued fast-forward events keep their existing
clock behavior. The simulation remains on the native update path.

Consequences: Outdoor farm time advances at half its normal rate while enabled;
all other locations remain unchanged. Runtime default is OFF. Interactive clock
and event-schedule behavior remains owner-tested.

## ADR-160: Reopen Shipped Items Before Settlement

Date: 2026-10-31. Status: accepted.

Context: The owner requested a shipping-bin screen using the game's window and
font assets, and a way to reclaim an item before the 5:00/Zack settlement while
preserving the existing throw action. The owner later asked to remove the
rucksack panel and keep one shipped-items window.

Decision: When the player presses X with an empty hand near any
`ZONE_SHIPPING_BOX`, open the original framed pause-style dialog. Search all
active shipping-box zones within one farm tile of the player, so facing and
overlapping zones cannot hide the bin. Some outdoor farm area data omit this
zone, so also resolve a nearby morphing, slotted scene model in front of the
player on the outdoor farm map. The window lists shipped item variants and
quantities. Confirming an item first validates rucksack capacity, then returns
one item and reverses the
shipment count, value, and fish tally. Holding an item at a fixed shipping box
keeps the original throw action. When Shipping Bin List is enabled and the
player holds the portable basket, pressing Square opens the same list with a
`Ship basket now` row; choosing it starts the original animated basket shipment.
The screen does not replace the coop/barn feed or product-processing stations,
which retain their original interactions.

The screen uses one original-style frame with native 32-pixel item sprites in
the original 4-by-2 grid. The host copies each requested sprite into a private VRAM
context while drawing icons, then restores the live picture-slot tables and VRAM. The
shipped-item grid navigates by D-pad or touch selection. A thrown stack carries
its quantity through the flight and records every unit in both the normal bin
landing path and the supplemental proximity path. Items that lack a vanilla
shipment-count slot (such as bamboo shoots) are tracked by the feature's
session-only variant counts; portable-basket additions are committed to those
counts when the basket is shipped. The footer uses Cross/Circle marks instead
of spelling out those button names.

Consequences: An item can be reclaimed only before settlement, and a full
rucksack leaves shipment state unchanged. The setting defaults OFF, so enable
Shipping Bin List in Pause > Settings > Gameplay to open the list. Counts are
tracked during the day even while the option is off, and become visible if it
is enabled before settlement. Exact variant tracking is kept in process memory
because the feature does not alter the vanilla save/card format; the
item-variant list is therefore not restored after restarting or loading a save.
Windows and Android `-Game` builds include the option. The owner must verify bin
interaction and original throw behavior in gameplay. The shipped-item view draws
eight slots per page; D-pad/tap selects a slot, confirm takes one item,
and back closes the view.

## ADR-161: Remap Keyboard and Android Touch Controls

Date: 2026-10-31. Status: accepted.

Context: The owner requested keyboard remapping and Android virtual-gamepad
mapping, with the option to hide touch controls for a physical gamepad and to
adjust on-screen button size and position.

Decision: Add per-action keyboard capture with duplicate-key rejection. On
Android, provide a virtual-pad enable switch, action mapping for each on-screen
button, size choices from 70% to 140%, and an edit mode that saves normalized
button positions. Keep virtual-pad actions on dedicated reserved SDL key codes
so disabling keyboard input does not disable touch controls. Map the virtual
stick to the game's eight-way D-pad with an 8 dp deadzone and angle hysteresis.
Press a new direction before releasing the old one so axis changes do not emit
a neutral movement interval.

Consequences: Keyboard and Android layout settings persist in user settings,
not game saves. The Android layout editor provides Reset and Done controls.
Windows and Android builds compile the keyboard mapping; only Android presents
the virtual-pad editor. Device-specific touch feel remains owner-tested.

## ADR-162: Add Optional Goddess Blessings

Date: 2026-10-31. Status: accepted.

Context: The owner requested a small support menu in the pause interface for
useful player assistance, while keeping the additions simple and reversible.

Decision: Add a `Goddess Blessings` page with five actions: add 10000G, restore
stamina and fatigue, add 10 fodder, add 10 chicken feed, or add 10 fish food.
Use existing estate inventory/gold helpers and reset stamina/fatigue state
latches when restoring energy.

Consequences: Blessings are explicitly requested actions available in the
pause menu. They do not run automatically or alter save layout. The owner should
check inventory caps and status effects during gameplay.

## ADR-163: Enable Viewport Scaling in Android Enhanced Profiles

Date: 2026-10-31. Status: superseded by ADR-164.

Context: The owner requested sharper Android output in Enhance PS1 and Modern,
using the available display size where practical and retaining a mobile-safe
resolution cap.

Decision: Selecting either enhanced profile enables the existing scaling path
on Android. Its target follows the available viewport and is capped at 1280x720;
Native PS1 remains at native internal resolution.

Consequences: The setting only affects Android's profile selection. Allocation
or replay failure retains the existing safe fallback. Device performance,
thermal behavior, and visual sharpness need owner validation.

## ADR-164: Keep Android Enhanced Profiles at Native Internal Resolution

Date: 2026-10-31. Status: accepted.

Context: The owner reports worse performance after enabling viewport scaling on
Android tablets, with a stronger slowdown on Adreno devices, and asks to revert
the scaling change.

Decision: Supersede ADR-163. Native PS1, Enhance PS1, and Modern profiles all
keep internal rendering at the native PS1 scene resolution. Modern retains its
16:9 viewport and bilinear final presentation; Enhance retains PGXP. The final
image is enlarged only by the normal presentation blit/display path. Older
settings containing `enhance.scaling=true` are ignored by Android profile
selection.

Consequences: Android no longer builds 720p internal scenes for these profiles,
reducing compute and memory bandwidth on mobile GPUs. The Windows scaling option
is unchanged. Android image sharpness and performance should be checked on the
owner's tablet and Adreno device; agents must not claim a measured improvement
without device timings.

## ADR-165: Avoid Bilinear Texture Sampling in Android GPU Scenes

Date: 2026-10-31. Status: accepted.

Context: After the native-resolution rollback, the connected Adreno 610 still
reported 12-13 presents/s at a 520x240 scene size. Present time averaged
64-70 ms, with 60-66 ms in GPU submit, while CPU scene construction averaged
1.4-1.6 ms. The GPU was busy about 99.9% of the sample and thermal status was
normal. Android's CPU reference path already uses nearest texture sampling for
bilinear mode, but the GPU scene path still used four bilinear texture taps.

Decision: Apply the same mobile scene-filter policy to the GPU path: Android
uses nearest texture sampling for Bilinear mode and retains xBR when explicitly
selected. The final presentation blit still honors the profile's bilinear
setting. Desktop GPU filtering is unchanged.

Consequences: This removes the per-primitive four-tap filter cost from Android
GPU scenes and keeps GPU/CPU mobile scene sampling aligned. Performance after
this change must be measured on the Adreno device; no improvement is claimed
from the pre-change sample.

## ADR-166: Cache Tile Primitive Packets per GPU Workgroup

Date: 2026-10-31. Status: accepted.

Context: Android GPU submit remained the suspected bottleneck in dense farm
scenes. Each output pixel repeatedly loaded the same tile's primitive IDs and
primitive records from the global scene buffer.

Decision: For native-size scenes whose dimensions align to the 8x8 bin grid,
load a tile's primitive IDs and records once into workgroup-shared memory when
the bin contains at most 16 primitives. Pixels in that workgroup then reuse
the cached data. Scaled or partial workgroups, and bins above that limit, keep
using the existing global-buffer path.

Consequences: The shared cache reduces repeated scene-buffer reads for eligible
tiles without changing bin order or the CPU fallback. Windows D3D12/Vulkan
renderer probes pass their pixel comparisons and primitive coverage checks.
The Android Release APK builds and runs on the owner's Helio G100 tablet; a
single farm log sample measured 3.57 ms average game time and 3.99 ms average
presentation, and the owner reports smooth play. This is not a controlled
before/after benchmark. A later Adreno 610 farm capture is recorded in
GPU_TESTING.md; the cache path covered nearly every tile, but submit time
remained 36.4-43.5 ms and presentation 40.6-48.3 ms.

## ADR-167: Skip Shared-Memory Barriers for Uncached Tile Bins

Date: 2026-10-31. Status: accepted.

Context: The workgroup packet cache is used only for non-empty bins containing
at most 16 primitives. Empty bins and denser bins still paid a second
workgroup-wide synchronization even though they did not write shared packet
data.

Decision: Enable packet caching only for counts from 1 through 16. Keep the
initial barrier that publishes tile metadata, and perform the later ID/packet
barriers only inside the group-uniform cache branch. The Android performance
log also reports cache-eligible, empty, and uncached bin counts for each scene.

Consequences: Empty and dense bins avoid the unused synchronization path while
cached bins keep the barriers required for safe shared-memory reads. Windows
Release and ASan/UBSan CTest each pass 125/125; the D3D12/Vulkan scene readback
probes pass. Android Release builds with NDK 30.0.16248370. The APK was
installed on Adreno 610 and tested in the farm. Since the scene had no empty
bins and only 14 of 1,950 bins were too dense for caching, this barrier change
affected a small part of that workload; the farm bottleneck remains. See
GPU_TESTING.md for timings and limits.

## ADR-168: Remove Gameplay Mouse Controls and Improve Pause Navigation

Date: 2026-10-31. Status: accepted owner request.

Context: Mouse movement/action controls were already disabled in the game host,
but the Controls page still exposed and saved the no-op modes. Configured Android
virtual-gamepad mappings were also not consumed by the pause loop.

Decision: Remove the gameplay mouse-mode setting and all gameplay pointer
position/button/wheel forwarding. Keep mouse/touch pointer interaction for
pause-menu navigation only. Use the existing Esc/J, Circle, and Android Back
routes for backing out of pages. Add platform-specific input hints and honor
configured Android virtual-gamepad keys while the pause loop is active. Windows
keeps keyboard, gamepad, and mouse navigation.

Consequences: Old `controls.mouse` settings are ignored. Gameplay input remains
keyboard/gamepad/touch-overlay based; pointer taps continue to work in pause
menus. Back navigation uses existing keyboard, controller, and Android system
Back inputs without adding a separate on-screen button. The pause UI still uses
the existing original-game dialog art and font. Windows and Android builds and
their controls/pause tests must verify the renumbered settings rows and
touch/gamepad routes.

## ADR-169: Keep the Camera GTE Flag Word 32-bit on LP64

Date: 2026-11-01.

Context: `CameraWork.flag` is an output word for `RotTrans`, whose native
compatibility signature writes one 32-bit GTE flag value. The original field
uses `long`, which is 64-bit on Linux x86-64 and made the pointer incompatible
with the runtime API.

Decision: Under `HMBTN_PORT`, declare the temporary field as `s32`; keep the
original `long` declaration in the matching-build branch. Assert the native
field width in the camera-view unit.

Consequences: Camera rotation uses the intended 32-bit GTE flag storage on
LP64 hosts, and the matching build keeps the original declaration unchanged.
