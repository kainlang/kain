// ============================================================================
//  api_coverage.c — Kaintana API Coverage Demo
//
//  Exercises EVERY public Kaintana API function at least once.
//  Single-file Win32 demo that includes the backend .c files directly.
//
//  Window title: "Kaintana API Coverage"
//  Layout is proportional to framebuffer size (DPI-aware, resize-friendly).
//
//  API listing — every function called in this file, grouped by category:
//
//  ── Session Lifecycle ──
//    kt_init()            § initialize before any session
//    kt_make()            § create UI session
//    kt_free()            § destroy UI session
//
//  ── Frame Loop ──
//    kt_begin()           § start a new frame with delta
//    kt_end()             § conclude command recording
//    kt_present()         § put pixels on screen
//    kt_should_close()    § check close request
//
//  ── Element Tree ──
//    kt_row()             § begin an element (returns ID)
//    kt_end_row()         § close most recent element
//    kt_text()            § set text content
//
//  ── Layout Attributes ──
//    kt_width()           § set element width
//    kt_height()          § set element height
//    kt_pad()             § set uniform padding
//    kt_pad_xy()          § set independent h/v padding
//    kt_gap()             § set gap between children
//    kt_direction()       § set flex direction (row/col)
//
//  ── Style Attributes ──
//    kt_fill()            § set fill color (hex string)
//    kt_stroke()          § set stroke color + width
//    kt_radius()          § set corner radius
//    kt_opacity()         § set opacity (0.0-1.0)
//    kt_font()            § set font size
//
//  ── State Persistence ──
//    kt_put()             § store int64 state
//    kt_put_f()           § store double state
//    kt_put_s()           § store string state
//    kt_get()             § read int64 state
//    kt_get_f()           § read double state
//    kt_get_s()           § read string state
//
//  ── Draw Output ──
//    kt_cmd_count()       § count draw commands this frame
//    kt_cmd_get()         § read draw command by index
//
//  ── Backend Registry ──
//    kt_backend_register() § register Win32 backend vtable
//    kt_backend_select()   § activate Win32 backend
//    kt_backend_probe()    § auto-select via first registered
//
//  ── Input Funnel ──
//    kt_input_mouse_move() § feed mouse position
//    kt_input_mouse_down() § feed mouse button press
//    kt_input_mouse_up()   § feed mouse button release
//    kt_input_scroll()     § feed scroll delta
//    kt_input_key_down()   § feed key press
//    kt_input_key_up()     § feed key release
//    kt_input_text()       § feed UTF-8 text input
//
//  ── DPI / Scale API ──
//    kt_scale_factor_x()   § effective horizontal scale
//    kt_scale_factor_y()   § effective vertical scale
//    kt_native_scale_x()   § OS-reported DPI X scale
//    kt_native_scale_y()   § OS-reported DPI Y scale
//    kt_set_native_scale() § notify session of DPI change
//    kt_set_zoom()         § set user zoom factor
//
//  ── Pixel Snap (inline) ──
//    kt_round_to_pixel_x()
//    kt_round_to_pixel_y()
//    kt_round_to_pixel_center_x()
//    kt_one_physical_pixel()
//    kt_round_ui()
//
//  ── Color Inlines ──
//    kt_color_from_u32()
//    kt_color_to_u32()
//    kt_color_parse_hex()
//    kt_color_premultiply()
//    kt_color_unpremultiply()
//    kt_color_lerp()
//    kt_color_luminance()
//    kt_color_saturation()
//    kt_color_srgb_to_linear()
//    kt_color_linear_to_srgb()
//    kt_apply_opacity()
//    kt_apply_opacity_u32()
//
//  ── Blend Inlines ──
//    kt_blend_compose()    § Porter-Duff SRC_OVER
//
//  ── Easing Inlines ──
//    kt_ease_smoothstep()
//    kt_ease_smootherstep()
//    kt_ease_in()
//    kt_ease_out()
//    kt_ease_in_out()
//    kt_ease_cubic_in()
//    kt_ease_cubic_out()
//    kt_ease_cubic_in_out()
//
//  ── Win32 Globals (directly via #include host_win32.c) ──
//    win32_get_fb_width()     § framebuffer pixel width
//    win32_get_fb_height()    § framebuffer pixel height
//    win32_get_mouse_x()      § mouse logical X
//    win32_get_mouse_y()      § mouse logical Y
//    win32_get_mouse_down()   § mouse button state
//    g_needs_present          § present flag (read access)
//
//  Build:
//    cd X:/runtime/native/src/ui_v2
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror -Wno-unused-function
//        -I X:/runtime/native/include -I . -D_WIN32
//        tree.c box_math.c damage.c draw_pixels.c arena.c hash_table.c
//        color.c attr_table.c kaintana_runtime_stubs.c
//        ../../src/core/arena.c ../../src/core/version.c
//        ../../src/core/component_surface.c ../../src/core/handle.c
//        ../../src/core/input_system.c
//        examples/api_coverage.c
//        -o examples/api_coverage.exe -lgdi32 -lws2_32 -lopengl32
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>     // Win32 API (before our includes)
#include <stdio.h>       // snprintf
#include <math.h>        // fmodf

