#ifndef KAIN_RUNTIME_VENDOR_UI_BRIDGE_H
#define KAIN_RUNTIME_VENDOR_UI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

const char* kain_vendor_imgui_version_string(void);
int kain_vendor_imgui_probe(void);

const char* kain_vendor_yoga_version_string(void);
int kain_vendor_yoga_probe(void);

const char* kain_vendor_rmlui_version_string(void);
int kain_vendor_rmlui_probe(void);

const char* kain_vendor_skia_version_string(void);
int kain_vendor_skia_probe(void);

const char* kain_vendor_slint_version_string(void);
int kain_vendor_slint_probe(void);

const char* kain_vendor_qt_version_string(void);
int kain_vendor_qt_probe(void);

const char* kain_vendor_cef_version_string(void);
int kain_vendor_cef_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RUNTIME_VENDOR_UI_BRIDGE_H */
