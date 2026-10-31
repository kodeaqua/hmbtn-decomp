# Enhancements

## Current status (2026-10-31)

Graphics Mode offers Native PS1, Enhance PS1, and Modern. Modern effect
selections are retained when another mode is active. Android has the same
three-profile selector, keeps interpolation off, and bypasses the desktop PGXP
compute path for mobile performance (ADR-123/124/131/132).

| Area | Current state |
| --- | --- |
| Pause interface | ESC pause, Resume, Settings, Goddess Blessings, and Exit Game are available by default per owner request; set enhance.pause_menu=false to turn it off. |
| Renderer | GPU Auto attempts shared scene compute for every captured frame on desktop and Android. It handles all polygon, line, and rectangle opcode variants, clear/load/copy transfers, mask behavior, and ordered texture feedback. Missing or invalid scene captures try direct GPU VRAM scanout; CPU presentation remains for device/resource failures, explicit Software mode, and diagnostic capture. Windows Direct3D 12 completed a 39,000-VBlank farm replay with 48,599/48,599 renderable frames on GPU, zero fallbacks, eight direct scanouts, and one empty-area skip. The connected G100 previously reported 600/600 GPU farm frames and, after materialization optimization, 60-61 presents/s with zero audio underruns at 384x240. Broader device/scene coverage and scaled gameplay performance remain unverified. |
| Scaling and widescreen | Implemented as optional paths; both default OFF. Widescreen covers the implemented screens, but complete scene/device coverage and CPU fallback performance are unverified. |
| Filtering and anti-aliasing | Nearest, Bilinear, xBR, FXAA, and GPU MSAA 2x/4x are implemented. FXAA/MSAA default OFF; MSAA requires GPU Auto. |
| Interpolation | Optional and default OFF. Private primitive replay is limited to eligible farm views; Android clamps it off. It does not advance simulation. |
| Dithering and color | Native PS1 retains original dithering. Enhance PS1/Modern can bypass it; 8-bit shading is preserved only for selected opaque GPU paths. Other paths and Software output retain BGR555 precision. |
| Controls | Gamepad, keyboard presets, and per-key keyboard remapping are available. Android adds a configurable virtual-pad action map, show/hide toggle, button-size setting, and drag-to-position layout editor. Mouse settings remain in the controls model, but in-game mouse/touch actions are disabled by ADR-136. |
| Gameplay options | Optional gameplay enhancements default OFF, except wild-dog attacks stay ON to preserve vanilla behavior. Optional features include NPC Relationships (ADR-144), mature planted fodder-grass spread, half-speed outdoor farm time, and the reversible shipping-bin list. Goddess Blessings add gold, restore stamina/fatigue, and provide small amounts of fodder and animal feed. Fast Text remains removed pending a better multi-line pagination design (ADR-133). |
| Android | Native PS1, Enhance PS1, and Modern profiles render internally at native PS1 scene resolution (ADR-164); Modern retains 16:9 and bilinear presentation, and Enhance retains PGXP. Android GPU scenes use nearest texture sampling in Bilinear mode to match the mobile reference path; final presentation remains bilinear (ADR-165). The connected G100's prior 384x240 farm sample held 60-61 presents/s; current scaling/filter changes have not yet been measured on-device. See GPU_TESTING.md. |

Graphics and gameplay settings are stored under Documents/VanillaBTN/settings.json
on Windows. The ESC interface is the explicit runtime-default exception to the
general enhancement-OFF rule. Automated probes and the recorded Windows/G100
runtime samples establish coverage only for their tested scenes and devices;
they do not establish complete gameplay parity, all display modes, or broad
performance. See GPU_TESTING.md and PLATFORMS.md.
The 2026-10-31 gameplay/control batch is recorded in ADR-158 through ADR-165.
The shipping-bin list tracks exact item variants in process memory for the
current play session; it does not extend the original save format, so its exact
variant list is not restored after restarting or loading a save.

## Dated implementation history

Entries below record the state at the time of each change. The current summary
above and later ADRs supersede older feature descriptions and validation status.

The owner requested the native pause interface and graphics settings on
2026-10-26. Implementation starts on port/windows-pause-menu, after PR #1
(windows port init) was merged and all old branches were removed.

