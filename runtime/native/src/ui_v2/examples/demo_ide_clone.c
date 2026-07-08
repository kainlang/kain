// ============================================================================
//  demo_ide_clone.c — Visual Studio Code-style IDE UI Demo (Win32 GDI)
//
//  Renders a complete VS Code-like IDE layout directly to the Win32 GDI
//  framebuffer using the pixel-level fill primitives from host_win32.c.
//
//  THIS VERSION bypasses the Kaintana element tree because the current
//  C substrate has fundamental limitations:
//    - Layout arena indices not allocated before attribute setters are called
//    - Hidden root node's resolved_size never initialized
//    - Layout direction hardcoded to row
//    - Initial node capacity (128) insufficient for complex layouts
//
//  Instead, we render rectangles with explicit pixel coordinates for each
//  IDE region.  The Win32 backend handles window creation, the message pump,
//  DIB framebuffer, SDF rounded-rect engine, and dirty-rect present.
//
//  Build:
//    python build.py examples/demo_ide_clone.c --run
//
//  Run:
//    examples/demo_ide_clone.exe
// ============================================================================

// ── Include the Win32 GDI backend directly ──────────────────────────────
#include "backends/win32/host_win32.c"

// ── Include internal session struct for vtable access
#include "internal.h"

// ── Stubs for GDI renderer lifecycle functions ──────────────────────────
int  gdi_renderer_init(HDC hdc, int w, int h)
    { (void)hdc; (void)w; (void)h; return 0; }
void gdi_renderer_shutdown(void) {}
void gdi_renderer_begin_frame(void) {}
void gdi_renderer_execute(HDC hdc, const kt_DrawData* dd, int fb_w, int fb_h)
    { (void)hdc; (void)dd; (void)fb_w; (void)fb_h; }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
//  VS CODE DARK+ COLOR PALETTE
// ============================================================================
//  Every color hex value is 0xAARRGGBB for premultiplied rendering.

#define C_BG              0xFF1E1E1E  // Editor background
#define C_MENU_BG         0xFF2D2D2D  // Menu bar background
#define C_SIDEBAR_BG      0xFF252526  // Sidebar / panels
#define C_ACTIVITY_BG     0xFF333333  // Activity (icon) bar
#define C_ACTIVITY_SEL    0xFF1E1E1E  // Selected activity item
#define C_ACTIVITY_TOP    0xFF007ACC  // Activity bar top accent
#define C_TAB_ACTIVE      0xFF1E1E1E  // Active tab
#define C_TAB_INACTIVE    0xFF2D2D2D  // Inactive tab
#define C_TAB_BAR_BG      0xFF252526  // Tab bar background
#define C_STATUS_BG       0xFF007ACC  // Status bar (blue)
#define C_GUTTER_BG       0xFF252526  // Line number gutter
#define C_TERMINAL_BG     0xFF1E1E1E  // Terminal panel
#define C_TERMINAL_HDR    0xFF2D2D2D  // Terminal header
#define C_BORDER          0xFF3C3C3C  // Panel borders
#define C_HEADER_SECT     0xFF2D2D2D  // Sidebar section header
#define C_FILE_SELECTED   0xFF37373D  // Selected file highlight
#define C_FOLDER_ENTRY    0xFF2A2A2A  // Folder entry
#define C_HIGHLIGHT       0xFF094771  // Hover / selection
#define C_SEP             0xFF1A66A8  // Status bar separator
#define C_SCROLLBAR       0xFF424242  // Scrollbar

// Syntax highlighting colors
#define C_KEYWORD         0xFF569CD6  // blue
#define C_STRING          0xFFCE9178  // orange
#define C_FUNCTION        0xFFDCDCAA  // yellow
#define C_TYPE            0xFF4EC9B0  // teal
#define C_COMMENT         0xFF6A9955  // green
#define C_NUMBER          0xFFB5CEA8  // pale green
#define C_VARIABLE        0xFF9CDCFE  // cyan
#define C_PLAIN           0xFFD4D4D4  // white

// ============================================================================
//  RENDER HELPERS
// ============================================================================
//  All coordinates are pixel-based. The framebuffer is 1200x800.

