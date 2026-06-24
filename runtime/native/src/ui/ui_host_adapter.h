#ifndef ABI_UI_HOST_ADAPTER_H
#define ABI_UI_HOST_ADAPTER_H

#include "ui_system_internal.h"

#include <stddef.h>
#include <stdint.h>

int abi_ui_host_adapter_is_live_backend(const char* backend_id);
int64_t abi_ui_host_adapter_attach(KainNativeUiSession* session, const char* backend_id);
int64_t abi_ui_host_adapter_pump(KainNativeUiSession* session);
int64_t abi_ui_host_adapter_present(KainNativeUiSession* session);
void abi_ui_host_adapter_shutdown(KainNativeUiSession* session);
int abi_ui_host_adapter_clipboard_set_text(KainNativeUiSession* session, const char* text);
int abi_ui_host_adapter_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
);

// ── Framebuffer accessors ────────────────────────────────────
// Expose DIB framebuffer metadata so Kain can write pixels directly.
// Returns 0 on error (session not found, no winit host attached).
int64_t abi_ui_framebuffer_ptr(int64_t session_id);
int64_t abi_ui_framebuffer_width(int64_t session_id);
int64_t abi_ui_framebuffer_height(int64_t session_id);
int64_t abi_ui_framebuffer_stride(int64_t session_id);

// ── Window invalidation ────────────────────────────────────
// Trigger WM_PAINT to BitBlt the framebuffer to screen.
// Returns 0 on success, -1 on invalid session.
int64_t abi_ui_invalidate_window(int64_t session_id);

#endif /* ABI_UI_HOST_ADAPTER_H */
