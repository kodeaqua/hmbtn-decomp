#ifndef HMBTN_PAUSE_MENU_H
#define HMBTN_PAUSE_MENU_H
#include "platform/controls_options.h"

typedef enum {
    HM_PAUSE_ROOT,
    HM_PAUSE_SETTINGS,
    HM_PAUSE_GRAPHICS,
    HM_PAUSE_GAMEPLAY,
    HM_PAUSE_CONTROLS,
    HM_PAUSE_AUDIO,
    HM_PAUSE_BLESSINGS,
    HM_PAUSE_SHIPPING
} hm_pause_page;

enum {
#ifdef __ANDROID__
    HM_PAUSE_GRAPHICS_ITEMS = 1,
#else
    HM_PAUSE_GRAPHICS_ITEMS = 11,
#endif
    HM_PAUSE_GAMEPLAY_ITEMS =
#ifdef HMBTN_ENH_NPC_BONDS
        12
#else
        11
#endif
#ifdef HMBTN_ENH_GRASS_SPREAD
        + 1
#endif
#ifdef HMBTN_ENH_SLOW_FARM_TIME
        + 1
#endif
#ifdef HMBTN_ENH_SHIPPING_BIN
        + 1
#endif
        ,
#ifdef __ANDROID__
    HM_PAUSE_CONTROLS_ITEMS = HM_VIRTUAL_PAD_ROWS_START + HM_VIRTUAL_PAD_ACTIONS,
#else
    HM_PAUSE_CONTROLS_ITEMS = HM_KEYBOARD_BINDING_ROWS,
#endif
    HM_PAUSE_AUDIO_ITEMS = 3,
    HM_PAUSE_BLESSINGS_ITEMS = 5,
    HM_PAUSE_SHIPPING_COLUMNS = 4,
    HM_PAUSE_SHIPPING_ROWS = 2,
    HM_PAUSE_SHIPPING_VISIBLE_ITEMS = HM_PAUSE_SHIPPING_COLUMNS * HM_PAUSE_SHIPPING_ROWS,
    HM_PAUSE_VISIBLE_ROWS = 7
};

typedef enum {
    HM_PAUSE_UP,
    HM_PAUSE_DOWN,
    HM_PAUSE_BACK,
    HM_PAUSE_CONFIRM,
    HM_PAUSE_LEFT,
    HM_PAUSE_RIGHT
} hm_pause_input;

typedef enum {
    HM_PAUSE_NONE,
    HM_PAUSE_RESUME,
    HM_PAUSE_EXIT,
    HM_PAUSE_CHANGE_GRAPHICS,
    HM_PAUSE_CHANGE_GAMEPLAY,
    HM_PAUSE_CHANGE_CONTROLS,
    HM_PAUSE_CHANGE_AUDIO,
    HM_PAUSE_USE_BLESSING,
    HM_PAUSE_CHANGE_SHIPPING,
    HM_PAUSE_REMAP_CONTROLS,
    HM_PAUSE_EDIT_TOUCH_LAYOUT
} hm_pause_action;

typedef struct {
    hm_pause_page page;
    int selected;
    int first_visible;
    int graphics_modern;
    int content_count;
} hm_pause_menu;

void hm_pause_open(hm_pause_menu *menu);
int hm_pause_count(const hm_pause_menu *menu);
const char *hm_pause_label(const hm_pause_menu *menu, int index);
hm_pause_action hm_pause_handle(hm_pause_menu *menu, hm_pause_input input);
hm_pause_input hm_pause_pointer_row_input(hm_pause_page page, int x);

#endif
