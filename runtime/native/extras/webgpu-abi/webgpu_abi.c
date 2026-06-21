// ============================================================================
//  webgpu_abi.c — Kain WebGPU ABI library (native path).
// ============================================================================
//  Implements the KainComponentSurface trait using the wgpu-native C API
//  (or Dawn). The actual wgpuCreate* calls live here, not in the runtime
//  shim. Mirrors the cuda_runtime.c / libkain-cuda-abi.dll pattern.
//
//  Loader strategy (native):
//    1. dlopen("libwgpu_native.so") / LoadLibraryA("wgpu_native.dll")
//    2. Fallback: dlopen("libdawn.so") / LoadLibraryA("dawn.dll")
//
//  Loader strategy (WASM):
//    This file is NOT compiled for WASM — webgpu_abi_wasm.c is used instead.
//    See the __wasm__ guard at the top of this file.
// ============================================================================

#ifndef __wasm__

#include "webgpu_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#define KAIN_WEBGPU_NATIVE_LIB_PRIMARY   "wgpu_native.dll"
#define KAIN_WEBGPU_NATIVE_LIB_FALLBACK  "dawn.dll"
typedef HMODULE KainWebgpuNativeLib;
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#define KAIN_WEBGPU_NATIVE_LIB_PRIMARY   "libwgpu_native.dylib"
#define KAIN_WEBGPU_NATIVE_LIB_FALLBACK  "libdawn.dylib"
#else
#define KAIN_WEBGPU_NATIVE_LIB_PRIMARY   "libwgpu_native.so"
#define KAIN_WEBGPU_NATIVE_LIB_FALLBACK  "libdawn.so"
#endif
typedef void* KainWebgpuNativeLib;
#endif

// ============================================================================
//  Section 1: Dynamic loader
// ============================================================================
//  All wgpu-native entry points are resolved at runtime — we never link the
//  SDK. The struct KainWebgpuPfnTable mirrors the prototype set declared
//  in webgpu_loader_subset.h. Every member is a function pointer matching
//  the corresponding wgpu* symbol.
// ============================================================================

/* Field names match the wgpu* symbol name verbatim so the
   WEBGPU_PFN(name) macro can resolve both the symbol AND the field with
   the same token. e.g. WEBGPU_PFN(wgpuCreateInstance) stores the symbol
   "wgpuCreateInstance" into the field g_pfn.wgpuCreateInstance.    */
typedef struct KainWebgpuPfnTable {
    /* Instance */
    WGPUInstance  (*wgpuCreateInstance)(const void*);
    void          (*wgpuInstanceRelease)(WGPUInstance);
    WGPUSurface   (*wgpuInstanceCreateSurface)(WGPUInstance, const void*);
    void          (*wgpuInstanceProcessEvents)(WGPUInstance);

    /* Adapter */
    void          (*wgpuInstanceRequestAdapter)(WGPUInstance, const void*,
                                                  void (*)(uint32_t, WGPUAdapter,
                                                           const char*, void*),
                                                  void*);
    void          (*wgpuAdapterRelease)(WGPUAdapter);

    /* Device */
    void          (*wgpuAdapterRequestDevice)(WGPUAdapter, const void*,
                                              void (*)(uint32_t, WGPUDevice,
                                                       const char*, void*),
                                              void*);
    void          (*wgpuDeviceRelease)(WGPUDevice);
    WGPUQueue     (*wgpuDeviceGetQueue)(WGPUDevice);

    /* Swapchain */
    WGPUSwapChain    (*wgpuDeviceCreateSwapChain)(WGPUDevice, WGPUSurface, const void*);
    void             (*wgpuSwapChainRelease)(WGPUSwapChain);
    WGPUTextureView  (*wgpuSwapChainGetCurrentTextureView)(WGPUSwapChain);
    void             (*wgpuSwapChainPresent)(WGPUSwapChain);

    /* Command encoding */
    WGPUCommandEncoder    (*wgpuDeviceCreateCommandEncoder)(WGPUDevice, const void*);
    void                  (*wgpuCommandEncoderRelease)(WGPUCommandEncoder);
    WGPURenderPassEncoder (*wgpuCommandEncoderBeginRenderPass)(WGPUCommandEncoder, const void*);
    void                  (*wgpuRenderPassEncoderEnd)(WGPURenderPassEncoder);
    void                  (*wgpuRenderPassEncoderRelease)(WGPURenderPassEncoder);
    WGPUCommandBuffer     (*wgpuCommandEncoderFinish)(WGPUCommandEncoder, const void*);
    void                  (*wgpuCommandBufferRelease)(WGPUCommandBuffer);

    /* Clear */
    void (*wgpuRenderPassEncoderClearColor)(WGPURenderPassEncoder, const void*);

    /* Submit */
    void (*wgpuQueueSubmit)(WGPUQueue, uint32_t, const WGPUCommandBuffer*);
    void (*wgpuQueueRelease)(WGPUQueue);

    /* Shader (optional) */
    WGPUShaderModule (*wgpuDeviceCreateShaderModule)(WGPUDevice, const void*);
    void             (*wgpuShaderModuleRelease)(WGPUShaderModule);

    /* Pipeline (optional) */
    WGPURenderPipeline (*wgpuDeviceCreateRenderPipeline)(WGPUDevice, const void*);
    void               (*wgpuRenderPipelineRelease)(WGPURenderPipeline);

    /* Buffer (optional) */
    WGPUBuffer (*wgpuDeviceCreateBuffer)(WGPUDevice, const void*);
    void       (*wgpuBufferRelease)(WGPUBuffer);

    /* Bind group (optional) */
    WGPUBindGroupLayout (*wgpuDeviceCreateBindGroupLayout)(WGPUDevice, const void*);
    WGPUBindGroup       (*wgpuDeviceCreateBindGroup)(WGPUDevice, const void*);
    WGPUPipelineLayout  (*wgpuDeviceCreatePipelineLayout)(WGPUDevice, const void*);

    /* Reserved for ABI growth (pad to keep struct layout stable) */
    void* (*_reserved[8])(void);
} KainWebgpuPfnTable;

