// ============================================================================
//  host_wasm.c — WebAssembly/Canvas backend for Kaintana
//
//  Implements KaintanaBackendVTable with a software framebuffer rendered via
//  Canvas 2D putImageData. Designed for Emscripten (emcc) compilation.
//
//  VTable:  4-function KaintanaBackendVTable (init, shutdown, new_frame, render)
//  Canvas:  Phase 1 — Canvas 2D putImageData  (software framebuffer blit)
//           Phase 2 — WebGL2 vertex buffer upload (future)
//           Phase 3 — WebGPU WGSL compute shaders (future)
//  Input:   Emscripten HTML5 API callbacks (mouse, keyboard, wheel, touch)
//  DPI:     window.devicePixelRatio with matchMedia change listener
//  Timing:  emscripten_get_now() (performance.now)
//
//  Emscripten linker flags (Phase 1):
//    emcc host_wasm.c -s USE_WEBGL2=0 -s ALLOW_MEMORY_GROWTH=1
//      -s TOTAL_STACK=262144 --shell-file shell.html -o kaintana_app.html
//
//  Line count: ~500 lines (Phase 1)
// ============================================================================

// ============================================================================
//  SECTION 1: IMPORTS + FORWARD DECLARATIONS
// ============================================================================

#include "../../internal.h"         // KaintanaSession, KaintanaNode, etc.
#include "../../kaintana.h"         // KaintanaBackendVTable, kt_DrawData, etc.
#include <emscripten/emscripten.h>  // EM_ASM, emscripten_get_now
#include <emscripten/html5.h>       // Emscripten HTML5 input callbacks
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations for the 4 vtable functions
static int  wasm_init(const KaintanaBackendConfig* config);
static void wasm_shutdown(void);
static void wasm_new_frame(void);
static void wasm_render(const kt_DrawData* draw_data);

// Forward declarations for internal helpers
static int  wasm_fb_create(int width, int height);
static void wasm_fb_recreate(int width, int height);
static void wasm_fb_destroy(void);
static void wasm_blit_to_canvas(void);
static void wasm_timer_init(void);
static void wasm_timer_tick(void);
static void wasm_register_dpi_change_handler(void);
static void wasm_handle_resize(int new_logical_w, int new_logical_h);
static void wasm_fill_rect(kt_Rect bounds, uint32_t color, uint32_t color_b, float radius);

// Forward declarations for input callbacks
static EM_BOOL wasm_on_mouse_move(int eventType, const EmscriptenMouseEvent* e, void* ud);
static EM_BOOL wasm_on_mouse_down(int eventType, const EmscriptenMouseEvent* e, void* ud);
static EM_BOOL wasm_on_mouse_up(int eventType, const EmscriptenMouseEvent* e, void* ud);
static EM_BOOL wasm_on_wheel(int eventType, const EmscriptenWheelEvent* e, void* ud);
static EM_BOOL wasm_on_key_down(int eventType, const EmscriptenKeyboardEvent* e, void* ud);
static EM_BOOL wasm_on_key_up(int eventType, const EmscriptenKeyboardEvent* e, void* ud);
static EM_BOOL wasm_on_key_press(int eventType, const EmscriptenKeyboardEvent* e, void* ud);
static EM_BOOL wasm_on_focus(int eventType, const EmscriptenFocusEvent* e, void* ud);
static EM_BOOL wasm_on_touch_start(int eventType, const EmscriptenTouchEvent* e, void* ud);
static EM_BOOL wasm_on_touch_end(int eventType, const EmscriptenTouchEvent* e, void* ud);
static EM_BOOL wasm_on_touch_move(int eventType, const EmscriptenTouchEvent* e, void* ud);

// Called from JS when devicePixelRatio changes
void wasm_on_dpi_change(void);

// Callback for resize events
static EM_BOOL wasm_on_resize(int eventType, const EmscriptenUiEvent* e, void* ud);

// ============================================================================
//  SECTION 2: CONSTANTS + STATIC STATE
// ============================================================================

