#include "platform/gameplay_options.h"
#include "enhance/gameplay/gameplay.h"
#include "platform/user_data.h"
#include "farm_state.h"
#include "main.h"
#include "psx/gpu_links.h"
#include "event_flags.h"
#include "psx/vram.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int fatigue_render_checks(void) {
    static u32 ot[64], buffer[256];
    static FrameState frame;
    FrameState *saved_frame = current_frame;
    frame.otTags[0] = ot;
    frame.workBase = (u8 *)buffer;
    frame.workSize = sizeof(buffer);
    current_frame = &frame;
    hm_gameplay_options opt;
    hm_gameplay_defaults(&opt);
    hm_gameplay_set_active(&opt);
    ResetGraph(0);
    hm_vram_reset();
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    ClearOTagR(ot, 64);
    GsSetWorkBase(buffer);
    hm_gameplay_draw_fatigue_bar(14, 16, 50);
    CHECK(GsGetWorkBase() == buffer); /* The OFF path emits nothing. */
    opt.stamina_hud = 1;
    hm_gameplay_set_active(&opt);
    game_state.player.fatigue = 50;
    hm_gameplay_draw_fatigue_bar(14, 16, game_state.player.fatigue);
    CHECK(GsGetWorkBase() != buffer && game_state.player.fatigue == 50);
    DrawOTag(ot + 63);
    CHECK(hm_vram_pixel(50, 31) == ((0xc8 >> 3) | ((0x99 >> 3) << 5) | ((0x40 >> 3) << 10)));
    CHECK(hm_vram_pixel(80, 31) == ((0x80 >> 3) | ((0x6c >> 3) << 5) | ((0x54 >> 3) << 10)));
    hm_gameplay_defaults(&opt);
    hm_gameplay_set_active(&opt);
    current_frame = saved_frame;
    return 0;
}

