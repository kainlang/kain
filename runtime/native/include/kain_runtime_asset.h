#ifndef KAIN_RUNTIME_ASSET_H
#define KAIN_RUNTIME_ASSET_H

#include "kain_runtime_scene.h"
#include "kain_runtime_win32.h"

#ifdef _WIN32
#define KAIN_NATIVE_WORLD_ASSET_ENV "KAIN_NATIVE_WORLD_ASSET"
#define KAIN_NATIVE_WORLD_TARGET_EXTENT_ENV "KAIN_NATIVE_WORLD_TARGET_EXTENT"
#define KAIN_NATIVE_WORLD_SCALE_ENV "KAIN_NATIVE_WORLD_SCALE"
#define KAIN_NATIVE_WORLD_SKIP_SKY_ENV "KAIN_NATIVE_WORLD_SKIP_SKY"

#define KAIN_NATIVE_WORLD_MAX_PATH 512
#define KAIN_NATIVE_WORLD_MAX_LABEL 160

typedef enum {
    KAIN_RUNTIME_INGESTION_SOURCE_UNKNOWN = 0,
    KAIN_RUNTIME_INGESTION_SOURCE_EXPLICIT_PATH,
    KAIN_RUNTIME_INGESTION_SOURCE_ENVIRONMENT,
    KAIN_RUNTIME_INGESTION_SOURCE_CURRENT_PROCESS_SIDECAR,
    KAIN_RUNTIME_INGESTION_SOURCE_HOST_STAGED,
    KAIN_RUNTIME_INGESTION_SOURCE_COMPILER_EMITTED,
} KainRuntimeIngestionSourceKind;

typedef enum {
    KAIN_RUNTIME_INGESTION_PAYLOAD_UNKNOWN = 0,
    KAIN_RUNTIME_INGESTION_PAYLOAD_SCENE_ASSET,
    KAIN_RUNTIME_INGESTION_PAYLOAD_RUNTIME_CONTRACT,
    KAIN_RUNTIME_INGESTION_PAYLOAD_REFLECTION_PAYLOAD,
    KAIN_RUNTIME_INGESTION_PAYLOAD_REALTIME_BUNDLE,
    KAIN_RUNTIME_INGESTION_PAYLOAD_GRAPHICS_BUNDLE,
    KAIN_RUNTIME_INGESTION_PAYLOAD_UI_BUNDLE,
} KainRuntimeIngestionPayloadKind;

typedef struct {
    int declared;
    KainRuntimeIngestionSourceKind source_kind;
    KainRuntimeIngestionPayloadKind payload_kind;
    KainSceneHandle target_scene;
    KainSceneResourceKind target_kind;
    unsigned int flags;
    char source_path[KAIN_NATIVE_WORLD_MAX_PATH];
    char logical_name[KAIN_NATIVE_WORLD_MAX_LABEL];
    char source_env[KAIN_NATIVE_WORLD_MAX_LABEL];
    char detail[KAIN_RUNTIME_SCENE_MESSAGE_MAX];
} KainRuntimeIngestionDescriptor;

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
void kain_runtime_ingestion_descriptor_init(KainRuntimeIngestionDescriptor* descriptor);
void kain_runtime_ingestion_descriptor_from_path(
    KainRuntimeIngestionDescriptor* descriptor,
    KainRuntimeIngestionPayloadKind payload_kind,
    KainRuntimeIngestionSourceKind source_kind,
    const char* source_path,
    const char* logical_name
);
void kain_native_scene_asset_describe_ingestion(
    const KainNativeSceneAsset* asset,
    KainRuntimeIngestionDescriptor* descriptor
);
#endif

#endif
