# Windows native playtest

## Current status (2026-10-30)

The Windows -Game build launches the original compiled game, reaches the farm,
supports free movement, and includes pause/settings, memory-card containers,
and the optional graphics paths summarized in ENHANCEMENTS.md. The latest
recorded Windows suite is 124/124; it was not rerun for this documentation
update. Full-playthrough parity, complete GPU scene coverage, and several
display/device cases remain unverified.

The owner confirmed that a reported horizontal line disappeared in one farm
scene with Modern, Scaling On, Widescreen Off, MSAA 4x, Nearest, Interpolation
Off, VSync On, and unlimited FPS. Other scenes and performance remain
unverified. ADR-136 temporarily disables in-game mouse/touch actions; pause
menu mouse navigation remains available.

Owner follow-up: the owner reports that sleep/save is now safe. The exact
scenarios covered were not specified, so card-container tests and this report
do not establish broad sleep/day-transition/save/load parity. Reopen the
earlier crash report only if it reproduces. The owner performs interactive
gameplay checks. Use the owner's original USA SLUS_011.15 BIN; it is validated
at startup and is never copied into git. No WSL, Docker, or CI is required.

The dated checkpoints below are historical; later ADRs and the status above
supersede older test counts and feature limitations.

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/run.ps1 -DiscPath 'D:/Games/Harvest Moon USA.bin'
```

The executable is dist/VanillaBTN-windows-x64/VanillaBTN.exe. It finds the disc in
this order: an explicit path argument (used alone, never replaced by a fallback);
the `disc_path` remembered in Documents/VanillaBTN/settings.json; then each
`*.bin` next to the executable, in name order. Every candidate is validated
against the USA disc, and a working path is stored as an absolute `disc_path`.
If none validates, a message box explains how to supply it. run.ps1 accepts
-DiscPath optionally. The launcher
builds it if missing, disables diagnostic replay/tick limits for normal play,
and restores the caller's environment afterward. Click the game window for
keyboard focus. Enter is Start; use arrows to select New Game and X to confirm.
X selects name characters and Enter accepts a completed name. Progress opening
dialogue with X.

## User data and manual validation

The owner performs interactive gameplay validation. Development checks use
isolated unit tests; no automated gameplay is run without a new owner request.

Native Windows builds now initialize the active user's Documents/VanillaBTN
directory through SDL's user-folder API. Redirected Documents (including OneDrive)
is respected. The directory contains:

```
VanillaBTN/
  saves/
    card1.hmc
    card2.hmc
  settings.json
  .lock