static void fill(int x, int y, int w, int h, uint32_t color) {
    win32_fb_fill_rect(x, y, x + w, y + h, color);
}

static void fill_rnd(int x, int y, int w, int h, float r, uint32_t color) {
    win32_fb_fill_rounded_rect(x, y, x + w, y + h, r, color);
}

// ============================================================================
//  MENU BAR
// ============================================================================
static void draw_menu_bar(void) {
    int y = 0;
    fill(0, y, 1200, 30, C_MENU_BG);

    // Individual menu item backgrounds
    int xs[] = { 4, 58, 110, 188, 240, 280, 326, 398 };
    int ws[] = { 50, 48, 74, 48, 36, 42, 68, 48 };
    for (int i = 0; i < 8; i++) {
        uint32_t c = (i == 0) ? 0xFF3C3C3C : C_MENU_BG;
        fill_rnd(xs[i], y + 2, ws[i], 26, 3, c);
    }
}

// ============================================================================
//  ACTIVITY BAR
// ============================================================================
static void draw_activity_bar(void) {
    int x = 0, y = 31;  // below menu border
    fill(x, y, 48, 748, C_ACTIVITY_BG);

    // Top accent
    fill(x, y, 48, 3, C_ACTIVITY_TOP);

    // Icon slots (48x48 each)
    for (int i = 0; i < 5; i++) {
        int iy = y + 4 + i * 49;
        uint32_t bg = (i == 0) ? C_ACTIVITY_SEL : C_ACTIVITY_BG;
        fill(x, iy, 48, 48, bg);

        // Left indicator
        fill(x, iy, 2, 48, (i == 0) ? 0xFF007ACC : C_ACTIVITY_BG);

        // Icon square
        uint32_t ic = (i == 0) ? 0xFFFFFFFF : 0xFF858585;
        fill_rnd(x + 12, iy + 12, 24, 24, 4, ic);

        // Separator
        if (i < 4) fill(x, iy + 48, 48, 1, C_BORDER);
    }
}

// ============================================================================
//  SIDEBAR
// ============================================================================
static void draw_sidebar(void) {
    int x = 50, y = 31;  // after activity bar + 1px border
    fill(x, y, 260, 748, C_SIDEBAR_BG);

    // Header
    fill(x, y, 260, 35, C_HEADER_SECT);

    // Section: OPEN EDITORS
    int sy = y + 36;
    fill(x, sy, 260, 22, C_SIDEBAR_BG);

    // File entry 1: main.kn (selected)
    fill(x, sy + 22, 260, 22, C_FILE_SELECTED);
    fill_rnd(x + 22, sy + 24, 14, 14, 2, C_KEYWORD);

    // File entry 2: style.css
    fill(x, sy + 44, 260, 22, C_SIDEBAR_BG);
    fill_rnd(x + 22, sy + 46, 14, 14, 2, C_STRING);

    // Section: WORKSPACE
    int wy = sy + 66;
    fill(x, wy, 260, 22, C_SIDEBAR_BG);

    // Folder: src/
    int fy = wy + 22;
    fill(x, fy, 260, 22, C_FOLDER_ENTRY);
    fill_rnd(x + 36, fy + 4, 14, 14, 2, C_TYPE);

    // src/main.kn, src/utils.kn, src/types.kn
    for (int i = 0; i < 3; i++) {
        int ffy = fy + 22 + i * 20;
        fill(x, ffy, 260, 20, C_SIDEBAR_BG);
        uint32_t fc = (i == 0) ? C_KEYWORD : ((i == 1) ? C_FUNCTION : C_TYPE);
        fill_rnd(x + 36, ffy + 3, 13, 13, 2, fc);
    }

    // Folder: tests/
    int tfy = fy + 22 + 3 * 20;
    fill(x, tfy, 260, 22, C_FOLDER_ENTRY);
    fill_rnd(x + 36, tfy + 4, 14, 14, 2, C_TYPE);

    // tests/test_main.kn
    fill(x, tfy + 22, 260, 20, C_SIDEBAR_BG);
    fill_rnd(x + 36, tfy + 25, 13, 13, 2, C_KEYWORD);
}

