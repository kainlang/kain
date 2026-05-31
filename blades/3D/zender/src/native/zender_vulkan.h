#ifndef ZENDER_VULKAN_H
#define ZENDER_VULKAN_H

#include <stdint.h>

#if defined(_WIN32)
#define ZV_EXPORT __declspec(dllexport)
#else
#define ZV_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

ZV_EXPORT int32_t zv_probe(void);
ZV_EXPORT const char* zv_backend_name(void);
ZV_EXPORT const char* zv_last_error(void);
ZV_EXPORT int64_t zv_frames_presented(void);
ZV_EXPORT int64_t zv_particles_drawn(void);
ZV_EXPORT int32_t zv_glb_probe_file(const char* path);
ZV_EXPORT int64_t zv_glb_byte_len(void);
ZV_EXPORT int32_t zv_glb_version(void);
ZV_EXPORT int32_t zv_glb_json_chunk_len(void);
ZV_EXPORT const char* zv_glb_json_text(void);

ZV_EXPORT int32_t zv_run_window(
    const char* title,
    int32_t width,
    int32_t height,
    int64_t particle_count,
    int32_t frame_budget,
    int32_t mode,
    int32_t sphere_instances,
    int32_t ring_resolution,
    int32_t shell_resolution,
    float orbit_speed,
    float chaos,
    const char* vertex_spv_path,
    const char* fragment_spv_path
);

ZV_EXPORT int32_t zv_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
