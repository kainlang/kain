#ifndef GPU_SURFACE_EXTENSION_H
#define GPU_SURFACE_EXTENSION_H

#include <stdint.h>

typedef struct KainGpuSurfaceExtension {
    /// Load a fragment shader from hex-encoded SPIR-V.
    /// Creates render pass, descriptor set layout, pipeline layout,
    /// graphics pipeline (with embedded fullscreen-triangle VS),
    /// descriptor pool, uniform buffers, and descriptor writes.
    /// Returns 0 on success, negative on error.
    int64_t (*load_shader)(int64_t session_id, const char* spirv_hex);

    /// Update a uniform buffer binding before the next frame.
    /// binding: 0=time (Float, 4 bytes), 1=resolution (Vec2, 8 bytes), 2=mouse (Vec2, 8 bytes)
    /// data: pointer to the raw bytes
    /// size: byte count
    /// Returns 0 on success, negative on error.
    int64_t (*set_uniform)(int64_t session_id, uint32_t binding,
                            const void* data, uint64_t size);
} KainGpuSurfaceExtension;

#endif /* GPU_SURFACE_EXTENSION_H */
