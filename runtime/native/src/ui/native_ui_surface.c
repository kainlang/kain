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
#include "../../include/ui_font.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
//  session_create wrapper — auto-attach winit host on Win32.
// ============================================================================
//  abi_ui_session_create sets host_backend = "memory" (no OS window).
//  We then attach the "winit" host to create a real visible HWND.
//
//  This function is only called from compiler-emitted frame loops, which are
//  only emitted for worlds that declare `surface native_ui => Component`.
//  The Kain source IS the rendering intent — no env var needed.
//
//  For GPU backends (Vulkan, D3D12, WebGPU), the component_surface.c
//  registry swaps the entire KainComponentSurface vtable at resolve time
//  (based on RENDERER_BACKEND env var or ABI library availability). Those
//  backends have their own session_create that creates swapchain windows.
//  This function handles the GDI software path exclusively.
//
//  Programs that don't want a window simply don't declare a surface on
//  their world. No surface → no frame loop → this function is never called.

// ── Default font loading ──────────────────────────────────────────
// Loads a platform TTF at session birth so the renderer's tree-walker
// can rasterize text nodes that have ink_color set. Path priority:
//   1. KAIN_UI_FONT env var (explicit override)
//   2. Platform default (segoeui.ttf / DejaVuSans.ttf / Helvetica)
// Returns the font resource ID, or 0 if no font could be loaded.
static int64_t native_ui_load_default_font(int64_t session_id) {
    const char* env_path = getenv("KAIN_UI_FONT");
    const char* paths[4] = { NULL, NULL, NULL, NULL };
    int path_count = 0;

    if (env_path && env_path[0]) {
        paths[path_count++] = env_path;
    }
#ifdef _WIN32
    paths[path_count++] = "C:/Windows/Fonts/segoeui.ttf";
    paths[path_count++] = "C:/Windows/Fonts/arial.ttf";
#elif defined(__APPLE__)
    paths[path_count++] = "/System/Library/Fonts/Helvetica.ttc";
    paths[path_count++] = "/Library/Fonts/Arial.ttf";
#else
    paths[path_count++] = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    paths[path_count++] = "/usr/share/fonts/TTF/DejaVuSans.ttf";
#endif

    for (int i = 0; i < path_count; i++) {
        if (!paths[i] || !paths[i][0]) continue;
        FILE* f = fopen(paths[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0 || len > 64 * 1024 * 1024) { fclose(f); continue; }
        uint8_t* data = (uint8_t*)malloc((size_t)len);
        if (!data) { fclose(f); continue; }
        size_t nread = fread(data, 1, (size_t)len, f);
        fclose(f);
        if (nread != (size_t)len) { free(data); continue; }
        int64_t font_id = abi_ui_font_load_ttf(
            session_id, "default", "system", 14.0, data, (int64_t)len);
        free(data);
        if (font_id > 0) return font_id;
    }
    return 0;
}

static int64_t native_ui_session_create(const char* name, int64_t width, int64_t height) {
    int64_t sid = abi_ui_session_create(name, width, height);
    if (sid <= 0) return sid;

    // Render intent is declared in Kain source via `surface native_ui => Component`.
    // Attach the winit host to create a real OS window.
#ifdef _WIN32
    abi_ui_host_attach(sid, "winit");
#endif
    // Load a default system font so text nodes rendered through the tree
    // walker (ui_render_node) can find a font resource and rasterize glyphs.
    // Font path is data-driven via KAIN_UI_FONT env var with platform fallbacks.
    native_ui_load_default_font(sid);
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
