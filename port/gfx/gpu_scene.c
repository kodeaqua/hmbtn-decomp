#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#endif
#include "gfx/gpu_scene.h"
#include "enhance/pgxp/pgxp.h"
#include "enhance/dithering/dithering.h"
#include <math.h>
#include "psx/gpu_environment.h"
#include "psx/vram.h"
#include <stdlib.h>
#include <string.h>
enum { HM_SCENE_VRAM_TILE_SIZE = 16, HM_SCENE_VRAM_TILES_X = HM_VRAM_WIDTH / 16,
       HM_SCENE_VRAM_TILES_Y = HM_VRAM_HEIGHT / 16 };
struct hm_gpu_scene {
    u32 (*primitives)[HM_SCENE_STRIDE];
    u32 *bins;
    u32 *packed;
    size_t bin_capacity_words, packed_capacity_words;
    u32 primitive_capacity, bin_stride;
    u16 vram_snapshot[HM_VRAM_HEIGHT][HM_VRAM_WIDTH];
    u16 *vram_versions;
    u64 *vram_version_hashes;
    u64 *vram_version_tiles;
    u64 vram_tile_hashes[HM_SCENE_VRAM_TILES_X * HM_SCENE_VRAM_TILES_Y];
    u64 vram_tile_hash_valid[HM_SCENE_VRAM_TILES_Y];
    u64 vram_changed_tiles[HM_SCENE_VRAM_TILES_Y];
    u64 vram_hash, vram_base_hash;
    u32 width, height, count, tiles_x, tiles_y;
    u32 vram_version_count, vram_version_capacity;
    u32 pending_transfer_indices[4], pending_transfer_count;
    int pending_transfer_requires_materialization;
    RECT area, native;
    const u16 *vram_pixels;
    u16 background;
    const char *failure;
    int begun, texture_filter, materialize_vram, needs_materialization, vram_dirty_tracking;
    u64 written_tiles[32]; /* Conservative 16x16 VRAM tile footprint for prior writes. */
    u64 current_texture_tiles[32];
    u64 materialization_tiles[32]; /* Tiles read after an earlier scene write. */
    unsigned multisamples;
    u64 revision;
    const u32 *motion_previous, *motion_current;
    u32 motion_count, motion_alpha;
};
_Static_assert(sizeof(((hm_gpu_scene *)0)->primitives[0]) == 164, "GPU descriptor ABI");
static int hm_scene_reserve_primitives(hm_gpu_scene *s, u32 needed) {
    if (needed <= s->primitive_capacity)
        return 1;
    u32 capacity = s->primitive_capacity ? s->primitive_capacity : HM_SCENE_INITIAL_PRIMITIVES;
    while (capacity < needed) {
        if (capacity > UINT32_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if ((size_t)capacity > SIZE_MAX / sizeof(*s->primitives))
        return 0;
    void *primitives = realloc(s->primitives, (size_t)capacity * sizeof(*s->primitives));
    if (!primitives)
        return 0;
    s->primitives = primitives;
    s->primitive_capacity = capacity;
    return 1;
}
static int hm_scene_reserve_bins(hm_gpu_scene *s, size_t tile_count, u32 stride) {
    if (stride < HM_SCENE_BIN_WORDS)
        stride = HM_SCENE_BIN_WORDS;
    if (tile_count > SIZE_MAX / stride)
        return 0;
    size_t needed = tile_count * stride;
    if (stride == s->bin_stride && needed <= s->bin_capacity_words)
        return 1;
    u32 *bins = calloc(needed, sizeof(*bins));
    if (!bins)
        return 0;
    if (s->bins && s->bin_stride) {
        size_t old_tiles = s->bin_capacity_words / s->bin_stride;
        size_t copy_tiles = old_tiles < tile_count ? old_tiles : tile_count;
        u32 copy_words = s->bin_stride < stride ? s->bin_stride : stride;
        for (size_t i = 0; i < copy_tiles; ++i)
            memcpy(bins + i * stride, s->bins + i * s->bin_stride,
                   (size_t)copy_words * sizeof(*bins));
    }
    free(s->bins);
    s->bins = bins;
    s->bin_stride = stride;
    s->bin_capacity_words = needed;
    return 1;
}
static int hm_scene_reserve_packed(hm_gpu_scene *s, size_t needed) {
    if (needed <= s->packed_capacity_words)
        return 1;
    size_t capacity = s->packed_capacity_words ? s->packed_capacity_words : 4096;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*s->packed))
        return 0;
    u32 *packed = realloc(s->packed, capacity * sizeof(*packed));
    if (!packed)
        return 0;
    s->packed = packed;
    s->packed_capacity_words = capacity;
    return 1;
}
static int hm_scene_signed(u32 n) { return (int)((n & 2047) ^ 1024) - 1024; }
static s64 hm_scene_floor(s64 n, s64 d) { return n >= 0 ? n / d : -((-n + d - 1) / d); }
static s64 hm_scene_coordinate(hm_gpu_scene *s, u32 word, int shift, u32 fallback, int offset) {
    if (s->motion_previous && s->motion_current && word < s->motion_count) {
        int a = hm_scene_signed(s->motion_previous[word] >> shift),
            b = hm_scene_signed(s->motion_current[word] >> shift);
        return (s64)(a + offset) * 65536 + (s64)(b - a) * s->motion_alpha;
    }
    return (s64)(hm_scene_signed(fallback >> shift) + offset) * 65536;
}
static u32 hm_scene_scaled(s64 q, int origin, u32 size, int logical) {
    return (u32)hm_scene_floor((q - (s64)origin * 65536) * size * 256, (s64)logical * 65536);
}
static u64 hm_scene_vram_tile_hash(const u16 *pixels, u32 tile_x, u32 tile_y) {
    const u64 prime = UINT64_C(1099511628211);
    u64 hash = UINT64_C(14695981039346656037) ^ ((u64)tile_y << 32) ^ tile_x;
    for (u32 y = 0; y < HM_SCENE_VRAM_TILE_SIZE; ++y) {
        const u16 *row = pixels + (size_t)(tile_y * HM_SCENE_VRAM_TILE_SIZE + y) * HM_VRAM_WIDTH +
                         tile_x * HM_SCENE_VRAM_TILE_SIZE;
        for (u32 x = 0; x < HM_SCENE_VRAM_TILE_SIZE; ++x) {
            hash = (hash ^ row[x]) * prime;
        }
    }
    return hash;
}
static u32 hm_scene_first_set_tile(u64 mask) {
#if defined(__GNUC__) || defined(__clang__)
    return (u32)__builtin_ctzll(mask);
#else
    u32 index = 0;
    while (!(mask & 1)) {
        mask >>= 1;
        ++index;
    }
    return index;
#endif
}
static int hm_scene_vram_tile_equal(const u16 *a, const u16 *b, u32 tile_index) {
    u32 tile_x = tile_index % HM_SCENE_VRAM_TILES_X;
    u32 tile_y = tile_index / HM_SCENE_VRAM_TILES_X;
    for (u32 y = 0; y < HM_SCENE_VRAM_TILE_SIZE; ++y) {
        const u16 *a_row = a + (size_t)(tile_y * HM_SCENE_VRAM_TILE_SIZE + y) * HM_VRAM_WIDTH +
                           tile_x * HM_SCENE_VRAM_TILE_SIZE;
        const u16 *b_row = b + (size_t)(tile_y * HM_SCENE_VRAM_TILE_SIZE + y) * HM_VRAM_WIDTH +
                           tile_x * HM_SCENE_VRAM_TILE_SIZE;
        if (memcmp(a_row, b_row, HM_SCENE_VRAM_TILE_SIZE * sizeof(u16)))
            return 0;
    }
    return 1;
}
static int hm_scene_vram_equal_tiles(const u16 *a, const u16 *b,
                                    const u64 tiles[HM_SCENE_VRAM_TILES_Y]) {
    for (u32 y = 0; y < HM_SCENE_VRAM_TILES_Y; ++y) {
        u64 row = tiles[y];
        while (row) {
            u32 x = hm_scene_first_set_tile(row);
            u32 tile_index = y * HM_SCENE_VRAM_TILES_X + x;
            if (!hm_scene_vram_tile_equal(a, b, tile_index))
                return 0;
            row &= row - 1;
        }
    }
    return 1;
}
static void hm_scene_vram_hash_init(hm_gpu_scene *s) {
    s->vram_hash = 0;
    s->vram_base_hash = s->vram_hash;
    memset(s->vram_tile_hash_valid, 0, sizeof(s->vram_tile_hash_valid));
    memset(s->vram_changed_tiles, 0, sizeof(s->vram_changed_tiles));
    /* The copied baseline already includes any dirty tiles inherited by this
     * private replay context. Compute baseline hashes lazily when a tile first
     * changes, so sparse feedback does not hash all 512K VRAM pixels per frame. */
    for (u32 y = 0; y < HM_SCENE_VRAM_TILES_Y; ++y)
        hm_vram_acknowledge(y, hm_vram_dirty_row(y));
}
static void hm_scene_vram_hash_update(hm_gpu_scene *s, const u16 *pixels) {
    for (u32 y = 0; y < HM_SCENE_VRAM_TILES_Y; ++y) {
        u64 dirty = hm_vram_dirty_row(y);
        for (u32 x = 0; x < HM_SCENE_VRAM_TILES_X; ++x) {
            u64 bit = UINT64_C(1) << x;
            if (!(dirty & bit))
                continue;
            u32 index = y * HM_SCENE_VRAM_TILES_X + x;
            u64 previous = s->vram_tile_hash_valid[y] & bit
                               ? s->vram_tile_hashes[index]
                               : hm_scene_vram_tile_hash(&s->vram_snapshot[0][0], x, y);
            u64 next = hm_scene_vram_tile_hash(pixels, x, y);
            s->vram_hash ^= previous ^ next;
            s->vram_tile_hashes[index] = next;
            s->vram_tile_hash_valid[y] |= bit;
            if (hm_scene_vram_tile_equal(pixels, &s->vram_snapshot[0][0], index))
                s->vram_changed_tiles[y] &= ~bit;
            else
                s->vram_changed_tiles[y] |= bit;
        }
        if (dirty)
            hm_vram_acknowledge(y, dirty);
    }
}
static int hm_scene_tiles(u64 tiles[32], u32 x, u32 y, u32 width, u32 height, int mark) {
    if (!width || !height)
        return 0;
    u32 last_x = (x + width - 1) / 16, last_y = (y + height - 1) / 16;
    int overlap = 0;
    for (u32 ty = y / 16; ty <= last_y; ++ty) {
        for (u32 tx = x / 16; tx <= last_x; ++tx) {
            u64 bit = UINT64_C(1) << (tx & 63);
            u64 *row = &tiles[ty & 31];
            overlap |= (*row & bit) != 0;
            if (mark)
                *row |= bit;
        }
    }
    return overlap;
}
static int hm_scene_mark_write(hm_gpu_scene *s, int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > HM_VRAM_WIDTH) x1 = HM_VRAM_WIDTH;
    if (y1 > HM_VRAM_HEIGHT) y1 = HM_VRAM_HEIGHT;
    if (x0 >= x1 || y0 >= y1)
        return 0;
    return hm_scene_tiles(s->written_tiles, (u32)x0, (u32)y0, (u32)(x1 - x0),
                          (u32)(y1 - y0), 1);
}
static u32 hm_scene_snapshot(hm_gpu_scene *s, int allow_unmaterialized) {
    if (!s->materialize_vram && !allow_unmaterialized) {
        s->needs_materialization = 1;
        return 0;
    }
    const u16 *current = hm_vram_state()->pixels[0];
    size_t bytes = sizeof(s->vram_snapshot);
    if (s->vram_dirty_tracking) {
        hm_scene_vram_hash_update(s, current);
        int matches_base = 1;
        for (u32 y = 0; y < HM_SCENE_VRAM_TILES_Y; ++y)
            matches_base &= s->vram_changed_tiles[y] == 0;
        if (matches_base)
            return 0;
        for (u32 i = 0; i < s->vram_version_count; ++i) {
            const u16 *version = s->vram_versions + (size_t)i * HM_VRAM_WIDTH * HM_VRAM_HEIGHT;
            const u64 *version_tiles = s->vram_version_tiles +
                                       (size_t)i * HM_SCENE_VRAM_TILES_Y;
            if (s->vram_version_hashes[i] == s->vram_hash &&
                !memcmp(version_tiles, s->vram_changed_tiles,
                        sizeof(s->vram_changed_tiles)) &&
                hm_scene_vram_equal_tiles(version, current, s->vram_changed_tiles))
                return i + 1;
        }
    } else {
        for (u32 i = 0; i < s->vram_version_count; ++i) {
            const u16 *version = s->vram_versions + (size_t)i * HM_VRAM_WIDTH * HM_VRAM_HEIGHT;
            if (!memcmp(version, current, bytes))
                return i + 1;
        }
    }
    if (s->vram_version_count == s->vram_version_capacity) {
        u32 capacity = s->vram_version_capacity ? s->vram_version_capacity * 2 : 2;
        if (capacity < s->vram_version_count ||
            (size_t)capacity > SIZE_MAX / (HM_VRAM_WIDTH * HM_VRAM_HEIGHT * sizeof(u16)) ||
            (size_t)capacity > SIZE_MAX / sizeof(*s->vram_version_hashes) ||
            (size_t)capacity > SIZE_MAX / sizeof(*s->vram_version_tiles) /
                                    HM_SCENE_VRAM_TILES_Y) {
            s->failure = "texture snapshot capacity";
            return 0;
        }
        u16 *versions = realloc(s->vram_versions,
                                (size_t)capacity * HM_VRAM_WIDTH * HM_VRAM_HEIGHT * sizeof(u16));
        if (!versions) {
            s->failure = "texture snapshot allocation";
            return 0;
        }
        s->vram_versions = versions;
        u64 *hashes = realloc(s->vram_version_hashes, (size_t)capacity * sizeof(*hashes));
        if (!hashes) {
            s->failure = "texture snapshot hash allocation";
            return 0;
        }
        s->vram_version_hashes = hashes;
        u64 *tiles = realloc(s->vram_version_tiles,
                             (size_t)capacity * HM_SCENE_VRAM_TILES_Y * sizeof(*tiles));
        if (!tiles) {
            s->failure = "texture snapshot tile allocation";
            return 0;
        }
        s->vram_version_tiles = tiles;
        s->vram_version_capacity = capacity;
    }
    u16 *destination = s->vram_versions +
                       (size_t)s->vram_version_count * HM_VRAM_WIDTH * HM_VRAM_HEIGHT;
    memcpy(destination, current, bytes);
    if (s->vram_dirty_tracking) {
        s->vram_version_hashes[s->vram_version_count] = s->vram_hash;
        memcpy(s->vram_version_tiles +
                   (size_t)s->vram_version_count * HM_SCENE_VRAM_TILES_Y,
               s->vram_changed_tiles, sizeof(s->vram_changed_tiles));
    }
    return ++s->vram_version_count;
}
static int hm_scene_mark_materialization(hm_gpu_scene *s, u32 x, u32 y, u32 width,
                                         u32 height) {
    if (!width || !height)
        return 0;
    u32 last_x = (x + width - 1) / 16, last_y = (y + height - 1) / 16;
    int dependency = 0;
    for (u32 ty = y / 16; ty <= last_y; ++ty) {
        u32 row = ty & 31u;
        for (u32 tx = x / 16; tx <= last_x; ++tx) {
            u64 bit = UINT64_C(1) << (tx & 63u);
            if (s->written_tiles[row] & bit) {
                s->materialization_tiles[row] |= bit;
                dependency = 1;
            }
        }
    }
    return dependency;
}
static int hm_scene_material(hm_gpu_scene *s, u32 *p, u32 op, u32 page, u32 clut) {
    const hm_gpu_environment *env = hm_gpu_environment_state();
    const DRAWENV *d = &env->draw;
    memset(s->current_texture_tiles, 0, sizeof(s->current_texture_tiles));
    if (op & 4) {
        u32 depth = (page >> 7) & 3;
        if (depth > 2) {
            s->failure = "reserved texture depth";
            return 0;
        }
        if (depth < 2) {
            u32 clut_x = (clut & 63) * 16, clut_y = (clut >> 6) & 511;
            u32 clut_width = depth ? 256 : 16;
            hm_scene_tiles(s->current_texture_tiles, clut_x, clut_y, clut_width, 1, 1);
        }
    }
    const hm_vram_context *vram = hm_vram_state();
    p[1] = op;
    p[2] = page;
    p[3] = clut;
    int dtd = hm_dither_enabled() ? (int)d->dtd : 0;
    /* Bit 64 permits higher precision only in enhanced presentation. */
    p[4] = (u32)dtd | (d->dfe << 1) | (vram->set_mask ? 4u : 0) | (vram->check_mask ? 8u : 0) |
           (!hm_dither_enabled() ? 64u : 0);
    p[5] = d->tw.x;
    p[6] = d->tw.y;
    p[7] = d->tw.w;
    p[8] = d->tw.h;
    p[9] = d->clip.x;
    p[10] = d->clip.y;
    p[11] = d->clip.w;
    p[12] = d->clip.h;
    p[13] = env->display.disp.x;
    p[14] = env->display.disp.y;
    p[15] = env->display.disp.w;
    p[16] = env->display.disp.h;
    p[17] = env->display_enabled;
    return 1;
}
/* Conservative sampled UV footprint, including texture-window remapping and
 * 8-bit wrapping. A page can share unused rows with the native framebuffer. */