// ── Canvas config defaults ─────────────────────────────────────────────────
#define WASM_DEFAULT_WIDTH     800
#define WASM_DEFAULT_HEIGHT    600
#define WASM_MAX_CANVAS_W      4096
#define WASM_MAX_CANVAS_H      4096
#define WASM_CLIP_STACK_DEPTH  32
#define WASM_SCROLL_NORMALIZE  100.0f   // deltaY / 100.0 -> ~consistent scroll units
#define WASM_DELTA_CLAMP_S     0.1      // clamp delta to prevent spiral on tab bg
#define WASM_DEFAULT_DELTA_S   0.016    // ~60fps fallback delta

// ── DOM keyCode → Kaintana virtual key mapping (256 entries) ──────────────
// Maps DOM Level 3 KeyboardEvent keyCode values to the scancode integers
// expected by kt_input_key_down/up. Unmapped entries = 0.
static const unsigned char wasm_dom_to_kaintana_key[256] = {
    // 0x00-0x07: Reserved/unprintable
    0, 0, 0, 0, 0, 0, 0, 0,
    // 0x08: Backspace
    0x08, 0x09, 0, 0, 0, 0x0D, 0, 0,
    // 0x10-0x12: Shift, Ctrl, Alt
    0x10, 0x11, 0x12,
    // 0x13: Pause, 0x14: CapsLock
    0x13, 0x14,
    // 0x15-0x1A
    0, 0, 0, 0, 0, 0,
    // 0x1B: Escape
    0x1B,
    // 0x1C-0x1F
    0, 0, 0, 0,
    // 0x20: Space
    0x20,
    // 0x21-0x24
    0, 0, 0, 0,
    // 0x25-0x28: Arrow keys
    0x25, 0x26, 0x27, 0x28,
    // 0x29-0x2C
    0, 0, 0, 0,
    // 0x2D: Insert, 0x2E: Delete
    0x2D, 0x2E,
    // 0x2F: Help
    0,
    // 0x30-0x39: '0'-'9'
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    // 0x3A-0x40
    0, 0, 0, 0, 0, 0, 0,
    // 0x41-0x5A: 'A'-'Z'
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A,
    // 0x5B: Left Meta (Win/Cmd key)
    0x5B,
    // 0x5C-0x5F
    0, 0, 0, 0,
    // 0x60-0x69: Numpad 0-9
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    // 0x6A-0x6F
    0, 0, 0, 0, 0, 0,
    // 0x70-0x7B: F1-F12
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B,
    // 0x7C-0x7F
    0, 0, 0, 0,
    // 0x80-0xFF: Extended (unmapped)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// ── Global static state (singleton — one canvas per page) ─────────────────
static kt_Session*      g_wasm_session      = NULL;
static uint32_t*        g_fb_pixels         = NULL;
static int              g_fb_width          = 0;
static int              g_fb_height         = 0;
static int              g_logical_width     = WASM_DEFAULT_WIDTH;
static int              g_logical_height    = WASM_DEFAULT_HEIGHT;
static bool             g_is_open           = false;
static bool             g_should_close      = false;
static bool             g_full_dirty        = true;

// ── Input state (accumulated by callbacks, bridged in wasm_new_frame) ─────
static float            g_mouse_x           = 0.0f;
static float            g_mouse_y           = 0.0f;
static bool             g_mouse_down[5]     = { false };
static float            g_scroll_dx         = 0.0f;
static float            g_scroll_dy         = 0.0f;
static bool             g_keys[256]         = { false };
static char             g_text_buffer[64]   = { 0 };
static int              g_text_len          = 0;
static bool             g_focus_gained      = true;

// ── Timing ─────────────────────────────────────────────────────────────────
static double           g_last_time_ms      = 0;
static double           g_delta_seconds     = WASM_DEFAULT_DELTA_S;

// ── DPI ────────────────────────────────────────────────────────────────────
static float            g_dpi_scale         = 1.0f;

// ── Clip stack (for software renderer) ─────────────────────────────────────
static kt_Rect          g_clip_stack[WASM_CLIP_STACK_DEPTH];
static int              g_clip_depth        = -1;   // -1 = no clip

// ============================================================================
//  SECTION 3: CLIP STACK HELPERS
// ============================================================================

// Return the effective clip rect. Full framebuffer if stack is empty.
static kt_Rect wasm_clip_current(void) {
    kt_Rect full;
    full.x = 0.0f;
    full.y = 0.0f;
    full.w = (float)g_fb_width;
    full.h = (float)g_fb_height;

    if (g_clip_depth < 0)
        return full;

    return g_clip_stack[g_clip_depth];
}

// Push a new clip rect, intersecting `r` with the current clip.
static void wasm_clip_push(kt_Rect r) {
    if (g_clip_depth >= WASM_CLIP_STACK_DEPTH - 1)
        return;

    kt_Rect cur = wasm_clip_current();

    // Intersection: max of left/top, min of right/bottom
    float x1 = (r.x > cur.x) ? r.x : cur.x;
    float y1 = (r.y > cur.y) ? r.y : cur.y;
    float r_r = r.x + r.w;
    float cur_r = cur.x + cur.w;
    float r_b = r.y + r.h;
    float cur_b = cur.y + cur.h;
    float x2 = (r_r < cur_r) ? r_r : cur_r;
    float y2 = (r_b < cur_b) ? r_b : cur_b;

    // Clamp degenerate rects to zero area
    if (x2 < x1) x2 = x1;
    if (y2 < y1) y2 = y1;

    g_clip_depth++;
    g_clip_stack[g_clip_depth].x = x1;
    g_clip_stack[g_clip_depth].y = y1;
    g_clip_stack[g_clip_depth].w = x2 - x1;
    g_clip_stack[g_clip_depth].h = y2 - y1;
}

// Pop the current clip rect.
static void wasm_clip_pop(void) {
    if (g_clip_depth >= 0)
        g_clip_depth--;
}

// ============================================================================
//  SECTION 4: PIXEL FILL — Fill a bounding rect with premultiplied ARGB color
// ============================================================================

static void wasm_fill_rect(kt_Rect bounds, uint32_t color, uint32_t color_b, float radius) {
    (void)color_b;   // Gradient end color — Phase 2
    (void)radius;    // SDF rounded rect — Phase 2

    if (!g_fb_pixels)
        return;

    kt_Rect clip = wasm_clip_current();

    // Intersect bounds with clip rect
    float x1 = (bounds.x > clip.x) ? bounds.x : clip.x;
    float y1 = (bounds.y > clip.y) ? bounds.y : clip.y;
    float r_r = bounds.x + bounds.w;
    float c_r = clip.x + clip.w;
    float r_b = bounds.y + bounds.h;
    float c_b = clip.y + clip.h;
    float x2 = (r_r < c_r) ? r_r : c_r;
    float y2 = (r_b < c_b) ? r_b : c_b;

    // Degenerate → nothing to draw
    if (x2 <= x1 || y2 <= y1)
        return;

    // Clamp to framebuffer bounds
    int ix1 = (int)x1;
    if (ix1 < 0) ix1 = 0;
    int iy1 = (int)y1;
    if (iy1 < 0) iy1 = 0;
    int ix2 = (int)(x2 + 0.5f);
    if (ix2 > g_fb_width)  ix2 = g_fb_width;
    int iy2 = (int)(y2 + 0.5f);
    if (iy2 > g_fb_height) iy2 = g_fb_height;

    for (int y = iy1; y < iy2; y++) {
        uint32_t* row = g_fb_pixels + (y * g_fb_width);
        for (int x = ix1; x < ix2; x++) {
            row[x] = color;
        }
    }
}

// ============================================================================
//  SECTION 5: CANVAS FRAMEBUFFER
// ============================================================================

static int wasm_fb_create(int width, int height) {
    if (width <= 0 || height <= 0 ||
        width > WASM_MAX_CANVAS_W || height > WASM_MAX_CANVAS_H)
        return -1;

    // Allocate in WASM heap (4 bytes per pixel)
    size_t total = (size_t)width * (size_t)height * sizeof(uint32_t);
    g_fb_pixels = (uint32_t*)malloc(total);
    if (!g_fb_pixels)
        return -1;

    g_fb_width  = width;
    g_fb_height = height;
    memset(g_fb_pixels, 0, total);
    return 0;
}

static void wasm_fb_destroy(void) {
    free(g_fb_pixels);
    g_fb_pixels  = NULL;
    g_fb_width   = 0;
    g_fb_height  = 0;
}

static void wasm_fb_recreate(int width, int height) {
    wasm_fb_destroy();
    wasm_fb_create(width, height);
}

// ============================================================================
//  SECTION 6: RENDER — Software framebuffer + Canvas 2D putImageData
// ============================================================================

// Blit the software framebuffer to the Canvas 2D context via putImageData.
// Creates an ImageData from the WASM heap, sets pixel data via Uint8ClampedArray,
// then calls putImageData(0, 0) for full-frame blit.
static void wasm_blit_to_canvas(void) {
    if (!g_fb_pixels || g_fb_width <= 0 || g_fb_height <= 0)
        return;

    uintptr_t ptr = (uintptr_t)g_fb_pixels; (void)ptr;

    EM_ASM_({
        var canvas = Module.canvas;
        if (!canvas) {
            canvas = document.getElementById('canvas');
            if (!canvas) return;
        }
        var ctx = canvas.getContext('2d');
        if (!ctx) return;
        var w = $0;
        var h = $1;
        var ptr = $2;

        var imageData = ctx.createImageData(w, h);
        var buf = Module.HEAPU8.subarray(ptr, ptr + w * h * 4);
        imageData.data.set(new Uint8ClampedArray(buf));
        ctx.putImageData(imageData, 0, 0);
    }, g_fb_width, g_fb_height, ptr);
}

// Process all draw commands into the software framebuffer, then blit to canvas.
//   - KT_CMD_FILL:   Fill the bounds rect with premultiplied ARGB color
//   - KT_CMD_CLIP:   Push a clip rect (intersected with current clip)
//   - KT_CMD_UNCLIP: Pop the clip rect stack
//   - Other commands: silently skipped (Phase 2+)
static void wasm_render(const kt_DrawData* draw_data) {
    if (!g_fb_pixels || !draw_data || !draw_data->cmds || draw_data->cmd_count <= 0) {
        return;
    }

    // If full dirty, clear framebuffer; otherwise keep existing pixels
    if (g_full_dirty) {
        memset(g_fb_pixels, 0, (size_t)g_fb_width * g_fb_height * sizeof(uint32_t));
        g_full_dirty = false;
    }

    for (int i = 0; i < draw_data->cmd_count; i++) {
        const kt_Cmd* cmd = &draw_data->cmds[i];

        switch (cmd->type) {
            case KT_CMD_FILL:
                wasm_fill_rect(cmd->bounds, cmd->color, cmd->color_b, cmd->radius);
                break;

            case KT_CMD_CLIP:
                wasm_clip_push(cmd->bounds);
                break;

            case KT_CMD_UNCLIP:
                wasm_clip_pop();
                break;

            // KT_CMD_STROKE, KT_CMD_TEXT, KT_CMD_IMAGE:
            // Phase 2 — WebGL2 + Canvas 2D fillText for text.
            default:
                break;
        }
    }

    // Single full-frame blit to canvas
    wasm_blit_to_canvas();
}

// ============================================================================
//  SECTION 7: DPI DETECTION + CHANGE HANDLING
// ============================================================================

// Query device pixel ratio via inline JavaScript.
// Returns 1.0 standard, 2.0 Retina, fractional on mobile (1.5-3.0).
static double wasm_get_device_pixel_ratio(void) {
    return EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });
}

