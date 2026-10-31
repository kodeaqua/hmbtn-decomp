#include "platform/gameplay_options.h"
#include "enhance/gameplay/gameplay.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HMBTN_ENH_NPC_BONDS
#define HM_GAMEPLAY_EXTRA_INDEX 12
#else
#define HM_GAMEPLAY_EXTRA_INDEX 11
#endif
#define HM_GAMEPLAY_GRASS_SPREAD_INDEX HM_GAMEPLAY_EXTRA_INDEX
#define HM_GAMEPLAY_SLOW_TIME_INDEX (HM_GAMEPLAY_EXTRA_INDEX + 1)
#define HM_GAMEPLAY_SHIPPING_BIN_INDEX (HM_GAMEPLAY_EXTRA_INDEX + 2)

void hm_gameplay_defaults(hm_gameplay_options *options) {
    if (!options)
        return;
    options->walk_through_crops = 0;
    options->item_stacking = 0;
    options->durable_fences = 0;
    options->connect_fences = 0;
    options->wild_dog = 1;
    options->smart_bell = 0;
    options->stamina_hud = 0;
    options->calendar_events = 0;
    options->roaming_horse = 0;
    options->fast_text = 0;
    options->tool_charge_gauge = 0;
    options->tile_targeting = 0;
    options->npc_bonds = 0;
    options->grass_spread = 0;
    options->slow_farm_time = 0;
    options->shipping_bin = 0;
}

void hm_gameplay_load(hm_gameplay_options *options) {
    hm_gameplay_defaults(options);
    char *text = malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1) {
        const char *val;
        size_t val_len;
        if (hm_json_property(object, length, "walk_through_crops", &val, &val_len) == 1)
            options->walk_through_crops = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "item_stacking", &val, &val_len) == 1)
            options->item_stacking = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "durable_fences", &val, &val_len) == 1)
            options->durable_fences = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "connect_fences", &val, &val_len) == 1)
            options->connect_fences = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "wild_dog", &val, &val_len) == 1) {
            if (val_len == 5 && !memcmp(val, "false", 5))
                options->wild_dog = 0;
            else if (val_len == 4 && !memcmp(val, "true", 4))
                options->wild_dog = 1;
        }
        if (hm_json_property(object, length, "smart_bell", &val, &val_len) == 1)
            options->smart_bell = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "stamina_hud", &val, &val_len) == 1)
            options->stamina_hud = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "calendar_events", &val, &val_len) == 1)
            options->calendar_events = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "roaming_horse", &val, &val_len) == 1)
            options->roaming_horse = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "fast_text", &val, &val_len) == 1)
            options->fast_text = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "tool_charge_gauge", &val, &val_len) == 1)
            options->tool_charge_gauge = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "tile_targeting", &val, &val_len) == 1)
            options->tile_targeting = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "npc_bonds", &val, &val_len) == 1)
            options->npc_bonds = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "grass_spread", &val, &val_len) == 1)
            options->grass_spread = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "slow_farm_time", &val, &val_len) == 1)
            options->slow_farm_time = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "shipping_bin", &val, &val_len) == 1)
            options->shipping_bin = (val_len == 4 && !memcmp(val, "true", 4));
    }
    free(text);
    hm_gameplay_set_active(options);
}

int hm_gameplay_save(const hm_gameplay_options *options) {
    if (!options)
        return 0;
    hm_gameplay_set_active(options);
    int ok = 1;
    ok &= hm_user_data_set_nested_setting("enhance", "walk_through_crops",
                                          options->walk_through_crops ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "item_stacking",
                                          options->item_stacking ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "durable_fences",
                                          options->durable_fences ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "connect_fences",
                                          options->connect_fences ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "wild_dog",
                                          options->wild_dog ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "smart_bell",
                                          options->smart_bell ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "stamina_hud",
                                          options->stamina_hud ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "calendar_events",
                                          options->calendar_events ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "roaming_horse",
                                          options->roaming_horse ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "fast_text",
                                          options->fast_text ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "tool_charge_gauge",
                                          options->tool_charge_gauge ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "tile_targeting",
                                          options->tile_targeting ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "npc_bonds",
                                          options->npc_bonds ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "grass_spread",
                                          options->grass_spread ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "slow_farm_time",
                                          options->slow_farm_time ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "shipping_bin",
                                          options->shipping_bin ? "true" : "false");
    return ok;
}

