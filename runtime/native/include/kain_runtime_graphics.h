#ifndef KAIN_RUNTIME_GRAPHICS_H
#define KAIN_RUNTIME_GRAPHICS_H

#include "kain_runtime_win32.h"

#ifdef _WIN32
#define KAIN_RUNTIME_GRAPHICS_ENV "KAIN_REALTIME_APP_BUNDLE"
#define KAIN_RUNTIME_GRAPHICS_SIDECAR_SUFFIX ".realtime_app.json"
#define KAIN_RUNTIME_GRAPHICS_MAX_TARGET 32
#define KAIN_RUNTIME_GRAPHICS_MAX_ORIGIN 32
#define KAIN_RUNTIME_GRAPHICS_MAX_PATH 512
#define KAIN_RUNTIME_GRAPHICS_MAX_NODE 96
#define KAIN_RUNTIME_GRAPHICS_MAX_TAG 96
#define KAIN_RUNTIME_GRAPHICS_MAX_TITLE 160
#define KAIN_RUNTIME_GRAPHICS_MAX_INLINE 256
#define KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY 256
#define KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS 8

typedef struct {
    char key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char resource_type[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char stage[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char access[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    int slot;
} KainRuntimeGraphicsBinding;

typedef struct {
    int loaded;
    char material_id[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char source[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    int shader_ref_count;
    int parameter_count;
    int resource_binding_count;
    KainRuntimeGraphicsBinding resource_bindings[KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS];
} KainRuntimeGraphicsMaterialPlan;

typedef struct {
    int loaded;
    char shader_key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char module_name[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char entry_point[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    int workgroup_size[3];
    int dispatch_size[3];
    int resource_binding_count;
    KainRuntimeGraphicsBinding resource_bindings[KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS];
} KainRuntimeGraphicsComputePlan;

typedef struct {
    int loaded;
    int schema_version;
    int scene_count;
    int material_count;
    int shader_bundle_ref_count;
    int shader_vertex_ref_count;
    int shader_fragment_ref_count;
    int shader_compute_ref_count;
    int asset_count;
    int tool_cap_count;
    int requirement_count;
    int material_shader_ref_key_count;
    int primary_material_ref_count;
    int primary_shader_ref_key_count;
    KainRuntimeGraphicsMaterialPlan primary_material;
    KainRuntimeGraphicsComputePlan primary_compute;
    char target[KAIN_RUNTIME_GRAPHICS_MAX_TARGET];
    char load_origin[KAIN_RUNTIME_GRAPHICS_MAX_ORIGIN];
    char source_path[KAIN_RUNTIME_GRAPHICS_MAX_PATH];
    char primary_viewport_node[KAIN_RUNTIME_GRAPHICS_MAX_NODE];
    char primary_viewport_kind[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char primary_scene[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char primary_title[KAIN_RUNTIME_GRAPHICS_MAX_TITLE];
    char primary_material_refs[KAIN_RUNTIME_GRAPHICS_MAX_INLINE];
    char primary_shader_ref_keys[KAIN_RUNTIME_GRAPHICS_MAX_INLINE];
} KainRuntimeGraphicsBundle;

typedef struct {
    int loaded;
    int target_is_llvm;
    int has_render_scene;
    int has_viewport3d;
    int has_material_bindings;
    int has_compute_artifacts;
    int material_binding_valid;
    int compute_plan_valid;
    int gl_lane_ready;
    int compute_metadata_valid;
    char summary[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];
    char reason[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];
} KainRuntimeGraphicsValidation;

void kain_runtime_graphics_init(KainRuntimeGraphicsBundle* bundle);
int kain_runtime_graphics_load_from_json(const char* json, KainRuntimeGraphicsBundle* bundle);
int kain_runtime_graphics_load_from_path(const char* path, KainRuntimeGraphicsBundle* bundle);
int kain_runtime_graphics_load_from_env(const char* env_name, KainRuntimeGraphicsBundle* bundle);
int kain_runtime_graphics_load_for_current_process(
    const char* env_name,
    KainRuntimeGraphicsBundle* bundle
);
void kain_runtime_graphics_validation_init(KainRuntimeGraphicsValidation* validation);
int kain_runtime_graphics_validate_bundle(
    const KainRuntimeGraphicsBundle* bundle,
    KainRuntimeGraphicsValidation* validation
);
void kain_runtime_graphics_format_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
);
int kain_win32_gl_surface_supports_graphics_bundle(const KainRuntimeGraphicsBundle* bundle);
#endif

#endif
