#ifndef KAIN_RUNTIME_REALTIME_H
#define KAIN_RUNTIME_REALTIME_H

#include "kain_runtime_win32.h"

#ifdef _WIN32
#define KAIN_RUNTIME_REALTIME_ENV "KAIN_REALTIME_APP_BUNDLE"
#define KAIN_RUNTIME_REALTIME_SIDECAR_SUFFIX ".realtime_app.json"
#define KAIN_RUNTIME_REALTIME_MAX_TARGET 32
#define KAIN_RUNTIME_REALTIME_MAX_ORIGIN 32
#define KAIN_RUNTIME_REALTIME_MAX_PATH 512
#define KAIN_RUNTIME_REALTIME_MAX_NODE 96
#define KAIN_RUNTIME_REALTIME_MAX_TAG 96
#define KAIN_RUNTIME_REALTIME_MAX_TITLE 160
#define KAIN_RUNTIME_REALTIME_MAX_INLINE 256

typedef struct {
    int loaded;
    int valid_for_native_viewport;
    int scene_count;
    int material_count;
    int shader_ref_count;
    char target[KAIN_RUNTIME_REALTIME_MAX_TARGET];
    char load_origin[KAIN_RUNTIME_REALTIME_MAX_ORIGIN];
    char source_path[KAIN_RUNTIME_REALTIME_MAX_PATH];
    char primary_viewport_node[KAIN_RUNTIME_REALTIME_MAX_NODE];
    char primary_scene[KAIN_RUNTIME_REALTIME_MAX_TAG];
    char primary_title[KAIN_RUNTIME_REALTIME_MAX_TITLE];
    char primary_material_refs[KAIN_RUNTIME_REALTIME_MAX_INLINE];
    char primary_shader_ref_keys[KAIN_RUNTIME_REALTIME_MAX_INLINE];
} KainRuntimeRealtimeBundle;

void kain_runtime_realtime_init(KainRuntimeRealtimeBundle* bundle);
int kain_runtime_realtime_load_from_json(const char* json, KainRuntimeRealtimeBundle* bundle);
int kain_runtime_realtime_load_from_path(const char* path, KainRuntimeRealtimeBundle* bundle);
int kain_runtime_realtime_load_from_env(const char* env_name, KainRuntimeRealtimeBundle* bundle);
int kain_runtime_realtime_load_for_current_process(
    const char* env_name,
    KainRuntimeRealtimeBundle* bundle
);
#endif

#endif
