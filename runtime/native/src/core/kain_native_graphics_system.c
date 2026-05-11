#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/kain_native_graphics_system.h"
#include "../../include/kain_runtime_base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define KAIN_NATIVE_GRAPHICS_TEXT_EQUALS_CI _stricmp
#else
#include <strings.h>
#define KAIN_NATIVE_GRAPHICS_TEXT_EQUALS_CI strcasecmp
#endif

typedef struct KainNativeGraphicsBackendDescriptor {
    const char* id;
    const char* status;
    int64_t supported;
    int64_t available;
} KainNativeGraphicsBackendDescriptor;

typedef struct KainNativeGraphicsBuffer {
    int in_use;
    int64_t id;
    int64_t byte_length;
    int64_t element_stride;
    uint8_t* bytes;
    char kind[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    char label[KAIN_NATIVE_GRAPHICS_MAX_KEY];
} KainNativeGraphicsBuffer;

typedef struct KainNativeGraphicsShader {
    int in_use;
    int64_t id;
    int64_t byte_length;
    uint8_t* bytes;
    char key[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    char stage[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    char entry_point[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    char source[KAIN_NATIVE_GRAPHICS_MAX_TEXT];
} KainNativeGraphicsShader;

typedef struct KainNativeGraphicsMesh {
    int in_use;
    int64_t id;
    int64_t vertex_buffer_id;
    int64_t index_buffer_id;
    int64_t vertex_count;
    int64_t index_count;
    char label[KAIN_NATIVE_GRAPHICS_MAX_KEY];
} KainNativeGraphicsMesh;

typedef struct KainNativeGraphicsPipeline {
    int in_use;
    int64_t id;
    int64_t vertex_shader_id;
    int64_t fragment_shader_id;
    char label[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    char backend_id[KAIN_NATIVE_GRAPHICS_MAX_KEY];
} KainNativeGraphicsPipeline;

typedef struct KainNativeGraphicsDrawCommand {
    char kind[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    int64_t pipeline_id;
    int64_t mesh_id;
    int64_t instance_count;
} KainNativeGraphicsDrawCommand;

typedef struct KainNativeGraphicsSession {
    int in_use;
    int64_t id;
    int64_t width;
    int64_t height;
    int64_t frame_index;
    int64_t last_presented_frame;
    int64_t next_buffer_id;
    int64_t next_shader_id;
    int64_t next_mesh_id;
    int64_t next_pipeline_id;
    double last_delta_ms;
    char app_name[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    char active_backend_id[KAIN_NATIVE_GRAPHICS_MAX_KEY];
    KainNativeGraphicsBuffer buffers[KAIN_NATIVE_GRAPHICS_MAX_BUFFERS];
    KainNativeGraphicsShader shaders[KAIN_NATIVE_GRAPHICS_MAX_SHADERS];
    KainNativeGraphicsMesh meshes[KAIN_NATIVE_GRAPHICS_MAX_MESHES];
    KainNativeGraphicsPipeline pipelines[KAIN_NATIVE_GRAPHICS_MAX_PIPELINES];
    KainNativeGraphicsDrawCommand draw_commands[KAIN_NATIVE_GRAPHICS_MAX_DRAW_COMMANDS];
    int64_t buffer_count;
    int64_t shader_count;
    int64_t mesh_count;
    int64_t pipeline_count;
    int64_t draw_command_count;
} KainNativeGraphicsSession;

static const KainNativeGraphicsBackendDescriptor g_backends[] = {
    {
        "auto",
        "auto resolves to the software command recorder until a direct graphics executor is attached",
        1,
        1
    },
    {
        "software",
        "software command recording is available in the native graphics kernel",
        1,
        1
    },
    {
        "opengl",
        "opengl requires a platform viewport executor; this raw kernel only records authored commands",
        1,
        0
    },
    {
        "vulkan",
        "vulkan is a declared native backend target, but no direct Vulkan executor is attached yet",
        1,
        0
    },
    {
        "d3d12",
        "directx12 is a declared native backend target, but no direct D3D12 executor is attached yet",
        1,
        0
    },
};

static KainNativeGraphicsSession g_sessions[KAIN_NATIVE_GRAPHICS_MAX_SESSIONS];
static int64_t g_next_session_id = 1;
static int64_t g_last_status = KAIN_NATIVE_GRAPHICS_OK;
static char g_last_error_kind[KAIN_NATIVE_GRAPHICS_MAX_KEY] = "ok";
static char g_last_error_message[KAIN_NATIVE_GRAPHICS_MAX_TEXT] = "ok";
static char g_empty_string[] = "";

static void kain_native_graphics_free_bytes(uint8_t** bytes) {
    if (!bytes || !*bytes) {
        return;
    }
    free(*bytes);
    *bytes = NULL;
}

static void kain_native_graphics_release_session_resources(
    KainNativeGraphicsSession* session
) {
    int64_t index;
    if (!session) {
        return;
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_BUFFERS; index += 1) {
        kain_native_graphics_free_bytes(&session->buffers[index].bytes);
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SHADERS; index += 1) {
        kain_native_graphics_free_bytes(&session->shaders[index].bytes);
    }
}

static void kain_native_graphics_copy_text(
    char* destination,
    size_t destination_size,
    const char* source
) {
    if (!destination || destination_size == 0) {
        return;
    }
    if (!source) {
        source = "";
    }
    snprintf(destination, destination_size, "%s", source);
}

static const char* kain_native_graphics_return_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static int64_t kain_native_graphics_set_status(
    int64_t status,
    const char* kind,
    const char* message
) {
    g_last_status = status;
    kain_native_graphics_copy_text(
        g_last_error_kind,
        sizeof(g_last_error_kind),
        kind ? kind : "ok"
    );
    kain_native_graphics_copy_text(
        g_last_error_message,
        sizeof(g_last_error_message),
        message ? message : "ok"
    );
    return status;
}

static int64_t kain_native_graphics_ok(void) {
    return kain_native_graphics_set_status(KAIN_NATIVE_GRAPHICS_OK, "ok", "ok");
}

static int64_t kain_native_graphics_fail(
    int64_t status,
    const char* kind,
    const char* message
) {
    return kain_native_graphics_set_status(status, kind, message);
}

static const KainNativeGraphicsBackendDescriptor* kain_native_graphics_find_backend(
    const char* backend_id
) {
    size_t index;
    const char* requested = backend_id;
    if (!requested || !requested[0]) {
        requested = "auto";
    }
    for (index = 0; index < sizeof(g_backends) / sizeof(g_backends[0]); index += 1) {
        if (KAIN_NATIVE_GRAPHICS_TEXT_EQUALS_CI(g_backends[index].id, requested) == 0) {
            return &g_backends[index];
        }
    }
    if (KAIN_NATIVE_GRAPHICS_TEXT_EQUALS_CI(requested, "dx12") == 0 ||
        KAIN_NATIVE_GRAPHICS_TEXT_EQUALS_CI(requested, "directx12") == 0 ||
        KAIN_NATIVE_GRAPHICS_TEXT_EQUALS_CI(requested, "direct3d12") == 0) {
        return &g_backends[4];
    }
    return NULL;
}

static KainNativeGraphicsSession* kain_native_graphics_find_session(int64_t session_id) {
    int64_t index;
    if (session_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use && g_sessions[index].id == session_id) {
            return &g_sessions[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsBuffer* kain_native_graphics_find_buffer(
    KainNativeGraphicsSession* session,
    int64_t buffer_id
) {
    int64_t index;
    if (!session || buffer_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_BUFFERS; index += 1) {
        if (session->buffers[index].in_use && session->buffers[index].id == buffer_id) {
            return &session->buffers[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsShader* kain_native_graphics_find_shader(
    KainNativeGraphicsSession* session,
    int64_t shader_id
) {
    int64_t index;
    if (!session || shader_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SHADERS; index += 1) {
        if (session->shaders[index].in_use && session->shaders[index].id == shader_id) {
            return &session->shaders[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsMesh* kain_native_graphics_find_mesh(
    KainNativeGraphicsSession* session,
    int64_t mesh_id
) {
    int64_t index;
    if (!session || mesh_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_MESHES; index += 1) {
        if (session->meshes[index].in_use && session->meshes[index].id == mesh_id) {
            return &session->meshes[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsPipeline* kain_native_graphics_find_pipeline(
    KainNativeGraphicsSession* session,
    int64_t pipeline_id
) {
    int64_t index;
    if (!session || pipeline_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_PIPELINES; index += 1) {
        if (session->pipelines[index].in_use && session->pipelines[index].id == pipeline_id) {
            return &session->pipelines[index];
        }
    }
    return NULL;
}

static int kain_native_graphics_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

static int64_t kain_native_graphics_decode_hex(
    const char* bytes_hex,
    uint8_t** out_bytes
) {
    size_t index;
    size_t length;
    size_t byte_count;
    uint8_t* bytes;
    if (!bytes_hex) {
        return -1;
    }
    if (out_bytes) {
        *out_bytes = NULL;
    }
    length = strlen(bytes_hex);
    if ((length % 2u) != 0u) {
        return -1;
    }
    for (index = 0; index < length; index += 1) {
        if (kain_native_graphics_hex_value(bytes_hex[index]) < 0) {
            return -1;
        }
    }
    byte_count = length / 2u;
    if (byte_count == 0u) {
        return 0;
    }
    bytes = (uint8_t*)malloc(byte_count);
    if (!bytes) {
        return -1;
    }
    for (index = 0; index < byte_count; index += 1) {
        int high = kain_native_graphics_hex_value(bytes_hex[index * 2u]);
        int low = kain_native_graphics_hex_value(bytes_hex[index * 2u + 1u]);
        bytes[index] = (uint8_t)((high << 4) | low);
    }
    if (out_bytes) {
        *out_bytes = bytes;
    } else {
        free(bytes);
    }
    return (int64_t)byte_count;
}

static int64_t kain_native_graphics_read_file_bytes(
    const char* path,
    uint8_t** out_bytes
) {
    FILE* file;
    uint8_t stack_buffer[4096];
    uint8_t* bytes = NULL;
    size_t capacity = 0;
    size_t total = 0;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (!path || !path[0]) {
        return -1;
    }

    file = fopen(path, "rb");
    if (!file) {
        return -1;
    }

    for (;;) {
        size_t read = fread(stack_buffer, 1, sizeof(stack_buffer), file);
        if (read > 0u) {
            size_t needed = total + read;
            if (needed > capacity) {
                size_t next_capacity = capacity ? capacity : sizeof(stack_buffer);
                uint8_t* resized;
                while (next_capacity < needed) {
                    next_capacity *= 2u;
                }
                resized = (uint8_t*)realloc(bytes, next_capacity);
                if (!resized) {
                    free(bytes);
                    fclose(file);
                    return -1;
                }
                bytes = resized;
                capacity = next_capacity;
            }
            memcpy(bytes + total, stack_buffer, read);
            total = needed;
        }
        if (read < sizeof(stack_buffer)) {
            if (ferror(file)) {
                free(bytes);
                fclose(file);
                return -1;
            }
            break;
        }
    }

    fclose(file);
    if (total == 0u) {
        free(bytes);
        return 0;
    }
    if (out_bytes) {
        *out_bytes = bytes;
    } else {
        free(bytes);
    }
    return (int64_t)total;
}

int64_t kain_native_graphics_reset(void) {
    int64_t index;
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SESSIONS; index += 1) {
        kain_native_graphics_release_session_resources(&g_sessions[index]);
    }
    memset(g_sessions, 0, sizeof(g_sessions));
    g_next_session_id = 1;
    return kain_native_graphics_ok();
}

int64_t kain_native_graphics_session_create(
    const char* app_name,
    int64_t width,
    int64_t height
) {
    int64_t index;
    if (width <= 0 || height <= 0) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT,
            "invalid-argument",
            "graphics session dimensions must be positive"
        );
    }
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SESSIONS; index += 1) {
        if (!g_sessions[index].in_use) {
            memset(&g_sessions[index], 0, sizeof(g_sessions[index]));
            g_sessions[index].in_use = 1;
            g_sessions[index].id = g_next_session_id++;
            g_sessions[index].width = width;
            g_sessions[index].height = height;
            g_sessions[index].next_buffer_id = 1;
            g_sessions[index].next_shader_id = 1;
            g_sessions[index].next_mesh_id = 1;
            g_sessions[index].next_pipeline_id = 1;
            kain_native_graphics_copy_text(
                g_sessions[index].app_name,
                sizeof(g_sessions[index].app_name),
                app_name
            );
            kain_native_graphics_copy_text(
                g_sessions[index].active_backend_id,
                sizeof(g_sessions[index].active_backend_id),
                "software"
            );
            kain_native_graphics_ok();
            return g_sessions[index].id;
        }
    }
    return kain_native_graphics_fail(
        KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
        "capacity-exceeded",
        "graphics session capacity exceeded"
    );
}

int64_t kain_native_graphics_session_destroy(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    kain_native_graphics_release_session_resources(session);
    memset(session, 0, sizeof(*session));
    return kain_native_graphics_ok();
}

int64_t kain_native_graphics_session_count(void) {
    int64_t index;
    int64_t count = 0;
    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use) {
            count += 1;
        }
    }
    return count;
}

int64_t kain_native_graphics_backend_supported(const char* backend_id) {
    return kain_native_graphics_find_backend(backend_id) ? 1 : 0;
}

int64_t kain_native_graphics_backend_available(const char* backend_id) {
    const KainNativeGraphicsBackendDescriptor* backend =
        kain_native_graphics_find_backend(backend_id);
    return backend ? backend->available : 0;
}

const char* kain_native_graphics_backend_status(const char* backend_id) {
    const KainNativeGraphicsBackendDescriptor* backend =
        kain_native_graphics_find_backend(backend_id);
    if (!backend) {
        return kain_native_graphics_return_string("unknown graphics backend");
    }
    return kain_native_graphics_return_string(backend->status);
}

int64_t kain_native_graphics_backend_select(int64_t session_id, const char* backend_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    const KainNativeGraphicsBackendDescriptor* backend =
        kain_native_graphics_find_backend(backend_id);

    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!backend) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_UNSUPPORTED_BACKEND,
            "unsupported-backend",
            "graphics backend is not known to the native access layer"
        );
    }

    if (strcmp(backend->id, "auto") == 0) {
        kain_native_graphics_copy_text(
            session->active_backend_id,
            sizeof(session->active_backend_id),
            "software"
        );
    } else {
        kain_native_graphics_copy_text(
            session->active_backend_id,
            sizeof(session->active_backend_id),
            backend->id
        );
    }

    if (!backend->available) {
        return kain_native_graphics_set_status(
            KAIN_NATIVE_GRAPHICS_OK,
            "degraded-backend",
            backend->status
        );
    }

    return kain_native_graphics_ok();
}

const char* kain_native_graphics_active_backend(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session) {
        kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
        return kain_native_graphics_return_string("");
    }
    return kain_native_graphics_return_string(session->active_backend_id);
}

int64_t kain_native_graphics_begin_frame(int64_t session_id, double delta_ms) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    session->frame_index += 1;
    session->last_delta_ms = delta_ms;
    session->draw_command_count = 0;
    return session->frame_index;
}

int64_t kain_native_graphics_end_frame(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    return session->draw_command_count;
}

int64_t kain_native_graphics_present(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    session->last_presented_frame = session->frame_index;
    return session->last_presented_frame;
}

int64_t kain_native_graphics_frame_index(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    return session ? session->frame_index : KAIN_NATIVE_GRAPHICS_INVALID_SESSION;
}

int64_t kain_native_graphics_last_presented_frame(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    return session ? session->last_presented_frame : KAIN_NATIVE_GRAPHICS_INVALID_SESSION;
}

int64_t kain_native_graphics_buffer_create(
    int64_t session_id,
    const char* kind,
    const char* label,
    int64_t byte_length,
    int64_t element_stride
) {
    int64_t index;
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);

    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!kind || !kind[0] || byte_length < 0 || element_stride < 0) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT,
            "invalid-argument",
            "graphics buffer requires a kind and non-negative sizing"
        );
    }
    if (session->buffer_count >= KAIN_NATIVE_GRAPHICS_MAX_BUFFERS) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics buffer capacity exceeded"
        );
    }

    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_BUFFERS; index += 1) {
        if (!session->buffers[index].in_use) {
            memset(&session->buffers[index], 0, sizeof(session->buffers[index]));
            session->buffers[index].in_use = 1;
            session->buffers[index].id = session->next_buffer_id++;
            session->buffers[index].byte_length = byte_length;
            session->buffers[index].element_stride = element_stride;
            kain_native_graphics_copy_text(
                session->buffers[index].kind,
                sizeof(session->buffers[index].kind),
                kind
            );
            kain_native_graphics_copy_text(
                session->buffers[index].label,
                sizeof(session->buffers[index].label),
                label
            );
            session->buffer_count += 1;
            kain_native_graphics_ok();
            return session->buffers[index].id;
        }
    }

    return kain_native_graphics_fail(
        KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
        "capacity-exceeded",
        "graphics buffer capacity exceeded"
    );
}

int64_t kain_native_graphics_buffer_create_from_hex(
    int64_t session_id,
    const char* kind,
    const char* label,
    const char* bytes_hex,
    int64_t element_stride
) {
    uint8_t* bytes = NULL;
    int64_t byte_length = kain_native_graphics_decode_hex(bytes_hex, &bytes);
    int64_t buffer_id;
    KainNativeGraphicsBuffer* buffer;

    if (byte_length < 0) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT,
            "invalid-hex",
            "graphics buffer hex payload must contain complete bytes"
        );
    }
    buffer_id = kain_native_graphics_buffer_create(
        session_id,
        kind,
        label,
        byte_length,
        element_stride
    );
    if (buffer_id <= 0) {
        free(bytes);
        return buffer_id;
    }
    buffer = kain_native_graphics_find_buffer(
        kain_native_graphics_find_session(session_id),
        buffer_id
    );
    if (!buffer) {
        free(bytes);
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "graphics buffer registration failed"
        );
    }
    buffer->bytes = bytes;
    return buffer_id;
}

int64_t kain_native_graphics_buffer_byte_length(int64_t session_id, int64_t buffer_id) {
    KainNativeGraphicsBuffer* buffer =
        kain_native_graphics_find_buffer(kain_native_graphics_find_session(session_id), buffer_id);
    return buffer ? buffer->byte_length : KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
}

int64_t kain_native_graphics_buffer_byte_at(
    int64_t session_id,
    int64_t buffer_id,
    int64_t byte_offset
) {
    KainNativeGraphicsBuffer* buffer =
        kain_native_graphics_find_buffer(kain_native_graphics_find_session(session_id), buffer_id);
    if (!buffer || !buffer->bytes || byte_offset < 0 || byte_offset >= buffer->byte_length) {
        return KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
    }
    return (int64_t)buffer->bytes[byte_offset];
}

const char* kain_native_graphics_buffer_kind(int64_t session_id, int64_t buffer_id) {
    KainNativeGraphicsBuffer* buffer =
        kain_native_graphics_find_buffer(kain_native_graphics_find_session(session_id), buffer_id);
    return kain_native_graphics_return_string(buffer ? buffer->kind : "");
}

const char* kain_native_graphics_buffer_label(int64_t session_id, int64_t buffer_id) {
    KainNativeGraphicsBuffer* buffer =
        kain_native_graphics_find_buffer(kain_native_graphics_find_session(session_id), buffer_id);
    return kain_native_graphics_return_string(buffer ? buffer->label : "");
}

static int64_t kain_native_graphics_shader_register_bytes(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* source,
    uint8_t* owned_bytes,
    int64_t byte_length
) {
    int64_t index;
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);

    if (!session) {
        free(owned_bytes);
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!key || !key[0] || !stage || !stage[0] || !owned_bytes || byte_length <= 0) {
        free(owned_bytes);
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT,
            "invalid-argument",
            "SPIR-V shader registration requires a key, stage, and non-empty byte payload"
        );
    }
    if (session->shader_count >= KAIN_NATIVE_GRAPHICS_MAX_SHADERS) {
        free(owned_bytes);
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics shader capacity exceeded"
        );
    }

    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_SHADERS; index += 1) {
        if (!session->shaders[index].in_use) {
            memset(&session->shaders[index], 0, sizeof(session->shaders[index]));
            session->shaders[index].in_use = 1;
            session->shaders[index].id = session->next_shader_id++;
            session->shaders[index].byte_length = byte_length;
            session->shaders[index].bytes = owned_bytes;
            kain_native_graphics_copy_text(
                session->shaders[index].key,
                sizeof(session->shaders[index].key),
                key
            );
            kain_native_graphics_copy_text(
                session->shaders[index].stage,
                sizeof(session->shaders[index].stage),
                stage
            );
            kain_native_graphics_copy_text(
                session->shaders[index].entry_point,
                sizeof(session->shaders[index].entry_point),
                entry_point && entry_point[0] ? entry_point : "main"
            );
            kain_native_graphics_copy_text(
                session->shaders[index].source,
                sizeof(session->shaders[index].source),
                source
            );
            session->shader_count += 1;
            kain_native_graphics_ok();
            return session->shaders[index].id;
        }
    }

    free(owned_bytes);
    return kain_native_graphics_fail(
        KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
        "capacity-exceeded",
        "graphics shader capacity exceeded"
    );
}

