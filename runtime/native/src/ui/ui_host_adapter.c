// ============================================================================
//  ui_host_adapter.c — Host adapter: dispatches abi_ui_* calls to platform
//  backends through the kainHostVTable (Phase 1 refactor).
// ============================================================================
//  Win32 GDI code was extracted to kain/kain_host_win32.c as part of Phase 1
//  C substrate extraction (P1-C-012). This file now delegates window creation,
//  event pumping, framebuffer access, and rendering to the kainHostVTable.
//
//  ABI surface (abi_ui_*) is UNCHANGED — existing blades MUST still work.
// ============================================================================

#include "ui_host_adapter.h"
#include "ui_system_internal.h"
#include "kain/kain_host.h"
#include "../../include/input_system.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ── Win32-specific helpers (defined in kain/kain_host_win32.c) ─────
// These give the framebuffer accessors direct access to Win32 host state
// fields that aren't exposed through the generic vtable.
#ifdef _WIN32
extern uint32_t* kain_win32_framebuffer_ptr(void* state);
extern int       kain_win32_framebuffer_width(void* state);
extern int       kain_win32_framebuffer_height(void* state);
extern int       kain_win32_framebuffer_stride_elems(void* state);
extern void*     kain_win32_hwnd(void* state);
extern int       kain_win32_is_running(void* state);
extern int64_t   kain_win32_input_session_id(void* state);
extern void      kain_win32_set_session_id(void* state, int64_t sid);
extern void      kain_win32_set_input_session_id(void* state, int64_t sid);
#endif

static int64_t abi_ui_host_adapter_attach_passive(
    KainNativeUiSession* session,
    const char* resolved_backend_id
) {
    if (!session || !resolved_backend_id || !resolved_backend_id[0]) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    session->host_attached = 1;
    session->host_state = NULL;
    snprintf(session->host_backend, sizeof(session->host_backend), "%s", resolved_backend_id);
    return ABI_UI_OK;
}

int abi_ui_host_adapter_is_live_backend(const char* backend_id) {
    if (!backend_id) return 0;
    if (strcmp(backend_id, "winit") == 0) return 1;
    if (strcmp(backend_id, "vulkan") == 0) return 1;
    if (strcmp(backend_id, "d3d12") == 0) return 1;
    if (strcmp(backend_id, "webgpu") == 0) return 1;
    (void)backend_id;
    return 0;
}

int64_t abi_ui_host_adapter_attach(KainNativeUiSession* session, const char* backend_id) {
    if (!session || !backend_id || !backend_id[0]) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (strcmp(backend_id, "auto") == 0) {
        return abi_ui_host_adapter_attach_passive(session, "software");
    }
    if (strcmp(backend_id, "headless") == 0 ||
        strcmp(backend_id, "memory") == 0 ||
        strcmp(backend_id, "software") == 0) {
        return abi_ui_host_adapter_attach_passive(session, backend_id);
    }
    if (strcmp(backend_id, "winit") == 0) {
        const kainHostVTable* vt = kain_host_get(KAIN_HOST_WIN32);
        if (!vt) return ABI_UI_INVALID_ARGUMENT;

        void* host_state = vt->window_create(
            session->window_title,
            (int)session->width,
            (int)session->height);
        if (!host_state) return ABI_UI_INVALID_ARGUMENT;

        // Store session ID on the host for WNDPROC callback context
        kain_win32_set_session_id(host_state, session->id);

        // Create a companion input session for OS → input event bridge
        int64_t input_sid = abi_input_session_create(session->app_name);
        kain_win32_set_input_session_id(host_state, input_sid);

        session->host_state = host_state;
        session->host_attached = 1;
        snprintf(session->host_backend, sizeof(session->host_backend), "%s",
                 vt->backend_id());

        // Sync session dimensions with actual DPI-scaled client rect
        int actual_w = 0, actual_h = 0;
        vt->window_get_size(host_state, &actual_w, &actual_h);
        session->width  = actual_w;
        session->height = actual_h;
        session->dpi_scale = (double)vt->window_get_dpi(host_state);

        return ABI_UI_OK;
    }
    if (strcmp(backend_id, "vulkan") == 0) {
        const KainComponentSurface* surface =
            kain_component_surface_resolve("vulkan");
        if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;
        int64_t vulkan_session = surface->session_create(
            session->window_title, session->width, session->height);
        if (vulkan_session < 0) return ABI_UI_CAPACITY_EXCEEDED;
        session->host_backend[0] = '\0';
        snprintf(session->host_backend, sizeof(session->host_backend), "vulkan");
        session->component_surface = surface;
        session->component_session_id = vulkan_session;
        session->host_attached = 1;
        return ABI_UI_OK;
    }
    if (strcmp(backend_id, "d3d12") == 0) {
        const KainComponentSurface* surface =
            kain_component_surface_resolve("d3d12");
        if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;
        int64_t d3d12_session = surface->session_create(
            session->window_title, session->width, session->height);
        if (d3d12_session < 0) return ABI_UI_CAPACITY_EXCEEDED;
        session->host_backend[0] = '\0';
        snprintf(session->host_backend, sizeof(session->host_backend), "d3d12");
        session->component_surface = surface;
        session->component_session_id = d3d12_session;
        session->host_attached = 1;
        return ABI_UI_OK;
    }
    if (strcmp(backend_id, "webgpu") == 0) {
        const KainComponentSurface* surface =
            kain_component_surface_resolve("webgpu");
        if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;
        int64_t webgpu_session = surface->session_create(
            session->window_title, session->width, session->height);
        if (webgpu_session < 0) return ABI_UI_CAPACITY_EXCEEDED;
        session->host_backend[0] = '\0';
        snprintf(session->host_backend, sizeof(session->host_backend), "webgpu");
        session->component_surface = surface;
        session->component_session_id = webgpu_session;
        session->host_attached = 1;
        return ABI_UI_OK;
    }
    return ABI_UI_INVALID_ARGUMENT;
}