static KainWebgpuNativeLib   g_native_lib = NULL;
static KainWebgpuPfnTable    g_pfn        = { 0 };
static int                   g_loader_ready = 0;

// ── dlsym / GetProcAddress wrapper ─────────────────────────────────────

static void* webgpu_resolve_symbol(KainWebgpuNativeLib lib, const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress(lib, name);
#else
    return dlsym(lib, name);
#endif
}

static KainWebgpuNativeLib webgpu_open_lib(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void webgpu_close_lib(KainWebgpuNativeLib lib) {
    if (!lib) return;
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

// ── Resolve a single PFN, fatal if missing (counts as loader failure) ──

static int webgpu_load_pfn(KainWebgpuNativeLib lib,
                            const char* name,
                            void** out) {
    void* sym = webgpu_resolve_symbol(lib, name);
    if (!sym) {
        return 0;
    }
    *out = sym;
    return 1;
}

#define WEBGPU_PFN(name) \
    webgpu_load_pfn(g_native_lib, #name, (void**)&g_pfn.name)

static int webgpu_load_all_pfns(void) {
    memset(&g_pfn, 0, sizeof(g_pfn));

    /* Instance */
    if (!WEBGPU_PFN(wgpuCreateInstance))                 return 0;
    if (!WEBGPU_PFN(wgpuInstanceRelease))                return 0;
    if (!WEBGPU_PFN(wgpuInstanceCreateSurface))           return 0;
    if (!WEBGPU_PFN(wgpuInstanceProcessEvents))           return 0;

    /* Adapter */
    if (!WEBGPU_PFN(wgpuInstanceRequestAdapter))          return 0;
    if (!WEBGPU_PFN(wgpuAdapterRelease))                  return 0;

    /* Device */
    if (!WEBGPU_PFN(wgpuAdapterRequestDevice))            return 0;
    if (!WEBGPU_PFN(wgpuDeviceRelease))                   return 0;
    if (!WEBGPU_PFN(wgpuDeviceGetQueue))                  return 0;

    /* Swapchain */
    if (!WEBGPU_PFN(wgpuDeviceCreateSwapChain))           return 0;
    if (!WEBGPU_PFN(wgpuSwapChainRelease))                return 0;
    if (!WEBGPU_PFN(wgpuSwapChainGetCurrentTextureView))  return 0;
    if (!WEBGPU_PFN(wgpuSwapChainPresent))                return 0;

    /* Command encoding */
    if (!WEBGPU_PFN(wgpuDeviceCreateCommandEncoder))      return 0;
    if (!WEBGPU_PFN(wgpuCommandEncoderRelease))           return 0;
    if (!WEBGPU_PFN(wgpuCommandEncoderBeginRenderPass))  return 0;
    if (!WEBGPU_PFN(wgpuRenderPassEncoderEnd))            return 0;
    if (!WEBGPU_PFN(wgpuRenderPassEncoderRelease))        return 0;
    if (!WEBGPU_PFN(wgpuCommandEncoderFinish))            return 0;
    if (!WEBGPU_PFN(wgpuCommandBufferRelease))            return 0;

    /* Clear */
    if (!WEBGPU_PFN(wgpuRenderPassEncoderClearColor))     return 0;

    /* Submit */
    if (!WEBGPU_PFN(wgpuQueueSubmit))                     return 0;
    if (!WEBGPU_PFN(wgpuQueueRelease))                    return 0;

    /* Shader / Pipeline (optional for MVP — load if available) */
    webgpu_load_pfn(g_native_lib, "wgpuDeviceCreateShaderModule",
                     (void**)&g_pfn.wgpuDeviceCreateShaderModule);
    webgpu_load_pfn(g_native_lib, "wgpuShaderModuleRelease",
                     (void**)&g_pfn.wgpuShaderModuleRelease);
    webgpu_load_pfn(g_native_lib, "wgpuDeviceCreateRenderPipeline",
                     (void**)&g_pfn.wgpuDeviceCreateRenderPipeline);
    webgpu_load_pfn(g_native_lib, "wgpuRenderPipelineRelease",
                     (void**)&g_pfn.wgpuRenderPipelineRelease);
    webgpu_load_pfn(g_native_lib, "wgpuDeviceCreateBuffer",
                     (void**)&g_pfn.wgpuDeviceCreateBuffer);
    webgpu_load_pfn(g_native_lib, "wgpuBufferRelease",
                     (void**)&g_pfn.wgpuBufferRelease);
    webgpu_load_pfn(g_native_lib, "wgpuDeviceCreateBindGroupLayout",
                     (void**)&g_pfn.wgpuDeviceCreateBindGroupLayout);
    webgpu_load_pfn(g_native_lib, "wgpuDeviceCreateBindGroup",
                     (void**)&g_pfn.wgpuDeviceCreateBindGroup);
    webgpu_load_pfn(g_native_lib, "wgpuDeviceCreatePipelineLayout",
                     (void**)&g_pfn.wgpuDeviceCreatePipelineLayout);

    return 1;
}

static int webgpu_open_native_lib(void) {
    if (g_loader_ready) return 1;
    if (g_native_lib)   return 1;

    /* Try primary name first */
    g_native_lib = webgpu_open_lib(KAIN_WEBGPU_NATIVE_LIB_PRIMARY);
    if (!g_native_lib) {
        /* Fallback: alternate vendor name */
        g_native_lib = webgpu_open_lib(KAIN_WEBGPU_NATIVE_LIB_FALLBACK);
    }
    if (!g_native_lib) {
        return 0;
    }

    if (!webgpu_load_all_pfns()) {
        webgpu_close_lib(g_native_lib);
        g_native_lib = NULL;
        return 0;
    }

    g_loader_ready = 1;
    return 1;
}

// ============================================================================
//  Section 2: Session table + helpers
// ============================================================================

static KainWebgpuSession g_sessions[KAIN_WEBGPU_MAX_SESSIONS];
static int               g_session_count = 0;
static int64_t           g_next_session_id = 1;

static void webgpu_set_error(const char* msg) {
    /* Set on every vtable slot — but we only have one vtable (singleton). */
    /* Stored statically in the vtable struct.                                  */
    extern KainWebgpuAbiVtable g_webgpu_abi_vtable;
    snprintf(g_webgpu_abi_vtable.last_error,
             KAIN_WEBGPU_STATUS_MESSAGE_MAX,
             "%s", msg ? msg : "unknown error");
    g_webgpu_abi_vtable.last_status = -1;
}

static KainWebgpuSession* webgpu_find_session(int64_t id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < KAIN_WEBGPU_MAX_SESSIONS; ++i) {
        if (g_sessions[i].initialized && g_sessions[i].session_id == id) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static KainWebgpuSession* webgpu_alloc_session(void) {
    for (int i = 0; i < KAIN_WEBGPU_MAX_SESSIONS; ++i) {
        if (!g_sessions[i].initialized) {
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
            g_sessions[i].session_id = g_next_session_id++;
            g_sessions[i].initialized = 1;
            g_session_count++;
            return &g_sessions[i];
        }
    }
    return NULL;
}

static void webgpu_free_session(KainWebgpuSession* s) {
    if (!s) return;
    s->initialized = 0;
    s->session_id = 0;
    if (g_session_count > 0) g_session_count--;
}

// ── Async callback adapters ──────────────────────────────────────────
//  wgpu-native uses C-style callbacks for adapter/device request. We
//  marshal a (status, result, message) tuple back to the calling thread
//  via a small context struct.

typedef struct WebgpuAdapterRequestCtx {
    WGPUAdapter adapter;
    int         done;
} WebgpuAdapterRequestCtx;

static void webgpu_on_adapter(uint32_t status, WGPUAdapter adapter,
                                const char* message, void* userdata) {
    WebgpuAdapterRequestCtx* ctx = (WebgpuAdapterRequestCtx*)userdata;
    (void)message;
    if (status == 0) {
        ctx->adapter = adapter;
    }
    ctx->done = 1;
}

typedef struct WebgpuDeviceRequestCtx {
    WGPUDevice device;
    int        done;
} WebgpuDeviceRequestCtx;

static void webgpu_on_device(uint32_t status, WGPUDevice device,
                              const char* message, void* userdata) {
    WebgpuDeviceRequestCtx* ctx = (WebgpuDeviceRequestCtx*)userdata;
    (void)message;
    if (status == 0) {
        ctx->device = device;
    }
    ctx->done = 1;
}

/* Busy-wait fallback for callback delivery. wgpu-native calls callbacks
   synchronously on the requesting thread when the implementation
   supports it, so this loop rarely spins.                          */
static void webgpu_drain_events(WGPUInstance instance) {
    if (instance && g_pfn.wgpuInstanceProcessEvents) {
        g_pfn.wgpuInstanceProcessEvents(instance);
    }
}

static WGPUAdapter webgpu_request_adapter_blocking(WGPUInstance instance) {
    WebgpuAdapterRequestCtx ctx = { 0, 0 };
    /* WGPURequestAdapterOptions — powerPreference = HighPerformance = 1 */
    static const uint32_t options_buffer[8] = {
        0x00000001u, /* next = 0, compatibleSurface = 0, power = HighPerformance */
        0, 0, 0, 0, 0, 0, 0
    };
    g_pfn.wgpuInstanceRequestAdapter(instance, options_buffer,
                                  webgpu_on_adapter, &ctx);
    /* Drain until callback fires */
    int spins = 0;
    while (!ctx.done && spins < 1000000) {
        webgpu_drain_events(instance);
        spins++;
    }
    if (!ctx.done) {
        webgpu_set_error("wgpuInstanceRequestAdapter timed out");
        return 0;
    }
    if (ctx.adapter == 0) {
        webgpu_set_error("wgpuInstanceRequestAdapter returned no adapter");
        return 0;
    }
    return ctx.adapter;
}

static WGPUDevice webgpu_request_device_blocking(WGPUAdapter adapter,
                                                   WGPUDevice device_already) {
    if (device_already) return device_already;

    WebgpuDeviceRequestCtx ctx = { 0, 0 };
    static const uint32_t desc_buffer[8] = {
        0x00000001u, /* next = 0, label = 0, defaultQueue = 0, ... */
        0, 0, 0, 0, 0, 0, 0
    };
    g_pfn.wgpuAdapterRequestDevice(adapter, desc_buffer,
                                 webgpu_on_device, &ctx);
    int spins = 0;
    while (!ctx.done && spins < 1000000) {
        spins++;
    }
    if (!ctx.done || ctx.device == 0) {
        webgpu_set_error("wgpuAdapterRequestDevice timed out or returned no device");
        return 0;
    }
    return ctx.device;
}

// ============================================================================
//  Section 3: Surface + Swapchain creation
// ============================================================================
//  Surface descriptor is platform-specific. We build the right descriptor
//  shape per OS and pass it through wgpuInstanceCreateSurface.
// ============================================================================

static WGPUSurface webgpu_create_surface(KainWebgpuSession* s) {
    if (!s || !s->instance) return 0;

    /* The WGPUSurfaceDescriptor struct is opaque to us. The exact layout
       is implementation-specific (wgpu-native and Dawn differ). We allocate
       a small buffer with the platform handle at known offsets.

       The descriptor layout for the most common platforms (Win32, X11, Wayland,
       macOS) places a chained struct pointer at offset 0 (next-in-chain)
       followed by the label. The chained struct (WGPUSurfaceSource*) carries
       the native handle.                                                    */
    uint8_t desc[256];
    memset(desc, 0, sizeof(desc));

    /* Generic descriptor header — next-in-chain pointer at offset 0.
       wgpu-native interprets this as WGPUSurfaceDescriptor.nextInChain.    */
    void* chained = (void*)(desc + 64);
    *(void**)(desc + 0) = chained;       /* nextInChain */
    *(const char**)(desc + 8) = NULL;   /* label       */

    /* WGPUSurfaceSource* — variant tag at byte 0 of the chained struct. */
    uint8_t* chain = (uint8_t*)chained;
    memset(chain, 0, 64);

#ifdef _WIN32
    /* WGPUSurfaceSourceWindowsHWND: chain.st = 5 (WindowsHWND) */
    *(uint32_t*)(chain + 0)  = 5u;
    *(void**)(chain + 8)     = s->hinstance;
    *(void**)(chain + 16)    = s->hwnd;
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    /* WGPUSurfaceSourceWaylandSurface: chain.st = 3 */
    *(uint32_t*)(chain + 0)  = 3u;
    *(void**)(chain + 8)     = NULL;     /* wl_display — not captured here */
    *(void**)(chain + 16)    = NULL;     /* wl_surface */
#elif defined(__linux__)
    /* WGPUSurfaceSourceXlibWindow: chain.st = 1 */
    *(uint32_t*)(chain + 0)  = 1u;
    *(void**)(chain + 8)     = s->x11_display;
    *(uintptr_t*)(chain + 16) = s->x11_window;
#elif defined(__APPLE__)
    /* WGPUSurfaceSourceMetalLayer: chain.st = 4 */
    *(uint32_t*)(chain + 0)  = 4u;
    *(void**)(chain + 8)     = s->metal_layer;
#else
    webgpu_set_error("WebGPU surface creation: no platform handle provided");
    return 0;
#endif

    WGPUSurface surface = g_pfn.wgpuInstanceCreateSurface(s->instance, desc);
    if (!surface) {
        webgpu_set_error("wgpuInstanceCreateSurface returned NULL");
    }
    return surface;
}

static WGPUSwapChain webgpu_create_swapchain(KainWebgpuSession* s) {
    if (!s || !s->device || !s->surface) return 0;

    /* WGPUSwapChainDescriptor:
         nextInChain     @ 0   (NULL for basic usage)
         label           @ 8
         usage           @ 16  (WGPUTextureUsage_RenderAttachment = 0x10)
         format          @ 24  (WGPUTextureFormat_BGRA8Unorm = 0x0C)
         width           @ 28
         height          @ 32
         presentMode     @ 36  (WGPUPresentMode_Fifo = 2)            */
    uint8_t desc[64];
    memset(desc, 0, sizeof(desc));
    *(const char**)(desc + 8)  = NULL;
    *(uint64_t*)(desc + 16)    = 0x10ull;            /* RenderAttachment */
    *(uint32_t*)(desc + 24)    = 0x0Cu;              /* BGRA8Unorm      */
    *(uint32_t*)(desc + 28)    = (uint32_t)s->width;
    *(uint32_t*)(desc + 32)    = (uint32_t)s->height;
    *(uint32_t*)(desc + 36)    = 2u;                 /* Fifo            */

    WGPUSwapChain sc = g_pfn.wgpuDeviceCreateSwapChain(s->device, s->surface, desc);
    if (!sc) {
        webgpu_set_error("wgpuDeviceCreateSwapChain returned NULL");
    }
    return sc;
}

// ============================================================================
//  Section 4: Session lifecycle
// ============================================================================

static int64_t webgpu_session_create(const char* name, int64_t width, int64_t height) {
    if (!webgpu_open_native_lib()) {
        webgpu_set_error("failed to load wgpu-native / dawn library");
        return -1;
    }

    KainWebgpuSession* s = webgpu_alloc_session();
    if (!s) {
        webgpu_set_error("no free WebGPU session slot");
        return -2;
    }

    s->name   = name;
    s->width  = width  > 0 ? width  : 800;
    s->height = height > 0 ? height : 600;

    /* Step 1 — Instance */
    s->instance = g_pfn.wgpuCreateInstance(NULL);
    if (!s->instance) {
        webgpu_set_error("wgpuCreateInstance returned NULL");
        webgpu_free_session(s);
        return -3;
    }

    /* Step 2 — Adapter */
    s->adapter = webgpu_request_adapter_blocking(s->instance);
    if (!s->adapter) {
        if (g_pfn.wgpuInstanceRelease) g_pfn.wgpuInstanceRelease(s->instance);
        webgpu_free_session(s);
        return -4;
    }

    /* Step 3 — Device + Queue */
    s->device = webgpu_request_device_blocking(s->adapter, 0);
    if (!s->device) {
        if (g_pfn.wgpuAdapterRelease) g_pfn.wgpuAdapterRelease(s->adapter);
        if (g_pfn.wgpuInstanceRelease) g_pfn.wgpuInstanceRelease(s->instance);
        webgpu_free_session(s);
        return -5;
    }
    s->queue = g_pfn.wgpuDeviceGetQueue(s->device);
    if (!s->queue) {
        webgpu_set_error("wgpuDeviceGetQueue returned NULL");
    }

    /* Surface + swapchain are created lazily after session_attach_platform
       delivers the native window handle.                                */
    return s->session_id;
}

static void webgpu_session_destroy(int64_t session_id) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s) return;

    /* Release per-frame resources */
    if (s->command_buffer && g_pfn.wgpuCommandBufferRelease) {
        g_pfn.wgpuCommandBufferRelease(s->command_buffer);
    }
    if (s->command_encoder && g_pfn.wgpuCommandEncoderRelease) {
        g_pfn.wgpuCommandEncoderRelease(s->command_encoder);
    }
    if (s->render_pass && g_pfn.wgpuRenderPassEncoderRelease) {
        g_pfn.wgpuRenderPassEncoderRelease(s->render_pass);
    }
    if (s->swapchain && g_pfn.wgpuSwapChainRelease) {
        g_pfn.wgpuSwapChainRelease(s->swapchain);
    }
    if (s->queue && g_pfn.wgpuQueueRelease) {
        g_pfn.wgpuQueueRelease(s->queue);
    }
    if (s->device && g_pfn.wgpuDeviceRelease) {
        g_pfn.wgpuDeviceRelease(s->device);
    }
    if (s->adapter && g_pfn.wgpuAdapterRelease) {
        g_pfn.wgpuAdapterRelease(s->adapter);
    }
    if (s->instance && g_pfn.wgpuInstanceRelease) {
        g_pfn.wgpuInstanceRelease(s->instance);
    }

    webgpu_free_session(s);
}

static void webgpu_session_attach_platform(int64_t session_id, void* platform_handle) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s || !platform_handle) {
        webgpu_set_error("session_attach_platform: invalid args");
        return;
    }

    /* KainPlatformSurfaceHandle layout (component_surface.h): */
#ifdef _WIN32
    s->hinstance = ((KainPlatformSurfaceHandle*)platform_handle)->hinstance;
    s->hwnd      = ((KainPlatformSurfaceHandle*)platform_handle)->hwnd;
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    (void)platform_handle;
#elif defined(__linux__)
    s->x11_display = ((KainPlatformSurfaceHandle*)platform_handle)->x11_display;
    s->x11_window  = ((KainPlatformSurfaceHandle*)platform_handle)->x11_window;
#elif defined(__APPLE__)
    s->metal_layer = ((KainPlatformSurfaceHandle*)platform_handle)->metal_layer;
#else
    (void)platform_handle;
#endif

    /* Now that we have the platform handle, create surface + swapchain. */
    s->surface = webgpu_create_surface(s);
    if (!s->surface) {
        return;
    }
    s->swapchain = webgpu_create_swapchain(s);
}

