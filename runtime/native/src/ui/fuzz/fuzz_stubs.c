// ============================================================================
//  fuzz_stubs.c — Minimal stubs for ui_system ABI functions needed by fuzzer
//  ============================================================================
//  The kain_input.c and kain_font.c substrate files call abi_ui_* functions
//  from ui_system.c. For fuzz testing we provide stubs that return safe
//  default values, allowing the fuzzer to test the wrapper logic without
//  pulling in the entire 3500-line ui_system.c.
//
//  These stubs are FOR FUZZ TESTING ONLY — never link them into production.
//  ============================================================================

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── ABI font glyph stub ──────────────────────────────────────────
// Forward declare KainUiGlyph with minimal fields for the fuzzer
typedef struct KainUiGlyph {
    int      x_offset;
    int      y_offset;
    int      width;
    int      height;
    int      advance;
    uint8_t* bitmap;
} KainUiGlyph;

// Global dummy glyph for returning from get_glyph
static KainUiGlyph g_dummy_glyph = {0, 0, 0, 0, 8, NULL};

// ── Stub implementations ─────────────────────────────────────────

// Session stubs
int64_t abi_ui_session_create(const char* app_name, int64_t width, int64_t height) {
    (void)app_name; (void)width; (void)height;
    return 1; // always returns session_id 1
}