// Include the backend source files directly (existing demo pattern).
// This puts all Win32 static state and functions into our translation unit.
#include "backends/win32/host_win32.c"
#include "backends/win32/render_gdi.c"

// ============================================================================
//  STATIC STATE — per-run tracking for input edge detection, animation, etc.
// ============================================================================

// Previous mouse button state for edge detection (kt_input_mouse_down/up)
static bool  prev_mouse_down[5] = { false };
static int   frame_counter      = 0;
static int   resize_since_reset = 0;  // track resize events


// ============================================================================
//  FORWARD DECLARATIONS — UI panel builders
// ============================================================================

static void build_title_bar(kt_Session* s, int root, int fb_w, int fb_h);
static void build_content_area(kt_Session* s, int root, int fb_w, int fb_h, float mx, float my);
static void build_status_bar(kt_Session* s, int root, int fb_w, int fb_h);
static void exercise_inline_helpers(kt_Session* s, int frame);

// ============================================================================
//  MAIN
// ============================================================================

int main(void) {
    // ── Initialize Kaintana system ──
    kt_init();

    // ── Create session ──
    // Window size hint (actual size may differ due to DPI, WM_SIZE, etc.)
    kt_Session* s = kt_make("Kaintana API Coverage", 1024, 768);
    if (!s) {
        fprintf(stderr, "kt_make failed\n");
        return 1;
    }

    // ── Register Win32 backend ──
    if (kt_backend_register(s, "win32", &kaintana_win32_backend) == 0) {
        fprintf(stderr, "kt_backend_register failed\n");
        kt_free(s);
        return 1;
    }

    // ── Exercise kt_backend_probe — auto-select first registered ──
    //     (then re-select explicitly to guarantee win32)
    kt_backend_probe(s);
    if (kt_backend_select(s, "win32") == 0) {
        fprintf(stderr, "kt_backend_select failed\n");
        kt_free(s);
        return 1;
    }

    // ── Exercise DPI setter APIs explicitly ──
    //     (win32_init also calls kt_set_native_scale during select;
    //      we call here again to exercise the public API directly)
    kt_set_native_scale(s, 1.0f, 1.0f);
    kt_set_zoom(s, 1.0f);

    // ── Initialize persistent state ──
    kt_put(s,   "click_count",  0);
    kt_put_f(s, "total_elapsed", 0.0);
    kt_put_s(s, "status",       "running");

    // ── Main loop ──
    while (!kt_should_close(s)) {
        // ── Pump OS message queue ──
        //     (updates win32 globals: g_mouse_x/y, g_mouse_down[], etc.)
        win32_pump_messages();

        // ── Query framebuffer size ──
        int fb_w = win32_get_fb_width();
        int fb_h = win32_get_fb_height();
        if (fb_w < 1) fb_w = 1024;
        if (fb_h < 1) fb_h = 768;

        // ── Feed input funnel from Win32 globals ──
        float mx = win32_get_mouse_x();
        float my = win32_get_mouse_y();

        kt_input_mouse_move(s, mx, my);

        // Edge-detect mouse button state changes
        for (int b = 0; b < 5; b++) {
            bool down = win32_get_mouse_down(b);
            if (down && !prev_mouse_down[b]) kt_input_mouse_down(s, b);
            if (!down && prev_mouse_down[b]) kt_input_mouse_up(s, b);
            prev_mouse_down[b] = down;
        }

        // Exercise scroll, key, and text funnel
        kt_input_scroll(s, 0.0f, 0.0f);
        kt_input_key_down(s, 32);   // Space
        kt_input_key_up(s, 32);

        if (frame_counter % 60 == 0) {
            kt_input_text(s, "K");  // Feed text every ~60 frames
        }

        // ── Begin frame (delta = ~16ms for 60fps target) ──
        kt_begin(s, 16.0);

        // ── Read framebuffer size for proportional layout ──
        int fw = win32_get_fb_width();
        int fh = win32_get_fb_height();
        if (fw < 1) fw = 1024;
        if (fh < 1) fh = 768;

        // Root: full-window column with padding
        int root = kt_row(s, 0, "column", "root");
        kt_width(s,  root, (float)fw);
        kt_height(s, root, (float)fh);
        kt_pad(s,    root, (float)fw * 0.02f);
        kt_fill(s,   root, "#1A1A2E");

        // ── Title bar ──
        build_title_bar(s, root, fw, fh);

        // ── Main content (left/right split) ──
        build_content_area(s, root, fw, fh, mx, my);

        // ── Status bar ──
        build_status_bar(s, root, fw, fh);

        kt_end_row(s);  // root

        // ── End frame ──
        kt_end(s);

        // ── Present ──
        kt_present(s);

        // ── Read draw output ──
        int cmd_count = kt_cmd_count(s);
        if (cmd_count > 0) {
            kt_Cmd first_cmd = kt_cmd_get(s, 0);
            (void)first_cmd;  // exercised — read first command
        }

        // ── Update state persistence ──
        int64_t clicks      = kt_get(s, "click_count", 0);
        double  elapsed     = kt_get_f(s, "total_elapsed", 0.0);
        const char* status  = kt_get_s(s, "status", "unknown");

        kt_put(s,   "click_count",  clicks + 1);
        kt_put_f(s, "total_elapsed", elapsed + 16.0);
        if (frame_counter == 0) {
            kt_put_s(s, "status", "api_coverage running");
        }
        (void)status;  // suppress unused

        // ── Exercise g_needs_present (global from host_win32.c) ──
        if (g_needs_present) {
            // Already cleared by win32_render during kt_present,
            // but we exercise read access to prove it's accessible.
            // win32_present_to_screen() is a safe no-op when !g_hwnd.
            win32_present_to_screen();
        }

        // ── Exercise inline helpers (color, easing, pixel-snap, blend) ──
        exercise_inline_helpers(s, frame_counter);

        // ── Detect resize events ──
        static int last_fb_w = 0, last_fb_h = 0;
        if (fw != last_fb_w || fh != last_fb_h) {
            resize_since_reset++;
            last_fb_w = fw;
            last_fb_h = fh;
        }

        frame_counter++;

        // ── Throttle to ~60fps ──
        Sleep(16);
    }

    // ── Cleanup ──
    kt_free(s);
    return 0;
}

