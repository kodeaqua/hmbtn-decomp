#include "farm_state.h"
#include "estate.h"
#include "event_flags.h"
#include "people/player.h"
#if defined(__ANDROID__)
#include <android/log.h>
#endif
#include "platform/gpu_present.h"
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "gfx/game_frame.h"
#include "gfx/software_output.h"
#endif
#ifdef HMBTN_ENH_FILTERING
#include "gfx/game_frame.h"
#include "gfx/software_output.h"
#endif
#ifdef HMBTN_GPU_RASTER
#include "gfx/frame_replay.h"
#include "gfx/game_frame.h"
#include "platform/gpu_renderer.h"
#endif
#ifdef HMBTN_ENH_SCALING
#include "enhance/scaling/scaling.h"
#include "gfx/game_frame.h"
#endif
#ifdef HMBTN_ENH_INTERPOLATION
#include "enhance/interpolation/interpolation.h"
#include "gfx/game_frame.h"
#endif
#include "platform/audio_output.h"
#include "platform/game_host.h"
#include "platform/input_replay.h"
#include "platform/presentation_clock.h"
#include "platform/user_data.h"
#include "platform/version.h"
#include "platform/window_icon.h"
#include "platform/mouse_input.h"
#include "platform/controls_options.h"
#include "platform/audio_options.h"
#ifdef HMBTN_ENH_PAUSE_MENU
#include "enhance/pause_menu/assets.h"
#include "enhance/pause_menu/canvas.h"
#include "enhance/pause_menu/menu.h"
#include "platform/graphics_options.h"
#ifdef HMBTN_ENH_PGXP
#include "enhance/pgxp/pgxp.h"
#endif
#ifdef HMBTN_ENH_DITHERING
#include "enhance/dithering/dithering.h"
#endif
#ifdef HMBTN_ENH_GAMEPLAY
#include "enhance/gameplay/gameplay.h"
#include "platform/gameplay_options.h"
#endif
#ifdef HMBTN_ENH_ANTIALIASING
#include "platform/fxaa.h"
#define HM_HOST_AA 1
#endif
#endif
#include "psx/gpu_environment.h"
#include "psx/pad.h"
#include "psx/spu_voice.h"
#include "psx/timing.h"
#include "psx/vram.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "items.h"
#include "picture_cells.h"
#include <stdio.h>
#include <stdlib.h>

static SDL_Window *hm_window;
static SDL_GPUDevice *hm_device;
static SDL_GPUTexture *hm_texture;
static SDL_GPUTransferBuffer *hm_upload;
static u32 hm_image_width, hm_image_height, hm_upload_bytes;
static u64 hm_scanout_cached_vram_gen;
static RECT hm_scanout_cached_area;
static int hm_scanout_cached_enabled;
static Uint64 hm_start;
static hm_input_replay hm_replay;
static int hm_replay_enabled;
static u64 hm_host_tick_limit;
static hm_controls_options hm_controls;
#ifdef __ANDROID__
static u16 hm_virtual_pad_press_latch;
#endif
static hm_audio_options hm_audio;
static SDL_Gamepad *hm_active_gamepad = NULL;
static int hm_trace_camera;
static hm_present_clock hm_presentation;
#ifdef HMBTN_ENH_PAUSE_MENU
static hm_pause_assets hm_assets;
static hm_graphics_options hm_options;
#ifdef HMBTN_ENH_GAMEPLAY
static hm_gameplay_options hm_gameplay;
#endif
#ifdef HM_HOST_AA
static hm_fxaa *hm_fxaa_output;
#endif
static int hm_ignore_held;
static int hm_pause_enabled;
static int hm_host_in_pause;
static u8 hm_pause_background[320 * 240 * 4];
static u8 hm_pause_frozen[1024 * 240 * 4];
#endif
#ifdef HMBTN_GPU_RASTER
static hm_gpu_renderer *hm_gpu_output;
static hm_gpu_scene *hm_gpu_commands;
static hm_frame_replay *hm_gpu_replay;
static hm_gpu_scene_data hm_gpu_cached;
static u64 hm_gpu_cached_serial;
enum { HM_GPU_COVERAGE_REPORT_FRAMES = 600 };
static u32 hm_gpu_coverage_frames, hm_gpu_coverage_successes, hm_gpu_coverage_fallbacks;
static u32 hm_gpu_coverage_scanouts, hm_gpu_coverage_skipped;
static char hm_gpu_coverage_last_reason[160] = "none";
#endif

#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
static hm_software_output *hm_native_output;
#endif
enum { HM_SCAN_WIDTH = 1024, HM_SCAN_HEIGHT = 512 };

static void hm_host_fail(void) {
    fprintf(stderr, "Native window: %s\n", SDL_GetError());
    hm_game_host_stop();
    exit(1);
}

/* Texture bounds match the image so linear sampling cannot bleed from a stale
 * border. Transfer capacity only grows; SDL defers released queued resources. */
static int hm_host_image_size(u32 width, u32 height) {
    if (hm_texture && width == hm_image_width && height == hm_image_height)
        return 1;
    if (!width || !height || width > 4096 || height > 4096)
        return 0;
    SDL_GPUTextureCreateInfo texture = {.type = SDL_GPU_TEXTURETYPE_2D,
                                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                        .width = width,
                                        .height = height,
                                        .layer_count_or_depth = 1,
                                        .num_levels = 1};
    SDL_GPUTexture *image = SDL_CreateGPUTexture(hm_device, &texture);
    if (!image)
        return 0;
    u32 bytes = width * height * 4;
    SDL_GPUTransferBuffer *upload = hm_upload;
    if (!upload || bytes > hm_upload_bytes) {
        SDL_GPUTransferBufferCreateInfo transfer = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                    .size = bytes};
        upload = SDL_CreateGPUTransferBuffer(hm_device, &transfer);
        if (!upload) {
            SDL_ReleaseGPUTexture(hm_device, image);
            return 0;
        }
    }
    if (hm_texture)
        SDL_ReleaseGPUTexture(hm_device, hm_texture);
    if (hm_upload && upload != hm_upload)
        SDL_ReleaseGPUTransferBuffer(hm_device, hm_upload);
    hm_texture = image;
    hm_upload = upload;
    hm_image_width = width;
    hm_image_height = height;
    hm_scanout_cached_vram_gen = 0;
    if (bytes > hm_upload_bytes)
        hm_upload_bytes = bytes;
    return 1;
}
#ifdef HMBTN_ENH_SCALING
static void hm_host_scaling_unavailable(void) {
#ifdef HMBTN_ENH_PAUSE_MENU
    hm_options.scaling = 0;
#endif
    hm_scaling_enable(0);
    Uint64 before = SDL_GetTicksNS();
    hm_audio_output_pause(1);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Back to Nature",
                             "Enhance Scaling could not allocate the render size (maximum 4096 per "
                             "axis). Native rendering remains active.",
                             hm_window);
    hm_start += SDL_GetTicksNS() - before;
    hm_audio_output_pause(0);
}
#ifdef HMBTN_ENH_PAUSE_MENU
static int hm_host_wide_active(void);
static void hm_host_output_size(int *w, int *h);
static int hm_host_scaling_prepare(void) {
    int w, h;
    if (!SDL_GetWindowSizeInPixels(hm_window, &w, &h) || w <= 0 || h <= 0)
        return 0;
    hm_host_output_size(&w, &h);
#ifdef __ANDROID__
    /* Keep a conservative guard for any programmatic scaling request; shipped
     * Android profiles use native internal resolution (ADR-164). */
    if (w > 1280 || h > 720) {
        if (hm_host_wide_active()) {
            w = 1280;
            h = 720;
        } else {
            w = 960;
            h = 720;
        }
    }
#endif
#ifdef HMBTN_ENH_FILTERING
    hm_scaling_filter(hm_options.linear);
#endif
    if (hm_scaling_prepare((u32)w, (u32)h))
        return 1;
    hm_host_scaling_unavailable();
    return 0;
}
#endif
#endif

static hm_present_rate hm_host_present_rate(void) {
#ifdef HMBTN_ENH_PAUSE_MENU
    return hm_graphics_effective_rate(hm_window, &hm_options);
#else
    return (hm_present_rate){60, 1};
#endif
}
#ifdef __ANDROID__
static u64 hm_android_last_vram_gen;
static RECT hm_android_last_area;
static int hm_android_last_enabled;
static char hm_perf_notice[64];
typedef struct {
    Uint64 scene_ns, scene_max_ns;
    Uint64 replay_ns, replay_max_ns;
    Uint64 materialize_ns, materialize_max_ns;
    Uint64 materialize_setup_ns, materialize_setup_max_ns;
    Uint64 materialize_mask_ns, materialize_mask_max_ns;
    Uint64 materialize_bind_ns, materialize_bind_max_ns;
    Uint64 materialize_replay_ns, materialize_replay_max_ns;
    Uint64 pack_ns, pack_max_ns;
    Uint64 encode_ns, encode_max_ns;
    Uint64 acquire_ns, acquire_max_ns;
    Uint64 blit_ns, blit_max_ns;
    Uint64 submit_ns, submit_max_ns;
    u32 scenes, replays, materialized_replays, packs, encodes, acquires, blits, submits;
    u32 width, height, primitive_count, tile_count, max_bin_count;
    u32 empty_bin_count, shared_cache_bin_count;
    u32 vram_version_count, materialization_tile_count;
    u64 total_bin_references;
    size_t scene_bytes;
} hm_gpu_perf_stats;
static hm_gpu_perf_stats hm_gpu_perf;
#endif
static void hm_host_reset_presentation(void) {
    hm_scanout_cached_vram_gen = 0;
#ifdef __ANDROID__
    hm_android_last_vram_gen = 0;
#endif
    hm_gpu_present_invalidate();
#ifdef HMBTN_GPU_RASTER
    hm_gpu_cached_serial = 0;
#endif
    if (!hm_present_clock_init(&hm_presentation, SDL_GetTicksNS(), hm_host_present_rate())) {
        SDL_SetError("Invalid presentation rate");
        hm_host_fail();
    }
}
static void hm_host_display_event(Uint32 type) {
#ifdef HMBTN_ENH_PAUSE_MENU
    if (type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
        type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED ||
        type == SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED) {
        hm_graphics_sync_monitor(hm_window, &hm_options);
        if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options)) {
            hm_options.vsync = 1;
            if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options))
                hm_host_fail();
        }
        hm_host_reset_presentation();
    }
#else
    (void)type;
