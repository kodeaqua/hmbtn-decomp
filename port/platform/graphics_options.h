#ifndef HMBTN_PLATFORM_GRAPHICS_OPTIONS_H
#define HMBTN_PLATFORM_GRAPHICS_OPTIONS_H
#include "platform/presentation_clock.h"
#include "enhance/antialiasing/antialiasing.h"
#include <SDL3/SDL.h>
typedef struct {
    int monitor, mode, width, height, linear, vsync, interpolation, scaling, widescreen, pgxp, dither;
    int renderer, gpu_active;           /* 0 = GPU Auto, 1 = Software; active is transient. */
    int antialiasing;
    int profile;
    int modern_linear, modern_scaling, modern_widescreen, modern_interpolation, modern_antialiasing;
    unsigned aa_capabilities; /* Transient supported-mode bitmask; never serialized. */
    int fps_numerator, fps_denominator; /* 0 = Native (30); -1 = Off (Unlimited). */
} hm_graphics_options;
void hm_graphics_defaults(hm_graphics_options *options);
void hm_graphics_load(hm_graphics_options *options);
int hm_graphics_save(const hm_graphics_options *options);
int hm_pause_config_enabled(void);
int hm_graphics_apply(SDL_Window *window, const hm_graphics_options *options);
int hm_graphics_change(SDL_GPUDevice *device, SDL_Window *window, hm_graphics_options *options,
                       int item, int direction);
int hm_graphics_swapchain(SDL_GPUDevice *device, SDL_Window *window,
                          const hm_graphics_options *options);
hm_present_rate hm_graphics_present_rate(const hm_graphics_options *options);
hm_present_rate hm_graphics_effective_rate(SDL_Window *window, const hm_graphics_options *options);
void hm_graphics_sync_monitor(SDL_Window *window, hm_graphics_options *options);
void hm_graphics_value(const hm_graphics_options *options, int item, char *text, size_t capacity);
void hm_graphics_row_value(const hm_graphics_options *options, int item, char *text,
                           size_t capacity);
enum {
    HM_GRAPHICS_PROFILE_NATIVE = 0,
    HM_GRAPHICS_PROFILE_ENHANCE = 1,
    HM_GRAPHICS_PROFILE_MODERN = 2
};
int hm_graphics_profile_get(const hm_graphics_options *options);
void hm_graphics_profile_set(hm_graphics_options *options, int profile);
int hm_graphics_scene_filter_mode(const hm_graphics_options *options, int mobile);
#endif
