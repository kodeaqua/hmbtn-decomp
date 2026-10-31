# Native source audit - 2026-10-27

## Coverage and limits

The baseline compiler pass and Clang Static Analyzer covered all 113 C files in
src/, including all 13 overlays, using the actual Windows Release native compile
definitions and include paths. Eight linked native adapters were included in that
pass. A separate analyzer pass covered 68 native implementation translation units
linked into hmbtn_native; it reported no core/unix/security analyzer warnings.
The subsequently added asset_wait adapter is covered by a bounded timing fixture.
Headers are checked through their including translation units, not independently.
No SDL dependency source, matching SDK/toolchain, asset content or gameplay was
audited by these compiler passes. There were no compiler/analyzer invocation
errors. This is automated whole-tree coverage, not a manual review of every line
or proof that every path is safe. Analyzer warnings remain hypotheses until
verified; padding warnings especially do not establish behavioral bugs.

Local evidence is ignored: build/port/source-audit-diagnostics.json,
source-audit-analyzer.json, port-audit-analyzer.json and regression/test logs.
No owner save or screenshot is included in git. No game/replay was automated.

ADR-073 adds a targeted farm-state AST inventory after the owner reproduces a
second-load overlay abort. It covers all 51 farm TUs and 703 file-scope objects;
the compiled inventory snapshots 677 mutable objects and excludes 26 const
objects. All 56 function-local static arrays are cue tables with one read-only
consumer, checked by AST reference use and a read-only-storage fixture. Generated
native wrappers retain source-specific SDK definitions. No matching source was
edited. These checks establish state restoration, not a complete gameplay audit.

## Confirmed repairs (ADR-064)

Later owner-triggered finding (ADR-069): Start opens the farm summary through
an empty blocking CD poll that cannot advance deterministic interrupts. Remap
that translation unit to the existing yielding adapter. The original loader
fixture subsequently reproduces indexing beyond status_page_sprites[3]; native
slots 3/4 now resolve the shared cursor/mark globals explicitly. All five loader
bindings and the variable draw accesses use the same checked resolver. The
bounded loader fixture passes with two host ticks and no disc/save/gameplay.
107 Release/ASan tests pass; this does not establish full status-page navigation.

- Event preparation copied 48-byte sprites at a 468-byte villager stride. Native
  AnimState and FarmVillager are larger. Copy full named sprite80 into sprite in
  each native record, preserving the original 0x82 reload behavior. The old copier
  fails the same fixture's whole-record comparison; the new copier preserves all
  surrounding fields and complete bindings for all 37 records.
- Gift reaction lookup formed an address by truncating pictureCodes to s32.
  Native indexing preserves the real pointer and original 3-bit extraction.
- Chicken clearing retained native animation tails after its PS1-sized memset.
  Clear the full native record, with a poisoned-tail regression.
- Farm re-entry repeated the same non-progressing audio/CD wait as overnight.
  Reuse the deterministic wait helper already tested with required VBlank credit.
- Fifteen minigame startup asset waits do not yield hardware progress. Remap only
  those five overlays' afile polls through a pending-read yield adapter; completed
  reads/request initiation and ordinary asynchronous per-frame reads add no tick.
- Two item callbacks have incompatible historical function signatures. Native use
  dispatch recognizes their identities and calls their actual prototypes. It does
  not invoke a converted pointer or fabricate callback results. Coin and berry
  behavior is exercised through the original item table and use dispatcher.

The reported Saibara/outside-blacksmith crash is an access violation at
sprite_draw_frame+0x26, RVA 0xedc36, release timestamp 0x6adfcd97. The bad villager
copy is consistent with corrupted animation bindings, but the actual crash has
no stack capture establishing that call chain. Owner revalidation remains needed.
Camera displacement after two sleeps remains open; logged centre/player divergence
can also occur at camera clamp boundaries and is not itself proof of corruption.

## Open findings and triage

