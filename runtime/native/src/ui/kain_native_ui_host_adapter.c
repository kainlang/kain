#include "kain_native_ui_host_adapter.h"

#include <string.h>

#ifdef _WIN32
int64_t kain_native_ui_win32_gl_attach(KainNativeUiSession* session, const char* backend_id);
int64_t kain_native_ui_win32_gl_pump(KainNativeUiSession* session);
int64_t kain_native_ui_win32_gl_present(KainNativeUiSession* session);
void kain_native_ui_win32_gl_shutdown(KainNativeUiSession* session);
int kain_native_ui_win32_gl_clipboard_set_text(KainNativeUiSession* session, const char* text);
int kain_native_ui_win32_gl_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
);
#endif

int kain_native_ui_host_adapter_is_live_backend(const char* backend_id) {
#ifdef _WIN32
    return backend_id &&
           (strcmp(backend_id, "win32-gl") == 0 || strcmp(backend_id, "auto") == 0);
#else
    (void)backend_id;
    return 0;
#endif
}

int64_t kain_native_ui_host_adapter_attach(KainNativeUiSession* session, const char* backend_id) {
    if (!session || !backend_id || !backend_id[0]) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    if (strcmp(backend_id, "win32-gl") == 0 || strcmp(backend_id, "auto") == 0) {
        return kain_native_ui_win32_gl_attach(session, backend_id);
    }
#else
    (void)backend_id;
#endif
    return KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_host_adapter_pump(KainNativeUiSession* session) {
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
#ifdef _WIN32
    if (strcmp(session->host_backend, "win32-gl") == 0) {
        return kain_native_ui_win32_gl_pump(session);
    }
#endif
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_host_adapter_present(KainNativeUiSession* session) {
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
#ifdef _WIN32
    if (strcmp(session->host_backend, "win32-gl") == 0) {
        return kain_native_ui_win32_gl_present(session);
    }
#endif
    return KAIN_NATIVE_UI_OK;
}

void kain_native_ui_host_adapter_shutdown(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
#ifdef _WIN32
    if (strcmp(session->host_backend, "win32-gl") == 0 || session->host_state) {
        kain_native_ui_win32_gl_shutdown(session);
    }
#endif
}

int kain_native_ui_host_adapter_clipboard_set_text(KainNativeUiSession* session, const char* text) {
    if (!session) {
        return 0;
    }
#ifdef _WIN32
    if (strcmp(session->host_backend, "win32-gl") == 0) {
        return kain_native_ui_win32_gl_clipboard_set_text(session, text);
    }
#endif
    (void)text;
    return 0;
}

int kain_native_ui_host_adapter_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
) {
    if (!session) {
        return 0;
    }
#ifdef _WIN32
    if (strcmp(session->host_backend, "win32-gl") == 0) {
        return kain_native_ui_win32_gl_clipboard_get_text(session, out_text, out_text_cap);
    }
#endif
    (void)out_text;
    (void)out_text_cap;
    return 0;
}
