#ifndef KAIN_NATIVE_GRAPHICS_SYSTEM_H
#define KAIN_NATIVE_GRAPHICS_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_NATIVE_GRAPHICS_MAX_SESSIONS 16
#define KAIN_NATIVE_GRAPHICS_MAX_BUFFERS 4096
#define KAIN_NATIVE_GRAPHICS_MAX_SHADERS 1024
#define KAIN_NATIVE_GRAPHICS_MAX_MESHES 2048
#define KAIN_NATIVE_GRAPHICS_MAX_PIPELINES 1024
#define KAIN_NATIVE_GRAPHICS_MAX_DRAW_COMMANDS 8192
#define KAIN_NATIVE_GRAPHICS_MAX_TEXT 256
#define KAIN_NATIVE_GRAPHICS_MAX_KEY 96

typedef enum KainNativeGraphicsStatus {
    KAIN_NATIVE_GRAPHICS_OK = 0,
    KAIN_NATIVE_GRAPHICS_INVALID_SESSION = -1,
    KAIN_NATIVE_GRAPHICS_INVALID_RESOURCE = -2,
    KAIN_NATIVE_GRAPHICS_CAPACITY_EXCEEDED = -3,
    KAIN_NATIVE_GRAPHICS_INVALID_ARGUMENT = -4,
    KAIN_NATIVE_GRAPHICS_UNSUPPORTED_BACKEND = -5,
} KainNativeGraphicsStatus;

int64_t kain_native_graphics_reset(void);

int64_t kain_native_graphics_session_create(const char* app_name, int64_t width, int64_t height);
int64_t kain_native_graphics_session_destroy(int64_t session_id);
int64_t kain_native_graphics_session_count(void);

int64_t kain_native_graphics_backend_supported(const char* backend_id);
int64_t kain_native_graphics_backend_available(const char* backend_id);
const char* kain_native_graphics_backend_status(const char* backend_id);
int64_t kain_native_graphics_backend_select(int64_t session_id, const char* backend_id);
const char* kain_native_graphics_active_backend(int64_t session_id);

int64_t kain_native_graphics_begin_frame(int64_t session_id, double delta_ms);
int64_t kain_native_graphics_end_frame(int64_t session_id);
int64_t kain_native_graphics_present(int64_t session_id);
int64_t kain_native_graphics_frame_index(int64_t session_id);
int64_t kain_native_graphics_last_presented_frame(int64_t session_id);

int64_t kain_native_graphics_buffer_create(
    int64_t session_id,
    const char* kind,
    const char* label,
    int64_t byte_length,
    int64_t element_stride
);
int64_t kain_native_graphics_buffer_create_from_hex(
    int64_t session_id,
    const char* kind,
    const char* label,
    const char* bytes_hex,
    int64_t element_stride
);
int64_t kain_native_graphics_buffer_byte_length(int64_t session_id, int64_t buffer_id);
int64_t kain_native_graphics_buffer_byte_at(int64_t session_id, int64_t buffer_id, int64_t byte_offset);
const char* kain_native_graphics_buffer_kind(int64_t session_id, int64_t buffer_id);
const char* kain_native_graphics_buffer_label(int64_t session_id, int64_t buffer_id);

int64_t kain_native_graphics_shader_spirv_from_hex(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* bytes_hex
);
int64_t kain_native_graphics_shader_spirv_from_file(
    int64_t session_id,
    const char* key,
    const char* stage,
    const char* entry_point,
    const char* path
);
int64_t kain_native_graphics_shader_byte_length(int64_t session_id, int64_t shader_id);
int64_t kain_native_graphics_shader_byte_at(int64_t session_id, int64_t shader_id, int64_t byte_offset);
const char* kain_native_graphics_shader_key(int64_t session_id, int64_t shader_id);
const char* kain_native_graphics_shader_stage(int64_t session_id, int64_t shader_id);

int64_t kain_native_graphics_mesh_create(
    int64_t session_id,
    const char* label,
    int64_t vertex_buffer_id,
    int64_t index_buffer_id,
    int64_t vertex_count,
    int64_t index_count
);
int64_t kain_native_graphics_mesh_vertex_count(int64_t session_id, int64_t mesh_id);
int64_t kain_native_graphics_mesh_index_count(int64_t session_id, int64_t mesh_id);
const char* kain_native_graphics_mesh_label(int64_t session_id, int64_t mesh_id);

int64_t kain_native_graphics_pipeline_create(
    int64_t session_id,
    const char* label,
    int64_t vertex_shader_id,
    int64_t fragment_shader_id,
    const char* backend_id
);
const char* kain_native_graphics_pipeline_label(int64_t session_id, int64_t pipeline_id);
const char* kain_native_graphics_pipeline_backend(int64_t session_id, int64_t pipeline_id);

int64_t kain_native_graphics_draw_mesh(
    int64_t session_id,
    int64_t pipeline_id,
    int64_t mesh_id,
    int64_t instance_count
);
int64_t kain_native_graphics_draw_command_count(int64_t session_id);
const char* kain_native_graphics_draw_command_kind(int64_t session_id, int64_t command_index);
int64_t kain_native_graphics_draw_command_mesh(int64_t session_id, int64_t command_index);
int64_t kain_native_graphics_draw_command_pipeline(int64_t session_id, int64_t command_index);
int64_t kain_native_graphics_draw_command_instances(int64_t session_id, int64_t command_index);

int64_t kain_native_graphics_last_status(void);
const char* kain_native_graphics_last_error_kind(void);
const char* kain_native_graphics_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_GRAPHICS_SYSTEM_H */