int64_t kain_native_graphics_shader_spirv_from_hex(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* bytes_hex
) {
    uint8_t* bytes = NULL;
    int64_t byte_length = kain_native_graphics_decode_hex(bytes_hex, &bytes);
    if (byte_length <= 0) {
        free(bytes);
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT,
            "invalid-spirv",
            "SPIR-V hex payload must contain at least one byte"
        );
    }
    return kain_native_graphics_shader_register_bytes(
        session_id,
        key,
        stage,
        entry_point,
        "inline-spirv-hex",
        bytes,
        byte_length
    );
}

int64_t kain_native_graphics_shader_spirv_from_file(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* path
) {
    uint8_t* bytes = NULL;
    int64_t byte_length = kain_native_graphics_read_file_bytes(path, &bytes);

    if (byte_length <= 0) {
        free(bytes);
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT,
            "invalid-spirv-file",
            "SPIR-V file could not be read or was empty"
        );
    }

    return kain_native_graphics_shader_register_bytes(
        session_id,
        key,
        stage,
        entry_point,
        path,
        bytes,
        byte_length
    );
}

int64_t kain_native_graphics_shader_byte_length(int64_t session_id, int64_t shader_id) {
    KainNativeGraphicsShader* shader =
        kain_native_graphics_find_shader(kain_native_graphics_find_session(session_id), shader_id);
    return shader ? shader->byte_length : KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
}

