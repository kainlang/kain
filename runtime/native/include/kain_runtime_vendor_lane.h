#ifndef KAIN_RUNTIME_VENDOR_LANE_H
#define KAIN_RUNTIME_VENDOR_LANE_H

#include "kain_runtime_services.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KAIN Native Vendor Lane
 *
 * This header exposes a Kain-owned catalog for third-party runtime engines
 * staged under runtime/thirdparty. The lane is intentionally additive: it
 * defines descriptors and function-table pointers without mutating the live
 * service registry, contract, or manifest layers.
 */

#define KAIN_VENDOR_SERVICE_KEY_IO_LOOP           "io.loop"
#define KAIN_VENDOR_SERVICE_KEY_IO_FS             "io.fs"
#define KAIN_VENDOR_SERVICE_KEY_IO_NET            "io.net"
#define KAIN_VENDOR_SERVICE_KEY_IO_PROCESS        "io.process"
#define KAIN_VENDOR_SERVICE_KEY_IO_TIMERS         "io.timers"
#define KAIN_VENDOR_SERVICE_KEY_SCRIPT_QUICKJS    "script.quickjs"
#define KAIN_VENDOR_SERVICE_KEY_AUDIO_BACKEND     "audio.backend"
#define KAIN_VENDOR_SERVICE_KEY_AUDIO_GRAPH       "audio.graph"
#define KAIN_VENDOR_SERVICE_KEY_AUDIO_DEVICE      "audio.device"
#define KAIN_VENDOR_SERVICE_KEY_AUDIO_ASSETS      "audio.assets"
#define KAIN_VENDOR_SERVICE_KEY_WASM_RUNTIME_LIGHT "wasm.runtime.light"
#define KAIN_VENDOR_SERVICE_KEY_WASM_MODULE_LIGHT  "wasm.module.light"
#define KAIN_VENDOR_SERVICE_KEY_WASM_WASI_LIGHT    "wasm.wasi.light"
#define KAIN_VENDOR_SERVICE_KEY_WASM_RUNTIME_FULL  "wasm.runtime.full"
#define KAIN_VENDOR_SERVICE_KEY_WASM_MODULE_FULL   "wasm.module.full"
#define KAIN_VENDOR_SERVICE_KEY_WASM_WASI_FULL     "wasm.wasi.full"
#define KAIN_VENDOR_SERVICE_KEY_ALLOCATOR_MIMALLOC "allocator.mimalloc"
#define KAIN_VENDOR_SERVICE_KEY_ALLOCATOR_RPMALLOC "allocator.rpmalloc"

typedef enum {
    KAIN_VENDOR_SERVICE_FAMILY_UNKNOWN = 0,
    KAIN_VENDOR_SERVICE_FAMILY_LIBUV,
    KAIN_VENDOR_SERVICE_FAMILY_QUICKJS,
    KAIN_VENDOR_SERVICE_FAMILY_MINIAUDIO,
    KAIN_VENDOR_SERVICE_FAMILY_WASM3,
    KAIN_VENDOR_SERVICE_FAMILY_WAMR,
    KAIN_VENDOR_SERVICE_FAMILY_MIMALLOC,
    KAIN_VENDOR_SERVICE_FAMILY_RPMALLOC,
    KAIN_VENDOR_SERVICE_FAMILY_COUNT
} KainVendorServiceFamily;

typedef struct {
    KainVendorServiceFamily family;
    const char* family_name;
    const char* vendor_name;
    KainServiceDescriptor descriptor;
} KainVendorServiceDescriptor;

typedef struct {
    const KainVendorServiceDescriptor* services;
    size_t service_count;
} KainVendorServiceCatalog;

const KainVendorServiceCatalog* kain_vendor_service_catalog(void);
size_t kain_vendor_service_count(void);
const KainVendorServiceDescriptor* kain_vendor_service_at(size_t index);
const KainVendorServiceDescriptor* kain_vendor_service_lookup(const char* service_key);
const KainVendorServiceDescriptor* kain_vendor_service_family_lookup(KainVendorServiceFamily family);
const KainServiceDescriptor* kain_vendor_service_runtime_descriptor(const char* service_key);
const void* kain_vendor_service_function_table(const char* service_key);
const char* kain_vendor_service_family_name(KainVendorServiceFamily family);
const char* kain_vendor_service_vendor_name(KainVendorServiceFamily family);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RUNTIME_VENDOR_LANE_H */