// ============================================================================
//  UI PANEL BUILDERS
// ============================================================================

static void build_title_bar(kt_Session* s, int root, int fb_w, int fb_h) {
    float title_h = (float)fb_h * 0.06f;
    float title_w = (float)fb_w * 0.96f;

    int title_row = kt_row(s, root, "row", "title_bar");
    kt_width(s,   title_row, title_w);
    kt_height(s,  title_row, title_h);
    kt_fill(s,    title_row, "#16213E");
    kt_radius(s,  title_row, 8.0f);
    kt_pad(s,     title_row, 8.0f);

    // Title text
    int title_text = kt_row(s, title_row, "text", "title_text");
    kt_font(s,     title_text, 20.0f);
    kt_fill(s,     title_text, "#E94560");
    kt_text(s,     title_text, "Kaintana API Coverage");
    kt_end_row(s);

    // State counter (using kt_get)
    int title_state = kt_row(s, title_row, "text", "title_state");
    int64_t clicks = kt_get(s, "click_count", 0);
    char buf[80];
    snprintf(buf, sizeof(buf), "frames: %lld", (long long)clicks);
    kt_font(s,  title_state, 14.0f);
    kt_fill(s,  title_state, "#21D4A1");
    kt_text(s,  title_state, buf);
    kt_end_row(s);

    kt_end_row(s);  // title_row
}