int64_t abi_ui_host_adapter_pump(KainNativeUiSession* session) {
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->host_state && strcmp(session->host_backend, "winit") == 0) {
        void* host_state = session->host_state;
        const kainHostVTable* vt = kain_host_get(KAIN_HOST_WIN32);
        if (vt) {
            vt->pump_events(host_state);
            if (vt->should_close(host_state)) {
                session->host_should_close = 1;
            }
        }
        // Process pending input events for this frame
        int64_t input_sid = kain_win32_input_session_id(host_state);
        if (input_sid > 0) {
            double delta = session->last_delta_ms > 0.0
                               ? session->last_delta_ms : 16.67;
            abi_input_begin_frame(input_sid, delta);
        }
    }
    return ABI_UI_OK;
}

int64_t abi_ui_host_adapter_present(KainNativeUiSession* session) {
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->component_surface != NULL) {
        session->component_surface->present(session->component_session_id);
    }
    if (session->host_state && strcmp(session->host_backend, "winit") == 0) {
        const kainHostVTable* vt = kain_host_get(KAIN_HOST_WIN32);
        if (vt) {
            vt->present(session->host_state, (void*)session);
        }
    }
    return ABI_UI_OK;
}

// ── Framebuffer accessors for direct pixel rendering from Kain ──────
// These expose the DIB framebuffer to Kain code so it can write pixels
// directly, bypassing the node tree renderer and layout engine.

int64_t abi_ui_framebuffer_ptr(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    uint32_t* fb = kain_win32_framebuffer_ptr(session->host_state);
    return (int64_t)(uintptr_t)fb;
}

int64_t abi_ui_framebuffer_width(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    return kain_win32_framebuffer_width(session->host_state);
}

int64_t abi_ui_framebuffer_height(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    return kain_win32_framebuffer_height(session->host_state);
}

int64_t abi_ui_framebuffer_stride(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    return kain_win32_framebuffer_stride_elems(session->host_state);
}

int64_t abi_ui_invalidate_window(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return -1;
    void* hwnd = kain_win32_hwnd(session->host_state);
    if (!hwnd) return -1;
#ifdef _WIN32
    InvalidateRect((HWND)hwnd, NULL, FALSE);
#else
    (void)hwnd;
#endif
    return 0;
}

void abi_ui_host_adapter_shutdown(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
    if (session->component_surface != NULL && session->component_session_id > 0) {
        session->component_surface->session_destroy(session->component_session_id);
        session->component_surface = NULL;
        session->component_session_id = 0;
    }
    if (session->host_state && strcmp(session->host_backend, "winit") == 0) {
        // Destroy the companion input session
        int64_t input_sid = kain_win32_input_session_id(session->host_state);
        if (input_sid > 0) {
            abi_input_session_destroy(input_sid);
        }
        // Destroy the window via the vtable
        const kainHostVTable* vt = kain_host_get(KAIN_HOST_WIN32);
        if (vt) {
            vt->window_destroy(session->host_state);
        }
    }
    session->host_state = NULL;
}

int abi_ui_host_adapter_clipboard_set_text(KainNativeUiSession* session, const char* text) {
    if (!session) {
        return 0;
    }
    (void)text;
    return 0;
}

int abi_ui_host_adapter_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
) {
    if (!session) {
        return 0;
    }
    (void)out_text;
    (void)out_text_cap;
    return 0;
}
