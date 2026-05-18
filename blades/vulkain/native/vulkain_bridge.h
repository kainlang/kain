#ifndef VULKAIN_BRIDGE_H
#define VULKAIN_BRIDGE_H

#include <stdint.h>

#if defined(_WIN32)
#define VULKAIN_EXPORT __declspec(dllexport)
#else
#define VULKAIN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

VULKAIN_EXPORT int32_t vulkain_native_probe(void);
VULKAIN_EXPORT int64_t vulkain_native_frames_presented(void);
VULKAIN_EXPORT int64_t vulkain_native_vertices_drawn(void);

VULKAIN_EXPORT int32_t vulkain_native_run_window(
    const char* title,
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    const char* vertex_spv_path,
    const char* fragment_spv_path
);

VULKAIN_EXPORT int32_t vulkain_native_run_mesh_scene(
    const char* title,
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    int32_t draw_vertices,
    int32_t camera_yaw_milli,
    int32_t camera_pitch_milli,
    int32_t mesh_scale_milli,
    int32_t mesh_twist_milli,
    int32_t depth_bias_milli,
    int32_t energy,
    const char* vertex_spv_path,
    const char* fragment_spv_path
);

VULKAIN_EXPORT int32_t vulkain_native_run_authored_mesh_scene(
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    int32_t draw_vertices,
    int32_t camera_yaw_milli,
    int32_t camera_pitch_milli,
    int32_t mesh_scale_milli,
    int32_t mesh_twist_milli,
    int32_t depth_bias_milli,
    int32_t energy
);

VULKAIN_EXPORT int32_t vulkain_native_write_report(const char* path);
VULKAIN_EXPORT int32_t vulkain_native_write_default_mesh_report(void);

#ifdef __cplusplus
}
#endif

#endif