static int hm_scene_texture_bounds(hm_gpu_scene *s, u32 *p, u32 u, u32 v, u32 width,
                                   u32 height) {
    if (!(p[1] & 4) || !width || !height)
        return 1;
    u32 depth = (p[2] >> 7) & 3, mx = (256 - p[7]) & 248, my = (256 - p[8]) & 248;
    int x0 = 1024, x1 = -1, y0 = 512, y1 = -1;
    if (width > 256)
        width = 256;
    if (height > 256)
        height = 256;
    u32 u_base = u & 255, v_base = v & 255;
    if (!mx && u_base + width <= 256) {
        x0 = (int)(u_base >> (2 - depth));
        x1 = (int)((u_base + width - 1) >> (2 - depth));
    } else {
        for (u32 i = 0; i < width; ++i) {
            u32 mapped = ((((u + i) & 255) & ~mx) | (p[5] & mx)) & 255;
            int x = (int)(mapped >> (2 - depth));
            if (x < x0)
                x0 = x;
            if (x > x1)
                x1 = x;
        }
    }
    if (!my && v_base + height <= 256) {
        y0 = (int)v_base;
        y1 = (int)(v_base + height - 1);
    } else {
        for (u32 i = 0; i < height; ++i) {
            int y = (int)(((((v + i) & 255) & ~my) | (p[6] & my)) & 255);
            if (y < y0)
                y0 = y;
            if (y > y1)
                y1 = y;
        }
    }
    u32 texture_x = (p[2] & 15) * 64 + (u32)x0;
    u32 texture_y = (p[2] & 16) * 16 + (u32)y0;
    u32 texture_w = (u32)(x1 - x0 + 1), texture_h = (u32)(y1 - y0 + 1);
    int dependency =
        hm_scene_mark_materialization(s, texture_x, texture_y, texture_w, texture_h);
    if (depth < 2) {
        u32 clut_x = (p[3] & 63) * 16, clut_y = (p[3] >> 6) & 511;
        u32 clut_w = depth ? 256 : 16;
        dependency |= hm_scene_mark_materialization(s, clut_x, clut_y, clut_w, 1);
        hm_scene_tiles(s->current_texture_tiles, clut_x, clut_y, clut_w, 1, 1);
    }
    if (dependency)
        p[40] = hm_scene_snapshot(s, 0);
    hm_scene_tiles(s->current_texture_tiles, texture_x, texture_y, texture_w, texture_h, 1);
    return 1;
}
static void hm_scene_add(hm_gpu_scene *s, u32 *p, int x0, int y0, int x1, int y1) {
    if (s->failure)
        return;
    if (p[4] & 32) {
        --x0;
        --y0;
        ++x1;
        ++y1;
    }
    if (x0 < (int)p[9])
        x0 = (int)p[9];
    if (y0 < (int)p[10])
        y0 = (int)p[10];
    if (x1 > (int)p[9] + (int)p[11])
        x1 = (int)p[9] + (int)p[11];
    if (y1 > (int)p[10] + (int)p[12])
        y1 = (int)p[10] + (int)p[12];
    if (x0 >= x1 || y0 >= y1)
        return;
    if ((p[1] & 4) &&
        hm_scene_tiles(s->current_texture_tiles, (u32)x0, (u32)y0, (u32)(x1 - x0),
                       (u32)(y1 - y0), 0)) {
        if (s->materialize_vram)
            p[40] = hm_scene_snapshot(s, 0);
        else
            s->needs_materialization = 1;
    }
    /* Track writes everywhere in native VRAM, not just offscreen draws. A
     * later textured primitive may sample an earlier draw from its own target. */
    hm_scene_mark_write(s, x0, y0, x1, y1);
    if (x0 < s->area.x || y0 < s->area.y || x1 > s->area.x + s->area.w ||
        y1 > s->area.y + s->area.h) {
        if (x1 <= s->area.x || y1 <= s->area.y || x0 >= s->area.x + s->area.w ||
            y0 >= s->area.y + s->area.h)
            return;
        if (x0 < s->area.x) x0 = s->area.x;
        if (y0 < s->area.y) y0 = s->area.y;
        if (x1 > s->area.x + s->area.w) x1 = s->area.x + s->area.w;
        if (y1 > s->area.y + s->area.h) y1 = s->area.y + s->area.h;
    }
    if (x0 >= x1 || y0 >= y1)
        return;
    if (x0 >= (int)p[9] + (int)p[11] || y0 >= (int)p[10] + (int)p[12] || x1 <= (int)p[9] ||
        y1 <= (int)p[10])
        return;
    if (x0 >= x1 || y0 >= y1)
        return;
    if (s->count == UINT32_MAX || !hm_scene_reserve_primitives(s, s->count + 1)) {
        s->failure = "primitive allocation";
        return;
    }
    u32 primitive_index = s->count;
    memcpy(s->primitives[primitive_index], p, HM_SCENE_STRIDE * sizeof(*p));
    /* A multisample can lie inside a triangle even when the output pixel's
     * logical centre belongs to the adjacent bin. Include neighboring bins;
     * retain the original bounds for VRAM dependency and clipping decisions. */
    int bin_x0 = x0, bin_y0 = y0, bin_x1 = x1, bin_y1 = y1;
    if (s->multisamples > 1 && p[0] == 0) {
        if (bin_x0 > s->area.x) --bin_x0;
        if (bin_y0 > s->area.y) --bin_y0;
        if (bin_x1 < s->area.x + s->area.w) ++bin_x1;
        if (bin_y1 < s->area.y + s->area.h) ++bin_y1;
    }
    u32 bx0 = (u32)(bin_x0 - s->area.x) / HM_SCENE_BIN_TILE_SIZE,
        by0 = (u32)(bin_y0 - s->area.y) / HM_SCENE_BIN_TILE_SIZE;
    u32 bx1 = (u32)(bin_x1 - 1 - s->area.x) / HM_SCENE_BIN_TILE_SIZE,
        by1 = (u32)(bin_y1 - 1 - s->area.y) / HM_SCENE_BIN_TILE_SIZE;
    for (u32 y = by0; y <= by1; ++y)
        for (u32 x = bx0; x <= bx1; ++x) {
            size_t bin_index = (size_t)y * s->tiles_x + x;
            u32 *bin = s->bins + bin_index * s->bin_stride;
            if (bin[0] + 1 >= s->bin_stride) {
                if (s->bin_stride > UINT32_MAX / 2 ||
                    !hm_scene_reserve_bins(s, (size_t)s->tiles_x * s->tiles_y,
                                           s->bin_stride * 2)) {
                    s->failure = "tile allocation";
                    return;
                }
                bin = s->bins + bin_index * s->bin_stride;
            }
            if (bin[0] == UINT32_MAX) {
                s->failure = "tile index overflow";
                return;
            }
            bin[++bin[0]] = primitive_index;
        }
    ++s->count;
}
static void hm_scene_begin_capture(void *context, const RECT *area) {
    hm_gpu_scene *s = context;
    s->begun = 0;
    int expanded = 0;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT expected = hm_widescreen_area(hm_gpu_environment_state()->draw.clip);
    expanded = area->x == expected.x && area->y == expected.y && area->w == expected.w &&
               area->h == expected.h;
#endif
    if (!expanded && (area->x < 0 || area->x + area->w > 1024)) {
        s->failure = "unsupported render dimensions";
        return;
    }
    if (!s->width || !s->height || s->width > 4096 || s->height > 4096 || area->x < -352 ||
        area->y < 0 || area->w <= 0 || area->w > 1024 || area->h <= 0 || area->x + area->w > 1376 ||
        area->y + area->h > 512) {
        s->failure = "unsupported render dimensions";
        return;
    }
    s->area = *area;
    s->native = hm_gpu_environment_state()->draw.clip;
    s->background = hm_gpu_background(&hm_gpu_environment_state()->draw);
    s->tiles_x = ((u32)area->w + HM_SCENE_BIN_TILE_SIZE - 1) / HM_SCENE_BIN_TILE_SIZE;
    s->tiles_y = ((u32)area->h + HM_SCENE_BIN_TILE_SIZE - 1) / HM_SCENE_BIN_TILE_SIZE;
    size_t tile_count = (size_t)s->tiles_x * s->tiles_y;
    if (!hm_scene_reserve_bins(s, tile_count, s->bin_stride)) {
        s->failure = "tile allocation";
        return;
    }
    for (size_t i = 0; i < tile_count; ++i)
        s->bins[i * s->bin_stride] = 0;
    /* The probe replay does not materialize feedback snapshots. Hash only the
     * private replay that needs exact VRAM versions, avoiding a full hash pass
     * on every ordinary frame. */
    s->vram_dirty_tracking = !hm_vram_is_native() && s->materialize_vram;
    memcpy(s->vram_snapshot, hm_vram_state()->pixels, sizeof(s->vram_snapshot));
    if (s->vram_dirty_tracking)
        hm_scene_vram_hash_init(s);
    s->vram_pixels = &s->vram_snapshot[0][0];
    s->begun = 1;
}
static void hm_scene_triangle(hm_gpu_scene *s, u32 *material, const u32 xy[3], const u32 rgb[3],
                              const u32 uv[3], const u32 indices[3], const u32 *w) {
    u32 p[HM_SCENE_STRIDE];
    memcpy(p, material, sizeof(p));
    p[0] = 0;
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    int x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;

    float pgxp_x[3], pgxp_y[3], pgxp_z[3];
    int has_pgxp = hm_pgxp_enabled();
    if (has_pgxp) {
        for (int i = 0; i < 3; ++i) {
            const void *ptr = &w[indices[i]];
            if (!hm_pgxp_lookup_vertex(ptr, xy[i], &pgxp_x[i], &pgxp_y[i], &pgxp_z[i])) {
                has_pgxp = 0;
                break;
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        int x = hm_scene_signed(xy[i]) + d->ofs[0], y = hm_scene_signed(xy[i] >> 16) + d->ofs[1];
        p[18 + i * 2] = (u32)x;
        p[19 + i * 2] = (u32)y;
        p[24 + i] = rgb[i] & 0xffffff;
        p[27 + i] = uv[i] & 65535;
        if (has_pgxp && !s->motion_previous) {
            s64 qx = (s64)roundf(pgxp_x[i] * 65536.0f) + (s64)d->ofs[0] * 65536;
            s64 qy = (s64)roundf(pgxp_y[i] * 65536.0f) + (s64)d->ofs[1] * 65536;
            p[34 + i * 2] = hm_scene_scaled(qx, s->area.x, s->width, s->area.w);
            p[35 + i * 2] = hm_scene_scaled(qy, s->area.y, s->height, s->area.h);
            float z_val = pgxp_z[i] > 1.0f ? pgxp_z[i] : 1.0f;
            memcpy(&p[30 + i], &z_val, sizeof(float));
        } else {
            s64 qx = hm_scene_coordinate(s, indices[i], 0, xy[i], d->ofs[0]);
            s64 qy = hm_scene_coordinate(s, indices[i], 16, xy[i], d->ofs[1]);
            p[34 + i * 2] = hm_scene_scaled(qx, s->area.x, s->width, s->area.w);
            p[35 + i * 2] = hm_scene_scaled(qy, s->area.y, s->height, s->area.h);
        }
        if (x < x0)
            x0 = x;
        if (x > x1)
            x1 = x;
        if (y < y0)
            y0 = y;
        if (y > y1)
            y1 = y;
    }
    /* Match the native reference rasterizer's bounded primitive span. */
    if (x1 - x0 > 1023 || y1 - y0 > 511)
        return;
    /* Bins must cover the entire interpolated primitive, including pixels at
     * the previous endpoint. The current-frame bounds alone can clip moving
     * edges and leave gaps between adjacent primitives. */
    if (s->motion_previous && s->motion_current) {
        for (int i = 0; i < 3; ++i) {
            if (indices[i] >= s->motion_count)
                continue;
            u32 previous = s->motion_previous[indices[i]];
            int x = hm_scene_signed(previous) + d->ofs[0];
            int y = hm_scene_signed(previous >> 16) + d->ofs[1];
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }

    s64 area = (s64)((s32)p[20] - (s32)p[18]) * ((s32)p[23] - (s32)p[19]) -
               (s64)((s32)p[21] - (s32)p[19]) * ((s32)p[22] - (s32)p[18]);
    if (s->motion_previous || has_pgxp) {
        p[4] |= 32;
        /* Interpolated vertices do not carry interpolated depth. Keep affine
         * texture mapping for those frames instead of using unset PGXP Z. */
        if (has_pgxp && !s->motion_previous) {
            p[33] = 1;
        }
        area = ((s64)(s32)p[36] - (s32)p[34]) * ((s64)(s32)p[39] - (s32)p[35]) -
               ((s64)(s32)p[37] - (s32)p[35]) * ((s64)(s32)p[38] - (s32)p[34]);
    }
    if (!area)
        return;
    if (area < 0) {
        u32 tmp;
        for (int j = 0; j < 2; ++j) {
            tmp = p[20 + j];
            p[20 + j] = p[22 + j];
            p[22 + j] = tmp;
        }
        for (int j = 0; j < 2; ++j) {
            tmp = p[36 + j];
            p[36 + j] = p[38 + j];
            p[38 + j] = tmp;
        }
        tmp = p[25];
        p[25] = p[26];
        p[26] = tmp;
        tmp = p[28];
        p[28] = p[29];
        p[29] = tmp;
        if (has_pgxp) {
            tmp = p[31];
            p[31] = p[32];
            p[32] = tmp;
        }
    }
    if (p[1] & 4) {
        u32 u0 = 255, u1 = 0, v0 = 255, v1 = 0;
        for (int i = 0; i < 3; ++i) {
            u32 u = p[27 + i] & 255, v = (p[27 + i] >> 8) & 255;
            if (u < u0)
                u0 = u;
            if (u > u1)
                u1 = u;
            if (v < v0)
                v0 = v;
            if (v > v1)
                v1 = v;
        }
        if (!hm_scene_texture_bounds(s, p, u0, v0, u1 - u0 + 1, v1 - v0 + 1))
            return;
    }
    hm_scene_add(s, p, x0, y0, x1, y1);
}
static void hm_scene_polygon(hm_gpu_scene *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, n = op & 8 ? 4 : 3;
    int tex = (op & 4) != 0, shaded = (op & 16) != 0;
    if (1 + n + (tex ? n : 0) + (shaded ? n - 1 : 0) > available) {
        s->failure = "truncated polygon";
        return;
    }
    u32 xy[4], rgb[4], uv[4] = {0}, indices[4], at = 1, color = w[0];
    for (u32 i = 0; i < n; ++i) {
        if (i && shaded)
            color = w[at++];
        rgb[i] = color;
        indices[i] = at;
        xy[i] = w[at++];
        if (tex)
            uv[i] = w[at++];
    }
    u32 p[HM_SCENE_STRIDE] = {0};
    u32 page = tex ? uv[1] >> 16 : hm_gpu_environment_state()->draw.tpage;
    if (!hm_scene_material(s, p, op, page, tex ? uv[0] >> 16 : 0))
        return;
    hm_scene_triangle(s, p, xy, rgb, uv, indices, w);
    if (n == 4)
        hm_scene_triangle(s, p, xy + 1, rgb + 1, uv + 1, indices + 1, w);
}
static void hm_scene_rectangle(hm_gpu_scene *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, size = (op >> 3) & 3;
    int tex = (op & 4) != 0;
    u32 length = 2 + (u32)tex + (size == 0);
    if (length > available) {
        s->failure = "truncated rectangle";
        return;
    }
    u32 width = size == 1 ? 1 : size == 2 ? 8 : 16, height = width;
    if (!size) {
        width = w[length - 1] & 1023;
        height = (w[length - 1] >> 16) & 511;
    }
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    u32 p[HM_SCENE_STRIDE] = {0};
    if (!hm_scene_material(s, p, op, d->tpage, tex ? w[2] >> 16 : 0))
        return;
    int x = hm_scene_signed(w[1]) + d->ofs[0], y = hm_scene_signed(w[1] >> 16) + d->ofs[1];
    p[0] = 1;
    p[24] = w[0] & 0xffffff;
    p[27] = tex ? w[2] & 65535 : 0;
    p[30] = (u32)x;
    p[31] = (u32)y;
    p[32] = width;
    p[33] = height;
    if (s->motion_previous) {
        s64 qx = hm_scene_coordinate(s, 1, 0, w[1], d->ofs[0]);
        s64 qy = hm_scene_coordinate(s, 1, 16, w[1], d->ofs[1]);
        p[4] |= 32;
        p[35] = hm_scene_scaled(qx, s->area.x, s->width, s->area.w);
        p[36] = hm_scene_scaled(qy, s->area.y, s->height, s->area.h);
        p[37] = hm_scene_scaled(qx + (s64)width * 65536, s->area.x, s->width, s->area.w);
        p[38] = hm_scene_scaled(qy + (s64)height * 65536, s->area.y, s->height, s->area.h);
    }
    if (!hm_scene_texture_bounds(s, p, p[27] & 255, (p[27] >> 8) & 255, width, height))
        return;
    int x0 = x, y0 = y, x1 = x + (int)width, y1 = y + (int)height;
    if (s->motion_previous && s->motion_current && s->motion_count > 1) {
        int previous_x = hm_scene_signed(s->motion_previous[1]) + d->ofs[0];
        int previous_y = hm_scene_signed(s->motion_previous[1] >> 16) + d->ofs[1];
        if (previous_x < x0) x0 = previous_x;
        if (previous_y < y0) y0 = previous_y;
        if (previous_x + (int)width > x1) x1 = previous_x + (int)width;
        if (previous_y + (int)height > y1) y1 = previous_y + (int)height;
    }
    hm_scene_add(s, p, x0, y0, x1, y1);
}
static void hm_scene_line_segment(hm_gpu_scene *s, u32 xy0, u32 xy1, u32 c0, u32 c1, u32 op,
                                  int skip) {
    int x = hm_scene_signed(xy0), y = hm_scene_signed(xy0 >> 16), x1 = hm_scene_signed(xy1),
        y1 = hm_scene_signed(xy1 >> 16);
    int dx = abs(x1 - x), dy = abs(y1 - y);
    if (dx > 1023 || dy > 511)
        return;
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    static const s8 dith[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    int sx = x < x1 ? 1 : -1, sy = y < y1 ? 1 : -1, error = dx - dy, steps = dx > dy ? dx : dy;
    /* Axis-aligned flat, undithered lines are one rectangle rather than N cells. */
    if (!skip && (c0 & 0xffffff) == (c1 & 0xffffff) && (!dx || !dy) && dx < 1023 && dy < 511) {
        u32 p[3] = {0x60000000u | ((op & 2) << 24) | (c0 & 0xffffff),
                    (u16)(x < x1 ? x : x1) | ((u32)(u16)(y < y1 ? y : y1) << 16),
                    (u32)(dx + 1) | ((u32)(dy + 1) << 16)};
        u32 before = s->count;
        hm_scene_rectangle(s, p, 3);
        if (s->count == before + 1)
            s->primitives[before][34] = 1;
        return;
    }
    for (int at = 0;; ++at) {
        if (!skip || at) {
            u32 rgb = 0;
            for (int c = 0; c < 3; ++c) {
                int a = (c0 >> (c * 8)) & 255, b = (c1 >> (c * 8)) & 255;
                int value = steps ? (a * (steps - at) + b * at + steps / 2) / steps : a;
                if (d->dtd)
                    value += dith[(y + d->ofs[1]) & 3][(x + d->ofs[0]) & 3];
                if (value < 0)
                    value = 0;
                if (value > 255)
                    value = 255;
                rgb |= (u32)value << (c * 8);
            }
            u32 p[3] = {0x60000000u | ((op & 2) << 24) | rgb, (u16)x | ((u32)(u16)y << 16),
                        0x10001};
            hm_scene_rectangle(s, p, 3);
        }
        if ((x == x1 && y == y1) || s->failure)
            break;
        int e = 2 * error;
        if (e > -dy) {
            error -= dy;
            x += sx;
        }
        if (e < dx) {
            error += dx;
            y += sy;
        }
    }
}
static void hm_scene_line(hm_gpu_scene *s, const u32 *w, u32 available) {
    s->motion_previous = s->motion_current = NULL;
    s->motion_count = 0;
    u32 op = w[0] >> 24;
    int shaded = (op & 16) != 0, poly = (op & 8) != 0;
    if (available < (shaded ? 4u : 3u)) {
        s->failure = "truncated line";
        return;
    }
    u32 at = 2, xy = w[1], color = w[0];
    int skip = 0;
    while (at < available) {
        if (poly && skip && (w[at] & 0xf000f000u) == 0x50005000u)
            return;
        u32 next = shaded ? w[at++] : color;
        if (at >= available)
            break;
        u32 point = w[at++];
        hm_scene_line_segment(s, xy, point, color, next, op, skip);
        if (!poly)
            return;
        xy = point;
        color = next;
        skip = 1;
    }
    s->failure = "unterminated line";
}
static int hm_scene_has_writes(const hm_gpu_scene *s) {
    for (u32 i = 0; i < 32; ++i)
        if (s->written_tiles[i])
            return 1;
    return 0;
}
static void hm_scene_transfer_piece(hm_gpu_scene *s, u32 op, u32 color, u32 x, u32 y,
                                    u32 width, u32 height, u32 source_x, u32 source_y,
                                    u32 source_layer) {
    u32 p[HM_SCENE_STRIDE] = {0};
    p[0] = 2;
    p[1] = op;
    const hm_vram_context *vram = hm_vram_state();
    p[4] = 128 | ((op == 0xa0 || op == 0x80) ? (vram->set_mask ? 4u : 0u) |
                                                    (vram->check_mask ? 8u : 0u)
                                              : 0u);
    p[9] = 0;
    p[10] = 0;
    p[11] = HM_VRAM_WIDTH;
    p[12] = HM_VRAM_HEIGHT;
    p[24] = color;
    p[30] = x;
    p[31] = y;
    p[32] = width;
    p[33] = height;
    p[34] = source_x;
    p[35] = source_y;
    p[40] = source_layer;
    u32 before = s->count;
    hm_scene_add(s, p, (int)x, (int)y, (int)(x + width), (int)(y + height));
    if (s->count != before &&
        s->pending_transfer_count < sizeof(s->pending_transfer_indices) /
                                        sizeof(s->pending_transfer_indices[0]))
        s->pending_transfer_indices[s->pending_transfer_count++] = before;
}
static void hm_scene_transfer(hm_gpu_scene *s, u32 op, u32 color, u32 x, u32 y, u32 width,
                              u32 height, u32 source_x, u32 source_y, u32 source_layer) {
    u32 offset_y = 0;
    while (offset_y < height) {
        u32 part_y = (y + offset_y) & (HM_VRAM_HEIGHT - 1);
        u32 part_height = height - offset_y;
        if (part_height > HM_VRAM_HEIGHT - part_y)
            part_height = HM_VRAM_HEIGHT - part_y;
        u32 offset_x = 0;
        while (offset_x < width) {
            u32 part_x = (x + offset_x) & (HM_VRAM_WIDTH - 1);
            u32 part_width = width - offset_x;
            if (part_width > HM_VRAM_WIDTH - part_x)
                part_width = HM_VRAM_WIDTH - part_x;
            hm_scene_transfer_piece(s, op, color, part_x, part_y, part_width, part_height,
                                    (source_x + offset_x) & (HM_VRAM_WIDTH - 1),
                                    (source_y + offset_y) & (HM_VRAM_HEIGHT - 1),
                                    source_layer);
            if (s->failure)
                return;
            offset_x += part_width;
        }
        offset_y += part_height;
    }
}
static int hm_scene_nonrendering_opcode(u32 opcode) {
    /* Mirrors of GP0(00h) are FIFO NOPs; the interrupt command changes GPU
     * status, not the framebuffer. GP0(03h) is undocumented and the native
     * command executor also skips it without changing VRAM. GP0(C0h..DFh)
     * request VRAM-to-CPU data and do not write the displayed scene. */
    return opcode <= 1 || opcode == 3 || (opcode >= 0x04 && opcode <= 0x1f) || opcode == 0xe0 ||
           (opcode >= 0xc0 && opcode <= 0xdf) || (opcode >= 0xe7 && opcode <= 0xef);
}
static void hm_scene_command(void *context, const u32 *w, u32 available, int after) {
    hm_gpu_scene *s = context;
    if (!s->begun || s->failure || !available)
        return;
    u32 op = w[0] >> 24, transfer = op & 0xe0u;
    int copy = transfer == 0x80u, upload = transfer == 0xa0u;
    if (op == 2 || copy || upload) {
        if (after) {
            if (upload && s->pending_transfer_count) {
                u32 layer = hm_scene_snapshot(s, !s->pending_transfer_requires_materialization);
                for (u32 i = 0; i < s->pending_transfer_count; ++i)
                    s->primitives[s->pending_transfer_indices[i]][40] = layer;
            }
            s->pending_transfer_count = 0;
            s->pending_transfer_requires_materialization = 0;
            return;
        }
        u32 header_words = copy ? 4u : 3u;
        if (available < header_words) {
            s->failure = "truncated VRAM transfer";
            return;
        }
        u32 x = w[1] & 1023u, y = (w[1] >> 16) & 511u;
        u32 width, height;
        u32 source_x = 0, source_y = 0, source_layer = 0;
        if (copy) {
            source_x = x;
            source_y = y;
            x = w[2] & 1023u;
            y = (w[2] >> 16) & 511u;
            width = w[3] & 1023u;
            height = (w[3] >> 16) & 511u;
            width = width ? width : HM_VRAM_WIDTH;
            height = height ? height : HM_VRAM_HEIGHT;
            int source_written =
                hm_scene_mark_materialization(s, source_x, source_y, width, height);
            if (source_written) {
                if (!s->materialize_vram)
                    s->needs_materialization = 1;
                source_layer = hm_scene_snapshot(s, s->materialize_vram);
            }
        } else if (upload) {
            width = (((w[2] & 0xffffu) - 1) & 1023u) + 1;
            height = (((w[2] >> 16) - 1) & 511u) + 1;
            size_t payload_words = ((size_t)width * height + 1) / 2;
            if (payload_words > available - header_words) {
                s->failure = "truncated VRAM transfer";
                return;
            }
        } else {
            x &= 0x3f0u;
            width = ((w[2] & 1023u) + 15u) & ~15u;
            height = (w[2] >> 16) & 511u;
        }
        if (upload) {
            s->pending_transfer_requires_materialization = !s->materialize_vram &&
                                                          hm_scene_has_writes(s);
            if (s->pending_transfer_requires_materialization)
                s->needs_materialization = 1;
        }
        s->pending_transfer_count = 0;
        u32 color = ((w[0] & 255u) >> 3) | (((w[0] >> 8) & 255u) >> 3) << 5 |
                    (((w[0] >> 16) & 255u) >> 3) << 10;
        u32 canonical_op = copy ? 0x80u : upload ? 0xa0u : 2u;
        hm_scene_transfer(s, canonical_op, color, x, y, width, height, source_x, source_y,
                          source_layer);
        return;
    }
    if (after)
        return;
    if (op >= 0x20 && op <= 0x3f)
        hm_scene_polygon(s, w, available);
    else if (op >= 0x60 && op <= 0x7f)
        hm_scene_rectangle(s, w, available);
    else if (op >= 0x40 && op <= 0x5f)
        hm_scene_line(s, w, available);
    else if (!hm_scene_nonrendering_opcode(op) && !(op >= 0xe1 && op <= 0xe6))
        s->failure = "unsupported GP0 opcode";
}
hm_gpu_scene *hm_gpu_scene_create(void) {
    hm_gpu_scene *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->bin_stride = HM_SCENE_BIN_WORDS;
    if (!hm_scene_reserve_primitives(s, HM_SCENE_INITIAL_PRIMITIVES)) {
        hm_gpu_scene_destroy(s);
        return NULL;
    }
    return s;
}
void hm_gpu_scene_destroy(hm_gpu_scene *s) {
    if (!s)
        return;
    free(s->primitives);
    free(s->bins);
    free(s->packed);
    free(s->vram_versions);
    free(s->vram_version_hashes);
    free(s->vram_version_tiles);
    free(s);
}
void hm_gpu_scene_texture_filter(hm_gpu_scene *s, int filter_mode) {
    s->texture_filter = filter_mode >= 0 && filter_mode <= 2 ? filter_mode : 0;
}
void hm_gpu_scene_multisamples(hm_gpu_scene *s, unsigned samples) {
    s->multisamples = samples == 2 || samples == 4 ? samples : 1;
}
void hm_gpu_scene_materialize_vram(hm_gpu_scene *s, int enabled) {
    if (s)
        s->materialize_vram = enabled != 0;
}
int hm_gpu_scene_needs_materialization(const hm_gpu_scene *s) {
    return s && s->needs_materialization;
}
u32 hm_gpu_scene_materialization_mask(const hm_gpu_scene *s,
                                     u64 tiles[HM_VRAM_DIRTY_ROWS]) {
    if (!tiles)
        return 0;
    if (!s) {
        memset(tiles, 0, sizeof(u64) * HM_VRAM_DIRTY_ROWS);
        return 0;
    }
    /* Record dependencies in command order: a write only requires CPU
     * materialization when a later texture/copy read samples that tile. */
    u32 count = 0;
    for (u32 y = 0; y < HM_VRAM_DIRTY_ROWS; ++y) {
        tiles[y] = s->materialization_tiles[y];
        u64 row = tiles[y];
        while (row) {
            row &= row - 1;
            ++count;
        }
    }
    return count;
}
static u32 hm_scene_consume(void *context, const u32 *w, u32 available) {
    hm_gpu_scene *s = context;
    if (s->materialize_vram)
        return 0;
    if (!available)
        return 0;
    u32 op = w[0] >> 24, length = 0;
    if (op >= 0x20 && op <= 0x3f) {
        u32 n = op & 8 ? 4 : 3;
        length = 1 + n + ((op & 4) ? n : 0) + ((op & 16) ? n - 1 : 0);
    } else if (op >= 0x60 && op <= 0x7f)
        length = 2 + ((op & 4) ? 1 : 0) + (((op >> 3) & 3) == 0);
    else if (op >= 0x40 && op <= 0x5f) {
        u32 at = 2;
        int shaded = (op & 16) != 0, poly = (op & 8) != 0, skip = 0;
        while (at < available) {
            if (poly && skip && (w[at] & 0xf000f000u) == 0x50005000u) {
                length = at + 1;
                break;
            }
            if (shaded)
                ++at;
            if (at >= available)
                break;
            ++at;
            if (!poly) {
                length = at;
                break;
            }
            skip = 1;
        }
    }
    if (!length || length > available)
        return 0;
    hm_scene_command(context, w, length, 0);
    s->motion_previous = s->motion_current = NULL;
    s->motion_count = 0;
    if (op >= 0x20 && op <= 0x3f && (op & 4)) {
        const DRAWENV *draw = &hm_gpu_environment_state()->draw;
        u32 page = w[(op & 16) ? 5 : 4] >> 16;
        hm_gpu_environment_command(0xe1000000u | (page & 511) | ((u32)draw->dtd << 9) |
                                   ((u32)draw->dfe << 10));
    }
    return length;
}
static void hm_scene_motion(void *context, const u32 *previous, const u32 *current, u32 count,
                            u32 alpha) {
    hm_gpu_scene *s = context;
    if (!previous || !current || !count || alpha > 65536) {
        previous = current = NULL;
        count = 0;
        alpha = 0;
    }
    s->motion_previous = previous;
    s->motion_current = current;
    s->motion_count = count;
    s->motion_alpha = alpha;
}
hm_replay_sink hm_gpu_scene_begin(hm_gpu_scene *s, u32 width, u32 height) {
    s->width = width;
    s->height = height;
    s->count = 0;
    s->pending_transfer_count = 0;
    s->pending_transfer_requires_materialization = 0;
    s->vram_version_count = 0;
    s->vram_dirty_tracking = 0;
    s->needs_materialization = 0;
    memset(s->written_tiles, 0, sizeof(s->written_tiles));
    memset(s->current_texture_tiles, 0, sizeof(s->current_texture_tiles));
    memset(s->materialization_tiles, 0, sizeof(s->materialization_tiles));
    s->failure = NULL;
    s->begun = 0;
    s->vram_pixels = NULL;
    s->motion_previous = s->motion_current = NULL;
    s->motion_count = 0;
    return hm_replay_bind((hm_replay_sink){s, hm_scene_begin_capture, hm_scene_command,
                                           hm_scene_consume, hm_scene_motion});
}
int hm_gpu_scene_end(hm_gpu_scene *s, hm_replay_sink previous, hm_gpu_scene_data *data) {
    hm_replay_bind(previous);
    if (!s->begun || s->failure || !s->vram_pixels)
        return 0;
    size_t primitives = HM_SCENE_HEADER;
    size_t primitive_words = (size_t)s->count * HM_SCENE_STRIDE;
    if ((s->count && primitive_words / HM_SCENE_STRIDE != (size_t)s->count) ||
        primitive_words > SIZE_MAX - primitives) {
        s->failure = "scene size overflow";
        return 0;
    }
    size_t tile_count = (size_t)s->tiles_x * s->tiles_y;
    if (!s->bin_stride || tile_count > UINT32_MAX || primitives > SIZE_MAX - primitive_words) {
        s->failure = "scene size overflow";
        return 0;
    }
    size_t bin_offset = primitives + primitive_words;
    size_t list_words = 0;
    u32 max_bin_count = 0;
    u32 empty_bin_count = 0;
    u32 shared_cache_bin_count = 0;
    u64 total_bin_references = 0;
    int shared_cache_scene = s->area.w > 0 && s->area.h > 0 &&
                            s->width == (u32)s->area.w && s->height == (u32)s->area.h &&
                            (s->width & (HM_SCENE_BIN_TILE_SIZE - 1)) == 0 &&
                            (s->height & (HM_SCENE_BIN_TILE_SIZE - 1)) == 0;
    for (size_t i = 0; i < tile_count; ++i) {
        u32 count = s->bins[i * s->bin_stride];
        if (list_words > SIZE_MAX - (size_t)count - 1) {
            s->failure = "scene size overflow";
            return 0;
        }
        list_words += (size_t)count + 1;
        if (count > max_bin_count)
            max_bin_count = count;
        if (count == 0)
            ++empty_bin_count;
        else if (shared_cache_scene && count <= HM_SCENE_GROUP_PRIMITIVE_CAPACITY)
            ++shared_cache_bin_count;
        total_bin_references += count;
    }
    if (tile_count > SIZE_MAX - bin_offset || list_words > SIZE_MAX - bin_offset - tile_count) {
        s->failure = "scene size overflow";
        return 0;
    }
    size_t list_offset = bin_offset + tile_count;
    size_t total = list_offset + list_words;
    if (total > UINT32_MAX || !hm_scene_reserve_packed(s, total)) {
        s->failure = "scene allocation";
        return 0;
    }
    memset(s->packed, 0, HM_SCENE_HEADER * sizeof(u32));
    s->packed[0] = s->width;
    s->packed[1] = s->height;
    s->packed[2] = s->area.x;
    s->packed[3] = s->area.y;
    s->packed[4] = s->area.w;
    s->packed[5] = s->area.h;
    s->packed[6] = s->tiles_x;
    s->packed[7] = (u32)tile_count;
    s->packed[8] = (u32)primitives;
    s->packed[9] = (u32)bin_offset;
    s->packed[10] = s->count;
    s->packed[11] = (u32)s->texture_filter;
    s->packed[12] = s->native.x;
    s->packed[13] = s->native.w;
    s->packed[14] = s->background;
    s->packed[15] = s->multisamples == 2 || s->multisamples == 4 ? s->multisamples : 1;
    memcpy(s->packed + primitives, s->primitives,
           (size_t)s->count * HM_SCENE_STRIDE * sizeof(u32));
    /* Store a tile-offset table followed by compact [count, primitive ids]
     * lists. Sparse farm tiles no longer reserve 256 unused IDs apiece. */
    size_t cursor = list_offset;
    for (size_t i = 0; i < tile_count; ++i) {
        const u32 *bin = s->bins + i * s->bin_stride;
        u32 count = bin[0];
        s->packed[bin_offset + i] = (u32)cursor;
        s->packed[cursor++] = count;
        if (count) {
            memcpy(s->packed + cursor, bin + 1, (size_t)count * sizeof(u32));
            cursor += count;
        }
    }
    *data = (hm_gpu_scene_data){.words = s->packed,
                                .bytes = total * sizeof(u32),
                                .vram_pixels = s->vram_pixels,
                                .vram_versions = s->vram_versions,
                                .vram_version_count = s->vram_version_count,
                                .primitive_count = s->count,
                                .tile_count = (u32)tile_count,
                                .max_bin_count = max_bin_count,
                                .empty_bin_count = empty_bin_count,
                                .shared_cache_bin_count = shared_cache_bin_count,
                                .total_bin_references = total_bin_references,
                                .revision = ++s->revision,
                                .width = s->width,
                                .height = s->height};
    return 1;
}
int hm_gpu_scene_scanout(hm_gpu_scene *s, const RECT *area, u32 width, u32 height,
                         hm_gpu_scene_data *data) {
    if (!s || !area || !data)
        return 0;
    hm_gpu_environment scanout_environment = *hm_gpu_environment_state();
    scanout_environment.draw.clip = *area;
    hm_gpu_environment *previous_environment = hm_gpu_environment_bind(&scanout_environment);
    hm_replay_sink previous = hm_gpu_scene_begin(s, width, height);
    hm_replay_begin(area);
    int ready = hm_gpu_scene_end(s, previous, data);
    hm_gpu_environment_bind(previous_environment);
    return ready;
}
const char *hm_gpu_scene_fallback(const hm_gpu_scene *s) {
    return s->failure ? s->failure : "missing frame history";
}
