#ifndef HMBTN_GPU_SCENE_H
#define HMBTN_GPU_SCENE_H
#include "gfx/replay_sink.h"
#include "psx/vram.h"
#include <stddef.h>
enum {
    HM_SCENE_HEADER = 16,
    HM_SCENE_STRIDE = 41,
    /* Smaller bins cut per-pixel candidate loops in dense game scenes. */
    HM_SCENE_BIN_TILE_SIZE = 8,
    HM_SCENE_GROUP_PRIMITIVE_CAPACITY = 16,
    HM_SCENE_INITIAL_PRIMITIVES = 8192,
    HM_SCENE_BIN_WORDS = 257
};
typedef struct hm_gpu_scene hm_gpu_scene;
typedef struct {
    const u32 *words;
    size_t bytes;
    const u16 *vram_pixels; /* Builder-owned snapshot; valid until its next begin or destruction. */
    const u16 *vram_versions; /* Additional packed VRAM snapshots, one 1024x512 layer each. */
    u32 vram_version_count;
    u32 primitive_count, tile_count, max_bin_count;
    u32 empty_bin_count, shared_cache_bin_count;
    u64 total_bin_references;
    u64 revision;
    u32 width, height;
} hm_gpu_scene_data;
hm_gpu_scene *hm_gpu_scene_create(void);
void hm_gpu_scene_destroy(hm_gpu_scene *scene);
void hm_gpu_scene_texture_filter(hm_gpu_scene *scene, int filter_mode);
void hm_gpu_scene_multisamples(hm_gpu_scene *scene, unsigned samples);
void hm_gpu_scene_materialize_vram(hm_gpu_scene *scene, int enabled);
int hm_gpu_scene_needs_materialization(const hm_gpu_scene *scene);
u32 hm_gpu_scene_materialization_mask(const hm_gpu_scene *scene,
                                      u64 tiles[HM_VRAM_DIRTY_ROWS]);
hm_replay_sink hm_gpu_scene_begin(hm_gpu_scene *scene, u32 width, u32 height);
int hm_gpu_scene_end(hm_gpu_scene *scene, hm_replay_sink previous, hm_gpu_scene_data *data);
/* Build an empty GPU scene that scans the selected visible area from live VRAM. */
int hm_gpu_scene_scanout(hm_gpu_scene *scene, const RECT *area, u32 width, u32 height,
                         hm_gpu_scene_data *data);
const char *hm_gpu_scene_fallback(const hm_gpu_scene *scene);
#endif
