# Native port reconnaissance

This is the source/overlay inventory baseline from 2026-10-25, not the current
feature-status page. The Windows game has since reached the title and farm; Android
arm64-v8a build support has also been added. Current Windows user data is under
Documents/VanillaBTN; older BackToNature references below are historical (ADR-137). See
PLATFORMS.md for current platform status and ENHANCEMENTS.md for current renderer
coverage.

At the time of this inventory, all 113 original C units compiled. A native
console probe executed the original boot prefix, but title rendering/gameplay
and the matching baseline had not yet been validated.

## Source and overlays

The tree contains 113 C files and 110 headers. `src/main/` contains the resident
program, with `cd`, `game`, `geometry`, `graphics`, `input`, `sound`, `text`, and
`ui` subdirectories. `src/include/` contains shared types, actor/game layouts,
GTE macros, CPU assembly helpers, and C-library declarations.

The root CMake project links the EXE and all overlays in one linker invocation.
See `cmake/psx.cmake`, `cmake/program.ld.in`, and each overlay's CMakeLists.txt.

| Overlay | Link region |
| --- | --- |
| 08_DEBUG | low |
| 10_PdaSamp | low |
| 11_GF_FARM | low |
| 15_saveload | high |
| 17_GF_MG1 | low |
| 19_GF_MG2 | low |
| 21_GF_MG3 | low |
| 23_GF_MG4 | low |
| 25_GF_MG5 | low |
| 27_GF_TITLE | low |
| 28_GF_STAFF | low |
| 30_GF_swind | high |
| 31_gf_mcard | low |

Low starts at `0x800B7E20`; high starts at `0x8012DDE8`. These are matching
link addresses, not host allocations. `src/main/overlay_regions.c` provides
the destination table. `main.c`'s overlay read macro delegates to
`afile_read_range`; the farm can coexist with a high-region overlay. Replacing
only the disc read is insufficient: dispatch, copied tables, and address
arithmetic must also be audited before static host linking.

All 113 native objects now build with Clang 20.1.8. An LLVM nm inventory finds
no duplicate strong external source definitions, excluding compiler-generated
COFF .refptr entries. The reachable resident/overlay graph links into the native
boot diagnostic. Existing source namespaces retain resident imports; no blanket
symbol prefixing is applied. Native LOAD_OVERLAY selects a member/region registry
instead of reading executable PS1 bytes. First activation uses C initializers;
reentry fails explicitly until native global/BSS restoration is implemented.

## SDK surface

`sdk/objects.txt` selects objects from LIBAPI, LIBC, LIBCARD, LIBCD, LIBETC,
LIBGPU, LIBGS, LIBGTE, LIBMATH, LIBMCRD, LIBMCX, LIBPAD, LIBSN, LIBSND and
LIBSPU, plus 2MBYTE.OBJ. This is a link-input inventory, not proof every
function in these libraries is called by the game.

Observed calls include:

| Area | Examples and source evidence |
| --- | --- |
| CD | `CdSearchFile`, `CdControl`, `CdRead`, `CdReadSync`, `CdIntToPos` in `main/cd/afile.c` |
| Timing/GPU | `ResetCallback`, `DrawSync`, `VSync` in `main/cd/afile.c` |
| Audio | `SpuSetCommonAttr`, `SpuGetAllKeysStatus`, `SsSeqOpen`, `SsSeqPlay`, `SsSeqStop`, `SsSeqPause`, `SsSeqReplay`, `SsSeqSetVol`, `SsSeqClose` in `main/sound/sound.c` |
| GTE | `gte_rtps`, `gte_rtpt`, `gte_nclip`, `gte_avsz3`, `gte_avsz4`, `gte_sqr0`, `gte_op12` definitions in `include/gte.h` |

A lexical call/macro inventory is now available; an undefined-symbol comparison
of the complete program is still required. No SDK header contents have been
copied into the port.

## Audio and video streams

XA streaming is confirmed: `main/sound/cd_audio.c` searches `STR/01.XA` through
`STR/04.XA`, selects file/channel, and drives playback with `CdlReadS`.
`sound.c` also opens SEQ data and VAB-related sound resources. A source search for
`DecDCT`, `StGet`, `StSet`, `CdPlay`, and `CdlPlay` found no occurrences.
This does not prove the absence of all FMV or CDDA paths; data inspection and
a full call inventory remain pending.

## Assembly, addresses, and 64-bit hazards

- `include/cpu_asm.h`: MIPS scratchpad stack switching, return-address stores,
  and `break 1024`. Scratchpad is `0x1F800000`, size `0x400`.
- `include/gte.h`: raw coprocessor words via `__asm__`, plus `inline_c.h`.
  These local headers redefine macros, so an SDK shim alone cannot replace them.
- `main/main.c:522`: overlay pointer cast to `s32` before address arithmetic.
- `main/graphics/anim.c:40`: pointer cast to `u32` for alignment checking.
- `main/sound/sound.c:429`: pointer cast to `s32` to compare a sentinel.
- `main/sound/sound.c:430`: writes a sequence handle through an aliased `s32 *`.
- `u_long` in SDK-facing reads must remain a 32-bit PS1 word on LP64 hosts.

These are examples, not an exhaustive hazard list. Native compilation and
sanitizer coverage of game code have not started. Suspicious original behavior
must remain unchanged until its meaning is established.

## Asset layouts

- `main/cd/afile.c`: 33 archive records, including 13 overlays and a sentinel;
  sector-rounded reads and offset tables. `tools/afile.py` describes archive IO.
- `main/graphics/anim.h`: ANI section offsets, image rectangles, part lists,
  ranges and frames. `AnimFile` has five 32-bit offsets after its header.
- `main/graphics/texture_pack.h`: type `0x0D` image banks; 12-byte image header,
  24-bit byte size plus flags, and variable-length word-padded image payloads.
- Save/card layouts, other asset structures, and embedded pointer relocation
  require a separate inventory before runtime loading.

Initial pointer-free asset layout checks now live in `port/tests/layouts.c`.
Original asset declarations remain unmodified; the native main.h pointer getter
guard is listed below.

## Repeatable lexical audit

`port/analysis/source_audit.py` scans source and headers without any SDK input.
It writes deterministic JSON with source/line evidence for includes, ordinary
function definitions, external call candidates, GTE invocations, and overlay
regions. The initial run reports:

| Item | Count |
| --- | --- |
| C sources / headers | 113 / 110 |
| Ordinary function definitions, including static functions | 2628 |
| Unique ordinary external function names | 950 |
| Duplicate ordinary external function name candidates | 1 |
| External call-like names requiring classification | 301 |
| GTE-prefixed invocation names | 40 |

