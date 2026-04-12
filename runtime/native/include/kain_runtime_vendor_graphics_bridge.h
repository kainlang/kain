#ifndef KAIN_RUNTIME_VENDOR_GRAPHICS_BRIDGE_H
#define KAIN_RUNTIME_VENDOR_GRAPHICS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

const char* kain_vendor_bgfx_version_string(void);
int kain_vendor_bgfx_probe(void);

const char* kain_vendor_filament_version_string(void);
int kain_vendor_filament_probe(void);

const char* kain_vendor_diligent_version_string(void);
int kain_vendor_diligent_probe(void);

const char* kain_vendor_forge_version_string(void);
int kain_vendor_forge_probe(void);

const char* kain_vendor_bimg_version_string(void);
int kain_vendor_bimg_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RUNTIME_VENDOR_GRAPHICS_BRIDGE_H */
