#include "../../include/scene.h"

static unsigned long long scene_pack_handle(
    KainSceneResourceKind kind,
    unsigned int slot,
    unsigned int generation
) {
    unsigned long long packed_kind = ((unsigned long long)((unsigned int)kind & 0xffu)) << 56;
    unsigned long long packed_generation = ((unsigned long long)(generation & 0x00ffffffu)) << 32;
    return packed_kind | packed_generation | (unsigned long long)slot;
}

static int runtime_append_text(char* out, size_t out_cap, const char* text) {
    size_t offset;
    size_t text_len;

    if (!out || out_cap == 0 || !text || !text[0]) {
        return 0;
    }

    offset = strlen(out);
    if (offset >= out_cap) {
        return 0;
    }

    text_len = strlen(text);
    if (offset + text_len + 1 > out_cap) {
        text_len = out_cap - offset - 1;
    }
    if (text_len == 0) {
        return 0;
    }

    memcpy(out + offset, text, text_len);
    out[offset + text_len] = '\0';
    return 1;
}

static int runtime_append_feature_name(
    unsigned long long feature_mask,
    unsigned long long flag,
    const char* name,
    int* emitted_any,
    char* out,
    size_t out_cap
) {
    if ((feature_mask & flag) == 0ull) {
        return 0;
    }
    if (emitted_any && *emitted_any) {
        runtime_append_text(out, out_cap, ",");
    }
    if (runtime_append_text(out, out_cap, name) && emitted_any) {
        *emitted_any = 1;
    }
    return 1;
}

KainSceneHandle kain_scene_handle_make(
    KainSceneResourceKind kind,
    unsigned int slot,
    unsigned int generation
) {
    KainSceneHandle handle;
    handle.value = scene_pack_handle(kind, slot, generation);
    return handle;
}

void kain_scene_handle_init(KainSceneHandle* handle) {
    if (!handle) {
        return;
    }
    handle->value = 0ull;
}

int kain_scene_handle_is_valid(KainSceneHandle handle) {
    return handle.value != 0ull;
}

KainSceneResourceKind kain_scene_handle_kind(KainSceneHandle handle) {
    return (KainSceneResourceKind)((handle.value >> 56) & 0xffull);
}

unsigned int kain_scene_handle_slot(KainSceneHandle handle) {
    return (unsigned int)(handle.value & 0xffffffffull);
}

unsigned int kain_scene_handle_generation(KainSceneHandle handle) {
    return (unsigned int)((handle.value >> 32) & 0x00ffffffull);
}

void kain_scene_mutation_request_init(KainSceneMutationRequest* request) {
    if (!request) {
        return;
    }
    ZeroMemory(request, sizeof(*request));
}

void kain_scene_mutation_receipt_init(KainSceneMutationReceipt* receipt) {
    if (!receipt) {
        return;
    }
    ZeroMemory(receipt, sizeof(*receipt));
}

void kain_scene_query_request_init(KainSceneQueryRequest* request) {
    if (!request) {
        return;
    }
    ZeroMemory(request, sizeof(*request));
    request->max_hits = SCENE_QUERY_MAX_HITS;
}

void kain_scene_query_result_init(KainSceneQueryResult* result) {
    if (!result) {
        return;
    }
    ZeroMemory(result, sizeof(*result));
}

int kain_scene_query_result_append_hit(
    KainSceneQueryResult* result,
    const KainSceneQueryHit* hit
) {
    if (!result || !hit) {
        return 0;
    }
    if (result->hit_count < 0 || result->hit_count >= SCENE_QUERY_MAX_HITS) {
        return 0;
    }
    result->hits[result->hit_count] = *hit;
    result->hit_count += 1;
    if (result->hit_count == 1) {
        result->primary_hit = hit->subject;
    }
    return 1;
}

void backend_descriptor_init(KainRuntimeBackendDescriptor* descriptor) {
    if (!descriptor) {
        return;
    }
    ZeroMemory(descriptor, sizeof(*descriptor));
}

void display_descriptor_init(KainRuntimeDisplayDescriptor* descriptor) {
    if (!descriptor) {
        return;
    }
    ZeroMemory(descriptor, sizeof(*descriptor));
}

void device_descriptor_init(KainRuntimeDeviceDescriptor* descriptor) {
    if (!descriptor) {
        return;
    }
    ZeroMemory(descriptor, sizeof(*descriptor));
}

int backend_supports_feature(
    const KainRuntimeBackendDescriptor* descriptor,
    unsigned long long feature_flag
) {
    if (!descriptor || feature_flag == 0ull) {
        return 0;
    }
    return (descriptor->feature_mask & feature_flag) != 0ull;
}

int device_supports_feature(
    const KainRuntimeDeviceDescriptor* descriptor,
    unsigned long long feature_flag
) {
    if (!descriptor || feature_flag == 0ull) {
        return 0;
    }
    return (descriptor->feature_mask & feature_flag) != 0ull;
}

