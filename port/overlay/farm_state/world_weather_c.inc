/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_world_weather_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(birthday_villager_ids),
        HM_OVERLAY_OBJECT(weather_particles),
        HM_OVERLAY_OBJECT(weather_anim),
        HM_OVERLAY_OBJECT(weather_pause_random_state),
        HM_OVERLAY_OBJECT(weather_pause_random_active),
        HM_OVERLAY_OBJECT(weather_drift_table),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