// ============================================================================
//  Section 5: Element tree (no-op stubs — surface-agnostic placeholders)
// ============================================================================
//  The WebGPU surface is a GPU presenter, not a UI tree manager. The
//  compiler calls into KainComponentSurface.element_* to walk the tree,
//  but the actual UI work happens in the renderer layer. For now we
//  return stable IDs and let the existing graphics pipeline handle the
//  GPU commands.
// ============================================================================

static int64_t webgpu_element_begin(int64_t session_id, int64_t parent_id,
                                      const char* kind, const char* stable_key) {
    (void)parent_id; (void)kind; (void)stable_key;
    if (!webgpu_find_session(session_id)) return -1;
    /* Return a synthesized element ID for the tree walk. The GPU
       presenter doesn't need persistent element state.            */
    return session_id * 1000000 + 1;
}

static void webgpu_element_end(int64_t session_id, int64_t element_id) {
    (void)session_id; (void)element_id;
    /* no-op — element tree is consumed by the renderer */
}

static void webgpu_element_set_text(int64_t session_id, int64_t element_id,
                                      const char* text) {
    (void)session_id; (void)element_id; (void)text;
}

static void webgpu_element_set_attr_i64(int64_t session_id, int64_t element_id,
                                          const char* key, int64_t value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void webgpu_element_set_attr_f64(int64_t session_id, int64_t element_id,
                                          const char* key, double value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void webgpu_element_set_attr_string(int64_t session_id, int64_t element_id,
                                             const char* key, const char* value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static int64_t webgpu_state_get_i64(int64_t session_id, const char* key) {
    (void)session_id; (void)key;
    return 0;
}

static void webgpu_state_set_i64(int64_t session_id, const char* key, int64_t value) {
    (void)session_id; (void)key; (void)value;
}

// ============================================================================
//  Section 6: Frame lifecycle
// ============================================================================
//  begin_frame  — create command encoder
//  end_frame    — record clear, finish command buffer
//  present      — submit to queue, present swapchain
// ============================================================================

static void webgpu_begin_frame(int64_t session_id, double delta_ms) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s) {
        webgpu_set_error("begin_frame: session not found");
        return;
    }
    if (!s->device || !s->swapchain) {
        /* Surface not attached yet — silently skip. */
        return;
    }
    (void)delta_ms;

    /* Create a fresh command encoder for this frame. */
    s->command_encoder = g_pfn.wgpuDeviceCreateCommandEncoder(s->device, NULL);
    if (!s->command_encoder) {
        webgpu_set_error("wgpuDeviceCreateCommandEncoder returned NULL");
        return;
    }

    /* Acquire the current swapchain texture view. */
    WGPUTextureView view = g_pfn.wgpuSwapChainGetCurrentTextureView(s->swapchain);
    if (!view) {
        webgpu_set_error("wgpuSwapChainGetCurrentTextureView returned NULL");
        return;
    }

    /* WGPURenderPassDescriptor:
         nextInChain        @ 0
         label              @ 8
         colorAttachmentCount @ 16
         colorAttachments   @ 24  (pointer to array of WGPURenderPassColorAttachment)
         depthStencilAttachment @ 32 (NULL for MVP)                       */
    uint8_t rp_desc[48];
    memset(rp_desc, 0, sizeof(rp_desc));
    *(const char**)(rp_desc + 8) = NULL;
    *(uint32_t*)(rp_desc + 16)   = 1u;

    /* WGPURenderPassColorAttachment array (one element) — separate buffer */
    uint8_t color_att[64];
    memset(color_att, 0, sizeof(color_att));
    *(WGPUTextureView*)(color_att + 0)  = view;       /* view             */
    *(uint32_t*)(color_att + 8)         = 0;          /* resolveTarget=0  */
    *(uint32_t*)(color_att + 16)        = 1u;         /* loadOp = Clear   */
    *(uint32_t*)(color_att + 20)        = 1u;         /* storeOp = Store  */
    *(void**)(color_att + 24)           = NULL;       /* clearValue ptr   */

    /* WGPUColor is r,g,b,a — 4× float32 */
    static const float clear_color[4] = { 0.05f, 0.07f, 0.10f, 1.0f };
    *(const float**)(color_att + 24)   = clear_color;

    *(void**)(rp_desc + 24)            = color_att;   /* colorAttachments */

    s->render_pass =
        g_pfn.wgpuCommandEncoderBeginRenderPass(s->command_encoder, rp_desc);
    if (!s->render_pass) {
        webgpu_set_error("wgpuCommandEncoderBeginRenderPass returned NULL");
        return;
    }

    s->has_frame_in_flight = 1;
}

static void webgpu_end_frame(int64_t session_id) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s || !s->has_frame_in_flight) return;
    if (!s->command_encoder) return;

    /* Close the render pass. */
    if (s->render_pass) {
        g_pfn.wgpuRenderPassEncoderEnd(s->render_pass);
        g_pfn.wgpuRenderPassEncoderRelease(s->render_pass);
        s->render_pass = 0;
    }

    /* Finish the command buffer. */
    s->command_buffer = g_pfn.wgpuCommandEncoderFinish(s->command_encoder, NULL);
    g_pfn.wgpuCommandEncoderRelease(s->command_encoder);
    s->command_encoder = 0;
}

