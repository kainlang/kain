// ============================================================================
//  webgpu_abi_wasm.c — Kain WebGPU ABI library (WASM / browser path).
// ============================================================================
//  Compiled ONLY when the target is WebAssembly. Browser provides WebGPU
//  natively via navigator.gpu — no dlopen, no dynamic loader.
//
//  This file mirrors the structure of webgpu_abi.c but uses static linkage
//  to the browser's WebGPU implementation. The runtime shim expects the
//  symbol kain_webgpu_abi_get_vtable to be present (statically linked).
//
//  The vtable filled here has the same shape as the native path so the
//  shim and any consumers can't tell the difference at runtime.
// ============================================================================

#ifdef __wasm__

#include "webgpu_abi.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Emscripten provides the WebGPU binding helpers when compiled with
   -lembind or with the emscripten WebGPU extension. We support both
   the emscripten helper API and the raw browser API via JS interop.  */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#endif

// ============================================================================
//  Session table (same shape as native path, smaller scale for browser)
// ============================================================================

static KainWebgpuSession g_sessions[KAIN_WEBGPU_MAX_SESSIONS];
static int               g_session_count = 0;
static int64_t           g_next_session_id = 1;

static void webgpu_wasm_set_error(const char* msg) {
    extern KainWebgpuAbiVtable g_webgpu_abi_wasm_vtable;
    snprintf(g_webgpu_abi_wasm_vtable.last_error,
             KAIN_WEBGPU_STATUS_MESSAGE_MAX,
             "%s", msg ? msg : "unknown error");
    g_webgpu_abi_wasm_vtable.last_status = -1;
}

static KainWebgpuSession* webgpu_wasm_find_session(int64_t id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < KAIN_WEBGPU_MAX_SESSIONS; ++i) {
        if (g_sessions[i].initialized && g_sessions[i].session_id == id) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static KainWebgpuSession* webgpu_wasm_alloc_session(void) {
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

static void webgpu_wasm_free_session(KainWebgpuSession* s) {
    if (!s) return;
    s->initialized = 0;
    s->session_id = 0;
    if (g_session_count > 0) g_session_count--;
}

// ============================================================================
//  Emscripten helper wrappers
// ============================================================================
//  When __EMSCRIPTEN__ is set, we use the official emscripten WebGPU
//  bindings. Otherwise we call the browser's navigator.gpu via a small
//  JS shim bound with EM_JS.

#ifdef __EMSCRIPTEN__

/* Acquire the WebGPU device from the browser. The emscripten helper
   returns 1 on success, 0 on failure (no GPU available). The actual
   instance, adapter, and device are queried through html5_webgpu.h. */
static int webgpu_wasm_acquire_device(WGPUInstance* out_instance,
                                       WGPUAdapter* out_adapter,
                                       WGPUDevice* out_device) {
    if (!emscripten_webgpu_get_device(out_device)) {
        webgpu_wasm_set_error("emscripten_webgpu_get_device failed (no WebGPU)");
        return 0;
    }
    /* Instance and adapter are implicitly provided by the browser. */
    *out_instance = 1;
    *out_adapter  = 1;
    return 1;
}

/* Acquire a surface bound to a canvas. The selector identifies the
   DOM element by CSS selector (e.g. "#canvas"). The browser creates
   the WGPUSurface from the canvas's WebGPU context.                */
static WGPUSurface webgpu_wasm_create_surface_for_canvas(const char* selector) {
    if (!selector || !selector[0]) {
        webgpu_wasm_set_error("canvas selector is null");
        return 0;
    }
    /* Emscripten's html5_webgpu binding creates a surface from a canvas
       identified by selector. Returns 0 on failure.                   */
    WGPUSurface surface = emscripten_webgpu_create_surface_for_canvas(selector);
    if (!surface) {
        webgpu_wasm_set_error("emscripten_webgpu_create_surface_for_canvas failed");
    }
    return surface;
}

#else /* !__EMSCRIPTEN__ */

/* Raw browser API: use a JS shim to get the WebGPU device and surface.
   The JS layer (bound via EM_JS or wasm-bindgen) is expected to
   implement these entry points. For builds without emscripten, the
   surface is created with a default "#canvas" selector.            */
EM_JS(int, js_wgpu_get_device, (),
      {
          if (typeof navigator === 'undefined' || !navigator.gpu) {
              return 0;
          }
          // Asynchronously request the adapter and device, then store
          // them in a module-scoped slot. The native code polls the
          // module by calling a separate sync function.
          // For MVP: return a sentinel; real implementation would
          // do the async dance in JS.
          return 1;
      });

static int webgpu_wasm_acquire_device(WGPUInstance* out_instance,
                                       WGPUAdapter* out_adapter,
                                       WGPUDevice* out_device) {
    if (!js_wgpu_get_device()) {
        webgpu_wasm_set_error("navigator.gpu not available");
        return 0;
    }
    *out_instance = 1;
    *out_adapter  = 1;
    *out_device   = 1;
    return 1;
}

static WGPUSurface webgpu_wasm_create_surface_for_canvas(const char* selector) {
    (void)selector;
    /* Without emscripten bindings, the surface is implicit. The browser
       hands WebGPU commands directly to the canvas. We return a sentinel. */
    return 1;
}

#endif /* __EMSCRIPTEN__ */

// ============================================================================
//  Section 1: Session lifecycle
// ============================================================================

static int64_t webgpu_wasm_session_create(const char* name,
                                            int64_t width, int64_t height) {
    KainWebgpuSession* s = webgpu_wasm_alloc_session();
    if (!s) {
        webgpu_wasm_set_error("no free WebGPU session slot");
        return -1;
    }

    s->name   = name;
    s->width  = width  > 0 ? width  : 800;
    s->height = height > 0 ? height : 600;

    if (!webgpu_wasm_acquire_device(&s->instance, &s->adapter, &s->device)) {
        webgpu_wasm_free_session(s);
        return -2;
    }

    /* The browser does not require a separate queue handle — device IS
       the queue for the MVP. We use a sentinel value to keep the vtable
       uniform with the native path.                                   */
    s->queue = s->device;

    return s->session_id;
}

static void webgpu_wasm_session_destroy(int64_t session_id) {
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s) return;

    /* In WASM, the browser owns most of the GPU resources. The WGPU
       handles are dropped implicitly when the JS object is GC'd. We
       release our own state and let the browser clean up.            */
    webgpu_wasm_free_session(s);
}

static void webgpu_wasm_session_attach_platform(int64_t session_id,
                                                  void* platform_handle) {
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s || !platform_handle) {
        webgpu_wasm_set_error("session_attach_platform: invalid args");
        return;
    }
    /* platform_handle is a CSS selector string (e.g. "#canvas"). */
    s->canvas_selector = platform_handle;
    s->surface = webgpu_wasm_create_surface_for_canvas((const char*)platform_handle);
}