This is not a preprocessor or a C semantic analysis. Macro-generated functions
(including farm courtship helpers), callbacks, type casts, declarations, global
data definitions and conditional compilation require compiler/linker analysis.
The 301 candidates are not 301 confirmed SDK functions; the 40 GTE-prefixed
names also include game helpers. Six regression fixtures cover comments,
strings, macro continuations, initializer braces, prototypes, operators and
source-line preservation, collision filtering and overlay ownership. Generated
reports stay in ignored `build/`.
The single duplicate-name candidate is main_get_region_base's mutually exclusive
native/matching definitions in one file. This illustrates the audit's lack of
preprocessing; it is not an actual linked collision.

## Initial native source compilation

`hmbtn_game_initial` initially compiled these eleven original units, and now
compiles all 113 units after the additions documented below:
`main/geometry/math_util.c`, `main/game/farm_data_size.c`,
`main/game/horse_names.c`, `main/game/picture_codes.c`,
`main/game/event_flags.c`, `main/game/farm_tables.c`, `main/game/farm_state.c`,
`main/graphics/frame_buffers.c`, `main/graphics/prim.c`, `main/main.c`, and `main/cd/afile.c`.
The port force-includes `compat/game.h`: fixed-width primitive types and scoped
SDK integer aliases. Minimal VECTOR/SVECTOR/CVECTOR/MATRIX and GPU packet
declarations follow source field usage and public GP0 formats (ADR-006).
GPU packet sizes/offsets and a known textured-quad byte sequence are checked;
GS ordering-table descriptors retain native pointers. GPU link handles now map
to full native pointers through a bounded registry (ADR-009). Native LINK_PRIM
and ot_insert_prim share this codec with SDK addPrim/AddPrim; packet layouts
stay fixed-width. DrawOTag and the source-used primitive rasterizers now have
CPU reference implementations; hardware rasterization parity remains pending.

The high-level trig/vector calls now use deterministic integer reference
implementations (ADR-033), including terrain cross products and camera matrix
application. Exact SDK lookup quantization remains unverified; remaining
unimplemented calls still abort by name. Tests cover type/endian and layout assumptions, original integer power,
32-bit wrapping, interpolation/truncation/clamping, vector operations and each
stub's failure diagnostic. Static helper paths are not exercised by game.math.
`game.state` exercises all 1184 valid event flags, word/sign-bit boundaries,
native table registration across 19 estate runs, the skipRuns path, entry-state
reset and shared-work busy/free behavior. The test defines a host FarmState and
three explicitly test-only abort traps for item-action callback link dependencies.
These callbacks are never invoked in the test and are not runtime implementations.
Their original type-mismatched casts remain tracked below. Save serialization
and pointer-bearing state layout parity have not been tested.

`compat.layouts` checks A_FILE records, ANI headers/images/parts/ranges/frames,
image bank headers, input replay records and estate rows. ANI frames are 10 bytes
in the original declaration, with control at offset 7. No source edit was needed.
No main loop, title screen, or renderer is linked into an executable yet.

An earlier `llvm-nm --defined-only --extern-only` pass confirmed six math functions
and four data symbols in the initial four-unit subset. A new pass confirms
`hm_game_main` and `afile_init` in the archive. main.c is renamed only through
its port build definition; matching branches are preserved. These objects
are not selected by the current test executables. The game's hardcoded
WORK_REGION and frame table addresses now select a native data workspace, and
the resident region helpers retain full pointers (ADR-008). Main-loop runtime
dependencies, full GPU execution and overlay dispatch remain unsafe/unimplemented;
the complete game entry point must not be executed yet (ADR-007).

The new source-facing libcd/libapi/libmcrd/libmcx declarations and the narrow
GPU/GS/GTE startup declarations permit native entry-point code generation.
Declarations alone do not implement callbacks, rendering, cards or GS work memory.
The game force-include now supplies string.h as well: the host's memory.h alone
does not declare memmove, unlike the original compilation environment.

Warnings retained in `math_util.c`: unused static functions `vec3_add`,
`vec3_differs`, `vec3_move_toward`, `math_cross2_sign`, and
`vec_dot_normalized`. These are tracked, not suppressed or removed.
`farm_tables.c` adds 22 missing-braces warnings for the existing crop table and
two incompatible function-pointer casts at lines 733 and 735. The latter adapt
`item_use_coin` and `farm_adjust_stamina_fatigue` to ItemUseFn; calling these through
that type needs a deliberate port solution before item actions are enabled.
The four pointer-to-s32 warnings in main.c are removed by native-only guards;
its four unused-function warnings remain.
afile.c adds 33 missing-pos initializer warnings for the existing A_FILE member
table, two unused functions and two unused parameters. No warnings were suppressed.
The truncated primitive-address warning in frame_buffers.c is removed by its
native link guard. Its unused debug hook parameter warning remains. The new
gpu.links test exercises original ot_insert_prim with native pointers.

## Native workspace validation

The aligned 0xCF1E0-byte C workspace holds the 0x49000 image window, 0x47390
section/free capacity and 0x3EE50 frame work region. It is not a PS1 RAM image;
code, other globals, overlays and heap allocations remain outside it. Its offsets
are derived from the matching layout, not an increased memory budget (ADR-008).

`game.work_memory` includes original main.c instead of copying its functions,
makes the unused game entry local, and calls the actual normal/wide frame setup,
frame begin, GS work allocator/release and region accessors. It also links and
calls original farm_state_init and frame_buffers_layout. Test-only GPU/GS functions
record which OT regions are cleared and which work base is selected; they do not
encode OT links or render. Unexpected host-debugger/system-error paths abort.

Both layouts keep OT, primitive and GS work spans inside the native frame object.
The demo buffer is correctly aligned below region_base, and its recording survives
a frame-area reset. Original table registration and farm buffer pointers bind
without subtracting addresses of separately linked overlay objects. Free-space
checks cover zero use, a partial section, exact exhaustion and a negative budget
one byte past capacity, preserving the signed vanilla result rather than clamping.
Release and ASan/UBSan pass. Full scene loads, raw asset pointer relocation and
overlay region copies remain outside this coverage. GPU links are covered by
the separate test below.

## Native ordering-table links

`psx/gpu_links.c` stores full pointers under nonzero 24-bit handles and preserves
the top packet-length byte. 0xFFFFFF remains the end marker; zero and unknown
handles fail instead of becoming host addresses. A fixed registry supports
65536 live tag pointers without per-frame allocations; reset requires discarding
every cached chain first. Tag lifetimes and payload extents remain the caller's
responsibility. This is not asset-pointer relocation or an untrusted chain parser.

