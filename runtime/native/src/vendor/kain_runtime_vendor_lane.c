#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "../../include/kain_runtime_vendor_lane.h"
#include "../../include/kain_runtime_base.h"

#include <stddef.h>
#include <string.h>

#include "../../../thirdparty/libuv/include/uv.h"
#include "../../../thirdparty/quickjs/quickjs.h"
#include "../../../thirdparty/miniaudio/miniaudio.h"
#include "../../../thirdparty/wasm3/source/wasm3.h"
#include "../../../thirdparty/wamr/core/iwasm/include/wasm_export.h"
#include "../../../thirdparty/mimalloc/include/mimalloc.h"
#include "../../../thirdparty/rpmalloc/rpmalloc/rpmalloc.h"

typedef struct {
    const char* (*version_string)(void);
    uv_loop_t* (*default_loop)(void);
    int (*loop_init)(uv_loop_t* loop);
    int (*loop_close)(uv_loop_t* loop);
    int (*run)(uv_loop_t* loop, uv_run_mode mode);
    void (*stop)(uv_loop_t* loop);
    int (*async_init)(uv_loop_t* loop, uv_async_t* async, uv_async_cb callback);
    int (*async_send)(uv_async_t* async);
    int (*timer_init)(uv_loop_t* loop, uv_timer_t* timer);
    int (*timer_start)(uv_timer_t* timer, uv_timer_cb callback, uint64_t timeout, uint64_t repeat);
    int (*timer_stop)(uv_timer_t* timer);
} KainVendorLibuvFunctionTable;

typedef struct {
    JSRuntime* (*new_runtime)(void);
    JSContext* (*new_context)(JSRuntime* runtime);
    JSValue (*eval)(JSContext* context, const char* input, size_t input_len, const char* filename, int flags);
    JSValue (*eval_function)(JSContext* context, JSValue function_value);
    void (*set_host_promise_rejection_tracker)(JSRuntime* runtime, JSHostPromiseRejectionTracker* callback, void* opaque);
    void (*set_module_loader_func)(JSRuntime* runtime, JSModuleNormalizeFunc* normalize, JSModuleLoaderFunc* loader, void* opaque);
    void (*free_context)(JSContext* context);
    void (*free_runtime)(JSRuntime* runtime);
} KainVendorQuickJSFunctionTable;

typedef struct {
    ma_engine_config (*engine_config_init)(void);
    ma_result (*engine_init)(const ma_engine_config* config, ma_engine* engine);
    void (*engine_uninit)(ma_engine* engine);
    ma_result (*engine_start)(ma_engine* engine);
    ma_result (*engine_stop)(ma_engine* engine);
    ma_device_config (*device_config_init)(ma_device_type device_type);
    ma_result (*device_init)(ma_context* context, const ma_device_config* config, ma_device* device);
    ma_result (*device_start)(ma_device* device);
    ma_result (*device_stop)(ma_device* device);
    void (*device_uninit)(ma_device* device);
} KainVendorMiniaudioFunctionTable;

typedef struct {
    IM3Environment (*new_environment)(void);
    void (*free_environment)(IM3Environment environment);
    IM3Runtime (*new_runtime)(IM3Environment environment, uint32_t stack_size_in_bytes, void* user_data);
    void (*free_runtime)(IM3Runtime runtime);
    M3Result (*parse_module)(IM3Environment environment, IM3Module* module, const uint8_t* wasm_bytes, uint32_t wasm_byte_count);
    void (*free_module)(IM3Module module);
    M3Result (*load_module)(IM3Runtime runtime, IM3Module module);
    M3Result (*link_raw_function)(IM3Module module, const char* module_name, const char* function_name, const char* signature, M3RawCall function);
    M3Result (*find_function)(IM3Function* function, IM3Runtime runtime, const char* function_name);
    M3Result (*call_v)(IM3Function function, ...);
    void (*get_error_info)(IM3Runtime runtime, M3ErrorInfo* info);
} KainVendorWasm3FunctionTable;