// ============================================================================
//  TAB BAR
// ============================================================================
static void draw_tab_bar(void) {
    int x = 312, y = 31;  // after sidebar + 1px border
    fill(x, y, 892, 35, C_TAB_BAR_BG);

    // Tab: main.kn (active)
    fill(x, y, 120, 35, C_TAB_ACTIVE);
    fill(x, y, 120, 2, 0xFF007ACC);   // active indicator

    // Tab: style.css
    fill(x + 120, y, 100, 35, C_TAB_INACTIVE);
    fill(x + 120, y, 100, 2, C_TAB_INACTIVE);

    // Tab: README.md
    fill(x + 220, y, 110, 35, C_TAB_INACTIVE);
    fill(x + 220, y, 110, 2, C_TAB_INACTIVE);
}

// ============================================================================
//  CODE LINES — 30+ syntax-highlighted lines
// ============================================================================
//  Each entry: { indent_px, { segment_width, color }[] }
//  n_segments = -1 terminates.

static void draw_code_lines(void) {
    int ex = 312, ey = 68;  // after tab bar + 1px border
    int editor_w = 892;
    int editor_h = 513;
    fill(ex, ey, editor_w, editor_h, C_BG);

    // Code line patterns: {indent, n_seg, seg0_w, seg0_c, seg1_w, seg1_c, ...}
    struct { int indent; int segs[8]; } lines[] = {
        { 0,  { 90, 0x569CD6,  200, 0x9CDCFE, 0,0,0,0 } },
        { 0,  { 70, 0x569CD6,  160, 0x4EC9B0, 20, 0xD4D4D4, 0,0 } },
        { 0,  { 60, 0x6A9955,  0,0,0,0,0,0 } },
        { 0,  { 0,0,0,0,0,0,0,0 } },  // blank
        { 0,  { 90, 0x569CD6,  100, 0x4EC9B0, 40, 0xD4D4D4, 20, 0xD4D4D4 } },
        { 12, { 40, 0x569CD6,  0,0,0,0,0,0 } },
        { 12, { 70, 0x9CDCFE,  140, 0x4EC9B0, 20, 0xD4D4D4, 0,0 } },
        { 12, { 60, 0x9CDCFE,  80, 0x4EC9B0, 20, 0xD4D4D4, 0,0 } },
        { 12, { 80, 0x9CDCFE,  100, 0xCE9178, 20, 0xD4D4D4, 0,0 } },
        { 0,  { 20, 0xD4D4D4,  0,0,0,0,0,0 } },
        { 0,  { 0,0,0,0,0,0,0,0 } },
        { 0,  { 80, 0x569CD6,  120, 0xDCDCAA, 30, 0xD4D4D4, 60, 0x9CDCFE } },
        { 0,  { 100, 0xD4D4D4, 40, 0xD4D4D4, 20, 0xD4D4D4, 0,0 } },
        { 12, { 60, 0x569CD6,  80, 0x9CDCFE, 0,0,0,0 } },
        { 12, { 70, 0x569CD6,  60, 0x9CDCFE, 40, 0xD4D4D4, 20, 0xB5CEA8 } },
        { 24, { 70, 0x569CD6,  120, 0x9CDCFE, 20, 0xD4D4D4, 0,0 } },
        { 24, { 70, 0x9CDCFE,  60, 0xD4D4D4, 40, 0xB5CEA8, 0,0 } },
        { 24, { 60, 0x569CD6,  80, 0x9CDCFE, 0,0,0,0 } },
        { 24, { 70, 0x9CDCFE,  120, 0xDCDCAA, 40, 0xD4D4D4, 20, 0xD4D4D4 } },
        { 12, { 70, 0x569CD6,  60, 0x9CDCFE, 40, 0xD4D4D4, 0,0 } },
        { 24, { 70, 0x9CDCFE,  100, 0xDCDCAA, 40, 0xD4D4D4, 60, 0x9CDCFE } },
        { 12, { 20, 0xD4D4D4,  0,0,0,0,0,0 } },
        { 0,  { 0,0,0,0,0,0,0,0 } },
        { 0,  { 80, 0x569CD6,  140, 0xDCDCAA, 40, 0xD4D4D4, 20, 0xD4D4D4 } },
        { 12, { 70, 0x569CD6,  60, 0x9CDCFE, 100, 0xD4D4D4, 0,0 } },
        { 12, { 70, 0x569CD6,  60, 0x9CDCFE, 80, 0xD4D4D4, 20, 0xB5CEA8 } },
        { 12, { 60, 0x569CD6,  100, 0xDCDCAA, 0,0,0,0 } },
        { 24, { 70, 0x569CD6,  80, 0x9CDCFE, 40, 0xD4D4D4, 60, 0x9CDCFE } },
        { 24, { 70, 0x9CDCFE,  60, 0xD4D4D4, 60, 0xB5CEA8, 0,0 } },
        { 12, { 70, 0x569CD6,  60, 0x9CDCFE, 80, 0xCE9178, 0,0 } },
        { 12, { 20, 0xD4D4D4,  0,0,0,0,0,0 } },
        { 0,  { 20, 0xD4D4D4,  0,0,0,0,0,0 } },
    };
    int n_lines = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < n_lines; i++) {
        int ly = ey + 3 + i * 15;

        // Render segments for this line
        int cx = ex + lines[i].indent;
        for (int j = 0; j < 6; j += 2) {
            int sw = lines[i].segs[j];
            uint32_t sc = (uint32_t)lines[i].segs[j + 1];
            if (sw == 0) break;
            fill_rnd(cx, ly, sw, 11, 2, sc);
            cx += sw;
        }
    }
}