static void webgpu_present(int64_t session_id) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s || !s->has_frame_in_flight) return;

    /* Submit */
    if (s->queue && s->command_buffer) {
        g_pfn.wgpuQueueSubmit(s->queue, 1, &s->command_buffer);
        g_pfn.wgpuCommandBufferRelease(s->command_buffer);
        s->command_buffer = 0;
    }

    /* Present */
    if (s->swapchain) {
        g_pfn.wgpuSwapChainPresent(s->swapchain);
    }

    s->has_frame_in_flight = 0;

    /* Bump telemetry on the vtable. */
    extern KainWebgpuAbiVtable g_webgpu_abi_vtable;
    g_webgpu_abi_vtable.present_count++;
    g_webgpu_abi_vtable.last_status = 0;
}

// ============================================================================
//  Section 7: Event pump + window lifecycle (no-ops for GPU presenter)
// ============================================================================

static int64_t webgpu_poll_event(int64_t session_id, void* out_event, int64_t max_size) {
    (void)session_id; (void)out_event; (void)max_size;
    return 0;
}

static int64_t webgpu_should_close(int64_t session_id) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s) return 1;
    return s->should_close;
}

static int64_t webgpu_window_open(int64_t session_id, const char* title,
                                    int64_t width, int64_t height) {
    (void)title;
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s) return -1;
    s->width  = width;
    s->height = height;
    /* Recreate swapchain if dimensions changed. */
    if (s->swapchain && g_pfn.wgpuSwapChainRelease) {
        g_pfn.wgpuSwapChainRelease(s->swapchain);
        s->swapchain = 0;
    }
    if (s->device && s->surface) {
        s->swapchain = webgpu_create_swapchain(s);
    }
    return 0;
}

