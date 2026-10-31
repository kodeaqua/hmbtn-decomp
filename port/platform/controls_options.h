#ifndef HMBTN_CONTROLS_OPTIONS_H
#define HMBTN_CONTROLS_OPTIONS_H

#include <stddef.h>

enum {
    HM_KEYBOARD_LAYOUT_ARROWS = 0,
    HM_KEYBOARD_LAYOUT_WASD   = 1,
    HM_KEYBOARD_LAYOUT_OFF    = 2,
    HM_KEYBOARD_LAYOUT_CUSTOM = 3,
    HM_KEYBOARD_ACTIONS = 14,
    HM_CONTROLS_PRIMARY_ROWS = 2,
    HM_KEYBOARD_BINDING_ROWS = HM_CONTROLS_PRIMARY_ROWS + HM_KEYBOARD_ACTIONS,
    HM_VIRTUAL_PAD_ACTIONS = 14,
    HM_VIRTUAL_GAMEPAD_ROW = HM_KEYBOARD_BINDING_ROWS,
    HM_VIRTUAL_GAMEPAD_SIZE_ROW = HM_VIRTUAL_GAMEPAD_ROW + 1,
    HM_VIRTUAL_GAMEPAD_EDIT_ROW = HM_VIRTUAL_GAMEPAD_ROW + 2,
    HM_VIRTUAL_PAD_ROWS_START = HM_VIRTUAL_GAMEPAD_ROW + 3
};

typedef struct {
    int controller_enabled; /* 0 = Off, 1 = Enabled (default 1) */
    int keyboard_layout;    /* 0 = Arrows+ZX, 1 = WASD+JK, 2 = Off (default 0) */
    int keyboard_mapping[16]; /* SDL scancodes by native PS1 pad-bit index */
    int virtual_gamepad_enabled;
    int virtual_gamepad_size;
    int virtual_pad_mapping[HM_VIRTUAL_PAD_ACTIONS]; /* native PS1 pad-bit index by touch control */
} hm_controls_options;

void hm_controls_defaults(hm_controls_options *opt);
int hm_controls_load(hm_controls_options *opt);
int hm_controls_save(const hm_controls_options *opt);
int hm_controls_change(hm_controls_options *opt, int item, int step);
void hm_controls_row_value(const hm_controls_options *opt, int item, char *out, size_t size);
void hm_controls_value(const hm_controls_options *opt, int item, char *out, size_t size);
int hm_controls_bind(hm_controls_options *opt, int item, int scancode);

#endif
