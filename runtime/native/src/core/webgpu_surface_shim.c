// ============================================================================
//  webgpu_surface_shim.c — Kain-level WebGPU surface contract.
// ============================================================================
//  Smallest shim — WebGPU has the simplest API. Has a WASM compile-time path
//  that bypasses dlopen entirely (browser provides WebGPU natively).
//
//  The actual wgpuCreate* calls live in libkain-webgpu-abi.so — this shim
//  never calls wgpuCreateInstance, wgpuDeviceCreateSwapChain, etc.
// ============================================================================

#ifdef KAIN_RUNTIME_HAS_WEBGPU

#include "../../include/webgpu_loader_subset.h"
#include "../../include/renderer_backend.h"
#include "../../include/services.h"
#include "../../include/component_surface.h"
#include "../../include/graphics_system.h"
#include "../../include/base.h"

#ifndef __wasm__
#ifdef _WIN32
#include <windows.h>
typedef HMODULE KainWebgpuAbiLibrary;
#else
#include <dlfcn.h>
typedef void* KainWebgpuAbiLibrary;
#endif
#endif /* !__wasm__ */

#include <stdio.h>
#include <string.h>

#define KAIN_WEBGPU_STATUS_MESSAGE_MAX 512u
#define KAIN_WEBGPU_PATH_MAX 1024u

// ── ABI library vtable shape ─────────────────────────────────────

typedef struct KainWebgpuAbiVtable {
    KainComponentSurface surface;
    int64_t              abi_version;
    int64_t              present_count;
    int64_t              swapchain_recreations;
    int64_t              last_status;
    char                 last_error[KAIN_WEBGPU_STATUS_MESSAGE_MAX];
} KainWebgpuAbiVtable;

typedef const KainWebgpuAbiVtable* (*KainWebgpuAbiGetVtableFn)(void);

// ── Telemetry globals ────────────────────────────────────────────

#ifndef __wasm__
static KainWebgpuAbiLibrary       g_webgpu_abi_library = NULL;
#endif
static const KainWebgpuAbiVtable* g_webgpu_vtable = NULL;
static int64_t                     g_webgpu_capability_probed = 0;
static int64_t                     g_webgpu_capability_available = 0;

// ── Helpers ──────────────────────────────────────────────────────

static void webgpu_copy_text(char* dest, size_t cap, const char* src) {
    if (!dest || cap == 0) return;
    dest[0] = '\0';
    if (!src) return;
    snprintf(dest, cap, "%s", src);
}