// Register a matchMedia listener for devicePixelRatio changes.
// Calls wasm_on_dpi_change() when the ratio changes.
static void wasm_register_dpi_change_handler(void) {
    EM_ASM({
        var mq = window.matchMedia('(resolution: 1dppx)');
        mq.addEventListener('change', function() {
            Module._wasm_on_dpi_change();
        });
    });
}

// Called from JavaScript when devicePixelRatio changes.
void wasm_on_dpi_change(void) {
    double new_dpr = emscripten_get_device_pixel_ratio(EMSCRIPTEN_EVENT_TARGET_WINDOW);
    if (fabs(new_dpr - (double)g_dpi_scale) > 0.001) {
        g_dpi_scale = (float)new_dpr;

        // Recreate framebuffer at new physical size
        int fb_w = (int)((float)g_logical_width * g_dpi_scale);
        int fb_h = (int)((float)g_logical_height * g_dpi_scale);
        wasm_fb_recreate(fb_w, fb_h);

        // Update canvas pixel dimensions
        emscripten_set_canvas_element_size("#canvas", fb_w, fb_h);

        // Bridge to core session
        if (g_wasm_session) {
            kt_set_native_scale(g_wasm_session, g_dpi_scale, g_dpi_scale);
        }

        g_full_dirty = true;
    }
}