int64_t abi_ui_session_destroy(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_session_count(void) { return 1; }

// Window stubs
int64_t abi_ui_window_open(int64_t session_id, const char* title, int64_t width, int64_t height) {
    (void)session_id; (void)title; (void)width; (void)height;
    return 1;
}

int64_t abi_ui_window_close(int64_t session_id) {
    (void)session_id;
    return 0;
}

// Frame stubs
int64_t abi_ui_begin_frame(int64_t session_id, double delta_ms) {
    (void)session_id; (void)delta_ms;
    return 0;
}

int64_t abi_ui_end_frame(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_present(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_frame_index(int64_t session_id) {
    (void)session_id;
    return 0;
}

// Host stubs
int64_t abi_ui_host_attach(int64_t session_id, const char* backend_id) {
    (void)session_id; (void)backend_id;
    return 0;
}

int64_t abi_ui_host_pump(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_host_present(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_host_presented_draw_count(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_host_frame_hash(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_host_should_close(int64_t session_id) {
    (void)session_id;
    return 0;
}

const char* abi_ui_host_backend(int64_t session_id) {
    (void)session_id;
    return "stub";
}

// Node stubs
int64_t abi_ui_node_create(int64_t session_id, const char* kind) {
    (void)session_id; (void)kind;
    return 42; // returns a dummy node_id
}

int64_t abi_ui_node_destroy(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 0;
}

int64_t abi_ui_node_count(int64_t session_id) {
    (void)session_id;
    return 1;
}

int64_t abi_ui_node_exists(int64_t session_id, int64_t node_id) {
    (void)session_id;
    return (node_id > 0) ? 1 : 0;
}

int64_t abi_ui_node_set_parent(int64_t session_id, int64_t node_id, int64_t parent_id) {
    (void)session_id; (void)node_id; (void)parent_id;
    return 0;
}

int64_t abi_ui_node_parent(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 0;
}

int64_t abi_ui_node_child_count(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 0;
}

int64_t abi_ui_node_set_rect(int64_t session_id, int64_t node_id,
                              double x, double y, double width, double height) {
    (void)session_id; (void)node_id; (void)x; (void)y; (void)width; (void)height;
    return 0;
}

double abi_ui_node_x(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 0.0;
}

double abi_ui_node_y(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 0.0;
}

double abi_ui_node_width(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 100.0;
}

double abi_ui_node_height(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 50.0;
}

int64_t abi_ui_node_set_text(int64_t session_id, int64_t node_id, const char* text) {
    (void)session_id; (void)node_id; (void)text;
    return 0;
}

const char* abi_ui_node_text(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return "";
}

const char* abi_ui_node_kind(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return "stub";
}

int64_t abi_ui_node_set_stable_key(int64_t session_id, int64_t node_id, const char* stable_key) {
    (void)session_id; (void)node_id; (void)stable_key;
    return 0;
}

const char* abi_ui_node_stable_key(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return "";
}

int64_t abi_ui_node_find_by_stable_key(int64_t session_id, const char* stable_key) {
    (void)session_id; (void)stable_key;
    return 0;
}

// Style/state stubs
int64_t abi_ui_node_set_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value) {
    (void)session_id; (void)node_id; (void)key; (void)value;
    return 0;
}

int64_t abi_ui_node_set_style_f64(int64_t session_id, int64_t node_id, const char* key, double value) {
    (void)session_id; (void)node_id; (void)key; (void)value;
    return 0;
}

int64_t abi_ui_node_set_style_string(int64_t session_id, int64_t node_id, const char* key, const char* value) {
    (void)session_id; (void)node_id; (void)key; (void)value;
    return 0;
}

int64_t abi_ui_node_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback) {
    (void)session_id; (void)node_id; (void)key;
    return fallback;
}

double abi_ui_node_style_f64(int64_t session_id, int64_t node_id, const char* key, double fallback) {
    (void)session_id; (void)node_id; (void)key;
    return fallback;
}

const char* abi_ui_node_style_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback) {
    (void)session_id; (void)node_id; (void)key;
    return fallback;
}

int64_t abi_ui_node_set_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value) {
    (void)session_id; (void)node_id; (void)key; (void)value;
    return 0;
}

int64_t abi_ui_node_set_state_f64(int64_t session_id, int64_t node_id, const char* key, double value) {
    (void)session_id; (void)node_id; (void)key; (void)value;
    return 0;
}

int64_t abi_ui_node_set_state_string(int64_t session_id, int64_t node_id, const char* key, const char* value) {
    (void)session_id; (void)node_id; (void)key; (void)value;
    return 0;
}

int64_t abi_ui_node_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback) {
    (void)session_id; (void)node_id; (void)key;
    return fallback;
}

double abi_ui_node_state_f64(int64_t session_id, int64_t node_id, const char* key, double fallback) {
    (void)session_id; (void)node_id; (void)key;
    return fallback;
}

const char* abi_ui_node_state_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback) {
    (void)session_id; (void)node_id; (void)key;
    return fallback;
}

int64_t abi_ui_state_count(int64_t session_id) {
    (void)session_id;
    return 0;
}

// Focus/hit-test stubs
int64_t abi_ui_focus(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return 0;
}

int64_t abi_ui_focused_node(int64_t session_id) {
    (void)session_id;
    return 0;
}

int64_t abi_ui_hit_test(int64_t session_id, double x, double y) {
    (void)session_id; (void)x; (void)y;
    return -1; // no hit
}

int64_t abi_ui_mark_dirty(int64_t session_id, int64_t node_id, int64_t reason) {
    (void)session_id; (void)node_id; (void)reason;
    return 0;
}

int64_t abi_ui_dirty_count(int64_t session_id) {
    (void)session_id;
    return 0;
}

// Event stubs
int64_t abi_ui_push_event(int64_t session_id, const char* kind, int64_t target_node_id,
                           double x, double y, int64_t key_code, const char* text) {
    (void)session_id; (void)kind; (void)target_node_id;
    (void)x; (void)y; (void)key_code; (void)text;
    return 0;
}

int64_t abi_ui_poll_event(int64_t session_id) {
    (void)session_id;
    return 0; // no events available
}

const char* abi_ui_event_kind(int64_t session_id) {
    (void)session_id;
    return "none";
}

int64_t abi_ui_event_target(int64_t session_id) {
    (void)session_id;
    return 0;
}

double abi_ui_event_x(int64_t session_id) {
    (void)session_id;
    return 0.0;
}

double abi_ui_event_y(int64_t session_id) {
    (void)session_id;
    return 0.0;
}

int64_t abi_ui_event_key_code(int64_t session_id) {
    (void)session_id;
    return 0;
}

const char* abi_ui_event_text(int64_t session_id) {
    (void)session_id;
    return "";
}

// Font ABI stubs
int64_t abi_ui_font_load_ttf(int64_t session_id, const char* key, const char* family,
                              double size, const uint8_t* ttf_data, int64_t ttf_len) {
    (void)session_id; (void)key; (void)family; (void)size; (void)ttf_data; (void)ttf_len;
    return 0; // always returns 0 (no actual font loaded)
}

KainUiGlyph* abi_ui_font_get_glyph(int64_t session_id, int64_t font_id, int codepoint) {
    (void)session_id; (void)font_id; (void)codepoint;
    return NULL; // no glyph available
}

void abi_ui_font_release_glyph(KainUiGlyph* glyph) {
    (void)glyph;
}

int64_t kain_ui_font_get_vmetrics(int64_t session_id, int64_t font_id,
                                   int* ascent, int* descent, int* line_gap) {
    (void)session_id; (void)font_id;
    if (ascent)  *ascent = 16;
    if (descent) *descent = -4;
    if (line_gap) *line_gap = 2;
    return 0;
}

double abi_ui_text_measure_width(int64_t session_id, int64_t font_resource_id, const char* text) {
    (void)session_id; (void)font_resource_id;
    if (!text) return 0.0;
    return (double)(strlen(text) * 8); // rough estimate: 8px per char
}

double abi_ui_text_measure_height(int64_t session_id, int64_t font_resource_id, const char* text) {
    (void)session_id; (void)font_resource_id; (void)text;
    return 20.0; // ~20px line height
}

// ── Color blend (needed by renderer) ────────────────────────────────
uint32_t ui_color_blend(uint32_t src, uint32_t dst) {
    // Simple alpha blend for fuzz testing
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0xFF) return src;
    if (sa == 0x00) return dst;
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >>  8) & 0xFF;
    uint32_t sb =  src        & 0xFF;
    uint32_t da = (dst >> 24) & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >>  8) & 0xFF;
    uint32_t db =  dst        & 0xFF;
    uint32_t a = sa + (da * (255 - sa)) / 255;
    if (a == 0) return 0;
    uint32_t r = (sr * sa + dr * da * (255 - sa) / 255) / a;
    uint32_t g = (sg * sa + dg * da * (255 - sa) / 255) / a;
    uint32_t b = (sb * sa + db * da * (255 - sa) / 255) / a;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// ── Surface stubs ──────────────────────────────────────────────────
