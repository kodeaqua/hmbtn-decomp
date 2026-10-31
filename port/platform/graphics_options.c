#ifdef HMBTN_ENH_SCALING
#include "enhance/scaling/scaling.h"
#endif
#include "platform/graphics_options.h"
#ifdef HMBTN_ENH_PGXP
#include "enhance/pgxp/pgxp.h"
#endif
#ifdef HMBTN_ENH_DITHERING
#include "enhance/dithering/dithering.h"
#endif
#include "platform/settings_json.h"
#include "platform/user_data.h"
#ifdef HMBTN_ENH_INTERPOLATION
#include "enhance/interpolation/interpolation.h"
#endif
static void hm_validate_rate(hm_graphics_options *options);

int hm_graphics_profile_get(const hm_graphics_options *o) {
    return o ? o->profile : HM_GRAPHICS_PROFILE_NATIVE;
}

int hm_graphics_scene_filter_mode(const hm_graphics_options *o, int mobile) {
    if (!o)
        return 0;
    /* Mobile keeps per-texture sampling cheap; final presentation may still
     * use the profile's bilinear mode when enlarging the completed scene. */
    return mobile && o->linear != 2 ? 0 : o->linear;
}

void hm_graphics_profile_set(hm_graphics_options *o, int profile) {
    if (!o || profile < HM_GRAPHICS_PROFILE_NATIVE || profile > HM_GRAPHICS_PROFILE_MODERN)
        return;
#ifdef __ANDROID__
    o->profile = profile;
    if (profile == HM_GRAPHICS_PROFILE_MODERN) {
        o->widescreen = 1;
        o->linear = 1;
        o->scaling = 0;
        o->vsync = 1;
        o->fps_numerator = 0;
        o->fps_denominator = 1;
        o->interpolation = 0;
        o->antialiasing = 0;
        o->pgxp = 1;
        o->dither = 0;
    } else if (profile == HM_GRAPHICS_PROFILE_ENHANCE) {
        o->widescreen = 0;
        o->linear = 0;
        o->scaling = 0;
        o->vsync = 1;
        o->fps_numerator = 0;
        o->fps_denominator = 1;
        o->interpolation = 0;
        o->antialiasing = 0;
        o->pgxp = 1;
        o->dither = 0;
    } else {
        o->widescreen = 0;
        o->linear = 0;
        o->scaling = 0;
        o->vsync = 1;
        o->fps_numerator = 0;
        o->fps_denominator = 1;
        o->interpolation = 0;
        o->antialiasing = 0;
        o->pgxp = 0;
        o->dither = 1;
    }
#else
    if (o->profile == HM_GRAPHICS_PROFILE_MODERN) {
        o->modern_linear = o->linear;
        o->modern_scaling = o->scaling;
        o->modern_widescreen = o->widescreen;
        o->modern_interpolation = o->interpolation;
        o->modern_antialiasing = o->antialiasing;
    }
    o->profile = profile;
    o->pgxp = profile != HM_GRAPHICS_PROFILE_NATIVE;
    o->dither = profile == HM_GRAPHICS_PROFILE_NATIVE;
    o->linear = profile == HM_GRAPHICS_PROFILE_MODERN ? o->modern_linear : 0;
    o->scaling = profile == HM_GRAPHICS_PROFILE_MODERN ? o->modern_scaling : 0;
    o->widescreen = profile == HM_GRAPHICS_PROFILE_MODERN ? o->modern_widescreen : 0;
    o->interpolation = profile == HM_GRAPHICS_PROFILE_MODERN ? o->modern_interpolation : 0;
    o->antialiasing = profile == HM_GRAPHICS_PROFILE_MODERN ? o->modern_antialiasing : 0;
#endif
}

