#include "enhance/gameplay/gameplay.h"
#include "enhance/gameplay/mine_grid.h"
#include "gfx/game_frame.h"
#include <libgpu.h>
#include <libgte.h>
#include "farm_state.h"
#include "farm_tables.h"
#include "field.h"
#include "main.h"
#include "prim.h"
#include "model.h"
#include "event_flags.h"
#include "text.h"
#include "menu_window.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FIELD_CELL_SIZE 400
#define LIVESTOCK_ADULT_DAYS 28

typedef struct {
    int walk_through_crops;
    int item_stacking;
    int durable_fences;
    int connect_fences;
    int wild_dog;
    int smart_bell;
    int stamina_hud;
    int calendar_events;
    int roaming_horse;
    int fast_text;
    int tool_charge_gauge;
    int tile_targeting;
    int npc_bonds;
    int grass_spread;
    int slow_farm_time;
    int shipping_bin;
} hm_gameplay_runtime_options;

static hm_gameplay_runtime_options active_options = {
    .walk_through_crops = 0,
    .item_stacking = 0,
    .durable_fences = 0,
    .connect_fences = 0,
    .wild_dog = 1,
    .smart_bell = 0,
    .stamina_hud = 0,
    .calendar_events = 0,
    .roaming_horse = 0,
    .fast_text = 0,
    .tool_charge_gauge = 0,
    .tile_targeting = 0,
    .npc_bonds = 0,
    .grass_spread = 0,
    .slow_farm_time = 0,
    .shipping_bin = 0,
};

#ifdef HMBTN_ENH_SHIPPING_BIN
static u16 shipping_today[ITEM_COUNT];
static u16 shipping_basket_pending[ITEM_COUNT];
static int shipping_ui_request;
#endif

int hm_gameplay_walk_through_crops(void) {
    return active_options.walk_through_crops;
}

int hm_gameplay_item_stacking(void) {
    return active_options.item_stacking;
}

int hm_gameplay_durable_fences(void) {
    return active_options.durable_fences;
}

int hm_gameplay_connect_fences(void) {
    return active_options.connect_fences;
}

int hm_gameplay_wild_dog_enabled(void) {
    return active_options.wild_dog;
}

int hm_gameplay_smart_bell(void) {
    return active_options.smart_bell;
}

int hm_gameplay_stamina_hud(void) {
    return active_options.stamina_hud;
}

int hm_gameplay_fatigue_fill(int fatigue) {
    if (fatigue < 0) fatigue = 0;
    if (fatigue > 100) fatigue = 100;
    int width = fatigue * 59 / 100;
    return fatigue && !width ? 1 : width;
}

void hm_gameplay_draw_fatigue_bar(s16 x, s16 y, int fatigue) {
    if (!hm_gameplay_stamina_hud()) return;
    int width = hm_gameplay_fatigue_fill(fatigue);
    int color = fatigue >= 80 ? 0xc05040 : fatigue >= 50 ? 0xc89940 : 0x6f9b70;
    u_long *background = &current_frame->otTags[0][35];
    u_long *foreground = &current_frame->otTags[0][34];
    prim_add_rect(background, x + 23, y + 14, 59, 4, 0x806c54, 0);
    if (width) prim_add_rect(foreground, x + 23, y + 14, (s16)width, 4, color, 0);
}

int hm_gameplay_calendar_events(void) {
    return active_options.calendar_events;
}

int hm_gameplay_roaming_horse(void) {
    return active_options.roaming_horse;
}
int hm_gameplay_fast_text(void) {
    return 0;
}

int hm_gameplay_tool_charge_gauge(void) {
    return active_options.tool_charge_gauge;
}

int hm_gameplay_tile_targeting(void) {
    return active_options.tile_targeting;
}

int hm_gameplay_npc_bonds(void) {
    return active_options.npc_bonds;
}

int hm_gameplay_grass_spread(void) {
    return active_options.grass_spread;
}

int hm_gameplay_slow_farm_time(void) {
    return active_options.slow_farm_time;
}

int hm_gameplay_shipping_bin(void) {
    return active_options.shipping_bin;
}

void hm_gameplay_shipping_record(int item_id) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    if (item_id >= 0 && item_id < ITEM_COUNT && shipping_today[item_id] < 65535)
        shipping_today[item_id]++;
#else
    (void)item_id;
#endif
}

