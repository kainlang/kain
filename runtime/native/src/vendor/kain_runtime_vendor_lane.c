#include "../../include/kain_runtime_vendor_lane.h"
#include "../../include/kain_runtime_vendor_graphics_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef CONFIG_VERSION
#define CONFIG_VERSION "kain-vendor"
#endif

#if !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#if KAIN_VENDOR_HAS_LIBUV
#include "uv.h"
#endif

#include "quickjs.h"
#include "miniaudio.h"
#include "wasm3.h"
#include "m3_env.h"
#include "mimalloc.h"
#include "rpmalloc.h"
#endif

static void kain_vendor_stub_shutdown(void) {
}

static int kain_vendor_stub_poll_once(int timeout_ms) {
    (void)timeout_ms;
    return 0;
}

static int kain_vendor_stub_eval_int32(const char* source, int* out_value) {
    (void)source;
    if (out_value) {
        *out_value = 0;
    }
    return 0;
}

static void* kain_vendor_stub_allocate(size_t size) {
    (void)size;
    return NULL;
}

static void kain_vendor_stub_deallocate(void* memory) {
    (void)memory;
}

#if KAIN_VENDOR_HAS_LIBUV && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
static int kain_vendor_libuv_probe(void) {
    uv_loop_t loop;
    if (uv_loop_init(&loop) != 0) {
        return 0;
    }
    uv_loop_close(&loop);
    return 1;
}

static int kain_vendor_libuv_poll_once(int timeout_ms) {
    uv_loop_t loop;
    (void)timeout_ms;
    if (uv_loop_init(&loop) != 0) {
        return 0;
    }
    uv_run(&loop, UV_RUN_NOWAIT);
    uv_loop_close(&loop);
    return 1;
}

static const char* kain_vendor_libuv_version(void) {
    return uv_version_string();
}
#else
static int kain_vendor_libuv_probe(void) {
    return 0;
}

static int kain_vendor_libuv_poll_once(int timeout_ms) {
    (void)timeout_ms;
    return 0;
}

static const char* kain_vendor_libuv_version(void) {
    return "libuv-unavailable";
}
#endif

#if !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
static int kain_vendor_quickjs_probe(void) {
    JSRuntime* runtime = JS_NewRuntime();
    if (!runtime) {
        return 0;
    }
    JS_FreeRuntime(runtime);
    return 1;
}

static int kain_vendor_quickjs_eval_int32(const char* source, int* out_value) {
    JSRuntime* runtime;
    JSContext* context;
    JSValue result;
    int32_t numeric_value = 0;
    int success = 0;

    if (!source || !out_value) {
        return 0;
    }

    runtime = JS_NewRuntime();
    if (!runtime) {
        return 0;
    }

    context = JS_NewContext(runtime);
    if (!context) {
        JS_FreeRuntime(runtime);
        return 0;
    }

    result = JS_Eval(
        context,
        source,
        strlen(source),
        "<kain-vendor>",
        JS_EVAL_TYPE_GLOBAL
    );
    if (!JS_IsException(result) &&
        JS_ToInt32(context, &numeric_value, result) == 0) {
        *out_value = (int)numeric_value;
        success = 1;
    }

    JS_FreeValue(context, result);
    JS_FreeContext(context);
    JS_FreeRuntime(runtime);
    return success;
}

static const char* kain_vendor_quickjs_version(void) {
    return CONFIG_VERSION;
}
#else
static int kain_vendor_quickjs_probe(void) {
    return 0;
}

static int kain_vendor_quickjs_eval_int32(const char* source, int* out_value) {
    (void)source;
    if (out_value) {
        *out_value = 0;
    }
    return 0;
}

static const char* kain_vendor_quickjs_version(void) {
    return "quickjs-stub";
}
#endif

#if !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
static int kain_vendor_miniaudio_probe(void) {
    return 1;
}

static int kain_vendor_miniaudio_start(void) {
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        return 0;
    }
    ma_context_uninit(&context);
    return 1;
}

static const char* kain_vendor_miniaudio_version(void) {
    return ma_version_string();
}
#else
static int kain_vendor_miniaudio_probe(void) {
    return 0;
}

static int kain_vendor_miniaudio_start(void) {
    return 0;
}

static const char* kain_vendor_miniaudio_version(void) {
    return "miniaudio-stub";
}
#endif