// Forward declare from kain_surface.h
typedef enum kainSurfaceKind {
    KAIN_SURFACE_SOFTWARE = 0,
    KAIN_SURFACE_VULKAN,
    KAIN_SURFACE_D3D12,
    KAIN_SURFACE_WEBGPU,
} kainSurfaceKind;

typedef struct kainSurface {
    kainSurfaceKind kind;
    int width;
    int height;
    uint32_t* pixels;
    int stride;
} kainSurface;

kainSurface* kain_surface_create(int width, int height, kainSurfaceKind kind) {
    if (width <= 0 || height <= 0) return NULL;
    if (kind > KAIN_SURFACE_WEBGPU) return NULL;
    kainSurface* s = (kainSurface*)calloc(1, sizeof(kainSurface));
    if (!s) return NULL;
    s->kind = kind;
    s->width = width;
    s->height = height;
    s->pixels = (uint32_t*)calloc((size_t)(width * height), sizeof(uint32_t));
    s->stride = width;
    return s;
}

void kain_surface_destroy(kainSurface* s) {
    if (s) {
        free(s->pixels);
        free(s);
    }
}

void kain_surface_resize(kainSurface* s, int width, int height) {
    if (!s) return;
    free(s->pixels);
    s->width = width;
    s->height = height;
    s->pixels = (uint32_t*)calloc((size_t)(width * height), sizeof(uint32_t));
    s->stride = width;
}

