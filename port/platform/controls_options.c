#include "platform/controls_options.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

static const int hm_keyboard_bind_indices[HM_KEYBOARD_ACTIONS] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15};
static const char *const hm_keyboard_bind_labels[HM_KEYBOARD_ACTIONS] = {
    "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square",
    "Select", "Start", "Up", "Right", "Down", "Left"};
static const int hm_virtual_pad_default[HM_VIRTUAL_PAD_ACTIONS] = {
    12, 13, 14, 15, 6, 5, 7, 4, 2, 3, 0, 1, 8, 11};
#ifdef __ANDROID__
static const char *const hm_pad_action_names[16] = {
    "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square",
    "Select", "L3", "R3", "Start", "Up", "Right", "Down", "Left"};
static const char *const hm_virtual_pad_labels[HM_VIRTUAL_PAD_ACTIONS] = {
    "Up", "Right", "Down", "Left", "Cross", "Circle", "Square", "Triangle",
    "L1", "R1", "L2", "R2", "Select", "Start"};
#endif

void hm_controls_defaults(hm_controls_options *opt) {
    if (!opt)
        return;
    opt->controller_enabled = 1;
    opt->keyboard_layout = HM_KEYBOARD_LAYOUT_ARROWS;
    opt->virtual_gamepad_enabled = 1;
    opt->virtual_gamepad_size = 100;
    static const int defaults[16] = {
        SDL_SCANCODE_Q, SDL_SCANCODE_E, SDL_SCANCODE_A, SDL_SCANCODE_S,
        SDL_SCANCODE_W, SDL_SCANCODE_D, SDL_SCANCODE_X, SDL_SCANCODE_Z,
        SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
        SDL_SCANCODE_RETURN, SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT};
    memcpy(opt->keyboard_mapping, defaults, sizeof(defaults));
    memcpy(opt->virtual_pad_mapping, hm_virtual_pad_default, sizeof(hm_virtual_pad_default));
}

int hm_controls_change(hm_controls_options *opt, int item, int step) {
    if (!opt)
        return 0;
    switch (item) {
    case 0:
        opt->controller_enabled = !opt->controller_enabled;
        return 1;
    case 1:
        if (step > 0)
            opt->keyboard_layout = (opt->keyboard_layout + 1) % 4;
        else
            opt->keyboard_layout = (opt->keyboard_layout + 3) % 4;
        return 1;
#ifdef __ANDROID__
    case HM_VIRTUAL_GAMEPAD_ROW:
        opt->virtual_gamepad_enabled = !opt->virtual_gamepad_enabled;
        return 1;
    case HM_VIRTUAL_GAMEPAD_SIZE_ROW: {
        int size = opt->virtual_gamepad_size + (step > 0 ? 10 : -10);
        if (size < 70) size = 140;
        if (size > 140) size = 70;
        opt->virtual_gamepad_size = size;
        return 1;
    }
#endif
    default:
#ifdef __ANDROID__
        if (item >= HM_VIRTUAL_PAD_ROWS_START &&
            item < HM_VIRTUAL_PAD_ROWS_START + HM_VIRTUAL_PAD_ACTIONS) {
            int index = item - HM_VIRTUAL_PAD_ROWS_START;
            int value = opt->virtual_pad_mapping[index] + (step > 0 ? 1 : 16);
            opt->virtual_pad_mapping[index] = value % 17;
            return 1;
        }
#endif
        return 0;
    }
}