// ── Main content: left panel (layout/style) + right panel (input/state) ──

static void build_content_area(kt_Session* s, int root, int fb_w, int fb_h, float mx, float my) {
    float content_h = (float)fb_h * 0.78f;
    float content_w = (float)fb_w * 0.96f;
    float gap       = (float)fb_w * 0.02f;
    float half_w    = (content_w - gap) * 0.5f;

    int content = kt_row(s, root, "row", "content");
    kt_width(s,   content, content_w);
    kt_height(s,  content, content_h);
    kt_gap(s,     content, gap);

    // ── LEFT PANEL: Layout & Style Demo ──
    {
        int left = kt_row(s, content, "column", "left_panel");
        kt_width(s,  left, half_w);
        kt_fill(s,   left, "#0F3460");
        kt_radius(s, left, 8.0f);
        kt_pad(s,    left, 10.0f);

        // ── Section header ──
        int hdr = kt_row(s, left, "text", "left_header");
        kt_font(s,  hdr, 18.0f);
        kt_fill(s,  hdr, "#E94560");
        kt_text(s,  hdr, "Layout & Style");
        kt_end_row(s);

        // ── 1. Filled + rounded + stroked box ──
        int box1 = kt_row(s, left, "box", "colored_box");
        kt_width(s,   box1, half_w * 0.85f);
        kt_height(s,  box1, (float)fb_h * 0.09f);
        kt_fill(s,    box1, "#21D4A1");
        kt_radius(s,  box1, 8.0f);
        kt_stroke(s,  box1, "#FFFFFF", 2.0f);
        kt_text(s,    box1, "Rounded Fill + Stroke");
        kt_end_row(s);

        // ── 2. Vertical gap — exercise kt_gap + kt_direction ──
        int gap_row = kt_row(s, left, "row", "gap_demo");
        kt_width(s,     gap_row, half_w * 0.85f);
        kt_height(s,    gap_row, (float)fb_h * 0.06f);
        kt_direction(s, gap_row, 0);   // row
        kt_gap(s,       gap_row, 10.0f);

        int ch_a = kt_row(s, gap_row, "box", "child_a");
        kt_width(s,  ch_a, half_w * 0.22f);
        kt_fill(s,   ch_a, "#FF6B6B");
        kt_radius(s, ch_a, 4.0f);
        kt_end_row(s);

        int ch_b = kt_row(s, gap_row, "box", "child_b");
        kt_width(s,  ch_b, half_w * 0.22f);
        kt_fill(s,   ch_b, "#4ECDC4");
        kt_radius(s, ch_b, 4.0f);
        kt_end_row(s);

        int ch_c = kt_row(s, gap_row, "box", "child_c");
        kt_width(s,  ch_c, half_w * 0.22f);
        kt_fill(s,   ch_c, "#45B7D1");
        kt_radius(s, ch_c, 4.0f);
        kt_end_row(s);

        kt_end_row(s);  // gap_demo

        // ── 3. Opacity (animated via easing) ──
        int fade_box = kt_row(s, left, "box", "opacity_box");
        kt_width(s,   fade_box, half_w * 0.60f);
        kt_height(s,  fade_box, (float)fb_h * 0.05f);
        kt_fill(s,    fade_box, "#E94560");
        kt_radius(s,  fade_box, 8.0f);
        float t = fmodf((float)frame_counter * 0.01f, 1.0f);
        float anim_opacity = 0.3f + kt_ease_in_out(t) * 0.7f;
        kt_opacity(s, fade_box, anim_opacity);
        kt_end_row(s);

        // ── 4. pad_xy exercise ──
        int pad_xy_demo = kt_row(s, left, "box", "pad_xy_demo");
        kt_width(s,     pad_xy_demo, half_w * 0.60f);
        kt_height(s,    pad_xy_demo, (float)fb_h * 0.04f);
        kt_fill(s,      pad_xy_demo, "#533483");
        kt_pad_xy(s,    pad_xy_demo, 20.0f, 5.0f);
        kt_end_row(s);

        // ── 5. Font size text ──
        int font_box = kt_row(s, left, "text", "font_demo");
        kt_font(s,  font_box, 22.0f);
        kt_fill(s,  font_box, "#F5F5F5");
        kt_text(s,  font_box, "Font 22px");
        kt_end_row(s);

        kt_end_row(s);  // left
    }

    // ── RIGHT PANEL: Input & State Demo ──
    {
        int right = kt_row(s, content, "column", "right_panel");
        kt_width(s,  right, half_w);
        kt_fill(s,   right, "#0F3460");
        kt_radius(s, right, 8.0f);
        kt_pad(s,    right, 10.0f);

        // ── Section header ──
        int hdr = kt_row(s, right, "text", "right_header");
        kt_font(s,  hdr, 18.0f);
        kt_fill(s,  hdr, "#E94560");
        kt_text(s,  hdr, "Input & State");
        kt_end_row(s);

        // ── Mouse position (from win32 globals) ──
        int mouse_info = kt_row(s, right, "text", "mouse_info");
        char buf[128];
        snprintf(buf, sizeof(buf), "Mouse: (%.1f, %.1f)", mx, my);
        kt_text(s, mouse_info, buf);
        kt_end_row(s);

        // ── Mouse button state ──
        int btn_info = kt_row(s, right, "text", "btn_info");
        snprintf(buf, sizeof(buf), "Btn0: %s",
                 win32_get_mouse_down(0) ? "DOWN" : "up");
        kt_text(s, btn_info, buf);
        kt_end_row(s);

        // ── State: click counter (kt_get) ──
        int state_line1 = kt_row(s, right, "text", "state_clicks");
        int64_t click_v = kt_get(s, "click_count", 0);
        snprintf(buf, sizeof(buf), "Clicks: %lld", (long long)click_v);
        kt_text(s, state_line1, buf);
        kt_end_row(s);

        // ── State: elapsed (kt_get_f) ──
        int state_line2 = kt_row(s, right, "text", "state_elapsed");
        double elap = kt_get_f(s, "total_elapsed", 0.0);
        snprintf(buf, sizeof(buf), "Elapsed: %.0f ms", elap);
        kt_text(s, state_line2, buf);
        kt_end_row(s);

        // ── State: status (kt_get_s) ──
        int state_line3 = kt_row(s, right, "text", "state_status");
        const char* st = kt_get_s(s, "status", "unknown");
        snprintf(buf, sizeof(buf), "Status: %s", st);
        kt_text(s, state_line3, buf);
        kt_end_row(s);

        // ── Draw command count ──
        int cmd_info = kt_row(s, right, "text", "cmd_info");
        int ccount = kt_cmd_count(s);
        snprintf(buf, sizeof(buf), "Cmds: %d", ccount);
        kt_text(s, cmd_info, buf);
        kt_end_row(s);

        // ── Resize counter ──
        int resize_info = kt_row(s, right, "text", "resize_info");
        snprintf(buf, sizeof(buf), "Resizes: %d", resize_since_reset);
        kt_text(s, resize_info, buf);
        kt_end_row(s);

        kt_end_row(s);  // right
    }

    kt_end_row(s);  // content
}

