# Native pause interface

Current graphics paths and their limits are summarized in
docs/port/ENHANCEMENTS.md. The implementation notes below are not a complete
feature inventory.

Initial implementation on port/windows-pause-menu, based on merged main.
HMBTN_ENH_PAUSE_MENU defaults OFF in CMake. Windows build.ps1 -Game enables it
for the owner's explicitly requested ESC interface. Set enhance.pause_menu to
false in settings.json to disable the runtime interface; existing settings
without that key use the requested available-by-default behavior.

The platform pauses at its VBlank host boundary without advancing game/CD/SPU
ticks. The private UI loop still pumps window events and presents frames.
Audio device playback pauses and the pacing epoch shifts by the pause duration.
Held menu-confirm input is suppressed until release on returning to gameplay.

The font is read from the validated owner's disc, using the game's original
resident UI texture-bank decoder, original English glyph layout and text CLUT.
No game pixels are stored in the repository. A private cache survives overlays.
The dialog frame executes src/main/ui/window_frame.c with private pixel/line/
gradient sinks. No game frame buffers, ordering tables or dialog state are used
while the pause interface is open. No replacement bitmap/font is authored.
Frame color arguments follow prim_make_pixel/line/gouraud_rect's 0xRRGGBB
contract. An inaccurate older comment in window_frame.c must not be used to
reverse the red and blue channels. The private canvas keeps the original cream
palette, with compact centered root/settings panels and a separate Graphics
header, aligned label/value columns, selected-row frame and detail footer.

Windows pause controls: D-pad, arrow keys, or WASD navigate; left/right change
values; Enter/K/X selects; and Esc/J backs out (Esc/J resumes from the root).
Held D-pad and left-stick directions repeat after a short delay. Mouse hover,
click, and wheel navigate the pause interface only; mouse movement and buttons
do not move or act in the game.

Android pause controls: tap a row to select it and tap its value to change it.
Use Circle or the Android Back action to return, and use the virtual or physical
gamepad's D-pad/Cross/Circle to navigate. Held virtual D-pad directions repeat
after a short delay. Configured virtual-gamepad mappings remain active while the
game tick is paused. Exit Game closes the application. The requested tree is
Resume, Settings > Graphics, Exit Game. Root and settings have no extra entries.

The frozen gameplay background receives a 40% black overlay before drawing the
original dialog and text at full brightness. Each pause presentation starts
from the unchanged snapshot, so darkness does not accumulate. Resume reads
the native scene again; this UI effect never modifies game VRAM.

Implemented platform controls: monitor selection; Windowed, exclusive Fullscreen
and Borderless Fullscreen; resolutions enumerated from SDL's driver display
modes; nearest or bilinear final-frame presentation. Borderless follows the
desktop resolution. These display operations require the owner's hardware test.
Widescreen OFF preserves 4:3 with black bars. Optional texture Bilinear now
works in GPU and Software output (ADR-055), alongside final presentation filtering.

VSync offers synchronized On (mailbox/FIFO) or supported immediate Off. FPS
Limit offers Native (30), Off (Unlimited), and monitor refresh-derived rates, preserving rational
values such as 60000/1001 (shown as 59.94). Windowed/borderless choices use the
desktop resolution; exclusive choices use the selected resolution. VSync can
reduce achieved presentation to the active OS refresh. A cap does not switch
desktop refresh. The graphics list scrolls through its visible rows.

Presentation deadlines are independent of the fixed 60 Hz hardware ticks.
Original normal game updates remain 30 Hz through VSync(2). Higher caps repeat
native pictures unless optional Interpolation is enabled, without advancing
game/RNG/audio faster. See ../interpolation/README.md for scope and limits. GPU/OS stalls and achieved FPS remain owner hardware tests.

Enhance Scaling can rerasterize triangle geometry at the viewport resolution
through GPU Auto or the software reference; enhance.scaling defaults false.
See ../scaling/README.md for coverage, limits and measured costs. Widescreen
Mode now covers all screens while keeping UI centered and native
sized (ADR-057); enhance.widescreen defaults false. See ../widescreen/README.md
for scope and owner validation. FXAA (final image) and MSAA 2x/4x (GPU scene)
were added in ADR-076 and default OFF. MSAA requires GPU Auto; the Software
reference supports Off/FXAA. Original texture assets and native simulation are
retained.

The graphics object in Documents/VanillaBTN/settings.json stores monitor
ordinal, window_mode (0 windowed, 1 exclusive, 2 borderless), width, height and
bilinear (0 or 1), vsync (0 or 1), fps_numerator and fps_denominator. Zero FPS
numerator selects Native; -1 selects Off (Unlimited), with denominator 1. Defaults are VSync On and Native (30). Unknown root properties are preserved when replacing the
owned graphics object. Malformed JSON is not overwritten. Writes use the
existing temporary-file replacement and instance lock. Missing or invalid
graphics fields fall back to windowed 640x480, nearest filtering, monitor 0.

Tests use synthetic glyph data for menu geometry/navigation, JSON validation and
unknown-property preservation. A separate local asset probe decodes the real
font and checks VRAM is reset, without opening a window or starting the game.
Presentation-clock tests verify exact rational boundaries and unchanged native
VBlank callback counts across caps; these do not run the actual game clock.
Physical ESC behavior, audio pause/resume and display switching remain owner
gameplay tests. No interactive session or replay is run by the agent.

Off (Unlimited) removes the software presentation cap. VSync On still follows
the active monitor refresh when available; choose VSync Off for uncapped
submission if the driver supports immediate presentation. Simulation remains
native regardless of presentation attempts. See ADR-051.

Renderer selects GPU Auto (graphics.renderer=0, default) or Software (1).
GPU Auto uses shared scene output at native and enhanced resolution, exposing
active GPU versus CPU fallback in its detail row. Unsupported shaders/commands
retain software output. GPU native integer readback checks pass, but actual
gameplay remains owner-tested. See ADR-054 and gfx/README.md for coverage limits.

The shipping-bin view uses one original-style window with no rucksack panel.
It shows up to eight shipped entries per page in the native four-column,
two-row icon grid, keeping all eight framed slots visible when a page is empty.
The centered item name and quantity sit below the grid; a compact page count and
touchable page arrows keep pagination inside the window. D-pad and touch can
select entries. Cross and Circle appear as button marks in the footer. Items without vanilla shipment-count slots, such as
bamboo shoots, are tracked by exact variant during the current session.
Portable-basket additions enter that list when the basket is shipped.
