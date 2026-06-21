// ============================================================================
//  d3d12_surface_shim.c — Kain-level D3D12 surface contract.
// ============================================================================
//  Mirrors vulkan_surface_shim.c but Windows-only. This file owns the
//  capability flag, env vars, error/telemetry globals, KainComponentSurface
//  vtable shape, and LoadLibrary of the separately-linked D3D12 ABI DLL.
//
//  The actual D3D12CreateDevice and COM vtable calls live in
//  libkain-d3d12-abi.dll — this shim never calls D3D12CreateDevice etc.
// ============================================================================

#ifdef KAIN_RUNTIME_HAS_D3D12

#include "../../include/d3d12_loader_subset.h"
#include "../../include/renderer_backend.h"
#include "../../include/services.h"
#include "../../include/component_surface.h"
#include "../../include/graphics_system.h"
#include "../../include/base.h"

#ifdef _WIN32
#include <windows.h>
typedef HMODULE KainD3D12AbiLibrary;
#else
#include <dlfcn.h>
typedef void* KainD3D12AbiLibrary;
#endif

#include <stdio.h>
#include <string.h>

#define KAIN_D3D12_STATUS_MESSAGE_MAX 512u
#define KAIN_D3D12_PATH_MAX 1024u

// ── ABI library vtable shape ─────────────────────────────────────

typedef struct KainD3D12AbiVtable {
    KainComponentSurface surface;
    int64_t              abi_version;
    int64_t              present_count;
    int64_t              swapchain_recreations;
    int64_t              last_status;
    char                 last_error[KAIN_D3D12_STATUS_MESSAGE_MAX];
} KainD3D12AbiVtable;

typedef const KainD3D12AbiVtable* (*KainD3D12AbiGetVtableFn)(void);

// ── Telemetry globals ────────────────────────────────────────────

static KainD3D12AbiLibrary       g_d3d12_abi_library = NULL;
static const KainD3D12AbiVtable* g_d3d12_vtable = NULL;
static int64_t                    g_d3d12_capability_probed = 0;
static int64_t                    g_d3d12_capability_available = 0;

// ── Helpers ──────────────────────────────────────────────────────

static void d3d12_copy_text(char* dest, size_t cap, const char* src) {
    if (!dest || cap == 0) return;
    dest[0] = '\0';
    if (!src) return;
    snprintf(dest, cap, "%s", src);
}