| Finding | Classification / next evidence needed |
| --- | --- |
| family_table_lookup returns raw PS1 addresses used as status text | Resolved by ADR-065: validated original-disc text and full-width native return |
| shop_screen_start_handlers has one declared entry but is indexed by request id | Resolved by ADR-065: verified original vector adjacency and explicit native indexing |
| shop_basket_step and MG1/PdaSamp non-void fall-through | Basket forwards completion (ADR-065); MG1 is now a native void frame handler (ADR-067); unsupported PdaSamp still needs review |
| Livestock-transfer raw SaveFile/SaveImage views | Resolved by ADR-066 native partner view and bounded stash; interactive failure/abort/retry still unverified |
| Additional incompatible callback casts in main/minigames | MG1/MG4 native frame/state signatures corrected (ADR-067); main/PdaSamp diagnostic paths remain open |
| sound_update pointer-to-s32 sentinel comparison | Resolved by full-width pointer comparison and ordinary/opaque/sentinel slot fixtures (ADR-067) |
| cue_history scriptOffset uses subtraction of truncated pointers | Selected offsets are masked u16 values up to 0xFFFC inside the same asset; wrapped s32 subtraction preserves this offset, no host address is dereferenced; no change made |
| player_try_nearby_action casts the greeting result to a pointer | Current callee always returns zero, so this does not currently dereference a nonzero result |
| Analyzer: texture pack/event/probe uninitialized paths | Texture index and terrain/estate probe findings resolved (ADR-067); event-facing range precondition still needs caller validation |
| Analyzer: helper/animal by-value vectors include uninitialized padding | Distinguish unused padding from actual vx/vy/vz use; do not alter original logic blindly |
| Analyzer: name-entry/estate/grid/helper null paths | Estate pickup/boolean query null outputs resolved (ADR-067); name-entry/grid/helper callers still need individual validation |
| Nine debug-font API traps and partial SDK modes | See API_COVERAGE.md; no full-SDK implementation claim |

Release and ASan/UBSan now pass 106/106 tests without disc data. ADR-065/066 close the
first batch's shop/family/transfer layout findings. Seven changed translation units
passed targeted follow-up analysis without core/unix/security findings. An additional
read-only fixture loaded all five family names from the validated owner BIN; it did not
run gameplay or access saves. The original 928-byte transfer stash was separately
reproduced as an ASan global-buffer-overflow. Remaining rows below still need individual
caller/layout evidence; this batch does not establish complete gameplay coverage.

## Third follow-up batch (ADR-067)

The optional estate pickup is a real dog-ball caller contract, not only an analyzer
hypothesis. A bounded fixture reproduces a null member read; a separate actual
terrain caller fixture reproduces a null store. Corrections retain record flags,
seasonal forms and boolean callback decisions, using local outputs only when none
are requested. Sequence low-word sentinel behavior is reproduced with a mocked
opaque address (never dereferenced) and checked alongside a real aligned fixture.

Texture bank missing-image paths now fail explicitly; valid decoder output stays
covered. Terrain hit class/estate probe fields are initialized. MG1's void frame
use and MG4's seven no-argument state callbacks now match native declarations.
Six touched translation units pass targeted analysis without selected analyzer,
function-cast, pointer-cast or return findings. The whole-tree inventory below
remains the original baseline, not a claim that all remaining warnings vanished.

Ignored evidence: build/port/audit3-before-tests.log,
audit3-probe-before-tests.log, audit3-analyzer.json, audit3-release-tests.log and
audit3-asan-tests.log. No owner assets or gameplay runs are used.

## Per-file automated coverage

Every row below completed the native syntax and analyzer passes. Warning counts
are baseline compiler warnings (including initializer/style warnings) and selected
core/unix/security analyzer diagnostics, not counts of proven defects. Repairs
above were made after this baseline and are covered by regression tests.