`psx/gpu_ot.c` implements reverse ClearOTagR, setlen, addPrim and AddPrim. The
generic add API preserves the head length; the guarded source helpers reproduce
the original OT-specific behavior of clearing that length. Original primitive
builders now compile and are linked into tests. ClearOTagR's return value is not
used by the game; validation covers its chain side effects.

`gpu.links` tests two live tag addresses with identical low 24 address bits,
identity reuse, relocation-independent handle order, reverse OT traversal,
LINK_PRIM length/value behavior, and original tile/rectangle/draw-mode bytes.
It calls original frame_buffers_layout and ot_insert_prim, checks upper-index
clamping and cursor advances, traverses a 2050-node chain, and verifies OT
re-clearing preserves cached IDs. GS allocation in the builder test is an
explicit test pool; its real source implementation is covered separately.
Six rejection cases cover null/unaligned tags, zero/unknown handles, exhausted
capacity and empty OT lengths. Capacity validation also checks reuse at the limit.
No renderer, GPU synchronization or overlay pointer restoration is tested.

A small optional benchmark measures only warm OT construction; measured setup,
numbers and limitations are in PLATFORMS.md. There is no full-game performance
or parity claim from these CPU packet tests.

## CD bring-up surface

`psx/cd_position.c` implements CdIntToPos/CdPosToInt from public BCD MSF addressing
rules, without a disc or SDK code. Known vectors cover lead-in, frame/second/minute
boundaries, a multi-minute location and the upper two-digit-minute location.
An exhaustive round trip covers 450000 positions (LBA -150 through 449849).
The reserved fourth byte is preserved. A separate eight-byte GetlocP response
fixture passes its final three bytes to CdPosToInt under ASan; the helper must
not read a fourth byte. Malformed BCD and out-of-domain input behavior is not
claimed as validated PSY-Q parity.

Disc access now uses a platform-owned positional stream, verified against the
original raw BIN SHA-1 before mounting ISO-9660. The parser checks directory
record bounds, both-endian extents/sizes, nested paths and track boundaries.
Mode 1 and Mode 2 Form 1 supply 2048-byte data blocks; Form 2 is rejected by the
filesystem reader rather than truncated. No SDK or game bytes are included.

CdInit, CdSearchFile, Setloc/Setmode/Pause/Standby, CdRead, CdReadSync, CdSync,
CdReadBreak and CdFlush have a single-threaded data-read implementation. Every
SDK entry requires an attached validated disc. Eight rejection tests cover
missing-disc failures. Nonblocking reads advance only through hm_cd_tick at
75/150 sectors per second on a 60 Hz tick. Blocking ReadSync drains the pending
transfer. Seek latency, XA commands/filtering, controller retry timing and full
hardware status behavior are not yet implemented or parity validated.

The local hmbtn_disc_probe executable validated the owner's original USA track
(90967 raw sectors), found SYSTEM.CNF, SLUS_011.15, A_FILE.HDT and A_FILE.BIN,
and read their initial sectors through both the parser and SDK-facing CD API.
All 33 A_FILE offsets (32 member ranges) are monotonic and inside the
133935104-byte archive. The probe is a diagnostic, not a game executable.
No assets were extracted or added to git. The subsequent native boot probe
runs the original afile_init against the validated disc.

There are 73 base CTests, one optional SDL filesystem test and one optional
pause-frame test, all 75 passing in Windows release and ASan/UBSan builds with
the native pause interface enabled. New coverage
includes SHA-1 published vectors, malformed synthetic sectors/directories,
cross-sector file reads, deterministic CD transfer/cancel/error behavior,
VRAM transfers/masks/wrapping and the original texture-pack decoder. Tests need
no disc. The separate local disc probe also passed under ASan/UBSan.
Digital pad replies, disconnected-card completion, analytic fixed-point trig,
OT command execution and streamed VAB ownership now have additional coverage.
Persistent native memory-card containers and the Documents/BackToNature host
store are implemented without decomp edits (ADR-044). Tests exercise complete
32768-byte files, reopening serialized data, capacity, directory queries,
slot isolation, malformed mounts and failed writes preserving previous data.
The SDL test uses unique build-local Unicode paths and preserves existing
settings. This is storage API coverage; actual save/load menu behavior and
game-state serialization still require owner gameplay testing. Automated
gameplay is deferred at the owner's request.
SPU tests cover literal ADPCM predictor/shift/saturation vectors, ADSR phase
transitions/frozen rates, Gaussian impulse values, loop/end behavior, native
key status/release, sparse tone/sample lookup, SEQ/NRPN timing, reverb impulses,
retired bank lifetime, audio-ring behavior and PCM equality across chunk sizes.
Original pad polling, text addresses, animation tails and queue safety also
have regressions. Full audio/raster parity remains unverified.

The scalar inline GTE core now implements RTPS/RTPT, SQR, OP, MVMVA (source-used
matrix and translation selectors), NCLIP, AVSZ3 and AVSZ4 with explicit signed shifts,
44-bit accumulator wrapping, MAC0 overflow, IR/OTZ saturation and flag summary.
Literal formula vectors cover signs, limits and summary-bit exclusions. This
core is connected to clean-room inline_c/gtemac helpers and the native source
command macros. UNR division, FIFO/packet placement, clipping, matrix snapshots
and the original gte_project_vertex XY/depth/rejection path are exercised.
Unused MVMVA hardware-bug selectors and unfinished high-level math still fail
explicitly. Complete SDK init defaults and hardware/full-game parity remain
unverified; this is not a full GTE instruction-set implementation.

CdlFILE is host-side result metadata with only the fields used by the source,
pos and size. It is never read from disc bytes and does not claim the proprietary
SDK's binary layout. CD command constants cover only commands observed in source.

## Full-tree native syntax inventory

`port/analysis/compile_probe.py` invokes Clang on all original C files with the
port force-include and the basename include directories. It writes per-file
diagnostics to ignored `build/port/compile-probe.json`. No file is changed and no
game code is linked or run by this analysis command. All 113 files now pass
syntax checking. The separate CMake native build compiles all of them and links
the boot probe. Warnings remain visible and inventoried in the local report.

Common blockers include missing pad/card headers and SDK declarations,
inline GTE commands, GCC nested functions, pointer-width assignments and
macro-generated declarations not accepted by modern C. The probe's five-error
limit means this is not a complete error count. Syntax passes can still fail
during code generation, linking, runtime layout checks or sanitizer execution.

Warnings include pointer-to-integer/integer-to-pointer truncation, excess and
missing initializers, incompatible callback types, and a transposed memset
argument warning. They remain visible; broad warning suppression or automatic
source cleanup would hide real fidelity and 64-bit hazards.