#ifndef __wasm__
static int webgpu_file_exists(const char* path) {
    if (!path || !path[0]) return 0;
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES &&
            !(attrib & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
#endif
}

static int webgpu_resolve_env_path(const char* env_name,
                                    const char* fallback_env,
                                    const char* file_name,
                                    char* out, size_t out_cap) {
    const char* env_value = getenv(env_name);
    if (env_value && env_value[0]) {
        snprintf(out, out_cap, "%s/%s", env_value, file_name);
        return webgpu_file_exists(out);
    }
    env_value = getenv(fallback_env);
    if (env_value && env_value[0]) {
        snprintf(out, out_cap, "%s/%s", env_value, file_name);
        return webgpu_file_exists(out);
    }
    snprintf(out, out_cap, "./%s", file_name);
    return webgpu_file_exists(out);
}

// ── Dynamic library loading ──────────────────────────────────────

static int webgpu_open_abi_library(const char* path,
                                    KainWebgpuAbiLibrary* out_lib,
                                    char* message, size_t message_cap) {
#ifdef _WIN32
    *out_lib = LoadLibraryA(path);
    if (*out_lib == NULL) {
        snprintf(message, message_cap,
                 "LoadLibraryA failed for %s (error %lu)", path,
                 GetLastError());
        return 0;
    }
#else
    *out_lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (*out_lib == NULL) {
        snprintf(message, message_cap,
                 "dlopen failed for %s: %s", path, dlerror());
        return 0;
    }
#endif
    return 1;
}

static void* webgpu_resolve_abi_symbol(KainWebgpuAbiLibrary lib,
                                        const char* name,
                                        char* message, size_t message_cap) {
#ifdef _WIN32
    void* sym = (void*)GetProcAddress(lib, name);
    if (sym == NULL) {
        snprintf(message, message_cap,
                 "GetProcAddress failed for %s (error %lu)", name,
                 GetLastError());
        return NULL;
    }
#else
    void* sym = dlsym(lib, name);
    if (sym == NULL) {
        snprintf(message, message_cap,
                 "dlsym failed for %s: %s", name, dlerror());
        return NULL;
    }
#endif
    return sym;
}

static void webgpu_close_abi_library(KainWebgpuAbiLibrary lib) {
    if (lib == NULL) return;
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}
#endif /* !__wasm__ */

// ── Public API — Capability probe ────────────────────────────────

int64_t kain_webgpu_runtime_capability(void) {
    if (g_webgpu_capability_probed) {
        return g_webgpu_capability_available;
    }
    g_webgpu_capability_probed = 1;

#ifdef __wasm__
    /* WASM: browser provides WebGPU natively — no dlopen needed. */
    g_webgpu_capability_available = 1;
    return 1;
#else
#ifdef _WIN32
    HMODULE probe = LoadLibraryA("wgpu_native.dll");
    if (probe != NULL) {
        FreeLibrary(probe);
        g_webgpu_capability_available = 1;
        return 1;
    }
#else
    void* probe = dlopen("libwgpu_native.so", RTLD_NOW | RTLD_LOCAL);
    if (probe != NULL) {
        dlclose(probe);
        g_webgpu_capability_available = 1;
        return 1;
    }
#endif
    g_webgpu_capability_available = 0;
    return 0;
#endif
}

// ── Public API — Surface resolve ─────────────────────────────────

int64_t kain_webgpu_surface_shim_resolve(KainComponentSurface* out_surface) {
    if (!out_surface) return -1;

    if (g_webgpu_vtable != NULL) {
        *out_surface = g_webgpu_vtable->surface;
        return 0;
    }

    if (!kain_webgpu_runtime_capability()) {
        return -2;
    }

#ifdef __wasm__
    /* WASM: no dlopen — the ABI library is statically linked. */
    /* The ABI library must define kain_webgpu_abi_get_vtable as extern. */
    extern KainWebgpuAbiGetVtableFn kain_webgpu_abi_get_vtable;
    if (!kain_webgpu_abi_get_vtable) return -5;
    g_webgpu_vtable = kain_webgpu_abi_get_vtable();
#else
    char abi_path[KAIN_WEBGPU_PATH_MAX];
    char message[KAIN_WEBGPU_STATUS_MESSAGE_MAX];
    if (!webgpu_resolve_env_path("KAIN_WEBGPU_ABI_LIBRARY", "KAIN_HOME",
                                  "lib/libkain-webgpu-abi.so",
                                  abi_path, sizeof(abi_path))) {
#ifdef _WIN32
        if (!webgpu_resolve_env_path("KAIN_WEBGPU_ABI_LIBRARY", "KAIN_HOME",
                                      "lib/libkain-webgpu-abi.dll",
                                      abi_path, sizeof(abi_path))) {
            return -3;
        }
#else
        return -3;
#endif
    }

    KainWebgpuAbiLibrary lib = NULL;
    if (!webgpu_open_abi_library(abi_path, &lib,
                                  message, sizeof(message))) {
        return -4;
    }

    KainWebgpuAbiGetVtableFn get_vtable =
        (KainWebgpuAbiGetVtableFn)webgpu_resolve_abi_symbol(
            lib, "kain_webgpu_abi_get_vtable",
            message, sizeof(message));
    if (get_vtable == NULL) {
        webgpu_close_abi_library(lib);
        return -5;
    }

    g_webgpu_vtable = get_vtable();
    /* Keep library handle for shutdown */
    g_webgpu_abi_library = lib;
#endif /* __wasm__ */

    if (g_webgpu_vtable == NULL || g_webgpu_vtable->abi_version < 1) {
#ifndef __wasm__
        webgpu_close_abi_library(g_webgpu_abi_library);
        g_webgpu_abi_library = NULL;
#endif
        return -6;
    }

    kain_component_surface_register("webgpu", &g_webgpu_vtable->surface);
    kain_component_surface_register("webgpu_default",
                                     &g_webgpu_vtable->surface);

    abi_graphics_backend_set_available("webgpu", 1);

    *out_surface = g_webgpu_vtable->surface;
    return 0;
}

// ── Telemetry accessors ──────────────────────────────────────────

int64_t abi_webgpu_last_status(void) {
    if (g_webgpu_vtable) return g_webgpu_vtable->last_status;
    return 0;
}

const char* abi_webgpu_last_error(void) {
    if (g_webgpu_vtable) return g_webgpu_vtable->last_error;
    return "webgpu surface not initialized";
}

int64_t abi_webgpu_present_count(void) {
    if (g_webgpu_vtable) return g_webgpu_vtable->present_count;
    return 0;
}

// ── Shutdown ─────────────────────────────────────────────────────

void kain_webgpu_surface_shim_shutdown(void) {
#ifndef __wasm__
    webgpu_close_abi_library(g_webgpu_abi_library);
    g_webgpu_abi_library = NULL;
#endif
    g_webgpu_vtable = NULL;
}

#else /* !KAIN_RUNTIME_HAS_WEBGPU */

#include "../../include/component_surface.h"
#include <stdint.h>

int64_t kain_webgpu_runtime_capability(void) {
    return 0;
}

int64_t kain_webgpu_surface_shim_resolve(KainComponentSurface* out_surface) {
    (void)out_surface;
    return -1;
}

int64_t abi_webgpu_last_status(void) {
    return 0;
}

const char* abi_webgpu_last_error(void) {
    return "webgpu surface not initialized (loader not built)";
}

int64_t abi_webgpu_present_count(void) {
    return 0;
}

void kain_webgpu_surface_shim_shutdown(void) {
}

#endif /* KAIN_RUNTIME_HAS_WEBGPU */
