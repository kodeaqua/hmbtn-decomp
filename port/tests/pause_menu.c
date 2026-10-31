#include "enhance/pause_menu/menu.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    hm_pause_menu menu;
    CHECK(hm_pause_pointer_row_input(HM_PAUSE_BLESSINGS, 250) == HM_PAUSE_CONFIRM);
    CHECK(hm_pause_pointer_row_input(HM_PAUSE_GAMEPLAY, 191) == HM_PAUSE_CONFIRM);
    CHECK(hm_pause_pointer_row_input(HM_PAUSE_GAMEPLAY, 192) == HM_PAUSE_LEFT);
    CHECK(hm_pause_pointer_row_input(HM_PAUSE_GAMEPLAY, 246) == HM_PAUSE_RIGHT);
    hm_pause_open(&menu);
    CHECK(menu.page == HM_PAUSE_ROOT && hm_pause_count(&menu) == 4);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Resume"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Goddess Blessings"));
    CHECK(!strcmp(hm_pause_label(&menu, 3), "Exit Game"));
    CHECK(hm_pause_label(&menu, -1) == NULL && hm_pause_label(&menu, 4) == NULL);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_RESUME);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_UP) == HM_PAUSE_NONE && menu.selected == 3);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_EXIT);
    hm_pause_open(&menu);
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_NONE);
    CHECK(menu.page == HM_PAUSE_SETTINGS && hm_pause_count(&menu) == 4);
    CHECK(!strcmp(hm_pause_label(&menu, 3), "Audio"));
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Graphics"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Gameplay"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Controls"));
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 1);
    hm_pause_handle(&menu, HM_PAUSE_UP);
    CHECK(menu.selected == 0);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
#ifdef __ANDROID__
    CHECK(menu.page == HM_PAUSE_GRAPHICS && hm_pause_count(&menu) == HM_PAUSE_GRAPHICS_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Graphics Mode"));
#else
    CHECK(menu.page == HM_PAUSE_GRAPHICS && hm_pause_count(&menu) == 4);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Graphics Mode"));
    menu.graphics_modern = 1;
    CHECK(hm_pause_count(&menu) == HM_PAUSE_GRAPHICS_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 5), "Widescreen Mode"));
    CHECK(!strcmp(hm_pause_label(&menu, 7), "Filtering"));
#endif
    for (int i = 0; i < HM_PAUSE_GRAPHICS_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(menu.selected >= menu.first_visible &&
              menu.selected < menu.first_visible + HM_PAUSE_VISIBLE_ROWS);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_GRAPHICS);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    CHECK(menu.selected == 0 && menu.first_visible == 0);
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 0);
    /* Test gameplay settings menu */
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 1);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_GAMEPLAY && hm_pause_count(&menu) == HM_PAUSE_GAMEPLAY_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Walk Crops"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Item Stacking"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Durable Fences"));
    CHECK(!strcmp(hm_pause_label(&menu, 3), "Connect Fences"));
    CHECK(!strcmp(hm_pause_label(&menu, 4), "Wild Dog Attacks"));
    CHECK(!strcmp(hm_pause_label(&menu, 5), "Smart Bell"));
    CHECK(!strcmp(hm_pause_label(&menu, 6), "Stamina HUD"));
    CHECK(!strcmp(hm_pause_label(&menu, 7), "Calendar Events"));
    CHECK(!strcmp(hm_pause_label(&menu, 8), "Roaming Horse"));
    CHECK(!strcmp(hm_pause_label(&menu, 9), "Tool Charge Bar"));
    CHECK(!strcmp(hm_pause_label(&menu, 10), "Tile Targeting"));
#ifdef HMBTN_ENH_NPC_BONDS
    CHECK(!strcmp(hm_pause_label(&menu, 11), "NPC Relationships"));
#endif
    for (int i = 0; i < HM_PAUSE_GAMEPLAY_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_GAMEPLAY);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_LEFT) == HM_PAUSE_CHANGE_GAMEPLAY);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 1);

    /* Test controls settings menu */
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 2);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_CONTROLS && hm_pause_count(&menu) == HM_PAUSE_CONTROLS_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Gamepad"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Keyboard"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "L2"));
    for (int i = 0; i < HM_PAUSE_CONTROLS_ITEMS; ++i) {
        CHECK(menu.selected == i);
        int option_row = i < HM_CONTROLS_PRIMARY_ROWS;
#ifdef __ANDROID__
        option_row = option_row ||
                     (i >= HM_VIRTUAL_GAMEPAD_ROW && i != HM_VIRTUAL_GAMEPAD_EDIT_ROW);
#endif
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) ==
              (option_row ? HM_PAUSE_CHANGE_CONTROLS : HM_PAUSE_NONE));
        CHECK(hm_pause_handle(&menu, HM_PAUSE_LEFT) ==
              (option_row ? HM_PAUSE_CHANGE_CONTROLS : HM_PAUSE_NONE));
        hm_pause_action confirm = hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
        if (i < HM_CONTROLS_PRIMARY_ROWS)
            CHECK(confirm == HM_PAUSE_CHANGE_CONTROLS);
        else if (i < HM_KEYBOARD_BINDING_ROWS)
            CHECK(confirm == HM_PAUSE_REMAP_CONTROLS);
#ifdef __ANDROID__
        else if (i == HM_VIRTUAL_GAMEPAD_EDIT_ROW)
            CHECK(confirm == HM_PAUSE_EDIT_TOUCH_LAYOUT);
        else
            CHECK(confirm == HM_PAUSE_CHANGE_CONTROLS);
#else
        else
            CHECK(confirm == HM_PAUSE_REMAP_CONTROLS);
#endif
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 2);
    /* Test audio settings menu */
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 3);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_AUDIO && hm_pause_count(&menu) == HM_PAUSE_AUDIO_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "BGM Volume"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "SFX Volume"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Resampling"));
    for (int i = 0; i < HM_PAUSE_AUDIO_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_AUDIO);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_LEFT) == HM_PAUSE_CHANGE_AUDIO);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_CHANGE_AUDIO);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 3);

    /* The owner support page is reachable independently from Settings. */
    hm_pause_open(&menu);
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 2);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_NONE);
    CHECK(menu.page == HM_PAUSE_BLESSINGS && hm_pause_count(&menu) == HM_PAUSE_BLESSINGS_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Add 10000G"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Restore stamina and fatigue"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Add 10 fodder"));
    for (int i = 0; i < HM_PAUSE_BLESSINGS_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_USE_BLESSING);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    CHECK(menu.selected == 0);
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_ROOT && menu.selected == 2);

    CHECK(hm_pause_handle(&menu, HM_PAUSE_BACK) == HM_PAUSE_RESUME);
    hm_pause_open(&menu);
    CHECK(menu.selected == 0 && menu.page == HM_PAUSE_ROOT);
    return 0;
}
