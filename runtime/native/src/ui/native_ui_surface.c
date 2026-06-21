// ============================================================================
//  native_ui_surface.c — Reference native_ui KainComponentSurface backend.
// ============================================================================
//  Implements the KainComponentSurface trait using the existing ui_system.h
//  retained-mode UI runtime. This is ecosystem code (blades/kaintana/), NOT
//  part of the runtime core.
//
//  The surface wraps every abi_ui_* call through the trait vtable. The compiler
//  never calls abi_ui_* directly — it calls through KainComponentSurface,
//  making the UI backend swappable (native_ui, web, headless, tui, ...).
//
//  Registration:
//    kain_component_surface_register("native_ui", &native_ui_surface);
//
//  Key design decisions:
//    - element_begin: find-or-create via stable key for reconciliation
//    - element_end:   no-op (ui_system retains nodes across frames)
//    - State:         persisted on a hidden "__kain_state_root" node
//    - Style keys:    mapped from abstract keys to ui_system operations
//    - All other ops: delegated directly to abi_ui_* functions
// ============================================================================

#include "ui_system.h"
#include "component_surface.h"
#include <string.h>

// ============================================================================
//  Forward declarations — wrappers for abi_ui_* signature mismatches.
// ============================================================================
//  The trait uses `void` return for fire-and-forget operations, but abi_ui_*
//  functions return int64_t status codes. We wrap them to match the trait.

static void wrap_session_destroy(int64_t session_id);
static void wrap_element_set_text(int64_t session_id, int64_t element_id, const char* text);
static void wrap_set_attr_i64(int64_t session_id, int64_t element_id, const char* key, int64_t value);
static void wrap_set_attr_f64(int64_t session_id, int64_t element_id, const char* key, double value);
static void wrap_set_attr_string(int64_t session_id, int64_t element_id, const char* key, const char* value);
static void wrap_begin_frame(int64_t session_id, double delta_ms);
static void wrap_end_frame(int64_t session_id);
static int64_t native_ui_session_create(const char* name, int64_t width, int64_t height);
static void native_ui_present(int64_t session_id);
static int64_t wrap_poll_event(int64_t session_id, void* out_event, int64_t max_size);

// ============================================================================
//  element_begin — find-or-create via stable key for reconciliation.
// ============================================================================
//  On first frame: node doesn't exist → create, set stable key, set parent.
//  On subsequent frames: find by stable key → update parent, return existing.
//  This is how the retained-mode tree survives frame boundaries.

static int64_t native_ui_element_begin(int64_t session_id, int64_t parent_id,
                                        const char* kind, const char* stable_key) {
    // If we have a stable key, try to find an existing node first.
    if (stable_key && *stable_key) {
        int64_t existing = abi_ui_node_find_by_stable_key(session_id, stable_key);
        if (existing > 0) {
            // Node already exists from a previous frame — update parent and reuse.
            abi_ui_node_set_parent(session_id, existing, parent_id);
            return existing;
        }
    }

    // No existing node found — create a new one.
    int64_t node = abi_ui_node_create(session_id, kind);
    if (node <= 0) {
        return node; // creation failed — propagate error to caller
    }

    // Attach the stable key so future frames find this node.
    if (stable_key && *stable_key) {
        abi_ui_node_set_stable_key(session_id, node, stable_key);
    }

    // Link into the tree.
    abi_ui_node_set_parent(session_id, node, parent_id);

    return node;
}

// ============================================================================
//  element_end — no-op for retained-mode ui_system.
// ============================================================================
//  Nodes persist across frames in the retained tree. They are only destroyed
//  at session teardown or via explicit node_destroy. Ending an element just
//  completes the tree walk for this frame — no action needed.

static void native_ui_element_end(int64_t session_id, int64_t element_id) {
    (void)session_id;
    (void)element_id;
}

// ============================================================================
//  element_set_attr_string — map abstract keys to ui_system operations.
// ============================================================================
//  Known keys are forwarded to abi_ui_node_set_style_string. Unknown keys
//  are silently ignored — this allows future attribute additions without
//  breaking older backends.