#endif
}
static int hm_host_wide_active(void) {
#ifdef HMBTN_ENH_WIDESCREEN
    return hm_widescreen_margin() != 0;
#else
    return 0;
#endif
}
#if defined(HMBTN_ENH_SCALING) && defined(HMBTN_ENH_PAUSE_MENU)
static void hm_host_output_size(int *w, int *h) {
    if (hm_host_wide_active())
        return;
    if ((s64)*w * 3 > (s64)*h * 4)
        *w = *h * 4 / 3;
    else
        *h = *w * 3 / 4;
}
#endif
static void hm_host_blit(SDL_GPUCommandBuffer *command, SDL_GPUTexture *image, u32 image_width,
                         u32 image_height, SDL_GPUTexture *target, Uint32 width, Uint32 height,
                         int linear, int expanded) {
    int enabled = 0;
#ifdef HMBTN_ENH_WIDESCREEN
    enabled = hm_widescreen_enabled();
#endif
#ifdef HM_HOST_AA
    if (hm_options.antialiasing == HM_AA_FXAA && !hm_host_in_pause) {
        SDL_GPUTexture *filtered = hm_fxaa_encode(hm_fxaa_output, command, image, image_width, image_height);
        if (filtered) image = filtered;
        else {
            hm_options.antialiasing = HM_AA_OFF;
            fprintf(stderr, "FXAA output unavailable; anti-aliasing disabled\n");
        }
    }
#endif
    hm_gpu_present_image(command, image, image_width, image_height, target, width, height, linear,
                         expanded, enabled);
}
static int hm_host_submit(u32 image_width, u32 image_height, int linear, int wide) {
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
    if (!command)
        hm_host_fail();
    SDL_GPUTexture *swapchain = NULL;
    Uint32 width, height;
    if (!SDL_AcquireGPUSwapchainTexture(command, hm_window, &swapchain, &width, &height)) {
        SDL_CancelGPUCommandBuffer(command);
        hm_host_fail();
    }
    if (!swapchain) {
        /* Skip a full frame-in-flight queue rather than explicitly wait on it. */
        if (!SDL_CancelGPUCommandBuffer(command))
            hm_host_fail();
        return 0;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureTransferInfo source = {.transfer_buffer = hm_upload,
                                         .pixels_per_row = image_width,
                                         .rows_per_layer = image_height};
    SDL_GPUTextureRegion region = {
        .texture = hm_texture, .w = image_width, .h = image_height, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &region, true);
    SDL_EndGPUCopyPass(copy);
    hm_host_blit(command, hm_texture, image_width, image_height, swapchain, width, height, linear,
                 wide);
    if (!SDL_SubmitGPUCommandBuffer(command))
        hm_host_fail();
    return 1;
}

#ifdef HMBTN_GPU_RASTER
static void hm_host_gpu_coverage_report(void) {
    if (hm_gpu_coverage_frames < HM_GPU_COVERAGE_REPORT_FRAMES)
        return;
    u32 renderable = hm_gpu_coverage_successes + hm_gpu_coverage_fallbacks;
    double percent = renderable ? (100.0 * (double)hm_gpu_coverage_successes / renderable) : 0.0;
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "VanillaBTN",
                        "GPU Auto game-loop coverage: %u/%u renderable frames (%.1f%%; %u direct VRAM scanouts), %u CPU fallbacks, %u skipped of %u attempts; last: %s",
                        hm_gpu_coverage_successes, renderable, percent, hm_gpu_coverage_scanouts,
                        hm_gpu_coverage_fallbacks, hm_gpu_coverage_skipped,
                        hm_gpu_coverage_frames,
                        hm_gpu_coverage_last_reason);
#else
    SDL_Log("GPU Auto game-loop coverage: %u/%u renderable frames (%.1f%%; %u direct VRAM scanouts), %u CPU fallbacks, %u skipped of %u attempts; last: %s",
            hm_gpu_coverage_successes, renderable, percent, hm_gpu_coverage_scanouts,
            hm_gpu_coverage_fallbacks, hm_gpu_coverage_skipped, hm_gpu_coverage_frames,
            hm_gpu_coverage_last_reason);
#endif
    hm_gpu_coverage_frames = 0;
    hm_gpu_coverage_successes = 0;
    hm_gpu_coverage_fallbacks = 0;
    hm_gpu_coverage_scanouts = 0;
    hm_gpu_coverage_skipped = 0;
    SDL_strlcpy(hm_gpu_coverage_last_reason, "none", sizeof(hm_gpu_coverage_last_reason));
}
static void hm_host_gpu_coverage_record(int success, const char *reason) {
    ++hm_gpu_coverage_frames;
    if (success)
        ++hm_gpu_coverage_successes;
    else {
        ++hm_gpu_coverage_fallbacks;
        if (reason && *reason)
            SDL_snprintf(hm_gpu_coverage_last_reason, sizeof(hm_gpu_coverage_last_reason), "%s",
                         reason);
    }
    hm_host_gpu_coverage_report();
}
static void hm_host_gpu_coverage_skip(const char *reason) {
    ++hm_gpu_coverage_frames;
    ++hm_gpu_coverage_skipped;
    if (reason && *reason)
        SDL_snprintf(hm_gpu_coverage_last_reason, sizeof(hm_gpu_coverage_last_reason), "%s",
                     reason);
    hm_host_gpu_coverage_report();
}
static int hm_host_gpu_scanout_scene(const RECT *area, u32 width, u32 height) {
    hm_gpu_scene_texture_filter(hm_gpu_commands, 0);
    hm_gpu_scene_multisamples(hm_gpu_commands, 1);
    return hm_gpu_scene_scanout(hm_gpu_commands, area, width, height, &hm_gpu_cached);
}
static int hm_host_gpu_clear_present(void) {
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
    if (!command)
        hm_host_fail();
    SDL_GPUTexture *swapchain = NULL;
    Uint32 width, height;
    if (!SDL_AcquireGPUSwapchainTexture(command, hm_window, &swapchain, &width, &height)) {
        SDL_CancelGPUCommandBuffer(command);
        hm_host_fail();
    }
    if (!swapchain) {
        if (!SDL_CancelGPUCommandBuffer(command))
            hm_host_fail();
        hm_host_gpu_coverage_skip("GPU swapchain temporarily unavailable");
        return -1;
    }
    SDL_GPUColorTargetInfo target = {.texture = swapchain,
                                     .clear_color = {0, 0, 0, 1},
                                     .load_op = SDL_GPU_LOADOP_CLEAR,
                                     .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(command, &target, 1, NULL);
    if (!pass) {
        if (!SDL_CancelGPUCommandBuffer(command))
            hm_host_fail();
        hm_host_gpu_coverage_record(0, "GPU blank-frame render pass unavailable");
        return 0;
    }
    SDL_EndGPURenderPass(pass);
    if (!SDL_SubmitGPUCommandBuffer(command))
        hm_host_fail();
    hm_host_gpu_coverage_record(1, NULL);
    return 1;
}
static int hm_host_gpu_present(const RECT *native_area) {
    if (!native_area) {
        hm_host_gpu_coverage_record(0, "PS1 display area unavailable");
        return 0;
    }
#ifdef HMBTN_ENH_PAUSE_MENU
    /* GPU Auto submits every captured scene on desktop and Android. The
     * explicit Software mode and per-frame CPU fallback remain available. */
    if (hm_options.renderer) {
        hm_frame_replay_release(hm_gpu_replay);
        hm_gpu_replay = NULL;
        hm_gpu_cached_serial = 0;
        return 0;
    }
#endif
    /* The native output is black while the PS1 display is disabled. Keep this
     * valid blank frame on the GPU instead of converting/uploading black pixels
     * through the software scanout path. */
    if (!hm_gpu_environment_state()->display_enabled) {
        hm_gpu_cached_serial = 0;
        if (!hm_device || !hm_window) {
            hm_host_gpu_coverage_record(0, "GPU device or window unavailable for blank frame");
            return 0;
        }
        return hm_host_gpu_clear_present();
    }
    if (!hm_gpu_output || !hm_gpu_commands) {
        hm_host_gpu_coverage_record(0, "GPU scene renderer unavailable");
        return 0;
    }
    if (native_area->w == 0 || native_area->h == 0) {
        hm_host_gpu_coverage_skip("empty PS1 display area");
        return -1;
    }
    if (native_area->w < 0 || native_area->h < 0) {
        hm_host_gpu_coverage_record(0, "invalid PS1 display area");
        return 0;
    }
    if (!hm_gpu_replay) {
        hm_gpu_replay = hm_frame_replay_acquire();
        hm_gpu_cached_serial = 0;
    }
    /* Diagnostic capture deliberately uses the software reference. */
    if (SDL_getenv("HMBTN_CAPTURE_FRAME")) {
        hm_host_gpu_coverage_record(0, "diagnostic software frame capture requested");
        return 0;
    }
    RECT output_area = *native_area;
#ifdef HMBTN_ENH_WIDESCREEN
    output_area = hm_widescreen_area(output_area);
#endif
    u32 width = (u32)output_area.w, height = (u32)output_area.h;
#if defined(HMBTN_ENH_SCALING) && defined(HMBTN_ENH_PAUSE_MENU)
    if (hm_options.scaling) {
        int w, h;
        if (!SDL_GetWindowSizeInPixels(hm_window, &w, &h) || w <= 0 || h <= 0) {
            hm_host_gpu_coverage_record(0, "GPU output size unavailable");
            return 0;
        }
        hm_host_output_size(&w, &h);
#ifdef __ANDROID__
        /* Retain the mobile allocation guard for any future explicit scaling
         * request; current Android profiles keep this path disabled (ADR-164). */
        if (w > 1280 || h > 720) {
            if (hm_host_wide_active()) {
                w = 1280;
                h = 720;
            } else {
                w = 960;
                h = 720;
            }
        }
#endif
        if (w > 4096 || h > 4096) {
            hm_host_scaling_unavailable();
            hm_host_gpu_coverage_record(0, "GPU output exceeds 4096 pixels per axis");
            return 0;
        }
        width = (u32)w;
        height = (u32)h;
    }
#endif
    hm_render_frame live = hm_game_render_frame();
    u64 serial = hm_gpu_replay ? hm_frame_replay_serial(hm_gpu_replay, &live) : 0;
    int interpolate = 0;
#if defined(HMBTN_ENH_INTERPOLATION) && defined(HMBTN_ENH_PAUSE_MENU)
    interpolate = hm_options.interpolation && live.eligible;
#endif
    u32 texture_filter = 0;
    unsigned multisamples = 1;
#ifdef HM_HOST_AA
    multisamples = hm_antialiasing_samples(hm_options.antialiasing);
#endif
#if defined(HMBTN_ENH_FILTERING) && defined(HMBTN_ENH_PAUSE_MENU)
    texture_filter = (u32)hm_graphics_scene_filter_mode(&hm_options,
#ifdef __ANDROID__
                                                        1
#else
                                                        0
#endif
    );
#endif
    u64 cache_serial = interpolate ? 0 : serial;
    int scanout_only = hm_gpu_replay == NULL;
    if (scanout_only) {
        /* Replay capture is only one source of a frame. If observer or buffer
         * allocation fails, the live VRAM page can still be rendered directly
         * by the GPU before falling back to the CPU reference. */
        hm_gpu_cached_serial = 0;
        hm_gpu_cached = (hm_gpu_scene_data){0};
        if (!hm_host_gpu_scanout_scene(native_area, width, height)) {
            hm_host_gpu_coverage_record(
                0, "frame replay allocation failed and GPU VRAM scanout was unavailable");
            return 0;
        }
    } else if (!cache_serial || hm_gpu_cached_serial != cache_serial ||
        hm_gpu_cached.width != width ||
        hm_gpu_cached.height != height ||
        (hm_gpu_cached.words && (hm_gpu_cached.words[11] != texture_filter ||
                                hm_gpu_cached.words[15] != multisamples))) {
#ifdef __ANDROID__
        Uint64 scene_begin = SDL_GetTicksNS();
#endif
        hm_gpu_cached_serial = 0;
        hm_gpu_cached = (hm_gpu_scene_data){0};
        hm_gpu_scene_texture_filter(hm_gpu_commands, texture_filter);
        hm_gpu_scene_multisamples(hm_gpu_commands, multisamples);
        hm_replay_sink previous = hm_gpu_scene_begin(hm_gpu_commands, width, height);
#ifdef __ANDROID__
        Uint64 replay_begin = SDL_GetTicksNS();
#endif
        int replayed = 0;
#ifdef HMBTN_ENH_INTERPOLATION
        if (interpolate)
            replayed = hm_interpolation_replay(SDL_GetTicksNS() - hm_start, &live);
#endif
        if (!replayed)
            replayed = hm_frame_replay_draw(hm_gpu_replay, &live);
#ifdef __ANDROID__
        Uint64 replay_spent = SDL_GetTicksNS() - replay_begin;
        hm_gpu_perf.replay_ns += replay_spent;
        if (replay_spent > hm_gpu_perf.replay_max_ns)
            hm_gpu_perf.replay_max_ns = replay_spent;
        ++hm_gpu_perf.replays;
#endif
        /* Most scenes need only the immutable pre-draw VRAM snapshot. If a
         * textured primitive reads an earlier scene write, replay once more
         * through the native reference executor to materialize that version. */
        if (replayed && hm_gpu_scene_needs_materialization(hm_gpu_commands)) {
#ifdef __ANDROID__
            Uint64 materialize_begin = SDL_GetTicksNS();
            Uint64 materialize_setup_begin = materialize_begin;
#endif
#ifdef __ANDROID__
            Uint64 materialize_mask_begin = SDL_GetTicksNS();
#endif
            u64 materialization_tiles[HM_VRAM_DIRTY_ROWS];
            u32 materialization_tile_count =
                hm_gpu_scene_materialization_mask(hm_gpu_commands, materialization_tiles);
#ifdef __ANDROID__
            Uint64 materialize_mask_spent = SDL_GetTicksNS() - materialize_mask_begin;
            hm_gpu_perf.materialize_mask_ns += materialize_mask_spent;
            if (materialize_mask_spent > hm_gpu_perf.materialize_mask_max_ns)
                hm_gpu_perf.materialize_mask_max_ns = materialize_mask_spent;
            hm_gpu_perf.materialization_tile_count = materialization_tile_count;
#else
            (void)materialization_tile_count;
#endif
#ifdef __ANDROID__
            Uint64 materialize_bind_begin = SDL_GetTicksNS();
#endif
            hm_gpu_scene_materialize_vram(hm_gpu_commands, 1);
            hm_vram_write_filter_begin(materialization_tiles);
            (void)hm_gpu_scene_begin(hm_gpu_commands, width, height);
#ifdef __ANDROID__
            Uint64 materialize_bind_spent = SDL_GetTicksNS() - materialize_bind_begin;
            hm_gpu_perf.materialize_bind_ns += materialize_bind_spent;
            if (materialize_bind_spent > hm_gpu_perf.materialize_bind_max_ns)
                hm_gpu_perf.materialize_bind_max_ns = materialize_bind_spent;
            Uint64 materialize_setup_spent = SDL_GetTicksNS() - materialize_setup_begin;
            hm_gpu_perf.materialize_setup_ns += materialize_setup_spent;
            if (materialize_setup_spent > hm_gpu_perf.materialize_setup_max_ns)
                hm_gpu_perf.materialize_setup_max_ns = materialize_setup_spent;
            Uint64 materialize_replay_begin = SDL_GetTicksNS();
#endif
            replayed = hm_frame_replay_draw(hm_gpu_replay, &live);
#ifdef __ANDROID__
            Uint64 materialize_replay_spent = SDL_GetTicksNS() - materialize_replay_begin;
            hm_gpu_perf.materialize_replay_ns += materialize_replay_spent;
            if (materialize_replay_spent > hm_gpu_perf.materialize_replay_max_ns)
                hm_gpu_perf.materialize_replay_max_ns = materialize_replay_spent;
#endif
            hm_vram_write_filter_end();
            hm_gpu_scene_materialize_vram(hm_gpu_commands, 0);
#ifdef __ANDROID__
            Uint64 materialize_spent = SDL_GetTicksNS() - materialize_begin;
            hm_gpu_perf.materialize_ns += materialize_spent;
            if (materialize_spent > hm_gpu_perf.materialize_max_ns)
                hm_gpu_perf.materialize_max_ns = materialize_spent;
            ++hm_gpu_perf.materialized_replays;
#endif
        }
#ifdef __ANDROID__
        Uint64 pack_begin = SDL_GetTicksNS();
#endif
        int scene_ready = hm_gpu_scene_end(hm_gpu_commands, previous, &hm_gpu_cached);
#ifdef __ANDROID__
        Uint64 pack_spent = SDL_GetTicksNS() - pack_begin;
        hm_gpu_perf.pack_ns += pack_spent;
        if (pack_spent > hm_gpu_perf.pack_max_ns)
            hm_gpu_perf.pack_max_ns = pack_spent;
        ++hm_gpu_perf.packs;
        Uint64 scene_spent = SDL_GetTicksNS() - scene_begin;
        hm_gpu_perf.scene_ns += scene_spent;
        if (scene_spent > hm_gpu_perf.scene_max_ns)
            hm_gpu_perf.scene_max_ns = scene_spent;
        ++hm_gpu_perf.scenes;
        if (scene_ready) {
            hm_gpu_perf.width = width;
            hm_gpu_perf.height = height;
            hm_gpu_perf.primitive_count = hm_gpu_cached.primitive_count;
            hm_gpu_perf.tile_count = hm_gpu_cached.tile_count;
            hm_gpu_perf.max_bin_count = hm_gpu_cached.max_bin_count;
            hm_gpu_perf.empty_bin_count = hm_gpu_cached.empty_bin_count;
            hm_gpu_perf.shared_cache_bin_count = hm_gpu_cached.shared_cache_bin_count;
            hm_gpu_perf.total_bin_references = hm_gpu_cached.total_bin_references;
            hm_gpu_perf.vram_version_count = hm_gpu_cached.vram_version_count;
            hm_gpu_perf.scene_bytes = hm_gpu_cached.bytes;
        }
#endif
        if (!scene_ready || !replayed) {
            const char *reason = serial
                                     ? hm_gpu_scene_fallback(hm_gpu_commands)
                                     : hm_frame_replay_failure(hm_gpu_replay);
            if (hm_host_gpu_scanout_scene(native_area, width, height)) {
                /* The PS1 VRAM is authoritative even when a captured scene
                 * cannot be replayed or packed. Scan the exact visible page on
                 * the GPU and keep CPU fallback for scanout/device failures. */
                scanout_only = 1;
            } else {
                hm_host_gpu_coverage_record(
                    0, reason ? reason
                              : "no complete captured scene (loading, bypass, or invalidated frame)");
                return 0;
            }
        }
        if (!scanout_only)
            hm_gpu_cached_serial = cache_serial;
    }
#ifdef __ANDROID__
    Uint64 acquire_begin = SDL_GetTicksNS();
#endif
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
    if (!command)
        hm_host_fail();
    SDL_GPUTexture *swapchain = NULL;
    Uint32 window_width, window_height;
    if (!SDL_AcquireGPUSwapchainTexture(command, hm_window, &swapchain, &window_width,
                                        &window_height)) {
        SDL_CancelGPUCommandBuffer(command);
        hm_host_fail();
    }
    if (!swapchain) {
        SDL_CancelGPUCommandBuffer(command);
#ifdef __ANDROID__
        Uint64 acquire_spent = SDL_GetTicksNS() - acquire_begin;
        hm_gpu_perf.acquire_ns += acquire_spent;
        if (acquire_spent > hm_gpu_perf.acquire_max_ns)
            hm_gpu_perf.acquire_max_ns = acquire_spent;
        ++hm_gpu_perf.acquires;
#endif
        hm_gpu_cached_serial = 0;
        hm_host_gpu_coverage_skip("GPU swapchain temporarily unavailable");
        return -1;
    }
#ifdef __ANDROID__
    Uint64 acquire_spent = SDL_GetTicksNS() - acquire_begin;
    hm_gpu_perf.acquire_ns += acquire_spent;
    if (acquire_spent > hm_gpu_perf.acquire_max_ns)
        hm_gpu_perf.acquire_max_ns = acquire_spent;
    ++hm_gpu_perf.acquires;
    Uint64 encode_begin = SDL_GetTicksNS();
#endif
    SDL_GPUTexture *image = hm_gpu_renderer_encode(hm_gpu_output, command, &hm_gpu_cached);
#ifdef __ANDROID__
    Uint64 encode_spent = SDL_GetTicksNS() - encode_begin;
    hm_gpu_perf.encode_ns += encode_spent;
    if (encode_spent > hm_gpu_perf.encode_max_ns)
        hm_gpu_perf.encode_max_ns = encode_spent;
    ++hm_gpu_perf.encodes;
#endif
    if (!image) {
        SDL_CancelGPUCommandBuffer(command);
        /* The scene was cached before encode. A failed encode must not make a
         * later attempt look like a successfully presented frame. */
        hm_gpu_cached_serial = 0;
        hm_host_gpu_coverage_record(0, SDL_GetError());
        return 0;
    }
    int linear = 0;
#ifdef HMBTN_ENH_PAUSE_MENU
    linear = hm_options.linear;
#endif
#ifdef __ANDROID__
    Uint64 blit_begin = SDL_GetTicksNS();
#endif
    hm_host_blit(command, image, width, height, swapchain, window_width, window_height, linear,
                 hm_host_wide_active());
#ifdef __ANDROID__
    Uint64 blit_spent = SDL_GetTicksNS() - blit_begin;
    hm_gpu_perf.blit_ns += blit_spent;
    if (blit_spent > hm_gpu_perf.blit_max_ns)
        hm_gpu_perf.blit_max_ns = blit_spent;
    ++hm_gpu_perf.blits;
    Uint64 submit_begin = SDL_GetTicksNS();
#endif
    if (!SDL_SubmitGPUCommandBuffer(command))
        hm_host_fail();
#ifdef __ANDROID__
    Uint64 submit_spent = SDL_GetTicksNS() - submit_begin;
    hm_gpu_perf.submit_ns += submit_spent;
    if (submit_spent > hm_gpu_perf.submit_max_ns)
        hm_gpu_perf.submit_max_ns = submit_spent;
    ++hm_gpu_perf.submits;
#endif
    if (scanout_only)
        ++hm_gpu_coverage_scanouts;
    hm_host_gpu_coverage_record(1, NULL);
    return 1;
}
#endif

#ifdef HMBTN_ENH_PAUSE_MENU
#ifdef HMBTN_ENH_SHIPPING_BIN
static u16 hm_pause_shipping_ids[ITEM_COUNT];
static u8 hm_pause_shipping_actions[ITEM_COUNT];
static char hm_pause_shipping_text[ITEM_COUNT][32];
static u32 hm_pause_item_icon_pixels[ITEM_COUNT][HM_PAUSE_ITEM_ICON_SIZE * HM_PAUSE_ITEM_ICON_SIZE];
static u8 hm_pause_item_icon_state[ITEM_COUNT];
static hm_vram_context hm_pause_item_icon_vram;

enum { HM_PAUSE_SHIPPING_ROW_ITEM, HM_PAUSE_SHIPPING_ROW_SHIP_BASKET };

static u32 hm_host_pause_icon_texel(const FarmGridEntry *entry, int page_x, int x, int y) {
    u16 packed = hm_vram_pixel((u32)(page_x + entry->x + x / 4), (u32)(entry->y + y));
    u16 color_index = (packed >> ((x & 3) * 4)) & 15;
    if (color_index == 0)
        return 0;
    u16 color = hm_vram_pixel((u32)(entry->clutX + color_index), (u32)entry->clutY);
    if (color == 0)
        return 0;
    u32 red = color & 31;
    u32 green = (color >> 5) & 31;
    u32 blue = (color >> 10) & 31;
    red = (red << 3) | (red >> 2);
    green = (green << 3) | (green >> 2);
    blue = (blue << 3) | (blue >> 2);
    return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

/* Load a game's own 4bpp item sprite into private VRAM and cache its 16x16 icon.
 * The farm's live picture-slot tables and VRAM are restored before returning. */
static const u32 *hm_host_pause_item_icon(int id) {
    if (id <= ITEM_NOTHING || id >= ITEM_COUNT)
        return NULL;
    if (hm_pause_item_icon_state[id] == 2)
        return hm_pause_item_icon_pixels[id];
    if (hm_pause_item_icon_state[id] != 0)
        return NULL;
    hm_pause_item_icon_state[id] = 1;

    s16 saved_ids[PICTURE_ROWS][PICTURE_SLOTS];
    FarmGridEntry saved_entries[PICTURE_ROWS][PICTURE_SLOTS];
    memcpy(saved_ids, farm_area.mode.pictureIds, sizeof(saved_ids));
    memcpy(saved_entries, farm_area.mode.pictureEntries, sizeof(saved_entries));
    memset(farm_area.mode.pictureIds, 0, sizeof(farm_area.mode.pictureIds));
    memset(farm_area.mode.pictureEntries, 0xFF, sizeof(farm_area.mode.pictureEntries));

    hm_vram_context *previous_vram = hm_vram_bind(&hm_pause_item_icon_vram);
    hm_vram_reset();
    int loaded = picture_load_to_vram(id);
    FarmGridEntry *picture = NULL;
    if (loaded) {
        for (int row = 0; row < PICTURE_ROWS && !picture; ++row) {
            for (int slot = 0; slot < PICTURE_SLOTS; ++slot) {
                if (farm_area.mode.pictureIds[row][slot] == id) {
                    picture = &farm_area.mode.pictureEntries[row][slot];
                    break;
                }
            }
        }
    }

    int width = picture ? picture->w : 0;
    int height = picture ? picture->h : 0;
    int page_x = picture && picture->clutX >= 1008 ? 960 : 832;
    if (width <= 0 || height <= 0) {
        width = picture ? picture->w2 : 0;
        height = picture ? picture->h2 : 0;
    }
    if (width > 0 && height > 0 && width <= 256 && height <= 256 && picture) {
        int output_width = width >= height ? HM_PAUSE_ITEM_ICON_SIZE
                                           : width * HM_PAUSE_ITEM_ICON_SIZE / height;
        int output_height = height >= width ? HM_PAUSE_ITEM_ICON_SIZE
                                            : height * HM_PAUSE_ITEM_ICON_SIZE / width;
        if (output_width < 1) output_width = 1;
        if (output_height < 1) output_height = 1;
        int x_offset = (HM_PAUSE_ITEM_ICON_SIZE - output_width) / 2;
        int y_offset = (HM_PAUSE_ITEM_ICON_SIZE - output_height) / 2;
        for (int y = 0; y < output_height; ++y) {
            int source_y = y * height / output_height;
            for (int x = 0; x < output_width; ++x) {
                int source_x = x * width / output_width;
                hm_pause_item_icon_pixels[id][(y + y_offset) * HM_PAUSE_ITEM_ICON_SIZE +
                                              x + x_offset] =
                    hm_host_pause_icon_texel(picture, page_x, source_x, source_y);
            }
        }
    }

    memcpy(farm_area.mode.pictureIds, saved_ids, sizeof(saved_ids));
    memcpy(farm_area.mode.pictureEntries, saved_entries, sizeof(saved_entries));
    hm_vram_bind(previous_vram);
    int has_pixels = 0;
    for (int i = 0; i < HM_PAUSE_ITEM_ICON_SIZE * HM_PAUSE_ITEM_ICON_SIZE; ++i)
        has_pixels |= hm_pause_item_icon_pixels[id][i] != 0;
    hm_pause_item_icon_state[id] = has_pixels ? 2 : 3;
    return has_pixels ? hm_pause_item_icon_pixels[id] : NULL;
}

static const char *hm_pause_item_name(int id) {
    static const char *const crop_names[15] = {
        "Turnip", "Potato", "Cucumber", "Strawberry", "Cabbage", "Tomato", "Corn",
        "Onion", "Pumpkin", "Pineapple", "Eggplant", "Carrot", "Sweet Potato",
        "Spinach", "Green Pepper"};
    if (id >= ITEM_TURNIP && id < ITEM_GRASS)
        return crop_names[id - ITEM_TURNIP];
    switch (id) {
    case ITEM_GRASS: return "Grass";
    case ITEM_MOON_DROP_FLOWER: return "Moon Drop";
    case ITEM_PINK_CAT_FLOWER: return "Pink Cat";
    case ITEM_BLUE_MAGIC_RED_FLOWER: return "Blue Flower";
    case ITEM_RED_MAGIC_RED_FLOWER: return "Red Flower";
    case ITEM_TOY_FLOWER: return "Toy Flower";
    case ITEM_ORANGE_CUP_FRUIT: return "Orange Cup";
    case ITEM_APPLE: return "Apple";
    case ITEM_BAMBOO_SHOOT: return "Bamboo Shoot";
    case ITEM_WILD_GRAPE: return "Wild Grape";
    case ITEM_MUSHROOM: return "Mushroom";
    case ITEM_POISONOUS_MUSHROOM: return "Poison Mushroom";
    case ITEM_TRUFFLE: return "Truffle";
    case ITEM_BLUE_GRASS: return "Blue Grass";
    case ITEM_RED_GRASS: return "Red Grass";
    case ITEM_GREEN_GRASS: return "Green Grass";
    case ITEM_GOLDEN_EGG: return "Golden Egg";
    case ITEM_NORMAL_EGG: return "Egg";
    case ITEM_GOOD_EGG: return "Good Egg";
    case ITEM_EXCELLENT_EGG: return "Excellent Egg";
    case ITEM_MILK_S: return "Small Milk";
    case ITEM_MILK_M: return "Medium Milk";
    case ITEM_MILK_L: return "Large Milk";
    case ITEM_MILK_G: return "Gold Milk";
    case ITEM_WOOL_S: return "Small Wool";
    case ITEM_WOOL_M: return "Medium Wool";
    case ITEM_WOOL_L: return "Large Wool";
    case ITEM_WOOL_G: return "Gold Wool";
    case ITEM_MAYONNAISE_S: return "Small Mayo";
    case ITEM_MAYONNAISE_M: return "Medium Mayo";
    case ITEM_MAYONNAISE_L: return "Large Mayo";
    case ITEM_MAYONNAISE_G: return "Gold Mayo";
    case ITEM_CHEESE_S: return "Small Cheese";
    case ITEM_CHEESE_M: return "Medium Cheese";
    case ITEM_CHEESE_L: return "Large Cheese";
    case ITEM_CHEESE_G: return "Gold Cheese";
    case ITEM_YARN_BALL_S: return "Small Yarn";
    case ITEM_YARN_BALL_M: return "Medium Yarn";
    case ITEM_YARN_BALL_L: return "Large Yarn";
    case ITEM_YARN_G: return "Gold Yarn";
    case ITEM_LARGE_FISH: return "Large Fish";
    case ITEM_MEDIUM_FISH: return "Medium Fish";
    case ITEM_SMALL_FISH: return "Small Fish";
    default: return "Item";
    }
}

static int hm_host_shipping_build_rows(hm_pause_view *view, int portable_basket) {
    int count = 0;
    view->shipping_basket = portable_basket;
    for (int id = 1; id < ITEM_COUNT && count < HM_PAUSE_SHIPPING_ITEMS; ++id) {
        int amount = hm_gameplay_shipping_count(id);
        if (amount <= 0)
            continue;
        hm_pause_shipping_ids[count] = (u16)id;
        hm_pause_shipping_actions[count] = HM_PAUSE_SHIPPING_ROW_ITEM;
        SDL_snprintf(hm_pause_shipping_text[count], sizeof(hm_pause_shipping_text[count]),
                     "%s", hm_pause_item_name(id));
        view->shipping_items[count] = hm_pause_shipping_text[count];
        view->shipping_icons[count] = hm_host_pause_item_icon(id);
        view->shipping_quantities[count] = amount;
        ++count;
    }
    if (portable_basket && count < HM_PAUSE_SHIPPING_ITEMS) {
        hm_pause_shipping_ids[count] = ITEM_NOTHING;
        hm_pause_shipping_actions[count] = HM_PAUSE_SHIPPING_ROW_SHIP_BASKET;
        SDL_snprintf(hm_pause_shipping_text[count], sizeof(hm_pause_shipping_text[count]),
                     "Ship Basket");
        view->shipping_items[count] = hm_pause_shipping_text[count];
        view->shipping_icons[count] = hm_host_pause_item_icon(ITEM_BASKET);
        view->shipping_quantities[count] = game_state.progress.estate.basket.count;
        ++count;
    }
    view->shipping_count = count;
    return count;
}

static int hm_host_shipping_slot_at(int x, int y) {
    if (x < 92 || x >= 92 + HM_PAUSE_SHIPPING_COLUMNS * 34 || y < 54 ||
        y >= 54 + HM_PAUSE_SHIPPING_ROWS * 34)
        return -1;
    int column = (x - 92) / 34;
    int row = (y - 54) / 34;
    return row < HM_PAUSE_SHIPPING_ROWS ? row * HM_PAUSE_SHIPPING_COLUMNS + column : -1;
}

static int hm_host_shipping_page_button_at(int x, int y) {
    if (y < 36 || y >= 50)
        return 0;
    if (x >= 206 && x < 219)
        return -1;
    if (x >= 285 && x < 298)
        return 1;
    return 0;
}

static int hm_host_shipping_change_page(hm_pause_menu *menu, int count, int direction) {
    int page_count = (count + HM_PAUSE_SHIPPING_VISIBLE_ITEMS - 1) /
                     HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
    int page = menu->first_visible / HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
    int next_page = page + direction;
    if (next_page < 0 || next_page >= page_count)
        return 0;
    menu->first_visible = next_page * HM_PAUSE_SHIPPING_VISIBLE_ITEMS;
    menu->selected = menu->first_visible;
    return 1;
}

static int hm_host_shipping_withdraw(int selected) {
    if (selected < 0 || selected >= ITEM_COUNT)
        return 0;
    u16 id = hm_pause_shipping_ids[selected];
    if (hm_gameplay_shipping_count(id) <= 0)
        return 0;
    int bin = estate_item_slot_index(id);
    int price = item_sell_price(id);
    if (price < 0 || bin >= 25 ||
        (bin >= 0 && game_state.progress.estate.shipment.counts[bin] == 0))
        return 0;
    int slots = game_state.player.itemSlots;
    if (slots < 0 || slots > HM_PAUSE_RUCKSACK_ITEMS)
        return 0;

    FarmStoredItem *destination = NULL;
    if (hm_gameplay_item_stacking()) {
        for (int i = 0; i < slots; ++i) {
            FarmStoredItem *item = &game_state.player.items[i];
            if (item->id == id && item->count > 0 && item->count < HM_STACK_MAX) {
                destination = item;
                ++destination->count;
                break;
            }
        }
    }
    if (!destination) {
        for (int i = 0; i < slots; ++i) {
            if (game_state.player.items[i].id == ITEM_NOTHING) {
                destination = &game_state.player.items[i];
                picture_load_to_vram(id);
                stored_item_set(destination, (s16)id, 0, 1);
                break;
            }
        }
    }
    if (!destination)
        return 0;

    if (bin >= 0)
        game_state.progress.estate.shipment.counts[bin]--;
    if (game_state.progress.estate.shipment.shippedValue >= price)
        game_state.progress.estate.shipment.shippedValue -= price;
    else
        game_state.progress.estate.shipment.shippedValue = 0;
    if (id == ITEM_LARGE_FISH || id == ITEM_MEDIUM_FISH || id == ITEM_SMALL_FISH) {
        if (game_state.progress.estate.shipmentTotal > 0)
            game_state.progress.estate.shipmentTotal--;
        if (game_state.progress.estate.shipmentTotal < 200)
            event_flag_set(EVENT_FLAG_FISH_SHIPPED_200, 0);
    }
    hm_gameplay_shipping_forget(id);
    return 1;
}
#endif

#ifdef __ANDROID__
static int hm_host_pause_virtual_pad_input(SDL_Scancode scancode, hm_pause_input *input) {
    static const SDL_Scancode keys[HM_VIRTUAL_PAD_ACTIONS] = {
        SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4,
        SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8,
        SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
        SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK};
    if (!hm_controls.virtual_gamepad_enabled || !input)
        return 0;
    for (int i = 0; i < HM_VIRTUAL_PAD_ACTIONS; ++i) {
        if (keys[i] != scancode)
            continue;
        switch (hm_controls.virtual_pad_mapping[i]) {
        case 12: *input = HM_PAUSE_UP; return 1;
        case 13: *input = HM_PAUSE_RIGHT; return 1;
        case 14: *input = HM_PAUSE_DOWN; return 1;
        case 15: *input = HM_PAUSE_LEFT; return 1;
        case 6: *input = HM_PAUSE_CONFIRM; return 1;
        case 5:
        case 11: *input = HM_PAUSE_BACK; return 1;
        default: return 0;
        }
    }
    return 0;
}
#endif

static int hm_host_pause_held_navigation(hm_pause_input *input) {
    if (!input)
        return 0;
    if (hm_active_gamepad) {
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
            *input = HM_PAUSE_UP;
            return 1;
        }
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
            *input = HM_PAUSE_DOWN;
            return 1;
        }
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
            *input = HM_PAUSE_LEFT;
            return 1;
        }
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
            *input = HM_PAUSE_RIGHT;
            return 1;
        }
        Sint16 x = SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        Sint16 y = SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        if (y < -16000) {
            *input = HM_PAUSE_UP;
            return 1;
        }
        if (y > 16000) {
            *input = HM_PAUSE_DOWN;
            return 1;
        }
        if (x < -16000) {
            *input = HM_PAUSE_LEFT;
            return 1;
        }
        if (x > 16000) {
            *input = HM_PAUSE_RIGHT;
            return 1;
        }
    }
