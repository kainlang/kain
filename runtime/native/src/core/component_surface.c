// ============================================================================
//  component_surface.c — Surface registry for KainComponentSurface backends.
// ============================================================================
//  Maps human-readable surface names ("native_ui", "web", ...) to
//  KainComponentSurface vtable pointers. The compiler resolves a surface
//  once at frame-loop init via kain_component_surface_resolve(), then calls
//  through the vtable every frame.
//
//  GPU Backend Routing (2026-06-21):
//    When the RENDERER_BACKEND env var is set to "vulkan", "d3d12", or
//    "webgpu", resolving "native_ui" routes through the corresponding
//    GPU shim → dlopen → ABI library → GPU vtable. The codegen never
//    knows which backend it's talking to — it always calls through the
//    same KainComponentSurface vtable.
//
//  Registration happens at startup — typically from a blade's init function
//  or from the platform app host before the main frame loop begins.
// ============================================================================

#include "component_surface.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ── Forward declarations for GPU backend shims ──────────────────
// These live in src/core/vulkan_surface_shim.c etc. and are
// build-gated behind KAIN_RUNTIME_HAS_* flags.

#ifdef KAIN_RUNTIME_HAS_VULKAN_LOADER
extern int64_t kain_vulkan_surface_shim_resolve(KainComponentSurface* out_surface);
#endif

#ifdef KAIN_RUNTIME_HAS_D3D12
extern int64_t kain_d3d12_surface_shim_resolve(KainComponentSurface* out_surface);
#endif

#ifdef KAIN_RUNTIME_HAS_WEBGPU
extern int64_t kain_webgpu_surface_shim_resolve(KainComponentSurface* out_surface);
#endif

// ── Constants ──────────────────────────────────────────────────

#define KAIN_MAX_SURFACES 16

// ── Registry state ─────────────────────────────────────────────

static struct {
    const char*                name;    // borrowed pointer — caller owns lifetime
    const KainComponentSurface* surface; // borrowed pointer — static or heap, caller owns
} g_surface_registry[KAIN_MAX_SURFACES];

static int g_surface_count = 0;

// ── Helpers ─────────────────────────────────────────────────────

/// Resolve a GPU backend by name via its shim. Returns the vtable
/// pointer on success, NULL if the backend is unavailable or the
/// shim fails to load the ABI library.
static const KainComponentSurface* resolve_gpu_backend(const char* backend_id) {
    if (!backend_id || !backend_id[0]) return NULL;

#ifdef KAIN_RUNTIME_HAS_VULKAN_LOADER
    if (strcmp(backend_id, "vulkan") == 0) {
        KainComponentSurface vsurf;
        if (kain_vulkan_surface_shim_resolve(&vsurf) == 0) {
            // The shim registers itself as "vulkan" in the registry.
            // Return the registered entry so the pointer is stable.
            return kain_component_surface_resolve("vulkan");
        }
        return NULL;
    }
#endif

#ifdef KAIN_RUNTIME_HAS_D3D12
    if (strcmp(backend_id, "d3d12") == 0) {
        KainComponentSurface dsurf;
        if (kain_d3d12_surface_shim_resolve(&dsurf) == 0) {
            return kain_component_surface_resolve("d3d12");
        }
        return NULL;
    }
#endif

#ifdef KAIN_RUNTIME_HAS_WEBGPU
    if (strcmp(backend_id, "webgpu") == 0) {
        KainComponentSurface wsurf;
        if (kain_webgpu_surface_shim_resolve(&wsurf) == 0) {
            return kain_component_surface_resolve("webgpu");
        }
        return NULL;
    }
#endif

    (void)backend_id;
    return NULL;
}

// ── Public API ─────────────────────────────────────────────────

void kain_component_surface_register(const char* name,
                                     const KainComponentSurface* surface) {
    // Safety: reject NULL pointers. The codegen trusts this.
    if (!name || !surface) {
        return;
    }

    // Prevent duplicate registration — silently overwrite if already present.
    for (int i = 0; i < g_surface_count; i++) {
        if (strcmp(g_surface_registry[i].name, name) == 0) {
            g_surface_registry[i].surface = surface;
            return;
        }
    }

    // Append new entry if there is room.
    if (g_surface_count >= KAIN_MAX_SURFACES) {
        return; // registry full — surface not available
    }

    g_surface_registry[g_surface_count].name    = name;
    g_surface_registry[g_surface_count].surface = surface;
    g_surface_count++;
}

const KainComponentSurface* kain_component_surface_resolve(const char* name) {
    if (!name) {
        return NULL;
    }

    // ── GPU backend routing ──────────────────────────────────
    // When codegen asks for "native_ui", check if the user wants
    // a GPU backend via the RENDERER_BACKEND env var.
    // The codegen never knows which backend it gets — it always
    // calls through the same KainComponentSurface vtable.
    if (strcmp(name, "native_ui") == 0) {
        const char* backend = getenv("RENDERER_BACKEND");
        if (backend && backend[0]) {
            const KainComponentSurface* gpu_surface =
                resolve_gpu_backend(backend);
            if (gpu_surface) {
                // Overwrite the registry entry so the GDI backend
                // is never called — all future resolves get the
                // GPU vtable instead.
                kain_component_surface_register("native_ui", gpu_surface);
                return gpu_surface;
            }
            // GPU backend requested but unavailable — fall through
            // to the GDI backend below.
        }
    }

    // ── Normal registry lookup ───────────────────────────────
    for (int i = 0; i < g_surface_count; i++) {
        if (strcmp(g_surface_registry[i].name, name) == 0) {
            return g_surface_registry[i].surface;
        }
    }

    return NULL; // Codegen checks for NULL, emits runtime error.
}

// ============================================================================
//  Runtime helpers — used by component surface codegen
// ============================================================================

/// Called by the codegen when a surface resolution or session creation
/// fails. Logs the message and aborts the process.
void kain_runtime_panic(const char* message) {
    fprintf(stderr, "KAIN RUNTIME PANIC: %s\n", message ? message : "(null)");
    fflush(stderr);
    abort();
}

/// Returns the frame delta in milliseconds since the last frame.
/// For now uses a fixed 16.67 ms (60 FPS). Future: use QueryPerformanceCounter.
double __kain_frame_delta_ms(void) {
    // Fixed-rate stub for testing. Replace with high-resolution timer.
    return 16.67;
}