The routine native archive now compiles all 113 original units, including the sound
loader/request/XA-control code, farm event commands, player/scene-view code,
debug overlay and minigames 3/5. Clean-room source-facing declarations for
libsnd/libspu and matrix/font/lighting calls supply native call contracts only;
remaining implementations fail explicitly at runtime in the diagnostic. A
complete overlay lifecycle and full gameplay are not yet validated. SpuCommonAttr
is native configuration metadata with only source-used fields, not a proprietary
SDK or asset ABI. SPU voice constants are the 24 bits consumed by key-status queries.
Callback/VSync dispatch, sound initialization/configuration and GPU environments
now have native state implementations. VAB banks load into owned native buffers;
SEQ playback now drives native voices and the SDL audio callback consumes PCM;
XA remains unfinished. DrawOTag supports sprites, tiles, polygons and lines in
a CPU reference rasterizer. Hardware raster/audio parity remains unverified.
The pending SDK layer contains named
abort paths for remaining calls, preventing unsupported work from returning
fabricated success. Real-disc Vulkan captures show the title, Start menu and
naming dialogue. The SDL/ASan window run completed 1200 VBlanks with actual
audio callback consumption; underflows were observed. Replays pass naming and
birthday setup and enter farm initialization, where SDK bring-up is incomplete.
No farm rendering/player movement or full playthrough is claimed yet. Physical
keyboard interaction, file-backed cards and overlay reentry remain unverified
or unfinished. Native memory fixes cover the title-picture overread, truncated
text addresses, discarded animation table tails and hardcoded queue offsets.

Eleven capture-free GCC nested helpers now have file-scoped native adapters under
ADR-013. Eight Python audit/regression tests pass, including token equality of
all eleven adapter bodies against matching definitions. The C helper test covers
triangle winding, boundary/height rejection, quad fallback, coordinate extrema,
estate flag priority, the ten visible medal-price rows and scene visibility
handoff at its counter threshold. The source compares
the ACTOR_FLAG_DIED_OF_AGE mask (1 << 19) with 1; that unusual comparison is
preserved and covered rather than corrected. The remaining captured-local
harvest helper receives its captured object explicitly; a body-token comparison
and distinct-object C test check that capture semantics are retained.

## Port guards and validation

Eighty `HMBTN_PORT` guard blocks have been introduced under ADR-008 through
ADR-047 (workspace, GPU/GTE/CPU adapters, nested helpers, event/text pointers,
overlays, title copies, SEQ spans, animation table tails, estate queues and
initial area selection):

| File | Guarded change |
| --- | --- |
| src/main/main.c | WORK_REGION selects native storage |
| src/main/main.c | main_region_gap_const subtracts pointers within that storage |
| src/main/main.c | main_region_free_bytes preserves the budget without overlay-address subtraction |
| src/main/main.c | main_get_region_base returns a full native byte pointer |
| src/main/main.c | Normal frame OT base uses its original offset |
| src/main/main.c | Wide frame OT base uses its original offset |
| src/main/main.h | Native region accessor has a pointer return type |
| src/main/game/farm_state.c | Overlay-address temporaries are matching-only |
| src/main/game/farm_state.c | Farm load window binds to native storage |
| src/main/graphics/prim.h | LINK_PRIM stores a native pointer handle instead of address bits |
| src/main/graphics/frame_buffers.c | ot_insert_prim uses the same native link codec |
| src/include/cpu_asm.h | Keep the native stack and use explicit diagnostic hooks |
| src/main/graphics/anim.c | Check alignment using the full host pointer |
| src/main/geometry/ground.c | Include two static helpers and exclude their nested copies (3 guards) |
| src/main/geometry/ground.c | Use 32-bit VECTOR component pointers on LP64 hosts (ADR-110) |
| src/main/geometry/model.c | Use 32-bit VECTOR component pointers in camera view construction (ADR-110) |
| src/overlay/11_GF_FARM/animals/animals.c | Include three static helpers and exclude nested copies (4 guards) |
| src/overlay/11_GF_FARM/estate.c | Include the static estate flag helper and exclude its nested copy (2 guards) |
| src/overlay/11_GF_FARM/menu/shop_dialogues.c | Include the static medal-row helper and exclude its nested copy (2 guards) |
| src/overlay/11_GF_FARM/event/event_rules.c | Include the static winter helper and exclude its nested copy (2 guards) |
| src/overlay/11_GF_FARM/world/scene.c | Include the static visibility handoff helper and exclude its nested copy (2 guards) |
| src/overlay/19_GF_MG2/mg2.c | Include the static board-marker helper and exclude its nested copy (2 guards) |
| src/overlay/30_GF_swind/swind.c | Include the static screen-area helper and exclude its nested copy (2 guards) |
| src/include/gte.h | Dispatch command words into native GTE state (1 guard) |
| src/main/text/text.h | Exclude conflicting externs for private format strings (1 guard) |
| src/overlay/11_GF_FARM/event/event_script.c | Native addresses, full-pointer alignment, byte-read offsets and fixed-prefix assertions (9 guards) |
| src/overlay/11_GF_FARM/event/event_script.h | Match native staging address declaration (1 guard) |
| src/overlay/11_GF_FARM/people/harvest_sprites.c | Explicit captured-object helper, nested-body exclusion and macro cleanup (3 guards) |
| src/main/overlay_regions.c | Bind asset regions to native workspace (1 guard) |
| src/main/main.c | Select statically linked overlays without loading PS1 code (1 additional guard) |
| src/overlay/27_GF_TITLE/title.c | Bound title-picture copy to the selected source array (1 guard) |
| src/main/sound/sound.c | Register the copied native sequence asset bounds (1 guard) |
| src/main/text/text_bank.h | Native offsetof and full text addresses (2 guards) |
| src/main/graphics/anim.h | Retain initialized flexible tails and byte-read table entries (2 guards) |
| src/overlay/11_GF_FARM/people/player.c | Preserve the native toy asset base (1 guard) |
| src/overlay/11_GF_FARM/estate.c | Clear message queue fields instead of PS1 offsets (1 additional guard) |
| src/overlay/11_GF_FARM/world/scene.c | Skip previous-mine overrides until an area is loaded (2 additional guards) |
| src/overlay/11_GF_FARM/world/scene.c | Write visibility only for present spouse markers (1 additional guard) |
| src/overlay/11_GF_FARM/people/player.c | Resolve cross-object probe step indices through named tables (1 additional guard) |
| src/overlay/11_GF_FARM/people/player.c | Select the facing reach row without crossing row-zero bounds (1 additional guard) |
| src/overlay/11_GF_FARM/event/event_script.c | Yield native VBlanks in background/foreground script and sound-bank waits (3 additional guards) |
| src/overlay/11_GF_FARM/people/player.c | Preserve no-terrain results without native address-zero reads (2 additional guards) |
| src/overlay/11_GF_FARM/event/event_request.c | Explicitly return the claimed request pointers (2 guards) |
| src/overlay/11_GF_FARM/people/harvest_sprites.c | Pass the helper to its action dispatcher callback (1 additional guard) |
| src/overlay/11_GF_FARM/menu/item_storage.c | Route native bag slots and marker navigation within their arrays (2 guards) |
| src/overlay/15_saveload/saveload.c | Contiguous save workspace and VBlank yields in both asset-loading waits (3 guards) |
| src/main/ui/bg_tile.c | Exactly typed native adapters for both background callbacks (3 guards) |
| src/overlay/27_GF_TITLE/title.c | Capture all native writable title state for reentry (1 additional guard) |
| src/overlay/15_saveload/saveload.c | Capture writable save/load state and workspace for reentry (1 additional guard) |