static int64_t webgpu_host_pump(int64_t session_id) {
    KainWebgpuSession* s = webgpu_find_session(session_id);
    if (!s || !s->instance) return 0;
    if (g_pfn.wgpuInstanceProcessEvents) {
        g_pfn.wgpuInstanceProcessEvents(s->instance);
    }
    return 0;
}

// ============================================================================
//  Section 8: Init / shutdown
// ============================================================================

int kain_webgpu_abi_init(void) {
    if (g_loader_ready) return 0;
    if (!webgpu_open_native_lib()) return -1;
    return 0;
}

void kain_webgpu_abi_shutdown(void) {
    /* Destroy any active sessions. */
    for (int i = 0; i < KAIN_WEBGPU_MAX_SESSIONS; ++i) {
        if (g_sessions[i].initialized) {
            webgpu_session_destroy(g_sessions[i].session_id);
        }
    }
    if (g_native_lib) {
        webgpu_close_lib(g_native_lib);
        g_native_lib = NULL;
    }
    g_loader_ready = 0;
    memset(&g_pfn, 0, sizeof(g_pfn));
}

// ============================================================================
//  Section 9: Static vtable instance + entry point
// ============================================================================
//  This is the only symbol the runtime shim looks up via dlsym/GetProcAddress.
// ============================================================================

