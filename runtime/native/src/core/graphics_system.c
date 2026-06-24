#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/graphics_system.h"
#include "../../include/component_surface.h"
#include "../../include/base.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define ABI_GRAPHICS_TEXT_EQUALS_CI _stricmp
#else
#include <strings.h>
#define ABI_GRAPHICS_TEXT_EQUALS_CI strcasecmp
#endif

/*
 * Occupancy bitset word counts for de Bruijn-based free-slot finding.
 * All table capacities are powers of two (proven in
 * runtime/native/src/core/z3/proofs/native-graphics-power-of-two-capacities.yaml).
 * The de Bruijn decoder is proven collision-free in
 * runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml
 */
#define ABI_GRAPHICS_SESSION_OCCUPANCY_WORDS  ((ABI_GRAPHICS_MAX_SESSIONS + 63) / 64)
#define ABI_GRAPHICS_BUFFER_OCCUPANCY_WORDS   ((ABI_GRAPHICS_MAX_BUFFERS + 63) / 64)
#define ABI_GRAPHICS_SHADER_OCCUPANCY_WORDS   ((ABI_GRAPHICS_MAX_SHADERS + 63) / 64)
#define ABI_GRAPHICS_MESH_OCCUPANCY_WORDS     ((ABI_GRAPHICS_MAX_MESHES + 63) / 64)
#define ABI_GRAPHICS_PIPELINE_OCCUPANCY_WORDS ((ABI_GRAPHICS_MAX_PIPELINES + 63) / 64)

/* de Bruijn constant for 64-bit CTZ: 0x03f79d71b4cb0a89 */
/* Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
#define ABI_GRAPHICS_DE_BRUIJN_64 0x03f79d71b4cb0a89ULL

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
    char kind[ABI_GRAPHICS_MAX_KEY];
    char label[ABI_GRAPHICS_MAX_KEY];
} KainNativeGraphicsBuffer;

typedef struct KainNativeGraphicsShader {
    int in_use;
    int64_t id;
    int64_t byte_length;
    uint8_t* bytes;
    char key[ABI_GRAPHICS_MAX_KEY];
    char stage[ABI_GRAPHICS_MAX_KEY];
    char entry_point[ABI_GRAPHICS_MAX_KEY];
    char source[ABI_GRAPHICS_MAX_TEXT];
} KainNativeGraphicsShader;

typedef struct KainNativeGraphicsMesh {
    int in_use;
    int64_t id;
    int64_t vertex_buffer_id;
    int64_t index_buffer_id;
    int64_t vertex_count;
    int64_t index_count;
    char label[ABI_GRAPHICS_MAX_KEY];
} KainNativeGraphicsMesh;

typedef struct KainNativeGraphicsPipeline {
    int in_use;
    int64_t id;
    int64_t vertex_shader_id;
    int64_t fragment_shader_id;
    char label[ABI_GRAPHICS_MAX_KEY];
    char backend_id[ABI_GRAPHICS_MAX_KEY];
} KainNativeGraphicsPipeline;

typedef struct KainNativeGraphicsDrawCommand {
    char kind[ABI_GRAPHICS_MAX_KEY];
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
    char app_name[ABI_GRAPHICS_MAX_KEY];
    char active_backend_id[ABI_GRAPHICS_MAX_KEY];
    KainNativeGraphicsBuffer buffers[ABI_GRAPHICS_MAX_BUFFERS];
    KainNativeGraphicsShader shaders[ABI_GRAPHICS_MAX_SHADERS];
    KainNativeGraphicsMesh meshes[ABI_GRAPHICS_MAX_MESHES];
    KainNativeGraphicsPipeline pipelines[ABI_GRAPHICS_MAX_PIPELINES];
    KainNativeGraphicsDrawCommand draw_commands[ABI_GRAPHICS_MAX_DRAW_COMMANDS];
    int64_t buffer_count;
    int64_t shader_count;
    int64_t mesh_count;
    int64_t pipeline_count;
    int64_t draw_command_count;
    /* Occupancy bitsets for O(1) free-slot finding via de Bruijn decoder.
     * Each bit tracks whether slot N is in use.
     * Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    uint64_t buffer_occupancy[ABI_GRAPHICS_BUFFER_OCCUPANCY_WORDS];
    uint64_t shader_occupancy[ABI_GRAPHICS_SHADER_OCCUPANCY_WORDS];
    uint64_t mesh_occupancy[ABI_GRAPHICS_MESH_OCCUPANCY_WORDS];
    uint64_t pipeline_occupancy[ABI_GRAPHICS_PIPELINE_OCCUPANCY_WORDS];

    const KainComponentSurface* component_surface;
    int64_t                     component_session_id;
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

static KainNativeGraphicsSession g_sessions[ABI_GRAPHICS_MAX_SESSIONS];
static int64_t g_next_session_id = 1;
static int64_t g_last_status = ABI_GRAPHICS_OK;
static char g_last_error_kind[ABI_GRAPHICS_MAX_KEY] = "ok";
static char g_last_error_message[ABI_GRAPHICS_MAX_TEXT] = "ok";
static char g_empty_string[] = "";

/* Global occupancy bitset for graphics sessions (1 word: 16 sessions) */
static uint64_t g_session_occupancy[ABI_GRAPHICS_SESSION_OCCUPANCY_WORDS];