// Handle browser window resize (separate from DPI change).
static void wasm_handle_resize(int new_logical_w, int new_logical_h) {
    if (new_logical_w <= 0) new_logical_w = WASM_DEFAULT_WIDTH;
    if (new_logical_h <= 0) new_logical_h = WASM_DEFAULT_HEIGHT;

    g_logical_width  = new_logical_w;
    g_logical_height = new_logical_h;

    // Update canvas at new size with current DPR
    int fb_w = (int)((float)g_logical_width * g_dpi_scale);
    int fb_h = (int)((float)g_logical_height * g_dpi_scale);
    emscripten_set_canvas_element_size("#canvas", fb_w, fb_h);
    wasm_fb_recreate(fb_w, fb_h);

    // Update CSS size
    char css_w[32], css_h[32];
    snprintf(css_w, sizeof(css_w), "%dpx", g_logical_width);
    snprintf(css_h, sizeof(css_h), "%dpx", g_logical_height);
    EM_ASM_({
        var c = document.getElementById('canvas');
        if (c) {
            c.style.width  = UTF8ToString($0);
            c.style.height = UTF8ToString($1);
        }
    }, css_w, css_h);

    g_full_dirty = true;
}

// ============================================================================
//  SECTION 8: PERFORMANCE TIMER
// ============================================================================