#if !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
static int kain_vendor_wasm3_probe(void) {
    IM3Environment environment = m3_NewEnvironment();
    IM3Runtime runtime;

    if (!environment) {
        return 0;
    }

    runtime = m3_NewRuntime(environment, 64 * 1024, NULL);
    if (!runtime) {
        m3_FreeEnvironment(environment);
        return 0;
    }

    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(environment);
    return 1;
}

static const char* kain_vendor_wasm3_version(void) {
    return M3_VERSION;
}
#else
static int kain_vendor_wasm3_probe(void) {
    return 0;
}

static const char* kain_vendor_wasm3_version(void) {
    return "wasm3-stub";
}
#endif

static int kain_vendor_wamr_probe(void) {
    return 0;
}

static const char* kain_vendor_wamr_version(void) {
    return "wamr-staged";
}

#if !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
static int kain_vendor_mimalloc_probe(void) {
    return mi_version() > 0;
}

static const char* kain_vendor_mimalloc_version(void) {
    static char version_buffer[32];
    static int initialized = 0;
    if (!initialized) {
        snprintf(version_buffer, sizeof(version_buffer), "%d", mi_version());
        initialized = 1;
    }
    return version_buffer;
}

static void* kain_vendor_mimalloc_allocate(size_t size) {
    return mi_malloc(size);
}

static void kain_vendor_mimalloc_deallocate(void* memory) {
    mi_free(memory);
}

static int g_kain_vendor_rpmalloc_initialized = 0;

static int kain_vendor_rpmalloc_start(void) {
    if (!g_kain_vendor_rpmalloc_initialized) {
        if (rpmalloc_initialize(NULL) != 0) {
            return 0;
        }
        g_kain_vendor_rpmalloc_initialized = 1;
    }

    if (!rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_initialize();
    }
    return 1;
}

static void kain_vendor_rpmalloc_shutdown(void) {
    if (!g_kain_vendor_rpmalloc_initialized) {
        return;
    }
    if (rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_finalize();
    }
    rpmalloc_finalize();
    g_kain_vendor_rpmalloc_initialized = 0;
}

static int kain_vendor_rpmalloc_probe(void) {
    return kain_vendor_rpmalloc_start();
}

static void* kain_vendor_rpmalloc_allocate(size_t size) {
    if (!kain_vendor_rpmalloc_start()) {
        return NULL;
    }
    return rpmalloc(size);
}

static void kain_vendor_rpmalloc_deallocate(void* memory) {
    if (!memory) {
        return;
    }
    if (!rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_initialize();
    }
    rpfree(memory);
}

static const char* kain_vendor_rpmalloc_version(void) {
    return "rpmalloc-curated";
}
#else
static int kain_vendor_mimalloc_probe(void) {
    return 0;
}

static const char* kain_vendor_mimalloc_version(void) {
    return "mimalloc-stub";
}

static void* kain_vendor_mimalloc_allocate(size_t size) {
    (void)size;
    return NULL;
}

static void kain_vendor_mimalloc_deallocate(void* memory) {
    (void)memory;
}

static int kain_vendor_rpmalloc_start(void) {
    return 0;
}

static void kain_vendor_rpmalloc_shutdown(void) {
}

static int kain_vendor_rpmalloc_probe(void) {
    return 0;
}

static void* kain_vendor_rpmalloc_allocate(size_t size) {
    (void)size;
    return NULL;
}

static void kain_vendor_rpmalloc_deallocate(void* memory) {
    (void)memory;
}

static const char* kain_vendor_rpmalloc_version(void) {
    return "rpmalloc-stub";
}
#endif

