#ifndef KAIN_HOST_H
#define KAIN_HOST_H

// ============================================================================
//  kainHostVTable — Platform-agnostic host interface vtable
// ============================================================================
//  Each platform backend (Win32 GDI, Vulkan, D3D12, WebGPU, X11, Wayland,
//  macOS, WASM) implements this vtable. The host adapter (ui_host_adapter.c)
//  dispatches through the vtable without knowing concrete platform details.
//
//  In Phase 1 only KAIN_HOST_WIN32 is implemented (via kain_host_win32.c).
//  GPU backends will fill their vtables in Phase 2.
//
//  Twin header: both src/ui/kain/kain_host.h and include/kain_host.h
//  are identical copies.
// ============================================================================

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Platform identifiers ─────────────────────────────────────────────
typedef enum kainHostPlatform {
    KAIN_HOST_UNKNOWN = 0,
    KAIN_HOST_WIN32,
    KAIN_HOST_X11,
    KAIN_HOST_WAYLAND,
    KAIN_HOST_MACOS,
    KAIN_HOST_WASM,
} kainHostPlatform;

// ── Cursor types (forward-looking; only arrow is implemented) ───────
typedef enum kainHostCursor {
    KAIN_CURSOR_ARROW = 0,
    KAIN_CURSOR_IBEAM,
    KAIN_CURSOR_HAND,
    KAIN_CURSOR_RESIZE_NS,
    KAIN_CURSOR_RESIZE_EW,
    KAIN_CURSOR_WAIT,
} kainHostCursor;

// ── Host interface vtable ──────────────────────────────────────────
// All function pointers are non-NULL for a live backend. The host_state
// pointer is opaque to callers — each backend defines its own struct.
typedef struct kainHostVTable {
    // ── Identification ──────────────────────────────────────────
    const char* (*backend_id)(void);
    kainHostPlatform (*platform)(void);

    // ── Window lifecycle ────────────────────────────────────────
    // Returns an opaque host_state pointer, or NULL on failure.
    void* (*window_create)(const char* title, int width, int height);
    void  (*window_destroy)(void* state);

    // ── Window management ───────────────────────────────────────
    void  (*window_set_title)(void* state, const char* title);
    void  (*window_set_size)(void* state, int width, int height);
    void  (*window_get_size)(void* state, int* out_w, int* out_h);
    float (*window_get_dpi)(void* state);

    // ── Message pump ────────────────────────────────────────────
    void  (*pump_events)(void* state);
    int   (*should_close)(void* state);

    // ── Framebuffer access ──────────────────────────────────────
    // Returns the current pixel buffer and its stride (in uint32_t
    // elements). NULL if no framebuffer is available (headless).
    uint32_t* (*get_framebuffer)(void* state, int* out_stride_elems);
    int  (*get_framebuffer_width)(void* state);
    int  (*get_framebuffer_height)(void* state);

    // ── Present ─────────────────────────────────────────────────
    // Renders the current frame and presents it to the screen.
    // For software (Win32 GDI): layout → draw → InvalidateRect.
    // For GPU: records commands → submit → present swapchain.
    // The session parameter is a KainNativeUiSession* (opaque here).
    void  (*present)(void* state, void* session);

    // ── Clipboard ───────────────────────────────────────────────
    int   (*clipboard_set_text)(void* state, const char* text);
    int   (*clipboard_get_text)(void* state, char* out, size_t cap);

    // ── Cursor ──────────────────────────────────────────────────
    void  (*set_cursor)(void* state, kainHostCursor cursor);

    // ── GPU surface extension ───────────────────────────────────
    // Returns a platform-specific GPU surface handle (e.g. VkSurfaceKHR,
    // IDXGISwapChain, WGPUSurface). NULL for software backends.
    void* (*get_gpu_surface)(void* state);
} kainHostVTable;

// ── Host dispatch ──────────────────────────────────────────────────
// Returns the vtable for the given platform, or NULL if unavailable.
const kainHostVTable* kain_host_get(kainHostPlatform platform);

// Convenience: returns the vtable for the current native platform.
const kainHostVTable* kain_host_native(void);

// Returns the current platform kind.
kainHostPlatform kain_host_current_platform(void);

// Human-readable name for a platform kind.
const char* kain_host_platform_name(kainHostPlatform p);

// ── Per-platform vtable declarations ───────────────────────────────
// Each platform .c file defines one of these.
#ifdef _WIN32
extern const kainHostVTable kain_host_win32_vtable;
#endif

#ifdef __cplusplus
}
#endif

#endif /* KAIN_HOST_H */