uint32_t* kain_surface_pixels(kainSurface* s, int* out_width, int* out_height, int* out_stride) {
    if (!s) return NULL;
    if (out_width)  *out_width  = s->width;
    if (out_height) *out_height = s->height;
    if (out_stride) *out_stride = s->stride;
    return s->pixels;
}

kainSurfaceKind kain_surface_backend(kainSurface* s) {
    return s ? s->kind : KAIN_SURFACE_SOFTWARE;
}

int kain_surface_width(kainSurface* s) {
    return s ? s->width : 0;
}

int kain_surface_height(kainSurface* s) {
    return s ? s->height : 0;
}

const char* kain_surface_kind_name(kainSurfaceKind kind) {
    switch (kind) {
        case KAIN_SURFACE_SOFTWARE: return "software";
        case KAIN_SURFACE_VULKAN:   return "vulkan";
        case KAIN_SURFACE_D3D12:    return "d3d12";
        case KAIN_SURFACE_WEBGPU:   return "webgpu";
        default:                    return "unknown";
    }
}

// ── KainComponentSurface vtable registration ──────────────────
#include "../../include/component_surface.h"

// Forward declare the stub functions used in the vtable
static int64_t stub_session_create(const char* name, int64_t w, int64_t h);
static void stub_session_destroy(int64_t sid);
static int64_t stub_element_begin(int64_t sid, int64_t pid, const char* kind, const char* key);
static void stub_element_end(int64_t sid, int64_t eid);
static void stub_set_text(int64_t sid, int64_t eid, const char* text);
static void stub_set_attr_i64(int64_t sid, int64_t eid, const char* key, int64_t v);
static void stub_set_attr_f64(int64_t sid, int64_t eid, const char* key, double v);
static void stub_set_attr_str(int64_t sid, int64_t eid, const char* key, const char* v);
static int64_t stub_state_get_i64(int64_t sid, const char* key);
static void stub_state_set_i64(int64_t sid, const char* key, int64_t v);
static void stub_begin_frame(int64_t sid, double d);
static void stub_end_frame(int64_t sid);
static void stub_present(int64_t sid);
static int64_t stub_poll_event(int64_t sid, void* buf, int64_t sz);
static int64_t stub_should_close(int64_t sid);
static int64_t stub_window_open(int64_t sid, const char* t, int64_t w, int64_t h);
static int64_t stub_host_pump(int64_t sid);
static void stub_attach_platform(int64_t sid, void* h);
static const KainGpuSurfaceExtension* stub_get_gpu_ext(int64_t sid);
static double stub_state_get_f64(int64_t sid, const char* key);
static void stub_state_set_f64(int64_t sid, const char* key, double v);
static const char* stub_state_get_str(int64_t sid, const char* key);
static void stub_state_set_str(int64_t sid, const char* key, const char* v);
static void stub_set_callback(int64_t sid, int64_t eid, const char* evt, void* cb);

static const KainComponentSurface stub_native_ui_surface = {
    stub_session_create, stub_session_destroy,
    stub_element_begin, stub_element_end, stub_set_text,
    stub_set_attr_i64, stub_set_attr_f64, stub_set_attr_str,
    stub_state_get_i64, stub_state_set_i64,
    stub_begin_frame, stub_end_frame, stub_present,
    stub_poll_event, stub_should_close, stub_window_open,
    stub_host_pump, stub_attach_platform, stub_get_gpu_ext,
    stub_state_get_f64, stub_state_set_f64,
    stub_state_get_str, stub_state_set_str,
    stub_set_callback
};

// ── Registration state ──────────────────────────────────────────
static const KainComponentSurface* g_registered_surface = NULL;
static char g_surface_name[64] = {0};

void kain_component_surface_register(const char* name, const KainComponentSurface* surface) {
    if (name && surface) {
        strncpy(g_surface_name, name, sizeof(g_surface_name) - 1);
        g_registered_surface = surface;
    }
}

