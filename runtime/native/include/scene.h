#ifndef SCENE_H
#define SCENE_H

#include "base.h"
#include <stddef.h>

#define SCENE_NAME_MAX 128
#define SCENE_PATH_MAX 512
#define SCENE_MESSAGE_MAX 256
#define SCENE_QUERY_MAX_HITS 8
#define DEVICE_NAME_MAX 128
#define DEVICE_VENDOR_MAX 64
#define RUNTIME_BINDING_KEY_MAX 96

typedef enum {
    KAIN_SCENE_RESOURCE_UNKNOWN = 0,
    KAIN_SCENE_RESOURCE_SCENE,
    KAIN_SCENE_RESOURCE_ENTITY,
    KAIN_SCENE_RESOURCE_MESH,
    KAIN_SCENE_RESOURCE_MATERIAL,
    KAIN_SCENE_RESOURCE_LIGHT,
    KAIN_SCENE_RESOURCE_CAMERA,
    KAIN_SCENE_RESOURCE_VOLUME,
    KAIN_SCENE_RESOURCE_FIELD,
    KAIN_SCENE_RESOURCE_INSTANCER,
    KAIN_SCENE_RESOURCE_VIEWPORT,
    KAIN_SCENE_RESOURCE_WORKSPACE,
    KAIN_SCENE_RESOURCE_PANEL,
} KainSceneResourceKind;

typedef enum {
    KAIN_SCENE_MUTATION_UNKNOWN = 0,
    KAIN_SCENE_MUTATION_CREATE,
    KAIN_SCENE_MUTATION_UPDATE,
    KAIN_SCENE_MUTATION_DELETE,
    KAIN_SCENE_MUTATION_ATTACH,
    KAIN_SCENE_MUTATION_DETACH,
    KAIN_SCENE_MUTATION_SELECT,
    KAIN_SCENE_MUTATION_DESELECT,
    KAIN_SCENE_MUTATION_RENAME,
} KainSceneMutationKind;

typedef enum {
    KAIN_SCENE_MUTATION_STATUS_UNKNOWN = 0,
    KAIN_SCENE_MUTATION_STATUS_ACCEPTED,
    KAIN_SCENE_MUTATION_STATUS_APPLIED,
    KAIN_SCENE_MUTATION_STATUS_REJECTED,
    KAIN_SCENE_MUTATION_STATUS_DEFERRED,
} KainSceneMutationStatus;

typedef enum {
    KAIN_SCENE_QUERY_UNKNOWN = 0,
    KAIN_SCENE_QUERY_PICK,
    KAIN_SCENE_QUERY_RAYCAST,
    KAIN_SCENE_QUERY_BOUNDS,
    KAIN_SCENE_QUERY_SELECTION_MASK,
    KAIN_SCENE_QUERY_VISIBILITY,
} KainSceneQueryKind;

typedef enum {
    KAIN_SCENE_QUERY_STATUS_UNKNOWN = 0,
    KAIN_SCENE_QUERY_STATUS_OK,
    KAIN_SCENE_QUERY_STATUS_EMPTY,
    KAIN_SCENE_QUERY_STATUS_UNSUPPORTED,
    KAIN_SCENE_QUERY_STATUS_FAILED,
} KainSceneQueryStatus;

typedef enum {
    BACKEND_UNKNOWN = 0,
    BACKEND_VULKAN,
    BACKEND_D3D12,
    BACKEND_METAL,
    BACKEND_SOFTWARE,
} KainRuntimeBackendKind;

typedef struct {
    unsigned long long value;
} KainSceneHandle;

typedef struct {
    double x;
    double y;
} KainRuntimeFloat2;

typedef struct {
    double x;
    double y;
    double z;
} KainRuntimeFloat3;

typedef struct {
    KainSceneMutationKind kind;
    KainSceneResourceKind subject_kind;
    KainSceneHandle scene;
    KainSceneHandle subject;
    KainSceneHandle parent;
    unsigned long long transaction_id;
    unsigned long long sequence_id;
    unsigned long long submitted_tick;
    unsigned int flags;
    char subject_name[SCENE_NAME_MAX];
    char binding_key[RUNTIME_BINDING_KEY_MAX];
} KainSceneMutationRequest;

typedef struct {
    int accepted;
    KainSceneMutationStatus status;
    KainSceneHandle scene;
    KainSceneHandle subject;
    unsigned long long transaction_id;
    unsigned long long sequence_id;
    unsigned long long applied_tick;
    char message[SCENE_MESSAGE_MAX];
} KainSceneMutationReceipt;

typedef struct {
    KainSceneHandle subject;
    KainSceneResourceKind subject_kind;
    double distance;
    unsigned int selection_mask;
    KainRuntimeFloat3 position;
    KainRuntimeFloat3 normal;
    char subject_name[SCENE_NAME_MAX];
} KainSceneQueryHit;