| Feature | Initial implementation state (2026-10-26) |
| --- | --- |
| ESC pause, Resume, Settings > Graphics, Exit Game | Host interface implemented; owner interactive test pending |
| Original dialog presentation/font | Original window-frame C and runtime disc glyph cache; synthetic composition and local asset decoding checks pass |
| Monitor/window mode/resolution | SDL display controls implemented; owner monitor/fullscreen test pending |
| Filtering | Texture filtering: Nearest Neighbor, Bilinear, and xBR sharp edge-directed curves (ADR-085) |
| VSync | On (mailbox/FIFO) or supported immediate Off; device validation pending |
| FPS limit | Native (30), Off (Unlimited), or monitor-derived rational caps; fixed 60 Hz game ticks; native images unless optional interpolation is enabled |
| Visual interpolation | Optional private primitive replay for normal farm views; synthetic checks pass, owner gameplay validation pending |
| Enhance Scaling | Optional viewport-resolution output through GPU Auto or Software; default OFF; gameplay checks pending |
| Widescreen Mode | All-screen wide canvas, original title background tiles, proportional UI and stable fallback (ADR-057); owner validation pending |
| FXAA/MSAA | FXAA (final-image) and MSAA 2x/4x (GPU scene) implemented (ADR-072); runtime default Off; owner gameplay validation pending |
| Gameplay Enhancements | Suite of 9 toggleable options in Settings > Gameplay (ADR-080); runtime default OFF except wild dog; owner gameplay validation pending |
| Controls Settings | Gamepad, keyboard presets, and mouse interaction in Settings > Controls (ADR-081); persistent in settings.json |
| PGXP Geometry | Subpixel vertex precision and perspective-correct texture mapping (ADR-083); reduces wobble and texture warping where supported; default OFF |
| Dithering | PS1 hardware 4x4 Bayer ordered dithering toggle (ADR-084); original PS1 dither pattern or bypass; native mode retains dithering |

HMBTN_ENH_PAUSE_MENU defaults OFF in CMake; Windows build.ps1 -Game enables it.
The owner explicitly requests ESC availability across scenes, so that interface
defaults available at runtime; enhance.pause_menu=false disables it. This is
recorded as a narrow owner override in AGENTS.md. Rendering enhancements retain
their OFF runtime defaults; implemented paths and current coverage limits are
summarized at the top of this document.

Graphics properties are saved in Documents/VanillaBTN/settings.json. Unknown
root properties are preserved; malformed input is not overwritten. No game
state, RNG, CD progress, GPU VRAM or SPU synthesis advances inside the pause UI
loop. Paused wall time is excluded from the pacing epoch. No game art, fonts,
SDK material or BIOS is included in the repository. See ADR-048 and
port/enhance/pause_menu/README.md for controls, schema and current limits.
VSync/FPS controls follow ADR-049. Presentation counts do not advance game time.
Achieved FPS, physical VSync and gameplay time remain owner tests.

The owner subsequently requested visual interpolation (ADR-050). Windows -Game
builds HMBTN_ENH_INTERPOLATION; runtime enhance.interpolation defaults false.
Enable Interpolation and choose a higher FPS Limit in Graphics. Positions are
interpolated through private GPU command replay, without extra game updates or
image crossfading. Initial scope is normal farm views; unmatched primitives,
loading, transitions and other contexts retain native cadence. See
port/enhance/interpolation/README.md. Optional Widescreen is available. FXAA
and MSAA were implemented later in ADR-076; gameplay/device validation remains
owner-tested.

FPS Limit Off removes the software presentation cap (ADR-051). VSync On still
synchronizes to the active monitor; Native remains the default. A synthetic
regression verifies 600000 visual attempts do not add hardware/game ticks.
The pause backdrop also receives a 40% black overlay; original dialog/text
remain fully bright, and native VRAM is unchanged.

Enhance Scaling now has a private viewport-resolution raster path (ADR-052).
Windows -Game compiles HMBTN_ENH_SCALING; enhance.scaling defaults false. The
initial implementation rerasterizes triangles/quads at viewport size, preserving
original texture assets and 4:3 bars. Output supports up to 4096 per axis with software fallback. High FPS in actual gameplay is not established. See
port/enhance/scaling/README.md and the synthetic measurements in PLATFORMS.md.

Interpolation now uses stable field-cell identities and a shared-vertex mesh
coherence guard (ADR-053). The owner confirmed the reported ground cuts disappear
with Interpolation OFF. Synthetic reproductions for mismatched neighboring
history and reordered field packets pass after the fix; owner farm confirmation
is still pending. History changes may reduce interpolation cadence locally.

