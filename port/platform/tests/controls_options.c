#include "platform/controls_options.h"
#include "platform/user_data.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2 && SDL_Init(0));
    char *root = NULL;
    CHECK(SDL_asprintf(&root, "%s-%llu", argv[1], (unsigned long long)SDL_GetPerformanceCounter()) >= 0);
    CHECK(hm_user_data_start_at(root));

    hm_controls_options opt;
    hm_controls_defaults(&opt);

    CHECK(opt.controller_enabled == 1);
    CHECK(opt.keyboard_layout == HM_KEYBOARD_LAYOUT_ARROWS);

    char val[64];
    hm_controls_row_value(&opt, 0, val, sizeof(val));
    CHECK(!strcmp(val, "Enabled"));
    hm_controls_row_value(&opt, 1, val, sizeof(val));
    CHECK(!strcmp(val, "Arrows ZX"));
    hm_controls_row_value(&opt, HM_CONTROLS_PRIMARY_ROWS, val, sizeof(val));
    CHECK(!strcmp(val, "Q"));
    CHECK(hm_controls_change(&opt, HM_CONTROLS_PRIMARY_ROWS, 1) == 0);

    CHECK(hm_controls_change(&opt, 0, 1) == 1);
    CHECK(opt.controller_enabled == 0);
    hm_controls_row_value(&opt, 0, val, sizeof(val));
    CHECK(!strcmp(val, "Off"));

    CHECK(hm_controls_change(&opt, 1, 1) == 1);
    CHECK(opt.keyboard_layout == HM_KEYBOARD_LAYOUT_WASD);
    hm_controls_row_value(&opt, 1, val, sizeof(val));
    CHECK(!strcmp(val, "WASD JK"));

    /* Test save & load persistence */
    CHECK(hm_controls_save(&opt) == 1);
    /* Legacy mouse mappings are ignored after gameplay mouse controls were removed. */
    CHECK(hm_user_data_set_nested_setting("controls", "mouse", "\"move\"") == 1);

    hm_controls_options loaded;
    CHECK(hm_controls_load(&loaded) == 1);
    CHECK(loaded.controller_enabled == 0);
    CHECK(loaded.keyboard_layout == HM_KEYBOARD_LAYOUT_WASD);
    hm_controls_row_value(&loaded, HM_CONTROLS_PRIMARY_ROWS, val, sizeof(val));
    CHECK(!strcmp(val, "Q"));

    /* Rebinding assigns a free scancode and rejects duplicate assignments. */
    CHECK(hm_controls_bind(&loaded, HM_CONTROLS_PRIMARY_ROWS + 6, SDL_SCANCODE_C) == 1);
    CHECK(loaded.keyboard_layout == HM_KEYBOARD_LAYOUT_CUSTOM);
    CHECK(loaded.keyboard_mapping[6] == SDL_SCANCODE_C);
    CHECK(hm_controls_bind(&loaded, HM_CONTROLS_PRIMARY_ROWS + 7, SDL_SCANCODE_C) == 0);
#ifdef __ANDROID__
    CHECK(loaded.virtual_pad_mapping[4] == 6);
    CHECK(hm_controls_change(&loaded, HM_VIRTUAL_GAMEPAD_ROW, 1) == 1);
    CHECK(loaded.virtual_gamepad_enabled == 0);
    CHECK(hm_controls_change(&loaded, HM_VIRTUAL_PAD_ROWS_START + 4, 1) == 1);
    CHECK(loaded.virtual_pad_mapping[4] == 7);
    hm_controls_row_value(&loaded, HM_VIRTUAL_PAD_ROWS_START + 4, val, sizeof(val));
    CHECK(!strcmp(val, "Square"));
#endif

    /* Check helper descriptions fit 34-char font box */
    for (int i = 0; i < HM_CONTROLS_PRIMARY_ROWS; ++i) {
        char desc[128];
        hm_controls_value(&opt, i, desc, sizeof(desc));
        CHECK(strlen(desc) <= 34);
    }

    SDL_free(root);
    SDL_Quit();
    return 0;
}
