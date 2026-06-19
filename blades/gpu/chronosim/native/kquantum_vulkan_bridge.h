#ifndef KQUANTUM_VULKAN_BRIDGE_H
#define KQUANTUM_VULKAN_BRIDGE_H

#include <stdint.h>

#if defined(_WIN32)
#define KQV_EXPORT __declspec(dllexport)
#else
#define KQV_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

KQV_EXPORT int32_t kqvulkan_probe(void);
KQV_EXPORT int64_t kqvulkan_frames_presented(void);
KQV_EXPORT int64_t kqvulkan_particles_drawn(void);

KQV_EXPORT int32_t kqvulkan_run_particle_window(
    const char* title,
    int32_t width,
    int32_t height,
    int64_t particle_count,
    int32_t frame_budget,
    int32_t mode,
    const char* vertex_spv_path,
    const char* fragment_spv_path
);

KQV_EXPORT int32_t kqvulkan_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