void hm_gameplay_shipping_basket_record(int item_id) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    if (item_id >= 0 && item_id < ITEM_COUNT && shipping_basket_pending[item_id] < 65535)
        shipping_basket_pending[item_id]++;
#else
    (void)item_id;
#endif
}

void hm_gameplay_shipping_basket_commit(void) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    for (int id = 0; id < ITEM_COUNT; ++id) {
        u32 total = (u32)shipping_today[id] + shipping_basket_pending[id];
        shipping_today[id] = total > 65535 ? 65535 : (u16)total;
    }
    memset(shipping_basket_pending, 0, sizeof(shipping_basket_pending));
#endif
}

void hm_gameplay_shipping_basket_clear(void) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    memset(shipping_basket_pending, 0, sizeof(shipping_basket_pending));
#endif
}

void hm_gameplay_shipping_forget(int item_id) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    if (item_id >= 0 && item_id < ITEM_COUNT && shipping_today[item_id] > 0)
        shipping_today[item_id]--;
#else
    (void)item_id;
#endif
}

int hm_gameplay_shipping_count(int item_id) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    return item_id >= 0 && item_id < ITEM_COUNT ? shipping_today[item_id] : 0;
#else
    (void)item_id;
    return 0;
#endif
}

void hm_gameplay_shipping_clear(void) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    memset(shipping_today, 0, sizeof(shipping_today));
    shipping_ui_request = HM_SHIPPING_REQUEST_NONE;
#endif
}

void hm_gameplay_shipping_request(int source) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    if (source == HM_SHIPPING_REQUEST_FIXED_BIN ||
        source == HM_SHIPPING_REQUEST_PORTABLE_BASKET) {
        shipping_ui_request = source;
    }
#else
    (void)source;
#endif
}

int hm_gameplay_shipping_take_request(void) {
#ifdef HMBTN_ENH_SHIPPING_BIN
    int requested = shipping_ui_request;
    shipping_ui_request = HM_SHIPPING_REQUEST_NONE;
    return requested;
#else
    return 0;
#endif
}

static u32 hm_gameplay_grass_spread_hash(int year, int season, int day, int row, int column) {
    u32 value = (u32)year * 0x9E3779B1u;
    value ^= (u32)(season + 1) * 0x85EBCA6Bu;
    value ^= (u32)(day + 1) * 0xC2B2AE35u;
    value ^= (u32)(row + 1) * 0x27D4EB2Fu;
    value ^= (u32)(column + 1) * 0x165667B1u;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    return value;
}

/* Half of the farm days are spread days for each mature tile. The direction is
 * repeatable from date and position, so this enhancement never consumes the
 * original game's random stream. */
int hm_gameplay_grass_spread_due(int year, int season, int day, int row, int column) {
    return (hm_gameplay_grass_spread_hash(year, season, day, row, column) & 1u) == 0;
}

int hm_gameplay_grass_spread_direction(int year, int season, int day, int row, int column) {
    return (int)((hm_gameplay_grass_spread_hash(year, season, day, row, column) >> 8) & 3u);
}


void hm_gameplay_set_active(const void *options_ptr) {
    if (options_ptr) {
        const hm_gameplay_runtime_options *opt = (const hm_gameplay_runtime_options *)options_ptr;
        active_options = *opt;
        if (!active_options.roaming_horse) {
            hm_gameplay_horse_safety_check();
        }
    }
}

extern s32 estate_point_project(SVECTOR *input, FarmScreenPoint *result);