int64_t kain_native_graphics_shader_byte_at(
    int64_t session_id,
    int64_t shader_id,
    int64_t byte_offset
) {
    KainNativeGraphicsShader* shader =
        kain_native_graphics_find_shader(kain_native_graphics_find_session(session_id), shader_id);
    if (!shader || !shader->bytes || byte_offset < 0 || byte_offset >= shader->byte_length) {
        return KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
    }
    return (int64_t)shader->bytes[byte_offset];
}

const char* kain_native_graphics_shader_key(int64_t session_id, int64_t shader_id) {
    KainNativeGraphicsShader* shader =
        kain_native_graphics_find_shader(kain_native_graphics_find_session(session_id), shader_id);
    return kain_native_graphics_return_string(shader ? shader->key : "");
}

const char* kain_native_graphics_shader_stage(int64_t session_id, int64_t shader_id) {
    KainNativeGraphicsShader* shader =
        kain_native_graphics_find_shader(kain_native_graphics_find_session(session_id), shader_id);
    return kain_native_graphics_return_string(shader ? shader->stage : "");
}

int64_t kain_native_graphics_mesh_create(
    int64_t session_id,
    const char* label,
    int64_t vertex_buffer_id,
    int64_t index_buffer_id,
    int64_t vertex_count,
    int64_t index_count
) {
    int64_t index;
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);

    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!kain_native_graphics_find_buffer(session, vertex_buffer_id) ||
        !kain_native_graphics_find_buffer(session, index_buffer_id) ||
        vertex_count <= 0 ||
        index_count <= 0) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "mesh creation requires valid vertex and index buffers with positive counts"
        );
    }
    if (session->mesh_count >= KAIN_NATIVE_GRAPHICS_MAX_MESHES) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics mesh capacity exceeded"
        );
    }

    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_MESHES; index += 1) {
        if (!session->meshes[index].in_use) {
            memset(&session->meshes[index], 0, sizeof(session->meshes[index]));
            session->meshes[index].in_use = 1;
            session->meshes[index].id = session->next_mesh_id++;
            session->meshes[index].vertex_buffer_id = vertex_buffer_id;
            session->meshes[index].index_buffer_id = index_buffer_id;
            session->meshes[index].vertex_count = vertex_count;
            session->meshes[index].index_count = index_count;
            kain_native_graphics_copy_text(
                session->meshes[index].label,
                sizeof(session->meshes[index].label),
                label
            );
            session->mesh_count += 1;
            kain_native_graphics_ok();
            return session->meshes[index].id;
        }
    }

    return kain_native_graphics_fail(
        KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
        "capacity-exceeded",
        "graphics mesh capacity exceeded"
    );
}

