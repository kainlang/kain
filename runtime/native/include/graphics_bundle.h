#ifndef GRAPHICS_BUNDLE_H
#define GRAPHICS_BUNDLE_H

#include <stddef.h>
#include "win32.h"

#define GRAPHICS_BUNDLE_ENV "KAIN_REALTIME_APP_BUNDLE"
#define GRAPHICS_BUNDLE_SIDECAR_SUFFIX ".realtime_app.json"
#define GRAPHICS_BUNDLE_MAX_TARGET 32
#define GRAPHICS_BUNDLE_MAX_ORIGIN 32
#define GRAPHICS_BUNDLE_MAX_PATH 512
#define GRAPHICS_BUNDLE_MAX_NODE 96
#define GRAPHICS_BUNDLE_MAX_TAG 96
#define GRAPHICS_BUNDLE_MAX_TITLE 160
#define GRAPHICS_BUNDLE_MAX_INLINE 256
#define GRAPHICS_BUNDLE_MAX_SUMMARY 256
#define GRAPHICS_BUNDLE_MAX_BINDINGS 8
#define GRAPHICS_BUNDLE_MAX_RENDER_PASSES 8
#define GRAPHICS_BUNDLE_MAX_RENDER_ATTACHMENTS 12
#define GRAPHICS_BUNDLE_MAX_RENDER_DEPENDENCIES 12
#define GRAPHICS_BUNDLE_MAX_RESIDENCY_RESOURCES 16
#define GRAPHICS_BUNDLE_MAX_SCHEDULE_STEPS 8
#define GRAPHICS_BUNDLE_MAX_SCHEDULE_BARRIERS 12
#define KAIN_COMPUTE_RESIDENCY_ENV "KAIN_COMPUTE_RESIDENCY"
#define KAIN_GPU_RUNTIME_LIBRARY_ENV "KAIN_GPU_RUNTIME_LIBRARY"
#define KAIN_GPU_RUNTIME_WINDOWS_DLL "kain_gpu_runtime.dll"

typedef struct {
    char key[GRAPHICS_BUNDLE_MAX_TAG];
    char resource_type[GRAPHICS_BUNDLE_MAX_TAG];
    char stage[GRAPHICS_BUNDLE_MAX_TAG];
    char access[GRAPHICS_BUNDLE_MAX_TAG];
    int slot;
} KainRuntimeGraphicsBinding;

typedef struct {
    int loaded;
    char material_id[GRAPHICS_BUNDLE_MAX_TAG];
    char source[GRAPHICS_BUNDLE_MAX_TAG];
    int shader_ref_count;
    int parameter_count;
    int resource_binding_count;
    KainRuntimeGraphicsBinding resource_bindings[GRAPHICS_BUNDLE_MAX_BINDINGS];
} KainRuntimeGraphicsMaterialPlan;

typedef struct {
    int loaded;
    char shader_key[GRAPHICS_BUNDLE_MAX_TAG];
    char module_name[GRAPHICS_BUNDLE_MAX_TAG];
    char entry_point[GRAPHICS_BUNDLE_MAX_TAG];
    char execution_domain[GRAPHICS_BUNDLE_MAX_TAG];
    int workgroup_size[3];
    int dispatch_size[3];
    int resource_binding_count;
    int tensor_binding_count;
    int stream_binding_count;
    int neural_node_count;
    KainRuntimeGraphicsBinding resource_bindings[GRAPHICS_BUNDLE_MAX_BINDINGS];
} KainRuntimeGraphicsComputePlan;

typedef enum {
    GRAPHICS_BUNDLE_PASS_UNKNOWN = 0,
    GRAPHICS_BUNDLE_PASS_RENDER,
    GRAPHICS_BUNDLE_PASS_COMPUTE,
    GRAPHICS_BUNDLE_PASS_PRESENT,
    GRAPHICS_BUNDLE_PASS_TRANSFER,
} KainRuntimeGraphicsPassKind;

