#include "enhance/pause_menu/menu.h"
#include <stddef.h>

static const char *const hm_root_items[] = {"Resume", "Settings", "Goddess Blessings", "Exit Game"};
static const char *const hm_settings_items[] = {"Graphics", "Gameplay", "Controls", "Audio"};
#ifdef __ANDROID__
static const char *const hm_graphics_items[] = {"Graphics Mode"};
#else
static const char *const hm_graphics_items[] = {
    "Graphics Mode", "Monitor",   "Window Mode", "Resolution", "Enhance Scaling", "Widescreen Mode", "Anti-Aliasing",
    "Filtering", "VSync",       "FPS Limit",  "Interpolation"};
#endif
static const char *const hm_gameplay_items[] = {
    "Walk Crops", "Item Stacking", "Durable Fences", "Connect Fences", "Wild Dog Attacks",
    "Smart Bell", "Stamina HUD",   "Calendar Events","Roaming Horse",
    "Tool Charge Bar", "Tile Targeting",
#ifdef HMBTN_ENH_NPC_BONDS
    "NPC Relationships",
#endif
#ifdef HMBTN_ENH_GRASS_SPREAD
    "Grass Spread",
#endif
#ifdef HMBTN_ENH_SLOW_FARM_TIME
    "Slow Farm Time",
#endif
#ifdef HMBTN_ENH_SHIPPING_BIN
    "Shipping Bin List",
#endif
};
static const char *const hm_controls_items[] = {
    "Gamepad", "Keyboard", "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross",
    "Square", "Select", "Start", "Up", "Right", "Down", "Left"
#ifdef __ANDROID__
    , "Virtual Gamepad", "Button Size", "Edit Layout",
    "Pad Up", "Pad Right", "Pad Down", "Pad Left", "Pad Cross", "Pad Circle",
    "Pad Square", "Pad Triangle", "Pad L1", "Pad R1", "Pad L2", "Pad R2",
    "Pad Select", "Pad Start"
#endif
};
static const char *const hm_audio_items[] = {"BGM Volume", "SFX Volume", "Resampling"};
static const char *const hm_blessing_items[] = {
    "Add 10000G", "Restore stamina and fatigue", "Add 10 fodder",
    "Add 10 chicken feed", "Add 10 fish food"};

void hm_pause_open(hm_pause_menu *menu) {
    menu->page = HM_PAUSE_ROOT;
    menu->selected = 0;
    menu->first_visible = 0;
    menu->graphics_modern = 0;
    menu->content_count = 0;
}

hm_pause_input hm_pause_pointer_row_input(hm_pause_page page, int x) {
    if (page == HM_PAUSE_BLESSINGS || x < 192)
        return HM_PAUSE_CONFIRM;
    return x < 246 ? HM_PAUSE_LEFT : HM_PAUSE_RIGHT;
}

int hm_pause_count(const hm_pause_menu *menu) {
    return menu->page == HM_PAUSE_ROOT       ? 4
           : menu->page == HM_PAUSE_SETTINGS ? 4
           : menu->page == HM_PAUSE_GAMEPLAY ? HM_PAUSE_GAMEPLAY_ITEMS
           : menu->page == HM_PAUSE_CONTROLS ? HM_PAUSE_CONTROLS_ITEMS
           : menu->page == HM_PAUSE_AUDIO    ? HM_PAUSE_AUDIO_ITEMS
           : menu->page == HM_PAUSE_BLESSINGS ? HM_PAUSE_BLESSINGS_ITEMS
           : menu->page == HM_PAUSE_SHIPPING ? menu->content_count
                                             :
#ifdef __ANDROID__
                                               HM_PAUSE_GRAPHICS_ITEMS;
#else
                                               (menu->graphics_modern ? HM_PAUSE_GRAPHICS_ITEMS : 4);
#endif
}

const char *hm_pause_label(const hm_pause_menu *menu, int index) {
    if (index < 0 || index >= hm_pause_count(menu))
        return NULL;
    return menu->page == HM_PAUSE_ROOT       ? hm_root_items[index]
           : menu->page == HM_PAUSE_SETTINGS ? hm_settings_items[index]
           : menu->page == HM_PAUSE_GAMEPLAY ? hm_gameplay_items[index]
           : menu->page == HM_PAUSE_CONTROLS ? hm_controls_items[index]
           : menu->page == HM_PAUSE_AUDIO    ? hm_audio_items[index]
           : menu->page == HM_PAUSE_BLESSINGS ? hm_blessing_items[index]
           : menu->page == HM_PAUSE_SHIPPING ? "Shipped Item"
                                             : hm_graphics_items[index];
}

