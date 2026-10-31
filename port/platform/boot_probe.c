/* Diagnostic entry into compiled game C. This is not a packaged game launcher;
 * unsupported runtime calls must stop here with their named error. */
#include "platform/input_replay.h"
#include "compat/family_names.h"
#include "platform/crash_handler.h"
#include "overlay/game_lifecycle.h"
#include "psx/cd_runtime.h"
#include "psx/pad.h"
#include "psx/timing.h"
#ifdef HMBTN_SDL_HOST
#include "platform/disc_locator.h"
#include "platform/game_host.h"
#include "platform/user_data.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define RECT HMBTN_WIN32_RECT
#include <io.h>
#include <windows.h>
#undef RECT
#undef LoadImage
#else
#include <sys/mman.h>
#endif

extern int hm_game_main(void);

#ifndef HMBTN_SDL_HOST
#include "farm_state.h"
#include "platform/frame_capture.h"
static hm_input_replay hm_replay;
static u64 hm_boot_tick_limit;
static int hm_boot_replay;
static int hm_boot_trace_state;
static const char *hm_boot_capture;
static void hm_boot_tick(void *context, u64 tick) {
    (void)context;
    if (hm_boot_trace_state && tick % 600 == 0) {
        const FarmPlayer *player = &game_state.player;
        fprintf(stderr,
                "Game tick %llu: area=%d place=%d,%d entry=%d player=%d,%d,%d mode=%u flags=%08x "
                "global=%08x status=%08x stamina=%d tool=%d item=%d\n",
                (unsigned long long)tick, farm_area.mode.area, farm_area.mode.place.value,
                farm_area.mode.place.location, game_state.progress.entryStep, player->position.vx,
                player->position.vy, player->position.vz, player->mode, player->stateFlags,
                game_state.globalFlags, game_state.statusFlags, player->stamina, player->held[0].id,
                player->held[1].id);
    }
    if (hm_boot_tick_limit && tick > hm_boot_tick_limit) {
        if (hm_boot_capture && !hm_frame_capture(hm_boot_capture)) {
            fprintf(stderr, "Cannot capture native diagnostic scanout\n");
            exit(1);
        }
        printf("Native diagnostic completed %llu VBlanks\n",
               (unsigned long long)hm_boot_tick_limit);
        exit(0);
    }
    if (hm_boot_replay)
        hm_pad_sample(0, 1, hm_input_replay_sample(&hm_replay, tick));
}
#endif

enum { HM_DISC_CACHE_BYTES = 112 * 2352 };

typedef struct {
    FILE *file;
    hm_disc *disc;
    const u8 *mapped;
    u64 size;
#if defined(_WIN32)
    HANDLE mapping;
#endif
    u8 cache[HM_DISC_CACHE_BYTES];
    u64 cache_offset;
    size_t cache_valid;
} hm_disc_slot;

static void hm_close_disc_slot(hm_disc_slot *slot) {
    if (!slot)
        return;
#if defined(_WIN32)
    if (slot->mapped)
        UnmapViewOfFile(slot->mapped);
    if (slot->mapping)
        CloseHandle(slot->mapping);
    slot->mapped = NULL;
    slot->mapping = NULL;
#else
    if (slot->mapped && slot->mapped != MAP_FAILED)
        munmap((void *)slot->mapped, (size_t)slot->size);
    slot->mapped = NULL;
#endif
    if (slot->file) {
        fclose(slot->file);
        slot->file = NULL;
    }
    slot->cache_valid = 0;
}

static int hm_read_at(void *context, u64 offset, void *destination, size_t bytes) {
    hm_disc_slot *slot = context;
    if (offset > slot->size || bytes > slot->size - offset)
        return 0;
    if (slot->mapped) {
        memcpy(destination, slot->mapped + offset, bytes);
        return 1;
    }
    if (slot->cache_valid && offset >= slot->cache_offset &&
        offset + bytes <= slot->cache_offset + slot->cache_valid) {
        memcpy(destination, slot->cache + (offset - slot->cache_offset), bytes);
        return 1;
    }
    if (bytes <= HM_DISC_CACHE_BYTES && offset <= LONG_MAX) {
        if (fseek(slot->file, (long)offset, SEEK_SET) != 0)
            return 0;
        size_t to_read = (size_t)(slot->size - offset < HM_DISC_CACHE_BYTES
                                      ? slot->size - offset
                                      : HM_DISC_CACHE_BYTES);
        size_t got = fread(slot->cache, 1, to_read, slot->file);
        if (got < bytes) {
            slot->cache_valid = 0;
            return 0;
        }
        slot->cache_offset = offset;
        slot->cache_valid = got;
        memcpy(destination, slot->cache, bytes);
        return 1;
    }
    return offset <= LONG_MAX && !fseek(slot->file, (long)offset, SEEK_SET) &&
           fread(destination, 1, bytes, slot->file) == bytes;
}

/* Open and validate one candidate BIN. On success the slot owns the open file. */
static int hm_open_disc(void *context, const char *path, int verbose) {
    hm_disc_slot *slot = context;
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END)) {
        if (verbose)
            fprintf(stderr, "Cannot open the disc image\n");
        if (file)
            fclose(file);
        return 0;
    }
    long bytes = ftell(file);
    if (bytes <= 0) {
        fclose(file);
        return 0;
    }
    slot->file = file;
    slot->size = (u64)bytes;
    slot->mapped = NULL;
    slot->cache_valid = 0;