void hm_graphics_defaults(hm_graphics_options *o) {
    *o = (hm_graphics_options){.width = 640, .height = 480, .vsync = 1, .fps_denominator = 1, .pgxp = 0, .dither = 1};
    o->aa_capabilities =
#ifdef HMBTN_ENH_ANTIALIASING
        (1u << HM_AA_COUNT) - 1;
#else
        1;
#endif
}
void hm_graphics_load(hm_graphics_options *o) {
    hm_graphics_defaults(o);
    char *text = SDL_malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "graphics", &object, &length) == 1) {
        hm_graphics_options candidate = *o;
        hm_json_integer(object, length, "monitor", &candidate.monitor);
        hm_json_integer(object, length, "window_mode", &candidate.mode);
        hm_json_integer(object, length, "width", &candidate.width);
        hm_json_integer(object, length, "height", &candidate.height);
        hm_json_integer(object, length, "bilinear", &candidate.linear);
        hm_json_integer(object, length, "vsync", &candidate.vsync);
        hm_json_integer(object, length, "renderer", &candidate.renderer);
        hm_json_integer(object, length, "fps_numerator", &candidate.fps_numerator);
        hm_json_integer(object, length, "fps_denominator", &candidate.fps_denominator);
        if (candidate.monitor >= 0 && candidate.monitor < 64 && candidate.mode >= 0 &&
            candidate.mode <= 2 && candidate.width >= 320 && candidate.width <= 16384 &&
            candidate.height >= 240 && candidate.height <= 16384 && candidate.linear >= 0 &&
            candidate.linear <= 2 && candidate.renderer >= 0 && candidate.renderer <= 1 &&
            candidate.vsync >= 0 && candidate.vsync <= 1 && candidate.fps_numerator >= -1 &&
            candidate.fps_denominator > 0 && candidate.fps_denominator <= 1000000 &&
            (candidate.fps_numerator <= 0 ||
             (candidate.fps_numerator >= candidate.fps_denominator &&
              (u64)candidate.fps_numerator <= (u64)candidate.fps_denominator * 1000)))
            *o = candidate;
    }
#ifdef HMBTN_ENH_INTERPOLATION
    const char *value;
    size_t value_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "interpolation", &value, &value_length) == 1 &&
        value_length == 4 && !SDL_memcmp(value, "true", 4))
        o->interpolation = 1;
#endif
#ifdef HMBTN_ENH_SCALING
    const char *scale_value;
    size_t scale_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "scaling", &scale_value, &scale_length) == 1 &&
        scale_length == 4 && !SDL_memcmp(scale_value, "true", 4))
        o->scaling = 1;
#endif
#ifdef HMBTN_ENH_FILTERING
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1) {
        int mode_val = -1;
        if (hm_json_integer(object, length, "filtering_mode", &mode_val) && mode_val >= 0 && mode_val <= 2) {
            o->linear = mode_val;
        } else {
            const char *filter_value;
            size_t filter_length;
            if (hm_json_property(object, length, "filtering", &filter_value, &filter_length) == 1) {
                if (filter_length == 4 && !SDL_memcmp(filter_value, "true", 4))
                    o->linear = 1;
                else if (filter_length == 5 && !SDL_memcmp(filter_value, "false", 5))
                    o->linear = 0;
            }
        }
    }
#endif
#ifdef HMBTN_ENH_WIDESCREEN
    const char *wide_value;
    size_t wide_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "widescreen", &wide_value, &wide_length) == 1 &&
        wide_length == 4 && !SDL_memcmp(wide_value, "true", 4))
        o->widescreen = 1;
#endif
#ifdef HMBTN_ENH_ANTIALIASING
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1)
        hm_json_integer(object, length, "antialiasing", &o->antialiasing);
    if (o->antialiasing < HM_AA_OFF || o->antialiasing >= HM_AA_COUNT ||
        (o->renderer && o->antialiasing >= HM_AA_MSAA2))
        o->antialiasing = HM_AA_OFF;
#endif
#ifdef HMBTN_ENH_PGXP
    const char *pgxp_value;
    size_t pgxp_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "pgxp", &pgxp_value, &pgxp_length) == 1 &&
        pgxp_length == 4 && !SDL_memcmp(pgxp_value, "true", 4))
        o->pgxp = 1;