For each of these thirty files, selecting every matching `#else` branch and
removing the new guard directives reconstructs the previous source text exactly,
including its whitespace (line endings normalized for comparison). This check
passed against baseline revision 9ace470. It does not establish byte-matching
compiler output. Event staging callers preserve complete native addresses;
additional pointer/layout hazards remain before full gameplay is safe.

The current reconstruction covers 80 guards in 30 files. Following the owner's
Load-menu hang report, an existing-process LLDB stack identified the tight
CD-poll path. Synthetic regressions now cover both save/load asset waits,
save workspace boundaries, original card polling and background callback types
(ADR-045/046). These checks do not establish an in-game save/load pass.
Title and save/load now restore audited native object inventories on reentry
(ADR-047). Unit tests cover actual private state, alternating both overlays and
resident-state preservation. Other overlays continue to reject unsupported
reentry; full lifecycle and save ABI parity remain unverified.
The separate owner-requested pause branch reuses original window-frame C and
disc font pixels through private UI resources. The initial UI, SDL display
controls and JSON persistence require no further decomp guards. Actual expanded
camera/render scaling and AA remain pending. No game loop or replay was run
for this interface; technical checks are listed in ENHANCEMENTS.md.
The owner saw the initial pause interface and reported a blue panel. The private
renderer had reversed the primitive builders' actual RRGGBB color contract;
the BBGGRR comment in window_frame.c is inaccurate and is left untouched in
the matching source. The conversion is corrected and regression-tested. The
rearranged root/Graphics UI has private asset-only previews, not game sessions.

Historical owner report: sleeping/saving once crashed with 0xC0000005 (native exit
-1073741819). The owner later reported it safe; exact sleep/save scenarios were not
specified, so broader parity remains unverified. Raw PS1 packing views and the in-game
save ABI remain unaudited even though the native card backend tests pass. VSync/FPS settings use a separate
rational presentation clock (ADR-049). The original VSync(2) game loop and fixed 60 Hz
tick runtime are untouched. Synthetic tests verify invariant native callback counts
across presentation caps. Extra presentations duplicate the native image unless
enhance.interpolation is enabled. The requested experimental interpolation module uses
private VRAM/environment replay and generic observers at native-only main.c SDK call
aliases (ADR-050). Normal farm geometry can interpolate; other contexts retain native
scanout. All 76 release/sanitizer units pass; physical/gameplay validation remains
pending. No additional matching source edits or guards are required. The port builds all
113 units and links a boot diagnostic and SDL game window. Local 39000-VBlank
release/sanitizer replays complete the opening and verify house exit and free farm
movement after the Thomas/Zack dialogue. Wider gameplay and full-playthrough parity
remain unverified. Project-local Windows tools were provisioned: CMake 3.31.6 and Ninja
1.12.1. LLVM-mingw was upgraded from 20241001 (Clang 19.1.1) to 20250709 (Clang 20.1.8)
after ASan failed during startup on this host; see ADR-005. Windows release and
ASan/UBSan tests are recorded in PLATFORMS.md. An Android NDK Clang 18.0.3 freestanding
syntax check of the type test also passed; this is not an Android build. Disc
diagnostics are recorded above; full-game and device tests have not run.

Remaining Phase 0 gates: complete SDK and layout inventory, compiled overlay
symbol collision map, broader sanitizer coverage, and reproducible
matching host-tool pins. The owner prioritizes Windows and excludes workflow/CI
work for now. Existing workflows remain unchanged.

## Requested viewport-resolution graphics path

Enhance Scaling uses bounded native command capture and a private software
raster target at actual 4:3 viewport dimensions (ADR-052). Generic private replay
hooks allow interpolation to share the target. Frame observation now supports
independent subscribers; overlay changes invalidate each history. Native VRAM,
GPU environment and fixed game timing are restored/retained. Repeat native
scanouts reuse their rendered surface; loading/direct paths fall back to native.

The optional rasterizer implements polygon coverage, original texture sampling,
sprites/rectangles, native line-cell coverage, clipping, masks, semi-transparency
and inline transfers/fills. It has 1x comparisons against native rendering and
synthetic higher-resolution coverage/state-preservation checks. All 77 Windows
release and sanitizer tests pass. The native executable also links with scaling
ON / interpolation OFF and with enhancements OFF. No new matching guard/source
edit, dependency or game data is introduced. High-FPS gameplay, framebuffer
feedback quality, GPU resource changes and real-device output remain unverified.

The owner subsequently isolated black farm-ground cuts to Interpolation ON.
Field cells use anim_draw_sprt_cursor with shared AnimState records and mutable
GS allocation offsets. Native-only port adapters now tag constructed field
packets by world cell and packet part. main.c's native ClearOTagR call sites
reset the bounded registry. Source files and matching guards are unchanged.
Connected polygons also require consistent previous shared-vertex coordinates;
otherwise the component uses current positions. A two-quad regression fails
against f93bb01 and passes with this guard. Real farm confirmation is pending.
Shared gfx/frame_replay now owns capture independently of scaling; generic
destination hooks support private geometry consumption and fractional motion.
Portable packet and seam tests bring Windows release/sanitizer totals to 79.

## Shared native/enhanced GPU output

GPU Auto (graphics.renderer=0) now uses a portable tiled scene packet stream
and platform SDL_GPU compute execution; Software (1) remains selectable.
Native 1x output uses integer coverage/attribute math and matches synthetic CPU
RGB readback exactly on Vulkan and D3D12. High-resolution/fractional coordinates
use fixed-position fields with floating attribute interpolation. Fractional
sprite movement is tested across a logical tile boundary. Native CPU VRAM
remains authoritative, and normal output has no GPU readback/fence wait.