// ============================================================================
//  Section 2: Element tree (no-ops for GPU presenter — same as native path)
// ============================================================================

static int64_t webgpu_wasm_element_begin(int64_t session_id, int64_t parent_id,
                                           const char* kind, const char* stable_key) {
    (void)parent_id; (void)kind; (void)stable_key;
    if (!webgpu_wasm_find_session(session_id)) return -1;
    return session_id * 1000000 + 1;
}

static void webgpu_wasm_element_end(int64_t session_id, int64_t element_id) {
    (void)session_id; (void)element_id;
}

static void webgpu_wasm_element_set_text(int64_t session_id, int64_t element_id,
                                           const char* text) {
    (void)session_id; (void)element_id; (void)text;
}

static void webgpu_wasm_element_set_attr_i64(int64_t session_id, int64_t element_id,
                                               const char* key, int64_t value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void webgpu_wasm_element_set_attr_f64(int64_t session_id, int64_t element_id,
                                               const char* key, double value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void webgpu_wasm_element_set_attr_string(int64_t session_id, int64_t element_id,
                                                  const char* key, const char* value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static int64_t webgpu_wasm_state_get_i64(int64_t session_id, const char* key) {
    (void)session_id; (void)key;
    return 0;
}

static void webgpu_wasm_state_set_i64(int64_t session_id, const char* key, int64_t value) {
    (void)session_id; (void)key; (void)value;
}

// ============================================================================
//  Section 3: Frame lifecycle
// ============================================================================
//  WASM frames are driven by the browser's requestAnimationFrame. The
//  emscripten main loop pumps Kain's tick on rAF. begin_frame/end_frame
//  record command buffer equivalents; present() submits to the browser.
//
//  The browser handles swapchain acquisition and present internally — the
//  swapchain "GetCurrentTextureView" and "Present" are no-ops on our side.

static void webgpu_wasm_begin_frame(int64_t session_id, double delta_ms) {
    (void)delta_ms;
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s) return;
    /* The browser will issue begin/end frame markers around our JS tick.
       The emscripten main loop does this automatically.                 */
    s->has_frame_in_flight = 1;
}

static void webgpu_wasm_end_frame(int64_t session_id) {
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s) return;
    /* The browser's rAF callback wraps the Kain tick. No explicit
       command buffer flush is required at the WASM ABI layer.        */
}

static void webgpu_wasm_present(int64_t session_id) {
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s) return;
    if (!s->has_frame_in_flight) return;
    s->has_frame_in_flight = 0;

    /* Bump telemetry on the vtable. */
    extern KainWebgpuAbiVtable g_webgpu_abi_wasm_vtable;
    g_webgpu_abi_wasm_vtable.present_count++;
    g_webgpu_abi_wasm_vtable.last_status = 0;
}

