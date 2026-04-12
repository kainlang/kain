#ifndef KAIN_RUNTIME_VENDOR_LANE_H
#define KAIN_RUNTIME_VENDOR_LANE_H

#include <stddef.h>

#if defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#define KAIN_VENDOR_HAS_LIBUV 0
#define KAIN_VENDOR_HAS_QUICKJS 0
#define KAIN_VENDOR_HAS_MINIAUDIO 0
#define KAIN_VENDOR_HAS_WASM3 0
#define KAIN_VENDOR_HAS_WAMR 0
#define KAIN_VENDOR_HAS_MIMALLOC 0
#define KAIN_VENDOR_HAS_RPMALLOC 0
#define KAIN_VENDOR_HAS_BGFX 0
#define KAIN_VENDOR_HAS_FILAMENT 0
#define KAIN_VENDOR_HAS_DILIGENT 0
#define KAIN_VENDOR_HAS_BIMG 0
#else
#if defined(__linux__) || defined(_WIN32)
#define KAIN_VENDOR_HAS_LIBUV 1
#else
#define KAIN_VENDOR_HAS_LIBUV 0
#endif
#define KAIN_VENDOR_HAS_QUICKJS 1
#define KAIN_VENDOR_HAS_MINIAUDIO 1
#define KAIN_VENDOR_HAS_WASM3 1
#define KAIN_VENDOR_HAS_WAMR 0
#define KAIN_VENDOR_HAS_MIMALLOC 1
#define KAIN_VENDOR_HAS_RPMALLOC 1
#define KAIN_VENDOR_HAS_BGFX 1
#define KAIN_VENDOR_HAS_FILAMENT 0
#define KAIN_VENDOR_HAS_DILIGENT 0
#define KAIN_VENDOR_HAS_BIMG 0
#endif

typedef struct {
    const char* service_key;
    const char* vendor_name;
    const char* runtime_name;
    const char* (*version_string)(void);
    int (*probe)(void);
    int (*start)(void);
    void (*shutdown)(void);
    int (*poll_once)(int timeout_ms);
    int (*eval_int32)(const char* source, int* out_value);
    void* (*allocate)(size_t size);
    void (*deallocate)(void* memory);
} KainVendorServiceFunctionTable;

typedef struct {
    const char* key;
    const char* family_name;
    const char* vendor_name;
    int available;
    const KainVendorServiceFunctionTable* function_table;
} KainVendorServiceDescriptor;

const KainVendorServiceDescriptor* kain_vendor_service_catalog(void);
size_t kain_vendor_service_count(void);
const KainVendorServiceDescriptor* kain_vendor_service_at(size_t index);
const KainVendorServiceDescriptor* kain_vendor_service_lookup(const char* key);

extern const KainVendorServiceFunctionTable g_kain_vendor_io_loop_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_io_fs_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_io_net_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_io_process_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_io_timers_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_script_quickjs_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_audio_backend_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_audio_graph_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_audio_device_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_audio_assets_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_wasm_runtime_light_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_wasm_runtime_full_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_wasm_module_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_wasm_wasi_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_allocator_mimalloc_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_allocator_rpmalloc_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_bgfx_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_filament_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_diligent_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_asset_image_bimg_service;
extern const KainVendorServiceFunctionTable g_kain_vendor_asset_texture_bimg_service;

#endif /* KAIN_RUNTIME_VENDOR_LANE_H */
