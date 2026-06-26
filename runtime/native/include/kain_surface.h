#ifndef KAIN_SURFACE_H
#define KAIN_SURFACE_H

// ============================================================================
//  KainSurface — GPU surface abstraction (Phase 1: forward-looking stub)
// ============================================================================
//  Owns the platform framebuffer/swapchain independent of any widget or
//  layout system. In Phase 1 only the software (GDI) backend is active;
//  Vulkan, D3D12, and WebGPU slots will be filled in Phase 2.
//
//  Twin header: both src/ui/kain/kain_surface.h and include/kain_surface.h
//  are identical copies. Internal UI code includes the local copy; external
//  consumers (stdlib bridges, blades) include from include/.
// ============================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Backend identifiers ──────────────────────────────────────────────
typedef enum kainSurfaceKind {
    KAIN_SURFACE_SOFTWARE = 0,
    KAIN_SURFACE_VULKAN,
    KAIN_SURFACE_D3D12,
    KAIN_SURFACE_WEBGPU,
} kainSurfaceKind;

// ── Opaque surface handle ──────────────────────────────────────────
// Defined in the backend .c file (software, vulkan, d3d12, webgpu).
// Consumers only hold a pointer; all mutation goes through the API.
typedef struct kainSurface kainSurface;

// ── Lifecycle ───────────────────────────────────────────────────────
kainSurface* kain_surface_create(int width, int height, kainSurfaceKind kind);
void         kain_surface_destroy(kainSurface* s);

// ── Resize (reallocates framebuffer/swapchain) ─────────────────────
void kain_surface_resize(kainSurface* s, int width, int height);

// ── Pixel access (software path only; NULL for GPU backends) ──────
// Returns a pointer to the raw pixel buffer. For software surfaces this
// is the DIB framebuffer; for GPU surfaces this returns NULL (pixels
// live on the GPU and must be read back explicitly).
uint32_t* kain_surface_pixels(kainSurface* s, int* out_width,
                               int* out_height, int* out_stride);

// ── Backend query ──────────────────────────────────────────────────
kainSurfaceKind kain_surface_backend(kainSurface* s);
int             kain_surface_width(kainSurface* s);
int             kain_surface_height(kainSurface* s);

// ── Backend name (for diagnostics) ────────────────────────────────
const char* kain_surface_kind_name(kainSurfaceKind kind);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_SURFACE_H */