```

The cards are native containers for the original game's save-file bytes, not
raw PS1 card images. Back them up together when the game is closed. The game
must be closed before another instance can use this directory. Write/permission
errors are reported rather than claiming a successful save. Invalid existing
cards are not automatically replaced; an explicit format erases the selected
card. settings.json is initialized with a schema version and preserved if it
already exists. Graphics, controls, and gameplay settings are loaded and
persisted; malformed settings are not silently overwritten.

The storage backend has unit coverage for writes and reopening data. Actual
in-game save/load, closing/reopening a session and repeated save-menu activation
still require owner testing. This build does not yet claim working full-session
save/load or full-playthrough parity.

The title-to-Load hang reported on 2026-10-26 was traced in the owner's existing
process to an asynchronous asset-read busy loop. Native save/load reads now yield
VBlank while pending, advancing CD work and pumping window events. The complete
save-image workspace is one bounded native object; card-existence polling and
shared menu-background callback types are corrected as well (ADR-045/046).
Release and ASan/UBSan pass 69 isolated tests. No new gameplay was launched for
this fix; the owner still needs to confirm the menu behavior interactively.

The owner confirmed that Load now opens, then reported overlay reentry failure
when returning to title. The title and save/load overlays now restore their
complete audited writable native object state on activation (ADR-047). Other
overlays still reject unsupported reentry. Release and sanitizer builds pass
71 technical tests; no game loop or interactive replay was run for this change.

## Historical pause branch checkpoint (2026-10-26)

On port/windows-pause-menu, build.ps1 -Game now enables the initial native ESC
interface. Use arrows to navigate, Enter/X to confirm, and ESC/D to go back;
ESC at the root resumes. Settings > Graphics offers monitor/window mode/
driver-enumerated resolution and nearest/bilinear final-frame presentation.
Graphics changes are persisted in settings.json. Borderless follows desktop
resolution; normal presentation preserves 4:3 with black bars.

The UI uses the game's runtime disc font and actual dialog-frame drawing code,
with private resources that survive overlay switches. The implementation holds
game/CD/SPU ticks, pauses the audio device and excludes paused time from pacing.
These interaction/audio/display behaviors still require owner testing; no game
was launched by the agent. Release and ASan/UBSan pass 74 technical tests.
Set enhance.pause_menu=false in the enhance object of settings.json to disable
ESC, or build with HMBTN_ENH_PAUSE_MENU=OFF for the compile-time OFF path.

At this checkpoint, Enhance Scaling, Widescreen Mode ON, and FXAA/MSAA were
not yet implemented. Later ADRs added those renderer paths; see the current
status above and ENHANCEMENTS.md for their coverage limits.

The owner reported the first pause panel was blue. Its private canvas had
reversed red/blue relative to the original primitive builders. This is now
corrected to the original cream palette. Root/Settings use compact centered
panels; Graphics has separate header, aligned label/value rows and a detail
footer. Synthetic RGB/layout checks and private asset-only previews cover the
change, without entering the game. Confirm the appearance on the updated build.

At this 2026-10-26 checkpoint, the owner had reported a sleep/save crash with
native exit -1073741819 (0xC0000005). The owner later reported that sleep/save
is safe; broad lifecycle coverage remains unverified.

Graphics also offers VSync and FPS Limit. Up/down scrolls the nine-item list;
left/right changes the selected value. Native is a 30 FPS presentation cap.
Other rates come from the monitor's modes; fractional rates retain their exact
ratio in settings.json. VSync On is synchronized; unsupported Off is refused.
The selected cap does not change desktop refresh, and VSync can limit output
to the active display rate. Game hardware ticks remain fixed at 60 Hz and the
normal original loop still uses VSync(2). At this checkpoint, high caps
repeated native images and interpolation was not yet implemented. ADR-050
later added optional primitive interpolation for eligible farm views. Physical
VSync, FPS, and gameplay timing remain owner validation.

| Keyboard | Original pad |
| --- | --- |
| Arrows | D-pad |
| X / Z | Cross / Square |
| D / W | Circle / Triangle |
| A / S | L1 / R1 |
| Q / E | L2 / R2 |
| Enter / Backspace | Start / Select |

## Verified checkpoint

On 2026-10-26 the authored input file
port/tests/replays/new_game_control.replay completed 31000 VBlanks in Windows
release and optimized ASan/UBSan headless builds with the owner's validated
disc. It traversed the grandparent/flashback scenes, XA playback requests and
the morning house. After the intro script released control, right/down input
changed player coordinates from (-90, 0, 541) at tick 30000 to
(1058, 0, -1386) at tick 30600. Both runs exited zero. Their final BMP captures
have identical SHA-256:

808c2ce6346e216d6792297f6d5d919e49d4e60328065034e0abe7f66d4dd37b

This compares these two native runs, not an original-hardware golden frame.

The longer new_game_farm.replay completes 39000 VBlanks in release and optimized
ASan/UBSan, including house exit, declining Thomas's tour and Zack's shipping
dialogue. With scripting/paused state cleared, down input moves the player
from (-4257, 0, 3959) to (-4257, 0, 1663); right input then moves him to
(-1838, 0, 1663). The final scene is the farm, with its HUD showing 7:00 AM.
Both runs exit zero and their final BMP SHA-256 is:

dbe06cf4b46a8cce8e2814f7f03af3058e1f20cd4a84b5462b3ceb321905d044
Physical keyboard focus/input was not automated successfully; the replay feeds
the same native pad samples used by the window host. A separate Vulkan window
test completed 6000 VBlanks and rendered farm terrain, player and HUD. No
glitch-free listening, frame-time budget or full-game claim follows from these
checks. Audio underflows were observed in instrumented window tests.

For local reproduction, build hmbtn_boot_probe in the desired build directory,
set HMBTN_INPUT_REPLAY to the authored replay, HMBTN_BOOT_TICKS=31000,
HMBTN_TRACE_GAME_STATE=1 and HMBTN_CAPTURE_FRAME to an ignored output BMP, then
run the probe with the validated BIN path. Unit tests need no disc. Never add
the disc, screenshots or other extracted data to git.



### Runtime-fixes branch: sleeping and save payload

The reported sleep-without-save access violation maps to the animal record scan,
where native pointers were truncated to 32 bits. ADR-058 fixes that scan and its
native action record clearing. ADR-059 separately replaces PS1-offset state
packing with explicit fixed-width save spans. Original checksums, shifts, card
file size and Documents/VanillaBTN storage remain unchanged.

Isolated regressions reproduce both old faults and pass after the fixes. The
codec fixture writes/reopens/reads a synthetic in-memory card and restores named
persistent state. It never accesses the owner's saves. This is not a gameplay
pass: the owner still needs to try sleeping without saving, overnight events,
sleep-and-save, returning to title and loading the saved slot. Other runtime
faults may only become reachable after these blocking faults are removed.

2026-10-27 follow-up: the owner confirms saving succeeds, but reports a later
day-transition hang, ordinary dialogue crash and ratan2 abort entering Yodel
Ranch. The branch now passes the required player argument, progresses VBlank
inside the overnight audio wait, implements runtime math and rejects unsupported
PocketStation operations through proper asynchronous results. All 89 Release
and ASan/UBSan tests pass. Re-test those three gameplay triggers with the newly
built release. The exact day-transition hang cause remains unconfirmed without
a hang stack. See API_COVERAGE.md for remaining explicit font failures.

### Widescreen and camera follow-up (2026-10-27)

The owner confirms the Natsume logo looks correct but reports grey title sides,
incomplete load/save coverage and farm widescreen only while dialogue is shown.
ADR-062 fixes interpolation replay grouping and extends original background
assets through port adapters. Both Windows suites pass 90/90 tests. Recheck
moving farm frames with interpolation On, title colour, and load/save sides.

Camera displacement after sleeping twice is a separate open report. The ignored
window capture is build/port/camera-after-sleep.png. Set HMBTN_TRACE_CAMERA=1 when
launching the native executable directly to record player/centre/eye coordinates,
view/event state and flags every 120 hardware ticks on stderr. No positions are
changed. The diagnostic is disabled by default; never commit owner screenshots,
save data or captured gameplay logs. The current local diagnostic launch writes
build/port/camera-play.stderr.log. Do not claim the camera issue fixed yet.

### Pause, transitions and source audit (2026-10-27)

The owner supplied a pause view with smeared horizontal side strips, also seen
before/after cutscenes and area transitions. ADR-063 freezes genuine wide side
pixels for pause, centres the original menu and removes edge-column stretching
from all fallback presentation. Unsupported wide transition snapshots may use
black sidebars; this is not full transition reconstruction.

A later outside-blacksmith NPC crash maps to sprite_draw_frame+0x26 in timestamp
0x6adfcd97. ADR-064 repairs a confirmed PS1-sized villager animation copy that
corrupts native fields, plus other source-audit findings. The old copier fails
its synthetic fixture. Current Release and ASan/UBSan each pass 94/94 tests.
Try the same NPC interaction, repeated sleeps, pause and transitions again. No
interactive success is claimed until the owner verifies them. SOURCE_AUDIT.md
lists static coverage and remaining issues, including shop/status/transfer paths.

### Second audit batch (2026-10-27)

Shop request initialization no longer depends on neighboring C objects. Family
status text is loaded from the validated original archive and returned as native
pointers. Animal exchange decodes a native partner view and uses a five-record
native stash; its old buffer reproduces an ASan overflow. See ADR-065/066.

Windows Release and ASan/UBSan pass 99/99 tests. A read-only asset check resolves
all five original spouse names from the owner's validated BIN. These checks do
not open owner save files or run gameplay. Still owner-tested: shop dialogue
lifecycles, married status UI, interactive animal exchange and card failure/retry,
plus the earlier Saibara and repeated-sleep camera reports. No new gameplay launch
was performed during this batch.

### Third source-audit batch (2026-10-27)

ADR-067 repairs reachable null output accesses in dog-ball pickup and boolean
terrain probes used by animals/basket code. It also fixes full-width sequence
sentinel comparison, uninitialized query fields and native minigame frame/state
callback signatures. Missing texture-bank images now give explicit diagnostics
instead of using an uninitialized rectangle.

Release and ASan/UBSan pass 106/106 tests. Fixtures exercise the actual pickup,
basket and callback dispatch code with synthetic state, not interactive game
sessions. Owner checks still needed: dog retrieves ball, animal movement/basket
placement, sound, MG1/MG4, and earlier NPC/camera reports. No claim that all
analyzer warnings or full-game paths are resolved.

### Wide transition follow-up (2026-10-27)

ADR-068 handles rucksack TILE shade, observes direct frame-blend OTs and removes
native edge-column seeding from both private renderers. Direct/loading scanout
keeps the wide canvas with proportional central content. Release and ASan/UBSan
pass 106 tests; offscreen D3D12/Vulkan comparisons pass. No gameplay was run.

Owner checks: Widescreen ON from boot through Natsume-to-title fade, load/save,
rucksack entry/exit, area changes and cutscene entry/exit; check both scaling
settings and interpolation settings. The shade/fade should cover the sides,
logo/text should retain proportions, and the output should not snap to a 4:3
destination. Also check 21:9/monitor changes and OFF's original 4:3 presentation.
Report remaining exposed map edges or missing original background coverage;
synthetic tests do not establish an all-screen playthrough result.

### Start / farm summary hang (2026-10-27)

Windows records AppHangB1 for the owner session, not an access violation. ADR-069 yields
hardware progress in the status sprite loader and maps its shared sprite slots
explicitly; the original loader fixture exposes the old native out-of-array address and
passes after correction. Release/ASan pass 107 tests. Owner checks: Start from the farm,
summary opening, page arrows, animal/helper/ tool pages where available, closing, then
repeated entry. No interactive result is claimed from the isolated loader fixture.

ADR-070 extends the original white DRAWENV background across Natsume's extra
canvas. Logo proportions remain unchanged. A completed displayed capture is no
longer rejected just because cutscene/game state advances before the buffer flip;
interpolation still rejects cross-scene blending. Check Natsume and the reported
brief Won cutscene flash again; the latter has no confirmed interactive result.
White/black offscreen GPU readbacks pass on Vulkan and D3D12.

### Reopened fade and post-save camera reports (2026-10-27)

The owner's initial 1-3 stability checks were overall safe, but subsequent tests
report Natsume fade flicker, side flashes entering/leaving TV, and a post-save
camera shift persisting outside cutscenes and across areas. These reports remain
open until revalidated. ADR-071 fixes independently reproduced backdrop-lifetime,
clear-only capture and saved/menu-return projection defects. All 108 Release/ASan
tests and Vulkan/D3D12 probes pass. No gameplay/save fixture accesses owner data.

Owner checks: boot through full Natsume fade-in/out; view TV programme information,
enter the show and exit to the room; save, then move around and change areas;
repeat sleep/save and test Widescreen OFF/ON. Confirm that the character remains
correctly framed after cutscenes, and that both sides follow the same fade.

Owner follow-up on aeb5563: fade-in Natsume and save/next-day camera are safe in
the tested session. Fade-out Natsume, TV start/end and home exit still flicker
at the sides. ADR-072 removes a verified unchanged-buffer timeout during blocking
waits and uses native write invalidation instead. 109 Release/ASan tests and
Vulkan/D3D12 probes pass. Recheck those specific transitions after updating the
executable; do not treat fixture success as an interactive resolution.

### Repeated load / farm overlay re-entry (2026-10-27)

Owner reports Natsume safe on 9c57d5a. Load later aborts with native overlay reentry
is not implemented; WER records the runtime abort. ADR-073 adds the missing farm
snapshot inventory instead of bypassing the error. 110 Release/ASan tests pass,
including a title/farm/load loop that fails on the old registry and a fixture
mutating every registered mutable object. No real save file is used in the tests.
Owner checks: load into farm, return to title, load the same/another slot again,
then repeat and verify player/day/animals and menus. Interactive success remains
pending. TV/home-exit flicker is not established resolved by this unrelated fix.

### Held-animal layout correction (2026-10-28, pending owner gameplay)

Branch fix is based on current main; multilingual work is parked separately.
The owner reproduced a dog put-down crash under LLDB. ADR-075 fixes the pickup
write that corrupted the native animation pointer, and the audited helper-number
read. Interactive follow-up: pick up and put down the young/adult dog repeatedly,
then try a chicken and talk to the Harvest Sprites. Confirm sprites, affection
cue, movement and dialogue remain correct. Isolated regression tests do not
establish complete gameplay coverage.