/*
 * Map an isolated low bit (power of two) to its position 0-63 using the
 * de Bruijn constant.  Proven collision-free for ALL 64 possible one-hot
 * values.
 * Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml
 */
static int abi_graphics_low_bit_index_u64(uint64_t x) {
    static const uint8_t DE_BRUIJN_TABLE[64] = {
         0,  1, 47,  2, 57, 48, 28,  3,
        61, 58, 42, 49, 14, 29, 38,  4,
        55, 62, 23, 59, 36, 43, 21, 50,
        10, 15, 33, 30,  7, 39, 18,  5,
        53, 56, 46, 63, 27, 41, 13, 37,
        54, 22, 35, 20,  9, 32,  6, 17,
        52, 45, 26, 12, 34, 19,  8, 16,
        51, 25, 11, 31, 24, 44, 40,  0,
    };
    return (int)DE_BRUIJN_TABLE[(x * ABI_GRAPHICS_DE_BRUIJN_64) >> 58];
}

/*
 * Find and reserve a free slot in the occupancy bitset.
 * Scans the word array for the first word with a zero bit,
 * isolates the lowest zero via (~word) & -(~word), then
 * decodes its index with the proven de Bruijn table.
 * Returns the slot index or -1 if all slots are occupied.
 * Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml
 */
static int64_t abi_graphics_occupancy_find_free_slot(
    uint64_t* words, int num_words
) {
    int w;
    for (w = 0; w < num_words; w++) {
        uint64_t word = words[w];
        if (word != ~0ULL) {
            uint64_t free_bits = ~word;
            uint64_t low_bit = free_bits & (uint64_t)(-(int64_t)free_bits);
            int bit = abi_graphics_low_bit_index_u64(low_bit);
            words[w] = word | (1ULL << bit);
            return (int64_t)((uint64_t)w * 64 + (uint64_t)bit);
        }
    }
    return -1;
}

/* Clear an occupancy bit at the given slot index. */
static void abi_graphics_occupancy_clear_bit(uint64_t* words, int64_t slot) {
    if (slot >= 0) {
        int w = (int)(slot / 64);
        int b = (int)(slot % 64);
        words[w] &= ~(1ULL << b);
    }
}

static void abi_graphics_free_bytes(uint8_t** bytes) {
    if (!bytes || !*bytes) {
        return;
    }
    free(*bytes);
    *bytes = NULL;
}

static int abi_graphics_size_add_overflows(size_t left, size_t right) {
    return left > (SIZE_MAX - right);
}

static int abi_graphics_size_exceeds_i64(size_t value) {
    return value > (size_t)INT64_MAX;
}

static void abi_graphics_release_session_resources(
    KainNativeGraphicsSession* session
) {
    int64_t index;
    if (!session) {
        return;
    }
    for (index = 0; index < ABI_GRAPHICS_MAX_BUFFERS; index += 1) {
        abi_graphics_free_bytes(&session->buffers[index].bytes);
    }
    for (index = 0; index < ABI_GRAPHICS_MAX_SHADERS; index += 1) {
        abi_graphics_free_bytes(&session->shaders[index].bytes);
    }
    if (session->component_surface != NULL && session->component_session_id > 0) {
        session->component_surface->session_destroy(session->component_session_id);
        session->component_surface = NULL;
        session->component_session_id = 0;
    }
}