The no-window probe also checks 2048 deterministic random primitives against
CPU. GP0 fill, upload and VRAM-copy transfers are ordered scene records;
texture feedback after an earlier scene write is materialized through the CPU
reference and then sampled from an immutable GPU VRAM version. Direct VRAM
mutations outside a captured command stream, missing captures, malformed work,
and unavailable resources still retain software output. Android GPU Auto now
attempts each captured frame after ADR-151; performance and complete gameplay
coverage remain unverified. Own HLSL and generated arrays are hash-checked; DXC
is a pinned ignored developer tool, not a runtime dependency. Source game files
and matching guards remain unchanged. See ADR-054 and THIRD_PARTY.md.

### Optional texture filtering (2026-10-26)

ADR-055 adds palette-aware bilinear in GPU and private software output, independent
of scaling. Native VRAM and game clocks remain authoritative and unchanged.
Release/ASan/UBSan: 80/80 tests. Offscreen Vulkan and D3D12 pass native exact
and enlarged tolerance comparisons with Nearest/Bilinear. Filtering-ON with
scaling-OFF also compiles. No game was launched, no gameplay FPS measurement
was made, and atlas behavior in actual gameplay remains owner-tested.
No decomp-side source or matching guard changed.

### Widescreen source guards and renderer (ADR-056, 2026-10-26)

New guards all require HMBTN_PORT && HMBTN_ENH_WIDESCREEN:
- src/main/geometry/model.c: enhancement include, near_x/wide_x clip bands,
  horizontal camera_frustum_setup angles, and eight primitive-pass arena-reserve
  checks in model_draw_tmd_object.
- src/overlay/11_GF_FARM/estate.c: enhancement include and estate_point_on_screen
  horizontal sprite/weather drawing box. The sole external caller is weather
  drawing; particle stepping/respawn and RNG remain outside the visibility test.
- src/overlay/11_GF_FARM/field/field.c: enhancement include and the two opposite
  corners in field_square_cells (rendering-only grid coverage).

The original else branches/disabled source reconstruct HEAD exactly in a local
mechanical comparison. No SDK/GAP changes. Source projection/depth remain native. Wider
CPU culling feeds existing OTs; private replay expands world clipping while UI remains
native. Original primitive/overlay backup buffers are unchanged; private GS work arenas
are restored on OFF/scene exit. See ADR-056 for visibility/capacity limitations.

### All-screen viewport and fallback correction (ADR-057)

Widescreen eligibility no longer depends on farm view/dialogue state. Native
320x240 replay uses a stable expanded domain across scenes; direct/native/pause
presentation fills extra horizontal background from original edge columns.
The title.c-only animation adapter tags existing 0x25 tiled-background packet
spans; packet observation preserves these as background/world groups without
changing source commands, animation progression or RNG. No new src/ edits.

GPU rejection now uses clipped actual primitive bounds and sampled UV footprints
instead of oversized draw clips or full texture pages. Genuine offscreen writes,
feedback, unsupported transfers/capacities still fall back. Auto fallback retries
at native internal resolution, retaining full-size software only when explicitly
selected. Owner real-world aspect/dialogue observations still need revalidation.

### Runtime sleep/save/load corrections (ADR-058/059)

Windows crash RVA 0x1e1573 in release 19f2994 maps to the animal selection scan
used during overnight processing. Both animal base pointers were truncated to
s32. An isolated original call reproduces the ASan access violation. Native
byte-pointer arithmetic, explicit result forwarding and complete action-record
clearing now cover this path. This is separate from save serialization.

New matching-source guards:
- src/overlay/11_GF_FARM/animals/animal_actions.c: native action clear size,
  explicit slot result, and the two animal record address expressions (4 guards).
- src/overlay/15_saveload/saveload.c: codec include and native pack/unpack routing
  (3 additional guards). Original else implementations remain exact.

compat/save_codec writes a fixed 0x6378 payload, independent of native pointer
width and struct growth. Animal/player/helper wire fields use explicit strides;
runtime pointers are omitted and existing native bindings are preserved on load
(with original animal source invalidation). The card file remains 0x8000 bytes
with original shifts/checksums. The old packer reproduces an invalid run16 read
through its obsolete FarmState offset; the new synthetic fixture passes.
Livestock-transfer SaveFile/SaveImage casts are not covered by this codec audit.

The disabled new guards reconstruct both prior source files exactly after line ending
normalization. Full sleep, overnight events, menu save/load and PS1 interchange still
need owner validation; no gameplay or replay was used by these technical regression
tests.

### Follow-up dialogue, overnight wait and SDK audit (2026-10-27)

Additional source guards under ADR-060:
- src/overlay/11_GF_FARM/people/player.c: forward the required player argument
  rather than invoking player_advance_state through a zero-argument cast (1).
- src/overlay/11_GF_FARM/world/clock_events.c: native helper include and
  deterministic VBlank/CD progress in the overnight sound-fader wait (2).

The owner confirms saving now succeeds, but reports a later day-transition hang,
ordinary-dialogue access violation and missing ratan2 on entry to Yodel Ranch.
The dialogue fault maps to the missing argument. The overnight helper addresses
a reproduced progress requirement; the reported hang itself has no captured stack.
ADR-061 and API_COVERAGE.md document implemented math, unsupported PocketStation
completion and all nine remaining debug-font traps. Release and ASan/UBSan each
pass 89 tests; these are technical checks, not a complete gameplay pass.

### Widescreen follow-up (ADR-062, 2026-10-27)

Interpolation previously selected replay groups inside mesh matching, leaving
execution clipped to the UI group. It now restores world/UI per executed command.
Title base animation 0x1D is repeated alongside the scrolling 0x25 layer; bg_tile
submissions repeat their original four-column periods for menu side coverage.
Background ranges merge when adjoining and include both sprite and mode arrays.
Only CMake adapters/port code change; no additional matching guards are added.
Release and ASan/UBSan each pass 90 tests. The owner-reported camera displacement
after two sleeps remains unresolved; opt-in host coordinate logging is available.

### Pause canvas and source audit (ADR-063/064, 2026-10-27)

Pause now reconstructs a full logical wide frame once and keeps the original
menu proportional at the centre. Unavailable private transition frames use
proportional native output/black sides instead of fabricated edge-column strips.
See ADR-063 for logical-resolution and resize limitations.

New HMBTN_PORT guards under ADR-064:
- event/event_script.c: native helper include and villager sprite-copy routing (2).
- picture_cells.c: full-width gift table addressing and typed callback calls (2).
- animals/chicken.c: sizeof-native record clearing (1).
- farm_init.c: async-wait include and farm re-entry loader progress (2).

