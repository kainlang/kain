#ifndef KAIN_WEBGPU_ABI_H
#define KAIN_WEBGPU_ABI_H

// ============================================================================
//  webgpu_abi.h — Public header for libkain-webgpu-abi.so/.dll
// ============================================================================
//  This is the separately-linked WebGPU ABI library. It owns ALL actual
//  wgpu-native (or browser WebGPU on WASM) calls: instance creation, adapter
//  request, device creation, surface creation, swapchain lifecycle, frame
//  submission, and present.
//
//  The runtime shim (webgpu_surface_shim.c) dlopens this library on native
//  targets, or links it statically on WASM, and calls
//  kain_webgpu_abi_get_vtable() to obtain a filled KainComponentSurface vtable.
//
//  This library NEVER includes <webgpu/webgpu.h>, <wgpu.h>, or any
//  wgpu-native SDK header. Everything is dynamically resolved via the
//  WGPU C API declared in webgpu_loader_subset.h.
// ============================================================================

#include "../../include/webgpu_loader_subset.h"
#include "../../include/component_surface.h"

#define KAIN_WEBGPU_ABI_VERSION          1
#define KAIN_WEBGPU_MAX_SESSIONS         4
#define KAIN_WEBGPU_STATUS_MESSAGE_MAX   512

// ── Public vtable struct — MUST match webgpu_surface_shim.c exactly ────────

typedef struct KainWebgpuAbiVtable {
    KainComponentSurface surface;
    int64_t              abi_version;
    int64_t              present_count;
    int64_t              last_status;
    char                 last_error[KAIN_WEBGPU_STATUS_MESSAGE_MAX];
} KainWebgpuAbiVtable;

// ── Per-session WebGPU state (lives in the library, not the shim) ──────────

typedef struct KainWebgpuSession {
    int64_t            session_id;
    const char*        name;
    int64_t            width;
    int64_t            height;
    int64_t            should_close;

    WGPUInstance       instance;
    WGPUAdapter        adapter;
    WGPUDevice         device;
    WGPUQueue          queue;
    WGPUSurface        surface;
    WGPUSwapChain      swapchain;

    WGPUCommandEncoder command_encoder;
    WGPURenderPassEncoder render_pass;
    WGPUCommandBuffer  command_buffer;

    /* Platform handle captured by session_attach_platform */
#ifdef _WIN32
    void*              hwnd;
    void*              hinstance;
#endif
#ifdef __linux__
    void*              x11_display;
    uintptr_t          x11_window;
#endif
#ifdef __APPLE__
    void*              metal_layer;
#endif
#ifdef __wasm__
    void*              canvas_selector;       /* CSS selector string, borrowed */
#endif

    int                initialized;
    int                has_frame_in_flight;
} KainWebgpuSession;

// ── The ONLY entry point exposed to the runtime shim ───────────────────────

const KainWebgpuAbiVtable* kain_webgpu_abi_get_vtable(void);

// ── Optional: explicit init/shutdown for blade-level control ───────────────

int  kain_webgpu_abi_init(void);
void kain_webgpu_abi_shutdown(void);

#endif // KAIN_WEBGPU_ABI_H
