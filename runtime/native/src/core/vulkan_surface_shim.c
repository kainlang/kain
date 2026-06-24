// ============================================================================
//  vulkan_surface_shim.c — Kain-level Vulkan surface contract.
// ============================================================================
//  This file owns the capability flag, env vars, error/telemetry globals,
//  KainComponentSurface vtable shape, renderer_session integration, and
//  dlopen of the separately-linked Vulkan ABI library.
//
//  The actual vkCreate* calls live in libkain-vulkan-abi.so — this shim
//  never calls vkCreateInstance, vkCreateDevice, vkCreateSwapchainKHR, etc.
//  Pattern: mirror cuda_runtime.c (contract in runtime, implementation in library).
// ============================================================================

#ifdef KAIN_RUNTIME_HAS_VULKAN_LOADER

#include "../../include/vulkan_loader_subset.h"
#include "../../include/renderer_backend.h"
#include "../../include/services.h"
#include "../../include/component_surface.h"
#include "../../include/graphics_system.h"
#include "../../include/base.h"

#ifdef _WIN32
#include <windows.h>
typedef HMODULE KainVulkanAbiLibrary;
#else
#include <dlfcn.h>
typedef void* KainVulkanAbiLibrary;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KAIN_VULKAN_STATUS_MESSAGE_MAX 512u
#define KAIN_VULKAN_PATH_MAX 1024u

// ── ABI library vtable shape ───────────────────────────────────
// Must match vulkan_abi.h exactly. Including the header to guarantee
// the struct layout is the same — the local typedef in this file had
// 6 fields missing KainVulkanPfnTable pfns (57 fn ptrs, 456 bytes),
// which caused all telemetry reads to return garbage.
#include "../../extras/vulkan-abi/vulkan_abi.h"

typedef const KainVulkanAbiVtable* (*KainVulkanAbiGetVtableFn)(void);

// ── Telemetry globals ────────────────────────────────────────────

static KainVulkanAbiLibrary       g_vulkan_abi_library = NULL;
const KainVulkanAbiVtable*        g_vulkan_vtable = NULL;
static int64_t                    g_vulkan_capability_probed = 0;
static int64_t                    g_vulkan_capability_available = 0;

// ── Helpers ──────────────────────────────────────────────────────

static void vulkan_copy_text(char* dest, size_t cap, const char* src) {
    if (!dest || cap == 0) return;
    dest[0] = '\0';
    if (!src) return;
    snprintf(dest, cap, "%s", src);
}