typedef struct {
    bool (*init)(void);
    void (*destroy)(void);
    wasm_module_t (*load)(uint8_t* buffer, uint32_t size, char* error_buf, uint32_t error_buf_size);
    wasm_module_inst_t (*instantiate)(const wasm_module_t module, uint32_t default_stack_size, uint32_t host_managed_heap_size, char* error_buf, uint32_t error_buf_size);
    wasm_function_inst_t (*lookup_function)(const wasm_module_inst_t module_inst, const char* name);
    void (*deinstantiate)(wasm_module_inst_t module_inst);
    void (*unload)(wasm_module_t module);
} KainVendorWamrFunctionTable;

typedef struct {
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t count, size_t size);
    void* (*realloc)(void* pointer, size_t size);
    void (*free)(void* pointer);
    void* (*zalloc)(size_t size);
    void (*collect)(bool force);
    void (*stats_print)(void* out);
    size_t (*usable_size)(const void* pointer);
} KainVendorMimallocFunctionTable;

typedef struct {
    int (*initialize)(rpmalloc_interface_t* memory_interface);
    void (*finalize)(void);
    void (*thread_initialize)(void);
    void (*thread_finalize)(void);
    void (*thread_collect)(void);
    void* (*malloc)(size_t size);
    void* (*realloc)(void* pointer, size_t size);
    void (*free)(void* pointer);
    size_t (*usable_size)(void* pointer);
} KainVendorRpmallocFunctionTable;

static const KainVendorLibuvFunctionTable g_kain_vendor_libuv_functions = {
    uv_version_string,
    uv_default_loop,
    uv_loop_init,
    uv_loop_close,
    uv_run,
    uv_stop,
    uv_async_init,
    uv_async_send,
    uv_timer_init,
    uv_timer_start,
    uv_timer_stop,
};

static const KainVendorQuickJSFunctionTable g_kain_vendor_quickjs_functions = {
    JS_NewRuntime,
    JS_NewContext,
    JS_Eval,
    JS_EvalFunction,
    JS_SetHostPromiseRejectionTracker,
    JS_SetModuleLoaderFunc,
    JS_FreeContext,
    JS_FreeRuntime,
};

static const KainVendorMiniaudioFunctionTable g_kain_vendor_miniaudio_functions = {
    ma_engine_config_init,
    ma_engine_init,
    ma_engine_uninit,
    ma_engine_start,
    ma_engine_stop,
    ma_device_config_init,
    ma_device_init,
    ma_device_start,
    ma_device_stop,
    ma_device_uninit,
};

static const KainVendorWasm3FunctionTable g_kain_vendor_wasm3_functions = {
    m3_NewEnvironment,
    m3_FreeEnvironment,
    m3_NewRuntime,
    m3_FreeRuntime,
    m3_ParseModule,
    m3_FreeModule,
    m3_LoadModule,
    m3_LinkRawFunction,
    m3_FindFunction,
    m3_CallV,
    m3_GetErrorInfo,
};

static const KainVendorWamrFunctionTable g_kain_vendor_wamr_functions = {
    wasm_runtime_init,
    wasm_runtime_destroy,
    wasm_runtime_load,
    wasm_runtime_instantiate,
    wasm_runtime_lookup_function,
    wasm_runtime_deinstantiate,
    wasm_runtime_unload,
};

static const KainVendorMimallocFunctionTable g_kain_vendor_mimalloc_functions = {
    mi_malloc,
    mi_calloc,
    mi_realloc,
    mi_free,
    mi_zalloc,
    mi_collect,
    mi_stats_print,
    mi_malloc_usable_size,
};

static const KainVendorRpmallocFunctionTable g_kain_vendor_rpmalloc_functions = {
    rpmalloc_initialize,
    rpmalloc_finalize,
    rpmalloc_thread_initialize,
    rpmalloc_thread_finalize,
    rpmalloc_thread_collect,
    rpmalloc,
    rprealloc,
    rpfree,
    rpmalloc_usable_size,
};