void hm_controls_row_value(const hm_controls_options *opt, int item, char *out, size_t size) {
    if (!opt || !out || size == 0)
        return;
    switch (item) {
    case 0:
        snprintf(out, size, "%s", opt->controller_enabled ? "Enabled" : "Off");
        break;
    case 1:
        switch (opt->keyboard_layout) {
        case HM_KEYBOARD_LAYOUT_ARROWS:
            snprintf(out, size, "Arrows ZX");
            break;
        case HM_KEYBOARD_LAYOUT_WASD:
            snprintf(out, size, "WASD JK");
            break;
        case HM_KEYBOARD_LAYOUT_CUSTOM:
            snprintf(out, size, "Custom");
            break;
        default:
            snprintf(out, size, "Off");
            break;
        }
        break;
    default:
#ifdef __ANDROID__
        if (item == HM_VIRTUAL_GAMEPAD_ROW) {
            snprintf(out, size, "%s", opt->virtual_gamepad_enabled ? "On" : "Off");
            break;
        }
        if (item == HM_VIRTUAL_GAMEPAD_SIZE_ROW) {
            snprintf(out, size, "%d%%", opt->virtual_gamepad_size);
            break;
        }
        if (item == HM_VIRTUAL_GAMEPAD_EDIT_ROW) {
            snprintf(out, size, "Adjust");
            break;
        }
        if (item >= HM_VIRTUAL_PAD_ROWS_START &&
            item < HM_VIRTUAL_PAD_ROWS_START + HM_VIRTUAL_PAD_ACTIONS) {
            int action = opt->virtual_pad_mapping[item - HM_VIRTUAL_PAD_ROWS_START];
            snprintf(out, size, "%s", action >= 0 && action < 16 ? hm_pad_action_names[action] : "Off");
            break;
        }
#endif
        if (item >= HM_CONTROLS_PRIMARY_ROWS && item < HM_KEYBOARD_BINDING_ROWS) {
            int action = item - HM_CONTROLS_PRIMARY_ROWS;
            int index = hm_keyboard_bind_indices[action];
            const char *name = SDL_GetScancodeName((SDL_Scancode)opt->keyboard_mapping[index]);
            snprintf(out, size, "%s", name && *name ? name : "None");
        } else {
            snprintf(out, size, "-");
        }
        break;
    }
}

void hm_controls_value(const hm_controls_options *opt, int item, char *out, size_t size) {
    if (!opt || !out || size == 0)
        return;
    switch (item) {
    case 0:
        if (opt->controller_enabled)
            snprintf(out, size, "Standard gamepad controller");
        else
            snprintf(out, size, "Controller/Gamepad input disabled");
        break;
    case 1:
        switch (opt->keyboard_layout) {
        case HM_KEYBOARD_LAYOUT_ARROWS:
            snprintf(out, size, "Preset A - Arrows and ZX");
            break;
        case HM_KEYBOARD_LAYOUT_WASD:
            snprintf(out, size, "Preset B - WASD and JK");
            break;
        case HM_KEYBOARD_LAYOUT_CUSTOM:
            snprintf(out, size, "Custom per-button keyboard bindings");
            break;
        default:
            snprintf(out, size, "Keyboard input disabled");
            break;
        }
        break;
    default:
#ifdef __ANDROID__
        if (item == HM_VIRTUAL_GAMEPAD_ROW) {
            snprintf(out, size, "Show or hide the on-screen virtual gamepad");
            break;
        }
        if (item == HM_VIRTUAL_GAMEPAD_SIZE_ROW) {
            snprintf(out, size, "Adjust the screen button size");
            break;
        }
        if (item == HM_VIRTUAL_GAMEPAD_EDIT_ROW) {
            snprintf(out, size, "Drag buttons to move them, then tap Done");
            break;
        }
        if (item >= HM_VIRTUAL_PAD_ROWS_START &&
            item < HM_VIRTUAL_PAD_ROWS_START + HM_VIRTUAL_PAD_ACTIONS) {
            snprintf(out, size, "Map %s to PS1 action",
                     hm_virtual_pad_labels[item - HM_VIRTUAL_PAD_ROWS_START]);
            break;
        }
#endif
        if (item >= HM_CONTROLS_PRIMARY_ROWS && item < HM_KEYBOARD_BINDING_ROWS)
            snprintf(out, size, "Bind %s to a key",
                     hm_keyboard_bind_labels[item - HM_CONTROLS_PRIMARY_ROWS]);
        else
            out[0] = '\0';
        break;
    }
}