static void wasm_timer_init(void) {
    g_last_time_ms = emscripten_get_now();
    g_delta_seconds = WASM_DEFAULT_DELTA_S;
}

static void wasm_timer_tick(void) {
    double now = emscripten_get_now();
    double delta = (now - g_last_time_ms) / 1000.0;

    // Clamp to prevent spiral of death on tab background
    if (delta > WASM_DELTA_CLAMP_S)
        delta = WASM_DEFAULT_DELTA_S;

    g_delta_seconds = delta;
    g_last_time_ms = now;
}

// ============================================================================
//  SECTION 9: EMSRIPTEN INPUT CALLBACKS
// ============================================================================
//
// Emscripten HTML5 API provides callback-based input. Unlike Win32's message
// pump, input comes as discrete callback invocations. These callbacks
// accumulate state into globals. wasm_new_frame() bridges to kt_input_*().
//
// Mouse coordinates from Emscripten are in CSS pixels (logical) — NO division
// by devicePixelRatio needed. Emscripten handles the conversion automatically.
// ============================================================================

static EM_BOOL wasm_on_mouse_move(int eventType, const EmscriptenMouseEvent* e, void* ud) {
    (void)eventType; (void)ud;
    g_mouse_x = (float)e->targetX;
    g_mouse_y = (float)e->targetY;
    return EM_TRUE;
}