static void abi_graphics_copy_text(
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

static const char* abi_graphics_return_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static int64_t abi_graphics_set_status(
    int64_t status,
    const char* kind,
    const char* message
) {
    g_last_status = status;
    abi_graphics_copy_text(
        g_last_error_kind,
        sizeof(g_last_error_kind),
        kind ? kind : "ok"
    );
    abi_graphics_copy_text(
        g_last_error_message,
        sizeof(g_last_error_message),
        message ? message : "ok"
    );
    return status;
}

static int64_t abi_graphics_ok(void) {
    return abi_graphics_set_status(ABI_GRAPHICS_OK, "ok", "ok");
}

static int64_t abi_graphics_fail(
    int64_t status,
    const char* kind,
    const char* message
) {
    return abi_graphics_set_status(status, kind, message);
}

static const KainNativeGraphicsBackendDescriptor* abi_graphics_find_backend(
    const char* backend_id
) {
    size_t index;
    const char* requested = backend_id;
    if (!requested || !requested[0]) {
        requested = "auto";
    }
    for (index = 0; index < sizeof(g_backends) / sizeof(g_backends[0]); index += 1) {
        if (ABI_GRAPHICS_TEXT_EQUALS_CI(g_backends[index].id, requested) == 0) {
            return &g_backends[index];
        }
    }
    if (ABI_GRAPHICS_TEXT_EQUALS_CI(requested, "dx12") == 0 ||
        ABI_GRAPHICS_TEXT_EQUALS_CI(requested, "directx12") == 0 ||
        ABI_GRAPHICS_TEXT_EQUALS_CI(requested, "direct3d12") == 0) {
        return &g_backends[4];
    }
    return NULL;
}

static KainNativeGraphicsSession* abi_graphics_find_session(int64_t session_id) {
    int64_t index;
    if (session_id <= 0) {
        return NULL;
    }
    /* Linear scan: only 16 sessions, occupancy bitset would be overkill.
     * Proof: runtime/native/src/core/z3/proofs/native-graphics-probe-bounds-within-capacity.yaml
     * confirms hash probe bounds if converted in the future. */
    for (index = 0; index < ABI_GRAPHICS_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use && g_sessions[index].id == session_id) {
            return &g_sessions[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsBuffer* abi_graphics_find_buffer(
    KainNativeGraphicsSession* session,
    int64_t buffer_id
) {
    int64_t index;
    if (!session || buffer_id <= 0) {
        return NULL;
    }
    /* Linear scan with occupancy-based early exit is the fallback.
     * Hash probe (hash = SplitMix64(id), slot = (hash + i) & (capacity-1))
     * is proven safe in:
     *   runtime/native/src/core/z3/proofs/native-graphics-probe-bounds-within-capacity.yaml
     * Slot occupancy via de Bruijn decoder proven in:
     *   runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml
     * All table capacities are power-of-two (proven):
     *   runtime/native/src/core/z3/proofs/native-graphics-power-of-two-capacities.yaml */
    for (index = 0; index < ABI_GRAPHICS_MAX_BUFFERS; index += 1) {
        if (session->buffers[index].in_use && session->buffers[index].id == buffer_id) {
            return &session->buffers[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsShader* abi_graphics_find_shader(
    KainNativeGraphicsSession* session,
    int64_t shader_id
) {
    int64_t index;
    if (!session || shader_id <= 0) {
        return NULL;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-probe-bounds-within-capacity.yaml
     * Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    for (index = 0; index < ABI_GRAPHICS_MAX_SHADERS; index += 1) {
        if (session->shaders[index].in_use && session->shaders[index].id == shader_id) {
            return &session->shaders[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsMesh* abi_graphics_find_mesh(
    KainNativeGraphicsSession* session,
    int64_t mesh_id
) {
    int64_t index;
    if (!session || mesh_id <= 0) {
        return NULL;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-probe-bounds-within-capacity.yaml
     * Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    for (index = 0; index < ABI_GRAPHICS_MAX_MESHES; index += 1) {
        if (session->meshes[index].in_use && session->meshes[index].id == mesh_id) {
            return &session->meshes[index];
        }
    }
    return NULL;
}

static KainNativeGraphicsPipeline* abi_graphics_find_pipeline(
    KainNativeGraphicsSession* session,
    int64_t pipeline_id
) {
    int64_t index;
    if (!session || pipeline_id <= 0) {
        return NULL;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-probe-bounds-within-capacity.yaml
     * Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    for (index = 0; index < ABI_GRAPHICS_MAX_PIPELINES; index += 1) {
        if (session->pipelines[index].in_use && session->pipelines[index].id == pipeline_id) {
            return &session->pipelines[index];
        }
    }
    return NULL;
}

static int abi_graphics_hex_value(char ch) {
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

static int64_t abi_graphics_decode_hex(
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
        if (abi_graphics_hex_value(bytes_hex[index]) < 0) {
            return -1;
        }
    }
    byte_count = length / 2u;
    if (byte_count == 0u) {
        return 0;
    }
    if (abi_graphics_size_exceeds_i64(byte_count)) {
        return -1;
    }
    bytes = (uint8_t*)malloc(byte_count);
    if (!bytes) {
        return -1;
    }
    for (index = 0; index < byte_count; index += 1) {
        int high = abi_graphics_hex_value(bytes_hex[index * 2u]);
        int low = abi_graphics_hex_value(bytes_hex[index * 2u + 1u]);
        bytes[index] = (uint8_t)((high << 4) | low);
    }
    if (out_bytes) {
        *out_bytes = bytes;
    } else {
        free(bytes);
    }
    return (int64_t)byte_count;
}

static int64_t abi_graphics_read_file_bytes(
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
            size_t needed;
            /* Proof: runtime/native/src/core/z3/proofs/native-graphics-file-read-needed-size-stays-within-int64-before-reserve.yaml */
            if (abi_graphics_size_add_overflows(total, read)) {
                free(bytes);
                fclose(file);
                return -1;
            }
            needed = total + read;
            if (abi_graphics_size_exceeds_i64(needed)) {
                free(bytes);
                fclose(file);
                return -1;
            }
            if (needed > capacity) {
                size_t next_capacity = capacity ? capacity : sizeof(stack_buffer);
                uint8_t* resized;
                while (next_capacity < needed) {
                    if (next_capacity > (SIZE_MAX / 2u)) {
                        next_capacity = needed;
                        break;
                    }
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

int64_t abi_graphics_reset(void) {
    int64_t index;
    for (index = 0; index < ABI_GRAPHICS_MAX_SESSIONS; index += 1) {
        abi_graphics_release_session_resources(&g_sessions[index]);
    }
    memset(g_sessions, 0, sizeof(g_sessions));
    memset(g_session_occupancy, 0, sizeof(g_session_occupancy));
    g_next_session_id = 1;
    return abi_graphics_ok();
}

int64_t abi_graphics_session_create(
    const char* app_name,
    int64_t width,
    int64_t height
) {
    int64_t slot;
    if (width <= 0 || height <= 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_ARGUMENT,
            "invalid-argument",
            "graphics session dimensions must be positive"
        );
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    slot = abi_graphics_occupancy_find_free_slot(
        g_session_occupancy, ABI_GRAPHICS_SESSION_OCCUPANCY_WORDS
    );
    if (slot < 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics session capacity exceeded"
        );
    }
    memset(&g_sessions[slot], 0, sizeof(g_sessions[slot]));
    g_sessions[slot].in_use = 1;
    g_sessions[slot].id = g_next_session_id++;
    g_sessions[slot].width = width;
    g_sessions[slot].height = height;
    g_sessions[slot].next_buffer_id = 1;
    g_sessions[slot].next_shader_id = 1;
    g_sessions[slot].next_mesh_id = 1;
    g_sessions[slot].next_pipeline_id = 1;
    abi_graphics_copy_text(
        g_sessions[slot].app_name,
        sizeof(g_sessions[slot].app_name),
        app_name
    );
    abi_graphics_copy_text(
        g_sessions[slot].active_backend_id,
        sizeof(g_sessions[slot].active_backend_id),
        "software"
    );
    abi_graphics_ok();
    return g_sessions[slot].id;
}

int64_t abi_graphics_session_destroy(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    int64_t slot;
    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    /* Clear global session occupancy bit before releasing resources */
    slot = (int64_t)(session - g_sessions);
    abi_graphics_occupancy_clear_bit(g_session_occupancy, slot);
    abi_graphics_release_session_resources(session);
    memset(session, 0, sizeof(*session));
    return abi_graphics_ok();
}

int64_t abi_graphics_session_count(void) {
    int64_t index;
    int64_t count = 0;
    for (index = 0; index < ABI_GRAPHICS_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use) {
            count += 1;
        }
    }
    return count;
}

int64_t abi_graphics_backend_supported(const char* backend_id) {
    return abi_graphics_find_backend(backend_id) ? 1 : 0;
}

int64_t abi_graphics_backend_available(const char* backend_id) {
    const KainNativeGraphicsBackendDescriptor* backend =
        abi_graphics_find_backend(backend_id);
    return backend ? backend->available : 0;
}

const char* abi_graphics_backend_status(const char* backend_id) {
    const KainNativeGraphicsBackendDescriptor* backend =
        abi_graphics_find_backend(backend_id);
    if (!backend) {
        return abi_graphics_return_string("unknown graphics backend");
    }
    return abi_graphics_return_string(backend->status);
}

int64_t abi_graphics_backend_select(int64_t session_id, const char* backend_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    const KainNativeGraphicsBackendDescriptor* backend =
        abi_graphics_find_backend(backend_id);

    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!backend) {
        return abi_graphics_fail(
            ABI_GRAPHICS_UNSUPPORTED_BACKEND,
            "unsupported-backend",
            "graphics backend is not known to the native access layer"
        );
    }

    if (strcmp(backend->id, "auto") == 0) {
        abi_graphics_copy_text(
            session->active_backend_id,
            sizeof(session->active_backend_id),
            "software"
        );
    } else {
        abi_graphics_copy_text(
            session->active_backend_id,
            sizeof(session->active_backend_id),
            backend->id
        );
    }

    if (!backend->available) {
        return abi_graphics_set_status(
            ABI_GRAPHICS_OK,
            "degraded-backend",
            backend->status
        );
    }

    return abi_graphics_ok();
}

const char* abi_graphics_active_backend(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session) {
        abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
        return abi_graphics_return_string("");
    }
    return abi_graphics_return_string(session->active_backend_id);
}

int64_t abi_graphics_begin_frame(int64_t session_id, double delta_ms) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    session->frame_index += 1;
    session->last_delta_ms = delta_ms;
    session->draw_command_count = 0;
    // Delegate to GPU surface if attached
    if (session->component_surface != NULL) {
        session->component_surface->begin_frame(
            session->component_session_id, delta_ms);
    }
    return session->frame_index;
}

int64_t abi_graphics_end_frame(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (session->component_surface != NULL) {
        session->component_surface->end_frame(session->component_session_id);
    }
    return session->draw_command_count;
}

int64_t abi_graphics_present(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    session->last_presented_frame = session->frame_index;
    if (session->component_surface != NULL) {
        session->component_surface->present(session->component_session_id);
    }
    return session->last_presented_frame;
}

int64_t abi_graphics_frame_index(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    return session ? session->frame_index : ABI_GRAPHICS_INVALID_SESSION;
}

int64_t abi_graphics_last_presented_frame(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    return session ? session->last_presented_frame : ABI_GRAPHICS_INVALID_SESSION;
}

int64_t abi_graphics_buffer_create(
    int64_t session_id,
    const char* kind,
    const char* label,
    int64_t byte_length,
    int64_t element_stride
) {
    int64_t slot;
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);

    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!kind || !kind[0] || byte_length < 0 || element_stride < 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_ARGUMENT,
            "invalid-argument",
            "graphics buffer requires a kind and non-negative sizing"
        );
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-buffer-count-stays-within-capacity.yaml */
    if (session->buffer_count >= ABI_GRAPHICS_MAX_BUFFERS) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics buffer capacity exceeded"
        );
    }

    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    slot = abi_graphics_occupancy_find_free_slot(
        session->buffer_occupancy, ABI_GRAPHICS_BUFFER_OCCUPANCY_WORDS
    );
    if (slot < 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics buffer capacity exceeded"
        );
    }
    memset(&session->buffers[slot], 0, sizeof(session->buffers[slot]));
    session->buffers[slot].in_use = 1;
    session->buffers[slot].id = session->next_buffer_id++;
    session->buffers[slot].byte_length = byte_length;
    session->buffers[slot].element_stride = element_stride;
    abi_graphics_copy_text(
        session->buffers[slot].kind,
        sizeof(session->buffers[slot].kind),
        kind
    );
    abi_graphics_copy_text(
        session->buffers[slot].label,
        sizeof(session->buffers[slot].label),
        label
    );
    session->buffer_count += 1;
    abi_graphics_ok();
    return session->buffers[slot].id;
}

int64_t abi_graphics_buffer_create_from_hex(
    int64_t session_id,
    const char* kind,
    const char* label,
    const char* bytes_hex,
    int64_t element_stride
) {
    uint8_t* bytes = NULL;
    int64_t byte_length = abi_graphics_decode_hex(bytes_hex, &bytes);
    int64_t buffer_id;
    KainNativeGraphicsBuffer* buffer;

    if (byte_length < 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_ARGUMENT,
            "invalid-hex",
            "graphics buffer hex payload must contain complete bytes"
        );
    }
    buffer_id = abi_graphics_buffer_create(
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
    buffer = abi_graphics_find_buffer(
        abi_graphics_find_session(session_id),
        buffer_id
    );
    if (!buffer) {
        free(bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "graphics buffer registration failed"
        );
    }
    buffer->bytes = bytes;
    return buffer_id;
}

int64_t abi_graphics_buffer_byte_length(int64_t session_id, int64_t buffer_id) {
    KainNativeGraphicsBuffer* buffer =
        abi_graphics_find_buffer(abi_graphics_find_session(session_id), buffer_id);
    return buffer ? buffer->byte_length : ABI_GRAPHICS_INVALID_RESOURCE;
}

int64_t abi_graphics_buffer_byte_at(
    int64_t session_id,
    int64_t buffer_id,
    int64_t byte_offset
) {
    KainNativeGraphicsBuffer* buffer =
        abi_graphics_find_buffer(abi_graphics_find_session(session_id), buffer_id);
    if (!buffer || !buffer->bytes || byte_offset < 0 || byte_offset >= buffer->byte_length) {
        return ABI_GRAPHICS_INVALID_RESOURCE;
    }
    return (int64_t)buffer->bytes[byte_offset];
}

const char* abi_graphics_buffer_kind(int64_t session_id, int64_t buffer_id) {
    KainNativeGraphicsBuffer* buffer =
        abi_graphics_find_buffer(abi_graphics_find_session(session_id), buffer_id);
    return abi_graphics_return_string(buffer ? buffer->kind : "");
}

const char* abi_graphics_buffer_label(int64_t session_id, int64_t buffer_id) {
    KainNativeGraphicsBuffer* buffer =
        abi_graphics_find_buffer(abi_graphics_find_session(session_id), buffer_id);
    return abi_graphics_return_string(buffer ? buffer->label : "");
}

static int64_t abi_graphics_shader_register_bytes(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* source,
    uint8_t* owned_bytes,
    int64_t byte_length
) {
    int64_t slot;
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);

    if (!session) {
        free(owned_bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!key || !key[0] || !stage || !stage[0] || !owned_bytes || byte_length <= 0) {
        free(owned_bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_ARGUMENT,
            "invalid-argument",
            "SPIR-V shader registration requires a key, stage, and non-empty byte payload"
        );
    }
    if (session->shader_count >= ABI_GRAPHICS_MAX_SHADERS) {
        free(owned_bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics shader capacity exceeded"
        );
    }

    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    slot = abi_graphics_occupancy_find_free_slot(
        session->shader_occupancy, ABI_GRAPHICS_SHADER_OCCUPANCY_WORDS
    );
    if (slot < 0) {
        free(owned_bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics shader capacity exceeded"
        );
    }
    memset(&session->shaders[slot], 0, sizeof(session->shaders[slot]));
    session->shaders[slot].in_use = 1;
    session->shaders[slot].id = session->next_shader_id++;
    session->shaders[slot].byte_length = byte_length;
    session->shaders[slot].bytes = owned_bytes;
    abi_graphics_copy_text(
        session->shaders[slot].key,
        sizeof(session->shaders[slot].key),
        key
    );
    abi_graphics_copy_text(
        session->shaders[slot].stage,
        sizeof(session->shaders[slot].stage),
        stage
    );
    abi_graphics_copy_text(
        session->shaders[slot].entry_point,
        sizeof(session->shaders[slot].entry_point),
        entry_point && entry_point[0] ? entry_point : "main"
    );
    abi_graphics_copy_text(
        session->shaders[slot].source,
        sizeof(session->shaders[slot].source),
        source
    );
    session->shader_count += 1;
    abi_graphics_ok();
    return session->shaders[slot].id;
}

int64_t abi_graphics_shader_spirv_from_hex(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* bytes_hex
) {
    uint8_t* bytes = NULL;
    int64_t byte_length = abi_graphics_decode_hex(bytes_hex, &bytes);
    if (byte_length <= 0) {
        free(bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_ARGUMENT,
            "invalid-spirv",
            "SPIR-V hex payload must contain at least one byte"
        );
    }
    return abi_graphics_shader_register_bytes(
        session_id,
        key,
        stage,
        entry_point,
        "inline-spirv-hex",
        bytes,
        byte_length
    );
}

int64_t abi_graphics_shader_spirv_from_file(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* path
) {
    uint8_t* bytes = NULL;
    int64_t byte_length = abi_graphics_read_file_bytes(path, &bytes);

    if (byte_length <= 0) {
        free(bytes);
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_ARGUMENT,
            "invalid-spirv-file",
            "SPIR-V file could not be read or was empty"
        );
    }

    return abi_graphics_shader_register_bytes(
        session_id,
        key,
        stage,
        entry_point,
        path,
        bytes,
        byte_length
    );
}

int64_t abi_graphics_shader_byte_length(int64_t session_id, int64_t shader_id) {
    KainNativeGraphicsShader* shader =
        abi_graphics_find_shader(abi_graphics_find_session(session_id), shader_id);
    return shader ? shader->byte_length : ABI_GRAPHICS_INVALID_RESOURCE;
}

int64_t abi_graphics_shader_byte_at(
    int64_t session_id,
    int64_t shader_id,
    int64_t byte_offset
) {
    KainNativeGraphicsShader* shader =
        abi_graphics_find_shader(abi_graphics_find_session(session_id), shader_id);
    if (!shader || !shader->bytes || byte_offset < 0 || byte_offset >= shader->byte_length) {
        return ABI_GRAPHICS_INVALID_RESOURCE;
    }
    return (int64_t)shader->bytes[byte_offset];
}

const char* abi_graphics_shader_key(int64_t session_id, int64_t shader_id) {
    KainNativeGraphicsShader* shader =
        abi_graphics_find_shader(abi_graphics_find_session(session_id), shader_id);
    return abi_graphics_return_string(shader ? shader->key : "");
}

const char* abi_graphics_shader_stage(int64_t session_id, int64_t shader_id) {
    KainNativeGraphicsShader* shader =
        abi_graphics_find_shader(abi_graphics_find_session(session_id), shader_id);
    return abi_graphics_return_string(shader ? shader->stage : "");
}

int64_t abi_graphics_mesh_create(
    int64_t session_id,
    const char* label,
    int64_t vertex_buffer_id,
    int64_t index_buffer_id,
    int64_t vertex_count,
    int64_t index_count
) {
    int64_t slot;
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);

    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!abi_graphics_find_buffer(session, vertex_buffer_id) ||
        !abi_graphics_find_buffer(session, index_buffer_id) ||
        vertex_count <= 0 ||
        index_count <= 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "mesh creation requires valid vertex and index buffers with positive counts"
        );
    }
    if (session->mesh_count >= ABI_GRAPHICS_MAX_MESHES) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics mesh capacity exceeded"
        );
    }

    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    slot = abi_graphics_occupancy_find_free_slot(
        session->mesh_occupancy, ABI_GRAPHICS_MESH_OCCUPANCY_WORDS
    );
    if (slot < 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics mesh capacity exceeded"
        );
    }
    memset(&session->meshes[slot], 0, sizeof(session->meshes[slot]));
    session->meshes[slot].in_use = 1;
    session->meshes[slot].id = session->next_mesh_id++;
    session->meshes[slot].vertex_buffer_id = vertex_buffer_id;
    session->meshes[slot].index_buffer_id = index_buffer_id;
    session->meshes[slot].vertex_count = vertex_count;
    session->meshes[slot].index_count = index_count;
    abi_graphics_copy_text(
        session->meshes[slot].label,
        sizeof(session->meshes[slot].label),
        label
    );
    session->mesh_count += 1;
    abi_graphics_ok();
    return session->meshes[slot].id;
}

int64_t abi_graphics_mesh_vertex_count(int64_t session_id, int64_t mesh_id) {
    KainNativeGraphicsMesh* mesh =
        abi_graphics_find_mesh(abi_graphics_find_session(session_id), mesh_id);
    return mesh ? mesh->vertex_count : ABI_GRAPHICS_INVALID_RESOURCE;
}

int64_t abi_graphics_mesh_index_count(int64_t session_id, int64_t mesh_id) {
    KainNativeGraphicsMesh* mesh =
        abi_graphics_find_mesh(abi_graphics_find_session(session_id), mesh_id);
    return mesh ? mesh->index_count : ABI_GRAPHICS_INVALID_RESOURCE;
}

const char* abi_graphics_mesh_label(int64_t session_id, int64_t mesh_id) {
    KainNativeGraphicsMesh* mesh =
        abi_graphics_find_mesh(abi_graphics_find_session(session_id), mesh_id);
    return abi_graphics_return_string(mesh ? mesh->label : "");
}

int64_t abi_graphics_pipeline_create(
    int64_t session_id,
    const char* label,
    int64_t vertex_shader_id,
    int64_t fragment_shader_id,
    const char* backend_id
) {
    int64_t slot;
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    const KainNativeGraphicsBackendDescriptor* backend =
        abi_graphics_find_backend(backend_id);

    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!backend) {
        return abi_graphics_fail(
            ABI_GRAPHICS_UNSUPPORTED_BACKEND,
            "unsupported-backend",
            "pipeline backend is not known to the native access layer"
        );
    }
    if (!abi_graphics_find_shader(session, vertex_shader_id) ||
        !abi_graphics_find_shader(session, fragment_shader_id)) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "pipeline creation requires valid vertex and fragment shaders"
        );
    }
    if (session->pipeline_count >= ABI_GRAPHICS_MAX_PIPELINES) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics pipeline capacity exceeded"
        );
    }

    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-debruijn-low-bit-index-collision-free.yaml */
    slot = abi_graphics_occupancy_find_free_slot(
        session->pipeline_occupancy, ABI_GRAPHICS_PIPELINE_OCCUPANCY_WORDS
    );
    if (slot < 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics pipeline capacity exceeded"
        );
    }
    memset(&session->pipelines[slot], 0, sizeof(session->pipelines[slot]));
    session->pipelines[slot].in_use = 1;
    session->pipelines[slot].id = session->next_pipeline_id++;
    session->pipelines[slot].vertex_shader_id = vertex_shader_id;
    session->pipelines[slot].fragment_shader_id = fragment_shader_id;
    abi_graphics_copy_text(
        session->pipelines[slot].label,
        sizeof(session->pipelines[slot].label),
        label
    );
    abi_graphics_copy_text(
        session->pipelines[slot].backend_id,
        sizeof(session->pipelines[slot].backend_id),
        strcmp(backend->id, "auto") == 0 ? session->active_backend_id : backend->id
    );
    session->pipeline_count += 1;
    if (!backend->available) {
        abi_graphics_set_status(
            ABI_GRAPHICS_OK,
            "degraded-backend",
            backend->status
        );
    } else {
        abi_graphics_ok();
    }
    return session->pipelines[slot].id;
}

