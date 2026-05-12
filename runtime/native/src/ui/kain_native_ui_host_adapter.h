#ifndef KAIN_NATIVE_UI_HOST_ADAPTER_H
#define KAIN_NATIVE_UI_HOST_ADAPTER_H

#include "kain_native_ui_system_internal.h"

#include <stddef.h>
#include <stdint.h>

int kain_native_ui_host_adapter_is_live_backend(const char* backend_id);
int64_t kain_native_ui_host_adapter_attach(KainNativeUiSession* session, const char* backend_id);
int64_t kain_native_ui_host_adapter_pump(KainNativeUiSession* session);
int64_t kain_native_ui_host_adapter_present(KainNativeUiSession* session);
void kain_native_ui_host_adapter_shutdown(KainNativeUiSession* session);
int kain_native_ui_host_adapter_clipboard_set_text(KainNativeUiSession* session, const char* text);
int kain_native_ui_host_adapter_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
);

#endif /* KAIN_NATIVE_UI_HOST_ADAPTER_H */