static EM_BOOL wasm_on_mouse_down(int eventType, const EmscriptenMouseEvent* e, void* ud) {
    (void)eventType; (void)ud;
    // e->button: 0=left, 2=right, 1=middle
    int btn = e->button;
    if (btn >= 0 && btn < 5)
        g_mouse_down[btn] = true;
    return EM_TRUE;
}

static EM_BOOL wasm_on_mouse_up(int eventType, const EmscriptenMouseEvent* e, void* ud) {
    (void)eventType; (void)ud;
    int btn = e->button;
    if (btn >= 0 && btn < 5)
        g_mouse_down[btn] = false;
    return EM_TRUE;
}

static EM_BOOL wasm_on_wheel(int eventType, const EmscriptenWheelEvent* e, void* ud) {
    (void)eventType; (void)ud;
    // deltaY is in pixels (browser-dependent). Normalize to match Win32's pattern.
    g_scroll_dx += (float)e->deltaX / WASM_SCROLL_NORMALIZE;
    g_scroll_dy += (float)e->deltaY / WASM_SCROLL_NORMALIZE;
    return EM_TRUE;
}

static EM_BOOL wasm_on_key_down(int eventType, const EmscriptenKeyboardEvent* e, void* ud) {
    (void)eventType; (void)ud;
    if (e->keyCode < 256) {
        unsigned char k = wasm_dom_to_kaintana_key[e->keyCode];
        if (k != 0) g_keys[k] = true;
    }
    // Return EM_TRUE calls preventDefault for handled keys (prevents browser shortcuts)
    return EM_TRUE;
}

static EM_BOOL wasm_on_key_up(int eventType, const EmscriptenKeyboardEvent* e, void* ud) {
    (void)eventType; (void)ud;
    if (e->keyCode < 256) {
        unsigned char k = wasm_dom_to_kaintana_key[e->keyCode];
        if (k != 0) g_keys[k] = false;
    }
    return EM_TRUE;
}

