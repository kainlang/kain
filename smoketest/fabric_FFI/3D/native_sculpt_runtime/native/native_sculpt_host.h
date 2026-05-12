#ifndef NATIVE_SCULPT_HOST_H
#define NATIVE_SCULPT_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define NSH_EXPORT __declspec(dllexport)
#else
#define NSH_EXPORT
#endif

NSH_EXPORT void* native_sculpt_runtime_create(
    int width,
    int height,
    int radius,
    int intensity,
    int hardness,
    int target_polys,
    const char* title
);

NSH_EXPORT int native_sculpt_runtime_run(void* runtime_handle, int duration_ms, const char* capture_bmp_path);
NSH_EXPORT int native_sculpt_runtime_frame_count(void* runtime_handle);
NSH_EXPORT int native_sculpt_runtime_message_count(void* runtime_handle);
NSH_EXPORT int native_sculpt_runtime_mouse_move_count(void* runtime_handle);
NSH_EXPORT int native_sculpt_runtime_last_brush_x(void* runtime_handle);
NSH_EXPORT int native_sculpt_runtime_last_brush_y(void* runtime_handle);
NSH_EXPORT int native_sculpt_runtime_average_fps_x100(void* runtime_handle);
NSH_EXPORT int native_sculpt_runtime_checksum(void* runtime_handle);
NSH_EXPORT const char* native_sculpt_runtime_signature(void* runtime_handle);
NSH_EXPORT void native_sculpt_runtime_destroy(void* runtime_handle);

#ifdef __cplusplus
}
#endif

#endif