static int vulkan_file_exists(const char* path) {
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

static int vulkan_resolve_env_path(const char* env_name,
                                    const char* fallback_env,
                                    const char* file_name,
                                    char* out, size_t out_cap) {
    const char* env_value = getenv(env_name);
    if (env_value && env_value[0]) {
        snprintf(out, out_cap, "%s/%s", env_value, file_name);
        return vulkan_file_exists(out);
    }
    env_value = getenv(fallback_env);
    if (env_value && env_value[0]) {
        snprintf(out, out_cap, "%s/%s", env_value, file_name);
        return vulkan_file_exists(out);
    }
    snprintf(out, out_cap, "./%s", file_name);
    return vulkan_file_exists(out);
}

// ── Dynamic library loading (mirror cuda_runtime.c) ──────────────

static int vulkan_open_abi_library(const char* path,
                                    KainVulkanAbiLibrary* out_lib,
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

static void* vulkan_resolve_abi_symbol(KainVulkanAbiLibrary lib,
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

static void vulkan_close_abi_library(KainVulkanAbiLibrary lib) {
    if (lib == NULL) return;
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

// ── Public API — Capability probe ────────────────────────────────

int64_t kain_vulkan_runtime_capability(void) {
    if (g_vulkan_capability_probed) {
        return g_vulkan_capability_available;
    }
    g_vulkan_capability_probed = 1;

#ifdef _WIN32
    HMODULE probe = LoadLibraryA("vulkan-1.dll");
    if (probe != NULL) {
        FreeLibrary(probe);
        g_vulkan_capability_available = 1;
        return 1;
    }
#else
    void* probe = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (probe != NULL) {
        dlclose(probe);
        g_vulkan_capability_available = 1;
        return 1;
    }
    /* Try MoltenVK on macOS */
    probe = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
    if (probe != NULL) {
        dlclose(probe);
        g_vulkan_capability_available = 1;
        return 1;
    }
#endif
    g_vulkan_capability_available = 0;
    return 0;
}

// ── Public API — Surface resolve ─────────────────────────────────

int64_t kain_vulkan_surface_shim_resolve(KainComponentSurface* out_surface) {
    if (!out_surface) return -1;

    /* Already resolved */
    if (g_vulkan_vtable != NULL) {
        *out_surface = g_vulkan_vtable->surface;
        return 0;
    }

    /* Probe capability first */
    if (!kain_vulkan_runtime_capability()) {
        return -2; /* no Vulkan driver available */
    }

    /* Resolve ABI library path */
    char abi_path[KAIN_VULKAN_PATH_MAX];
    char message[KAIN_VULKAN_STATUS_MESSAGE_MAX];
    if (!vulkan_resolve_env_path("KAIN_VULKAN_ABI_LIBRARY", "KAIN_HOME",
                                  "lib/libkain-vulkan-abi.so",
                                  abi_path, sizeof(abi_path))) {
#ifdef _WIN32
        if (!vulkan_resolve_env_path("KAIN_VULKAN_ABI_LIBRARY", "KAIN_HOME",
                                      "lib/libkain-vulkan-abi.dll",
                                      abi_path, sizeof(abi_path))) {
            return -3;
        }
#else
        return -3;
#endif
    }

    /* Open ABI library */
    KainVulkanAbiLibrary lib = NULL;
    if (!vulkan_open_abi_library(abi_path, &lib,
                                  message, sizeof(message))) {
        return -4;
    }

    /* Resolve get_vtable symbol */
    KainVulkanAbiGetVtableFn get_vtable =
        (KainVulkanAbiGetVtableFn)vulkan_resolve_abi_symbol(
            lib, "kain_vulkan_abi_get_vtable",
            message, sizeof(message));
    if (get_vtable == NULL) {
        vulkan_close_abi_library(lib);
        return -5;
    }

    /* Get vtable from ABI library */
    g_vulkan_vtable = get_vtable();
    if (g_vulkan_vtable == NULL || g_vulkan_vtable->abi_version < 1) {
        vulkan_close_abi_library(lib);
        return -6;
    }

    g_vulkan_abi_library = lib;

    /* Register as component surface */
    kain_component_surface_register("vulkan", &g_vulkan_vtable->surface);
    kain_component_surface_register("vulkan_default",
                                     &g_vulkan_vtable->surface);

    /* Mark backend as available in graphics catalog */
    abi_graphics_backend_set_available("vulkan", 1);

    *out_surface = g_vulkan_vtable->surface;
    return 0;
}

// ── Telemetry accessors (mirror abi_cuda_last_* pattern) ─────────

int64_t abi_vulkan_last_status(void) {
    if (g_vulkan_vtable) return g_vulkan_vtable->last_status;
    return 0;
}

const char* abi_vulkan_last_error(void) {
    if (g_vulkan_vtable) return g_vulkan_vtable->last_error;
    return "vulkan surface not initialized";
}

int64_t abi_vulkan_present_count(void) {
    if (g_vulkan_vtable) return g_vulkan_vtable->present_count;
    return 0;
}

int64_t abi_vulkan_swapchain_recreations(void) {
    if (g_vulkan_vtable) return g_vulkan_vtable->swapchain_recreations;
    return 0;
}

// ── Shutdown ─────────────────────────────────────────────────────

void kain_vulkan_surface_shim_shutdown(void) {
    vulkan_close_abi_library(g_vulkan_abi_library);
    g_vulkan_abi_library = NULL;
    g_vulkan_vtable = NULL;
}

// ── Stub mode (when KAIN_RUNTIME_HAS_VULKAN_LOADER is NOT defined) ─

#else /* !KAIN_RUNTIME_HAS_VULKAN_LOADER */

#include "../../include/component_surface.h"
#include <stdint.h>

int64_t kain_vulkan_runtime_capability(void) {
    return 0;
}

int64_t kain_vulkan_surface_shim_resolve(KainComponentSurface* out_surface) {
    (void)out_surface;
    return -1;
}

int64_t abi_vulkan_last_status(void) {
    return 0;
}

const char* abi_vulkan_last_error(void) {
    return "vulkan surface not initialized (loader not built)";
}

int64_t abi_vulkan_present_count(void) {
    return 0;
}

int64_t abi_vulkan_swapchain_recreations(void) {
    return 0;
}

void kain_vulkan_surface_shim_shutdown(void) {
}

const KainGpuSurfaceExtension* vulkan_get_gpu_extension(int64_t session_id) {
    (void)session_id;
    return NULL;
}

#endif /* KAIN_RUNTIME_HAS_VULKAN_LOADER */