// ── Status bar — DPI info, zoom, scale, FB size ──

static void build_status_bar(kt_Session* s, int root, int fb_w, int fb_h) {
    float bar_h = (float)fb_h * 0.05f;
    float bar_w = (float)fb_w * 0.96f;

    int bar = kt_row(s, root, "row", "status_bar");
    kt_width(s,  bar, bar_w);
    kt_height(s, bar, bar_h);
    kt_fill(s,   bar, "#16213E");
    kt_radius(s, bar, 6.0f);
    kt_pad(s,    bar, 6.0f);
    kt_gap(s,    bar, 12.0f);

    // DPI scale
    float sx = kt_scale_factor_x(s);
    float sy = kt_scale_factor_y(s);
    float nx = kt_native_scale_x(s);
    float ny = kt_native_scale_y(s);

    int dpi_item = kt_row(s, bar, "text", "dpi_item");
    char buf[128];
    snprintf(buf, sizeof(buf), "DPI: %.2fx%.2f", nx, ny);
    kt_font(s,  dpi_item, 13.0f);
    kt_fill(s,  dpi_item, "#F5F5F5");
    kt_text(s,  dpi_item, buf);
    kt_end_row(s);

    // Zoom
    int zoom_item = kt_row(s, bar, "text", "zoom_item");
    snprintf(buf, sizeof(buf), "Zoom: %.1f", sx / nx);  // zoom = scale / native
    kt_text(s, zoom_item, buf);
    kt_end_row(s);

    // Scale
    int scale_item = kt_row(s, bar, "text", "scale_item");
    snprintf(buf, sizeof(buf), "Scale: (%.2f, %.2f)", sx, sy);
    kt_text(s, scale_item, buf);
    kt_end_row(s);

    // Framebuffer size
    int fb_item = kt_row(s, bar, "text", "fb_item");
    snprintf(buf, sizeof(buf), "FB: %dx%d", fb_w, fb_h);
    kt_text(s, fb_item, buf);
    kt_end_row(s);

    // Draw command count
    int cmd_item = kt_row(s, bar, "text", "cmd_count");
    int cmds = kt_cmd_count(s);
    snprintf(buf, sizeof(buf), "Cmds: %d", cmds);
    kt_text(s, cmd_item, buf);
    kt_end_row(s);

    kt_end_row(s);  // bar
}