const KainComponentSurface* kain_component_surface_resolve(const char* name) {
    if (!name) return NULL;
    if (strcmp(name, g_surface_name) == 0) return g_registered_surface;
    return NULL;
}

// ── Vtable stub implementations ─────────────────────────────────
static int64_t s_stub_sid_counter = 100;
static int64_t s_stub_eid_counter = 200;

static int64_t stub_session_create(const char* name, int64_t w, int64_t h) {
    (void)name; (void)w; (void)h;
    return s_stub_sid_counter++;
}
static void stub_session_destroy(int64_t sid) { (void)sid; }
static int64_t stub_element_begin(int64_t sid, int64_t pid, const char* kind, const char* key) {
    (void)sid; (void)pid; (void)kind; (void)key;
    return ++s_stub_eid_counter;
}
static void stub_element_end(int64_t sid, int64_t eid) { (void)sid; (void)eid; }
static void stub_set_text(int64_t sid, int64_t eid, const char* text) { (void)sid; (void)eid; (void)text; }
static void stub_set_attr_i64(int64_t sid, int64_t eid, const char* key, int64_t v) { (void)sid; (void)eid; (void)key; (void)v; }
static void stub_set_attr_f64(int64_t sid, int64_t eid, const char* key, double v) { (void)sid; (void)eid; (void)key; (void)v; }
static void stub_set_attr_str(int64_t sid, int64_t eid, const char* key, const char* v) { (void)sid; (void)eid; (void)key; (void)v; }
static int64_t stub_state_get_i64(int64_t sid, const char* key) { (void)sid; (void)key; return 0; }
static void stub_state_set_i64(int64_t sid, const char* key, int64_t v) { (void)sid; (void)key; (void)v; }
static void stub_begin_frame(int64_t sid, double d) { (void)sid; (void)d; }
static void stub_end_frame(int64_t sid) { (void)sid; }
static void stub_present(int64_t sid) { (void)sid; }
static int64_t stub_poll_event(int64_t sid, void* buf, int64_t sz) { (void)sid; (void)buf; (void)sz; return 0; }
static int64_t stub_should_close(int64_t sid) { (void)sid; return 0; }
static int64_t stub_window_open(int64_t sid, const char* t, int64_t w, int64_t h) { (void)sid; (void)t; (void)w; (void)h; return 1; }
static int64_t stub_host_pump(int64_t sid) { (void)sid; return 0; }
static void stub_attach_platform(int64_t sid, void* h) { (void)sid; (void)h; }
static const KainGpuSurfaceExtension* stub_get_gpu_ext(int64_t sid) { (void)sid; return NULL; }
static double stub_state_get_f64(int64_t sid, const char* key) { (void)sid; (void)key; return 0.0; }
static void stub_state_set_f64(int64_t sid, const char* key, double v) { (void)sid; (void)key; (void)v; }
static const char* stub_state_get_str(int64_t sid, const char* key) { (void)sid; (void)key; return ""; }
static void stub_state_set_str(int64_t sid, const char* key, const char* v) { (void)sid; (void)key; (void)v; }
static void stub_set_callback(int64_t sid, int64_t eid, const char* evt, void* cb) { (void)sid; (void)eid; (void)evt; (void)cb; }

// Call this from main() to register the stub surface
void fuzz_register_stub_surface(void) {
    kain_component_surface_register("native_ui", &stub_native_ui_surface);
}

// ── Accessibility stubs ────────────────────────────────────────────
int64_t abi_ui_accessibility_set_role(int64_t session_id, int64_t node_id, const char* role) {
    (void)session_id; (void)node_id; (void)role;
    return 0;
}
int64_t abi_ui_accessibility_set_label(int64_t session_id, int64_t node_id, const char* label) {
    (void)session_id; (void)node_id; (void)label;
    return 0;
}
const char* abi_ui_accessibility_role(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return "";
}
const char* abi_ui_accessibility_label(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id;
    return "";
}