hm_pause_action hm_pause_handle(hm_pause_menu *menu, hm_pause_input input) {
    if (menu->page == HM_PAUSE_SHIPPING &&
        (input == HM_PAUSE_UP || input == HM_PAUSE_DOWN || input == HM_PAUSE_LEFT ||
         input == HM_PAUSE_RIGHT)) {
        int count = hm_pause_count(menu);
        if (count <= 0)
            return HM_PAUSE_NONE;
        int selected = menu->selected;
        int page_start = (selected / HM_PAUSE_SHIPPING_VISIBLE_ITEMS) *
                         HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
        int column = selected % HM_PAUSE_SHIPPING_COLUMNS;
        if (input == HM_PAUSE_LEFT && column > 0)
            --selected;
        else if (input == HM_PAUSE_LEFT && page_start > 0)
            --selected;
        else if (input == HM_PAUSE_RIGHT && column < HM_PAUSE_SHIPPING_COLUMNS - 1 &&
                 selected + 1 < count)
            ++selected;
        else if (input == HM_PAUSE_RIGHT &&
                 selected == page_start + HM_PAUSE_SHIPPING_VISIBLE_ITEMS - 1 &&
                 selected + 1 < count)
            ++selected;
        else if (input == HM_PAUSE_UP && selected >= page_start + HM_PAUSE_SHIPPING_COLUMNS)
            selected -= HM_PAUSE_SHIPPING_COLUMNS;
        else if (input == HM_PAUSE_UP && page_start > 0)
            selected = page_start - HM_PAUSE_SHIPPING_COLUMNS + column;
        else if (input == HM_PAUSE_DOWN && selected + HM_PAUSE_SHIPPING_COLUMNS < count)
            selected += HM_PAUSE_SHIPPING_COLUMNS;
        menu->selected = selected;
        menu->first_visible = (selected / HM_PAUSE_SHIPPING_VISIBLE_ITEMS) *
                              HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
        return HM_PAUSE_NONE;
    }
    if (input == HM_PAUSE_UP || input == HM_PAUSE_DOWN) {
        int count = hm_pause_count(menu);
        menu->selected = (menu->selected + (input == HM_PAUSE_UP ? count - 1 : 1)) % count;
        if (menu->selected < menu->first_visible)
            menu->first_visible = menu->selected;
        if (menu->selected >= menu->first_visible + HM_PAUSE_VISIBLE_ROWS)
            menu->first_visible = menu->selected - HM_PAUSE_VISIBLE_ROWS + 1;
    } else if (input == HM_PAUSE_BACK) {
        if (menu->page == HM_PAUSE_ROOT)
            return HM_PAUSE_RESUME;
        if (menu->page == HM_PAUSE_GRAPHICS || menu->page == HM_PAUSE_GAMEPLAY ||
            menu->page == HM_PAUSE_CONTROLS || menu->page == HM_PAUSE_AUDIO) {
            int prev = (menu->page == HM_PAUSE_AUDIO) ? 3 :
                       ((menu->page == HM_PAUSE_CONTROLS) ? 2 :
                       ((menu->page == HM_PAUSE_GAMEPLAY) ? 1 : 0));
            menu->page = HM_PAUSE_SETTINGS;
            menu->selected = prev;
        } else if (menu->page == HM_PAUSE_BLESSINGS) {
            menu->page = HM_PAUSE_ROOT;
            menu->selected = 2;
        } else if (menu->page == HM_PAUSE_SHIPPING) {
            return HM_PAUSE_RESUME;
        } else {
            menu->page = HM_PAUSE_ROOT;
            menu->selected = 1;
        }
        menu->first_visible = 0;
    } else if (input == HM_PAUSE_CONFIRM) {
        if (menu->page == HM_PAUSE_ROOT) {
            if (menu->selected == 0)
                return HM_PAUSE_RESUME;
            if (menu->selected == 3)
                return HM_PAUSE_EXIT;
            menu->page = menu->selected == 2 ? HM_PAUSE_BLESSINGS : HM_PAUSE_SETTINGS;
        } else if (menu->page == HM_PAUSE_SETTINGS) {
            menu->page = (menu->selected == 3) ? HM_PAUSE_AUDIO :
                         ((menu->selected == 2) ? HM_PAUSE_CONTROLS :
                         ((menu->selected == 1) ? HM_PAUSE_GAMEPLAY : HM_PAUSE_GRAPHICS));
        } else if (menu->page == HM_PAUSE_GAMEPLAY) {
            return HM_PAUSE_CHANGE_GAMEPLAY;
        } else if (menu->page == HM_PAUSE_CONTROLS) {
            if (menu->selected < HM_CONTROLS_PRIMARY_ROWS)
                return HM_PAUSE_CHANGE_CONTROLS;
            if (menu->selected < HM_KEYBOARD_BINDING_ROWS)
                return HM_PAUSE_REMAP_CONTROLS;
#ifdef __ANDROID__
            if (menu->selected == HM_VIRTUAL_GAMEPAD_EDIT_ROW)
                return HM_PAUSE_EDIT_TOUCH_LAYOUT;
            return HM_PAUSE_CHANGE_CONTROLS;
#else
            return HM_PAUSE_REMAP_CONTROLS;
#endif
        } else if (menu->page == HM_PAUSE_AUDIO) {
            return HM_PAUSE_CHANGE_AUDIO;
        } else if (menu->page == HM_PAUSE_BLESSINGS) {
            return HM_PAUSE_USE_BLESSING;
        } else if (menu->page == HM_PAUSE_SHIPPING) {
            return HM_PAUSE_CHANGE_SHIPPING;
        } else {
            return HM_PAUSE_CHANGE_GRAPHICS;
        }
        menu->selected = 0;
        menu->first_visible = 0;
    } else if (menu->page == HM_PAUSE_GRAPHICS &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_GRAPHICS;
    } else if (menu->page == HM_PAUSE_GAMEPLAY &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_GAMEPLAY;
    } else if (menu->page == HM_PAUSE_CONTROLS &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT) &&
               (menu->selected < HM_CONTROLS_PRIMARY_ROWS
#ifdef __ANDROID__
                || (menu->selected >= HM_VIRTUAL_GAMEPAD_ROW &&
                    menu->selected != HM_VIRTUAL_GAMEPAD_EDIT_ROW)
#endif
                )) {
        return HM_PAUSE_CHANGE_CONTROLS;
    } else if (menu->page == HM_PAUSE_AUDIO &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_AUDIO;
    }
    return HM_PAUSE_NONE;
}