static int d3d12_file_exists(const char* path) {
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

static int d3d12_resolve_env_path(const char* env_name,
                                   const char* fallback_env,
                                   const char* file_name,
                                   char* out, size_t out_cap) {
    const char* env_value = kain_get_env(env_name);
    if (env_value && env_value[0]) {
        snprintf(out, out_cap, "%s/%s", env_value, file_name);
        return d3d12_file_exists(out);
    }
    env_value = kain_get_env(fallback_env);
    if (env_value && env_value[0]) {
        snprintf(out, out_cap, "%s/%s", env_value, file_name);
        return d3d12_file_exists(out);
    }
    snprintf(out, out_cap, "./%s", file_name);
    return d3d12_file_exists(out);
}

// ── Dynamic library loading ──────────────────────────────────────

static int d3d12_open_abi_library(const char* path,
                                   KainD3D12AbiLibrary* out_lib,
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

static void* d3d12_resolve_abi_symbol(KainD3D12AbiLibrary lib,
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

static void d3d12_close_abi_library(KainD3D12AbiLibrary lib) {
    if (lib == NULL) return;
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

// ── Public API — Capability probe ────────────────────────────────

int64_t kain_d3d12_runtime_capability(void) {
    if (g_d3d12_capability_probed) {
        return g_d3d12_capability_available;
    }
    g_d3d12_capability_probed = 1;

#ifdef _WIN32
    /* Probe d3d12.dll and dxgi.dll */
    HMODULE probe_d3d12 = LoadLibraryA("d3d12.dll");
    HMODULE probe_dxgi  = LoadLibraryA("dxgi.dll");
    if (probe_d3d12 != NULL && probe_dxgi != NULL) {
        if (probe_d3d12) FreeLibrary(probe_d3d12);
        if (probe_dxgi)  FreeLibrary(probe_dxgi);
        g_d3d12_capability_available = 1;
        return 1;
    }
    if (probe_d3d12) FreeLibrary(probe_d3d12);
    if (probe_dxgi)  FreeLibrary(probe_dxgi);
#endif
    g_d3d12_capability_available = 0;
    return 0;
}

// ── Public API — Surface resolve ─────────────────────────────────

int64_t kain_d3d12_surface_shim_resolve(KainComponentSurface* out_surface) {
    if (!out_surface) return -1;

    if (g_d3d12_vtable != NULL) {
        *out_surface = g_d3d12_vtable->surface;
        return 0;
    }

    if (!kain_d3d12_runtime_capability()) {
        return -2; /* no D3D12 driver available */
    }

    char abi_path[KAIN_D3D12_PATH_MAX];
    char message[KAIN_D3D12_STATUS_MESSAGE_MAX];
    if (!d3d12_resolve_env_path("KAIN_D3D12_ABI_LIBRARY", "KAIN_HOME",
                                 "lib/libkain-d3d12-abi.dll",
                                 abi_path, sizeof(abi_path))) {
        return -3;
    }

    KainD3D12AbiLibrary lib = NULL;
    if (!d3d12_open_abi_library(abi_path, &lib,
                                 message, sizeof(message))) {
        return -4;
    }

    KainD3D12AbiGetVtableFn get_vtable =
        (KainD3D12AbiGetVtableFn)d3d12_resolve_abi_symbol(
            lib, "kain_d3d12_abi_get_vtable",
            message, sizeof(message));
    if (get_vtable == NULL) {
        d3d12_close_abi_library(lib);
        return -5;
    }

    g_d3d12_vtable = get_vtable();
    if (g_d3d12_vtable == NULL || g_d3d12_vtable->abi_version < 1) {
        d3d12_close_abi_library(lib);
        return -6;
    }

    g_d3d12_abi_library = lib;

    kain_component_surface_register("d3d12", &g_d3d12_vtable->surface);
    kain_component_surface_register("d3d12_default",
                                     &g_d3d12_vtable->surface);

    abi_graphics_backend_set_available("d3d12", 1);

    *out_surface = g_d3d12_vtable->surface;
    return 0;
}

// ── Telemetry accessors ──────────────────────────────────────────

int64_t abi_d3d12_last_status(void) {
    if (g_d3d12_vtable) return g_d3d12_vtable->last_status;
    return 0;
}

const char* abi_d3d12_last_error(void) {
    if (g_d3d12_vtable) return g_d3d12_vtable->last_error;
    return "d3d12 surface not initialized";
}

int64_t abi_d3d12_present_count(void) {
    if (g_d3d12_vtable) return g_d3d12_vtable->present_count;
    return 0;
}

// ── Shutdown ─────────────────────────────────────────────────────

void kain_d3d12_surface_shim_shutdown(void) {
    d3d12_close_abi_library(g_d3d12_abi_library);
    g_d3d12_abi_library = NULL;
    g_d3d12_vtable = NULL;
}

#else /* !KAIN_RUNTIME_HAS_D3D12 */

#include "../../include/component_surface.h"
#include <stdint.h>

int64_t kain_d3d12_runtime_capability(void) {
    return 0;
}

int64_t kain_d3d12_surface_shim_resolve(KainComponentSurface* out_surface) {
    (void)out_surface;
    return -1;
}

int64_t abi_d3d12_last_status(void) {
    return 0;
}

const char* abi_d3d12_last_error(void) {
    return "d3d12 surface not initialized (loader not built)";
}

int64_t abi_d3d12_present_count(void) {
    return 0;
}

void kain_d3d12_surface_shim_shutdown(void) {
}

#endif /* KAIN_RUNTIME_HAS_D3D12 */