static EM_BOOL wasm_on_key_press(int eventType, const EmscriptenKeyboardEvent* e, void* ud) {
    (void)eventType; (void)ud;
    // Character input from keypress event — charCode contains Unicode
    uint32_t ch = (uint32_t)e->charCode;
    if (ch >= 32 && ch != 127 && g_text_len < 63) {
        // Convert to UTF-8
        if (ch < 0x80) {
            g_text_buffer[g_text_len++] = (char)ch;
        } else if (ch < 0x800) {
            g_text_buffer[g_text_len++] = (char)(0xC0 | (ch >> 6));
            g_text_buffer[g_text_len++] = (char)(0x80 | (ch & 0x3F));
        } else {
            g_text_buffer[g_text_len++] = (char)(0xE0 | (ch >> 12));
            g_text_buffer[g_text_len++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            g_text_buffer[g_text_len++] = (char)(0x80 | (ch & 0x3F));
        }
        g_text_buffer[g_text_len] = '\0';
    }
    return EM_TRUE;
}

static EM_BOOL wasm_on_focus(int eventType, const EmscriptenFocusEvent* e, void* ud) {
    (void)e; (void)ud;
    g_focus_gained = (eventType == EMSCRIPTEN_EVENT_FOCUS);

    if (!g_focus_gained) {
        // Release all keys on focus loss (same as Win32 pattern)
        memset(g_keys, 0, sizeof(g_keys));
        memset(g_mouse_down, 0, sizeof(g_mouse_down));
    }
    return EM_TRUE;
}

// ── Touch events (mobile support) ──────────────────────────────────────────

static EM_BOOL wasm_on_touch_start(int eventType, const EmscriptenTouchEvent* e, void* ud) {
    (void)eventType; (void)ud;
    if (e->numTouches > 0) {
        g_mouse_x = (float)e->touches[0].targetX;
        g_mouse_y = (float)e->touches[0].targetY;
        g_mouse_down[0] = true;
    }
    return EM_TRUE;
}

static EM_BOOL wasm_on_touch_end(int eventType, const EmscriptenTouchEvent* e, void* ud) {
    (void)eventType; (void)e; (void)ud;
    g_mouse_down[0] = false;
    return EM_TRUE;
}

static EM_BOOL wasm_on_touch_move(int eventType, const EmscriptenTouchEvent* e, void* ud) {
    (void)eventType; (void)ud;
    if (e->numTouches > 0) {
        g_mouse_x = (float)e->touches[0].targetX;
        g_mouse_y = (float)e->touches[0].targetY;
    }
    return EM_TRUE;
}

// ============================================================================
//  SECTION 10: BACKEND LIFECYCLE — The 4-function KaintanaBackendVTable
// ============================================================================

// wasm_init: Set up canvas, register input callbacks, initialize framebuffer.
// Returns 0 on success, -1 on failure.
static int wasm_init(const KaintanaBackendConfig* config) {
    if (!config)
        return -1;

    // Store session pointer from config->platform_handle
    g_wasm_session = (kt_Session*)config->platform_handle;

    // Use config dimensions or defaults
    g_logical_width  = (config->width  > 0) ? config->width  : WASM_DEFAULT_WIDTH;
    g_logical_height = (config->height > 0) ? config->height : WASM_DEFAULT_HEIGHT;

    // Query device pixel ratio
    double dpr = wasm_get_device_pixel_ratio();
    g_dpi_scale = (float)dpr;

    // ── Canvas sizing ────────────────────────────────────────────────────────
    // CSS pixels = logical size (what layout math uses)
    // Canvas pixels = logical size x devicePixelRatio (what the GPU sees)
    int fb_w = (int)((float)g_logical_width * g_dpi_scale);
    int fb_h = (int)((float)g_logical_height * g_dpi_scale);

    // Set canvas element pixel buffer size (physical pixels)
    emscripten_set_canvas_element_size("#canvas", fb_w, fb_h);

    // Set CSS display size (logical pixels)
    char css_w[32], css_h[32];
    snprintf(css_w, sizeof(css_w), "%dpx", g_logical_width);
    snprintf(css_h, sizeof(css_h), "%dpx", g_logical_height);
    EM_ASM_({
        var c = document.getElementById('canvas');
        if (c) {
            c.style.width  = UTF8ToString($0);
            c.style.height = UTF8ToString($1);
        }
    }, css_w, css_h);

    // ── Framebuffer ──────────────────────────────────────────────────────────
    if (wasm_fb_create(fb_w, fb_h) != 0)
        return -1;

    // ── Input callbacks ──────────────────────────────────────────────────────
    emscripten_set_mousemove_callback("#canvas", NULL, EM_TRUE, wasm_on_mouse_move);
    emscripten_set_mousedown_callback("#canvas", NULL, EM_TRUE, wasm_on_mouse_down);
    emscripten_set_mouseup_callback("#canvas", NULL, EM_TRUE, wasm_on_mouse_up);
    emscripten_set_wheel_callback("#canvas", NULL, EM_TRUE, wasm_on_wheel);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, wasm_on_key_down);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, wasm_on_key_up);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, wasm_on_key_press);
    emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, wasm_on_focus);

    // Touch (mobile)
    emscripten_set_touchstart_callback("#canvas", NULL, EM_TRUE, wasm_on_touch_start);
    emscripten_set_touchend_callback("#canvas", NULL, EM_TRUE, wasm_on_touch_end);
    emscripten_set_touchmove_callback("#canvas", NULL, EM_TRUE, wasm_on_touch_move);

    // ── DPI change listener ──────────────────────────────────────────────────
    wasm_register_dpi_change_handler();

    // ── Resize handler ───────────────────────────────────────────────────────
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, wasm_on_resize);

    // ── Timer ────────────────────────────────────────────────────────────────
    wasm_timer_init();

    // ── Bridge DPI to session ────────────────────────────────────────────────
    if (g_wasm_session) {
        kt_set_native_scale(g_wasm_session, g_dpi_scale, g_dpi_scale);
    }

    g_is_open      = true;
    g_should_close = false;
    g_full_dirty   = true;
    g_clip_depth   = -1;

    return 0;
}