static const KainVendorServiceDescriptor g_kain_vendor_service_catalog[] = {
    {
        KAIN_VENDOR_SERVICE_FAMILY_LIBUV,
        "libuv",
        "libuv",
        {
            KAIN_VENDOR_SERVICE_KEY_IO_LOOP,
            "Kain IO Loop",
            "Kain-owned event loop surface backed by libuv",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_libuv_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_LIBUV,
        "libuv",
        "libuv",
        {
            KAIN_VENDOR_SERVICE_KEY_IO_FS,
            "Kain IO Filesystem",
            "Kain-owned filesystem surface backed by libuv",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_libuv_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_LIBUV,
        "libuv",
        "libuv",
        {
            KAIN_VENDOR_SERVICE_KEY_IO_NET,
            "Kain IO Network",
            "Kain-owned networking surface backed by libuv",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_libuv_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_LIBUV,
        "libuv",
        "libuv",
        {
            KAIN_VENDOR_SERVICE_KEY_IO_PROCESS,
            "Kain IO Process",
            "Kain-owned process surface backed by libuv",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_libuv_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_LIBUV,
        "libuv",
        "libuv",
        {
            KAIN_VENDOR_SERVICE_KEY_IO_TIMERS,
            "Kain IO Timers",
            "Kain-owned timer surface backed by libuv",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_libuv_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_QUICKJS,
        "QuickJS",
        "QuickJS",
        {
            KAIN_VENDOR_SERVICE_KEY_SCRIPT_QUICKJS,
            "Kain QuickJS Runtime",
            "Kain-owned JavaScript runtime surface backed by QuickJS",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_quickjs_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_MINIAUDIO,
        "miniaudio",
        "miniaudio",
        {
            KAIN_VENDOR_SERVICE_KEY_AUDIO_BACKEND,
            "Kain Audio Backend",
            "Kain-owned audio backend surface backed by miniaudio",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_miniaudio_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_MINIAUDIO,
        "miniaudio",
        "miniaudio",
        {
            KAIN_VENDOR_SERVICE_KEY_AUDIO_GRAPH,
            "Kain Audio Graph",
            "Kain-owned audio graph surface backed by miniaudio",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_miniaudio_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_MINIAUDIO,
        "miniaudio",
        "miniaudio",
        {
            KAIN_VENDOR_SERVICE_KEY_AUDIO_DEVICE,
            "Kain Audio Device",
            "Kain-owned audio device surface backed by miniaudio",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_miniaudio_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_MINIAUDIO,
        "miniaudio",
        "miniaudio",
        {
            KAIN_VENDOR_SERVICE_KEY_AUDIO_ASSETS,
            "Kain Audio Assets",
            "Kain-owned audio asset surface backed by miniaudio",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_miniaudio_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_WASM3,
        "wasm3",
        "wasm3",
        {
            KAIN_VENDOR_SERVICE_KEY_WASM_RUNTIME_LIGHT,
            "Kain wasm3 Runtime",
            "Kain-owned lightweight WASM runtime surface backed by wasm3",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_wasm3_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_WASM3,
        "wasm3",
        "wasm3",
        {
            KAIN_VENDOR_SERVICE_KEY_WASM_MODULE_LIGHT,
            "Kain wasm3 Module",
            "Kain-owned lightweight WASM module surface backed by wasm3",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_wasm3_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_WASM3,
        "wasm3",
        "wasm3",
        {
            KAIN_VENDOR_SERVICE_KEY_WASM_WASI_LIGHT,
            "Kain wasm3 WASI",
            "Kain-owned lightweight WASI surface backed by wasm3",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_wasm3_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_WAMR,
        "WAMR",
        "WAMR",
        {
            KAIN_VENDOR_SERVICE_KEY_WASM_RUNTIME_FULL,
            "Kain WAMR Runtime",
            "Kain-owned full WASM runtime surface backed by WAMR",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_DEGRADED,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_wamr_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_WAMR,
        "WAMR",
        "WAMR",
        {
            KAIN_VENDOR_SERVICE_KEY_WASM_MODULE_FULL,
            "Kain WAMR Module",
            "Kain-owned full WASM module surface backed by WAMR",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_DEGRADED,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_wamr_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_WAMR,
        "WAMR",
        "WAMR",
        {
            KAIN_VENDOR_SERVICE_KEY_WASM_WASI_FULL,
            "Kain WAMR WASI",
            "Kain-owned full WASI surface backed by WAMR",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_DEGRADED,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_wamr_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_MIMALLOC,
        "mimalloc",
        "mimalloc",
        {
            KAIN_VENDOR_SERVICE_KEY_ALLOCATOR_MIMALLOC,
            "Kain mimalloc Backend",
            "Kain-owned allocator surface backed by mimalloc",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_mimalloc_functions
        }
    },
    {
        KAIN_VENDOR_SERVICE_FAMILY_RPMALLOC,
        "rpmalloc",
        "rpmalloc",
        {
            KAIN_VENDOR_SERVICE_KEY_ALLOCATOR_RPMALLOC,
            "Kain rpmalloc Backend",
            "Kain-owned allocator surface backed by rpmalloc",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            (void*)&g_kain_vendor_rpmalloc_functions
        }
    },
};

static const KainVendorServiceCatalog g_kain_vendor_catalog = {
    g_kain_vendor_service_catalog,
    sizeof(g_kain_vendor_service_catalog) / sizeof(g_kain_vendor_service_catalog[0]),
};

static const char* const g_kain_vendor_family_names[KAIN_VENDOR_SERVICE_FAMILY_COUNT] = {
    "unknown",
    "libuv",
    "QuickJS",
    "miniaudio",
    "wasm3",
    "WAMR",
    "mimalloc",
    "rpmalloc",
};

static const char* const g_kain_vendor_vendor_names[KAIN_VENDOR_SERVICE_FAMILY_COUNT] = {
    "unknown",
    "libuv",
    "QuickJS",
    "miniaudio",
    "wasm3",
    "WAMR",
    "mimalloc",
    "rpmalloc",
};

const KainVendorServiceCatalog* kain_vendor_service_catalog(void) {
    return &g_kain_vendor_catalog;
}

size_t kain_vendor_service_count(void) {
    return g_kain_vendor_catalog.service_count;
}

const KainVendorServiceDescriptor* kain_vendor_service_at(size_t index) {
    if (index >= g_kain_vendor_catalog.service_count) {
        return NULL;
    }
    return &g_kain_vendor_catalog.services[index];
}

const KainVendorServiceDescriptor* kain_vendor_service_lookup(const char* service_key) {
    size_t index;

    if (!service_key || !service_key[0]) {
        return NULL;
    }

    for (index = 0; index < g_kain_vendor_catalog.service_count; ++index) {
        const KainVendorServiceDescriptor* descriptor = &g_kain_vendor_catalog.services[index];
        if (strcmp(descriptor->descriptor.key, service_key) == 0) {
            return descriptor;
        }
    }

    return NULL;
}

const KainVendorServiceDescriptor* kain_vendor_service_family_lookup(KainVendorServiceFamily family) {
    size_t index;

    if (family <= KAIN_VENDOR_SERVICE_FAMILY_UNKNOWN || family >= KAIN_VENDOR_SERVICE_FAMILY_COUNT) {
        return NULL;
    }

    for (index = 0; index < g_kain_vendor_catalog.service_count; ++index) {
        const KainVendorServiceDescriptor* descriptor = &g_kain_vendor_catalog.services[index];
        if (descriptor->family == family) {
            return descriptor;
        }
    }

    return NULL;
}

const KainServiceDescriptor* kain_vendor_service_runtime_descriptor(const char* service_key) {
    const KainVendorServiceDescriptor* descriptor = kain_vendor_service_lookup(service_key);
    if (!descriptor) {
        return NULL;
    }
    return &descriptor->descriptor;
}

const void* kain_vendor_service_function_table(const char* service_key) {
    const KainVendorServiceDescriptor* descriptor = kain_vendor_service_lookup(service_key);
    if (!descriptor) {
        return NULL;
    }
    return descriptor->descriptor.function_table;
}

const char* kain_vendor_service_family_name(KainVendorServiceFamily family) {
    if (family < 0 || family >= KAIN_VENDOR_SERVICE_FAMILY_COUNT) {
        return g_kain_vendor_family_names[KAIN_VENDOR_SERVICE_FAMILY_UNKNOWN];
    }
    return g_kain_vendor_family_names[family];
}

const char* kain_vendor_service_vendor_name(KainVendorServiceFamily family) {
    if (family < 0 || family >= KAIN_VENDOR_SERVICE_FAMILY_COUNT) {
        return g_kain_vendor_vendor_names[KAIN_VENDOR_SERVICE_FAMILY_UNKNOWN];
    }
    return g_kain_vendor_vendor_names[family];
}