#if defined(_WIN32)
    slot->mapping = NULL;
    HANDLE hfile = (HANDLE)_get_osfhandle(_fileno(file));
    if (hfile != INVALID_HANDLE_VALUE) {
        slot->mapping = CreateFileMappingA(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (slot->mapping)
            slot->mapped = (const u8 *)MapViewOfFile(slot->mapping, FILE_MAP_READ, 0, 0, 0);
    }
#else
    void *map = mmap(NULL, (size_t)bytes, PROT_READ, MAP_PRIVATE, fileno(file), 0);
    if (map != MAP_FAILED)
        slot->mapped = (const u8 *)map;
#endif
    hm_disc_result status =
        hm_disc_mount(slot->disc, (hm_disc_stream){slot, (u64)bytes, hm_read_at});
    if (status != HM_DISC_OK) {
        if (verbose)
            fprintf(stderr, "%s\n", hm_disc_error(status));
        hm_close_disc_slot(slot);
        return 0;
    }
    return 1;
}

#if defined(__ANDROID__)
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_messagebox.h>
#include <android/log.h>
#define HM_ANDROID_LOG(...) __android_log_print(ANDROID_LOG_INFO, "VanillaBTN", __VA_ARGS__)
SDLMAIN_DECLSPEC int SDL_main(int argc, char **argv) {
#else
#define HM_ANDROID_LOG(...)
int main(int argc, char **argv) {
#endif
    hm_crash_handler_install();
    static hm_disc disc;
    static hm_disc_slot slot = {.file = NULL, .disc = &disc};
#ifdef HMBTN_SDL_HOST
    if (argc > 2) {
        fprintf(stderr, "Usage: hmbtn_native [original USA BIN track]\n");
        return 1;
    }
    /* Settings hold the remembered disc path; the host reuses this running user data. */
    if (!hm_user_data_start()) {
        fprintf(stderr, "Cannot open Documents/VanillaBTN\n");
#if defined(__ANDROID__)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
            "Cannot open Documents/VanillaBTN directory. Check storage permissions.", NULL);
#endif
        return 1;
    }
    HM_ANDROID_LOG("VanillaBTN native startup: user data initialized at %s", hm_user_data_root());
    if (!hm_disc_locate(argc == 2 ? argv[1] : NULL, hm_open_disc, &slot)) {
        HM_ANDROID_LOG("Disc locate failed: USA disc image not found or failed validation");
#if defined(__ANDROID__)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
            "Disc validation failed.\n\nPlease ensure you have placed the original USA Track 1 BIN file (game.bin / hmbtn.bin) in Documents/VanillaBTN.", NULL);
#endif
        hm_user_data_stop();
        return 1;
    }
    HM_ANDROID_LOG("Disc located and mounted successfully");
#else
    if (argc != 2) {
        fprintf(stderr, "Usage: hmbtn_boot_probe <original USA BIN track>\n");
        return 1;
    }
    if (!hm_open_disc(&slot, argv[1], 1))
        return 1;
#endif
    hm_disc_result status;
    status = hm_family_names_load(&disc);
    if (status != HM_DISC_OK) {
        fprintf(stderr, "Cannot read original family text: %s\n", hm_disc_error(status));
        HM_ANDROID_LOG("Cannot read original family text: %s", hm_disc_error(status));
#if defined(__ANDROID__)
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf), "Cannot read game data from disc:\n%s", hm_disc_error(status));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN", errbuf, NULL);
#endif
        hm_close_disc_slot(&slot);
        return 1;
    }
    hm_cd_attach(&disc);
#ifndef HMBTN_SDL_HOST
    const char *replay = getenv("HMBTN_INPUT_REPLAY");
    const char *limit = getenv("HMBTN_BOOT_TICKS");
    if (replay && !hm_input_replay_load(&hm_replay, replay)) {
        fprintf(stderr, "Cannot load input replay\n");
        hm_close_disc_slot(&slot);
        return 1;
    }
    hm_boot_replay = replay != NULL;
    hm_boot_trace_state = getenv("HMBTN_TRACE_GAME_STATE") != NULL;
    hm_boot_capture = getenv("HMBTN_CAPTURE_FRAME");
    if (limit) {
        char *end;
        hm_boot_tick_limit = strtoull(limit, &end, 10);
        if (!*limit || *end || !hm_boot_tick_limit || hm_boot_tick_limit > 1000000) {
            fprintf(stderr, "Invalid diagnostic tick limit\n");
            hm_close_disc_slot(&slot);
            return 1;
        }
    }
    hm_timing_reset((hm_timing_host){NULL, hm_boot_tick, NULL});
#endif
    puts("Validated USA disc; entering compiled native game C");
    fflush(stdout);
    HM_ANDROID_LOG("Entering hm_game_host_start...");
#ifdef HMBTN_SDL_HOST
    if (!hm_game_host_start(&disc)) {
        fprintf(stderr, "Cannot initialize native game window\n");
        HM_ANDROID_LOG("hm_game_host_start failed! SDL_GetError: %s", SDL_GetError());
#if defined(__ANDROID__)
        char errbuf[512];
        snprintf(errbuf, sizeof(errbuf), "Cannot initialize graphics/window:\n\n%s", SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN Graphics Error", errbuf, NULL);
#endif
        hm_game_host_stop();
        hm_cd_detach();
        hm_close_disc_slot(&slot);
        return 1;
    }
#endif
    HM_ANDROID_LOG("hm_game_host_start succeeded! Initializing overlays...");
    hm_game_overlays_init();
    HM_ANDROID_LOG("Calling hm_game_main()...");
    int result = hm_game_main();
#ifdef HMBTN_SDL_HOST
    hm_game_host_stop();
#endif
    hm_cd_detach();
    hm_close_disc_slot(&slot);
    return result;
}


#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif
