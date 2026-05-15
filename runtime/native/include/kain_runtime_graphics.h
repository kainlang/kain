#ifndef KAIN_RUNTIME_GRAPHICS_H
#define KAIN_RUNTIME_GRAPHICS_H

#include <stddef.h>
#include "kain_runtime_win32.h"

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
#define KAIN_RUNTIME_GRAPHICS_MAX_RENDER_PASSES 8
#define KAIN_RUNTIME_GRAPHICS_MAX_RENDER_ATTACHMENTS 12
#define KAIN_RUNTIME_GRAPHICS_MAX_RENDER_DEPENDENCIES 12
#define KAIN_RUNTIME_GRAPHICS_MAX_RESIDENCY_RESOURCES 16
#define KAIN_RUNTIME_GRAPHICS_MAX_SCHEDULE_STEPS 8
#define KAIN_RUNTIME_GRAPHICS_MAX_SCHEDULE_BARRIERS 12
#define KAIN_COMPUTE_RESIDENCY_ENV "KAIN_COMPUTE_RESIDENCY"
#define KAIN_GPU_RUNTIME_LIBRARY_ENV "KAIN_GPU_RUNTIME_LIBRARY"
#define KAIN_GPU_RUNTIME_WINDOWS_DLL "kain_gpu_runtime.dll"

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
    char execution_domain[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    int workgroup_size[3];
    int dispatch_size[3];
    int resource_binding_count;
    int tensor_binding_count;
    int stream_binding_count;
    int neural_node_count;
    KainRuntimeGraphicsBinding resource_bindings[KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS];
} KainRuntimeGraphicsComputePlan;

typedef enum {
    KAIN_RUNTIME_GRAPHICS_PASS_UNKNOWN = 0,
    KAIN_RUNTIME_GRAPHICS_PASS_RENDER,
    KAIN_RUNTIME_GRAPHICS_PASS_COMPUTE,
    KAIN_RUNTIME_GRAPHICS_PASS_PRESENT,
    KAIN_RUNTIME_GRAPHICS_PASS_TRANSFER,
} KainRuntimeGraphicsPassKind;

typedef enum {
    KAIN_RUNTIME_GRAPHICS_ATTACHMENT_UNKNOWN = 0,
    KAIN_RUNTIME_GRAPHICS_ATTACHMENT_COLOR,
    KAIN_RUNTIME_GRAPHICS_ATTACHMENT_DEPTH,
    KAIN_RUNTIME_GRAPHICS_ATTACHMENT_STORAGE,
    KAIN_RUNTIME_GRAPHICS_ATTACHMENT_SWAPCHAIN,
} KainRuntimeGraphicsAttachmentKind;

typedef enum {
    KAIN_RUNTIME_GRAPHICS_LIFETIME_UNKNOWN = 0,
    KAIN_RUNTIME_GRAPHICS_LIFETIME_IMPORTED,
    KAIN_RUNTIME_GRAPHICS_LIFETIME_FRAME_TRANSIENT,
    KAIN_RUNTIME_GRAPHICS_LIFETIME_PERSISTENT,
} KainRuntimeGraphicsLifetimeKind;

typedef enum {
    KAIN_RUNTIME_GRAPHICS_RESIDENCY_UNKNOWN = 0,
    KAIN_RUNTIME_GRAPHICS_RESIDENCY_GPU_ONLY,
    KAIN_RUNTIME_GRAPHICS_RESIDENCY_CPU_TO_GPU,
    KAIN_RUNTIME_GRAPHICS_RESIDENCY_READBACK,
    KAIN_RUNTIME_GRAPHICS_RESIDENCY_TRANSIENT_POOL,
} KainRuntimeGraphicsResidencyKind;

typedef enum {
    KAIN_RUNTIME_GRAPHICS_QUEUE_UNKNOWN = 0,
    KAIN_RUNTIME_GRAPHICS_QUEUE_GRAPHICS,
    KAIN_RUNTIME_GRAPHICS_QUEUE_COMPUTE,
    KAIN_RUNTIME_GRAPHICS_QUEUE_TRANSFER,
    KAIN_RUNTIME_GRAPHICS_QUEUE_PRESENT,
} KainRuntimeGraphicsQueueKind;

typedef enum {
    KAIN_RUNTIME_GRAPHICS_BARRIER_UNKNOWN = 0,
    KAIN_RUNTIME_GRAPHICS_BARRIER_EXECUTION,
    KAIN_RUNTIME_GRAPHICS_BARRIER_BUFFER,
    KAIN_RUNTIME_GRAPHICS_BARRIER_TEXTURE,
} KainRuntimeGraphicsBarrierKind;