Renderer > GPU Auto is now shared by native and enhanced output (ADR-054),
independent of enhance.scaling. Software remains selectable. Native GPU pixel
comparisons pass on Vulkan/D3D12; unsupported transfers/feedback and direct
loading retain CPU fallback. The normal low-resolution VRAM compatibility path
and pause canvas still use CPU. No complete GPU-offload or playthrough claim.

Texture Bilinear now works in GPU Auto and Software, at native or enhanced
resolution. HMBTN_ENH_FILTERING gates compilation; enhance.filtering defaults
false, with migration of an existing explicit graphics.bilinear setting.
Nearest preserves the reference path. Transparent taps preserve the background;
UV bounds prevent neighboring atlas samples. See enhance/filtering/README.md
and ADR-055 for mask policy and limitations. Actual gameplay remains unverified.

Widescreen (ADR-056) now expands horizontal world coverage on the farm normal
view, including its UI overlays. HMBTN_ENH_WIDESCREEN gates compilation;
enhance.widescreen defaults false. Windows -Game includes it. OFF and non-world
scenes retain 4:3 bars. GPU Auto/Software, native/enhanced output and palette
filtering share the path. Original UI size, GTE projection and game clocks stay
native. See port/enhance/widescreen/README.md for current limits and owner checks.

ADR-057 supersedes the world-only restriction above: Widescreen now applies to
logo/title/load/save/minigames/pause as well as world rendering, with native
logos/text and background extension from original assets. GPU/native fallback
keeps the presentation aspect. Auto retries at native internal resolution to
bound stalls; explicit Software keeps full-resolution CPU output. Actual
in-game dialogue performance and all-screen visuals remain owner-tested.

ADR-068 fixes full-width rucksack TILE shade and captures direct frame-blend
commands over the retained extended scene. Native/direct/loading output uses a
centred scanout on the same expanded canvas when replay is unavailable; it does
not change back to a 4:3 destination. Uncovered pixels are black. Both renderers
stop seeding side pixels from changing native edge columns during logo fades.
This supersedes the earlier edge-extension description. Original artwork and
text remain proportional. Interactive all-screen coverage is still owner-tested.

ADR-070 supersedes the unconditional black backdrop: Natsume's original enabled
DRAWENV white clear now covers the expanded area too; disabled clearing uses
black. Shared frame replay keeps the capture for the actual displayed buffer
when simulation state advances before a flip. Interpolation still rejects
cross-scene blending. Owner verification of the reported Won flash is pending.

ADR-071 keeps GPU backdrop colour in the captured scene through finalization
and accepts completed clear-only OTs. Original fade packets are tested across
both framebuffers and changing backdrops. A separate native SDK adapter restores
the farm projection centre after save/menu returns, independently of enhancement
settings. Natsume/TV side flashes and persistent post-save framing remain pending
owner revalidation despite passing isolated regressions.

ADR-081 adds a dedicated Settings > Controls menu with Gamepad, Keyboard layout presets (Arrows+ZX / WASD+JK / Off), and Mouse modes (Off / Action / Move & Act with click-to-walk). Input is mapped cleanly to PS1 pad bitmasks and persisted under controls in settings.json. Unit tests cover configuration and pause menu transitions.

The owner requested PGXP subpixel vertex precision and perspective-correct texture mapping (ADR-083).
Windows -Game builds HMBTN_ENH_PGXP; runtime enhance.pgxp defaults false (vanilla parity).
When enabled, GTE projection captures high-precision 32-bit floating-point screen coordinates and
view-space depth W without modifying the integer coordinates used by original game logic or collision.
The GPU scene backend retrieves subpixel coordinates and depth values, rendering geometry with
perspective-divided texture coordinates ($u/w, v/w, 1/w$), reducing vertex snapping and affine texture distortion where supported. Graphics Mode controls this via Enhance PS1 and Modern.

The owner requested an option to toggle PS1 hardware dithering (ADR-084).
Windows -Game builds HMBTN_ENH_DITHERING; runtime enhance.disable_dithering defaults false (vanilla parity).
PS1 hardware uses 16-bit color depth (R5G5B5) and applies an ordered 4x4 Bayer dithering matrix to reduce color banding.
On modern high-resolution displays, this ordered pattern manifests as checkerboard noise.
Graphics Mode bypasses dithering in Enhance PS1 and Modern. Intermediate GPU colors remain BGR555.