static void native_ui_set_attr_string(int64_t session_id, int64_t element_id,
                                       const char* key, const char* value) {
    if (!key || !value) {
        return;
    }

    // Known style keys that map to string-valued ui_system style slots.
    if (strcmp(key, "fill_color")   == 0 ||
        strcmp(key, "border_color") == 0 ||
        strcmp(key, "ink_color")    == 0 ||
        strcmp(key, "title")        == 0) {
        abi_ui_node_set_style_string(session_id, element_id, key, value);
        return;
    }

    // Unknown string keys → silently ignored (future-proof).
}

// ============================================================================
//  State persistence — stored on a hidden "__kain_state_root" node.
// ============================================================================
//  Component `state` fields survive across frames by storing values as
//  node state on a hidden root node. The key format is "ComponentName:field_name".
//  The state root is created lazily on first access.

static int64_t get_state_root(int64_t session_id) {
    int64_t root = abi_ui_node_find_by_stable_key(session_id, "__kain_state_root");
    if (root <= 0) {
        root = abi_ui_node_create(session_id, "__kain_state");
        if (root > 0) {
            abi_ui_node_set_stable_key(session_id, root, "__kain_state_root");
            // Mark as hidden so it never appears in hit-tests, draw walks, or
            // bundle serialization. It exists purely for state persistence.
            abi_ui_node_set_flag(session_id, root, "hidden", 1);
        }
    }
    return root;
}

static int64_t native_ui_state_get_i64(int64_t session_id, const char* key) {
    int64_t root = get_state_root(session_id);
    if (root <= 0) {
        return 0; // state root creation failed — return default (0)
    }
    return abi_ui_node_state_i64(session_id, root, key, /*fallback=*/0);
}

static void native_ui_state_set_i64(int64_t session_id, const char* key, int64_t value) {
    int64_t root = get_state_root(session_id);
    if (root <= 0) {
        return; // state root creation failed — silently drop
    }
    abi_ui_node_set_state_i64(session_id, root, key, value);
}

// ============================================================================
//  Wrappers — bridge abi_ui_* return types to trait void signatures.
// ============================================================================
//  abi_ui_* functions return int64_t status codes, but the trait declares
//  these as void (fire-and-forget). We wrap them to discard the return value.
//  This avoids undefined behavior from calling through mismatched function
//  pointer types.

static void wrap_session_destroy(int64_t session_id) {
    abi_ui_session_destroy(session_id);
}

static void wrap_element_set_text(int64_t session_id, int64_t element_id,
                                   const char* text) {
    abi_ui_node_set_text(session_id, element_id, text);
}

static void wrap_set_attr_i64(int64_t session_id, int64_t element_id,
                               const char* key, int64_t value) {
    abi_ui_node_set_style_i64(session_id, element_id, key, value);
}

static void wrap_set_attr_f64(int64_t session_id, int64_t element_id,
                               const char* key, double value) {
    abi_ui_node_set_style_f64(session_id, element_id, key, value);
}

static void wrap_begin_frame(int64_t session_id, double delta_ms) {
    abi_ui_begin_frame(session_id, delta_ms);
}

static void wrap_end_frame(int64_t session_id) {
    abi_ui_end_frame(session_id);
}

// ============================================================================
//  session_create wrapper — auto-attaches winit backend on Win32.
// ============================================================================
//  abi_ui_session_create sets host_backend = "memory" (no OS window).
//  On Win32, we immediately attach the "winit" host adapter which calls
//  RegisterClassA + CreateWindowExA to create a real visible HWND.
//  Without this, the session is a headless memory buffer — the codegen's
//  frame loop renders into it but nothing appears on screen.

static int64_t native_ui_session_create(const char* name, int64_t width, int64_t height) {
    int64_t sid = abi_ui_session_create(name, width, height);
    if (sid <= 0) return sid;

#ifdef _WIN32
    // Auto-attach the "winit" backend to create a real OS window.
    // This calls: abi_ui_host_attach → abi_ui_host_adapter_attach("winit")
    //           → win32_host_create → RegisterClassA("KainWin32UI")
    //           → CreateWindowExA(WS_OVERLAPPEDWINDOW | WS_VISIBLE)
    //           → CreateDIBSection (GDI software framebuffer)
    // The window appears immediately after this call.
    abi_ui_host_attach(sid, "winit");
#endif
    return sid;
}