#ifdef __ANDROID__
    const bool *keys = SDL_GetKeyboardState(NULL);
    static const SDL_Scancode virtual_pad_keys[HM_VIRTUAL_PAD_ACTIONS] = {
        SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4,
        SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8,
        SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
        SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK};
    if (keys && hm_controls.virtual_gamepad_enabled) {
        for (int i = 0; i < HM_VIRTUAL_PAD_ACTIONS; ++i) {
            hm_pause_input candidate;
            if (keys[virtual_pad_keys[i]] &&
                hm_host_pause_virtual_pad_input(virtual_pad_keys[i], &candidate) &&
                (candidate == HM_PAUSE_UP || candidate == HM_PAUSE_DOWN ||
                 candidate == HM_PAUSE_LEFT || candidate == HM_PAUSE_RIGHT)) {
                *input = candidate;
                return 1;
            }
        }
    }
#endif
    return 0;
}

static void hm_host_pause_queue_navigation(hm_pause_input input) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(hm_window);
    event.key.down = true;
    event.key.repeat = true;
    switch (input) {
    case HM_PAUSE_UP:
        event.key.key = SDLK_UP;
        event.key.scancode = SDL_SCANCODE_UP;
        break;
    case HM_PAUSE_DOWN:
        event.key.key = SDLK_DOWN;
        event.key.scancode = SDL_SCANCODE_DOWN;
        break;
    case HM_PAUSE_LEFT:
        event.key.key = SDLK_LEFT;
        event.key.scancode = SDL_SCANCODE_LEFT;
        break;
    case HM_PAUSE_RIGHT:
        event.key.key = SDLK_RIGHT;
        event.key.scancode = SDL_SCANCODE_RIGHT;
        break;
    default:
        return;
    }
    SDL_PushEvent(&event);
}

