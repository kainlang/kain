#include "kain_native_ui_host_adapter.h"

#include <stdio.h>
#include <string.h>

static int64_t kain_native_ui_host_adapter_attach_passive(
    KainNativeUiSession* session,
    const char* resolved_backend_id
) {
    if (!session || !resolved_backend_id || !resolved_backend_id[0]) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    session->host_attached = 1;
    session->host_state = NULL;
    snprintf(session->host_backend, sizeof(session->host_backend), "%s", resolved_backend_id);
    return KAIN_NATIVE_UI_OK;
}

int kain_native_ui_host_adapter_is_live_backend(const char* backend_id) {
    (void)backend_id;
    return 0;
}

int64_t kain_native_ui_host_adapter_attach(KainNativeUiSession* session, const char* backend_id) {
    if (!session || !backend_id || !backend_id[0]) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (strcmp(backend_id, "auto") == 0) {
        return kain_native_ui_host_adapter_attach_passive(session, "software");
    }
    if (strcmp(backend_id, "headless") == 0 ||
        strcmp(backend_id, "memory") == 0 ||
        strcmp(backend_id, "software") == 0) {
        return kain_native_ui_host_adapter_attach_passive(session, backend_id);
    }
    return KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_host_adapter_pump(KainNativeUiSession* session) {
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_host_adapter_present(KainNativeUiSession* session) {
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    return KAIN_NATIVE_UI_OK;
}

void kain_native_ui_host_adapter_shutdown(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
    session->host_state = NULL;
}

int kain_native_ui_host_adapter_clipboard_set_text(KainNativeUiSession* session, const char* text) {
    if (!session) {
        return 0;
    }
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
    (void)out_text;
    (void)out_text_cap;
    return 0;
}