static int tile_render_checks(void) {
    static u32 ot[FRAME_OT_DEPTH], buffer[FRAME_PRIM_SIZE / 4];
    static FrameState frame;
    static FarmTmdQuad ground;
    FrameState *saved_frame = current_frame;
    frame.ot = ot;
    frame.primBuffer = (u8 *)buffer;
    frame.primStart = frame.primBuffer;
    frame.primSize = sizeof(buffer);
    current_frame = &frame;
    InitGeom();
    MATRIX camera = {{{4096, 0, 0}, {0, 0, -4096}, {0, 4096, 0}}, {0, 0, 10000}};
    SetRotMatrix(&camera);
    SetTransMatrix(&camera);
    SetGeomOffset(160, 120);
    SetGeomScreen(256);
    memset(&game_state.player, 0, sizeof(game_state.player));
    game_state.globalFlags = 0;
    farm_area.mode.eventMode = FARM_MODE_IDLE;
    game_state.player.place.location = 1;
    farm_area.mode.place.location = 1;
    for (int y = 0; y < FIELD_ROWS; ++y)
        for (int x = 0; x < FIELD_COLS; ++x)
            farm_area.mode.quads[y][x] = &ground;
    hm_gameplay_options opt;
    hm_gameplay_defaults(&opt);
    hm_gameplay_set_active(&opt);
    frame.primCursor.bytes = frame.primBuffer;
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes == frame.primBuffer); /* OFF is inert. */
    opt.tile_targeting = 1;
    hm_gameplay_set_active(&opt);
    ClearOTagR(ot, FRAME_OT_DEPTH);
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes > frame.primBuffer);
    /* Projected ground depth is ~2500, slot 625. The marker must be in
     * front of the ground, not one slot behind it. */
    CHECK(hm_gpu_link_decode(ot[623]) != &ot[622]);
    /* Render an opaque terrain patch at its normal depth. The cyan fill
     * must survive the back-to-front traversal above that patch. */
    POLY_F4 terrain = {0};
    SetPolyF4(&terrain);
    setRGB0(&terrain, 128, 0, 0);
    terrain.x0 = terrain.x2 = 150;
    terrain.x1 = terrain.x3 = 180;
    terrain.y0 = terrain.y1 = 120;
    terrain.y2 = terrain.y3 = 150;
    AddPrim(&ot[624], &terrain);
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    DrawOTag(ot + FRAME_OT_DEPTH - 1);
    CHECK((hm_vram_pixel(165, 135) & 0x7fe0) != 0);
    /* Overlay backup storage and active primitive storage can differ. The
     * old comparison against primBuffer rejected this valid relocated arena. */
    static u32 relocated[FRAME_PRIM_SIZE / 4];
    frame.primStart = (u8 *)relocated;
    frame.primCursor.bytes = frame.primStart;
    ClearOTagR(ot, FRAME_OT_DEPTH);
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes > frame.primStart);
    opt.tool_charge_gauge = 1;
    hm_gameplay_set_active(&opt);
    game_state.player.mode = 0x16;
    game_state.player.toolCharge = 10;
    game_state.player.held[HELD_TOOL].modifier = 1;
    frame.primCursor.bytes = frame.primStart;
    ClearOTagR(ot, FRAME_OT_DEPTH);
    hm_gameplay_draw_tool_charge_gauge(&game_state.player, 2500, 160, 120);
    CHECK(frame.primCursor.bytes > frame.primStart);
    frame.primCursor.bytes = frame.primStart + frame.primSize - 4;
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes == frame.primStart + frame.primSize - 4);
    hm_gameplay_draw_tool_charge_gauge(&game_state.player, 2500, 160, 120);
    CHECK(frame.primCursor.bytes == frame.primStart + frame.primSize - 4);
    frame.primStart = frame.primBuffer;
    const u32 hidden[] = {PLAYER_FLAG_SCRIPTED, PLAYER_FLAG_EVENT_SPRITE,
                         PLAYER_FLAG_NO_INPUT, PLAYER_FLAG_AUTO_WALK,
                         PLAYER_FLAG_AIRBORNE, PLAYER_FLAG_ASLEEP,
                         PLAYER_FLAG_SCENE_CHANGE, PLAYER_FLAG_RIDING};
    for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i) {
        game_state.player.stateFlags = hidden[i];
        frame.primCursor.bytes = frame.primBuffer;
        hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
        CHECK(frame.primCursor.bytes == frame.primBuffer);
    }
    game_state.player.stateFlags = 0;
    farm_area.mode.eventMode = FARM_MODE_IDLE + 1;
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes == frame.primBuffer);
    farm_area.mode.eventMode = FARM_MODE_IDLE;
    const u32 blocked[] = {GLOBAL_FLAG_MENU_OPEN, GLOBAL_FLAG_TRANSITION, GLOBAL_FLAG_PAUSED,
                           GLOBAL_FLAG_CAMERA_BUSY, GLOBAL_FLAG_DEMO_START, GLOBAL_FLAG_INPUT_REPLAY};
    for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); ++i) {
        game_state.globalFlags = blocked[i];
        hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
        CHECK(frame.primCursor.bytes == frame.primBuffer);
    }
    game_state.globalFlags = 0;

    /* Cutscene, event, and dialogue suppression */
    game_state.player.stateFlags = PLAYER_FLAG_SCRIPTED;
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes == frame.primBuffer);
    game_state.player.stateFlags = 0;

    game_state.requests.state = FARM_REQUEST_RUN;
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes == frame.primBuffer);
    game_state.requests.state = 0;

    game_state.system.menuState = MENU_STATE_OPEN;
    hm_gameplay_draw_tile_targeting(&game_state.player, 2500);
    CHECK(frame.primCursor.bytes == frame.primBuffer);
    game_state.system.menuState = MENU_STATE_IDLE;
    current_frame = saved_frame;
    hm_gameplay_defaults(&opt);
    hm_gameplay_set_active(&opt);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(fatigue_render_checks() == 0);
    CHECK(tile_render_checks() == 0);
    CHECK(argc == 2 && SDL_Init(0));
    char *root = NULL;
    CHECK(SDL_asprintf(&root, "%s-%llu", argv[1], (unsigned long long)SDL_GetPerformanceCounter()) >= 0);
    CHECK(hm_user_data_start_at(root));

    hm_gameplay_options opt;
    hm_gameplay_defaults(&opt);

    CHECK(opt.walk_through_crops == 0);
    CHECK(opt.item_stacking == 0);
    CHECK(opt.durable_fences == 0);
    CHECK(opt.connect_fences == 0);
    CHECK(opt.wild_dog == 1);
    CHECK(opt.smart_bell == 0);
    CHECK(opt.stamina_hud == 0);
    CHECK(opt.calendar_events == 0);
    CHECK(opt.roaming_horse == 0);
    CHECK(opt.fast_text == 0);
    CHECK(opt.tool_charge_gauge == 0);
    CHECK(opt.tile_targeting == 0);
    CHECK(opt.npc_bonds == 0);
    CHECK(opt.grass_spread == 0);
    CHECK(opt.slow_farm_time == 0);
    CHECK(opt.shipping_bin == 0);

    CHECK(hm_gameplay_walk_through_crops() == 0);
    CHECK(hm_gameplay_item_stacking() == 0);
    CHECK(hm_gameplay_durable_fences() == 0);
    CHECK(hm_gameplay_connect_fences() == 0);
    CHECK(hm_gameplay_wild_dog_enabled() == 1);
    CHECK(hm_gameplay_smart_bell() == 0);
    CHECK(hm_gameplay_stamina_hud() == 0);
    CHECK(hm_gameplay_fatigue_fill(-1) == 0);
    CHECK(hm_gameplay_fatigue_fill(0) == 0);
    CHECK(hm_gameplay_fatigue_fill(1) == 1);
    CHECK(hm_gameplay_fatigue_fill(50) == 29);
    CHECK(hm_gameplay_fatigue_fill(99) == 58);
    CHECK(hm_gameplay_fatigue_fill(100) == 59);
    CHECK(hm_gameplay_fatigue_fill(101) == 59);
    CHECK(hm_gameplay_calendar_events() == 0);
    CHECK(hm_gameplay_roaming_horse() == 0);
    CHECK(hm_gameplay_fast_text() == 0);
    CHECK(hm_gameplay_tool_charge_gauge() == 0);
    CHECK(hm_gameplay_tile_targeting() == 0);
    CHECK(hm_gameplay_npc_bonds() == 0);
    CHECK(hm_gameplay_grass_spread() == 0);
    CHECK(hm_gameplay_slow_farm_time() == 0);
    CHECK(hm_gameplay_shipping_bin() == 0);
    CHECK(hm_gameplay_grass_spread_due(1, 2, 3, 4, 5) ==
          hm_gameplay_grass_spread_due(1, 2, 3, 4, 5));
    CHECK(hm_gameplay_grass_spread_direction(1, 2, 3, 4, 5) >= 0 &&
          hm_gameplay_grass_spread_direction(1, 2, 3, 4, 5) < 4);

    char val[64];
    hm_gameplay_row_value(&opt, 0, val, sizeof(val));
    CHECK(!strcmp(val, "Off"));
    hm_gameplay_row_value(&opt, 4, val, sizeof(val));
    CHECK(!strcmp(val, "Enabled"));

    CHECK(hm_gameplay_change(&opt, 0, 1) == 1);
    CHECK(opt.walk_through_crops == 1);
    CHECK(hm_gameplay_walk_through_crops() == 1);

    CHECK(hm_gameplay_change(&opt, 4, -1) == 1);
    CHECK(opt.wild_dog == 0);
    CHECK(hm_gameplay_wild_dog_enabled() == 0);

    CHECK(hm_gameplay_change(&opt, 5, 1) == 1);
    CHECK(opt.smart_bell == 1);
    CHECK(hm_gameplay_smart_bell() == 1);

    CHECK(hm_gameplay_change(&opt, 6, 1) == 1);
    CHECK(opt.stamina_hud == 1);
    CHECK(hm_gameplay_stamina_hud() == 1);

    CHECK(hm_gameplay_change(&opt, 7, 1) == 1);
    CHECK(opt.calendar_events == 1);
    CHECK(hm_gameplay_calendar_events() == 1);

    CHECK(hm_gameplay_change(&opt, 8, 1) == 1);
    CHECK(opt.roaming_horse == 1);
    CHECK(hm_gameplay_roaming_horse() == 1);

    CHECK(hm_gameplay_change(&opt, 9, 1) == 1);
    CHECK(opt.tool_charge_gauge == 1);
    CHECK(hm_gameplay_tool_charge_gauge() == 1);

    CHECK(hm_gameplay_change(&opt, 10, 1) == 1);
    CHECK(opt.tile_targeting == 1);
    CHECK(hm_gameplay_tile_targeting() == 1);

    CHECK(hm_gameplay_change(&opt, 11, 1) == 1);
    CHECK(opt.npc_bonds == 1);
    CHECK(hm_gameplay_npc_bonds() == 1);
    hm_gameplay_row_value(&opt, 11, val, sizeof(val));
    CHECK(!strcmp(val, "On"));

    int extra = 12;