// ============================================================================
//  TERMINAL PANEL
// ============================================================================
static void draw_terminal(void) {
    int x = 312, y = 582;  // after editor + 1px border
    fill(x, y, 892, 198, C_TERMINAL_BG);

    // Terminal header
    fill(x, y, 892, 30, C_TERMINAL_HDR);
    fill_rnd(x + 6, y + 3, 80, 24, 3, C_ACTIVITY_TOP);

    // Terminal output lines
    int ty = y + 32;
    (void)0; // texts placeholder
    for (int i = 0; i < 9; i++) {
        uint32_t tc;
        int tw;
        if (i == 0) {
            // Prompt
            fill_rnd(x + 10, ty, 120, 12, 2, C_VARIABLE);
            fill_rnd(x + 134, ty, 220, 12, 2, C_PLAIN);
        } else if (i == 1) {
            // empty line
        } else {
            tc = (i <= 4 || i >= 7) ? C_PLAIN : C_COMMENT;
            tw = (i == 8) ? 320 : (180 + i * 30);
            if (tw > 800) tw = 800;
            fill_rnd(x + 10, ty, tw, 12, 2, tc);
        }
        ty += 17;
    }
}

// ============================================================================
//  EDITOR GROUP
// ============================================================================
static void draw_editor_group(void) {
    draw_tab_bar();
    // Border between tabs and editor
    fill(312, 66, 892, 1, C_BORDER);
    draw_code_lines();
    // Border between editor and terminal
    fill(312, 581, 892, 1, C_BORDER);
    draw_terminal();
}

// ============================================================================
//  STATUS BAR
// ============================================================================
static void draw_status_bar(void) {
    int y = 778;  // 800 - 22
    fill(0, y, 1200, 22, C_STATUS_BG);

    // Left group
    int x = 4;
    fill(x, y, 90, 22, C_STATUS_BG); x += 90;     // Ln 42, Col 12
    fill(x, y + 4, 1, 14, C_SEP); x += 4;          // separator
    fill(x, y, 70, 22, C_STATUS_BG); x += 70;      // Spaces: 4
    fill(x, y + 4, 1, 14, C_SEP); x += 4;
    fill(x, y, 50, 22, C_STATUS_BG); x += 50;      // UTF-8
    fill(x, y + 4, 1, 14, C_SEP); x += 4;
    fill(x, y, 50, 22, C_STATUS_BG);               // Indent

    // Right group
    x = 1200 - 280;
    fill(x, y, 50, 22, C_STATUS_BG); x += 50;      // Kain
    fill(x, y + 4, 1, 14, C_SEP); x += 4;
    fill(x, y, 60, 22, C_STATUS_BG); x += 60;      // Prettier
    fill(x, y + 4, 1, 14, C_SEP); x += 4;
    fill(x, y, 60, 22, C_STATUS_BG); x += 60;      // UTF-8
    fill(x, y + 4, 1, 14, C_SEP); x += 4;
    fill(x, y, 40, 22, C_STATUS_BG);               // LF
}