The owner requested edge-directed sharp texture filtering without blur (ADR-085).
Filtering now supports three selectable modes in Pause Menu > Settings > Graphics:
- **Nearest Neighbor**: Authentic raw PS1 pixels.
- **Bilinear**: Soft/smooth linear interpolation.
- **xBR**: Edge-directed smart scaling that rounds pixel art corners into smooth diagonal contours while preserving 100% crispness without blur.

Additionally, FXAA is now bypassed during Pause Menu presentation to keep UI text, dialogue, and font pixel art perfectly crisp.
Graphics Mode supersedes the earlier individual PGXP and dithering controls (ADR-124).

## Recent Enhancements & Fixes (2026-10-29 to 2026-10-30)

### Fast Text Dialogue Flow (ADR-106 & ADR-109; removed by ADR-133)

This records the earlier implementation. Fast Text is currently removed from
the menu and disabled while multi-line pagination behavior is reconsidered.
Fast Text provides instant dialogue text typing while strictly preserving game control codes and timing events:
- At TEXT_CODE_PAGE_MARK and TEXT_CODE_PAGE_END, Fast Text immediately pauses text advancement and locks 	ext_phase = TEXT_STATE_PAGE_BREAK.
- Prevents subsequent glyph evaluations from overwriting the phase back to TEXT_STATE_PRINTING.
- Displays the red blinking advance prompt at page breaks, ensuring player Confirm (X / Circle) presses cleanly step pages or open choices without dialogue freezing.
- When Fast Text is disabled (0), original vanilla PS1 typing speed and behavior execute identically.

### Rucksack Tool/Item Category Protection (ADR-107)
Enforces authentic inventory categorization in the rucksack reorganize screen:
- Disallows cross-swapping between Tool slots (left column) and Item slots (right column) under mouse clicks and controller navigation.
- Disallows splitting stacked items into empty tool slots.
- Restricts mouse hover cursor tracking to the marked category until a swap is confirmed or cancelled.

### Android Platform Optimization & 720p Resolution Cap (ADR-105)
- Historical: Android's internal 720p scaling cap was removed from the Enhanced profiles by ADR-164 after device lag reports. Current Android profiles stay at native internal resolution.
- Android builds feature touch controls, Storage Access Framework (SAF) disc selection, and automatic audio device management.

### Tile Targeting visibility and ground row seams (ADR-111)

Tile Targeting now rejects event/script control and paused, menu, transition,
riding, airborne and sleeping states. Its marker is ordered in front of the
terrain rather than hidden behind it. With interpolation enabled, adjoining
ground rectangles now share the existing polygon coherence/fallback rule,
preventing mixed history from opening horizontal gaps in enlarged output.
Defaults and the vanilla OFF paths are unchanged. Synthetic regression checks
do not replace the owner's gameplay validation of the reported screen lines.

ADR-112 corrects an additional missing-indicator cause observed in the owner's
running game: widescreen uses a private primitive arena, but Tile Targeting and
the tool charge gauge checked capacity against the original backup buffer.
Both now validate the active arena and reject draws only when its remaining
space is insufficient. The owner's horizontal-line report while running is
deferred; no further fix for that report is claimed here.

The audio resampler defaults to PS1 Gaussian. Studio HQ remains an explicit
opt-in through audio settings; existing saved choices are preserved (ADR-110).


ADR-130 adds a fatigue bar to the optional Stamina HUD. Its fill shows the
original fatigue value from 0 to 100, and it shares the existing off-by-default
Stamina HUD setting. Gameplay and display validation remain owner checks.

### NPC Relationships Page (ADR-144)

Settings > Gameplay can enable a ninth status page listing all 32 named adult
villagers. Each row uses a small pixel head icon cropped from that NPC's original
portrait bank in the user's `face.bin`, the NPC name in title case, and a bar
sourced from the game's original friendship value (0-255) or bachelorette
affection value (0-65535). Friendship is green and affection is pink. Portraits
are decoded at runtime; the page heading and bar labels use title case, and no
game image data is shipped. The setting
`enhance.npc_bonds` defaults to `false`; disabled builds retain the original
eight-page order and wrap behavior. The Windows and Android `-Game` builds
compile the option. The owner still needs to check the page in gameplay.
