/* Mutate every registered farm object, then verify cold overlay restoration.
 * No gameplay, assets, disc, input replay or owner saves are used. */
#include "animals.h"
#include "farm_state.h"
#include "overlay/farm_state.h"
#include "overlay/game_lifecycle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static FarmState resident_game;
static FarmLoadArea resident_area;
static int mutable_value;
static const int constant_value;
static const int *mutable_pointer;
static int *const constant_pointer = &mutable_value;
static const int constant_array[2];
static const int *pointer_array[2];
static const hm_overlay_span type_cases[] = {
    HM_OVERLAY_OBJECT(mutable_value),   HM_OVERLAY_OBJECT(constant_value),
    HM_OVERLAY_OBJECT(mutable_pointer), HM_OVERLAY_OBJECT(constant_pointer),
    HM_OVERLAY_OBJECT(constant_array),  HM_OVERLAY_OBJECT(pointer_array)};
int main(void) {
    CHECK(type_cases[0].bytes == sizeof mutable_value && type_cases[1].bytes == 0);
    CHECK(type_cases[2].bytes == sizeof mutable_pointer && type_cases[3].bytes == 0);
    CHECK(type_cases[4].bytes == 0 && type_cases[5].bytes == sizeof pointer_array);
    /* Function-local cue tables escape only to this read-only consumer. Test
     * both location branches with storage in the read-only data segment. */
    static const FarmCueStep cue[] = {{0, 1}, {-1, 0}};
    for (int location = 0; location < 2; ++location) {
        FarmActor actor = {0};
        actor.place.location = location;
        actor.sprite.flags = 0x300;
        s16 step = 1, timer = 0;
        CHECK(farm_cue_step_run(&actor, (FarmCueStep *)cue, &step, -1, &timer) == 1);
        CHECK(step == 2 && timer == 0 && cue[0].frames == 1 && cue[1].pose == -1);
    }
    hm_game_overlays_init();
    size_t count;
    const hm_overlay_inventory *parts = hm_farm_overlay_parts(&count);
    CHECK(count == 45);
    size_t total = 0, objects = 0, immutable = 0;
    for (size_t i = 0; i < count; ++i)
        for (size_t j = 0; j < parts[i].count; ++j) {
            total += parts[i].spans[j].bytes;
            objects += parts[i].spans[j].bytes != 0;
            immutable += parts[i].spans[j].bytes == 0;
        }
    CHECK(total && objects == 679 && immutable == 26);
    u8 *pristine = malloc(total);
    CHECK(pristine);
    size_t offset = 0;
    for (size_t i = 0; i < count; ++i)
        for (size_t j = 0; j < parts[i].count; ++j) {
            const hm_overlay_span *span = &parts[i].spans[j];
            memcpy(pristine + offset, span->address, span->bytes);
            offset += span->bytes;
        }
    game_state.player.stamina = 73;
    farm_area.mode.area = 77;
    resident_game = game_state;
    resident_area = farm_area;
    for (int visit = 0; visit < 4; ++visit) {
        CHECK(hm_overlay_select(27, 0) == 0);
        CHECK(hm_overlay_select(15, 1) == 0);
        CHECK(hm_overlay_select(11, 0) == 0 && hm_overlay_active(0) == 11);
        CHECK(!memcmp(&game_state, &resident_game, sizeof game_state));
        CHECK(!memcmp(&farm_area, &resident_area, sizeof farm_area));
        offset = 0;
        for (size_t i = 0; i < count; ++i)
            for (size_t j = 0; j < parts[i].count; ++j) {
                const hm_overlay_span *span = &parts[i].spans[j];
                if (span->bytes) {
                    CHECK(!memcmp(pristine + offset, span->address, span->bytes));
                    memset(span->address, 0xa5 + visit, span->bytes);
                }
                offset += span->bytes;
            }
        hm_game_overlays_init(); /* Must never capture live mutations as pristine. */
        CHECK(hm_overlay_select(15, 1) == 0 && hm_overlay_active(0) == 11);
        for (size_t i = 0; i < count; ++i)
            for (size_t j = 0; j < parts[i].count; ++j) {
                const hm_overlay_span *span = &parts[i].spans[j];
                const u8 *bytes = span->address;
                for (size_t k = 0; k < span->bytes; ++k)
                    CHECK(bytes[k] == 0xa5 + visit); /* High save overlay keeps the farm resident. */
            }
    }
    CHECK(hm_overlay_select(11, 0) == 0); /* Restore after the final mutation. */
    free(pristine);
    printf("Restored %zu mutable objects (%zu bytes); excluded %zu const objects\n", objects, total,
           immutable);
    return 0;
}
