#include "enhance/pause_menu/assets.h"
#include "enhance/pause_menu/menu.h"
#include <stdio.h>
#include <string.h>

/* Execute the game's actual frame geometry with private canvas sinks. This
 * preserves its bevel, corners and gradient; no replacement UI art is authored. */
#define window_frame_draw hm_pause_original_frame
#define prim_add_pixel hm_pause_pixel
#define prim_add_line hm_pause_line
#define prim_add_gouraud_rect hm_pause_gradient
#include "../../../src/main/ui/window_frame.c"
#undef window_frame_draw
#undef prim_add_pixel
#undef prim_add_line
#undef prim_add_gouraud_rect

static u8 *hm_canvas;
static void hm_put(int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= 320 || y >= 240)
        return;
    u8 *p = hm_canvas + ((size_t)y * 320 + (size_t)x) * 4;
    /* Original prim_make_pixel/line/gouraud_rect consume 0xRRGGBB. */
    p[0] = (u8)(color >> 16);
    p[1] = (u8)(color >> 8);
    p[2] = (u8)color;
    p[3] = 255;
}
void hm_pause_pixel(u_long *ot, s16 x, s16 y, s32 color, s16 semi) {
    (void)ot;
    (void)semi;
    hm_put(x, y, (u32)color);
}
void hm_pause_line(u_long *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color, s16 semi) {
    (void)ot;
    (void)semi;
    /* The original window frame uses only horizontal/vertical edges. */
    if (x0 == x1)
        for (int y = y0; y <= y1; ++y)
            hm_put(x0, y, (u32)color);
    else if (y0 == y1)
        for (int x = x0; x <= x1; ++x)
            hm_put(x, y0, (u32)color);
}
void hm_pause_gradient(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2, s32 c3,
                       s16 semi) {
    (void)ot;
    (void)c1;
    (void)c3;
    (void)semi;
    for (int row = 0; row < h; ++row) {
        u32 color = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            int top = ((u32)c0 >> (channel * 8)) & 255;
            int bottom = ((u32)c2 >> (channel * 8)) & 255;
            color |= (u32)(top + (bottom - top) * row / (h > 1 ? h - 1 : 1)) << (channel * 8);
        }
        for (int col = 0; col < w; ++col)
            hm_put(x + col, y + row, color);
    }
}
static void hm_text_font(const u16 *font, const char *text, int x, int y) {
    if (!text || !font)
        return;
    for (; *text && x < 304; ++text, x += 8) {
        unsigned char c = (unsigned char)*text;
        if (c == '%') {
            static const u8 pct_map[12] = {
                0x00, 0x00, 0x62, 0x64, 0x08, 0x10, 0x20, 0x46, 0x86, 0x00, 0x00, 0x00
            };
            for (int r = 0; r < 12; ++r) {
                u8 row_bits = pct_map[r];
                for (int col = 0; col < 8; ++col) {
                    if (row_bits & (0x80 >> col)) {
                        hm_put(x + col + 1, y + r + 1, 0x00101010);
                        hm_put(x + col, y + r, 0x00EFEFEF);
                    }
                }
            }
            continue;
        }
        unsigned glyph;
        if (c >= '0' && c <= '9')
            glyph = 0x40 + c - '0';
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            glyph = c - 'A';
        else if (c == '-')
            glyph = 0x69;
        else if (c == '(')
            glyph = 0x6A;
        else if (c == ')')
            glyph = 0x6B;
        else if (c == '.')
            glyph = 0x6C;
        else
            continue;
        for (unsigned row = 0; row < 12; ++row)
            for (unsigned col = 0; col < 8; ++col) {
                u16 color = font[((glyph / 16) * 12 + row) * HM_PAUSE_FONT_W +
                                 (glyph % 16) * 8 + col];
                if (!color)
                    continue;
                u32 rgb = 0;
                for (unsigned k = 0; k < 3; ++k) {
                    unsigned value = (color >> (k * 5)) & 31;
                    rgb |= ((value << 3) | (value >> 2)) << ((2 - k) * 8);
                }
                hm_put(x + (int)col, y + (int)row, rgb);
            }
    }
}
static void hm_text(const hm_pause_assets *assets, const char *text, int x, int y) {
    hm_text_font(assets ? assets->font : NULL, text, x, y);
}
static void hm_text_limited(const hm_pause_assets *assets, const char *text, int x, int y,
                            size_t max_chars) {
    if (!text)
        return;
    char clipped[40];
    size_t length = strlen(text);
    if (length > max_chars)
        length = max_chars;
    if (length >= sizeof(clipped))
        length = sizeof(clipped) - 1;
    memcpy(clipped, text, length);
    clipped[length] = '\0';
    hm_text(assets, clipped, x, y);
}
static void hm_center_font(const u16 *font, const char *text, int y) {
    if (!text || !font)
        return;
    size_t length = strlen(text);
    if (length > 36)
        length = 36;
    hm_text_font(font, text, 160 - (int)length * 4, y);
}
static void hm_center(const hm_pause_assets *assets, const char *text, int y) {
    hm_center_font(assets ? assets->font : NULL, text, y);
}
static void hm_item_icon(const u32 *pixels, int x, int y) {
    if (!pixels)
        return;
    for (int py = 0; py < HM_PAUSE_ITEM_ICON_SIZE; ++py) {
        for (int px = 0; px < HM_PAUSE_ITEM_ICON_SIZE; ++px) {
            u32 color = pixels[py * HM_PAUSE_ITEM_ICON_SIZE + px];
            if ((color >> 24) == 0)
                continue;
            u8 *dst = hm_canvas + ((size_t)(y + py) * 320 + (size_t)(x + px)) * 4;
            dst[0] = (u8)(color >> 16);
            dst[1] = (u8)(color >> 8);
            dst[2] = (u8)color;
            dst[3] = 255;
        }
    }
}
static void hm_pause_button_icon(int x, int y, int cross) {
    const u32 shadow = 0x00382A22;
    const u32 outline = 0x00F0D7A0;
    const u32 fill = cross ? 0x005B94D0 : 0x00D45D69;
    if (cross) {
        for (int i = 1; i < 10; ++i) {
            for (int thickness = -1; thickness <= 1; ++thickness) {
                hm_put(x + i + thickness + 1, y + i + 1, shadow);
                hm_put(x + 10 - i + thickness + 1, y + i + 1, shadow);
                hm_put(x + i + thickness, y + i, outline);
                hm_put(x + 10 - i + thickness, y + i, outline);
            }
            hm_put(x + i, y + i, fill);
            hm_put(x + 10 - i, y + i, fill);
        }
    } else {
        for (int dy = -5; dy <= 5; ++dy) {
            for (int dx = -5; dx <= 5; ++dx) {
                int radius = dx * dx + dy * dy;
                if (radius >= 13 && radius <= 34) {
                    hm_put(x + 5 + dx + 1, y + 5 + dy + 1, shadow);
                    hm_put(x + 5 + dx, y + 5 + dy, outline);
                    if (radius >= 18 && radius <= 26)
                        hm_put(x + 5 + dx, y + 5 + dy, fill);
                }
            }
        }
    }
}
static void hm_pause_page_arrow(int x, int y, int direction) {
    const u32 shadow = 0x00382A22;
    const u32 outline = 0x00F0D7A0;
    const u32 fill = 0x00D49B46;
    for (int row = -4; row <= 4; ++row) {
        int distance = row < 0 ? -row : row;
        int start = direction < 0 ? x + distance : x;
        int end = direction < 0 ? x + 4 : x + 4 - distance;
        for (int px = start; px <= end; ++px) {
            int edge = row == -4 || row == 4 || px == start || px == end;
            hm_put(px + 1, y + row + 1, shadow);
            hm_put(px, y + row, edge ? outline : fill);
        }
    }
}
void hm_pause_draw(const hm_pause_assets *assets, u8 *rgba, const hm_pause_menu *menu,
                   const hm_pause_view *view) {
    /* The host supplies a fresh frozen background each presentation. Apply a
     * 40% black scrim before drawing the original, fully bright dialog art. */
    for (size_t pixel = 0; pixel < 320 * 240; ++pixel)
        for (size_t channel = 0; channel < 3; ++channel)
            rgba[pixel * 4 + channel] = (u8)((u32)rgba[pixel * 4 + channel] * 3 / 5);
    hm_canvas = rgba;
    if (menu->page == HM_PAUSE_SHIPPING) {
        /* Reuse the original window frame and 4-by-2 item-slot layout. */
        hm_pause_original_frame(NULL, 25, 16, 270, 210);
        hm_center(assets, "Shipped Items", 20);
        int shipping_count = view ? view->shipping_count : 0;
        int page_count = (shipping_count + HM_PAUSE_SHIPPING_VISIBLE_ITEMS - 1) /
                         HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
        if (page_count < 1)
            page_count = 1;
        char page_text[24];
        snprintf(page_text, sizeof(page_text), "%d of %d",
                 menu->first_visible / HM_PAUSE_SHIPPING_VISIBLE_ITEMS + 1, page_count);
        hm_text_limited(assets, page_text, 220, 38, 8);
        if (page_count > 1) {
            int page = menu->first_visible / HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
            if (page > 0)
                hm_pause_page_arrow(207, 39, -1);
            if (page + 1 < page_count)
                hm_pause_page_arrow(286, 39, 1);
        }
        if (shipping_count > 0 && menu->selected < shipping_count) {
            char selected_item[40];
            snprintf(selected_item, sizeof(selected_item), "%s - %dx",
                     view->shipping_items[menu->selected],
                     view->shipping_quantities[menu->selected]);
            hm_center(assets, selected_item, 137);
        }
        for (int slot = 0; slot < HM_PAUSE_SHIPPING_VISIBLE_ITEMS; ++slot) {
            int index = menu->first_visible + slot;
            int x = 92 + (slot % HM_PAUSE_SHIPPING_COLUMNS) * 34;
            int y = 54 + (slot / HM_PAUSE_SHIPPING_COLUMNS) * 34;
            if (index < shipping_count && index == menu->selected)
                hm_pause_original_frame(NULL, x - 1, y - 1, 34, 34);
            hm_pause_original_frame(NULL, x, y, 32, 32);
            if (index >= shipping_count)
                continue;
            hm_item_icon(view->shipping_icons[index], x, y);
            if (view->shipping_quantities[index] > 1) {
                char quantity[8];
                snprintf(quantity, sizeof(quantity), "%d",
                         view->shipping_quantities[index] > 99 ? 99 :
                                                                 view->shipping_quantities[index]);
                hm_text_limited(assets, quantity, x + 4, y + 20, 2);
            }
        }
        hm_center(assets, view && view->shipping_basket ? "Tap an item to take or ship basket"
                                                        : "Tap an item to take", 161);
        hm_pause_button_icon(106, 190, 1);
        hm_text(assets, "Take", 121, 190);
        hm_pause_button_icon(173, 190, 0);
        hm_text(assets, "Close", 188, 190);
    } else if (menu->page == HM_PAUSE_GRAPHICS || menu->page == HM_PAUSE_GAMEPLAY ||
        menu->page == HM_PAUSE_CONTROLS || menu->page == HM_PAUSE_AUDIO ||
        menu->page == HM_PAUSE_BLESSINGS) {
        hm_pause_original_frame(NULL, 12, 8, 296, 28);
        const char *title = menu->page == HM_PAUSE_GRAPHICS ? "Settings - Graphics"
                            : (menu->page == HM_PAUSE_GAMEPLAY ? "Settings - Gameplay"
                            : (menu->page == HM_PAUSE_CONTROLS ? "Settings - Controls"
                            : (menu->page == HM_PAUSE_BLESSINGS ? "Goddess Blessings"
                                                               : "Settings - Audio")));
        hm_center(assets, title, 16);
        if (hm_pause_count(menu) > HM_PAUSE_VISIBLE_ROWS) {
            char range[16];
            snprintf(range, sizeof(range), "(%d-%d)", menu->first_visible + 1,
                     menu->first_visible + HM_PAUSE_VISIBLE_ROWS);
            hm_text(assets, range, 256, 16);
        }
        hm_pause_original_frame(NULL, 12, 40, 296, 156);
        int first = menu->first_visible;
        hm_pause_original_frame(NULL, 20, 45 + (menu->selected - first) * 20, 280, 20);
        for (int row = 0; row < HM_PAUSE_VISIBLE_ROWS && first + row < hm_pause_count(menu);
             ++row) {
            int i = first + row;
            int y = 49 + row * 20;
            if (i == menu->selected)
                hm_text(assets, "-", 26, y);
            hm_text(assets, hm_pause_label(menu, i), 40, y);
            if (menu->page != HM_PAUSE_BLESSINGS && view && view->values[i])
                hm_text(assets, view->values[i], 192, y);
        }
        hm_pause_original_frame(NULL, 12, 200, 296, 32);
        hm_text(assets, view ? view->detail : NULL, 24, 205);
        const char *footer = view && view->notice && *view->notice ? view->notice :
#ifdef __ANDROID__
                             "Tap rows; Cross select Circle back";
#else
                             "D-pad/Arrows K select Esc/J back";
#endif
        hm_text_limited(assets, footer, 24, 218, 34);
    } else {
        int root = menu->page == HM_PAUSE_ROOT;
        int top = root ? 54 : 38;
        int height = root ? 132 : 148;
        hm_pause_original_frame(NULL, 68, top, 184, height);
        hm_center(assets, root ? "Paused" : "Settings", top + 12);
        hm_pause_line(NULL, 84, top + 32, 236, top + 32, FRAME_DARK, 0);
        int row_start = top + (root ? 44 : 42);
        int row_step = root ? 20 : 24;
        hm_pause_original_frame(NULL, 80, row_start + menu->selected * row_step - 3, 160,
                                root ? 19 : 22);
        for (int i = 0; i < hm_pause_count(menu); ++i) {
            int y = row_start + i * row_step;
            if (i == menu->selected)
                hm_text(assets, "-", 90, y);
            hm_text(assets, hm_pause_label(menu, i), 108, y);
        }
        const u16 *font_prompt =
            assets && assets->font_light[0] ? assets->font_light : (assets ? assets->font : NULL);
#ifdef __ANDROID__
        hm_center_font(font_prompt, "Tap row / D-pad move", 202);
        hm_center_font(font_prompt, root ? "Cross Select  Circle Resume"
                                         : "Cross Select  Circle Back", 218);
#else
        hm_center_font(font_prompt, "D-pad/Arrows/WASD Move", 202);
        hm_center_font(font_prompt, root ? "Enter/K Select  Esc/J Resume"
                                         : "Enter/K Select  Esc/J Back", 218);
#endif
    }
    hm_canvas = NULL;
}
