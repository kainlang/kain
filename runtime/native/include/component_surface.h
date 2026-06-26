#ifndef KAIN_COMPONENT_SURFACE_H
#define KAIN_COMPONENT_SURFACE_H

// ============================================================================
//  KainComponentSurface — Surface-agnostic component rendering trait.
// ============================================================================
//  This is the ABI contract between the Kain compiler and any surface backend
//  (native_ui, web, viewport3d, headless, tui, ...). The compiler emits calls
//  through this vtable; the backend implements them. Neither side knows the
//  other's internals.
//
//  Registration: call kain_component_surface_register("name", &surface)
//  Resolution:   call kain_component_surface_resolve("name") -> KainComponentSurface*
//
//  The compiler resolves the surface once at frame-loop init, then calls
//  through the vtable every frame. The trait is surface-agnostic — "kind"
//  strings and style keys are interpreted by the backend.
//
//  Vtable size: 24 slots (0-23). sizeof = 24 * sizeof(void*) = 192 on x64.
// ============================================================================

#include <stdint.h>
#include "gpu_surface_extension.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Platform-native window handle for surface creation. ─────────
// One of these is non-NULL depending on the target platform.
// The platform app host provides this after session_create but before
// the first begin_frame. Compiler codegen never touches this struct.
typedef struct KainPlatformSurfaceHandle {
#ifdef _WIN32
    void*    hinstance;     // HINSTANCE
    void*    hwnd;          // HWND
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    void*    wl_display;    // struct wl_display*
    void*    wl_surface;    // struct wl_surface*
#elif defined(__linux__)
    void*    x11_display;   // Display*
    uintptr_t x11_window;   // Window
#elif defined(__APPLE__)
    void*    metal_layer;   // CAMetalLayer*
#endif
} KainPlatformSurfaceHandle;

typedef struct KainComponentSurface {
    // ── Session lifecycle ──────────────────────────────────────
    int64_t (*session_create) (const char* name, int64_t width, int64_t height);
    void    (*session_destroy)(int64_t session_id);

    // ── Element tree (abstract — "kind" is surface-interpreted) ─
    int64_t (*element_begin)  (int64_t session_id, int64_t parent_id,
                               const char* kind, const char* stable_key);
    void    (*element_end)    (int64_t session_id, int64_t element_id);
    void    (*element_set_text)(int64_t session_id, int64_t element_id,
                                const char* text);

    // ── Style/attribute setters ────────────────────────────────
    void    (*element_set_attr_i64)   (int64_t session_id, int64_t element_id,
                                       const char* key, int64_t value);
    void    (*element_set_attr_f64)   (int64_t session_id, int64_t element_id,
                                       const char* key, double value);
    void    (*element_set_attr_string)(int64_t session_id, int64_t element_id,
                                       const char* key, const char* value);

    // ── State persistence (component `state` survives frames) ───
    int64_t (*state_get_i64)(int64_t session_id, const char* key);
    void    (*state_set_i64)(int64_t session_id, const char* key, int64_t value);

    // ── Frame lifecycle ────────────────────────────────────────
    void    (*begin_frame)(int64_t session_id, double delta_ms);
    void    (*end_frame)  (int64_t session_id);
    void    (*present)    (int64_t session_id);

    // ── Event pump (opaque — surface decodes its own event type) ─
    int64_t (*poll_event)  (int64_t session_id, void* out_event, int64_t max_size);
    int64_t (*should_close)(int64_t session_id);

    // ── Window lifecycle ──────────────────────────────────────
    int64_t (*window_open)(int64_t session_id, const char* title,
                           int64_t width, int64_t height);
    int64_t (*host_pump)  (int64_t session_id);

    // ── Platform handle attachment (called after session_create, ─
    //     before first begin_frame. Codegen emits this with a       ─
    //     zero-initialized handle; backends that own their window   ─
    //     create it here when hwnd is NULL.) ──────────────────────
    void    (*session_attach_platform)(int64_t session_id,
                                       void*   platform_handle);

    //
    // GPU extension discovery (slot 18)
    //
    /// Slot 18: Get the GPU surface extension for this backend.
    /// Returns NULL if the backend does not support GPU operations
    /// (software/GDI backends). GPU backends (Vulkan, D3D12, WebGPU)
    /// return a pointer to a KainGpuSurfaceExtension with at least
    /// load_shader and set_uniform populated.
    const KainGpuSurfaceExtension* (*get_gpu_extension)(int64_t session_id);

    // ── Expanded state persistence (slots 19-22) ────────────────
    // Float and String state complement the i64 state (slots 8-9).
    // These enable slider values, text inputs, animation progress,
    // and any typed component state beyond i64 counters.

    /// Slot 19: Get float state value. Returns fallback (NaN sentinel
    /// recommended) when key is not found.
    double (*state_get_f64)(int64_t session_id, const char* key);

    /// Slot 20: Set float state value.
    void   (*state_set_f64)(int64_t session_id, const char* key, double value);

    /// Slot 21: Get string state value. Returns a pointer to the stored
    /// string, or empty string "" when key is not found.
    const char* (*state_get_string)(int64_t session_id, const char* key);

    /// Slot 22: Set string state value. The backend copies the string
    /// into its own storage.
    void   (*state_set_string)(int64_t session_id, const char* key, const char* value);

    /// Slot 23: Bind a callback function pointer to a named event on
    /// an element. The backend stores the callback for later invocation
    /// when the event fires (via abi_ui_node_invoke_callback).
    void   (*element_set_callback)(int64_t session_id, int64_t element_id,
                                    const char* event_name, void* callback_fn);
} KainComponentSurface;

// ── Surface registry ──────────────────────────────────────────
// Called at startup to register surface backends.
// name is a borrowed pointer — caller must keep it alive.
// surface must be non-NULL and all function pointers must be non-NULL.
void kain_component_surface_register(const char* name,
                                     const KainComponentSurface* surface);

// Called by codegen at frame-loop init to resolve the surface for a world.
// Returns NULL if the named surface is not registered.
const KainComponentSurface* kain_component_surface_resolve(const char* name);

// ── Built-in surface backends ─────────────────────────────────
// Registered automatically at runtime init.
// native_ui_surface wraps ui_system.h behind the KainComponentSurface trait.
extern const KainComponentSurface native_ui_surface;

#ifdef __cplusplus
}
#endif

#endif // KAIN_COMPONENT_SURFACE_H