All paths are under src/overlay/11_GF_FARM/. Five minigame overlays use only CMake
asset-poll aliases, adding no matching-source guards. SOURCE_AUDIT.md records all
113 game C files and 68 native implementation units initially analyzed, confirmed
repairs and unresolved findings. The old sprite copy fails its synthetic fixture;
Release and ASan/UBSan each pass 94 tests after the repairs. Saibara dialogue and
two-sleep camera behavior remain owner-validation items.

### Follow-up audit: shop, family text and animal exchange (2026-10-27)

ADR-065 resolves the original adjacent shop vectors explicitly and corrects the
family-name pointer return type using strings read from the owner's validated
archive. New guards: shop_dialogues.c includes runtime errors, validates request
IDs, selects the adjacent-vector equivalent and forwards the basket result (4);
people/family.c returns loaded native text (1); people/family.h preserves the
full-width return type (1). All are under src/overlay/11_GF_FARM/.

ADR-066 adds 15 guards in src/overlay/15_saveload/saveload.c for native partner
view/stash storage, decode/encode, farm identity, foreign capacities and names,
count/free-slot routing, list views, typed restoration and complete buffer reset.
The partner view is included in compat/saveload_overlay_state.inc. Original
SaveFile/SaveImage wire casts remain only in disabled original alternatives on
these transfer paths. They are not host object layouts.

The old 928-byte stash reproduces a global-buffer-overflow on a 224-byte write.
New transfer fixtures cover five native cows/chickens and preserve all non-animal
payload bytes. The family text loader passes synthetic fixtures and an isolated
read-only check with the validated owner BIN. Release and ASan/UBSan pass 99 tests;
targeted Clang analysis of seven touched units reports no core/unix/security
findings. Gameplay and matching compiler output remain unverified.

### Third audit batch: optional outputs and callback ABI (ADR-067)

New matching-source guards:
- src/overlay/11_GF_FARM/estate.c: optional pickup seasonal output and initialized
  basket probe (2).
- src/overlay/11_GF_FARM/farm_probe.c: tile hit-class initialization, optional
  capped output and local discard outputs for boolean-only occupant calls (3).
- src/main/sound/sound.c: full-width sequence sentinel comparison (1).
- src/main/graphics/texture_pack.c: diagnostic header, null bank and image index
  bounds (3).
- src/overlay/17_GF_MG1/mg1.c: void native award-frame declaration/definition and
  return, since only the void frame table consumes it (3).
- src/overlay/23_GF_MG4/mg4.c: no-argument native state callback type and call (2).

The old optional-pickup and boolean terrain paths reproduce null member/store
errors in bounded fixtures. Native sequence comparison fails its mocked low-word
sentinel fixture before correction. Valid pickup transformations, actor flags,
terrain decisions and callback quotas remain covered. Runtime image diagnostics
replace an uninitialized rectangle on missing images, not a successful fallback.
Both Windows builds pass all 106 tests; targeted analysis of six changed units
reports zero selected analyzer/ABI warnings. Original source reconstruction was compared separately; interactive gameplay
remains unverified. No gameplay was launched in this batch.

### Wide transitions and rucksack shade (2026-10-27)

ADR-068 adds observation of main.c's direct fade OT path, retaining the captured
wide scene while full-screen subtraction accumulates. Exact full-screen TILE
packets now expand without corrupting subsequent command boundaries. CPU/GPU
side initialization no longer copies changing native edge columns. Direct and
loading scanouts stay proportional within a wide output canvas. This batch
changes only port code/docs, adding no HMBTN_PORT guards or matching-source edits.
Release and ASan/UBSan pass 106/106 tests; offscreen Vulkan/D3D12 comparisons
pass. Full interactive coverage and map-side asset availability remain unverified.

### Farm summary hang follow-up (2026-10-27)

ADR-069 extends the blocking CD poll adapter to status_pages.c. Its only
afile_read_bytes calls initiate and poll inside the same blocking loader. The source
fixture also confirms out-of-array addressing of the two shared sprite globals. Four
HMBTN_PORT guards in src/overlay/11_GF_FARM/menu/status_pages.c add the slot resolver
include, guarded loader binding and guarded sprite selection in status_draw_sprite_label
and status_draw_entry. Matching toolchain unavailable, no WSL. The original loader
fixture and all 107 Release/ASan tests pass. No gameplay or owner save is used in the
fixture.

### Splash backdrop and cutscene buffer identity (2026-10-27)

ADR-070 corrects white Natsume side initialization using the actual DRAWENV
background colour. GPU header word 14 carries the same 15-bit colour used by
software and native scanout. Frame replay now tracks the displayed rectangle
rather than the simulation key that can change before a buffer flip; bypass,
age and overlay invalidation remain enforced, and interpolation stays strict.
Black/white GPU readbacks and displayed-buffer regression tests pass. This is
a candidate correction for the reported Won flash, not an interactive result.

### Fade/context lifetime and persistent post-save projection (2026-10-27)

ADR-071 fixes a verified GPU scene-end read from the restored live environment:
the backdrop now belongs to the captured frame. Clear-only completed frames are
valid without primitive words. An original screen_fade_frame fixture checks both
framebuffers, fade directions and backdrop changes against native VRAM pixels.

The original main frame-reset fixture reproduces a zero GTE projection centre
on the saved-farm return path, which skips full camera initialization. A native
SDK adapter at main.c's call site restores (160,120) for saved/menu farm returns
only. It does not change source, camera eye/target or the save format. No new
HMBTN_PORT guards are added. 108 Release/ASan tests and Vulkan/D3D12 offscreen
readbacks pass; actual TV/splash/post-save behavior still needs owner validation.

### Unchanged display buffers across blocking waits (2026-10-27)

ADR-072 replaces shared replay's four-tick age cutoff with native framebuffer
write invalidation. Source fixtures reproduce rejection of an unchanged display
after a long wait. Native image transfers notify overlapping capture/history
rectangles with SDK wrapping/alignment preserved; unobserved loading OTs and
native VRAM reset invalidate affected history. Private replay transfers do not.
109 Release/ASan tests and offscreen Vulkan/D3D12 checks pass. No new matching
guards or source edits are needed. Owner now reports save/next-day camera safe;
Natsume fade-out, TV and home-exit side flicker await revalidation.

### Farm overlay re-entry inventory (2026-10-27)