// ── Flag stubs ─────────────────────────────────────────────────────
int64_t abi_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled) {
    (void)session_id; (void)node_id; (void)flag; (void)enabled;
    return 0;
}
int64_t abi_ui_node_has_flag(int64_t session_id, int64_t node_id, const char* flag) {
    (void)session_id; (void)node_id; (void)flag;
    return 0;
}

// ── Draw command stubs ─────────────────────────────────────────────
int64_t abi_ui_draw_rect(int64_t session_id, int64_t node_id, double x, double y,
                          double width, double height, const char* style_key) {
    (void)session_id; (void)node_id; (void)x; (void)y; (void)width; (void)height; (void)style_key;
    return 0;
}
int64_t abi_ui_draw_text(int64_t session_id, int64_t node_id, int64_t font_resource_id,
                          double x, double y, const char* text, const char* style_key) {
    (void)session_id; (void)node_id; (void)font_resource_id;
    (void)x; (void)y; (void)text; (void)style_key;
    return 0;
}
int64_t abi_ui_draw_command_count(int64_t session_id) {
    (void)session_id;
    return 0;
}
const char* abi_ui_draw_command_kind(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return "";
}
int64_t abi_ui_draw_command_node(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0;
}
int64_t abi_ui_draw_command_resource(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0;
}
double abi_ui_draw_command_x(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0.0;
}
double abi_ui_draw_command_y(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0.0;
}
double abi_ui_draw_command_width(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0.0;
}
double abi_ui_draw_command_height(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0.0;
}
const char* abi_ui_draw_command_text(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return "";
}
const char* abi_ui_draw_command_style(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return "";
}
int64_t abi_ui_draw_command_font(int64_t session_id, int64_t command_index) {
    (void)session_id; (void)command_index;
    return 0;
}

// ── Resource stubs ─────────────────────────────────────────────────
int64_t abi_ui_resource_create(int64_t session_id, const char* resource_type, const char* key,
                                int64_t width, int64_t height, int64_t byte_length) {
    (void)session_id; (void)resource_type; (void)key; (void)width; (void)height; (void)byte_length;
    return 0;
}
int64_t abi_ui_font_create(int64_t session_id, const char* key, const char* family, double size) {
    (void)session_id; (void)key; (void)family; (void)size;
    return 0;
}
int64_t abi_ui_resource_count(int64_t session_id) {
    (void)session_id;
    return 0;
}

// ── Callback stubs ─────────────────────────────────────────────────
int64_t abi_ui_node_set_callback(int64_t session_id, int64_t node_id,
                                  const char* event_name, void* callback_fn) {
    (void)session_id; (void)node_id; (void)event_name; (void)callback_fn;
    return 0;
}
int64_t abi_ui_node_invoke_callback(int64_t session_id, int64_t node_id,
                                     const char* event_name, void* arg) {
    (void)session_id; (void)node_id; (void)event_name; (void)arg;
    return 0;
}