int64_t kain_native_graphics_mesh_vertex_count(int64_t session_id, int64_t mesh_id) {
    KainNativeGraphicsMesh* mesh =
        kain_native_graphics_find_mesh(kain_native_graphics_find_session(session_id), mesh_id);
    return mesh ? mesh->vertex_count : KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
}

int64_t kain_native_graphics_mesh_index_count(int64_t session_id, int64_t mesh_id) {
    KainNativeGraphicsMesh* mesh =
        kain_native_graphics_find_mesh(kain_native_graphics_find_session(session_id), mesh_id);
    return mesh ? mesh->index_count : KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
}

const char* kain_native_graphics_mesh_label(int64_t session_id, int64_t mesh_id) {
    KainNativeGraphicsMesh* mesh =
        kain_native_graphics_find_mesh(kain_native_graphics_find_session(session_id), mesh_id);
    return kain_native_graphics_return_string(mesh ? mesh->label : "");
}

int64_t kain_native_graphics_pipeline_create(
    int64_t session_id,
    const char* label,
    int64_t vertex_shader_id,
    int64_t fragment_shader_id,
    const char* backend_id
) {
    int64_t index;
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    const KainNativeGraphicsBackendDescriptor* backend =
        kain_native_graphics_find_backend(backend_id);

    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!backend) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_UNSUPPORTED_BACKEND,
            "unsupported-backend",
            "pipeline backend is not known to the native access layer"
        );
    }
    if (!kain_native_graphics_find_shader(session, vertex_shader_id) ||
        !kain_native_graphics_find_shader(session, fragment_shader_id)) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "pipeline creation requires valid vertex and fragment shaders"
        );
    }
    if (session->pipeline_count >= KAIN_NATIVE_GRAPHICS_MAX_PIPELINES) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics pipeline capacity exceeded"
        );
    }

    for (index = 0; index < KAIN_NATIVE_GRAPHICS_MAX_PIPELINES; index += 1) {
        if (!session->pipelines[index].in_use) {
            memset(&session->pipelines[index], 0, sizeof(session->pipelines[index]));
            session->pipelines[index].in_use = 1;
            session->pipelines[index].id = session->next_pipeline_id++;
            session->pipelines[index].vertex_shader_id = vertex_shader_id;
            session->pipelines[index].fragment_shader_id = fragment_shader_id;
            kain_native_graphics_copy_text(
                session->pipelines[index].label,
                sizeof(session->pipelines[index].label),
                label
            );
            kain_native_graphics_copy_text(
                session->pipelines[index].backend_id,
                sizeof(session->pipelines[index].backend_id),
                strcmp(backend->id, "auto") == 0 ? session->active_backend_id : backend->id
            );
            session->pipeline_count += 1;
            if (!backend->available) {
                kain_native_graphics_set_status(
                    KAIN_NATIVE_GRAPHICS_OK,
                    "degraded-backend",
                    backend->status
                );
            } else {
                kain_native_graphics_ok();
            }
            return session->pipelines[index].id;
        }
    }

    return kain_native_graphics_fail(
        KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
        "capacity-exceeded",
        "graphics pipeline capacity exceeded"
    );
}