#ifdef HMBTN_ENH_GRASS_SPREAD
    CHECK(hm_gameplay_change(&opt, extra, 1) == 1);
    CHECK(opt.grass_spread == 1 && hm_gameplay_grass_spread() == 1);
    hm_gameplay_row_value(&opt, extra, val, sizeof(val));
    CHECK(!strcmp(val, "On"));
    int spread_due_count = 0;
    int spread_direction_mask = 0;
    for (int day = 0; day < 28; ++day) {
        for (int row = 0; row < 25; ++row) {
            for (int column = 0; column < 45; ++column) {
                int due = hm_gameplay_grass_spread_due(1, 0, day, row, column);
                int direction = hm_gameplay_grass_spread_direction(1, 0, day, row, column);
                CHECK(due == hm_gameplay_grass_spread_due(1, 0, day, row, column));
                CHECK(direction >= 0 && direction < 4);
                spread_due_count += due;
                if (due)
                    spread_direction_mask |= 1 << direction;
            }
        }
    }
    CHECK(spread_due_count > 0 && spread_due_count < 28 * 25 * 45);
    CHECK(spread_direction_mask == 0x0F);
    ++extra;
#endif
#ifdef HMBTN_ENH_SLOW_FARM_TIME
    CHECK(hm_gameplay_change(&opt, extra, 1) == 1);
    CHECK(opt.slow_farm_time == 1 && hm_gameplay_slow_farm_time() == 1);
    ++extra;