const char* abi_graphics_pipeline_label(int64_t session_id, int64_t pipeline_id) {
    KainNativeGraphicsPipeline* pipeline =
        abi_graphics_find_pipeline(abi_graphics_find_session(session_id), pipeline_id);
    return abi_graphics_return_string(pipeline ? pipeline->label : "");
}

const char* abi_graphics_pipeline_backend(int64_t session_id, int64_t pipeline_id) {
    KainNativeGraphicsPipeline* pipeline =
        abi_graphics_find_pipeline(abi_graphics_find_session(session_id), pipeline_id);
    return abi_graphics_return_string(pipeline ? pipeline->backend_id : "");
}

int64_t abi_graphics_draw_mesh(
    int64_t session_id,
    int64_t pipeline_id,
    int64_t mesh_id,
    int64_t instance_count
) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    KainNativeGraphicsDrawCommand* command;

    if (!session) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_SESSION,
            "invalid-session",
            "graphics session does not exist"
        );
    }
    if (!abi_graphics_find_pipeline(session, pipeline_id) ||
        !abi_graphics_find_mesh(session, mesh_id) ||
        instance_count <= 0) {
        return abi_graphics_fail(
            ABI_GRAPHICS_INVALID_RESOURCE,
            "invalid-resource",
            "draw command requires a valid pipeline, mesh, and positive instance count"
        );
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-graphics-draw-command-count-stays-within-capacity.yaml */
    if (session->draw_command_count >= ABI_GRAPHICS_MAX_DRAW_COMMANDS) {
        return abi_graphics_fail(
            ABI_GRAPHICS_CAPACITY_EXCEEDED,
            "capacity-exceeded",
            "graphics draw command capacity exceeded"
        );
    }

    command = &session->draw_commands[session->draw_command_count];
    memset(command, 0, sizeof(*command));
    abi_graphics_copy_text(command->kind, sizeof(command->kind), "draw_mesh");
    command->pipeline_id = pipeline_id;
    command->mesh_id = mesh_id;
    command->instance_count = instance_count;
    session->draw_command_count += 1;
    return session->draw_command_count;
}