typedef enum {
    GRAPHICS_BUNDLE_ATTACHMENT_UNKNOWN = 0,
    GRAPHICS_BUNDLE_ATTACHMENT_COLOR,
    GRAPHICS_BUNDLE_ATTACHMENT_DEPTH,
    GRAPHICS_BUNDLE_ATTACHMENT_STORAGE,
    GRAPHICS_BUNDLE_ATTACHMENT_SWAPCHAIN,
} KainRuntimeGraphicsAttachmentKind;

typedef enum {
    GRAPHICS_BUNDLE_LIFETIME_UNKNOWN = 0,
    GRAPHICS_BUNDLE_LIFETIME_IMPORTED,
    GRAPHICS_BUNDLE_LIFETIME_FRAME_TRANSIENT,
    GRAPHICS_BUNDLE_LIFETIME_PERSISTENT,
} KainRuntimeGraphicsLifetimeKind;

typedef enum {
    GRAPHICS_BUNDLE_RESIDENCY_UNKNOWN = 0,
    GRAPHICS_BUNDLE_RESIDENCY_GPU_ONLY,
    GRAPHICS_BUNDLE_RESIDENCY_CPU_TO_GPU,
    GRAPHICS_BUNDLE_RESIDENCY_READBACK,
    GRAPHICS_BUNDLE_RESIDENCY_TRANSIENT_POOL,
} KainRuntimeGraphicsResidencyKind;

typedef enum {
    GRAPHICS_BUNDLE_QUEUE_UNKNOWN = 0,
    GRAPHICS_BUNDLE_QUEUE_GRAPHICS,
    GRAPHICS_BUNDLE_QUEUE_COMPUTE,
    GRAPHICS_BUNDLE_QUEUE_TRANSFER,
    GRAPHICS_BUNDLE_QUEUE_PRESENT,
} KainRuntimeGraphicsQueueKind;

typedef enum {
    GRAPHICS_BUNDLE_BARRIER_UNKNOWN = 0,
    GRAPHICS_BUNDLE_BARRIER_EXECUTION,
    GRAPHICS_BUNDLE_BARRIER_BUFFER,
    GRAPHICS_BUNDLE_BARRIER_TEXTURE,
} KainRuntimeGraphicsBarrierKind;

typedef struct {
    int loaded;
    KainRuntimeGraphicsAttachmentKind kind;
    KainRuntimeGraphicsLifetimeKind lifetime;
    int transient_attachment;
    char key[GRAPHICS_BUNDLE_MAX_TAG];
    char format[GRAPHICS_BUNDLE_MAX_TAG];
    char producer_pass[GRAPHICS_BUNDLE_MAX_TAG];
    char consumer_passes[GRAPHICS_BUNDLE_MAX_INLINE];
    int consumer_count;
} KainRuntimeGraphicsAttachmentDescriptor;

typedef struct {
    int loaded;
    KainRuntimeGraphicsPassKind kind;
    KainRuntimeGraphicsQueueKind queue;
    int async_capable;
    int read_count;
    int write_count;
    char key[GRAPHICS_BUNDLE_MAX_TAG];
    char label[GRAPHICS_BUNDLE_MAX_TITLE];
    char reads[GRAPHICS_BUNDLE_MAX_INLINE];
    char writes[GRAPHICS_BUNDLE_MAX_INLINE];
    char capture_hook[GRAPHICS_BUNDLE_MAX_TAG];
} KainRuntimeGraphicsRenderPassDescriptor;

typedef struct {
    int loaded;
    KainRuntimeGraphicsBarrierKind barrier_kind;
    char from_pass[GRAPHICS_BUNDLE_MAX_TAG];
    char to_pass[GRAPHICS_BUNDLE_MAX_TAG];
    char reason[GRAPHICS_BUNDLE_MAX_TITLE];
} KainRuntimeGraphicsRenderDependencyDescriptor;

typedef struct {
    int loaded;
    int synthesized_from_bundle;
    int pass_count;
    int dependency_count;
    int attachment_count;
    int capture_hook_count;
    char primary_pass_key[GRAPHICS_BUNDLE_MAX_TAG];
    KainRuntimeGraphicsRenderPassDescriptor passes[GRAPHICS_BUNDLE_MAX_RENDER_PASSES];
    KainRuntimeGraphicsAttachmentDescriptor attachments[GRAPHICS_BUNDLE_MAX_RENDER_ATTACHMENTS];
    KainRuntimeGraphicsRenderDependencyDescriptor dependencies[GRAPHICS_BUNDLE_MAX_RENDER_DEPENDENCIES];
} KainRuntimeGraphicsRenderGraphContract;