const char* kain_native_graphics_pipeline_label(int64_t session_id, int64_t pipeline_id) {
    KainNativeGraphicsPipeline* pipeline =
        kain_native_graphics_find_pipeline(kain_native_graphics_find_session(session_id), pipeline_id);
    return kain_native_graphics_return_string(pipeline ? pipeline->label : "");
}

const char* kain_native_graphics_pipeline_backend(int64_t session_id, int64_t pipeline_id) {
    KainNativeGraphicsPipeline* pipeline =
        kain_native_graphics_find_pipeline(kain_native_graphics_find_session(session_id), pipeline_id);
    return kain_native_graphics_return_string(pipeline ? pipeline->backend_id : "");
}

int64_t kain_native_graphics_draw_mesh(
    int64_t session_id,
    int64_t pipeline_id,
    int64_t mesh_id,
    int64_t instance_count
) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    KainNativeGraphicsDrawCommand* command;

    if (!session) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!kain_native_graphics_find_pipeline(session, pipeline_id) ||
        !kain_native_graphics_find_mesh(session, mesh_id) ||
        instance_count <= 0) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "draw command requires a valid pipeline, mesh, and positive instance count"
        );
    }
    if (session->draw_command_count >= KAIN_NATIVE_GRAPHICS_MAX_DRAW_COMMANDS) {
        return kain_native_graphics_fail(
            KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics draw command capacity exceeded"
        );
    }

    command = &session->draw_commands[session->draw_command_count];
    memset(command, 0, sizeof(*command));
    kain_native_graphics_copy_text(command->kind, sizeof(command->kind), "draw_mesh");
    command->pipeline_id = pipeline_id;
    command->mesh_id = mesh_id;
    command->instance_count = instance_count;
    session->draw_command_count += 1;
    return session->draw_command_count;
}

int64_t kain_native_graphics_draw_command_count(int64_t session_id) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    return session ? session->draw_command_count : KAIN_NATIVE_GRAPHICS_INVALID_SESSION;
}

const char* kain_native_graphics_draw_command_kind(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return kain_native_graphics_return_string("");
    }
    return kain_native_graphics_return_string(session->draw_commands[command_index].kind);
}

int64_t kain_native_graphics_draw_command_mesh(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
    }
    return session->draw_commands[command_index].mesh_id;
}

int64_t kain_native_graphics_draw_command_pipeline(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
    }
    return session->draw_commands[command_index].pipeline_id;
}

int64_t kain_native_graphics_draw_command_instances(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = kain_native_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE;
    }
    return session->draw_commands[command_index].instance_count;
}

int64_t kain_native_graphics_last_status(void) {
    return g_last_status;
}

const char* kain_native_graphics_last_error_kind(void) {
    return kain_native_graphics_return_string(g_last_error_kind);
}

const char* kain_native_graphics_last_error_message(void) {
    return kain_native_graphics_return_string(g_last_error_message);
}