// ── Misc remaining stubs ───────────────────────────────────────────
int64_t abi_ui_reset(void) { return 0; }
int64_t abi_ui_framebuffer_ptr(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_framebuffer_width(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_framebuffer_height(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_framebuffer_stride(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_invalidate_window(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_glyph_bitmap_ptr(int64_t glyph_ptr) { (void)glyph_ptr; return 0; }
int64_t abi_ui_glyph_width(int64_t glyph_ptr) { (void)glyph_ptr; return 0; }
int64_t abi_ui_glyph_height(int64_t glyph_ptr) { (void)glyph_ptr; return 0; }
int64_t abi_ui_glyph_x_offset(int64_t glyph_ptr) { (void)glyph_ptr; return 0; }
int64_t abi_ui_glyph_y_offset(int64_t glyph_ptr) { (void)glyph_ptr; return 0; }
int64_t abi_ui_glyph_advance(int64_t glyph_ptr) { (void)glyph_ptr; return 0; }
void abi_ui_glyph_release(int64_t glyph_ptr) { (void)glyph_ptr; }

// Widget ABI stubs (needed for linking ui_widget.c if included)
int64_t abi_ui_widget_create(int64_t session_id) { (void)session_id; return 0; }
void abi_ui_widget_destroy(int64_t ctx_ptr) { (void)ctx_ptr; }
void abi_ui_widget_begin_frame(int64_t ctx_ptr) { (void)ctx_ptr; }
void abi_ui_widget_end_frame(int64_t ctx_ptr) { (void)ctx_ptr; }
int64_t abi_ui_widget_load_font(int64_t ctx_ptr, const char* filepath, double size) {
    (void)ctx_ptr; (void)filepath; (void)size; return 0;
}
int64_t abi_ui_widget_load_default_font(int64_t ctx_ptr, double size) {
    (void)ctx_ptr; (void)size; return 0;
}

int64_t abi_ui_last_presented_frame(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_texture_create(int64_t session_id, const char* key, int64_t width, int64_t height, const char* format, int64_t byte_length) {
    (void)session_id; (void)key; (void)width; (void)height; (void)format; (void)byte_length; return 0;
}
int64_t abi_ui_canvas_create(int64_t session_id, const char* key, int64_t width, int64_t height) {
    (void)session_id; (void)key; (void)width; (void)height; return 0;
}
int64_t abi_ui_shader_create(int64_t session_id, const char* key, const char* stage, int64_t byte_length) {
    (void)session_id; (void)key; (void)stage; (void)byte_length; return 0;
}
int64_t abi_ui_resource_set_bytes(int64_t session_id, int64_t resource_id, const uint8_t* bytes, int64_t byte_length) {
    (void)session_id; (void)resource_id; (void)bytes; (void)byte_length; return 0;
}
int64_t abi_ui_resource_set_bytes_hex(int64_t session_id, int64_t resource_id, const char* bytes_hex) {
    (void)session_id; (void)resource_id; (void)bytes_hex; return 0;
}
int64_t abi_ui_resource_exists(int64_t session_id, int64_t resource_id) {
    (void)session_id; (void)resource_id; return 0;
}
const char* abi_ui_resource_type(int64_t session_id, int64_t resource_id) {
    (void)session_id; (void)resource_id; return "";
}
const char* abi_ui_resource_key(int64_t session_id, int64_t resource_id) {
    (void)session_id; (void)resource_id; return "";
}
int64_t abi_ui_resource_width(int64_t session_id, int64_t resource_id) {
    (void)session_id; (void)resource_id; return 0;
}
int64_t abi_ui_resource_height(int64_t session_id, int64_t resource_id) {
    (void)session_id; (void)resource_id; return 0;
}
int64_t abi_ui_resource_byte_length(int64_t session_id, int64_t resource_id) {
    (void)session_id; (void)resource_id; return 0;
}
int64_t abi_ui_font_ascent(int64_t session_id, int64_t font_resource_id) {
    (void)session_id; (void)font_resource_id; return 16;
}
int64_t abi_ui_font_descent(int64_t session_id, int64_t font_resource_id) {
    (void)session_id; (void)font_resource_id; return -4;
}
int64_t abi_ui_font_line_gap(int64_t session_id, int64_t font_resource_id) {
    (void)session_id; (void)font_resource_id; return 2;
}
int64_t abi_ui_draw_resource(int64_t session_id, int64_t node_id, int64_t resource_id,
                              double x, double y, double width, double height, const char* style_key) {
    (void)session_id; (void)node_id; (void)resource_id; (void)x; (void)y; (void)width; (void)height; (void)style_key;
    return 0;
}
int64_t abi_ui_clipboard_set_text(int64_t session_id, const char* text) {
    (void)session_id; (void)text; return 0;
}
const char* abi_ui_clipboard_text(int64_t session_id) {
    (void)session_id; return "";
}
int64_t abi_ui_ime_begin(int64_t session_id, int64_t node_id) {
    (void)session_id; (void)node_id; return 0;
}
int64_t abi_ui_ime_commit_text(int64_t session_id, const char* text) {
    (void)session_id; (void)text; return 0;
}
int64_t abi_ui_ime_end(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_ime_active_node(int64_t session_id) { (void)session_id; return 0; }
const char* abi_ui_ime_text(int64_t session_id) { (void)session_id; return ""; }
int64_t abi_ui_drag_begin(int64_t session_id, int64_t node_id, const char* payload, double x, double y) {
    (void)session_id; (void)node_id; (void)payload; (void)x; (void)y; return 0;
}
int64_t abi_ui_drag_update(int64_t session_id, double x, double y, int64_t drop_target_node_id) {
    (void)session_id; (void)x; (void)y; (void)drop_target_node_id; return 0;
}
int64_t abi_ui_drag_drop(int64_t session_id, int64_t drop_target_node_id) {
    (void)session_id; (void)drop_target_node_id; return 0;
}
int64_t abi_ui_drag_active_node(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_drag_drop_target(int64_t session_id) { (void)session_id; return 0; }
double abi_ui_drag_x(int64_t session_id) { (void)session_id; return 0.0; }
double abi_ui_drag_y(int64_t session_id) { (void)session_id; return 0.0; }
const char* abi_ui_drag_payload(int64_t session_id) { (void)session_id; return ""; }
int64_t abi_ui_menu_create(int64_t session_id, const char* key) { (void)session_id; (void)key; return 0; }
int64_t abi_ui_menu_add_item(int64_t session_id, int64_t menu_id, const char* key, const char* label, int64_t command_id) {
    (void)session_id; (void)menu_id; (void)key; (void)label; (void)command_id; return 0;
}
int64_t abi_ui_menu_open(int64_t session_id, int64_t menu_id, double x, double y) {
    (void)session_id; (void)menu_id; (void)x; (void)y; return 0;
}
int64_t abi_ui_menu_active(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_menu_item_count(int64_t session_id, int64_t menu_id) { (void)session_id; (void)menu_id; return 0; }
const char* abi_ui_menu_item_label(int64_t session_id, int64_t menu_id, int64_t item_index) {
    (void)session_id; (void)menu_id; (void)item_index; return "";
}
int64_t abi_ui_menu_item_command(int64_t session_id, int64_t menu_id, int64_t item_index) {
    (void)session_id; (void)menu_id; (void)item_index; return 0;
}
int64_t abi_ui_dialog_request(int64_t session_id, const char* kind, const char* title, const char* message) {
    (void)session_id; (void)kind; (void)title; (void)message; return 0;
}
int64_t abi_ui_dialog_active(int64_t session_id) { (void)session_id; return 0; }
const char* abi_ui_dialog_kind(int64_t session_id, int64_t dialog_id) {
    (void)session_id; (void)dialog_id; return "";
}
const char* abi_ui_dialog_title(int64_t session_id, int64_t dialog_id) {
    (void)session_id; (void)dialog_id; return "";
}
const char* abi_ui_dialog_message(int64_t session_id, int64_t dialog_id) {
    (void)session_id; (void)dialog_id; return "";
}
int64_t abi_ui_dialog_respond(int64_t session_id, int64_t dialog_id, int64_t result, const char* response_text) {
    (void)session_id; (void)dialog_id; (void)result; (void)response_text; return 0;
}
int64_t abi_ui_dialog_poll_response(int64_t session_id) { (void)session_id; return 0; }
const char* abi_ui_dialog_response_text(int64_t session_id) { (void)session_id; return ""; }
int64_t abi_ui_hot_reload_begin(int64_t session_id, const char* revision_key) {
    (void)session_id; (void)revision_key; return 0;
}
int64_t abi_ui_hot_reload_commit(int64_t session_id) { (void)session_id; return 0; }
int64_t abi_ui_hot_reload_generation(int64_t session_id) { (void)session_id; return 0; }
const char* abi_ui_hot_reload_key(int64_t session_id) { (void)session_id; return ""; }