void hm_gameplay_draw_fence_connections(s16 row, s16 col, s16 x, s16 y, s32 depth) {
    if (depth < 0 || depth >= (FRAME_OT_DEPTH << 2))
        return;
    s32 base_ot = (depth >> 2) + 1;
    if (base_ot >= FRAME_OT_DEPTH)
        base_ot = depth >> 2;
    if (base_ot < 0 || base_ot >= FRAME_OT_DEPTH)
        return;

    /* Check right neighbor (col + 1) */
    if (col + 1 < FIELD_COLS && game_state.fieldTiles[row].rows[col + 1].value == ITEM_LUMBER) {
        SVECTOR right_pt = {
            .vx = (col + 1) * FIELD_CELL_SIZE - 4050,
            .vy = -100,
            .vz = 1250 - row * FIELD_CELL_SIZE
        };
        FarmScreenPoint right_scr;
        if (estate_point_project(&right_pt, &right_scr) != 0) {
            /* Upper rail: wood highlight and shadow */
            POLY_F4 *p_top = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top);
            setRGB0(p_top, 175, 125, 78); /* Warm lumber highlight */
            p_top->x0 = x;           p_top->y0 = y - 9;
            p_top->x1 = right_scr.x; p_top->y1 = right_scr.y - 9;
            p_top->x2 = x;           p_top->y2 = y - 7;
            p_top->x3 = right_scr.x; p_top->y3 = right_scr.y - 7;
            AddPrim(&current_frame->ot[base_ot], p_top);

            POLY_F4 *p_bot = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot);
            setRGB0(p_bot, 105, 68, 40); /* Lumber shadow */
            p_bot->x0 = x;           p_bot->y0 = y - 7;
            p_bot->x1 = right_scr.x; p_bot->y1 = right_scr.y - 7;
            p_bot->x2 = x;           p_bot->y2 = y - 5;
            p_bot->x3 = right_scr.x; p_bot->y3 = right_scr.y - 5;
            AddPrim(&current_frame->ot[base_ot], p_bot);

            /* Lower rail: wood highlight and shadow */
            POLY_F4 *p_top2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top2);
            setRGB0(p_top2, 160, 115, 70);
            p_top2->x0 = x;           p_top2->y0 = y - 3;
            p_top2->x1 = right_scr.x; p_top2->y1 = right_scr.y - 3;
            p_top2->x2 = x;           p_top2->y2 = y - 1;
            p_top2->x3 = right_scr.x; p_top2->y3 = right_scr.y - 1;
            AddPrim(&current_frame->ot[base_ot], p_top2);

            POLY_F4 *p_bot2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot2);
            setRGB0(p_bot2, 95, 60, 35);
            p_bot2->x0 = x;           p_bot2->y0 = y - 1;
            p_bot2->x1 = right_scr.x; p_bot2->y1 = right_scr.y - 1;
            p_bot2->x2 = x;           p_bot2->y2 = y + 1;
            p_bot2->x3 = right_scr.x; p_bot2->y3 = right_scr.y + 1;
            AddPrim(&current_frame->ot[base_ot], p_bot2);
        }
    }

    /* Check down neighbor (row + 1) */
    if (row + 1 < FIELD_ROWS && game_state.fieldTiles[row + 1].rows[col].value == ITEM_LUMBER) {
        SVECTOR down_pt = {
            .vx = col * FIELD_CELL_SIZE - 4050,
            .vy = -100,
            .vz = 1250 - (row + 1) * FIELD_CELL_SIZE
        };
        FarmScreenPoint down_scr;
        if (estate_point_project(&down_pt, &down_scr) != 0) {
            /* Upper rail */
            POLY_F4 *p_top = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top);
            setRGB0(p_top, 165, 120, 75);
            p_top->x0 = x - 2;          p_top->y0 = y - 9;
            p_top->x1 = down_scr.x - 2; p_top->y1 = down_scr.y - 9;
            p_top->x2 = x + 1;          p_top->y2 = y - 7;
            p_top->x3 = down_scr.x + 1; p_top->y3 = down_scr.y - 7;
            AddPrim(&current_frame->ot[base_ot], p_top);

            POLY_F4 *p_bot = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot);
            setRGB0(p_bot, 95, 60, 35);
            p_bot->x0 = x - 2;          p_bot->y0 = y - 7;
            p_bot->x1 = down_scr.x - 2; p_bot->y1 = down_scr.y - 7;
            p_bot->x2 = x + 1;          p_bot->y2 = y - 5;
            p_bot->x3 = down_scr.x + 1; p_bot->y3 = down_scr.y - 5;
            AddPrim(&current_frame->ot[base_ot], p_bot);

            /* Lower rail */
            POLY_F4 *p_top2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top2);
            setRGB0(p_top2, 155, 110, 68);
            p_top2->x0 = x - 2;          p_top2->y0 = y - 3;
            p_top2->x1 = down_scr.x - 2; p_top2->y1 = down_scr.y - 3;
            p_top2->x2 = x + 1;          p_top2->y2 = y - 1;
            p_top2->x3 = down_scr.x + 1; p_top2->y3 = down_scr.y - 1;
            AddPrim(&current_frame->ot[base_ot], p_top2);

            POLY_F4 *p_bot2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot2);
            setRGB0(p_bot2, 85, 52, 30);
            p_bot2->x0 = x - 2;          p_bot2->y0 = y - 1;
            p_bot2->x1 = down_scr.x - 2; p_bot2->y1 = down_scr.y - 1;
            p_bot2->x2 = x + 1;          p_bot2->y2 = y + 1;
            p_bot2->x3 = down_scr.x + 1; p_bot2->y3 = down_scr.y + 1;
            AddPrim(&current_frame->ot[base_ot], p_bot2);
        }
    }
}

extern void window_frame_draw_at(u_long *ot, s32 style, s16 x, s16 y, s16 w, s16 h);

extern s32 livestock_action_dispatch(FarmLivestock *action, s32 state);
extern s32 farm_actor_dispatch_state(FarmActor *object, s32 state);

void hm_gameplay_smart_bell_ring(void) {
    if (!active_options.smart_bell)
        return;
    s16 loc = farm_area.mode.place.location;
    if (loc == PLACE_FARM) {
        /* On farm: herd cows & sheep to barn entrance (1200, 0, 3200) */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_FARM) {
                item->actor.destination.vx = 1200;
                item->actor.destination.vy = 0;
                item->actor.destination.vz = 3200;
                item->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                livestock_action_dispatch(item, 18); /* LIVESTOCK_STATE_WALK */
            }
        }
        /* Herd chickens to coop entrance (-5800, 0, -6950) */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_FARM) {
                ch->actor.destination.vx = -5800;
                ch->actor.destination.vy = 0;
                ch->actor.destination.vz = -6950;
                ch->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                farm_actor_dispatch_state(&ch->actor, 3);
            }
        }
    } else if (loc == PLACE_5) {
        /* Inside barn: herd cows & sheep to exit door (1500, 0, -2900) */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_5) {
                item->actor.destination.vx = 1500;
                item->actor.destination.vy = 0;
                item->actor.destination.vz = -2900;
                item->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                livestock_action_dispatch(item, 18);
            }
        }
    } else if (loc == PLACE_4) {
        /* Inside coop: herd chickens to exit door (1850, 0, -1200) */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_4) {
                ch->actor.destination.vx = 1850;
                ch->actor.destination.vy = 0;
                ch->actor.destination.vz = -1200;
                ch->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                farm_actor_dispatch_state(&ch->actor, 3);
            }
        }
    }
}

void hm_gameplay_smart_bell_tick(void) {
    if (!active_options.smart_bell)
        return;
    s16 loc = farm_area.mode.place.location;
    if (loc == PLACE_FARM) {
        /* Check if any livestock reached barn door */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_FARM &&
                (item->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(item->actor.position.vx - 1200);
                s32 dz = abs(item->actor.position.vz - 3200);
                if (dx < 350 && dz < 350) {
                    item->actor.place.location = PLACE_5;
                    item->actor.position.vx = 1534;
                    item->actor.position.vz = -2000;
                    item->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
        /* Check if any chicken reached coop door */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_FARM &&
                (ch->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(ch->actor.position.vx - (-5800));
                s32 dz = abs(ch->actor.position.vz - (-6950));
                if (dx < 350 && dz < 350) {
                    ch->actor.place.location = PLACE_4;
                    ch->actor.position.vx = 1300;
                    ch->actor.position.vz = -1255;
                    ch->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
    } else if (loc == PLACE_5) {
        /* Check if any livestock reached barn exit */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_5 &&
                (item->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(item->actor.position.vx - 1500);
                s32 dz = abs(item->actor.position.vz - (-2900));
                if (dx < 350 && dz < 350) {
                    item->actor.place.location = PLACE_FARM;
                    item->actor.position.vx = 1240;
                    item->actor.position.vz = 2800;
                    item->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
    } else if (loc == PLACE_4) {
        /* Check if any chicken reached coop exit */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_4 &&
                (ch->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(ch->actor.position.vx - 1850);
                s32 dz = abs(ch->actor.position.vz - (-1200));
                if (dx < 350 && dz < 350) {
                    ch->actor.place.location = PLACE_FARM;
                    ch->actor.position.vx = -5400;
                    ch->actor.position.vz = -7100;
                    ch->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
    }
}

typedef struct {
    s16 season;
    s16 day;
    const char *name;
} hm_calendar_extra_entry;

static const hm_calendar_extra_entry calendar_birthdays[] = {
    /* Spring */
    {0, 10, "Saibara's Birthday"},
    {0, 15, "Elli's Birthday"},
    {0, 16, "Barley's Birthday"},
    {0, 18, "Lillia's Birthday"},
    {0, 28, "Greg's Birthday"},
    {0, 29, "Sasha's Birthday"},
    /* Summer */
    {1, 2, "Popuri's Birthday"},
    {1, 10, "Basil's Birthday"},
    {1, 15, "Tim's Birthday"},
    {1, 16, "Ann's Birthday"},
    {1, 21, "Kai's Birthday"},
    {1, 24, "Mayor's Birthday"},
    {1, 28, "Zack's Birthday"},
    /* Fall */
    {2, 1, "Gotz's Birthday"},
    {2, 4, "Stu's Birthday"},
    {2, 9, "Manna's Birthday"},
    {2, 14, "Karen's Birthday"},
    {2, 16, "Doctor's Birthday"},
    {2, 19, "Carter's Birthday"},
    {2, 26, "Rick's Birthday"},
    /* Winter */
    {3, 1, "Kano's Birthday"},
    {3, 5, "Gray's Birthday"},
    {3, 10, "Doug's Birthday"},
    {3, 12, "Ellen's Birthday"},
    {3, 19, "Mary's Birthday"},
    {3, 21, "Cliff's Birthday"},
};

int hm_gameplay_calendar_has_event(s16 season, s16 day) {
    if (!active_options.calendar_events)
        return 0;
    /* Check villager birthdays */
    for (size_t i = 0; i < sizeof(calendar_birthdays) / sizeof(calendar_birthdays[0]); ++i) {
        if (calendar_birthdays[i].season == season && calendar_birthdays[i].day == day)
            return 1;
    }
    /* Check player birthday */
    if (game_state.player.birthday.season == season && game_state.player.birthday.day == day)
        return 1;
    /* Check livestock maturation */
    for (int i = 0; i < MAX_LIVESTOCK; ++i) {
        FarmLivestock *item = &game_state.animals.livestock[i];
        if (item->actor.kind != ITEM_NOTHING && item->growthDays < LIVESTOCK_ADULT_DAYS) {
            s32 mat_day = game_state.progress.clock.date.day + (LIVESTOCK_ADULT_DAYS - item->growthDays);
            if (mat_day == day && game_state.progress.clock.date.season == season)
                return 1;
        }
    }
    return 0;
}

const char *hm_gameplay_calendar_event_name(s16 season, s16 day) {
    if (!active_options.calendar_events)
        return NULL;
    for (size_t i = 0; i < sizeof(calendar_birthdays) / sizeof(calendar_birthdays[0]); ++i) {
        if (calendar_birthdays[i].season == season && calendar_birthdays[i].day == day)
            return calendar_birthdays[i].name;
    }
    if (game_state.player.birthday.season == season && game_state.player.birthday.day == day)
        return "Player's Birthday";
    for (int i = 0; i < MAX_LIVESTOCK; ++i) {
        FarmLivestock *item = &game_state.animals.livestock[i];
        if (item->actor.kind != ITEM_NOTHING && item->growthDays < LIVESTOCK_ADULT_DAYS) {
            s32 mat_day = game_state.progress.clock.date.day + (LIVESTOCK_ADULT_DAYS - item->growthDays);
            if (mat_day == day && game_state.progress.clock.date.season == season)
                return "Livestock Matures";
        }
    }
    return NULL;
}

void hm_gameplay_horse_safety_check(void) {
    /* If roaming horse is OFF, horse must only be on farm (PLACE_FARM) or stable (PLACE_3) */
    if (!active_options.roaming_horse) {
        FarmPet *horse = &game_state.animals.horse;
        if (horse->actor.kind != ITEM_NOTHING &&
            horse->actor.place.location != PLACE_FARM &&
            horse->actor.place.location != PLACE_3) {
            horse->actor.place.location = PLACE_3;
            horse->actor.position.vx = -173;
            horse->actor.position.vy = 0;
            horse->actor.position.vz = -400;
            if (game_state.player.stateFlags & PLAYER_FLAG_RIDING) {
                game_state.player.stateFlags &= ~(PLAYER_FLAG_RIDING_COW | PLAYER_FLAG_RIDING);
            }
        }
    }
}

extern s32 greenhouse_world_to_cell(VECTOR *position, s16 *column, s16 *row);

/* primBuffer belongs to overlay backups. Widescreen relocates only primStart,
 * so compare the cursor against the active arena and its actual capacity. */
static int hm_gameplay_prim_room(size_t bytes) {
    if (!current_frame || !current_frame->primStart || !current_frame->primCursor.bytes)
        return 0;
    uintptr_t start = (uintptr_t)current_frame->primStart;
    uintptr_t cursor = (uintptr_t)current_frame->primCursor.bytes;
    size_t capacity = hm_game_prim_capacity();
    if (cursor < start || cursor - start > capacity)
        return 0;
    return bytes <= capacity - (cursor - start);
}

void hm_gameplay_draw_tool_charge_gauge(const void *player_ptr, s32 depth, s16 screen_x, s16 screen_y) {
    if (!active_options.tool_charge_gauge)
        return;
    const FarmPlayer *player = player_ptr ? (const FarmPlayer *)player_ptr : &game_state.player;
    if (!player)
        return;

    /* Sickle, Hoe, Ax, Hammer, Watering Can (0x16 to 0x1E) */
    if (player->mode < 0x16 || player->mode > 0x1E || ((player->mode - 0x16) & 1) != 0)
        return;
    if (player->toolCharge <= 0)
        return;

    int max_level = player->held[HELD_TOOL].modifier;
    if (max_level < 1)
        max_level = 1;
    if (max_level > 5)
        max_level = 5;

    int max_charge = max_level * 20;
    int cur_charge = player->toolCharge;
    if (cur_charge > max_charge)
        cur_charge = max_charge;

    int cur_level = cur_charge / 20;

    s16 bar_w = 32;
    s16 bar_h = 2;
    s16 bar_x = screen_x - (bar_w / 2);
    s16 bar_y = screen_y - 44;

    s32 ot_idx = (depth >> 2) - 2;
    if (ot_idx < 1)
        ot_idx = 1;
    if (ot_idx >= FRAME_OT_DEPTH)
        ot_idx = FRAME_OT_DEPTH - 1;

    if (!hm_gameplay_prim_room(3 * sizeof(POLY_F4) + 5 * sizeof(LINE_F2)))
        return;

    /* Outer dark border */
    POLY_F4 *border = (POLY_F4 *)current_frame->primCursor.bytes;
    current_frame->primCursor.bytes += sizeof(POLY_F4);
    SetPolyF4(border);
    setRGB0(border, 16, 16, 20);
    border->x0 = bar_x - 1;         border->y0 = bar_y - 1;
    border->x1 = bar_x + bar_w + 1; border->y1 = bar_y - 1;
    border->x2 = bar_x - 1;         border->y2 = bar_y + bar_h + 1;
    border->x3 = bar_x + bar_w + 1; border->y3 = bar_y + bar_h + 1;
    AddPrim(&current_frame->ot[ot_idx], border);

    /* Background */
    POLY_F4 *bg = (POLY_F4 *)current_frame->primCursor.bytes;
    current_frame->primCursor.bytes += sizeof(POLY_F4);
    SetPolyF4(bg);
    setRGB0(bg, 40, 42, 50);
    bg->x0 = bar_x;         bg->y0 = bar_y;
    bg->x1 = bar_x + bar_w; bg->y1 = bar_y;
    bg->x2 = bar_x;         bg->y2 = bar_y + bar_h;
    bg->x3 = bar_x + bar_w; bg->y3 = bar_y + bar_h;
    AddPrim(&current_frame->ot[ot_idx], bg);

    /* Fill progress */
    s16 fill_w = (s16)((cur_charge * bar_w) / max_charge);
    if (fill_w > bar_w)
        fill_w = bar_w;

    u8 r, g, b;
    switch (cur_level) {
    case 0:
        r = 180; g = 190; b = 205; /* Base / Iron */
        break;
    case 1:
        r = 220; g = 130; b = 40;  /* Copper */
        break;
    case 2:
        r = 210; g = 230; b = 255; /* Silver */
        break;
    case 3:
        r = 255; g = 215; b = 20;  /* Gold */
        break;
    default:
        r = 50;  g = 185; b = 255; /* Mystrile */
        break;
    }

    if (cur_charge >= max_charge && (game_state.frameCount & 4) != 0) {
        r = 255; g = 255; b = 255;
    }

    if (fill_w > 0) {
        POLY_F4 *fill = (POLY_F4 *)current_frame->primCursor.bytes;
        current_frame->primCursor.bytes += sizeof(POLY_F4);
        SetPolyF4(fill);
        setRGB0(fill, r, g, b);
        fill->x0 = bar_x;          fill->y0 = bar_y;
        fill->x1 = bar_x + fill_w; fill->y1 = bar_y;
        fill->x2 = bar_x;          fill->y2 = bar_y + bar_h;
        fill->x3 = bar_x + fill_w; fill->y3 = bar_y + bar_h;
        AddPrim(&current_frame->ot[ot_idx], fill);
    }

    /* Notch dividers */
    for (int i = 1; i < max_level; ++i) {
        s16 notch_x = bar_x + (s16)((i * bar_w) / max_level);
        LINE_F2 *notch = (LINE_F2 *)current_frame->primCursor.bytes;
        current_frame->primCursor.bytes += sizeof(LINE_F2);
        setlen(notch, 3);
        notch->code = 0x40;
        setRGB0(notch, 25, 25, 30);
        notch->x0 = notch_x; notch->y0 = bar_y;
        notch->x1 = notch_x; notch->y1 = bar_y + bar_h - 1;
        AddPrim(&current_frame->ot[ot_idx], notch);
    }
}

void hm_gameplay_draw_tile_targeting(const void *player_ptr, s32 depth) {
    if (!active_options.tile_targeting)
        return;
    const FarmPlayer *player = player_ptr ? (const FarmPlayer *)player_ptr : &game_state.player;
    if (!player)
        return;
    if (farm_area.mode.eventMode != FARM_MODE_IDLE)
        return;

    if (game_state.globalFlags & (GLOBAL_FLAG_MENU_OPEN | GLOBAL_FLAG_TRANSITION |
                                  GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_CAMERA_BUSY |
                                  GLOBAL_FLAG_DEMO_START | GLOBAL_FLAG_INPUT_REPLAY))
        return;
    if (game_state.requests.state == FARM_REQUEST_RUN)
        return;
    if (game_state.system.menuState != MENU_STATE_IDLE)
        return;
    if (player->stateFlags & (PLAYER_FLAG_AIRBORNE | PLAYER_FLAG_ASLEEP |
                             PLAYER_FLAG_SCRIPTED | PLAYER_FLAG_EVENT_SPRITE |
                             PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_AUTO_WALK |
                             PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_RIDING))
        return;

    s16 place = player->place.location ? player->place.location : farm_area.mode.place.location;
    if (!place && farm_area.mode.data)
        place = farm_area.mode.data->head.place.location;
    u16 place_val = player->place.value ? player->place.value : farm_area.mode.place.value;
    if (!place_val && farm_area.mode.data)
        place_val = farm_area.mode.data->head.place.value;
    int is_farm = (place == 1);
    int is_greenhouse = (place == 6);
    int is_mine = (place_val == 0x37 || place_val == 0x43);
    if (!is_farm && !is_greenhouse && !is_mine)
        return;

    static const struct { s16 x; s16 z; } steps[8] = {
        {   0, -400 }, /* DOWN */
        {-400,    0 }, /* LEFT */
        {   0,  400 }, /* UP */
        { 400,    0 }, /* RIGHT */
        {   0, -300 },
        {-300,    0 },
        {   0,  300 },
        { 300,    0 },
    };

    u8 facing = player->facing38 & 7;
    VECTOR target_pos;
    target_pos.vx = player->position.vx + steps[facing].x;
    target_pos.vy = player->position.vy;
    target_pos.vz = player->position.vz + steps[facing].z;

    s16 col = -1, row = -1;
    if (is_farm) {
        field_world_to_cell(&target_pos, &col, &row);
        if (col < 0 || col >= FIELD_COLS || row < 0 || row >= FIELD_ROWS)
            return;
    } else if (is_greenhouse) {
        greenhouse_world_to_cell(&target_pos, &col, &row);
        if (col < 0 || col >= GREENHOUSE_SIZE || row < 0 || row >= GREENHOUSE_SIZE)
            return;
    } else {
        int mine_col, mine_row;
        if (!hm_mine_grid_cell(target_pos.vx, target_pos.vz, &mine_col, &mine_row))
            return;
        col = (s16)mine_col;
        row = (s16)mine_row;
    }

    s32 x0, x1, z0, z1;
    if (is_farm) {
        x0 = (s32)col * FIELD_CELL_SIZE - 4400;
        x1 = (s32)(col + 1) * FIELD_CELL_SIZE - 4400;
        z0 = 1600 - (s32)row * FIELD_CELL_SIZE;
        z1 = 1600 - (s32)(row + 1) * FIELD_CELL_SIZE;
    } else if (is_greenhouse) {
        x0 = (s32)col * FIELD_CELL_SIZE - 1620;
        x1 = (s32)(col + 1) * FIELD_CELL_SIZE - 1620;
        z0 = 1220 - (s32)row * FIELD_CELL_SIZE;
        z1 = 1220 - (s32)(row + 1) * FIELD_CELL_SIZE;
    } else {
        x0 = (s32)col * 400 - 2000;
        x1 = (s32)(col + 1) * 400 - 2000;
        z0 = 2000 - (s32)row * 400;
        z1 = 2000 - (s32)(row + 1) * 400;
    }

    s16 ground_y = (s16)(player->position.vy - 4);
    SVECTOR v0 = { (s16)x0, ground_y, (s16)z0, 0 };
    SVECTOR v1 = { (s16)x1, ground_y, (s16)z0, 0 };
    SVECTOR v2 = { (s16)x0, ground_y, (s16)z1, 0 };
    SVECTOR v3 = { (s16)x1, ground_y, (s16)z1, 0 };

    FarmScreenXY s0, s1, s2, s3;
    s32 d0 = gte_project_vertex(&s0, &v0);
    s32 d1 = gte_project_vertex(&s1, &v1);
    s32 d2 = gte_project_vertex(&s2, &v2);
    s32 d3 = gte_project_vertex(&s3, &v3);

    if (d0 <= 0 || d1 <= 0 || d2 <= 0 || d3 <= 0)
        return;

    s32 avg_depth = (d0 + d1 + d2 + d3) >> 2;
    /* The OT is drawn back to front. A larger slot hides the marker under
     * the terrain; bias towards the camera instead. */
    s32 ot_idx = (avg_depth >> 2) - 1;
    if (ot_idx < 1)
        ot_idx = 1;
    if (ot_idx >= FRAME_OT_DEPTH)
        ot_idx = FRAME_OT_DEPTH - 1;

    if (!hm_gameplay_prim_room(sizeof(DR_TPAGE) + sizeof(POLY_F4) + 4 * sizeof(LINE_F2)))
        return;

    /* Ensure semi-transparency blend mode is active (ABR = 0: 50% B + 50% F) */
    DR_TPAGE *tpage = (DR_TPAGE *)current_frame->primCursor.bytes;
    current_frame->primCursor.bytes += sizeof(DR_TPAGE);
    setDrawTPage(tpage, 0, 1, GetTPage(0, 0, 0, 0));

    /* Semi-transparent soft warm quad fill */
    POLY_F4 *fill = (POLY_F4 *)current_frame->primCursor.bytes;
    current_frame->primCursor.bytes += sizeof(POLY_F4);
    SetPolyF4(fill);
    SetSemiTrans(fill, 1);
    setRGB0(fill, 35, 130, 215);
    fill->x0 = s0.x; fill->y0 = s0.y;
    fill->x1 = s1.x; fill->y1 = s1.y;
    fill->x2 = s2.x; fill->y2 = s2.y;
    fill->x3 = s3.x; fill->y3 = s3.y;

    /* 4 Border lines */
    struct { s16 ax; s16 ay; s16 bx; s16 by; } edges[4] = {
        { s0.x, s0.y, s1.x, s1.y },
        { s1.x, s1.y, s3.x, s3.y },
        { s3.x, s3.y, s2.x, s2.y },
        { s2.x, s2.y, s0.x, s0.y },
    };

    /* Add to OT in order: lines first, then fill, then tpage.
     * Since AddPrim prepends to the linked list head, GPU execution order is:
     * tpage (sets ABR=0) -> fill (semi-transparent quad) -> lines (crisp perimeter borders). */
    for (int i = 0; i < 4; ++i) {
        LINE_F2 *line = (LINE_F2 *)current_frame->primCursor.bytes;
        current_frame->primCursor.bytes += sizeof(LINE_F2);
        setlen(line, 3);
        line->code = 0x40;
        setRGB0(line, 140, 220, 255);
        line->x0 = edges[i].ax; line->y0 = edges[i].ay;
        line->x1 = edges[i].bx; line->y1 = edges[i].by;
        AddPrim(&current_frame->ot[ot_idx], line);
    }
    AddPrim(&current_frame->ot[ot_idx], fill);
    AddPrim(&current_frame->ot[ot_idx], tpage);
}