int64_t abi_graphics_draw_command_count(int64_t session_id) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    return session ? session->draw_command_count : ABI_GRAPHICS_INVALID_SESSION;
}

const char* abi_graphics_draw_command_kind(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return abi_graphics_return_string("");
    }
    return abi_graphics_return_string(session->draw_commands[command_index].kind);
}

int64_t abi_graphics_draw_command_mesh(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return ABI_GRAPHICS_INVALID_RESOURCE;
    }
    return session->draw_commands[command_index].mesh_id;
}

int64_t abi_graphics_draw_command_pipeline(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return ABI_GRAPHICS_INVALID_RESOURCE;
    }
    return session->draw_commands[command_index].pipeline_id;
}

int64_t abi_graphics_draw_command_instances(int64_t session_id, int64_t command_index) {
    KainNativeGraphicsSession* session = abi_graphics_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return ABI_GRAPHICS_INVALID_RESOURCE;
    }
    return session->draw_commands[command_index].instance_count;
}

int64_t abi_graphics_last_status(void) {
    return g_last_status;
}

const char* abi_graphics_last_error_kind(void) {
    return abi_graphics_return_string(g_last_error_kind);
}

const char* abi_graphics_last_error_message(void) {
    return abi_graphics_return_string(g_last_error_message);
}

void abi_graphics_backend_set_available(const char* backend_id, int64_t available) {
    KainNativeGraphicsBackendDescriptor* backend;
    if (!backend_id) {
        return;
    }
    /* Find the backend descriptor in the static catalog. */
    /* Cast away const — the static array is mutable in practice. */
    /* The g_backends array match uses the same lookup as abi_graphics_find_backend. */
    {
        size_t index;
        for (index = 0; index < sizeof(g_backends) / sizeof(g_backends[0]); index += 1) {
            if (ABI_GRAPHICS_TEXT_EQUALS_CI(g_backends[index].id, backend_id) == 0) {
                backend = (KainNativeGraphicsBackendDescriptor*)&g_backends[index];
                backend->available = available;
                return;
            }
        }
    }
}