// ============================================================================
//  IDE RENDER — Draws the complete IDE layout
// ============================================================================
static void render_ide(void) {
    // Background fill
    fill(0, 0, 1200, 800, C_BG);

    // Menu bar
    draw_menu_bar();

    // Border below menu
    fill(0, 30, 1200, 1, C_BORDER);

    // Activity bar + border
    draw_activity_bar();
    fill(48, 31, 1, 748, C_BORDER);

    // Sidebar + border
    draw_sidebar();
    fill(310, 31, 1, 748, C_BORDER);

    // Editor group
    draw_editor_group();

    // Border above status bar
    fill(0, 777, 1200, 1, C_BORDER);

    // Status bar
    draw_status_bar();
}

// ============================================================================
//  MAIN
// ============================================================================
int main(void) {
    printf("=== Kaintana IDE Clone Demo (Win32 GDI) ===\n\n");

    // Init Kaintana system
    kt_init();

    // Create session
    kt_Session* s = kt_make("Kain IDE - Kaintana Demo", 1200, 800);
    if (!s) { fprintf(stderr, "FAIL: kt_make\n"); return 1; }
    printf("[OK] Session created\n"); fflush(stdout);

    // Register and select Win32 backend
    kt_backend_register(s, "win32", &kaintana_win32_backend);
    if (!kt_backend_select(s, "win32")) {
        fprintf(stderr, "FAIL: kt_backend_select\n");
        kt_free(s); return 1;
    }
    printf("[OK] Win32 backend selected\n"); fflush(stdout);

    // Init Win32 backend (creates the window)
    KaintanaBackendConfig config = {
        .title = "Kain IDE - Kaintana Demo",
        .width = 1200, .height = 800,
        .fullscreen = 0, .platform_handle = NULL
    };
    if (kaintana_win32_backend.init(&config) != 0) {
        fprintf(stderr, "FAIL: init\n");
        kt_free(s); return 1;
    }
    printf("[OK] Window created (1200x800)\n\n"); fflush(stdout);

    // ── Frame loop ─────────────────────────────────────
    int total_frames = 15;
    double frame_delta = 16.0; (void)frame_delta;

    printf("--- Frame Loop ---\n"); fflush(stdout);

    printf("  Starting frame loop...\n"); fflush(stdout);
    for (int frame = 0; frame < total_frames; frame++) {
        if (win32_should_close()) break;

        // Win32: pump messages, update timer
        kaintana_win32_backend.new_frame();

        // Render the IDE directly to the DIB framebuffer
        // Clear framebuffer
        if (g_pBits) {
            memset(g_pBits, 0, (size_t)(g_fb_width * g_fb_height * 4));
        }
        g_full_dirty = true;

        // Draw
        render_ide();

        // Mark dirty and present
        g_needs_present = true;
        win32_present_to_screen();

        printf("  Frame %2d\n", frame + 1);
        fflush(stdout);

        Sleep(16);
    }

    printf("\n--- Frame loop complete ---\n");
    printf("  Window remains open. Close it to exit.\n\n");
    fflush(stdout);

    // Idle loop
    while (!win32_should_close()) {
        kaintana_win32_backend.new_frame();
        if (g_pBits) memset(g_pBits, 0, (size_t)(g_fb_width * g_fb_height * 4));
        g_full_dirty = true;
        render_ide();
        g_needs_present = true;
        win32_present_to_screen();
        Sleep(16);
    }

    printf("--- Shutting down ---\n"); fflush(stdout);
    kaintana_win32_backend.shutdown();
    kt_free(s);
    printf("[OK] Clean exit.\n");
    fflush(stdout);
    return 0;
}