int hm_controls_load(hm_controls_options *opt) {
    if (!opt)
        return 0;
    hm_controls_defaults(opt);
    char *text = malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "controls", &object, &length) == 1) {
        const char *val;
        size_t val_len;
        if (hm_json_property(object, length, "controller", &val, &val_len) == 1) {
            if (val_len == 5 && !memcmp(val, "false", 5))
                opt->controller_enabled = 0;
            else if (val_len == 4 && !memcmp(val, "true", 4))
                opt->controller_enabled = 1;
        }
        if (hm_json_property(object, length, "keyboard", &val, &val_len) == 1) {
            if (val_len == 6 && !memcmp(val, "\"wasd\"", 6))
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_WASD;
            else if (val_len == 5 && !memcmp(val, "\"off\"", 5))
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_OFF;
            else if (val_len == 8 && !memcmp(val, "\"custom\"", 8))
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_CUSTOM;
            else
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_ARROWS;
        }
        for (int i = 0; i < 16; ++i) {
            char key[16];
            int value;
            snprintf(key, sizeof(key), "key_%d", i);
            if (hm_json_integer(object, length, key, &value) &&
                value >= SDL_SCANCODE_UNKNOWN && value < SDL_SCANCODE_COUNT)
                opt->keyboard_mapping[i] = value;
        }
#ifdef __ANDROID__
        if (hm_json_property(object, length, "virtual_gamepad", &val, &val_len) == 1)
            opt->virtual_gamepad_enabled = (val_len == 4 && !memcmp(val, "true", 4));
        hm_json_integer(object, length, "virtual_gamepad_size", &opt->virtual_gamepad_size);
        if (opt->virtual_gamepad_size < 70 || opt->virtual_gamepad_size > 140)
            opt->virtual_gamepad_size = 100;
        for (int i = 0; i < HM_VIRTUAL_PAD_ACTIONS; ++i) {
            char key[24];
            int value;
            snprintf(key, sizeof(key), "virtual_pad_%d", i);
            if (hm_json_integer(object, length, key, &value) && value >= 0 && value <= 16)
                opt->virtual_pad_mapping[i] = value;
        }
#endif
    }
    free(text);
    return 1;
}

int hm_controls_save(const hm_controls_options *opt) {
    if (!opt)
        return 0;
    int ok = 1;
    ok &= hm_user_data_set_nested_setting("controls", "controller",
                                          opt->controller_enabled ? "true" : "false");
    const char *kb_val = (opt->keyboard_layout == HM_KEYBOARD_LAYOUT_WASD) ? "\"wasd\"" :
                         ((opt->keyboard_layout == HM_KEYBOARD_LAYOUT_OFF) ? "\"off\"" :
                         ((opt->keyboard_layout == HM_KEYBOARD_LAYOUT_CUSTOM) ? "\"custom\"" : "\"arrows\""));
    ok &= hm_user_data_set_nested_setting("controls", "keyboard", kb_val);
#ifdef __ANDROID__
    ok &= hm_user_data_set_nested_setting("controls", "virtual_gamepad",
                                          opt->virtual_gamepad_enabled ? "true" : "false");
    char size_value[16];
    snprintf(size_value, sizeof(size_value), "%d", opt->virtual_gamepad_size);
    ok &= hm_user_data_set_nested_setting("controls", "virtual_gamepad_size", size_value);
    for (int i = 0; i < HM_VIRTUAL_PAD_ACTIONS; ++i) {
        char key[24];
        char value[16];
        snprintf(key, sizeof(key), "virtual_pad_%d", i);
        snprintf(value, sizeof(value), "%d", opt->virtual_pad_mapping[i]);
        ok &= hm_user_data_set_nested_setting("controls", key, value);
    }
#endif
    for (int i = 0; i < 16; ++i) {
        char key[16];
        char value[16];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "%d", opt->keyboard_mapping[i]);
        ok &= hm_user_data_set_nested_setting("controls", key, value);
    }
    return ok;
}

int hm_controls_bind(hm_controls_options *opt, int item, int scancode) {
    if (!opt || item < HM_CONTROLS_PRIMARY_ROWS || item >= HM_KEYBOARD_BINDING_ROWS ||
        scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT)
        return 0;
#ifdef __ANDROID__
    /* These rarely used Android hardware keys are reserved for the touch pad. */
    if ((scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) ||
        scancode == SDL_SCANCODE_PRINTSCREEN || scancode == SDL_SCANCODE_SCROLLLOCK)
        return 0;
#endif
    int index = hm_keyboard_bind_indices[item - HM_CONTROLS_PRIMARY_ROWS];
    for (int i = 0; i < HM_KEYBOARD_ACTIONS; ++i) {
        int other = hm_keyboard_bind_indices[i];
        if (other != index && opt->keyboard_mapping[other] == scancode)
            return 0;
    }
    opt->keyboard_mapping[index] = scancode;
    opt->keyboard_layout = HM_KEYBOARD_LAYOUT_CUSTOM;
    return 1;
}