#endif
#ifdef HMBTN_ENH_DITHERING
    const char *dither_value;
    size_t dither_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "disable_dithering", &dither_value, &dither_length) == 1 &&
        dither_length == 4 && !SDL_memcmp(dither_value, "true", 4))
        o->dither = 0;
#endif
    /* Preserve legacy enhancement settings when migrating an existing installation. */
    int profile = (o->scaling || o->widescreen || o->linear || o->interpolation || o->antialiasing)
                      ? HM_GRAPHICS_PROFILE_MODERN
                      : (o->pgxp || !o->dither ? HM_GRAPHICS_PROFILE_ENHANCE : HM_GRAPHICS_PROFILE_NATIVE);
    int saved_profile;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "graphics", &object, &length) == 1 &&
        hm_json_integer(object, length, "graphics_mode", &saved_profile) &&
        saved_profile >= HM_GRAPHICS_PROFILE_NATIVE && saved_profile <= HM_GRAPHICS_PROFILE_MODERN)
        profile = saved_profile;
    o->profile = HM_GRAPHICS_PROFILE_MODERN;
    hm_graphics_profile_set(o, profile);
    SDL_free(text);
    hm_validate_rate(o);
}
int hm_graphics_save(const hm_graphics_options *o) {
    hm_graphics_options saved = *o;
    if (o->profile != HM_GRAPHICS_PROFILE_MODERN) {
        saved.linear = o->modern_linear; saved.scaling = o->modern_scaling;
        saved.widescreen = o->modern_widescreen; saved.interpolation = o->modern_interpolation;
        saved.antialiasing = o->modern_antialiasing;
    }
    const hm_graphics_options *active = o;
    o = &saved;
    char json[256];
    SDL_snprintf(json, sizeof(json),
                 "{\"monitor\":%d,\"window_mode\":%d,\"width\":%d,\"height\":%d,\"bilinear\":%d,"
                 "\"renderer\":%d,\"vsync\":%d,\"fps_numerator\":%d,\"fps_denominator\":%d,\"graphics_mode\":%d}",
                 o->monitor, o->mode, o->width, o->height, o->linear, o->renderer, o->vsync,
                 o->fps_numerator, o->fps_denominator, active->profile);
    int ok = hm_user_data_set_setting("graphics", json);
#ifdef HMBTN_ENH_INTERPOLATION
    if (ok)
        ok = hm_user_data_set_nested_setting("enhance", "interpolation",
                                             o->interpolation ? "true" : "false");
#endif
#ifdef HMBTN_ENH_SCALING
    if (ok)
        ok = hm_user_data_set_nested_setting("enhance", "scaling", o->scaling ? "true" : "false");
#endif
#ifdef HMBTN_ENH_FILTERING
    if (ok) {
        char f_mode[16];
        SDL_snprintf(f_mode, sizeof(f_mode), "%d", o->linear);
        ok = hm_user_data_set_nested_setting("enhance", "filtering_mode", f_mode);
    }
#endif
#ifdef HMBTN_ENH_WIDESCREEN
    if (ok)
        ok = hm_user_data_set_nested_setting("enhance", "widescreen",
                                             o->widescreen ? "true" : "false");
#endif
#ifdef HMBTN_ENH_ANTIALIASING
    if (ok) {
        char mode[16];
        SDL_snprintf(mode, sizeof(mode), "%d", o->antialiasing);
        ok = hm_user_data_set_nested_setting("enhance", "antialiasing", mode);
    }
#endif
#ifdef HMBTN_ENH_PGXP
    if (ok)
        ok = hm_user_data_set_nested_setting("enhance", "pgxp", o->pgxp ? "true" : "false");
#endif
#ifdef HMBTN_ENH_DITHERING
    if (ok)
        ok = hm_user_data_set_nested_setting("enhance", "disable_dithering",
                                             !o->dither ? "true" : "false");
#endif
#ifdef __ANDROID__
    if (ok) {
        const char *prof_str =
            hm_graphics_profile_get(o) == HM_GRAPHICS_PROFILE_MODERN ? "\"modern\"" :
            hm_graphics_profile_get(o) == HM_GRAPHICS_PROFILE_ENHANCE ? "\"enhance\"" : "\"native\"";
        ok = hm_user_data_set_nested_setting("enhance", "profile", prof_str);
    }
#endif
    return ok;
}
int hm_pause_config_enabled(void) {
    int enabled = 1; /* The owner requests ESC throughout the native game. */
    char *text = SDL_malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object, *value;
    size_t length, value_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "pause_menu", &value, &value_length) == 1) {
        if (value_length == 5 && !SDL_memcmp(value, "false", 5))
            enabled = 0;
    }
    SDL_free(text);
    return enabled;
}
static SDL_DisplayID hm_display(int ordinal) {
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    SDL_DisplayID result =
        displays && count ? displays[ordinal >= 0 && ordinal < count ? ordinal : 0] : 0;
    SDL_free(displays);
    return result;
}
int hm_graphics_apply(SDL_Window *window, const hm_graphics_options *o) {
#ifdef __ANDROID__
    (void)window;
    (void)o;
    return 1;
#else
    SDL_DisplayID display = hm_display(o->monitor);
    if (!display || o->mode < 0 || o->mode > 2 || o->width < 320 || o->height < 240)
        return 0;
    if (!SDL_SetWindowFullscreen(window, false) || !SDL_SetWindowFullscreenMode(window, NULL) ||
        !SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                               SDL_WINDOWPOS_CENTERED_DISPLAY(display)))
        return 0;
    if (o->mode == 0)
        return SDL_SetWindowSize(window, o->width, o->height) &&
               SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                                     SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    if (o->mode == 2)
        return SDL_SetWindowFullscreen(window, true);
    int count;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &count);
    int ok = 0;
    for (int i = 0; modes && i < count; ++i) {
        if (modes[i]->w == o->width && modes[i]->h == o->height) {
            ok = SDL_SetWindowFullscreenMode(window, modes[i]) &&
                 SDL_SetWindowFullscreen(window, true);
            break;
        }
    }
    if (!ok && modes && count > 0) {
        ok = SDL_SetWindowFullscreenMode(window, modes[0]) &&
             SDL_SetWindowFullscreen(window, true);
    }
    if (!ok) {
        ok = SDL_SetWindowFullscreen(window, true);
    }
    SDL_free(modes);
    return ok;