int hm_gameplay_change(hm_gameplay_options *options, int item, int direction) {
    (void)direction;
    if (!options)
        return 0;
    switch (item) {
    case 0:
        options->walk_through_crops = !options->walk_through_crops;
        break;
    case 1:
        options->item_stacking = !options->item_stacking;
        break;
    case 2:
        options->durable_fences = !options->durable_fences;
        break;
    case 3:
        options->connect_fences = !options->connect_fences;
        break;
    case 4:
        options->wild_dog = !options->wild_dog;
        break;
    case 5:
        options->smart_bell = !options->smart_bell;
        break;
    case 6:
        options->stamina_hud = !options->stamina_hud;
        break;
    case 7:
        options->calendar_events = !options->calendar_events;
        break;
    case 8:
        options->roaming_horse = !options->roaming_horse;
        break;
    case 9:
        options->tool_charge_gauge = !options->tool_charge_gauge;
        break;
    case 10:
        options->tile_targeting = !options->tile_targeting;
        break;
#ifdef HMBTN_ENH_NPC_BONDS
    case 11:
        options->npc_bonds = !options->npc_bonds;
        break;
#endif
    case HM_GAMEPLAY_GRASS_SPREAD_INDEX:
        options->grass_spread = !options->grass_spread;
        break;
    case HM_GAMEPLAY_SLOW_TIME_INDEX:
        options->slow_farm_time = !options->slow_farm_time;
        break;
    case HM_GAMEPLAY_SHIPPING_BIN_INDEX:
        options->shipping_bin = !options->shipping_bin;
        break;
    default:
        return 0;
    }
    hm_gameplay_set_active(options);
    return 1;
}

void hm_gameplay_value(const hm_gameplay_options *options, int item, char *text, size_t capacity) {
    if (!options || !text || !capacity)
        return;
    switch (item) {
    case 0:
        snprintf(text, capacity, "Walk freely through farm crops");
        break;
    case 1:
        snprintf(text, capacity, "Stack items in rucksack up to 99");
        break;
    case 2:
        snprintf(text, capacity, "Prevent farm lumber from rotting");
        break;
    case 3:
        snprintf(text, capacity, "Connect adjacent wooden fences");
        break;
    case 4:
        snprintf(text, capacity, "Allow wild dog attacks at night");
        break;
    case 5:
        snprintf(text, capacity, "Bell guides animals in and out");
        break;
    case 6:
        snprintf(text, capacity, "Show stamina and hearts on HUD");
        break;
    case 7:
        snprintf(text, capacity, "Show birthdays on calendar");
        break;
    case 8:
        snprintf(text, capacity, "Allow riding horse outside farm");
        break;
    case 9:
        snprintf(text, capacity, "Show tool charge bar above Pete");
        break;
    case 10:
        snprintf(text, capacity, "Highlight targeted field tile grid");
        break;
#ifdef HMBTN_ENH_NPC_BONDS
    case 11:
        snprintf(text, capacity, "Show NPC friendship and affection page");
        break;
#endif
    case HM_GAMEPLAY_GRASS_SPREAD_INDEX:
        snprintf(text, capacity, "Planted mature fodder grass spreads on the farm");
        break;
    case HM_GAMEPLAY_SLOW_TIME_INDEX:
        snprintf(text, capacity, "Farm outdoor clock advances at half speed");
        break;
    case HM_GAMEPLAY_SHIPPING_BIN_INDEX:
        snprintf(text, capacity, "X at shipping box; Square with basket");
        break;
    default:
        text[0] = 0;
        break;
    }
}

void hm_gameplay_row_value(const hm_gameplay_options *options, int item, char *text, size_t capacity) {
    if (!options || !text || !capacity)
        return;
    switch (item) {
    case 0:
        snprintf(text, capacity, "%s", options->walk_through_crops ? "On" : "Off");
        break;
    case 1:
        snprintf(text, capacity, "%s", options->item_stacking ? "On" : "Off");
        break;
    case 2:
        snprintf(text, capacity, "%s", options->durable_fences ? "On" : "Off");
        break;
    case 3:
        snprintf(text, capacity, "%s", options->connect_fences ? "On" : "Off");
        break;
    case 4:
        snprintf(text, capacity, "%s", options->wild_dog ? "Enabled" : "Disabled");
        break;
    case 5:
        snprintf(text, capacity, "%s", options->smart_bell ? "On" : "Off");
        break;
    case 6:
        snprintf(text, capacity, "%s", options->stamina_hud ? "On" : "Off");
        break;
    case 7:
        snprintf(text, capacity, "%s", options->calendar_events ? "On" : "Off");
        break;
    case 8:
        snprintf(text, capacity, "%s", options->roaming_horse ? "On" : "Off");
        break;
    case 9:
        snprintf(text, capacity, "%s", options->tool_charge_gauge ? "On" : "Off");
        break;
    case 10:
        snprintf(text, capacity, "%s", options->tile_targeting ? "On" : "Off");
        break;
#ifdef HMBTN_ENH_NPC_BONDS
    case 11:
        snprintf(text, capacity, "%s", options->npc_bonds ? "On" : "Off");
        break;
#endif
    case HM_GAMEPLAY_GRASS_SPREAD_INDEX:
        snprintf(text, capacity, "%s", options->grass_spread ? "On" : "Off");
        break;
    case HM_GAMEPLAY_SLOW_TIME_INDEX:
        snprintf(text, capacity, "%s", options->slow_farm_time ? "On" : "Off");
        break;
    case HM_GAMEPLAY_SHIPPING_BIN_INDEX:
        snprintf(text, capacity, "%s", options->shipping_bin ? "On" : "Off");
        break;
    default:
        text[0] = 0;
        break;
    }
}