typedef struct {
    int loaded;
    KainRuntimeGraphicsResidencyKind residency_kind;
    int transient_resource;
    int gpu_resident;
    int cpu_visible;
    int slot;
    unsigned long long byte_length;
    char key[GRAPHICS_BUNDLE_MAX_TAG];
    char descriptor_kind[GRAPHICS_BUNDLE_MAX_TAG];
    char access_mode[GRAPHICS_BUNDLE_MAX_TAG];
    char residency_role[GRAPHICS_BUNDLE_MAX_TAG];
    char stage[GRAPHICS_BUNDLE_MAX_TAG];
} KainRuntimeGraphicsResidencyResourceDescriptor;

typedef struct {
    int loaded;
    int synthesized_from_bundle;
    int resource_count;
    int transient_pool_count;
    int async_stream_count;
    unsigned long long estimated_bytes;
    unsigned long long transient_pool_bytes;
    KainRuntimeGraphicsResidencyResourceDescriptor
        resources[GRAPHICS_BUNDLE_MAX_RESIDENCY_RESOURCES];
} KainRuntimeGraphicsResidencyContract;

typedef struct {
    int loaded;
    KainRuntimeGraphicsQueueKind queue;
    int async_capable;
    int resource_count;
    int dispatch_size[3];
    int workgroup_size[3];
    char key[GRAPHICS_BUNDLE_MAX_TAG];
    char label[GRAPHICS_BUNDLE_MAX_TITLE];
    char shader_key[GRAPHICS_BUNDLE_MAX_TAG];
    char resource_keys[GRAPHICS_BUNDLE_MAX_INLINE];
} KainRuntimeGraphicsScheduleStepDescriptor;

typedef struct {
    int loaded;
    KainRuntimeGraphicsBarrierKind barrier_kind;
    char key[GRAPHICS_BUNDLE_MAX_TAG];
    char from_step[GRAPHICS_BUNDLE_MAX_TAG];
    char to_step[GRAPHICS_BUNDLE_MAX_TAG];
    char resource_key[GRAPHICS_BUNDLE_MAX_TAG];
    char reason[GRAPHICS_BUNDLE_MAX_TITLE];
} KainRuntimeGraphicsScheduleBarrierDescriptor;

typedef struct {
    int loaded;
    int synthesized_from_bundle;
    int step_count;
    int barrier_count;
    int queue_count;
    int async_step_count;
    char primary_step_key[GRAPHICS_BUNDLE_MAX_TAG];
    KainRuntimeGraphicsScheduleStepDescriptor steps[GRAPHICS_BUNDLE_MAX_SCHEDULE_STEPS];
    KainRuntimeGraphicsScheduleBarrierDescriptor
        barriers[GRAPHICS_BUNDLE_MAX_SCHEDULE_BARRIERS];
} KainRuntimeGraphicsComputeSchedule;

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
    KainRuntimeGraphicsRenderGraphContract render_graph;
    KainRuntimeGraphicsResidencyContract residency;
    KainRuntimeGraphicsComputeSchedule primary_schedule;
    char target[GRAPHICS_BUNDLE_MAX_TARGET];
    char load_origin[GRAPHICS_BUNDLE_MAX_ORIGIN];
    char source_path[GRAPHICS_BUNDLE_MAX_PATH];
    char primary_viewport_node[GRAPHICS_BUNDLE_MAX_NODE];
    char primary_viewport_kind[GRAPHICS_BUNDLE_MAX_TAG];
    char primary_scene[GRAPHICS_BUNDLE_MAX_TAG];
    char primary_title[GRAPHICS_BUNDLE_MAX_TITLE];
    char primary_material_refs[GRAPHICS_BUNDLE_MAX_INLINE];
    char primary_shader_ref_keys[GRAPHICS_BUNDLE_MAX_INLINE];
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
    int graphics_lane_ready;
    int compute_metadata_valid;
    int tensor_metadata_valid;
    int stream_metadata_valid;
    int neural_metadata_valid;
    int has_render_graph_contract;
    int render_graph_valid;
    int has_residency_contract;
    int residency_valid;
    int has_compute_schedule_contract;
    int compute_schedule_valid;
    char summary[GRAPHICS_BUNDLE_MAX_SUMMARY];
    char reason[GRAPHICS_BUNDLE_MAX_SUMMARY];
} KainRuntimeGraphicsValidation;