| File | Compiler warnings | Analyzer findings |
| --- | ---: | ---: |
| src/main/cd/afile.c | 37 | 0 |
| src/main/debug_stubs.c | 8 | 0 |
| src/main/game/cooking_recipes.c | 760 | 0 |
| src/main/game/event_flags.c | 0 | 0 |
| src/main/game/farm_data_size.c | 0 | 0 |
| src/main/game/farm_state.c | 0 | 0 |
| src/main/game/farm_tables.c | 24 | 0 |
| src/main/game/horse_names.c | 0 | 0 |
| src/main/game/picture_codes.c | 0 | 0 |
| src/main/game/shop_restock.c | 1 | 0 |
| src/main/geometry/camera.c | 5 | 0 |
| src/main/geometry/ground.c | 3 | 0 |
| src/main/geometry/math_util.c | 5 | 0 |
| src/main/geometry/model.c | 28 | 0 |
| src/main/geometry/vec_length.c | 1 | 0 |
| src/main/graphics/anim.c | 10 | 0 |
| src/main/graphics/clut.c | 1 | 0 |
| src/main/graphics/frame_buffers.c | 1 | 0 |
| src/main/graphics/prim.c | 0 | 0 |
| src/main/graphics/screen_fade.c | 5 | 0 |
| src/main/graphics/texture_pack.c | 3 | 1 |
| src/main/input/pad.c | 0 | 0 |
| src/main/input/pad_vibration.c | 0 | 0 |
| src/main/main.c | 5 | 0 |
| src/main/overlay_regions.c | 0 | 0 |
| src/main/sound/cd_audio.c | 0 | 0 |
| src/main/sound/sound.c | 25 | 0 |
| src/main/sound/sound_request.c | 5 | 0 |
| src/main/system.c | 0 | 0 |
| src/main/text/text.c | 15 | 0 |
| src/main/text/text_bank.c | 0 | 0 |
| src/main/ui/bg_tile.c | 0 | 0 |
| src/main/ui/heart.c | 0 | 0 |
| src/main/ui/loading_screen.c | 7 | 0 |
| src/main/ui/menu_window.c | 6 | 0 |
| src/main/ui/name_entry.c | 1 | 2 |
| src/main/ui/ui_icons.c | 0 | 0 |
| src/main/ui/window_frame.c | 0 | 0 |
| src/overlay/08_DEBUG/debug.c | 1 | 0 |
| src/overlay/08_DEBUG/header.c | 1 | 0 |
| src/overlay/10_PdaSamp/header.c | 1 | 0 |
| src/overlay/10_PdaSamp/pdasamp.c | 13 | 0 |
| src/overlay/11_GF_FARM/animals/animal_actions.c | 1 | 2 |
| src/overlay/11_GF_FARM/animals/animals.c | 11 | 1 |
| src/overlay/11_GF_FARM/animals/chicken.c | 4 | 1 |
| src/overlay/11_GF_FARM/animals/dog.c | 4 | 0 |
| src/overlay/11_GF_FARM/animals/horse.c | 2 | 0 |
| src/overlay/11_GF_FARM/animals/livestock.c | 8 | 0 |
| src/overlay/11_GF_FARM/demo.c | 0 | 0 |
| src/overlay/11_GF_FARM/estate.c | 7 | 3 |
| src/overlay/11_GF_FARM/event/cue.c | 0 | 0 |
| src/overlay/11_GF_FARM/event/cue_history.c | 2 | 0 |
| src/overlay/11_GF_FARM/event/event_commands.c | 115 | 1 |
| src/overlay/11_GF_FARM/event/event_request.c | 2 | 0 |
| src/overlay/11_GF_FARM/event/event_rules.c | 0 | 0 |
| src/overlay/11_GF_FARM/event/event_script.c | 1 | 0 |
| src/overlay/11_GF_FARM/event/messages.c | 2 | 0 |
| src/overlay/11_GF_FARM/event/milestones.c | 0 | 0 |
| src/overlay/11_GF_FARM/farm_data.c | 0 | 0 |
| src/overlay/11_GF_FARM/farm_frame.c | 1 | 0 |
| src/overlay/11_GF_FARM/farm_init.c | 1 | 0 |
| src/overlay/11_GF_FARM/farm_load.c | 2 | 0 |
| src/overlay/11_GF_FARM/farm_main.c | 0 | 0 |
| src/overlay/11_GF_FARM/farm_probe.c | 8 | 1 |
| src/overlay/11_GF_FARM/farm_vars.c | 0 | 0 |
| src/overlay/11_GF_FARM/field/field.c | 8 | 2 |
| src/overlay/11_GF_FARM/field/field_item_requests.c | 0 | 0 |
| src/overlay/11_GF_FARM/field/fishing.c | 8 | 0 |
| src/overlay/11_GF_FARM/field/greenhouse.c | 1 | 0 |
| src/overlay/11_GF_FARM/field/mine.c | 2 | 0 |
| src/overlay/11_GF_FARM/field/pond.c | 0 | 0 |
| src/overlay/11_GF_FARM/header.c | 1 | 0 |
| src/overlay/11_GF_FARM/menu/calendar.c | 5 | 0 |
| src/overlay/11_GF_FARM/menu/house.c | 2 | 0 |
| src/overlay/11_GF_FARM/menu/item_storage.c | 4 | 0 |
| src/overlay/11_GF_FARM/menu/menus.c | 3 | 0 |
| src/overlay/11_GF_FARM/menu/options_menu.c | 1 | 0 |
| src/overlay/11_GF_FARM/menu/photo_album.c | 0 | 0 |
| src/overlay/11_GF_FARM/menu/shop_dialogues.c | 19 | 1 |
| src/overlay/11_GF_FARM/menu/shop_page.c | 0 | 0 |
| src/overlay/11_GF_FARM/menu/status_pages.c | 13 | 0 |
| src/overlay/11_GF_FARM/menu/storage_swap.c | 3 | 0 |
| src/overlay/11_GF_FARM/people/family.c | 3 | 0 |
| src/overlay/11_GF_FARM/people/harvest_sprites.c | 5 | 20 |
| src/overlay/11_GF_FARM/people/player.c | 24 | 2 |
| src/overlay/11_GF_FARM/people/villager.c | 7 | 0 |
| src/overlay/11_GF_FARM/picture_cells.c | 5 | 3 |
| src/overlay/11_GF_FARM/world/clock.c | 2 | 0 |
| src/overlay/11_GF_FARM/world/clock_events.c | 1 | 0 |
| src/overlay/11_GF_FARM/world/doors.c | 0 | 0 |
| src/overlay/11_GF_FARM/world/scene.c | 6 | 0 |
| src/overlay/11_GF_FARM/world/scene_view.c | 2 | 0 |
| src/overlay/11_GF_FARM/world/weather.c | 2 | 0 |
| src/overlay/15_saveload/header.c | 1 | 0 |
| src/overlay/15_saveload/saveload.c | 32 | 0 |
| src/overlay/17_GF_MG1/header.c | 1 | 0 |
| src/overlay/17_GF_MG1/mg1.c | 2 | 0 |
| src/overlay/19_GF_MG2/header.c | 1 | 0 |
| src/overlay/19_GF_MG2/mg2.c | 8 | 0 |
| src/overlay/21_GF_MG3/header.c | 1 | 0 |
| src/overlay/21_GF_MG3/mg3.c | 7 | 0 |
| src/overlay/23_GF_MG4/header.c | 1 | 0 |
| src/overlay/23_GF_MG4/mg4.c | 7 | 0 |
| src/overlay/25_GF_MG5/header.c | 1 | 0 |
| src/overlay/25_GF_MG5/mg5.c | 2 | 0 |
| src/overlay/27_GF_TITLE/header.c | 1 | 0 |
| src/overlay/27_GF_TITLE/title.c | 0 | 0 |
| src/overlay/28_GF_STAFF/header.c | 1 | 0 |
| src/overlay/28_GF_STAFF/staff.c | 0 | 0 |
| src/overlay/30_GF_swind/header.c | 1 | 0 |
| src/overlay/30_GF_swind/swind.c | 6 | 0 |
| src/overlay/31_gf_mcard/header.c | 1 | 0 |
| src/overlay/31_gf_mcard/mcard.c | 16 | 0 |
