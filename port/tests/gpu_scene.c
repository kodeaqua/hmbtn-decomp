#include "gfx/gpu_scene.h"
#include "gfx/frame_replay.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/timing.h"
#include "psx/vram.h"
#include "enhance/dithering/dithering.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_vram_context before, after, private_vram;
static void setup(int width, int height) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, width, height);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, width, height);
    PutDispEnv(&display);
}
static int scene_accepts(hm_gpu_scene *scene, const u32 *words, u32 count, u32 *primitive_count) {
    RECT area = {0, 0, 32, 32};
    hm_gpu_scene_data data;
    hm_replay_sink previous = hm_gpu_scene_begin(scene, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    if (!hm_gpu_scene_end(scene, previous, &data)) {
        fprintf(stderr, "GPU scene rejected GP0 opcode 0x%02x: %s\n", words[0] >> 24,
                hm_gpu_scene_fallback(scene));
        return 0;
    }
    if (primitive_count)
        *primitive_count = data.words[10];
    return 1;
}
static int scene_accepts_transfer(hm_gpu_scene *scene, const u32 *words, u32 count,
                                  u32 expected_opcode) {
    RECT area = {0, 0, 32, 32};
    hm_gpu_scene_data data;
    hm_replay_sink previous = hm_gpu_scene_begin(scene, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    if (!hm_gpu_scene_end(scene, previous, &data)) {
        fprintf(stderr, "GPU scene rejected transfer opcode 0x%02x: %s\n", words[0] >> 24,
                hm_gpu_scene_fallback(scene));
        return 0;
    }
    return data.words[10] == 1 && data.words[data.words[8] + 1] == expected_opcode;
}
int main(void) {
    hm_gpu_scene *s = hm_gpu_scene_create();
    CHECK(s);
    setup(32, 32);
    const RECT scanout_area = {0, 240, 32, 32};
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            hm_vram_draw_pixel(x, 240 + y, (u16)((x * 317 + y * 911) & 0x7fff));
    hm_gpu_environment saved_environment = *hm_gpu_environment_state();
    hm_gpu_scene_data scanout;
    CHECK(hm_gpu_scene_scanout(s, &scanout_area, 64, 64, &scanout));
    CHECK(scanout.words[10] == 0 && scanout.words[2] == 0 && scanout.words[3] == 240 &&
          scanout.words[4] == 32 && scanout.words[5] == 32);
    CHECK(!memcmp(&saved_environment, hm_gpu_environment_state(), sizeof(saved_environment)));
    CHECK(scanout.vram_pixels[240 * HM_VRAM_WIDTH + 17] == hm_vram_pixel(17, 240));
    /* Native aligned scenes report exactly which bins use workgroup cache. */
    const RECT cache_area = {0, 0, 32, 32};
    const u32 cache_rectangle[] = {0x600000ff, 0x00020002, 0x00040004};
    hm_replay_sink cache_previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&cache_area);
    hm_gpu_execute_words(cache_rectangle, 3);
    hm_gpu_scene_data cache_data;
    CHECK(hm_gpu_scene_end(s, cache_previous, &cache_data));
    CHECK(cache_data.tile_count == 16 && cache_data.empty_bin_count == 15 &&
          cache_data.shared_cache_bin_count == 1);
    cache_previous = hm_gpu_scene_begin(s, 64, 64);
    hm_replay_begin(&cache_area);
    hm_gpu_execute_words(cache_rectangle, 3);
    CHECK(hm_gpu_scene_end(s, cache_previous, &cache_data));
    CHECK(cache_data.shared_cache_bin_count == 0);
    /* Audit every render-producing primitive opcode and documented NOP mirror. */
    u32 packet[32] = {0};
    for (u32 opcode = 0x20; opcode <= 0x3f; ++opcode) {
        u32 vertices = (opcode & 8) ? 4 : 3;
        u32 count = 1, primitive_count = 0;
        memset(packet, 0, sizeof(packet));
        packet[0] = (opcode << 24) | 0x00e08040;
        for (u32 vertex = 0; vertex < vertices; ++vertex) {
            if (vertex && (opcode & 16))
                packet[count++] = 0x0020c0f0 - vertex * 0x00001020;
            static const u32 positions[] = {0x00020002, 0x0003001a, 0x001a0004, 0x001b001b};
            packet[count++] = positions[vertex];
            if (opcode & 4) {
                u32 clut = (252u << 6) | 56u, page = 26u | (((opcode >> 7) & 3u) << 7);
                packet[count++] = (vertex == 0 ? clut : vertex == 1 ? page : 0) << 16 |
                                  ((vertex * 7) << 8) | (vertex * 5);
            }
        }
        CHECK(scene_accepts(s, packet, count, &primitive_count));
        CHECK(primitive_count == (vertices == 4 ? 2u : 1u));
    }
    for (u32 opcode = 0x40; opcode <= 0x5f; ++opcode) {
        memset(packet, 0, sizeof(packet));
        packet[0] = (opcode << 24) | 0x00e08040;
        packet[1] = 0x00020002;
        u32 count = 2;
        if (opcode & 8) {
            if (opcode & 16)
                packet[count++] = 0x0020d0f0;
            packet[count++] = 0x0003001a;
            if (opcode & 16)
                packet[count++] = 0x00f020c0;
            packet[count++] = 0x001a001b;
            packet[count++] = 0x50005000;
        } else {
            if (opcode & 16)
                packet[count++] = 0x0020d0f0;
            packet[count++] = 0x001a001b;
        }
        u32 primitive_count = 0;
        CHECK(scene_accepts(s, packet, count, &primitive_count));
        CHECK(primitive_count > 0);
    }
    for (u32 opcode = 0x60; opcode <= 0x7f; ++opcode) {
        memset(packet, 0, sizeof(packet));
        packet[0] = (opcode << 24) | 0x00e08040;
        packet[1] = 0x00020002;
        u32 size = (opcode >> 3) & 3;
        u32 count = 2 + ((opcode & 4) != 0) + (size == 0);
        if (opcode & 4)
            packet[2] = ((252u << 6) | 56u) << 16;
        if (size == 0)
            packet[count - 1] = 7 | (6 << 16);
        u32 primitive_count = 0;
        CHECK(scene_accepts(s, packet, count, &primitive_count));
        CHECK(primitive_count == 1);
    }
    const u8 no_output_opcodes[] = {0x00, 0x01, 0x03, 0x1f, 0xc0, 0xdf, 0xe0};
    for (size_t i = 0; i < sizeof(no_output_opcodes); ++i) {
        u32 nop = (u32)no_output_opcodes[i] << 24, primitive_count = UINT32_MAX;
        CHECK(scene_accepts(s, &nop, 1, &primitive_count));
        CHECK(primitive_count == 0);
    }
    for (u32 opcode = 0x04; opcode <= 0x1e; ++opcode) {
        u32 nop = opcode << 24, primitive_count = UINT32_MAX;
        CHECK(scene_accepts(s, &nop, 1, &primitive_count));
        CHECK(primitive_count == 0);
    }
    for (u32 opcode = 0xe7; opcode <= 0xef; ++opcode) {
        u32 nop = opcode << 24, primitive_count = UINT32_MAX;
        CHECK(scene_accepts(s, &nop, 1, &primitive_count));
        CHECK(primitive_count == 0);
    }
    for (u32 opcode = 0x80; opcode <= 0x9f; ++opcode) {
        const u32 copy_alias[] = {opcode << 24, 0x00020002, 0x00060006,
                                  1u | (1u << 16)};
        CHECK(scene_accepts_transfer(s, copy_alias, 4, 0x80));
    }
    for (u32 opcode = 0xa0; opcode <= 0xbf; ++opcode) {
        const u32 upload_alias[] = {opcode << 24, 0x00060006, 1u | (1u << 16),
                                    0x00004d2b};
        CHECK(scene_accepts_transfer(s, upload_alias, 4, 0xa0));
    }
    u32 unknown = 0xf0000000;
    RECT unknown_area = {0, 0, 32, 32};
    hm_replay_sink previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&unknown_area);
    hm_gpu_execute_words(&unknown, 1);
    hm_gpu_scene_data unknown_data;
    CHECK(!hm_gpu_scene_end(s, previous, &unknown_data));
    CHECK(strstr(hm_gpu_scene_fallback(s), "unsupported GP0 opcode") != NULL);
    hm_gpu_scene_data recovered_scanout;
    CHECK(hm_gpu_scene_scanout(s, &unknown_area, 32, 32, &recovered_scanout));
    CHECK(recovered_scanout.words[10] == 0 && recovered_scanout.words[4] == 32 &&
          recovered_scanout.words[5] == 32);
    u32 rectangle[] = {0x600000ff, 0x00020002, 0x00040004};
    hm_frame_replay *a = hm_frame_replay_acquire(), *b = hm_frame_replay_acquire();
    CHECK(a && b);
    hm_frame_replay_release(a);
    hm_render_frame live = {.key = 9, .tick = 2, .eligible = 0}; /* Not a farm or enhancement. */
    hm_render_begin(&live);
    hm_gpu_execute_words(rectangle, 3);
    hm_render_packet(rectangle, 3);
    hm_render_end();
    CHECK(hm_frame_replay_serial(b, &live));
    u64 displayed_serial = hm_frame_replay_serial(b, &live);
    ++live.key; /* Cutscene state changes before the displayed buffer flips. */
    CHECK(hm_frame_replay_serial(b, &live) == displayed_serial);
    live.tick += 120; /* Blocking CD/audio wait; the display buffer is unchanged. */
    CHECK(hm_frame_replay_serial(b, &live) == displayed_serial);
    DISPENV other;
    SetDefDispEnv(&other, 0, 240, 32, 32);
    PutDispEnv(&other);
    CHECK(!hm_frame_replay_serial(b, &live));
    const char *failure = hm_frame_replay_failure(b);
    CHECK(failure && strstr(failure, "draw area differs from display area") != NULL);
    other.disp.y = 0;
    PutDispEnv(&other);
    hm_vram_snapshot(&before);
    hm_gpu_environment env = *hm_gpu_environment_state();
    u64 ticks = hm_timing_ticks();
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_gpu_scene_data data;
    CHECK(hm_frame_replay_draw(b, &live) && hm_gpu_scene_end(s, previous, &data));
    CHECK(data.width == 32 && data.height == 32 && data.bytes >= HM_SCENE_HEADER * sizeof(u32));
    CHECK(data.words[10] == 1 && data.vram_pixels[0] == 0);
    CHECK(!(data.words[data.words[8] + 4] & 64));
#ifdef HMBTN_ENH_DITHERING
    hm_dither_set_enabled(0);
    previous = hm_gpu_scene_begin(s, 32, 32);
    CHECK(hm_frame_replay_draw(b, &live) && hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[data.words[8] + 4] & 64);
    hm_dither_set_enabled(1);
#endif
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&before, &after, sizeof(before)));
    CHECK(!memcmp(&env, hm_gpu_environment_state(), sizeof(env)) && ticks == hm_timing_ticks());
    /* Replay restores the live environment before scene_end. The output must
     * keep its own captured backdrop instead of reading the next frame's clear. */
    hm_gpu_environment splash_env = env;
    splash_env.draw.isbg = 1;
    splash_env.draw.r0 = splash_env.draw.g0 = splash_env.draw.b0 = 255;
    hm_gpu_environment *live_env = hm_gpu_environment_bind(&splash_env);
    previous = hm_gpu_scene_begin(s, 32, 32);
    RECT splash_area = splash_env.draw.clip;
    hm_replay_begin(&splash_area);
    hm_gpu_environment_bind(live_env);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[14] == 0x7fff);
    hm_render_invalidate();
    CHECK(!hm_frame_replay_serial(b, &live));
    failure = hm_frame_replay_failure(b);
    CHECK(failure && strstr(failure, "no complete captured scene") != NULL);
    u32 captured_fill[] = {0x020000ff, 0, 0x00040004};
    hm_render_begin(&live);
    hm_gpu_execute_words(captured_fill, 3);
    hm_render_packet(captured_fill, 3);
    hm_render_end();
    CHECK(hm_frame_replay_serial(b, &live));
    u32 transfer_pixels[] = {0x00005678};
    RECT direct_load = {1, 1, 1, 1}, direct_move = {1, 1, 1, 1};
    RECT direct_clear = {0, 0, 16, 16};
    hm_render_begin(&live);
    CHECK(LoadImage(&direct_load, transfer_pixels) == 0);
    CHECK(MoveImage(&direct_move, 3, 3) == 0);
    ClearImage(&direct_clear, 0, 255, 0);
    hm_render_packet(rectangle, 3);
    hm_render_end();
    CHECK(hm_frame_replay_serial(b, &live));
    previous = hm_gpu_scene_begin(s, 32, 32);
    CHECK(hm_frame_replay_draw(b, &live));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    int saw_load = 0, saw_move = 0, saw_clear = 0;
    const u32 *direct_primitives = data.words + data.words[8];
    for (u32 i = 0; i < data.words[10]; ++i) {
        const u32 *primitive = direct_primitives + (size_t)i * HM_SCENE_STRIDE;
        if (primitive[0] != 2)
            continue;
        saw_load |= primitive[1] == 0xa0;
        saw_move |= primitive[1] == 0x80;
        saw_clear |= primitive[1] == 2;
    }
    CHECK(saw_load && saw_move && saw_clear);
    hm_frame_replay_release(b);
    /* GPU geometry consumes only output: it must not run the CPU raster a second
     * time, while embedded polygon tpages still affect subsequent sprites. */
    setup(32, 32);
    RECT area = {0, 0, 32, 32};
    u32 texture[] = {0x25000000, 0,          0, 0x00000010, (282u << 16) | 8, 0x00100000,
                     0x0800,     0x65000000, 0, 0,          0x00040004};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(texture, 11);
    CHECK(hm_gpu_scene_end(s, previous, &data) && hm_gpu_environment_state()->draw.tpage == 282);
    CHECK(hm_vram_pixel(0, 0) == 0);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 feedback[] = {0x65000000, 0, 0, 0x00040004};
    hm_gpu_execute_words(feedback, 4);
    CHECK(hm_gpu_scene_end(s, previous, &data));

    /* A texture that reads an earlier draw gets a version of VRAM from that
     * point in command order. First pass requests one CPU materialization. */
    setup(32, 32);
    const u32 feedback_sequence[] = {0x600000ff, 0x00020002, 0x00010001,
                                     0x6000ff00, 0x00140014, 0x00040004,
                                     0xe1000100, 0x65000001, 0x00040004,
                                     0x00020002, 0x00010001,
                                     0x65000001, 0x001e0004, 0x00020040,
                                     0x00010001};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(feedback_sequence,
                         sizeof(feedback_sequence) / sizeof(feedback_sequence[0]));
    CHECK(hm_gpu_scene_needs_materialization(s));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    u64 feedback_tiles[HM_VRAM_DIRTY_ROWS];
    u32 feedback_tile_count = hm_gpu_scene_materialization_mask(s, feedback_tiles);
    CHECK(feedback_tile_count == 1 && (feedback_tiles[0] & 1u));
    CHECK(!(feedback_tiles[1] & (UINT64_C(1) << 1)));
    CHECK(!(feedback_tiles[0] & (UINT64_C(1) << 4)));
    hm_gpu_scene_materialize_vram(s, 1);
    setup(32, 32);
    hm_vram_snapshot(&private_vram);
    hm_vram_context *native_vram = hm_vram_bind(&private_vram);
    hm_vram_write_filter_begin(feedback_tiles);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(feedback_sequence,
                         sizeof(feedback_sequence) / sizeof(feedback_sequence[0]));
    hm_vram_write_filter_end();
    hm_vram_bind(native_vram);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.vram_version_count == 1);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE * 2 + 40] == 1);
    CHECK(data.vram_versions[2 * HM_VRAM_WIDTH + 2] == 31);
    CHECK(data.vram_versions[20 * HM_VRAM_WIDTH + 20] == 0);
    hm_gpu_scene_materialize_vram(s, 0);

    /* A source tile written only after its texture read is not a feedback
     * dependency and must not trigger private CPU materialization. */
    setup(32, 32);
    const u32 read_then_write[] = {0xe1000100, 0x65000000, 0x00140014, 0x00000000,
                                   0x00040004, 0x600000ff, 0x00020002, 0x00040004};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(read_then_write,
                         sizeof(read_then_write) / sizeof(read_then_write[0]));
    CHECK(!hm_gpu_scene_needs_materialization(s));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    u64 read_then_write_tiles[HM_VRAM_DIRTY_ROWS];
    CHECK(hm_gpu_scene_materialization_mask(s, read_then_write_tiles) == 0);

    /* Private replays track changed VRAM tiles so equivalent feedback reads
     * reuse a version instead of comparing every full 1 MiB snapshot. */
    setup(64, 64);
    hm_vram_snapshot(&private_vram);
    native_vram = hm_vram_bind(&private_vram);
    hm_vram_reset();
    const RECT private_area = {0, 0, 64, 64};
    const u32 duplicate_feedback[] = {
        0x600000ff, 0x00020002, 0x00010001, /* write in the source 16x16 tile */
        0xe1000100,
        0x65000001, 0x00280028, 0x000a000a, 0x00010001,
        0x6000ff00, 0x00120002, 0x00010001, /* update a second tile */
        0x65000001, 0x002a0028, 0x000a000a, 0x00010001,
        0x65000001, 0x002c0028, 0x000a000a, 0x00010001};
    hm_gpu_scene_materialize_vram(s, 1);
    previous = hm_gpu_scene_begin(s, 64, 64);
    hm_replay_begin(&private_area);
    hm_gpu_execute_words(duplicate_feedback,
                         sizeof(duplicate_feedback) / sizeof(duplicate_feedback[0]));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    hm_vram_bind(native_vram);
    hm_gpu_scene_materialize_vram(s, 0);
    CHECK(data.vram_version_count == 2);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE + 40] == 1);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE * 3 + 40] == 2);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE * 4 + 40] == 2);

    /* If replay writes a tile and then restores every pixel to the frame
     * baseline, feedback reads reuse the baseline layer without a full-VRAM
     * compare or a redundant snapshot copy. */
    setup(64, 64);
    hm_vram_snapshot(&private_vram);
    native_vram = hm_vram_bind(&private_vram);
    hm_vram_reset();
    const u32 restore_feedback[] = {
        0x600000ff, 0x00020002, 0x00010001,
        0xe1000100,
        0x65000001, 0x00280028, 0x000a000a, 0x00010001,
        0x60000000, 0x00020002, 0x00010001,
        0x65000001, 0x002a0028, 0x000a000a, 0x00010001};
    hm_gpu_scene_materialize_vram(s, 1);
    previous = hm_gpu_scene_begin(s, 64, 64);
    hm_replay_begin(&private_area);
    hm_gpu_execute_words(restore_feedback,
                         sizeof(restore_feedback) / sizeof(restore_feedback[0]));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    hm_vram_bind(native_vram);
    hm_gpu_scene_materialize_vram(s, 0);
    CHECK(data.vram_version_count == 1);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE + 40] == 1);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE * 3 + 40] == 0);

    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(rectangle, 3);
    u32 fill[] = {0x020000ff, 0, 0x00040004};
    hm_gpu_execute_words(fill, 3);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[10] == 2 && data.words[data.words[8] + HM_SCENE_STRIDE] == 2 &&
          data.words[data.words[8] + HM_SCENE_STRIDE + 24] == 0x001f);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    const u32 load[] = {0xa0000000, 0x00040004, 0x00010004, 0x7fff03e0, 0x001f4210};
    hm_gpu_execute_words(rectangle, 3);
    hm_gpu_execute_words(load, sizeof(load) / sizeof(load[0]));
    CHECK(hm_gpu_scene_needs_materialization(s));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    hm_gpu_scene_materialize_vram(s, 1);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(rectangle, 3);
    hm_gpu_execute_words(load, sizeof(load) / sizeof(load[0]));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.vram_version_count == 1 &&
          data.words[data.words[8] + HM_SCENE_STRIDE] == 2 &&
          data.words[data.words[8] + HM_SCENE_STRIDE + 40] == 1);
    hm_gpu_scene_materialize_vram(s, 0);
    /* An independent GP0(80h) packet reads the immutable frame-start VRAM. */
    setup(32, 32);
    const u32 copy[] = {0x80000000, 20u | (21u << 16), 8u | (9u << 16), 4u | (3u << 16)};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(copy, 4);
    CHECK(!hm_gpu_scene_needs_materialization(s));
    CHECK(hm_gpu_scene_end(s, previous, &data) && data.words[10] == 1);
    CHECK(data.words[data.words[8] + 1] == 0x80 &&
          data.words[data.words[8] + 30] == 8 && data.words[data.words[8] + 31] == 9 &&
          data.words[data.words[8] + 34] == 20 && data.words[data.words[8] + 35] == 21 &&
          data.words[data.words[8] + 40] == 0);

    /* If a copy reads a preceding GPU draw, the capture requests one exact
     * CPU materialization and stores that pre-copy VRAM version for the GPU. */
    setup(32, 32);
    const u32 dependent_copy[] = {0x600000ff, 0x00020002, 0x00040004,
                                  0x80000000, 2u | (2u << 16), 8u | (8u << 16),
                                  4u | (4u << 16)};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(dependent_copy, sizeof(dependent_copy) / sizeof(*dependent_copy));
    CHECK(hm_gpu_scene_needs_materialization(s));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    hm_gpu_scene_materialize_vram(s, 1);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(dependent_copy, sizeof(dependent_copy) / sizeof(*dependent_copy));
    CHECK(hm_gpu_scene_end(s, previous, &data) && data.vram_version_count == 1);
    CHECK(data.words[data.words[8] + HM_SCENE_STRIDE + 40] == 1);
    hm_gpu_scene_materialize_vram(s, 0);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    for (int i = 0; i < 257; ++i)
        hm_gpu_execute_words(rectangle, 3);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[7] == 16 && data.words[data.words[data.words[9]]] == 257);
    setup(1024, 512);
    area = (RECT){0, 0, 1024, 512};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    for (u32 i = 0; i <= HM_SCENE_INITIAL_PRIMITIVES; ++i) {
        rectangle[1] = (u32)((i % 64) * 16) | ((u32)(((i / 64) % 32) * 16) << 16);
        hm_gpu_execute_words(rectangle, 3);
    }
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[10] == HM_SCENE_INITIAL_PRIMITIVES + 1);
    previous = hm_gpu_scene_begin(s, 0, 0);
    hm_replay_begin(&area);
    CHECK(!hm_gpu_scene_end(s, previous, &data));
    /* A clip extending past the viewport is safe when its geometry stays inside. */
    setup(32, 32);
    area = (RECT){0, 0, 32, 32};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 dialog[] = {0xe4000000u | 31u | (63u << 10), 0x600000ff, 0x00020002, 0x00040004};
    hm_gpu_execute_words(dialog, 4);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    /* Page overlaps the framebuffer, but the sampled rows do not. */
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 safe_uv[] = {0xe1000600, 0x65808080, 0x00020002, (getClut(896, 490) << 16) | 0xf000,
                     0x00040004};
    hm_gpu_execute_words(safe_uv, 5);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 unsafe_clip[] = {0xe4000000u | 31u | (63u << 10), 0x600000ff, 0x00280002, 0x00040004};
    hm_gpu_execute_words(unsafe_clip, 4);
    /* Independent textured draw sampling v=240 (tile y=15) stays on GPU */
    safe_uv[3] = ((u32)getClut(896, 490) << 16) | 0xf000;
    hm_gpu_execute_words(safe_uv, 5);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    /* A texture that reads an offscreen write requests a materialized replay. */
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(unsafe_clip, 4);
    safe_uv[3] = ((u32)getClut(896, 490) << 16) | 0x2800;
    hm_gpu_execute_words(safe_uv, 5);
    CHECK(hm_gpu_scene_needs_materialization(s));
    CHECK(hm_gpu_scene_end(s, previous, &data));
    /* Multisample locations can cross an 8-pixel bin boundary even when the
     * output pixel centre lies in the neighboring bin. */
    const u32 triangles[2][4] = {{0x200000ff, 0x00040002, 0x00040006, 0x00080002},
                                 {0x200000ff, 0x00020004, 0x00020008, 0x00060004}};
    for (unsigned samples = 1; samples <= 4; samples *= 2) {
        for (int axis = 0; axis < 2; ++axis) {
            setup(32, 32);
            hm_gpu_scene_multisamples(s, samples);
            previous = hm_gpu_scene_begin(s, 65, 97);
            hm_replay_begin(&area);
            hm_gpu_execute_words(triangles[axis], 4);
            CHECK(hm_gpu_scene_end(s, previous, &data));
            int bin = axis ? 1 : 4;
            u32 list = data.words[data.words[9] + bin];
            CHECK(data.words[list] == (samples == 1 ? 0u : 1u));
        }
    }
    hm_gpu_scene_destroy(s);
    return 0;
}