#endif
#ifdef HMBTN_ENH_SHIPPING_BIN
    CHECK(hm_gameplay_change(&opt, extra, 1) == 1);
    CHECK(opt.shipping_bin == 1 && hm_gameplay_shipping_bin() == 1);
    hm_gameplay_shipping_clear();
    hm_gameplay_shipping_record(ITEM_TURNIP);
    CHECK(hm_gameplay_shipping_count(ITEM_TURNIP) == 1);
    hm_gameplay_shipping_forget(ITEM_TURNIP);
    CHECK(hm_gameplay_shipping_count(ITEM_TURNIP) == 0);
    hm_gameplay_shipping_request(HM_SHIPPING_REQUEST_FIXED_BIN);
    CHECK(hm_gameplay_shipping_take_request() == HM_SHIPPING_REQUEST_FIXED_BIN);
    hm_gameplay_shipping_request(HM_SHIPPING_REQUEST_PORTABLE_BASKET);
    CHECK(hm_gameplay_shipping_take_request() == HM_SHIPPING_REQUEST_PORTABLE_BASKET);
    CHECK(hm_gameplay_shipping_take_request() == 0);
#endif

    /* Test save & load persistence */
    CHECK(hm_gameplay_save(&opt) == 1);

    hm_gameplay_options loaded;
    hm_gameplay_load(&loaded);
    CHECK(loaded.walk_through_crops == 1);
    CHECK(loaded.wild_dog == 0);
    CHECK(loaded.smart_bell == 1);
    CHECK(loaded.stamina_hud == 1);
    CHECK(loaded.calendar_events == 1);
    CHECK(loaded.roaming_horse == 1);
    CHECK(loaded.tool_charge_gauge == 1);
    CHECK(loaded.tile_targeting == 1);
    CHECK(loaded.npc_bonds == 1);
#ifdef HMBTN_ENH_GRASS_SPREAD
    CHECK(loaded.grass_spread == 1);
#endif
#ifdef HMBTN_ENH_SLOW_FARM_TIME
    CHECK(loaded.slow_farm_time == 1);
#endif
#ifdef HMBTN_ENH_SHIPPING_BIN
    CHECK(loaded.shipping_bin == 1);
#endif

    hm_gameplay_defaults(&opt);
    hm_gameplay_set_active(&opt);
    CHECK(hm_gameplay_walk_through_crops() == 0);
    CHECK(hm_gameplay_wild_dog_enabled() == 1);
    CHECK(hm_gameplay_smart_bell() == 0);
    CHECK(hm_gameplay_stamina_hud() == 0);
    CHECK(hm_gameplay_calendar_events() == 0);
    CHECK(hm_gameplay_roaming_horse() == 0);
    CHECK(hm_gameplay_fast_text() == 0);
    CHECK(hm_gameplay_tool_charge_gauge() == 0);
    CHECK(hm_gameplay_tile_targeting() == 0);
    CHECK(hm_gameplay_npc_bonds() == 0);
    CHECK(hm_gameplay_grass_spread() == 0);
    CHECK(hm_gameplay_slow_farm_time() == 0);
    CHECK(hm_gameplay_shipping_bin() == 0);

    SDL_free(root);
    SDL_Quit();
    return 0;
}
