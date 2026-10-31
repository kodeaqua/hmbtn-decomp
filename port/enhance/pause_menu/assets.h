#ifndef HMBTN_PAUSE_ASSETS_H
#define HMBTN_PAUSE_ASSETS_H
#include "enhance/pause_menu/menu.h"
#include "psx/disc.h"

enum { HM_PAUSE_FONT_W = 128, HM_PAUSE_FONT_H = 120 };
enum { HM_PAUSE_MAX_VIEW_ITEMS = 40 };
enum { HM_PAUSE_SHIPPING_ITEMS = 380, HM_PAUSE_RUCKSACK_ITEMS = 8 };
enum { HM_PAUSE_ITEM_ICON_SIZE = 32 };
typedef struct {
    u16 font[HM_PAUSE_FONT_W * HM_PAUSE_FONT_H];
    u16 font_light[HM_PAUSE_FONT_W * HM_PAUSE_FONT_H];
} hm_pause_assets;
typedef struct {
    const char *values[HM_PAUSE_MAX_VIEW_ITEMS];
    const char *shipping_items[HM_PAUSE_SHIPPING_ITEMS];
    const u32 *shipping_icons[HM_PAUSE_SHIPPING_ITEMS];
    int shipping_quantities[HM_PAUSE_SHIPPING_ITEMS];
    int shipping_count;
    int shipping_first;
    int shipping_basket;
    const char *detail;
    const char *notice;
} hm_pause_view;
int hm_pause_assets_load(hm_pause_assets *assets, hm_disc *disc);
/* Supply a fresh 320x240 RGBA background: dims RGB by 40%, then draws original
 * dialog art at full brightness. Never touches game VRAM. */
void hm_pause_draw(const hm_pause_assets *assets, u8 *rgba, const hm_pause_menu *menu,
                   const hm_pause_view *view);
#endif