// wasm_shutdown: Free framebuffer, remove callbacks, reset state.
static void wasm_shutdown(void) {
    g_is_open = false;

    // Free framebuffer
    wasm_fb_destroy();

    // Reset state
    g_wasm_session   = NULL;
    g_logical_width  = WASM_DEFAULT_WIDTH;
    g_logical_height = WASM_DEFAULT_HEIGHT;
    g_dpi_scale      = 1.0f;
    g_full_dirty     = true;
    g_clip_depth     = -1;

    // Clear input state
    memset(g_keys, 0, sizeof(g_keys));
    memset(g_mouse_down, 0, sizeof(g_mouse_down));
    g_text_len = 0;
    g_text_buffer[0] = '\0';
}

// wasm_new_frame: Update timing, bridge input to session, reset per-frame state.
static void wasm_new_frame(void) {
    if (!g_is_open || !g_wasm_session)
        return;

    // Update delta time
    wasm_timer_tick();

    // Bridge input to session via kt_input_*() functions
    kt_input_mouse_move(g_wasm_session, g_mouse_x, g_mouse_y);

    for (int b = 0; b < 5; b++) {
        if (g_mouse_down[b])
            kt_input_mouse_down(g_wasm_session, b);
        else
            kt_input_mouse_up(g_wasm_session, b);
    }

    if (g_scroll_dx != 0.0f || g_scroll_dy != 0.0f)
        kt_input_scroll(g_wasm_session, g_scroll_dx, g_scroll_dy);

    for (int k = 0; k < 256; k++) {
        if (g_keys[k])
            kt_input_key_down(g_wasm_session, k);
        else
            kt_input_key_up(g_wasm_session, k);
    }

    if (g_text_len > 0) {
        kt_input_text(g_wasm_session, g_text_buffer);
    }

    // Reset per-frame scratch input
    g_scroll_dx = 0.0f;
    g_scroll_dy = 0.0f;
    g_text_len = 0;
    g_text_buffer[0] = '\0';
}

// ============================================================================
//  SECTION 11: RESIZE EVENT HANDLER
// ============================================================================

static EM_BOOL wasm_on_resize(int eventType, const EmscriptenUiEvent* e, void* ud) {
    (void)eventType; (void)ud;
    (void)e;
    // Get new canvas size from element
    double css_w, css_h;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);

    int new_w = (int)css_w;
    int new_h = (int)css_h;

    // Only react if size actually changed
    if (new_w != g_logical_width || new_h != g_logical_height) {
        wasm_handle_resize(new_w, new_h);
    }
    return EM_TRUE;
}

// ============================================================================
//  SECTION 12: MAIN LOOP BRIDGE
// ============================================================================
//
// Unlike Win32/macOS where the application owns the event loop, Emscripten
// OWNS the event loop. The application registers a frame callback via
// emscripten_set_main_loop() which runs at requestAnimationFrame rate.
//
// Usage:
//   void wasm_app_frame(void) {
//       wasm_new_frame();           // bridge input
//       kt_begin(session, delta_ms); // start frame
//       build_ui(session);           // user UI code
//       kt_end(session);             // end frame
//       kt_present(session);         // calls wasm_render internally
//   }
//
//   int main() {
//       kt_init();
//       kt_Session* s = kt_make("WASM App", 800, 600);
//       kt_backend_register(s, "wasm", &kaintana_wasm_backend);
//       kt_backend_select(s, "wasm");
//       emscripten_set_main_loop(wasm_app_frame, 0, 1);
//       return 0;
//   }

// ============================================================================
//  SECTION 13: BACKEND VTABLE SINGLETON
// ============================================================================

const KaintanaBackendVTable kaintana_wasm_backend = {
    .init      = wasm_init,
    .shutdown  = wasm_shutdown,
    .new_frame = wasm_new_frame,
    .render    = wasm_render
};