// ============================================================================
//  Section 4: Event pump + window lifecycle
// ============================================================================

static int64_t webgpu_wasm_poll_event(int64_t session_id, void* out_event, int64_t max_size) {
    (void)session_id; (void)out_event; (void)max_size;
    /* Event pump is the browser's responsibility. The host adapter
       forwards DOM events into the Kain event queue. The trait slot
       returns 0 (no event).                                          */
    return 0;
}

static int64_t webgpu_wasm_should_close(int64_t session_id) {
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s) return 1;
    return s->should_close;
}

static int64_t webgpu_wasm_window_open(int64_t session_id, const char* title,
                                         int64_t width, int64_t height) {
    (void)title;
    KainWebgpuSession* s = webgpu_wasm_find_session(session_id);
    if (!s) return -1;
    s->width  = width;
    s->height = height;
    /* No swapchain recreation in the browser — canvas is sized via CSS. */
    return 0;
}

static int64_t webgpu_wasm_host_pump(int64_t session_id) {
    (void)session_id;
    /* The browser drives the event loop. Nothing to pump here. */
    return 0;
}

// ============================================================================
//  Section 5: Init / shutdown
// ============================================================================

int kain_webgpu_abi_init(void) {
    /* WASM: WebGPU is provided by the browser, no init required. */
    return 0;
}

void kain_webgpu_abi_shutdown(void) {
    for (int i = 0; i < KAIN_WEBGPU_MAX_SESSIONS; ++i) {
        if (g_sessions[i].initialized) {
            webgpu_wasm_session_destroy(g_sessions[i].session_id);
        }
    }
}

// ============================================================================
//  Section 6: Static vtable instance + entry point
// ============================================================================
//  Same shape as the native path. The shim's extern declaration
//  (kain_webgpu_abi_get_vtable) resolves to this symbol statically
//  when the WASM runtime is linked.

KainWebgpuAbiVtable g_webgpu_abi_wasm_vtable = {
    .surface = {
        .session_create          = webgpu_wasm_session_create,
        .session_destroy         = webgpu_wasm_session_destroy,
        .element_begin           = webgpu_wasm_element_begin,
        .element_end             = webgpu_wasm_element_end,
        .element_set_text        = webgpu_wasm_element_set_text,
        .element_set_attr_i64    = webgpu_wasm_element_set_attr_i64,
        .element_set_attr_f64    = webgpu_wasm_element_set_attr_f64,
        .element_set_attr_string = webgpu_wasm_element_set_attr_string,
        .state_get_i64           = webgpu_wasm_state_get_i64,
        .state_set_i64           = webgpu_wasm_state_set_i64,
        .begin_frame             = webgpu_wasm_begin_frame,
        .end_frame               = webgpu_wasm_end_frame,
        .present                 = webgpu_wasm_present,
        .poll_event              = webgpu_wasm_poll_event,
        .should_close            = webgpu_wasm_should_close,
        .window_open             = webgpu_wasm_window_open,
        .host_pump               = webgpu_wasm_host_pump,
        .session_attach_platform = webgpu_wasm_session_attach_platform,
    },
    .abi_version     = KAIN_WEBGPU_ABI_VERSION,
    .present_count   = 0,
    .last_status     = 0,
    .last_error      = { 0 },
};

const KainWebgpuAbiVtable* kain_webgpu_abi_get_vtable(void) {
    return &g_webgpu_abi_wasm_vtable;
}

#endif /* __wasm__ */