// ============================================================================
//  INLINE HELPER EXERCISES — every color, easing, pixel-snap, blend inline
// ============================================================================

static void exercise_inline_helpers(kt_Session* s, int frame) {
    (void)s;  // not needed for inline functions

    // ── Pixel Snap inlines ──
    float scale = 1.5f;  // e.g. 1.5x DPI
    float rpx = kt_round_to_pixel_x(100.25f, scale);
    float rpy = kt_round_to_pixel_y(200.75f, scale);
    float rpc = kt_round_to_pixel_center_x(100.0f, scale);
    float opp = kt_one_physical_pixel(scale);
    float rui = kt_round_ui(123.456f);
    (void)rpx; (void)rpy; (void)rpc; (void)opp; (void)rui;

    // ── Color inlines ──
    kt_Color c1   = kt_color_from_u32(0xFF21D4A1);
    uint32_t c2   = kt_color_to_u32(c1);
    uint32_t c3   = kt_color_parse_hex("#E94560");
    kt_Color c4   = kt_color_premultiply(c1);
    kt_Color c5   = kt_color_unpremultiply(c4);
    kt_Color c6   = kt_color_lerp(c1, kt_color_from_u32(c3), 0.5f);
    float    lum  = kt_color_luminance(c1);
    float    sat  = kt_color_saturation(c1);
    kt_Color c7   = kt_apply_opacity(c1, 0.5f);
    uint32_t c8   = kt_apply_opacity_u32(0xFF21D4A1, 128);
    float    sl   = kt_color_srgb_to_linear(0.5f);
    float    ls   = kt_color_linear_to_srgb(0.2f);
    (void)c2; (void)c3; (void)c4; (void)c5; (void)c6;
    (void)lum; (void)sat; (void)c7; (void)c8; (void)sl; (void)ls;

    // ── Blend inlines ──
    kt_Color bg = kt_color_from_u32(0xFFFFFFFF);  // white background
    kt_Color blend = kt_blend_compose(c1, 0.8f, bg, 1.0f, 0);  // SRC_OVER
    (void)blend;

    // ── Easing inlines ──
    float t = fmodf((float)frame * 0.01f, 1.0f);
    float e1 = kt_ease_smoothstep(t);
    float e2 = kt_ease_smootherstep(t);
    float e3 = kt_ease_in(t);
    float e4 = kt_ease_out(t);
    float e5 = kt_ease_in_out(t);
    float e6 = kt_ease_cubic_in(t);
    float e7 = kt_ease_cubic_out(t);
    float e8 = kt_ease_cubic_in_out(t);
    (void)e1; (void)e2; (void)e3; (void)e4; (void)e5;
    (void)e6; (void)e7; (void)e8;
}

// ============================================================================
//  END OF api_coverage.c
// ============================================================================