const char* kain_scene_resource_kind_name(KainSceneResourceKind kind) {
    switch (kind) {
        case KAIN_SCENE_RESOURCE_SCENE: return "scene";
        case KAIN_SCENE_RESOURCE_ENTITY: return "entity";
        case KAIN_SCENE_RESOURCE_MESH: return "mesh";
        case KAIN_SCENE_RESOURCE_MATERIAL: return "material";
        case KAIN_SCENE_RESOURCE_LIGHT: return "light";
        case KAIN_SCENE_RESOURCE_CAMERA: return "camera";
        case KAIN_SCENE_RESOURCE_VOLUME: return "volume";
        case KAIN_SCENE_RESOURCE_FIELD: return "field";
        case KAIN_SCENE_RESOURCE_INSTANCER: return "instancer";
        case KAIN_SCENE_RESOURCE_VIEWPORT: return "viewport";
        case KAIN_SCENE_RESOURCE_WORKSPACE: return "workspace";
        case KAIN_SCENE_RESOURCE_PANEL: return "panel";
        case KAIN_SCENE_RESOURCE_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* kain_scene_mutation_kind_name(KainSceneMutationKind kind) {
    switch (kind) {
        case KAIN_SCENE_MUTATION_CREATE: return "create";
        case KAIN_SCENE_MUTATION_UPDATE: return "update";
        case KAIN_SCENE_MUTATION_DELETE: return "delete";
        case KAIN_SCENE_MUTATION_ATTACH: return "attach";
        case KAIN_SCENE_MUTATION_DETACH: return "detach";
        case KAIN_SCENE_MUTATION_SELECT: return "select";
        case KAIN_SCENE_MUTATION_DESELECT: return "deselect";
        case KAIN_SCENE_MUTATION_RENAME: return "rename";
        case KAIN_SCENE_MUTATION_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* kain_scene_mutation_status_name(KainSceneMutationStatus status) {
    switch (status) {
        case KAIN_SCENE_MUTATION_STATUS_ACCEPTED: return "accepted";
        case KAIN_SCENE_MUTATION_STATUS_APPLIED: return "applied";
        case KAIN_SCENE_MUTATION_STATUS_REJECTED: return "rejected";
        case KAIN_SCENE_MUTATION_STATUS_DEFERRED: return "deferred";
        case KAIN_SCENE_MUTATION_STATUS_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* kain_scene_query_kind_name(KainSceneQueryKind kind) {
    switch (kind) {
        case KAIN_SCENE_QUERY_PICK: return "pick";
        case KAIN_SCENE_QUERY_RAYCAST: return "raycast";
        case KAIN_SCENE_QUERY_BOUNDS: return "bounds";
        case KAIN_SCENE_QUERY_SELECTION_MASK: return "selection-mask";
        case KAIN_SCENE_QUERY_VISIBILITY: return "visibility";
        case KAIN_SCENE_QUERY_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* kain_scene_query_status_name(KainSceneQueryStatus status) {
    switch (status) {
        case KAIN_SCENE_QUERY_STATUS_OK: return "ok";
        case KAIN_SCENE_QUERY_STATUS_EMPTY: return "empty";
        case KAIN_SCENE_QUERY_STATUS_UNSUPPORTED: return "unsupported";
        case KAIN_SCENE_QUERY_STATUS_FAILED: return "failed";
        case KAIN_SCENE_QUERY_STATUS_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* backend_kind_name(KainRuntimeBackendKind kind) {
    switch (kind) {
        case BACKEND_VULKAN: return "vulkan";
        case BACKEND_D3D12: return "d3d12";
        case BACKEND_METAL: return "metal";
        case BACKEND_SOFTWARE: return "software";
        case BACKEND_UNKNOWN:
        default:
            return "unknown";
    }
}

int runtime_format_feature_mask(
    unsigned long long feature_mask,
    char* out,
    size_t out_cap
) {
    int emitted_any = 0;

    if (!out || out_cap == 0) {
        return 0;
    }

    out[0] = '\0';
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_GRAPHICS,
        "graphics",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_COMPUTE,
        "compute",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_PRESENT,
        "present",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_VIEWPORT_INPUT,
        "viewport-input",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_SCENE_QUERY,
        "scene-query",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_SCENE_MUTATION,
        "scene-mutation",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_RUNTIME_REFLECTION,
        "runtime-reflection",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_INGESTION,
        "ingestion",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_HOTPLUG,
        "hotplug",
        &emitted_any,
        out,
        out_cap
    );
    runtime_append_feature_name(
        feature_mask,
        RUNTIME_FEATURE_PACKAGING,
        "packaging",
        &emitted_any,
        out,
        out_cap
    );

    if (!emitted_any) {
        runtime_append_text(out, out_cap, "none");
    }
    return (int)strlen(out);
}