KainWebgpuAbiVtable g_webgpu_abi_vtable = {
    .surface = {
        .session_create          = webgpu_session_create,
        .session_destroy         = webgpu_session_destroy,
        .element_begin           = webgpu_element_begin,
        .element_end             = webgpu_element_end,
        .element_set_text        = webgpu_element_set_text,
        .element_set_attr_i64    = webgpu_element_set_attr_i64,
        .element_set_attr_f64    = webgpu_element_set_attr_f64,
        .element_set_attr_string = webgpu_element_set_attr_string,
        .state_get_i64           = webgpu_state_get_i64,
        .state_set_i64           = webgpu_state_set_i64,
        .begin_frame             = webgpu_begin_frame,
        .end_frame               = webgpu_end_frame,
        .present                 = webgpu_present,
        .poll_event              = webgpu_poll_event,
        .should_close            = webgpu_should_close,
        .window_open             = webgpu_window_open,
        .host_pump               = webgpu_host_pump,
        .session_attach_platform = webgpu_session_attach_platform,
    },
    .abi_version     = KAIN_WEBGPU_ABI_VERSION,
    .present_count   = 0,
    .last_status     = 0,
    .last_error      = { 0 },
};

const KainWebgpuAbiVtable* kain_webgpu_abi_get_vtable(void) {
    return &g_webgpu_abi_vtable;
}

#endif /* !__wasm__ */