#endif
}
hm_present_rate hm_graphics_present_rate(const hm_graphics_options *o) {
    if (o->fps_numerator == -1)
        return (hm_present_rate){0, 1};
    return o->fps_numerator ? (hm_present_rate){(u32)o->fps_numerator, (u32)o->fps_denominator}
                            : (hm_present_rate){30, 1};
}
int hm_graphics_swapchain(SDL_GPUDevice *device, SDL_Window *window, const hm_graphics_options *o) {
#ifdef __ANDROID__
    /* On mobile Vulkan, prefer triple-buffering non-blocking MAILBOX mode to prevent
     * SurfaceFlinger VSync stalls on high-refresh (90Hz/120Hz) panels. */
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_MAILBOX;
    if (!SDL_WindowSupportsGPUPresentMode(device, window, mode))
        mode = o->vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
#else
    SDL_GPUPresentMode mode = o->vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
    if (!SDL_WindowSupportsGPUPresentMode(device, window, mode)) {
        if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
        else
            mode = SDL_GPU_PRESENTMODE_VSYNC;
    }
#endif
    return SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
}
static int hm_refresh_rate(const SDL_DisplayMode *mode, hm_present_rate *rate) {
    if (!mode)
        return 0;
    u32 n, d;
    if (mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0) {
        n = (u32)mode->refresh_rate_numerator;
        d = (u32)mode->refresh_rate_denominator;
    } else {
        if (!(mode->refresh_rate > 0 && mode->refresh_rate <= 1000))
            return 0;
        n = (u32)(mode->refresh_rate * 1000.0f + 0.5f);
        d = 1000;
    }
    u32 a = n, b = d;
    while (b) {
        u32 rest = a % b;
        a = b;
        b = rest;
    }
    n /= a;
    d /= a;
    if (!d || d > 1000000 || n < d || (u64)n > (u64)d * 1000)
        return 0;
    *rate = (hm_present_rate){n, d};
    return 1;
}
hm_present_rate hm_graphics_effective_rate(SDL_Window *window, const hm_graphics_options *o) {
    hm_present_rate wanted = hm_graphics_present_rate(o), actual;
    if (o->vsync &&
        hm_refresh_rate(SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window)), &actual) &&
        (!wanted.numerator ||
         (u64)actual.numerator * wanted.denominator < (u64)wanted.numerator * actual.denominator))
        return actual;
    return wanted;
}
void hm_graphics_sync_monitor(SDL_Window *window, hm_graphics_options *o) {
    SDL_DisplayID current = SDL_GetDisplayForWindow(window);
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    for (int i = 0; displays && i < count; ++i)
        if (displays[i] == current) {
            o->monitor = i;
            break;
        }
    SDL_free(displays);
    hm_validate_rate(o);
}
static int hm_rates(const hm_graphics_options *o, hm_present_rate rates[256]) {
    SDL_DisplayID display = hm_display(o->monitor);
    const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(display);
    int width = o->mode == 1 || !desktop ? o->width : desktop->w;
    int height = o->mode == 1 || !desktop ? o->height : desktop->h;
    rates[0] = (hm_present_rate){30, 1};
    rates[1] = (hm_present_rate){0, 1}; /* Off (Unlimited). */
    int used = 2, count = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &count);
    for (int i = 0; modes && i < count && used < 256; ++i) {
        hm_present_rate rate;
        if (modes[i]->w != width || modes[i]->h != height || !hm_refresh_rate(modes[i], &rate))
            continue;
        int duplicate = 0;
        for (int j = 0; j < used; ++j)
            if (hm_present_rate_equal(rate, rates[j]))
                duplicate = 1;
        if (!duplicate)
            rates[used++] = rate;
    }
    SDL_free(modes);
    const SDL_DisplayMode *current_mode = SDL_GetCurrentDisplayMode(display);
    const SDL_DisplayMode *fallbacks[2] = {desktop, current_mode};
    for (int i = 0; i < 2 && used < 256; ++i) {
        hm_present_rate rate;
        if (!fallbacks[i] || !hm_refresh_rate(fallbacks[i], &rate))
            continue;
        int duplicate = 0;
        for (int j = 0; j < used; ++j)
            if (hm_present_rate_equal(rate, rates[j]))
                duplicate = 1;
        if (!duplicate)
            rates[used++] = rate;
    }
    for (int i = 3; i < used; ++i) {
        hm_present_rate rate = rates[i];
        int j = i;
        while (j > 2 && (u64)rate.numerator * rates[j - 1].denominator <
                            (u64)rates[j - 1].numerator * rate.denominator) {
            rates[j] = rates[j - 1];
            --j;
        }
        rates[j] = rate;
    }
    return used;
}
#ifndef __ANDROID__
static int hm_cycle_rate(hm_graphics_options *o, int step) {
    hm_present_rate rates[256];
    int count = hm_rates(o, rates), current = 0;
    if (o->fps_numerator)
        for (int i = 1; i < count; ++i)
            if (hm_present_rate_equal(hm_graphics_present_rate(o), rates[i]))
                current = i;
    int next = (current + count + step) % count;
    o->fps_numerator = next == 1 ? -1 : next ? (int)rates[next].numerator : 0;
    o->fps_denominator = next ? (int)rates[next].denominator : 1;
    return count > 1;
}
#endif
static void hm_validate_rate(hm_graphics_options *o) {
    if (o->fps_numerator <= 0) {
        o->fps_denominator = 1;
        return;
    }
    hm_present_rate rates[256];
    int count = hm_rates(o, rates);
    for (int i = 1; i < count; ++i)
        if (hm_present_rate_equal(hm_graphics_present_rate(o), rates[i]))
            return;
    o->fps_numerator = 0;
    o->fps_denominator = 1;
}
int hm_graphics_change(SDL_GPUDevice *device, SDL_Window *window, hm_graphics_options *o, int item,
                       int direction) {
    if (item == 0) {
        hm_graphics_options candidate = *o;
        int step = direction < 0 ? -1 : 1;
        hm_graphics_profile_set(&candidate, (o->profile + 3 + step) % 3);
#ifdef HMBTN_ENH_SCALING
        if (!hm_scaling_enable(candidate.scaling)) return 0;
#endif
#ifdef HMBTN_ENH_INTERPOLATION
        if (!hm_interpolation_enable(candidate.interpolation)) {
#ifdef HMBTN_ENH_SCALING
            hm_scaling_enable(o->scaling);
#endif
            return 0;
        }
#endif
        *o = candidate;
        return 1;
    }
#ifndef __ANDROID__
    --item;
    if (o->profile != HM_GRAPHICS_PROFILE_MODERN && item >= 3 && item <= 6)
        return 0;
    if (o->profile != HM_GRAPHICS_PROFILE_MODERN && item == 9)
        return 0;
#endif
    hm_graphics_options candidate = *o;
    int step = direction < 0 ? -1 : 1;
#ifdef __ANDROID__
    (void)device;
    (void)window;
    if (item == 0) {
        int current = hm_graphics_profile_get(o);
        int next = (current + 3 + step) % 3;
        hm_graphics_profile_set(&candidate, next);
#ifdef HMBTN_ENH_SCALING
        if (!hm_scaling_enable(candidate.scaling))
            return 0;
#endif
#ifdef HMBTN_ENH_INTERPOLATION
        hm_interpolation_enable(candidate.interpolation);
#endif
        *o = candidate;
        return 1;
    }
    return 0;
#else
#ifdef HMBTN_ENH_ANTIALIASING
    if (item == 5) {
        unsigned supported = o->aa_capabilities & (o->renderer ? 3u : 15u);
        int next = hm_antialiasing_cycle(o->antialiasing, step, supported);
        if (next == o->antialiasing) return 0;
        o->antialiasing = next;
        return 1;
    }
    if (item == 10 && !o->renderer && o->antialiasing >= HM_AA_MSAA2)
        return 0; /* Keep the selected mode explicit: Software offers Off/FXAA. */
#endif
    if (item == 6) {
        o->linear = (o->linear + 3 + step) % 3;
        return 1;
    }
    if (item == 7)
        candidate.vsync = !candidate.vsync;
    else if (item == 8) {
        if (!hm_cycle_rate(&candidate, step))
            return 0;
    } else if (item == 0) {
        int count;
        SDL_DisplayID *displays = SDL_GetDisplays(&count);
        if (!displays || !count) {
            SDL_free(displays);
            return 0;
        }
        candidate.monitor = (o->monitor + count + step) % count;
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(displays[candidate.monitor]);
        if (desktop && candidate.mode == 1) {
            candidate.width = desktop->w;
            candidate.height = desktop->h;
        }
        SDL_free(displays);
    } else if (item == 1) {
        candidate.mode = (candidate.mode + 3 + step) % 3;
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(hm_display(candidate.monitor));
        if (desktop && candidate.mode == 1) {
            candidate.width = desktop->w;
            candidate.height = desktop->h;
        }
    } else if (item == 2) {
        if (candidate.mode == 2)
            return 0; /* Borderless follows the desktop. */
        int count;
        SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(hm_display(o->monitor), &count);
        int current = -1, unique = 0, widths[256], heights[256];
        for (int i = 0; modes && i < count && unique < 256; ++i) {
            int w = modes[i]->w, h = modes[i]->h, duplicate = 0;
            if (w < 320 || h < 240)
                continue;
            for (int j = 0; j < unique; ++j)
                if (widths[j] == w && heights[j] == h)
                    duplicate = 1;
            if (duplicate)
                continue;
            widths[unique] = w;
            heights[unique] = h;
            if (w == o->width && h == o->height)
                current = unique;
            ++unique;
        }
        SDL_free(modes);
        if (!unique)
            return 0;
        current = current < 0 ? (step < 0 ? unique - 1 : 0) : (current + unique + step) % unique;
        candidate.width = widths[current];
        candidate.height = heights[current];
    }
#ifdef HMBTN_ENH_SCALING
    else if (item == 3) {
        candidate.scaling = !candidate.scaling;
        if (!hm_scaling_enable(candidate.scaling))
            return 0;
    }
#endif
#ifdef HMBTN_ENH_INTERPOLATION
    else if (item == 9) {
        candidate.interpolation = !candidate.interpolation;
        if (!hm_interpolation_enable(candidate.interpolation))
            return 0;
    }
#endif
#ifdef HMBTN_ENH_WIDESCREEN
    else if (item == 4)
        candidate.widescreen = !candidate.widescreen;
#endif

    else
        return 0; /* Renderer enhancements remain unavailable until implemented. */
    int window_changed = item >= 0 && item <= 2;
    if (window_changed && !hm_graphics_apply(window, &candidate)) {
        hm_graphics_apply(window, o);
        return 0;
    }
    if ((window_changed || candidate.vsync != o->vsync) &&
        !hm_graphics_swapchain(device, window, &candidate)) {
        if (window_changed)
            hm_graphics_apply(window, o);
        hm_graphics_swapchain(device, window, o);
        return 0;
    }
    if (window_changed)
        hm_validate_rate(&candidate);
    *o = candidate;
    return 1;
#endif
}
void hm_graphics_value(const hm_graphics_options *o, int item, char *text, size_t capacity) {
    if (item == 0) {
        static const char *const names[] = {"Native PS1", "Enhance PS1", "Modern"};
        SDL_snprintf(text, capacity, "%s", names[o->profile]);
        return;
    }
#ifndef __ANDROID__
    --item;
#endif
#ifdef __ANDROID__
    if (item == 0) {
        int profile = hm_graphics_profile_get(o);
        if (profile == HM_GRAPHICS_PROFILE_MODERN)
            SDL_snprintf(text, capacity, "Modern - Mobile (16:9, Bilinear, 1x)");
        else if (profile == HM_GRAPHICS_PROFILE_ENHANCE)
            SDL_snprintf(text, capacity, "Enhance - PS1 Clean (4:3, 1x, PGXP)");
        else
            SDL_snprintf(text, capacity, "Native - Authentic PS1 (4:3, 1x)");
    } else {
        SDL_snprintf(text, capacity, "");
    }
#else
    static const char *const modes[] = {"Windowed", "Fullscreen", "Borderless Fullscreen"};
    const char *monitor_name = SDL_GetDisplayName(hm_display(o->monitor));
    if (item == 0)
        SDL_snprintf(text, capacity, "Monitor %d - %s", o->monitor + 1,
                     monitor_name ? monitor_name : "Unavailable");
    else if (item == 1)
        SDL_snprintf(text, capacity, "%s", modes[o->mode]);
    else if (item == 2) {
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(hm_display(o->monitor));
        SDL_snprintf(text, capacity, "%d x %d", o->mode == 2 && desktop ? desktop->w : o->width,
                     o->mode == 2 && desktop ? desktop->h : o->height);
    } else if (item == 3)
        SDL_snprintf(text, capacity, "%s",
                     o->scaling ? "On - viewport resolution" : "Off - PS1 resolution");
    else if (item == 4)
        SDL_snprintf(text, capacity, "%s",
                     o->widescreen ? "On - all screens" : "Off - original 4:3");
    else if (item == 7)
        SDL_snprintf(text, capacity, "%s",
                     o->vsync ? "On - synchronized display" : "Off - tearing possible");
    else if (item == 8) {
        if (o->fps_numerator == -1)
            SDL_snprintf(text, capacity, "Off - Unlimited FPS");
        else if (!o->fps_numerator)
            SDL_snprintf(text, capacity, "Native - 30 FPS");
        else {
            u64 scaled = ((u64)o->fps_numerator * 100 + (u32)o->fps_denominator / 2) /
                         (u32)o->fps_denominator;
            if (scaled % 100)
                SDL_snprintf(text, capacity, "%u.%02u FPS limit", (u32)(scaled / 100),
                             (u32)(scaled % 100));
            else
                SDL_snprintf(text, capacity, "%u FPS limit", (u32)(scaled / 100));
        }
    } else if (item == 9)
        SDL_snprintf(text, capacity, "%s", o->interpolation ? "On - world motion" : "Off");
    else if (item == 6)
        SDL_snprintf(text, capacity, "%s",
                     o->linear == 2 ? "xBR - sharp edge-directed curves" :
                     o->linear == 1 ? "Bilinear - smooth & soft" :
                                      "Nearest Neighbor - pixel art");
    else if (item == 5) {
#ifdef HMBTN_ENH_ANTIALIASING
        SDL_snprintf(text, capacity, "%s%s", hm_antialiasing_name(o->antialiasing),
                     o->antialiasing >= HM_AA_MSAA2 ?
                         (o->gpu_active ? " - GPU scenes" : " - inactive on CPU frames") :
                     o->antialiasing == HM_AA_FXAA ? " - final image" : "");
#else
        SDL_snprintf(text, capacity, "Off");
#endif
    }
    else
        SDL_snprintf(text, capacity, "Off - not available yet");
#endif
}
void hm_graphics_row_value(const hm_graphics_options *o, int item, char *text, size_t capacity) {
    if (item == 0) {
        static const char *const names[] = {"Native PS1", "Enhance PS1", "Modern"};
        SDL_snprintf(text, capacity, "%s", names[o->profile]);
        return;
    }
#ifndef __ANDROID__
    --item;
#endif
#ifdef __ANDROID__
    if (item == 0) {
        int profile = hm_graphics_profile_get(o);
        SDL_snprintf(text, capacity, "%s",
                     profile == HM_GRAPHICS_PROFILE_MODERN ? "Modern" :
                     profile == HM_GRAPHICS_PROFILE_ENHANCE ? "Enhance" : "Native");
    } else {
        SDL_snprintf(text, capacity, "-");
    }
#else
    if (item == 0)
        SDL_snprintf(text, capacity, "%d", o->monitor + 1);
    else if (item == 1) {
        static const char *const names[] = {"Windowed", "Fullscreen", "Borderless"};
        SDL_snprintf(text, capacity, "%s", names[o->mode]);
    } else if (item == 2)
        hm_graphics_value(o, item + 1, text, capacity);
    else if (item == 3)
        SDL_snprintf(text, capacity, "%s", o->scaling ? "On" : "Off");
    else if (item == 4)
        SDL_snprintf(text, capacity, "%s", o->widescreen ? "On" : "Off");
    else if (item == 7)
        SDL_snprintf(text, capacity, "%s", o->vsync ? "On" : "Off");
    else if (item == 8) {
        if (o->fps_numerator == -1)
            SDL_snprintf(text, capacity, "Off");
        else if (!o->fps_numerator)
            SDL_snprintf(text, capacity, "Native (30)");
        else {
            u64 scaled = ((u64)o->fps_numerator * 100 + (u32)o->fps_denominator / 2) /
                         (u32)o->fps_denominator;
            if (scaled % 100)
                SDL_snprintf(text, capacity, "%u.%02u", (u32)(scaled / 100), (u32)(scaled % 100));
            else
                SDL_snprintf(text, capacity, "%u", (u32)(scaled / 100));
        }
    } else if (item == 9)
        SDL_snprintf(text, capacity, "%s", o->interpolation ? "On" : "Off");
    else if (item == 6)
        SDL_snprintf(text, capacity, "%s",
                     o->linear == 2 ? "xBR" :
                     o->linear == 1 ? "Bilinear" : "Nearest");
    else if (item == 5) {
#ifdef HMBTN_ENH_ANTIALIASING
        SDL_snprintf(text, capacity, "%s", hm_antialiasing_name(o->antialiasing));
#else
        SDL_snprintf(text, capacity, "Off");
#endif
    }
    else
        SDL_snprintf(text, capacity, "Off");
#endif
}