static void hm_host_pause(int shipping) {
    hm_host_in_pause = 1;
#ifdef __ANDROID__
    (void)shipping;
#endif
    hm_pause_menu menu;
    hm_pause_open(&menu);
    menu.graphics_modern = hm_options.profile == HM_GRAPHICS_PROFILE_MODERN;
#ifdef HMBTN_ENH_SHIPPING_BIN
    int portable_basket = shipping == HM_SHIPPING_REQUEST_PORTABLE_BASKET;
    if (shipping) {
        menu.page = HM_PAUSE_SHIPPING;
        menu.content_count = hm_host_shipping_build_rows(&(hm_pause_view){0}, portable_basket);
        if (menu.content_count == 0)
            menu.content_count = 1;
    }
#else
    (void)shipping;
#endif
    Uint64 before = SDL_GetTicksNS();
    hm_audio_output_pause(1);
    hm_host_reset_presentation();
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
#ifdef HMBTN_ENH_INTERPOLATION
    hm_interpolated_view interpolated = {0};
    hm_render_frame live = hm_game_render_frame();
    if (state->display_enabled &&
        hm_interpolation_render(SDL_GetTicksNS() - hm_start, &live, &interpolated))
        area = &interpolated.area;
#endif
    u32 pause_width = 320;
#ifdef HMBTN_ENH_WIDESCREEN
    hm_software_view wide = {0};
    hm_software_output *frozen_output = NULL;
    RECT wide_area = hm_widescreen_area(*area);
    if (state->display_enabled && wide_area.w > 320 && wide_area.h == 240) {
        frozen_output = hm_software_output_create();
        if (frozen_output && hm_software_output_prepare(frozen_output, wide_area.w, 240)) {
            hm_render_frame frozen_frame = hm_game_render_frame();
            hm_replay_sink previous = hm_software_output_begin(frozen_output);
            int replayed = 0;
#ifdef HMBTN_ENH_INTERPOLATION
            if (hm_options.interpolation && frozen_frame.eligible)
                replayed = hm_interpolation_replay(SDL_GetTicksNS() - hm_start, &frozen_frame);
#endif
            if (!replayed)
                replayed = hm_software_output_replay(frozen_output, &frozen_frame);
            int completed = hm_software_output_end(frozen_output, previous, &wide);
            if (replayed && completed)
                pause_width = wide.width;
            else
                wide = (hm_software_view){0};
        }
    }
#endif
    for (unsigned y = 0; y < 240; ++y)
        for (unsigned x = 0; x < pause_width; ++x) {
            u16 color = state->display_enabled && area->w > 0 && area->h > 0
                            ? hm_vram_pixel((u32)area->x + x * (u32)area->w / 320,
                                            (u32)area->y + y * (u32)area->h / 240)
                            : 0;
#ifdef HMBTN_ENH_INTERPOLATION
            if (interpolated.pixels)
                color = interpolated
                            .pixels[((u32)area->y + y * (u32)area->h / 240) * interpolated.stride +
                                    (u32)area->x + x * (u32)area->w / 320];
#endif
#ifdef HMBTN_ENH_WIDESCREEN
            if (wide.pixels)
                color = wide.pixels[y * pause_width + x];
#endif
            u8 *out = hm_pause_frozen + (y * pause_width + x) * 4;
            for (unsigned k = 0; k < 3; ++k) {
                unsigned value = (color >> (k * 5)) & 31;
                out[k] = (u8)((value << 3) | (value >> 2));
            }
            out[3] = 255;
        }
#ifdef HMBTN_ENH_WIDESCREEN
    hm_software_output_destroy(frozen_output);
#endif
    if (!hm_host_image_size(pause_width, 240))
        hm_host_fail();
    int running = 1;
    int repeat_navigation = 0;
    hm_pause_input repeat_direction = HM_PAUSE_UP;
    Uint64 next_repeat_ns = 0;
    int keyboard_capture_item = -1;
    const char *notice = "";
    char value[128] = "";
    char rows[HM_PAUSE_MAX_VIEW_ITEMS][32] = {{0}};
    hm_pause_view view = {.detail = value};
    for (int i = 0; i < HM_PAUSE_MAX_VIEW_ITEMS; ++i)
        view.values[i] = rows[i];
#ifdef HMBTN_ENH_SHIPPING_BIN
        if (shipping)
            hm_host_shipping_build_rows(&view, portable_basket);
#endif
    int value_dirty = 1;
    hm_mouse_reset();
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            value_dirty = 1;
            if (event.type == SDL_EVENT_QUIT) {
                hm_game_host_stop();
                exit(0);
            }
            hm_host_display_event(event.type);
            if (keyboard_capture_item >= 0) {
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_AC_BACK) {
                        notice = "Key binding cancelled";
                    } else if (hm_controls_bind(&hm_controls, keyboard_capture_item,
                                                event.key.scancode)) {
                        notice = hm_controls_save(&hm_controls) ? "Key binding saved" :
                                                                  "Cannot save settings";
                    } else {
                        notice = "Key is already assigned or unavailable";
                    }
                    keyboard_capture_item = -1;
                    value_dirty = 1;
                }
                continue;
            }
            hm_pause_input input;
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                switch (event.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_DPAD_UP:
                    input = HM_PAUSE_UP; break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                    input = HM_PAUSE_DOWN; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                    input = HM_PAUSE_LEFT; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                    input = HM_PAUSE_RIGHT; break;
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    input = HM_PAUSE_CONFIRM; break;
                case SDL_GAMEPAD_BUTTON_EAST:
                case SDL_GAMEPAD_BUTTON_START:
                    input = HM_PAUSE_BACK; break;
                default:
                    continue;
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                int win_w = 0, win_h = 0;
                /* ADR-135: Android touch events are in physical pixels; use pixel size. */
#ifdef __ANDROID__
                if (SDL_GetWindowSizeInPixels(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
#else
                if (SDL_GetWindowSize(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
#endif
                    int vx, vy;
                    if (hm_mouse_calc_virtual_pos(event.motion.x, event.motion.y, win_w, win_h, &vx, &vy)) {
                        if (menu.page == HM_PAUSE_SHIPPING) {
                            int slot = hm_host_shipping_slot_at(vx, vy);
                            if (slot >= 0) {
                                int idx = menu.first_visible + slot;
                                if (idx < view.shipping_count && menu.selected != idx) {
                                    menu.selected = idx;
                                    value_dirty = 1;
                                }
                            }
                        } else if (menu.page == HM_PAUSE_GRAPHICS || menu.page == HM_PAUSE_GAMEPLAY ||
                            menu.page == HM_PAUSE_CONTROLS || menu.page == HM_PAUSE_AUDIO ||
                            menu.page == HM_PAUSE_BLESSINGS) {
                            if (vx >= 20 && vx < 300 && vy >= 45 && vy < 45 + HM_PAUSE_VISIBLE_ROWS * 20) {
                                int row = (vy - 45) / 20;
                                int idx = menu.first_visible + row;
                                if (idx < hm_pause_count(&menu) && menu.selected != idx) {
                                    menu.selected = idx;
                                    value_dirty = 1;
                                }
                            }
                        } else {
                            int root = menu.page == HM_PAUSE_ROOT;
                            int top = root ? 54 : 38;
                            int row_start = top + (root ? 44 : 42);
                            int row_step = root ? 20 : 24;
                            int count = hm_pause_count(&menu);
                            if (vx >= 80 && vx < 240) {
                                for (int i = 0; i < count; ++i) {
                                    int ry = row_start + i * row_step - (root ? 3 : 4);
                                    if (vy >= ry && vy < ry + (root ? 19 : 22)) {
                                        if (menu.selected != i) {
                                            menu.selected = i;
                                            value_dirty = 1;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                continue;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    input = HM_PAUSE_BACK;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    int win_w = 0, win_h = 0;
                    /* ADR-135: Android touch events are in physical pixels; use pixel size. */
#ifdef __ANDROID__
                    if (SDL_GetWindowSizeInPixels(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
#else
                    if (SDL_GetWindowSize(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
#endif
                        int vx, vy;
                        if (hm_mouse_calc_virtual_pos(event.button.x, event.button.y, win_w, win_h, &vx, &vy)) {
                            if (menu.page == HM_PAUSE_SHIPPING) {
                                int page_direction = hm_host_shipping_page_button_at(vx, vy);
                                if (page_direction != 0) {
                                    hm_host_shipping_change_page(&menu, view.shipping_count,
                                                                 page_direction);
                                    continue;
                                }
                                int slot = hm_host_shipping_slot_at(vx, vy);
                                if (slot >= 0) {
                                    int idx = menu.first_visible + slot;
                                    if (idx < view.shipping_count) {
                                        menu.selected = idx;
                                        input = HM_PAUSE_CONFIRM;
                                    } else {
                                        continue;
                                    }
                                } else {
                                    continue;
                                }
                            } else if (menu.page == HM_PAUSE_GRAPHICS || menu.page == HM_PAUSE_GAMEPLAY ||
                                menu.page == HM_PAUSE_CONTROLS || menu.page == HM_PAUSE_AUDIO ||
                                menu.page == HM_PAUSE_BLESSINGS) {
                                if (vx >= 20 && vx < 300 && vy >= 45 && vy < 45 + HM_PAUSE_VISIBLE_ROWS * 20) {
                                    int row = (vy - 45) / 20;
                                    int idx = menu.first_visible + row;
                                    if (idx < hm_pause_count(&menu)) {
                                        menu.selected = idx;
                                        input = hm_pause_pointer_row_input(menu.page, vx);
                                    } else {
                                        continue;
                                    }
                                } else {
                                    continue;
                                }
                            } else {
                                int root = menu.page == HM_PAUSE_ROOT;
                                int top = root ? 54 : 38;
                                int row_start = top + (root ? 44 : 42);
                                int row_step = root ? 20 : 24;
                                int count = hm_pause_count(&menu);
                                int found = 0;
                                if (vx >= 80 && vx < 240) {
                                    for (int i = 0; i < count; ++i) {
                                        int ry = row_start + i * row_step - (root ? 3 : 4);
                                        if (vy >= ry && vy < ry + (root ? 19 : 22)) {
                                            menu.selected = i;
                                            input = HM_PAUSE_CONFIRM;
                                            found = 1;
                                            break;
                                        }
                                    }
                                }
                                if (!found)
                                    continue;
                            }
                        } else {
                            continue;
                        }
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (event.wheel.y > 0)
                    input = HM_PAUSE_UP;
                else if (event.wheel.y < 0)
                    input = HM_PAUSE_DOWN;
                else
                    continue;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.repeat && event.key.key != SDLK_UP && event.key.key != SDLK_DOWN &&
                    event.key.key != SDLK_LEFT && event.key.key != SDLK_RIGHT &&
                    event.key.key != SDLK_W && event.key.key != SDLK_A &&
                    event.key.key != SDLK_S && event.key.key != SDLK_D)
                    continue;
#ifdef __ANDROID__
                if (hm_host_pause_virtual_pad_input(event.key.scancode, &input)) {
                    /* The Android virtual pad keeps its configured PS1 mappings
                     * while the game tick is paused inside this menu loop. */
                } else
#endif
                {
                    switch (event.key.key) {
                    case SDLK_ESCAPE:
                    case SDLK_AC_BACK:
                    case SDLK_BACKSPACE:
                    case SDLK_J:
                        input = HM_PAUSE_BACK;
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_X:
                    case SDLK_K:
                    case SDLK_SPACE:
                        input = HM_PAUSE_CONFIRM;
                        break;
                    case SDLK_UP:
                    case SDLK_W:
                        input = HM_PAUSE_UP;
                        break;
                    case SDLK_DOWN:
                    case SDLK_S:
                        input = HM_PAUSE_DOWN;
                        break;
                    case SDLK_LEFT:
                    case SDLK_A:
                        input = HM_PAUSE_LEFT;
                        break;
                    case SDLK_RIGHT:
                    case SDLK_D:
                        input = HM_PAUSE_RIGHT;
                        break;
                    default:
                        continue;
                    }
                }
            } else {
                continue;
            }
            if (input == HM_PAUSE_UP || input == HM_PAUSE_DOWN || input == HM_PAUSE_LEFT ||
                input == HM_PAUSE_RIGHT) {
                repeat_navigation = 1;
                repeat_direction = input;
                if (!(event.type == SDL_EVENT_KEY_DOWN && event.key.repeat))
                    next_repeat_ns = SDL_GetTicksNS() + 350000000;
            } else {
                repeat_navigation = 0;
            }
            hm_pause_action action = hm_pause_handle(&menu, input);
            notice = "";
            if (action == HM_PAUSE_RESUME) {
                running = 0;
                break;
            }
            if (action == HM_PAUSE_EXIT) {
                hm_game_host_stop();
                exit(0);
            }
            if (action == HM_PAUSE_CHANGE_GRAPHICS) {
                int g_item = menu.selected;
                if (hm_graphics_change(hm_device, hm_window, &hm_options, g_item,
                                       input == HM_PAUSE_LEFT ? -1 : 1)) {
                    menu.graphics_modern = hm_options.profile == HM_GRAPHICS_PROFILE_MODERN;
#ifdef HMBTN_ENH_PGXP
                    hm_pgxp_set_enabled(hm_options.pgxp);
#endif
#ifdef HMBTN_ENH_DITHERING
                    hm_dither_set_enabled(hm_options.dither);
#endif
#ifdef HMBTN_ENH_WIDESCREEN
                    int changed_w, changed_h;
                    if (SDL_GetWindowSizeInPixels(hm_window, &changed_w, &changed_h))
                        hm_widescreen_configure(hm_options.widescreen, changed_w, changed_h);
#endif
                    hm_host_reset_presentation();
                    notice = hm_graphics_save(&hm_options) ? "" : "Cannot save settings";
                } else {
                    notice = "Option unavailable";
#ifdef HM_HOST_AA
                    if (menu.selected == 10 && hm_options.antialiasing >= HM_AA_MSAA2)
                        notice = "MSAA requires GPU Auto";
#endif
                }
            }
#ifdef HMBTN_ENH_GAMEPLAY
            if (action == HM_PAUSE_CHANGE_GAMEPLAY) {
                if (hm_gameplay_change(&hm_gameplay, menu.selected,
                                       input == HM_PAUSE_LEFT ? -1 : 1)) {
                    notice = hm_gameplay_save(&hm_gameplay) ? "" : "Cannot save settings";
                }
            }
#else
            if (action == HM_PAUSE_CHANGE_GAMEPLAY) {
                notice = "Option unavailable";
            }
#endif
            if (action == HM_PAUSE_CHANGE_CONTROLS) {
                if (hm_controls_change(&hm_controls, menu.selected,
                                       input == HM_PAUSE_LEFT ? -1 : 1)) {
                    notice = hm_controls_save(&hm_controls) ? "" : "Cannot save settings";
                }
            }
            if (action == HM_PAUSE_CHANGE_AUDIO) {
                int dir = (input == HM_PAUSE_LEFT) ? -1 : 1;
                if (input == HM_PAUSE_CONFIRM && menu.selected < 2) {
                    if (menu.selected == 0 && hm_audio.bgm_volume >= 100)
                        dir = -10;
                    else if (menu.selected == 1 && hm_audio.sfx_volume >= 100)
                        dir = -10;
                }
                if (hm_audio_change(&hm_audio, menu.selected, dir)) {
                    notice = hm_audio_save(&hm_audio) ? "" : "Cannot save settings";
                }
            }
            if (action == HM_PAUSE_USE_BLESSING) {
                if (menu.selected == 0) {
                    int before = game_state.player.gold;
                    estate_gold_change(10000, 0);
                    int added = game_state.player.gold - before;
                    notice = added <= 0 ? "Purse is already full" :
                             (added < 10000 ? "Purse reached its limit" :
                                              "The Goddess added 10000G");
                } else if (menu.selected == 1) {
                    int changed = game_state.player.stamina < game_state.player.maxStamina ||
                                  game_state.player.fatigue > 0;
                    game_state.player.stamina = game_state.player.maxStamina;
                    game_state.player.fatigue = 0;
                    game_state.player.latches &= (u16)~(PLAYER_LATCH_STAMINA_EMPTY |
                        PLAYER_LATCH_STAMINA_UNDER_10 | PLAYER_LATCH_STAMINA_UNDER_30 |
                        PLAYER_LATCH_STAMINA_UNDER_50 | PLAYER_LATCH_FATIGUE_MAX |
                        PLAYER_LATCH_FATIGUE_80 | PLAYER_LATCH_FATIGUE_50);
                    notice = changed ? "Your stamina and fatigue are restored" :
                                       "Already fully restored";
                } else if (menu.selected == 2) {
                    int before = game_state.progress.estate.fodder;
                    estate_fodder_add(FODDER_ADD_BOUGHT, 10);
                    int added = (int)game_state.progress.estate.fodder - before;
                    notice = added <= 0 ? "Fodder store is full" :
                             (added < 10 ? "Fodder stock reached its limit" :
                                           "The Goddess added 10 fodder");
                } else if (menu.selected == 3) {
                    int before = game_state.progress.estate.chickenFeed;
                    estate_chicken_feed_add(2, 10);
                    int added = (int)game_state.progress.estate.chickenFeed - before;
                    notice = added <= 0 ? "Chicken feed store is full" :
                             (added < 10 ? "Chicken feed reached its limit" :
                                           "The Goddess added 10 chicken feed");
                } else {
                    int before = game_state.progress.estate.fishFood;
                    estate_fish_food_add(FISH_FOOD_ADD_AMOUNT, 10);
                    int added = (int)game_state.progress.estate.fishFood - before;
                    notice = added <= 0 ? "Fish food store is full" :
                             (added < 10 ? "Fish food reached its limit" :
                                           "The Goddess added 10 fish food");
                }
                value_dirty = 1;
            }
            if (action == HM_PAUSE_REMAP_CONTROLS) {
                keyboard_capture_item = menu.selected;
                notice = "Press a key to bind; ESC cancels";
                value_dirty = 1;
            }
            if (action == HM_PAUSE_EDIT_TOUCH_LAYOUT) {
                if (!hm_controls.virtual_gamepad_enabled) {
                    notice = "Enable Virtual Gamepad first";
                } else {
                    notice = hm_user_data_set_nested_setting("controls", "virtual_gamepad_edit", "true")
                                 ? "Drag buttons; tap Done to finish"
                                 : "Cannot save settings";
                }
                value_dirty = 1;
            }
#ifdef HMBTN_ENH_SHIPPING_BIN
            if (action == HM_PAUSE_CHANGE_SHIPPING) {
                if (menu.selected < view.shipping_count &&
                    hm_pause_shipping_actions[menu.selected] == HM_PAUSE_SHIPPING_ROW_SHIP_BASKET) {
                    if (player_shipping_basket_begin())
                        running = 0;
                    else
                        notice = "Basket is no longer available";
                } else if (menu.selected < view.shipping_count &&
                           hm_host_shipping_withdraw(menu.selected)) {
                    notice = "Item returned to rucksack";
                } else if (view.shipping_count == 0) {
                    notice = "No shipped items to take";
                } else {
                    notice = "Rucksack is full";
                }
                int count = hm_host_shipping_build_rows(&view, portable_basket);
                menu.content_count = count > 0 ? count : 1;
                if (menu.selected >= count)
                    menu.selected = count > 0 ? count - 1 : 0;
                menu.first_visible = count > 0
                                         ? (menu.selected / HM_PAUSE_SHIPPING_VISIBLE_ITEMS) *
                                               HM_PAUSE_SHIPPING_VISIBLE_ITEMS
                                         : 0;
                value_dirty = 1;
            }
#endif
        }
        if (!running)
            break;
        hm_pause_input held_navigation;
        if (!hm_host_pause_held_navigation(&held_navigation)) {
            repeat_navigation = 0;
        } else if (!repeat_navigation || held_navigation != repeat_direction) {
            repeat_navigation = 1;
            repeat_direction = held_navigation;
            next_repeat_ns = SDL_GetTicksNS() + 350000000;
            hm_host_pause_queue_navigation(held_navigation);
        } else if (SDL_GetTicksNS() >= next_repeat_ns) {
            hm_host_pause_queue_navigation(held_navigation);
            next_repeat_ns = SDL_GetTicksNS() + 110000000;
        }
        if (hm_present_clock_due(&hm_presentation, SDL_GetTicksNS())) {
            u8 *pixels = SDL_MapGPUTransferBuffer(hm_device, hm_upload, true);
            if (!pixels)
                hm_host_fail();
            hm_pause_canvas_extract(hm_pause_background, hm_pause_frozen, pause_width);
            if (value_dirty) {
                value[0] = 0;
                for (int i = 0; i < HM_PAUSE_MAX_VIEW_ITEMS; ++i)
                    rows[i][0] = 0;
                if (menu.page == HM_PAUSE_GRAPHICS) {
                    hm_graphics_value(&hm_options, menu.selected, value, sizeof(value));
                    for (int i = 0; i < HM_PAUSE_GRAPHICS_ITEMS; ++i)
                        hm_graphics_row_value(&hm_options, i, rows[i], sizeof(rows[i]));
                }
#ifdef HMBTN_ENH_GAMEPLAY
                if (menu.page == HM_PAUSE_GAMEPLAY)
                    hm_gameplay_value(&hm_gameplay, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_GAMEPLAY)
                    for (int i = 0; i < HM_PAUSE_GAMEPLAY_ITEMS; ++i)
                        hm_gameplay_row_value(&hm_gameplay, i, rows[i], sizeof(rows[i]));
#endif
                if (menu.page == HM_PAUSE_CONTROLS)
                    hm_controls_value(&hm_controls, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_CONTROLS)
                    for (int i = 0; i < HM_PAUSE_CONTROLS_ITEMS; ++i)
                        hm_controls_row_value(&hm_controls, i, rows[i], sizeof(rows[i]));
                if (menu.page == HM_PAUSE_AUDIO)
                    hm_audio_value(&hm_audio, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_AUDIO)
                    for (int i = 0; i < HM_PAUSE_AUDIO_ITEMS; ++i)
                        hm_audio_row_value(&hm_audio, i, rows[i], sizeof(rows[i]));
                if (menu.page == HM_PAUSE_BLESSINGS) {
                    value[0] = 0;
                }
#ifdef HMBTN_ENH_SHIPPING_BIN
                if (menu.page == HM_PAUSE_SHIPPING) {
                    int count = hm_host_shipping_build_rows(&view, portable_basket);
                    menu.content_count = count > 0 ? count : 1;
                    view.shipping_first = menu.first_visible;
                    if (count == 0)
                        view.shipping_items[0] = "No shipped items";
                }
#endif
                value_dirty = 0;
            }
            view.notice = notice;
#ifdef __ANDROID__
            if (!*notice && hm_perf_notice[0])
                view.notice = hm_perf_notice;
#endif
            hm_pause_draw(&hm_assets, hm_pause_background, &menu, &view);
            hm_pause_canvas_compose(pixels, hm_pause_frozen, pause_width, hm_pause_background);
            SDL_UnmapGPUTransferBuffer(hm_device, hm_upload);
            hm_scanout_cached_vram_gen = 0;
            hm_host_submit(pause_width, 240, 0, pause_width > 320);
        }
        Uint64 now = SDL_GetTicksNS();
        if (hm_presentation.next_ns > now) {
            Uint64 wait = hm_presentation.next_ns - now;
            SDL_DelayPrecise(wait < 5000000 ? wait : 5000000);
        }
    }
    hm_start += SDL_GetTicksNS() - before;
    hm_host_reset_presentation();
    hm_ignore_held = 1;
    hm_mouse_reset();
    hm_audio_output_pause(0);
    hm_host_in_pause = 0;
}
#endif

static int hm_host_present_scanout(u64 tick) {
    if (hm_trace_camera && tick % 120 == 0) {
        const FarmPlayer *player = &game_state.player;
        const FarmScroll *scroll = &farm_area.mode.scroll;
        fprintf(stderr,
                "Camera tick=%llu area=%d place=%d,%d player=%d,%d,%d centre=%d,%d,%d "
                "eye=%d,%d,%d yaw=%d distance=%d height=%d global=%08x debug=%08x "
                "event=%d view=%d entry=%d,%d\n",
                (unsigned long long)tick, farm_area.mode.area, farm_area.mode.place.value,
                farm_area.mode.place.location, player->position.vx, player->position.vy,
                player->position.vz, scroll->centre.vx, scroll->centre.vy, scroll->centre.vz,
                scroll->a18, scroll->b1A, scroll->c1C, scroll->yaw, scroll->z, scroll->x,
                game_state.globalFlags, game_state.debugFlags, farm_area.mode.eventMode,
                farm_area.mode.view, game_state.progress.entryStep,
                game_state.progress.entrySubStep);
    }
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
#if (defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)) && defined(HMBTN_ENH_PAUSE_MENU)
    int private_output = 0;
#ifdef HMBTN_ENH_FILTERING
    private_output |= (hm_options.linear == 2);
#endif
#ifdef HMBTN_ENH_WIDESCREEN
    private_output |= hm_widescreen_margin() != 0;
#endif
    if (private_output && !hm_native_output) {
        hm_native_output = hm_software_output_create();
        if (!hm_native_output) {
            hm_options.linear = 0;
#ifdef HMBTN_ENH_WIDESCREEN
            hm_options.widescreen = 0;
            hm_widescreen_configure(0, 0, 0);
#endif
            fprintf(stderr, "Private rendering unavailable; using native output\n");
        }
    }
    if (!private_output && hm_native_output) {
        hm_software_output_destroy(hm_native_output);
        hm_native_output = NULL;
    }
#endif

#ifdef HMBTN_ENH_PAUSE_MENU
    hm_options.gpu_active = 0;
#endif
#ifdef HMBTN_GPU_RASTER
    {
        int gpu_res = hm_host_gpu_present(area);
        if (gpu_res != 0) {
#ifdef HMBTN_ENH_PAUSE_MENU
            hm_options.gpu_active = 1;
#endif
            return gpu_res == 1 ? 1 : 0;
        }
    }
#endif
#if defined(HMBTN_ENH_SCALING) || defined(HMBTN_ENH_INTERPOLATION) ||                              \
    defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    hm_render_frame live = hm_game_render_frame();
#endif
#ifdef HMBTN_ENH_SCALING
    hm_scaled_view scaled = {0};
    int scaling_active = 0;
    hm_replay_sink previous_sink = {0};
#if defined(HMBTN_ENH_PAUSE_MENU)
    /* Auto never stalls simulation with a viewport-sized CPU retry. The
     * explicit Software renderer remains the full-resolution reference. */
    scaling_active = state->display_enabled && hm_options.scaling && hm_options.renderer &&
                     hm_host_scaling_prepare();
#endif
    if (scaling_active)
        previous_sink = hm_scaling_begin();
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    hm_software_view native_filtered = {0};
    int native_filter_active = 0;
    hm_replay_sink native_previous = {0};
    if (state->display_enabled && hm_native_output && area->w > 0 && area->h > 0
#ifdef HMBTN_ENH_SCALING
        && !scaling_active
#endif
    ) {
        int filter = 0;
#if defined(HMBTN_ENH_FILTERING) && defined(HMBTN_ENH_PAUSE_MENU)
        filter = hm_graphics_scene_filter_mode(&hm_options,
#ifdef __ANDROID__
                                               1
#else
                                               0
#endif
        );
#endif
        hm_software_output_filter(hm_native_output, filter);
        RECT output_area = *area;
#ifdef HMBTN_ENH_WIDESCREEN
        output_area = hm_widescreen_area(output_area);
#endif
        native_filter_active =
            hm_software_output_prepare(hm_native_output, (u32)output_area.w, (u32)output_area.h);
        if (native_filter_active)
            native_previous = hm_software_output_begin(hm_native_output);
        else {
            hm_software_output_destroy(hm_native_output);
            hm_native_output = NULL;
#ifdef HMBTN_ENH_PAUSE_MENU
            hm_options.linear = 0;
#ifdef HMBTN_ENH_WIDESCREEN
            hm_options.widescreen = 0;
            hm_widescreen_configure(0, 0, 0);
#endif
#endif
            fprintf(stderr, "Private output allocation failed; using native output\n");
        }
    }
#endif
#ifdef HMBTN_ENH_INTERPOLATION
    hm_interpolated_view interpolated = {0};
    int interp_sink_bound = 0;
#ifdef HMBTN_ENH_SCALING
    interp_sink_bound |= scaling_active;
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    interp_sink_bound |= native_filter_active;
#endif
    int interp_replayed = 0;
    if (state->display_enabled) {
        if (interp_sink_bound) {
            interp_replayed = hm_interpolation_replay(SDL_GetTicksNS() - hm_start, &live);
        } else if (hm_interpolation_render(SDL_GetTicksNS() - hm_start, &live, &interpolated)) {
            area = &interpolated.area;
            interp_replayed = 1;
        }
    }
#endif
#ifdef HMBTN_ENH_SCALING
    if (scaling_active) {
#ifdef HMBTN_ENH_INTERPOLATION
        if (!interp_replayed)
#endif
            hm_scaling_replay(&live);
        hm_scaling_end(previous_sink, &scaled);
    }
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    if (native_filter_active) {
#ifdef HMBTN_ENH_INTERPOLATION
        if (!interp_replayed)
#endif
            hm_software_output_replay(hm_native_output, &live);
        hm_software_output_end(hm_native_output, native_previous, &native_filtered);
    }
#endif
    /* A direct/loading scanout has no replayable scene. Keep its native pixels
     * centred on the same wide canvas rather than changing the output aspect. */
    RECT source_area = *area;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT scanout_area = hm_widescreen_area(source_area);
    int wide_scanout = 0;
#ifdef HMBTN_ENH_SCALING
    if (!scaled.pixels)
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
        if (!native_filtered.pixels)
#endif
        {
            wide_scanout = scanout_area.w != source_area.w;
            area = &scanout_area;
        }
#endif
    if (SDL_getenv("HMBTN_TRACE_SCANOUT") && tick % 60 == 0) {
        fprintf(stderr, "Scanout tick %llu: enabled=%d area=%d,%d %dx%d\n",
                (unsigned long long)tick, state->display_enabled, area->x, area->y, area->w,
                area->h);
    }
    if (!area->w || !area->h)
        return 0;
    if (area->w < 0 || area->w > HM_SCAN_WIDTH || area->h < 0 || area->h > HM_SCAN_HEIGHT) {
        SDL_SetError("unsupported display dimensions");
        hm_host_fail();
    }
    u32 output_width = (u32)area->w, output_height = (u32)area->h;
#ifdef HMBTN_ENH_SCALING
    if (scaled.pixels) {
        output_width = scaled.width;
        output_height = scaled.height;
    }
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    if (native_filtered.pixels) {
        output_width = native_filtered.width;
        output_height = native_filtered.height;
    }
#endif
    if (!hm_host_image_size(output_width, output_height)) {
#ifdef HMBTN_ENH_SCALING
        if (scaled.pixels) {
            hm_host_scaling_unavailable();
            scaled = (hm_scaled_view){0};
            output_width = (u32)area->w;
            output_height = (u32)area->h;
        }
#endif
        if (!hm_host_image_size(output_width, output_height))
            hm_host_fail();
    }
#ifdef HMBTN_ENH_SCALING
    const u16 *scaled_pixels = scaled.pixels;
#else
    const u16 *scaled_pixels = NULL;
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    const u16 *filter_pixels = native_filtered.pixels;
#else
    const u16 *filter_pixels = NULL;
#endif
#ifdef HMBTN_ENH_INTERPOLATION
    const u16 *interp_pixels = interpolated.pixels;
#else
    const u16 *interp_pixels = NULL;
#endif
    const char *capture = SDL_getenv("HMBTN_CAPTURE_FRAME");
    int direct_vram_scanout = !scaled_pixels && !filter_pixels && !interp_pixels &&
                              state->display_enabled && output_width == (u32)source_area.w &&
                              output_height == (u32)source_area.h;
    u64 current_vram_gen = hm_vram_generation();
    int need_upload = 1;
    if (direct_vram_scanout && !capture && hm_scanout_cached_vram_gen == current_vram_gen &&
        hm_scanout_cached_enabled == state->display_enabled &&
        hm_scanout_cached_area.x == source_area.x && hm_scanout_cached_area.y == source_area.y &&
        hm_scanout_cached_area.w == source_area.w && hm_scanout_cached_area.h == source_area.h) {
        need_upload = 0;
    }
    if (need_upload) {
        u8 *pixels = SDL_MapGPUTransferBuffer(hm_device, hm_upload, true);
        if (!pixels)
            hm_host_fail();
        u16 backdrop = state->display_enabled ? hm_gpu_background(&state->draw) : 0;
        const hm_vram_context *vram_ctx = hm_vram_state();
        if (direct_vram_scanout) {
            /* High-speed direct 32-bit VRAM blit path for vanilla / 1:1 scanout */
            for (u32 y = 0; y < output_height; ++y) {
                u32 *out_words = (u32 *)(pixels + y * output_width * 4);
                u32 sx = (u32)source_area.x;
                u32 sy = (u32)(source_area.y + (int)y) & 511u;
                const u16 *vram_row = vram_ctx->pixels[sy];
                for (u32 x = 0; x < output_width; ++x) {
                    u16 color = vram_row[(sx + x) & 1023u];
                    u32 r = ((color & 31) << 3) | ((color & 31) >> 2);
                    u32 g = (((color >> 5) & 31) << 3) | (((color >> 5) & 31) >> 2);
                    u32 b = (((color >> 10) & 31) << 3) | (((color >> 10) & 31) >> 2);
                    out_words[x] = 0xFF000000u | (r << 0) | (g << 8) | (b << 16);
                }
            }
        } else {
            hm_scanout_cached_vram_gen = 0;
            for (u32 y = 0; y < output_height; ++y) {
                u32 *out_words = (u32 *)(pixels + y * output_width * 4);
                int source_y = area->y + (int)y;
                int row_visible =
                    source_y >= source_area.y && source_y < source_area.y + source_area.h;
                const u16 *vram_row = vram_ctx->pixels[(u32)source_y & 511u];
                for (u32 x = 0; x < output_width; ++x) {
                    u16 color = 0;
                    int source_x = area->x + (int)x;
                    int source_visible = row_visible && source_x >= source_area.x &&
                                         source_x < source_area.x + source_area.w;
#ifdef HMBTN_ENH_SCALING
                    if (scaled.pixels)
                        color = scaled.pixels[(size_t)y * output_width + x];
                    else
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
                        if (native_filtered.pixels)
                        color = native_filtered.pixels[(size_t)y * output_width + x];
                    else
#endif
                        color = state->display_enabled && source_visible
                                    ? vram_row[(u32)source_x & 1023u]
                                    : backdrop;
#ifdef HMBTN_ENH_INTERPOLATION
                    if (interpolated.pixels && source_visible
#ifdef HMBTN_ENH_SCALING
                        && !scaled.pixels
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
                        && !native_filtered.pixels
#endif
                    )
                        color = interpolated.pixels[((u32)area->y + y) * interpolated.stride +
                                                    (u32)area->x + x];
#endif
                    u32 r = ((color & 31u) << 3) | ((color & 31u) >> 2);
                    u32 g = (((color >> 5) & 31u) << 3) | (((color >> 5) & 31u) >> 2);
                    u32 b = (((color >> 10) & 31u) << 3) | (((color >> 10) & 31u) >> 2);
                    out_words[x] = 0xFF000000u | (r << 0) | (g << 8) | (b << 16);
                }
            }
        }
        if (capture) {
            SDL_Surface *surface =
                SDL_CreateSurfaceFrom((int)output_width, (int)output_height, SDL_PIXELFORMAT_RGBA32,
                                      pixels, (int)output_width * 4);
            if (!surface || !SDL_SaveBMP(surface, capture))
                hm_host_fail();
            SDL_DestroySurface(surface);
        }
        SDL_UnmapGPUTransferBuffer(hm_device, hm_upload);
    }
#ifdef HMBTN_ENH_PAUSE_MENU
    int submit_wide =
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
        native_filtered.pixels ? hm_host_wide_active() :
#endif
#ifdef HMBTN_ENH_SCALING
        scaled.pixels ? hm_host_wide_active() :
#endif
#ifdef HMBTN_ENH_WIDESCREEN
        wide_scanout;
#else
        0;
#endif
    int submit_linear = hm_options.linear;
#else
    int submit_wide = 0;
    int submit_linear = 0;
#endif
    if (need_upload) {
        if (hm_host_submit(output_width, output_height, submit_linear, submit_wide)) {
            if (direct_vram_scanout) {
                hm_scanout_cached_vram_gen = current_vram_gen;
                hm_scanout_cached_area = source_area;
                hm_scanout_cached_enabled = state->display_enabled;
            }
            return 1;
        }
        return 0;
    } else {
        SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
        if (!command)
            hm_host_fail();
        SDL_GPUTexture *swapchain = NULL;
        Uint32 win_w, win_h;
        if (!SDL_AcquireGPUSwapchainTexture(command, hm_window, &swapchain, &win_w, &win_h)) {
            SDL_CancelGPUCommandBuffer(command);
            hm_host_fail();
        }
        if (!swapchain) {
            if (!SDL_CancelGPUCommandBuffer(command))
                hm_host_fail();
            return 0;
        }
        hm_host_blit(command, hm_texture, output_width, output_height, swapchain, win_w, win_h,
                     submit_linear, submit_wide);
        if (!SDL_SubmitGPUCommandBuffer(command))
            hm_host_fail();
        return 1;
    }
}

#ifdef __ANDROID__
/* Once-per-second frame timing summary written to logcat (tag VanillaBTN).
 * Measures where wall time goes so Android performance work is driven by data:
 * game = guest code between VBlanks (logic, CPU rasterizer, SPU), present =
 * scanout/GPU submission, wait = idle sleep inside the host tick. */
typedef struct {
    Uint64 window_start, last_exit;
    Uint64 game_ns, game_max, present_ns, present_max, wait_ns;
    u32 ticks, presents, gpu_presents, rebases, log_sec;
} hm_perf_stats;
static hm_perf_stats hm_perf;
static void hm_perf_tick_enter(Uint64 now) {
    if (hm_perf.last_exit) {
        Uint64 game = now - hm_perf.last_exit;
        hm_perf.game_ns += game;
        if (game > hm_perf.game_max)
            hm_perf.game_max = game;
        ++hm_perf.ticks;
    }
    if (!hm_perf.window_start)
        hm_perf.window_start = now;
    Uint64 elapsed = now - hm_perf.window_start;
    if (elapsed < SDL_NS_PER_SECOND)
        return;
    double ticks = hm_perf.ticks ? (double)hm_perf.ticks : 1.0;
    double presents = hm_perf.presents ? (double)hm_perf.presents : 1.0;
    double game_avg = hm_perf.game_ns / ticks / 1e6;
    double present_avg = hm_perf.present_ns / presents / 1e6;
    double scene_avg = hm_gpu_perf.scenes
                           ? (double)hm_gpu_perf.scene_ns / hm_gpu_perf.scenes / 1e6
                           : 0;
    double replay_avg = hm_gpu_perf.replays
                            ? (double)hm_gpu_perf.replay_ns / hm_gpu_perf.replays / 1e6
                            : 0;
    double materialize_avg = hm_gpu_perf.materialized_replays
                                 ? (double)hm_gpu_perf.materialize_ns /
                                       hm_gpu_perf.materialized_replays / 1e6
                                 : 0;
    double materialize_setup_avg = hm_gpu_perf.materialized_replays
                                       ? (double)hm_gpu_perf.materialize_setup_ns /
                                             hm_gpu_perf.materialized_replays / 1e6
                                       : 0;
    double materialize_replay_avg = hm_gpu_perf.materialized_replays
                                        ? (double)hm_gpu_perf.materialize_replay_ns /
                                              hm_gpu_perf.materialized_replays / 1e6
                                        : 0;
    double pack_avg = hm_gpu_perf.packs
                          ? (double)hm_gpu_perf.pack_ns / hm_gpu_perf.packs / 1e6
                          : 0;
    double encode_avg = hm_gpu_perf.encodes
                            ? (double)hm_gpu_perf.encode_ns / hm_gpu_perf.encodes / 1e6
                            : 0;
    double acquire_avg = hm_gpu_perf.acquires
                             ? (double)hm_gpu_perf.acquire_ns / hm_gpu_perf.acquires / 1e6
                             : 0;
    double blit_avg = hm_gpu_perf.blits
                          ? (double)hm_gpu_perf.blit_ns / hm_gpu_perf.blits / 1e6
                          : 0;
    double submit_avg = hm_gpu_perf.submits
                            ? (double)hm_gpu_perf.submit_ns / hm_gpu_perf.submits / 1e6
                            : 0;
    double bin_avg = hm_gpu_perf.tile_count
                         ? (double)hm_gpu_perf.total_bin_references / hm_gpu_perf.tile_count
                         : 0;
    u32 uncached_bin_count = hm_gpu_perf.tile_count - hm_gpu_perf.empty_bin_count -
                             hm_gpu_perf.shared_cache_bin_count;
    hm_audio_output_stats audio_stats;
    hm_audio_output_get_stats(&audio_stats);
    char gpu_detail[512];
    snprintf(gpu_detail, sizeof(gpu_detail),
             " | scene=%ux%u p=%u bins=%u refs=%.1f/%u cache=%u empty=%u uncached=%u"
             " vram=%u %.1fKiB matTiles=%u"
             " | ms scene=%.1f/%.1f replay=%.1f/%.1f mat=%.1f/%.1f"
             " matSetup=%.1f/%.1f matMask=%.1f/%.1f matBind=%.1f/%.1f"
             " matReplay=%.1f/%.1f pack=%.1f/%.1f"
             " enc=%.1f/%.1f acq=%.1f/%.1f blit=%.1f/%.1f sub=%.1f/%.1f matCount=%u/%u"
             " audio=%llu/%llu q=%u",
             hm_gpu_perf.width, hm_gpu_perf.height, hm_gpu_perf.primitive_count,
             hm_gpu_perf.tile_count, bin_avg, hm_gpu_perf.max_bin_count,
             hm_gpu_perf.shared_cache_bin_count, hm_gpu_perf.empty_bin_count,
             uncached_bin_count,
             hm_gpu_perf.vram_version_count,
             (double)hm_gpu_perf.scene_bytes / 1024.0,
             hm_gpu_perf.materialization_tile_count,
             scene_avg, hm_gpu_perf.scenes ? hm_gpu_perf.scene_max_ns / 1e6 : 0,
             replay_avg, hm_gpu_perf.replays ? hm_gpu_perf.replay_max_ns / 1e6 : 0,
             materialize_avg,
             hm_gpu_perf.materialized_replays ? hm_gpu_perf.materialize_max_ns / 1e6 : 0,
             materialize_setup_avg,
             hm_gpu_perf.materialized_replays ? hm_gpu_perf.materialize_setup_max_ns / 1e6 : 0,
             hm_gpu_perf.materialized_replays
                 ? (double)hm_gpu_perf.materialize_mask_ns /
                       hm_gpu_perf.materialized_replays / 1e6
                 : 0,
             hm_gpu_perf.materialized_replays
                 ? hm_gpu_perf.materialize_mask_max_ns / 1e6
                 : 0,
             hm_gpu_perf.materialized_replays
                 ? (double)hm_gpu_perf.materialize_bind_ns /
                       hm_gpu_perf.materialized_replays / 1e6
                 : 0,
             hm_gpu_perf.materialized_replays
                 ? hm_gpu_perf.materialize_bind_max_ns / 1e6
                 : 0,
             materialize_replay_avg,
             hm_gpu_perf.materialized_replays ? hm_gpu_perf.materialize_replay_max_ns / 1e6 : 0,
             pack_avg, hm_gpu_perf.packs ? hm_gpu_perf.pack_max_ns / 1e6 : 0,
             encode_avg, hm_gpu_perf.encodes ? hm_gpu_perf.encode_max_ns / 1e6 : 0,
             acquire_avg, hm_gpu_perf.acquires ? hm_gpu_perf.acquire_max_ns / 1e6 : 0,
             blit_avg, hm_gpu_perf.blits ? hm_gpu_perf.blit_max_ns / 1e6 : 0,
             submit_avg, hm_gpu_perf.submits ? hm_gpu_perf.submit_max_ns / 1e6 : 0,
             hm_gpu_perf.materialized_replays, hm_gpu_perf.scenes,
             (unsigned long long)audio_stats.underflow_frames,
             (unsigned long long)audio_stats.dropped_frames, audio_stats.queued_frames);
    snprintf(hm_perf_notice, sizeof(hm_perf_notice), "FPS %u  Game %.1fms  Present %.1fms",
             hm_perf.presents, game_avg, present_avg);
    __android_log_print(ANDROID_LOG_INFO, "VanillaBTN",
                        "perf: ticks=%u presents=%u gpu=%u rebases=%u | game avg=%.2f max=%.2f ms"
                        " | present avg=%.2f max=%.2f ms | wait=%.0f%%%s",
                        hm_perf.ticks, hm_perf.presents, hm_perf.gpu_presents, hm_perf.rebases,
                        game_avg, hm_perf.game_max / 1e6, present_avg, hm_perf.present_max / 1e6,
                        100.0 * (double)hm_perf.wait_ns / (double)elapsed, gpu_detail);
    u32 log_sec = hm_perf.log_sec + 1;
    if (log_sec >= 2) {
        fprintf(stderr,
                "perf: ticks=%u presents=%u gpu=%u rebases=%u | game avg=%.2f max=%.2f ms"
                " | present avg=%.2f max=%.2f ms | wait=%.0f%%%s\n",
                hm_perf.ticks, hm_perf.presents, hm_perf.gpu_presents, hm_perf.rebases,
                game_avg, hm_perf.game_max / 1e6, present_avg, hm_perf.present_max / 1e6,
                100.0 * (double)hm_perf.wait_ns / (double)elapsed, gpu_detail);
        log_sec = 0;
    }
    Uint64 last_exit = hm_perf.last_exit;
    hm_perf =
        (hm_perf_stats){.window_start = now, .last_exit = last_exit, .log_sec = log_sec};
    hm_gpu_perf = (hm_gpu_perf_stats){0};
}
static int hm_host_frame_dirty(int interp_active) {
#ifdef HMBTN_ENH_INTERPOLATION
    if (interp_active && hm_game_render_frame().eligible)
        return 1;
#else
    (void)interp_active;
#endif
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
    return hm_vram_generation() != hm_android_last_vram_gen ||
           state->display_enabled != hm_android_last_enabled ||
           area->x != hm_android_last_area.x || area->y != hm_android_last_area.y ||
           area->w != hm_android_last_area.w || area->h != hm_android_last_area.h;
}
static void hm_host_mark_frame_presented(void) {
    const hm_gpu_environment *state = hm_gpu_environment_state();
    hm_android_last_vram_gen = hm_vram_generation();
    hm_android_last_enabled = state->display_enabled;
    hm_android_last_area = state->display.disp;
}
#endif

static int hm_host_present_timed(u64 tick) {
#ifdef __ANDROID__
    Uint64 begin = SDL_GetTicksNS();
#endif
    int presented = hm_host_present_scanout(tick);
#ifdef __ANDROID__
    if (presented) {
        Uint64 spent = SDL_GetTicksNS() - begin;
        hm_perf.present_ns += spent;
        if (spent > hm_perf.present_max)
            hm_perf.present_max = spent;
        ++hm_perf.presents;
#ifdef HMBTN_ENH_PAUSE_MENU
        if (hm_options.gpu_active)
            ++hm_perf.gpu_presents;
#endif
    }
#endif
    return presented;
}

static void hm_host_poll_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            hm_game_host_stop();
            exit(0);
        }
        hm_host_display_event(event.type);
#ifdef __ANDROID__
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            hm_controls.virtual_gamepad_enabled) {
            static const SDL_Scancode virtual_pad_keys[HM_VIRTUAL_PAD_ACTIONS] = {
                SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4,
                SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8,
                SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
                SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK};
            for (int i = 0; i < HM_VIRTUAL_PAD_ACTIONS; ++i) {
                if (event.key.scancode == virtual_pad_keys[i]) {
                    int action = hm_controls.virtual_pad_mapping[i];
                    if (action >= 0 && action < 16)
                        hm_virtual_pad_press_latch |= (u16)(1u << action);
                    break;
                }
            }
        }
#endif
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            if (!hm_active_gamepad)
                hm_active_gamepad = SDL_OpenGamepad(event.gdevice.which);
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            if (hm_active_gamepad && SDL_GetGamepadID(hm_active_gamepad) == event.gdevice.which) {
                SDL_CloseGamepad(hm_active_gamepad);
                hm_active_gamepad = NULL;
            }
        }
#ifdef HMBTN_ENH_PAUSE_MENU
        if (hm_pause_enabled && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_AC_BACK))
            hm_host_pause(0);
#endif
    }
#ifdef HMBTN_ENH_SHIPPING_BIN
    if (hm_pause_enabled) {
        int shipping_request = hm_gameplay_shipping_take_request();
        if (shipping_request != HM_SHIPPING_REQUEST_NONE) {
            hm_host_pause(shipping_request);
        }
    }
#endif
}

static void hm_host_events(void) {
    int current_win_w = 0, current_win_h = 0;
    /* Keep pointer input out of gameplay. Pause navigation converts pointer
     * coordinates locally inside hm_host_pause(). */
#ifdef __ANDROID__
    if (SDL_GetWindowSizeInPixels(hm_window, &current_win_w, &current_win_h) &&
        current_win_w > 0 && current_win_h > 0) {
#else
    if (SDL_GetWindowSize(hm_window, &current_win_w, &current_win_h) &&
        current_win_w > 0 && current_win_h > 0) {
#endif
#if defined(HMBTN_ENH_WIDESCREEN) && defined(HMBTN_ENH_PAUSE_MENU)
        hm_widescreen_configure(hm_options.widescreen, current_win_w, current_win_h);
#endif
    }
    hm_host_poll_events();
}

static void hm_host_tick(void *context, u64 tick) {
    (void)context;
    if (hm_host_tick_limit && tick > hm_host_tick_limit) {
        printf("Native window completed %llu VBlanks\n", (unsigned long long)hm_host_tick_limit);
        hm_game_host_stop();
        exit(0);
    }
#ifdef __ANDROID__
    hm_perf_tick_enter(SDL_GetTicksNS());
#endif
    hm_host_events();
    Uint64 now = SDL_GetTicksNS();
    Uint64 deadline = hm_start + tick * SDL_NS_PER_SECOND / 60;

    /* Prevent simulation lag death spiral: if simulation has fallen behind real time
     * by more than 50ms (e.g. disc I/O loading, OS background activity, or slow frame),
     * rebase hm_start so we don't get trapped in a perpetual catch-up state that drops
     * all visual presentation frames. */
    if (now > deadline + SDL_NS_PER_SECOND / 20) {
        hm_start = now - tick * SDL_NS_PER_SECOND / 60;
#ifdef __ANDROID__
        ++hm_perf.rebases;
#endif
    }

    int interp_active = 0;
#if defined(HMBTN_ENH_INTERPOLATION) && defined(HMBTN_ENH_PAUSE_MENU)
    interp_active = hm_options.interpolation != 0;
#endif
    int presented_this_tick = 0;
#ifdef __ANDROID__
    if (hm_host_frame_dirty(interp_active)) {
        if (hm_host_present_timed(tick)) {
            hm_host_mark_frame_presented();
            presented_this_tick = 1;
        }
    }
#else
    if (hm_present_clock_due(&hm_presentation, now)) {
        hm_host_present_timed(tick);
        presented_this_tick = 1;
    }
#endif

    for (;;) {
        hm_host_poll_events();
        now = SDL_GetTicksNS();
        deadline = hm_start + tick * SDL_NS_PER_SECOND / 60;
        /* Let an overdue simulation tick run before doing more visual work. */
        if (now >= deadline)
            break;
#ifdef __ANDROID__
        if (!presented_this_tick && hm_host_frame_dirty(interp_active)) {
            if (hm_host_present_timed(tick)) {
                hm_host_mark_frame_presented();
                presented_this_tick = 1;
            }
        }
#else
        if ((interp_active || !presented_this_tick) &&
            hm_present_clock_due(&hm_presentation, now)) {
            hm_host_present_timed(tick);
            presented_this_tick = 1;
        }
#endif
        now = SDL_GetTicksNS();
        /* A recoverable display error can exclude modal time from the epoch. */
        deadline = hm_start + tick * SDL_NS_PER_SECOND / 60;
        if (now >= deadline)
            break;
#ifdef __ANDROID__
        Uint64 wake = (!presented_this_tick && hm_host_frame_dirty(interp_active) &&
                       now + 1000000 < deadline)
                          ? now + 1000000
                          : deadline;
        if (wake > now) {
            SDL_DelayNS(wake - now);
            hm_perf.wait_ns += SDL_GetTicksNS() - now;
        }
#else
        Uint64 next_present = (!interp_active && presented_this_tick)
                                  ? deadline
                                  : (hm_presentation.unlimited
                                         ? now + 2000000
                                         : (hm_presentation.next_ns <= now
                                                ? now + 1000000
                                                : hm_presentation.next_ns));
        Uint64 wake = next_present < deadline ? next_present : deadline;
        if (wake > now) {
            SDL_DelayPrecise(wake - now);
        }
#endif
    }
    u16 held = 0;
    if (hm_controls.keyboard_layout == HM_KEYBOARD_LAYOUT_ARROWS) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        static const SDL_Scancode mapping[16] = {
            SDL_SCANCODE_Q,         SDL_SCANCODE_E,       SDL_SCANCODE_A,       SDL_SCANCODE_S,
            SDL_SCANCODE_W,         SDL_SCANCODE_D,       SDL_SCANCODE_X,       SDL_SCANCODE_Z,
            SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_RETURN,
            SDL_SCANCODE_UP,        SDL_SCANCODE_RIGHT,   SDL_SCANCODE_DOWN,    SDL_SCANCODE_LEFT};
        for (u32 i = 0; i < 16; ++i)
            if (mapping[i] != SDL_SCANCODE_UNKNOWN && keys[mapping[i]])
                held |= (u16)(1u << i);
    } else if (hm_controls.keyboard_layout == HM_KEYBOARD_LAYOUT_WASD) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        static const SDL_Scancode mapping_wasd[16] = {
            SDL_SCANCODE_Y,         SDL_SCANCODE_P,       SDL_SCANCODE_O,       SDL_SCANCODE_U,
            SDL_SCANCODE_I,         SDL_SCANCODE_L,       SDL_SCANCODE_K,       SDL_SCANCODE_J,
            SDL_SCANCODE_SPACE,     SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_RETURN,
            SDL_SCANCODE_W,         SDL_SCANCODE_D,       SDL_SCANCODE_S,       SDL_SCANCODE_A};
        for (u32 i = 0; i < 16; ++i)
            if (mapping_wasd[i] != SDL_SCANCODE_UNKNOWN && keys[mapping_wasd[i]])
                held |= (u16)(1u << i);
    } else if (hm_controls.keyboard_layout == HM_KEYBOARD_LAYOUT_CUSTOM) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        for (u32 i = 0; i < 16; ++i) {
            int scancode = hm_controls.keyboard_mapping[i];
            if (scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT && keys[scancode])
                held |= (u16)(1u << i);
        }
    }
    if (hm_controls.controller_enabled && hm_active_gamepad) {
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
            held |= HM_PAD_UP;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
            held |= HM_PAD_DOWN;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
            held |= HM_PAD_LEFT;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
            held |= HM_PAD_RIGHT;

        Sint16 stick_x = SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        Sint16 stick_y = SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        if (stick_x < -12000) held |= HM_PAD_LEFT;
        if (stick_x > 12000) held |= HM_PAD_RIGHT;
        if (stick_y < -12000) held |= HM_PAD_UP;
        if (stick_y > 12000) held |= HM_PAD_DOWN;

        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_SOUTH))
            held |= HM_PAD_CROSS;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_EAST))
            held |= HM_PAD_CIRCLE;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_WEST))
            held |= HM_PAD_SQUARE;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_NORTH))
            held |= HM_PAD_TRIANGLE;

        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
            held |= HM_PAD_L1;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
            held |= HM_PAD_R1;
        if (SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000)
            held |= HM_PAD_L2;
        if (SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000)
            held |= HM_PAD_R2;

        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_START))
            held |= HM_PAD_START;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_BACK))
            held |= HM_PAD_SELECT;
    }