const KainVendorServiceFunctionTable g_kain_vendor_io_loop_service = {
    "io.loop",
    "libuv",
    "libuv-loop",
    kain_vendor_libuv_version,
    kain_vendor_libuv_probe,
    kain_vendor_libuv_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_libuv_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_io_fs_service = {
    "io.fs",
    "libuv",
    "libuv-fs",
    kain_vendor_libuv_version,
    kain_vendor_libuv_probe,
    kain_vendor_libuv_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_io_net_service = {
    "io.net",
    "libuv",
    "libuv-net",
    kain_vendor_libuv_version,
    kain_vendor_libuv_probe,
    kain_vendor_libuv_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_io_process_service = {
    "io.process",
    "libuv",
    "libuv-process",
    kain_vendor_libuv_version,
    kain_vendor_libuv_probe,
    kain_vendor_libuv_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_io_timers_service = {
    "io.timers",
    "libuv",
    "libuv-timers",
    kain_vendor_libuv_version,
    kain_vendor_libuv_probe,
    kain_vendor_libuv_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_libuv_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_script_quickjs_service = {
    "script.quickjs",
    "quickjs",
    "quickjs",
    kain_vendor_quickjs_version,
    kain_vendor_quickjs_probe,
    kain_vendor_quickjs_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_quickjs_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_audio_backend_service = {
    "audio.backend",
    "miniaudio",
    "miniaudio-backend",
    kain_vendor_miniaudio_version,
    kain_vendor_miniaudio_probe,
    kain_vendor_miniaudio_start,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_audio_graph_service = {
    "audio.graph",
    "miniaudio",
    "miniaudio-graph",
    kain_vendor_miniaudio_version,
    kain_vendor_miniaudio_probe,
    kain_vendor_miniaudio_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_audio_device_service = {
    "audio.device",
    "miniaudio",
    "miniaudio-device",
    kain_vendor_miniaudio_version,
    kain_vendor_miniaudio_probe,
    kain_vendor_miniaudio_start,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_audio_assets_service = {
    "audio.assets",
    "miniaudio",
    "miniaudio-assets",
    kain_vendor_miniaudio_version,
    kain_vendor_miniaudio_probe,
    kain_vendor_miniaudio_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_wasm_runtime_light_service = {
    "wasm.runtime.light",
    "wasm3",
    "wasm3",
    kain_vendor_wasm3_version,
    kain_vendor_wasm3_probe,
    kain_vendor_wasm3_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_wasm_runtime_full_service = {
    "wasm.runtime.full",
    "wamr",
    "wamr-staged",
    kain_vendor_wamr_version,
    kain_vendor_wamr_probe,
    kain_vendor_wamr_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_wasm_module_service = {
    "wasm.module",
    "wasm3",
    "wasm3-module",
    kain_vendor_wasm3_version,
    kain_vendor_wasm3_probe,
    kain_vendor_wasm3_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_wasm_wasi_service = {
    "wasm.wasi",
    "wamr",
    "wamr-wasi-staged",
    kain_vendor_wamr_version,
    kain_vendor_wamr_probe,
    kain_vendor_wamr_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_allocator_mimalloc_service = {
    "allocator.mimalloc",
    "mimalloc",
    "mimalloc",
    kain_vendor_mimalloc_version,
    kain_vendor_mimalloc_probe,
    kain_vendor_mimalloc_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_mimalloc_allocate,
    kain_vendor_mimalloc_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_allocator_rpmalloc_service = {
    "allocator.rpmalloc",
    "rpmalloc",
    "rpmalloc",
    kain_vendor_rpmalloc_version,
    kain_vendor_rpmalloc_probe,
    kain_vendor_rpmalloc_start,
    kain_vendor_rpmalloc_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_rpmalloc_allocate,
    kain_vendor_rpmalloc_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_bgfx_service = {
    "gfx.backend.bgfx",
    "bgfx",
    "bgfx-renderer",
    kain_vendor_bgfx_version_string,
    kain_vendor_bgfx_probe,
    kain_vendor_bgfx_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_filament_service = {
    "gfx.backend.filament",
    "filament-core",
    "filament-renderer-staged",
    kain_vendor_filament_version_string,
    kain_vendor_filament_probe,
    kain_vendor_filament_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_diligent_service = {
    "gfx.backend.diligent",
    "diligentcore",
    "diligent-renderer-staged",
    kain_vendor_diligent_version_string,
    kain_vendor_diligent_probe,
    kain_vendor_diligent_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_gfx_backend_forge_service = {
    "gfx.backend.forge",
    "the-forge",
    "the-forge-renderer-staged",
    kain_vendor_forge_version_string,
    kain_vendor_forge_probe,
    kain_vendor_forge_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_asset_image_bimg_service = {
    "asset.image.bimg",
    "bimg",
    "bimg-image-staged",
    kain_vendor_bimg_version_string,
    kain_vendor_bimg_probe,
    kain_vendor_bimg_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

const KainVendorServiceFunctionTable g_kain_vendor_asset_texture_bimg_service = {
    "asset.texture.bimg",
    "bimg",
    "bimg-texture-staged",
    kain_vendor_bimg_version_string,
    kain_vendor_bimg_probe,
    kain_vendor_bimg_probe,
    kain_vendor_stub_shutdown,
    kain_vendor_stub_poll_once,
    kain_vendor_stub_eval_int32,
    kain_vendor_stub_allocate,
    kain_vendor_stub_deallocate
};

static const KainVendorServiceDescriptor g_kain_vendor_service_catalog[] = {
    {"io.loop", "io", "libuv", KAIN_VENDOR_HAS_LIBUV, &g_kain_vendor_io_loop_service},
    {"io.fs", "io", "libuv", KAIN_VENDOR_HAS_LIBUV, &g_kain_vendor_io_fs_service},
    {"io.net", "io", "libuv", KAIN_VENDOR_HAS_LIBUV, &g_kain_vendor_io_net_service},
    {"io.process", "io", "libuv", KAIN_VENDOR_HAS_LIBUV, &g_kain_vendor_io_process_service},
    {"io.timers", "io", "libuv", KAIN_VENDOR_HAS_LIBUV, &g_kain_vendor_io_timers_service},
    {"script.quickjs", "script", "quickjs", KAIN_VENDOR_HAS_QUICKJS, &g_kain_vendor_script_quickjs_service},
    {"audio.backend", "audio", "miniaudio", KAIN_VENDOR_HAS_MINIAUDIO, &g_kain_vendor_audio_backend_service},
    {"audio.graph", "audio", "miniaudio", KAIN_VENDOR_HAS_MINIAUDIO, &g_kain_vendor_audio_graph_service},
    {"audio.device", "audio", "miniaudio", KAIN_VENDOR_HAS_MINIAUDIO, &g_kain_vendor_audio_device_service},
    {"audio.assets", "audio", "miniaudio", KAIN_VENDOR_HAS_MINIAUDIO, &g_kain_vendor_audio_assets_service},
    {"wasm.runtime.light", "wasm", "wasm3", KAIN_VENDOR_HAS_WASM3, &g_kain_vendor_wasm_runtime_light_service},
    {"wasm.runtime.full", "wasm", "wamr", KAIN_VENDOR_HAS_WAMR, &g_kain_vendor_wasm_runtime_full_service},
    {"wasm.module", "wasm", "wasm3", KAIN_VENDOR_HAS_WASM3, &g_kain_vendor_wasm_module_service},
    {"wasm.wasi", "wasm", "wamr", KAIN_VENDOR_HAS_WAMR, &g_kain_vendor_wasm_wasi_service},
    {"allocator.mimalloc", "allocator", "mimalloc", KAIN_VENDOR_HAS_MIMALLOC, &g_kain_vendor_allocator_mimalloc_service},
    {"allocator.rpmalloc", "allocator", "rpmalloc", KAIN_VENDOR_HAS_RPMALLOC, &g_kain_vendor_allocator_rpmalloc_service},
    {"gfx.backend.bgfx", "gfx", "bgfx", KAIN_VENDOR_HAS_BGFX, &g_kain_vendor_gfx_backend_bgfx_service},
    {"gfx.backend.filament", "gfx", "filament-core", KAIN_VENDOR_HAS_FILAMENT, &g_kain_vendor_gfx_backend_filament_service},
    {"gfx.backend.diligent", "gfx", "diligentcore", KAIN_VENDOR_HAS_DILIGENT, &g_kain_vendor_gfx_backend_diligent_service},
    {"gfx.backend.forge", "gfx", "the-forge", KAIN_VENDOR_HAS_FORGE, &g_kain_vendor_gfx_backend_forge_service},
    {"asset.image.bimg", "asset", "bimg", KAIN_VENDOR_HAS_BIMG, &g_kain_vendor_asset_image_bimg_service},
    {"asset.texture.bimg", "asset", "bimg", KAIN_VENDOR_HAS_BIMG, &g_kain_vendor_asset_texture_bimg_service}
};

const KainVendorServiceDescriptor* kain_vendor_service_catalog(void) {
    return g_kain_vendor_service_catalog;
}

size_t kain_vendor_service_count(void) {
    return sizeof(g_kain_vendor_service_catalog) / sizeof(g_kain_vendor_service_catalog[0]);
}

const KainVendorServiceDescriptor* kain_vendor_service_at(size_t index) {
    if (index >= kain_vendor_service_count()) {
        return NULL;
    }
    return &g_kain_vendor_service_catalog[index];
}

const KainVendorServiceDescriptor* kain_vendor_service_lookup(const char* key) {
    size_t i;
    if (!key || !key[0]) {
        return NULL;
    }
    for (i = 0; i < kain_vendor_service_count(); ++i) {
        if (strcmp(g_kain_vendor_service_catalog[i].key, key) == 0) {
            return &g_kain_vendor_service_catalog[i];
        }
    }
    return NULL;
}