typedef struct {
    int loaded;
    KainRuntimeGraphicsAttachmentKind kind;
    KainRuntimeGraphicsLifetimeKind lifetime;
    int transient_attachment;
    char key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char format[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char producer_pass[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char consumer_passes[KAIN_RUNTIME_GRAPHICS_MAX_INLINE];
    int consumer_count;
} KainRuntimeGraphicsAttachmentDescriptor;

typedef struct {
    int loaded;
    KainRuntimeGraphicsPassKind kind;
    KainRuntimeGraphicsQueueKind queue;
    int async_capable;
    int read_count;
    int write_count;
    char key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char label[KAIN_RUNTIME_GRAPHICS_MAX_TITLE];
    char reads[KAIN_RUNTIME_GRAPHICS_MAX_INLINE];
    char writes[KAIN_RUNTIME_GRAPHICS_MAX_INLINE];
    char capture_hook[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
} KainRuntimeGraphicsRenderPassDescriptor;

typedef struct {
    int loaded;
    KainRuntimeGraphicsBarrierKind barrier_kind;
    char from_pass[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char to_pass[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char reason[KAIN_RUNTIME_GRAPHICS_MAX_TITLE];
} KainRuntimeGraphicsRenderDependencyDescriptor;

typedef struct {
    int loaded;
    int synthesized_from_bundle;
    int pass_count;
    int dependency_count;
    int attachment_count;
    int capture_hook_count;
    char primary_pass_key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    KainRuntimeGraphicsRenderPassDescriptor passes[KAIN_RUNTIME_GRAPHICS_MAX_RENDER_PASSES];
    KainRuntimeGraphicsAttachmentDescriptor attachments[KAIN_RUNTIME_GRAPHICS_MAX_RENDER_ATTACHMENTS];
    KainRuntimeGraphicsRenderDependencyDescriptor dependencies[KAIN_RUNTIME_GRAPHICS_MAX_RENDER_DEPENDENCIES];
} KainRuntimeGraphicsRenderGraphContract;

typedef struct {
    int loaded;
    KainRuntimeGraphicsResidencyKind residency_kind;
    int transient_resource;
    int gpu_resident;
    int cpu_visible;
    int slot;
    unsigned long long byte_length;
    char key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char descriptor_kind[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char access_mode[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char residency_role[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char stage[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
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
        resources[KAIN_RUNTIME_GRAPHICS_MAX_RESIDENCY_RESOURCES];
} KainRuntimeGraphicsResidencyContract;

typedef struct {
    int loaded;
    KainRuntimeGraphicsQueueKind queue;
    int async_capable;
    int resource_count;
    int dispatch_size[3];
    int workgroup_size[3];
    char key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char label[KAIN_RUNTIME_GRAPHICS_MAX_TITLE];
    char shader_key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char resource_keys[KAIN_RUNTIME_GRAPHICS_MAX_INLINE];
} KainRuntimeGraphicsScheduleStepDescriptor;

typedef struct {
    int loaded;
    KainRuntimeGraphicsBarrierKind barrier_kind;
    char key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char from_step[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char to_step[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char resource_key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char reason[KAIN_RUNTIME_GRAPHICS_MAX_TITLE];
} KainRuntimeGraphicsScheduleBarrierDescriptor;

typedef struct {
    int loaded;
    int synthesized_from_bundle;
    int step_count;
    int barrier_count;
    int queue_count;
    int async_step_count;
    char primary_step_key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    KainRuntimeGraphicsScheduleStepDescriptor steps[KAIN_RUNTIME_GRAPHICS_MAX_SCHEDULE_STEPS];
    KainRuntimeGraphicsScheduleBarrierDescriptor
        barriers[KAIN_RUNTIME_GRAPHICS_MAX_SCHEDULE_BARRIERS];
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
    char summary[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];
    char reason[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];
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
    char schedule_key[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
    char summary[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];
} KainRuntimeGraphicsExecutionState;

typedef struct {
    const char* shader_bundle_path;
    const char* compute_residency_path;
    const char* compute_key;
} KainGpuRuntimeDispatchRequest;

typedef struct {
    int status_code;
    unsigned long long dispatch_invocations;
    unsigned int tensor_binding_count;
    unsigned int stream_binding_count;
    unsigned int neural_node_count;
    char message[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];
} KainGpuRuntimeDispatchResult;

typedef void* (*KainGpuRuntimeCreateFn)(const void* config);
typedef int (*KainGpuRuntimeDispatchFn)(
    void* handle,
    const KainGpuRuntimeDispatchRequest* request,
    KainGpuRuntimeDispatchResult* result
);
typedef void (*KainGpuRuntimeDestroyFn)(void* handle);

void kain_runtime_graphics_render_graph_init(KainRuntimeGraphicsRenderGraphContract* contract);
void kain_runtime_graphics_residency_init(KainRuntimeGraphicsResidencyContract* contract);
void kain_runtime_graphics_compute_schedule_init(KainRuntimeGraphicsComputeSchedule* schedule);
int kain_runtime_graphics_render_graph_is_valid(
    const KainRuntimeGraphicsRenderGraphContract* contract
);
int kain_runtime_graphics_residency_is_valid(
    const KainRuntimeGraphicsResidencyContract* contract
);
int kain_runtime_graphics_compute_schedule_is_valid(
    const KainRuntimeGraphicsComputeSchedule* schedule
);
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
void kain_runtime_graphics_format_contract_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
);
void kain_runtime_graphics_execution_state_init(KainRuntimeGraphicsExecutionState* state);
int kain_runtime_graphics_execute_primary_compute(
    const KainRuntimeGraphicsBundle* bundle,
    double frame_delta,
    double total_time,
    KainRuntimeGraphicsExecutionState* state
);
int kain_runtime_viewport_supports_graphics_bundle(const KainRuntimeGraphicsBundle* bundle);

#endif