#ifdef __ANDROID__
    const bool *android_keys = SDL_GetKeyboardState(NULL);
    static const SDL_Scancode virtual_pad_keys[HM_VIRTUAL_PAD_ACTIONS] = {
        SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4,
        SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8,
        SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
        SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK};
    for (int i = 0; i < HM_VIRTUAL_PAD_ACTIONS; ++i) {
        int action = hm_controls.virtual_pad_mapping[i];
        if (android_keys[virtual_pad_keys[i]] && action >= 0 && action < 16)
            held |= (u16)(1u << action);
    }
    /* Preserve short touch taps whose key-down and key-up arrive between
     * successive fixed-rate game input samples. */
    held |= hm_virtual_pad_press_latch;
    hm_virtual_pad_press_latch = 0;
#endif
    if (hm_replay_enabled)
        held = hm_input_replay_sample(&hm_replay, tick);
#ifdef HMBTN_ENH_PAUSE_MENU
    if (hm_ignore_held) {
        if (!held)
            hm_ignore_held = 0;
        held = 0;
    }
#endif
    hm_pad_sample(0, 1, held);
    hm_mouse_tick();
#ifdef __ANDROID__
    hm_perf.last_exit = SDL_GetTicksNS();
#endif
}

static SDL_GPUDevice *hm_create_gpu_device(void) {
#if defined(__ANDROID__)
    SDL_PropertiesID properties = SDL_CreateProperties();
    if (!properties)
        return NULL;
    bool configured =
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
    SDL_GPUDevice *device = configured ? SDL_CreateGPUDeviceWithProperties(properties) : NULL;
    SDL_DestroyProperties(properties);
    return device;
#else
    return SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                   SDL_GPU_SHADERFORMAT_MSL,
                               false, NULL);
