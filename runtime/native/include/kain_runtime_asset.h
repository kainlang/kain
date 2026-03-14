#ifndef KAIN_RUNTIME_ASSET_H
#define KAIN_RUNTIME_ASSET_H

#include "kain_runtime_win32.h"

#ifdef _WIN32
#define KAIN_NATIVE_WORLD_ASSET_ENV "KAIN_NATIVE_WORLD_ASSET"
#define KAIN_NATIVE_WORLD_TARGET_EXTENT_ENV "KAIN_NATIVE_WORLD_TARGET_EXTENT"
#define KAIN_NATIVE_WORLD_SCALE_ENV "KAIN_NATIVE_WORLD_SCALE"
#define KAIN_NATIVE_WORLD_SKIP_SKY_ENV "KAIN_NATIVE_WORLD_SKIP_SKY"

#define KAIN_NATIVE_WORLD_MAX_PATH 512
#define KAIN_NATIVE_WORLD_MAX_LABEL 160

typedef struct {
    int loaded;
    int used_fallback_colors;
    GLuint opaque_display_list;
    GLuint blend_display_list;
    char source_path[KAIN_NATIVE_WORLD_MAX_PATH];
    char asset_label[KAIN_NATIVE_WORLD_MAX_LABEL];
    double world_scale;
    KainVec3 raw_bounds_min;
    KainVec3 raw_bounds_max;
    KainVec3 world_bounds_min;
    KainVec3 world_bounds_max;
    KainVec3 raw_origin_offset;
    KainVec3 world_center;
    double ground_height;
    double recommended_spawn_distance;
    double recommended_far_clip;
    unsigned long long node_count;
    unsigned long long mesh_count;
    unsigned long long primitive_count;
    unsigned long long vertex_count;
    unsigned long long triangle_count;
} KainNativeSceneAsset;

void kain_native_scene_asset_init(KainNativeSceneAsset* asset);
void kain_native_scene_asset_shutdown(KainNativeSceneAsset* asset);
int kain_native_scene_asset_load_from_path(const char* path, KainNativeSceneAsset* asset);
int kain_native_scene_asset_load_from_env(const char* env_name, KainNativeSceneAsset* asset);
void kain_native_scene_asset_render(const KainNativeSceneAsset* asset);
#endif

#endif