typedef struct {
    KainSceneQueryKind kind;
    KainSceneHandle scene;
    KainSceneHandle viewport;
    KainSceneHandle focus;
    KainRuntimeFloat2 viewport_uv;
    KainRuntimeFloat3 origin;
    KainRuntimeFloat3 direction;
    KainRuntimeFloat3 bounds_min;
    KainRuntimeFloat3 bounds_max;
    double max_distance;
    unsigned int selection_mask;
    int max_hits;
    int require_visible;
} KainSceneQueryRequest;

typedef struct {
    KainSceneQueryStatus status;
    KainSceneHandle scene;
    KainSceneHandle viewport;
    KainSceneHandle primary_hit;
    int hit_count;
    KainSceneQueryHit hits[SCENE_QUERY_MAX_HITS];
    char message[SCENE_MESSAGE_MAX];
} KainSceneQueryResult;

#define RUNTIME_FEATURE_GRAPHICS            (1ull << 0)
#define RUNTIME_FEATURE_COMPUTE             (1ull << 1)
#define RUNTIME_FEATURE_PRESENT             (1ull << 2)
#define RUNTIME_FEATURE_VIEWPORT_INPUT      (1ull << 3)
#define RUNTIME_FEATURE_SCENE_QUERY         (1ull << 4)
#define RUNTIME_FEATURE_SCENE_MUTATION      (1ull << 5)
#define RUNTIME_FEATURE_RUNTIME_REFLECTION  (1ull << 6)
#define RUNTIME_FEATURE_INGESTION           (1ull << 7)
#define RUNTIME_FEATURE_HOTPLUG             (1ull << 8)
#define RUNTIME_FEATURE_PACKAGING           (1ull << 9)

typedef struct {
    KainRuntimeBackendKind kind;
    unsigned long long feature_mask;
    unsigned int api_version_major;
    unsigned int api_version_minor;
    unsigned int max_texture_dimension_2d;
    unsigned int max_resource_bindings;
    unsigned int queue_family_count;
    unsigned int frame_overlap_limit;
    char api_name[DEVICE_NAME_MAX];
    char adapter_name[DEVICE_NAME_MAX];
    char driver_name[DEVICE_NAME_MAX];
} KainRuntimeBackendDescriptor;

typedef struct {
    char display_name[DEVICE_NAME_MAX];
    unsigned int width;
    unsigned int height;
    double refresh_hz;
    int is_primary;
} KainRuntimeDisplayDescriptor;

typedef struct {
    KainRuntimeBackendKind backend_kind;
    unsigned long long feature_mask;
    unsigned long long dedicated_video_memory_bytes;
    unsigned long long shared_memory_bytes;
    unsigned int vendor_id;
    unsigned int device_id;
    unsigned int display_count;
    int online;
    int hotplug_generation;
    char device_name[DEVICE_NAME_MAX];
    char vendor_name[DEVICE_VENDOR_MAX];
    char driver_name[DEVICE_NAME_MAX];
} KainRuntimeDeviceDescriptor;

KainSceneHandle kain_scene_handle_make(
    KainSceneResourceKind kind,
    unsigned int slot,
    unsigned int generation
);
void kain_scene_handle_init(KainSceneHandle* handle);
int kain_scene_handle_is_valid(KainSceneHandle handle);
KainSceneResourceKind kain_scene_handle_kind(KainSceneHandle handle);
unsigned int kain_scene_handle_slot(KainSceneHandle handle);
unsigned int kain_scene_handle_generation(KainSceneHandle handle);

void kain_scene_mutation_request_init(KainSceneMutationRequest* request);
void kain_scene_mutation_receipt_init(KainSceneMutationReceipt* receipt);
void kain_scene_query_request_init(KainSceneQueryRequest* request);
void kain_scene_query_result_init(KainSceneQueryResult* result);
int kain_scene_query_result_append_hit(
    KainSceneQueryResult* result,
    const KainSceneQueryHit* hit
);

void backend_descriptor_init(KainRuntimeBackendDescriptor* descriptor);
void display_descriptor_init(KainRuntimeDisplayDescriptor* descriptor);
void device_descriptor_init(KainRuntimeDeviceDescriptor* descriptor);
int backend_supports_feature(
    const KainRuntimeBackendDescriptor* descriptor,
    unsigned long long feature_flag
);
int device_supports_feature(
    const KainRuntimeDeviceDescriptor* descriptor,
    unsigned long long feature_flag
);

const char* kain_scene_resource_kind_name(KainSceneResourceKind kind);
const char* kain_scene_mutation_kind_name(KainSceneMutationKind kind);
const char* kain_scene_mutation_status_name(KainSceneMutationStatus status);
const char* kain_scene_query_kind_name(KainSceneQueryKind kind);
const char* kain_scene_query_status_name(KainSceneQueryStatus status);
const char* backend_kind_name(KainRuntimeBackendKind kind);
int runtime_format_feature_mask(
    unsigned long long feature_mask,
    char* out,
    size_t out_cap
);

#endif /* SCENE_H */