#endif
}

int hm_game_host_start(hm_disc *disc) {
#ifndef HMBTN_ENH_PAUSE_MENU
    (void)disc;
#endif
    const char *limit = SDL_getenv("HMBTN_BOOT_TICKS");
    if (limit) {
        char *end;
        hm_host_tick_limit = strtoull(limit, &end, 10);
        if (!*limit || *end || !hm_host_tick_limit || hm_host_tick_limit > 1000000) {
            fprintf(stderr, "Invalid diagnostic tick limit\n");
            return 0;
        }
    }
    const char *replay_path = SDL_getenv("HMBTN_INPUT_REPLAY");
    if (replay_path) {
        if (!hm_input_replay_load(&hm_replay, replay_path)) {
            fprintf(stderr, "Cannot load input replay\n");
            return 0;
        }
        hm_replay_enabled = 1;
    }
#if defined(__ANDROID__)
    const char *data_dir = SDL_getenv("HMBTN_USER_DATA_DIR");
    if (data_dir) {
        char log_path[512];
        snprintf(log_path, sizeof(log_path), "%s/last_run.log", data_dir);
        freopen(log_path, "a", stdout);
        freopen(log_path, "a", stderr);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    SDL_SetHint("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif
    SDL_SetMainReady();
    hm_trace_camera = SDL_getenv("HMBTN_TRACE_CAMERA") != NULL;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
        return 0;
    /* The launcher starts user data first to read the remembered disc path. */
    if (!hm_user_data_root() && !hm_user_data_start()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
                                 "Cannot open Documents/VanillaBTN. Check folder permissions and "
                                 "close any other game instance.",
                                 NULL);
        return 0;
    }
    hm_window = SDL_CreateWindow(hm_version_window_title(), 640, 480, 0);
    if (hm_window) {
        SDL_Surface *icon = SDL_CreateSurfaceFrom(
            HMBTN_ICON_WIDTH, HMBTN_ICON_HEIGHT, SDL_PIXELFORMAT_RGBA32,
            (void *)hm_window_icon_rgba, HMBTN_ICON_WIDTH * 4);
        if (icon) {
            SDL_SetWindowIcon(hm_window, icon);
            SDL_DestroySurface(icon);
        }
    }
    hm_device = hm_create_gpu_device();
    /* Keep at most two frames in flight across all targets to cap input latency
     * and follow the presentation contract in ADR-121. */
    Uint32 allowed_frames_in_flight = 2;
    if (!hm_window || !hm_device || !SDL_ClaimWindowForGPUDevice(hm_device, hm_window) ||
        !SDL_SetGPUAllowedFramesInFlight(hm_device, allowed_frames_in_flight)) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, "VanillaBTN",
            "GPU Init failed! window=%p, device=%p, error=%s",
            (void*)hm_window, (void*)hm_device, SDL_GetError());
#endif
        return 0;
    }
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "VanillaBTN",
        "GPU Device created and claimed successfully (Driver: %s)",
        SDL_GetGPUDeviceDriver(hm_device));