Owner confirms Natsume safe on 9c57d5a but reports a load force-close. Runtime
diagnostics identify the existing unsupported second activation of member 11.
ADR-073 inventories all 51 farm TUs: 703 file-scope objects, 677 mutable objects
(49,201 bytes on Windows x64), 26 top-level const exclusions, and 56 local cue
tables read only by farm_cue_step_run. Native wrappers add per-object providers
without changing matching source or sanitizer instrumentation. Source/header
fingerprints and TU coverage prevent silently stale metadata. Original SDK remaps
for field/status are preserved. Resident state and owner save files are excluded.
110 Release/ASan tests and a fresh AST inventory --check pass. Original lifecycle
failure is reproduced before correction. Actual repeated load remains owner-tested.

### Actor alias layout audit (2026-10-28)

ADR-075 addresses the owner-reproduced dog put-down crash. LLDB identifies a
partLists read through a pointer whose low 32 bits became 0x0000ffff. An isolated
test reproduces that exact write in the original farm_action_begin: its padded
FarmActionSlot.cue at 0xa8 aliases native FarmActor.sprite.partLists, not cue.
The native replacement accesses the shared actor definition instead. The related
player helper-number alias at 0xac reads pointer bits instead of FarmHelper.number
at 0xd0; a checked native expression adapter corrects it.

Reviewed scalar-prefix aliases remain valid: item at 0, facing at 0x22, nearby
action value at 0x1a, villager mode at 0x1e, place at 0x30, flags at 0x34 and
position at 0x38. ACTOR_OF_POSITION still agrees with native position offset.
Static assertions protect the replacement's task/flags/speed assumptions. This
audit covers the animal/player aliases, not every runtime path. No matching
source, header, source guard, game data or save representation was changed.

HMBTN_PORT guard under ADR-077: people/player.c replaces the HOPS[row][facing]
read that overruns move_steps_large into the adjacent hop_steps object with
explicit row selection (move_steps_large for row 0, hop_steps for row 1).

HMBTN_PORT guards changed under ADR-079: the eight widescreen UI-reserve checks in
geometry/model.c now call hm_widescreen_prim_full instead of comparing against
primStart + primSize. Their original (#else) behavior is unchanged.

HMBTN_PORT guard under ADR-106: src/main/text/text.c terminates fast text glyph advancement at TEXT_CODE_PAGE_END instead of TEXT_CODE_PAGE_MARK so text_find_nth finds the page end and advances pages on user input.

HMBTN_PORT guards under ADR-107: src/overlay/11_GF_FARM/menu/storage_swap.c prevents cross-swapping between tools and items in rucksack reorganize screen under mouse and controller input.

HMBTN_PORT guards under ADR-108: src/overlay/11_GF_FARM/field/field.c and src/overlay/11_GF_FARM/field/greenhouse.c cast position->vz to (s16) to prevent 64-bit unsigned promotion sign loss for negative world Z coordinates.

HMBTN_PORT guard under ADR-109: src/main/text/text.c pauses dialogue typing instantly at TEXT_CODE_PAGE_MARK and TEXT_CODE_PAGE_END when Fast Text is active, preventing text_phase corruption and ensuring page breaks and choices advance cleanly on user confirm input.

ADR-111 (port-only): Tile Targeting lacked scripted/event visibility guards
and biased its reverse OT depth behind terrain. Its gameplay checks now cover
actual terrain occlusion and the OFF path. Interpolation's shared-vertex
coherence omitted implicit rectangle corners, allowing mixed fallback between
adjacent ground sprite rows. Include those corners in the existing rule and
test changed-material rows on CPU and enlarged GPU output. No matching source
or manifest changes are needed. The owner's full gameplay scenario remains
unverified by these isolated fixtures.

ADR-112 (port-only): Live inspection confirmed Tile Targeting was enabled but
its primitive-capacity guard compared a private widescreen cursor against the
original overlay-backup `primBuffer`. Both targeting and tool-charge indicator
checks now use `primStart` and the active arena capacity. Add regression checks
with separate active/backup buffers and a genuinely exhausted active buffer.
No matching source, layout guard or overlay manifest changes are required.
The owner deferred the separate horizontal-line report while running.

HMBTN_PORT guards under ADR-120: `src/overlay/11_GF_FARM/world/weather.c` includes `enhance/widescreen/widescreen.h` and scales the horizontal weather particle spawn range to `[-margin, 320 + margin)` when `hm_widescreen_margin() > 0`, ensuring rain and weather effects cover the full widescreen viewport while preserving exact 4:3 vanilla parity and PRNG step count when Widescreen is OFF.


ADR-130 extends the existing `HMBTN_PORT && HMBTN_ENH_GAMEPLAY` guarded
`status_draw_stamina_hud` in `src/overlay/11_GF_FARM/menu/status_pages.c` to
provide a fatigue indicator using the original state. The matching build does
not compile that guarded function. The farm overlay manifest records the
normalized source hash.

ADR-144 adds `HMBTN_PORT && HMBTN_ENH_NPC_BONDS` guards in
`src/overlay/11_GF_FARM/menu/status_pages.c` for the optional ninth page handler
runtime dispatch, disabled-page recovery, and previous/next wrap limits. The
exact original eight-page handler and wrap behavior remain in the matching-build
branches. The farm overlay manifest records the normalized source hash.

ADR-155 adds port-only weather particle stepping in
`src/overlay/11_GF_FARM/world/weather.c`. Paused native dialogue continues
particle movement and respawn with a private RNG state; the gameplay RNG is
restored after each paused weather update. The original particle pause behavior
remains unchanged in the matching-build branch, and the farm overlay manifest
tracks the normalized source hash.

ADR-080 preserves tile interaction flags while the native Walk Crops option makes
field crops and weeds passable. The movement-only probe forwards the player actor
identity, and the tile callback skips crop/weed hits before setting blocking result
flags; other actors and obstacles still participate. The separate sprint-step
lookup also honors the option. The matching branches remain unchanged, and the
farm overlay manifest tracks the normalized source and header hashes.

ADR-156 extends the GPU VRAM scanout recovery path to captured scenes that fail
replay or packing, with CPU rendering retained if scanout itself cannot be
submitted. A GPU scene regression verifies scanout can rebuild after an
unsupported GP0 opcode.

ADR-160 adds `HMBTN_PORT && HMBTN_ENH_SHIPPING_BIN` guards in
`src/overlay/11_GF_FARM/people/player.c` and `player.h`. The port searches all
active `ZONE_SHIPPING_BOX` entries within one farm tile of the player instead
of relying on a first-match zone probe, which can hide an overlapping shipping
  zone. The guard also stores a held stack's count in the thrown-item padding and
  records every unit in both the standard landing-to-bin branch and the
  supplemental proximity branch. The matching
branch retains the original single-item shipping call and struct layout; the
farm overlay manifest records the normalized source and header hashes.