typedef struct {
    int executed;
    unsigned long long dispatch_invocations;
    unsigned long long accumulated_invocations;
    double phase;
    double throughput;
    int tensor_binding_count;
    int stream_binding_count;
    int neural_node_count;
    int schedule_step_count;
    int schedule_barrier_count;
    char schedule_key[GRAPHICS_BUNDLE_MAX_TAG];
    char summary[GRAPHICS_BUNDLE_MAX_SUMMARY];
} KainRuntimeGraphicsExecutionState;

typedef struct {
    const char* shader_bundle_path;
    const char* compute_residency_path;
    const char* compute_key;
    unsigned int dispatch_size[3];
    /// Barrier metadata JSON for precise pipeline barriers.
    /// NULL = use full pipeline drain fallback.
    const char* barrier_json;
} KainGpuRuntimeDispatchRequest;

typedef struct {
    int status_code;
    unsigned long long dispatch_invocations;
    unsigned int tensor_binding_count;
    unsigned int stream_binding_count;
    unsigned int neural_node_count;
    unsigned int output_binding_count;
    unsigned long long total_output_bytes;
    unsigned int barrier_count;
    unsigned int async_queue_used;
    char message[GRAPHICS_BUNDLE_MAX_SUMMARY];
} KainGpuRuntimeDispatchResult;

typedef void* (*KainGpuRuntimeCreateFn)(const void* config);
typedef int (*KainGpuRuntimeDispatchFn)(
    void* handle,
    const KainGpuRuntimeDispatchRequest* request,
    KainGpuRuntimeDispatchResult* result
);
typedef void (*KainGpuRuntimeDestroyFn)(void* handle);

void graphics_bundle_render_graph_init(KainRuntimeGraphicsRenderGraphContract* contract);
void graphics_bundle_residency_init(KainRuntimeGraphicsResidencyContract* contract);
void graphics_bundle_compute_schedule_init(KainRuntimeGraphicsComputeSchedule* schedule);
int graphics_bundle_render_graph_is_valid(
    const KainRuntimeGraphicsRenderGraphContract* contract
);
int graphics_bundle_residency_is_valid(
    const KainRuntimeGraphicsResidencyContract* contract
);
int graphics_bundle_compute_schedule_is_valid(
    const KainRuntimeGraphicsComputeSchedule* schedule
);
void graphics_bundle_init(KainRuntimeGraphicsBundle* bundle);
int graphics_bundle_load_from_json(const char* json, KainRuntimeGraphicsBundle* bundle);
int graphics_bundle_load_from_path(const char* path, KainRuntimeGraphicsBundle* bundle);
int graphics_bundle_load_from_env(const char* env_name, KainRuntimeGraphicsBundle* bundle);
int graphics_bundle_load_for_current_process(
    const char* env_name,
    KainRuntimeGraphicsBundle* bundle
);
void graphics_bundle_validation_init(KainRuntimeGraphicsValidation* validation);
int graphics_bundle_validate_bundle(
    const KainRuntimeGraphicsBundle* bundle,
    KainRuntimeGraphicsValidation* validation
);
void graphics_bundle_format_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
);
void graphics_bundle_format_contract_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
);
void graphics_bundle_execution_state_init(KainRuntimeGraphicsExecutionState* state);
int graphics_bundle_execute_primary_compute(
    const KainRuntimeGraphicsBundle* bundle,
    double frame_delta,
    double total_time,
    KainRuntimeGraphicsExecutionState* state
);
int viewport_supports_graphics_bundle(const KainRuntimeGraphicsBundle* bundle);

#endif