#endif
    if (!hm_host_image_size(320, 240))
        return 0;
    if (!hm_audio_output_start()) {
        fprintf(stderr, "Native audio: %s\n", SDL_GetError());
        return 0;
    }
    hm_spu_set_hq(1); /* Modern high-fidelity Cubic Hermite audio resampling */
#ifdef HMBTN_ENH_PAUSE_MENU
    hm_pause_enabled = hm_pause_config_enabled();
    if (hm_pause_enabled && !hm_pause_assets_load(&hm_assets, disc)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Back to Nature",
                                 "Cannot load the original game font for the pause menu.",
                                 hm_window);
        return 0;
    }
    hm_graphics_load(&hm_options);
#ifdef HMBTN_ENH_GAMEPLAY
    hm_gameplay_load(&hm_gameplay);
    if (!hm_pause_enabled && hm_gameplay.shipping_bin) {
        hm_gameplay.shipping_bin = 0;
        hm_gameplay_set_active(&hm_gameplay);
    }
#endif
    if (!hm_graphics_apply(hm_window, &hm_options)) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, "VanillaBTN", "hm_graphics_apply failed! error=%s", SDL_GetError());
#endif
        hm_graphics_defaults(&hm_options);
        if (!hm_graphics_apply(hm_window, &hm_options))
            return 0;
    }
    if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options)) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, "VanillaBTN", "hm_graphics_swapchain initial failed! error=%s", SDL_GetError());