// ============================================================================
//  present wrapper — actually blits framebuffer to the OS window.
// ============================================================================
//  The old wrap_present only called abi_ui_present (counter update).
//  We also call abi_ui_host_present to blit the GDI framebuffer to the
//  visible window via InvalidateRect → WM_PAINT → BitBlt.

static void native_ui_present(int64_t session_id) {
    abi_ui_present(session_id);        // update frame counters, clear dirty
    abi_ui_host_present(session_id);   // blit framebuffer → InvalidateRect → BitBlt
}

// ============================================================================
//  poll_event wrapper — trait has extra params that ui_system doesn't use.
// ============================================================================
//  The trait signature is future-proofed with out_event/max_size for backends
//  that serialize events into a caller-owned buffer. ui_system stores events
//  internally and exposes them via accessor functions (abi_ui_event_kind, etc.).
//  We ignore out_event/max_size and delegate directly.

static int64_t wrap_poll_event(int64_t session_id, void* out_event, int64_t max_size) {
    (void)out_event;
    (void)max_size;
    return abi_ui_poll_event(session_id);
}

// ============================================================================
//  session_attach_platform — no-op stub for native_ui.
// ============================================================================
//  native_ui_surface wraps ui_system.h which creates its own window via
//  win32_host_create. Platform handles aren't consumed here — the host
//  adapter already owns the HWND. GPU backends (Vulkan, D3D12, WebGPU)
//  use this slot to receive the native window handle for WSI surface creation.

static void native_ui_session_attach_platform(int64_t session_id, void* platform_handle) {
    (void)session_id;
    (void)platform_handle;
}

// ============================================================================
//  Surface vtable — the full KainComponentSurface for native_ui.
// ============================================================================
//  Registered at startup via:
//    kain_component_surface_register("native_ui", &native_ui_surface);
//
//  The compiler resolves this once per world, then calls through the vtable
//  every frame. All function pointers are filled in — no NULL slots.

const KainComponentSurface native_ui_surface = {
    .session_create          = native_ui_session_create,
    .session_destroy         = wrap_session_destroy,
    .element_begin           = native_ui_element_begin,
    .element_end             = native_ui_element_end,
    .element_set_text        = wrap_element_set_text,
    .element_set_attr_i64    = wrap_set_attr_i64,
    .element_set_attr_f64    = wrap_set_attr_f64,
    .element_set_attr_string = native_ui_set_attr_string,
    .state_get_i64           = native_ui_state_get_i64,
    .state_set_i64           = native_ui_state_set_i64,
    .begin_frame             = wrap_begin_frame,
    .end_frame               = wrap_end_frame,
    .present                 = native_ui_present,
    .poll_event              = wrap_poll_event,
    .should_close            = abi_ui_host_should_close,
    .window_open             = abi_ui_window_open,
    .host_pump               = abi_ui_host_pump,
    .session_attach_platform = native_ui_session_attach_platform,
};

// ============================================================================
//  Auto-registration — runs before main() via static initializer.
//  Ensures the native_ui surface is available before any world-surface
//  frame loop begins, regardless of codegen call order.
// ============================================================================

#if defined(_WIN32)
#include <windows.h>
/* MSVC: CRT initializer section — runs before main() */
#pragma section(".CRT$XCU", read)
static void native_ui_surface_register_ctor(void) {
    kain_component_surface_register("native_ui", &native_ui_surface);
}
__declspec(allocate(".CRT$XCU"))
    void (*native_ui_surface_register_ptr)(void) = native_ui_surface_register_ctor;

#else
/* GCC/Clang: constructor attribute — runs before main() */
__attribute__((constructor))
static void native_ui_surface_register_ctor(void) {
    kain_component_surface_register("native_ui", &native_ui_surface);
}
#endif
