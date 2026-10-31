#include "enhance/pause_menu/assets.h"
#include "enhance/pause_menu/menu.h"
#include <stdio.h>
#include <string.h>
static u8 canvas[320 * 240 * 4];
static u8 blessing_reference[sizeof(canvas)];
static hm_pause_assets assets;
int main(void) {
    hm_pause_menu menu;
    hm_pause_open(&menu);
    memset(canvas, 0x11, sizeof(canvas));
    /* Synthetic glyph atlas: this test contains no original game pixels. */
    for (size_t i = 0; i < sizeof(assets.font) / sizeof(assets.font[0]); ++i)
        assets.font[i] = 31;
    hm_pause_draw(&assets, canvas, &menu, NULL);
    const u8 *gradient = canvas + (56 * 320 + 70) * 4;
    const u8 *glyph = canvas + (66 * 320 + 136) * 4;
    /* prim_make_gouraud_rect consumes 0xRRGGBB, not the reversed order
     * suggested by an old comment in window_frame.c. Keep that source intact. */
    if (canvas[0] != 10 || canvas[1] != 10 || canvas[2] != 10 || canvas[3] != 0x11 ||
        gradient[0] != 0xF4 || gradient[1] != 0xDF || gradient[2] != 0xBD || gradient[3] != 255 ||
        glyph[0] != 255 || glyph[1] != 0) {
        fprintf(stderr, "Original dialog geometry or cached glyph composition differs\n");
        return 1;
    }
    memset(canvas, 0x11, sizeof(canvas));
    menu.page = HM_PAUSE_GRAPHICS;
    menu.graphics_modern = 1;
    menu.selected = 4;
    const hm_pause_view view = {
        .values = {"Modern", "1", "Windowed", "1920 x 1080", "Off", "Off", "Off", "Nearest", "On", "59.94"},
        .detail = "Off - not available yet",
        .notice = ""};
    hm_pause_draw(&assets, canvas, &menu, &view);
    const u8 *label = canvas + (49 * 320 + 40) * 4;
    const u8 *row_value = canvas + (49 * 320 + 192) * 4;
    const u8 *gap = canvas + (38 * 320 + 16) * 4;
    const u8 *detail = canvas + (205 * 320 + 24) * 4;
    if (label[0] != 255 || row_value[0] != 255 || gap[0] != 10 || detail[0] != 255) {
        fprintf(stderr, "Graphics columns, separated header or detail panel overlap\n");
        return 1;
    }
    menu.selected = 9;
    menu.first_visible = 3;
    hm_pause_draw(&assets, canvas, &menu, &view);
    const u8 *fps_label = canvas + (169 * 320 + 40) * 4;
    const u8 *fps_value = canvas + (169 * 320 + 192) * 4;
    const u8 *decimal = canvas + (169 * 320 + 208) * 4;
    if (fps_label[0] != 255 || fps_value[0] != 255 || decimal[0] != 255 || decimal[1] != 0) {
        fprintf(stderr, "Scrolled FPS option is outside the visible menu\n");
        return 1;
    }
    menu.page = HM_PAUSE_BLESSINGS;
    menu.selected = 1;
    menu.first_visible = 0;
    hm_pause_view blessings_with_values = {
        .values = {"Use", "Use", "Use", "Use", "Use"}, .detail = "", .notice = ""};
    hm_pause_view blessings_without_values = {.detail = "", .notice = ""};
    memset(canvas, 0x11, sizeof(canvas));
    hm_pause_draw(&assets, canvas, &menu, &blessings_with_values);
    memcpy(blessing_reference, canvas, sizeof(canvas));
    memset(canvas, 0x11, sizeof(canvas));
    hm_pause_draw(&assets, canvas, &menu, &blessings_without_values);
    if (memcmp(blessing_reference, canvas, sizeof(canvas))) {
        fprintf(stderr, "Blessing actions overlap a settings value column\n");
        return 1;
    }
    return 0;
}