#endif
        hm_options.vsync = 1;
        if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options)) {
#if defined(__ANDROID__)
            __android_log_print(ANDROID_LOG_ERROR, "VanillaBTN", "hm_graphics_swapchain fallback failed! error=%s", SDL_GetError());
#endif
            return 0;
        }
    }
#endif
#if defined(HMBTN_ENH_WIDESCREEN) && defined(HMBTN_ENH_PAUSE_MENU)
    int initial_w, initial_h;
    if (SDL_GetWindowSizeInPixels(hm_window, &initial_w, &initial_h))
        hm_widescreen_configure(hm_options.widescreen, initial_w, initial_h);
#endif
#if defined(HMBTN_ENH_INTERPOLATION) && defined(HMBTN_ENH_PAUSE_MENU)
    if (!hm_interpolation_enable(hm_options.interpolation)) {
        SDL_SetError("Cannot allocate visual interpolation buffers");
        return 0;
    }
#endif
#if defined(HMBTN_ENH_SCALING) && defined(HMBTN_ENH_PAUSE_MENU)
    if (!hm_scaling_enable(hm_options.scaling)) {
        SDL_SetError("Cannot allocate scaling history");
        return 0;
    }
#endif
#if defined(HMBTN_ENH_PGXP)
    hm_pgxp_set_enabled(hm_options.pgxp);
#endif
#if defined(HMBTN_ENH_DITHERING)
    hm_dither_set_enabled(hm_options.dither);
#endif
#ifdef HMBTN_GPU_RASTER
    hm_gpu_output = hm_gpu_renderer_create(hm_device);
    if (hm_gpu_output) {
        hm_gpu_commands = hm_gpu_scene_create();
#ifdef HMBTN_ENH_PAUSE_MENU
        if (!hm_options.renderer)
#endif
            hm_gpu_replay = hm_frame_replay_acquire();
    }
    if (!hm_gpu_output || !hm_gpu_commands) {
        hm_frame_replay_release(hm_gpu_replay);
        hm_gpu_replay = NULL;
        hm_gpu_scene_destroy(hm_gpu_commands);
        hm_gpu_commands = NULL;
        hm_gpu_renderer_destroy(hm_gpu_output);
        hm_gpu_output = NULL;
        fprintf(stderr, "GPU scene renderer unavailable; using software rendering\n");
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "VanillaBTN",
        "GPU scene renderer: %s", hm_gpu_output ? "ACTIVE (GPU Auto)" : "FALLBACK (Software)");
#endif
#endif
#ifdef HM_HOST_AA
    hm_fxaa_output = hm_fxaa_create(hm_device);
    hm_options.aa_capabilities = 1u | (hm_fxaa_output ? 1u << HM_AA_FXAA : 0);
#ifdef HMBTN_GPU_RASTER
    if (hm_gpu_output && hm_gpu_commands)
        hm_options.aa_capabilities |= (1u << HM_AA_MSAA2) | (1u << HM_AA_MSAA4);
#endif
    if (!(hm_options.aa_capabilities & (1u << hm_options.antialiasing)))
        hm_options.antialiasing = HM_AA_OFF;
#endif
    hm_controls_defaults(&hm_controls);
    hm_controls_load(&hm_controls);
    hm_audio_load(&hm_audio);
    int pad_count = 0;
    SDL_JoystickID *pads = SDL_GetGamepads(&pad_count);
    if (pads && pad_count > 0) {
        hm_active_gamepad = SDL_OpenGamepad(pads[0]);
        SDL_free(pads);
    }
    hm_start = SDL_GetTicksNS();
    hm_host_reset_presentation();
    hm_timing_reset((hm_timing_host){NULL, hm_host_tick, NULL});
    printf("Native game window using SDL_GPU %s\n", SDL_GetGPUDeviceDriver(hm_device));
    fflush(stdout);
    return 1;
}

void hm_game_host_stop(void) {
    if (hm_active_gamepad) {
        SDL_CloseGamepad(hm_active_gamepad);
        hm_active_gamepad = NULL;
    }
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    hm_software_output_destroy(hm_native_output);
    hm_native_output = NULL;
#endif
#ifdef HMBTN_ENH_SCALING
    hm_scaling_enable(0);
#endif
#ifdef HMBTN_ENH_INTERPOLATION
    hm_interpolation_enable(0);
#ifdef HMBTN_ENH_PGXP
    hm_pgxp_reset();
#endif
#endif
#ifdef HMBTN_GPU_RASTER
    hm_frame_replay_release(hm_gpu_replay);
    hm_gpu_replay = NULL;
    hm_gpu_scene_destroy(hm_gpu_commands);
    hm_gpu_commands = NULL;
    hm_gpu_cached = (hm_gpu_scene_data){0};
    hm_gpu_cached_serial = 0;
#endif
    hm_audio_output_stop();
    if (hm_device) {
        SDL_WaitForGPUIdle(hm_device);
#ifdef HM_HOST_AA
        hm_fxaa_destroy(hm_fxaa_output);
        hm_fxaa_output = NULL;
#endif
#ifdef HMBTN_GPU_RASTER
        hm_gpu_renderer_destroy(hm_gpu_output);
        hm_gpu_output = NULL;
#endif
        if (hm_upload)
            SDL_ReleaseGPUTransferBuffer(hm_device, hm_upload);
        if (hm_texture)
            SDL_ReleaseGPUTexture(hm_device, hm_texture);
        if (hm_window)
            SDL_ReleaseWindowFromGPUDevice(hm_device, hm_window);
        SDL_DestroyGPUDevice(hm_device);
    }
    if (hm_window)
        SDL_DestroyWindow(hm_window);
    hm_device = NULL;
    hm_window = NULL;
    hm_upload = NULL;
    hm_texture = NULL;
    hm_image_width = hm_image_height = hm_upload_bytes = 0;
    hm_user_data_stop();
}
