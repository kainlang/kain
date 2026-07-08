// ============================================================================
//  host_x11.c — X11 backend for Kaintana
//
//  Implements the 4-function KaintanaBackendVTable contract with a real X11
//  window, software framebuffer via XImage, message pump, and XPutImage
//  presentation.
//
//  Architecture:
//    - Xlib + XImage software rendering (no GPU, no DRI)
//    - SINGLE static XImage wrapping a private pixel buffer
//    - Full event pump: mouse, keyboard, scroll, text, close, resize, focus
//    - DPI detection via Xft.dpi / GDK_SCALE / DisplayWidthMM chain
//    - 16-deep clip rect stack (matching Clay architecture)
//    - Premultiplied ARGB pixel format (0xAARRGGBB, little-endian)
//    - Zero hardcoded colors — all from kt_DrawData command stream
//    - Zero platform headers in core kernel (host_x11.c is the sole X11 file)
//
//  Usage:
//    extern const KaintanaBackendVTable kaintana_x11_backend;
//    kt_backend_register(s, "x11", &kaintana_x11_backend);
//    kt_backend_select(s, "x11");
//
//  Build:
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror
//        -I X:/runtime/native/include
//        -I X:/runtime/native/src/ui_v2
//        -fsyntax-only backends/x11/host_x11.c
//    Link: -lX11
//
//  Verify compilation (Windows cross-compile syntax check):
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror
//        -I X:/runtime/native/include
//        -I X:/runtime/native/src/ui_v2
//        -fsyntax-only backends/x11/host_x11.c
// ============================================================================

#include "../../kaintana.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>    // For Xft.dpi
#include <X11/keysym.h>       // KeySym → keycode mapping
#include <X11/cursorfont.h>   // Standard cursors

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
//  CONSTANTS
// ============================================================================
#define X11_DEFAULT_WIDTH        800
#define X11_DEFAULT_HEIGHT       600
#define X11_CLIP_STACK_MAX       16        // Matches NULL_CLIP_MAX_DEPTH
#define X11_TEXT_BUF_SIZE        32        // Per-frame text input buffer

// ============================================================================
//  STATIC STATE — singleton window + framebuffer + input
// ============================================================================

// ── Window ──────────────────────────────────────────────────────────────────
static Display*     g_display           = NULL;
static Window       g_window            = 0;
static int          g_screen            = 0;
static GC           g_gc                = NULL;
static Atom         g_wm_delete_window;
static Atom         g_net_wm_name;
static Atom         g_clipboard_atom;
static Atom         g_clipboard_targets;
static Atom         g_utf8_string_atom;
static Atom         g_clipboard_primary;

// ── Framebuffer (software) ──────────────────────────────────────────────────
static XImage*      g_ximage            = NULL;
static uint32_t*    g_pixels            = NULL;      // Direct pixel pointer
static int          g_fb_width          = 0;
static int          g_fb_height         = 0;

// ── Window geometry ─────────────────────────────────────────────────────────
static int          g_win_width         = X11_DEFAULT_WIDTH;
static int          g_win_height        = X11_DEFAULT_HEIGHT;
static bool         g_is_open           = false;
static bool         g_should_close      = false;

// ── Clip rect stack ─────────────────────────────────────────────────────────
static kt_Rect      g_clip_stack[X11_CLIP_STACK_MAX];
static int          g_clip_depth        = -1;   // -1 = no clip, full framebuffer

// ── Input state ─────────────────────────────────────────────────────────────
static float        g_mouse_x           = 0.0f;
static float        g_mouse_y           = 0.0f;
static bool         g_mouse_down[5]     = { false };
static float        g_scroll_dx         = 0.0f;
static float        g_scroll_dy         = 0.0f;
static bool         g_keys[256]         = { false };
static char         g_text_buf[X11_TEXT_BUF_SIZE];
static int          g_text_len          = 0;
static bool         g_focus_gained      = true;

// ── DPI ─────────────────────────────────────────────────────────────────────
static float        g_dpi_scale_x       = 1.0f;
static float        g_dpi_scale_y       = 1.0f;

// ── Session pointer (set via config->platform_handle in kt_backend_select) ──
static kt_Session*  g_x11_session       = NULL;

// ── Performance timer ───────────────────────────────────────────────────────
static struct timespec g_last_time;
static double       g_delta_seconds     = 0.016;

// ── Clipboard text buffer (simplified) ──────────────────────────────────────
static char         g_clipboard_text[4096];
static int          g_clipboard_text_len = 0;

// ============================================================================
//  CLIP RECT HELPERS (same pattern as host_null.c)
// ============================================================================

static inline kt_Rect x11_clip_current(void) {
    kt_Rect full;
    full.x = 0.0f;
    full.y = 0.0f;
    full.w = (float)g_fb_width;
    full.h = (float)g_fb_height;

    if (g_clip_depth < 0)
        return full;

    return g_clip_stack[g_clip_depth];
}

static void x11_clip_push(kt_Rect r) {
    if (g_clip_depth >= X11_CLIP_STACK_MAX - 1)
        return;

    kt_Rect cur = x11_clip_current();

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

static void x11_clip_pop(void) {
    if (g_clip_depth >= 0)
        g_clip_depth--;
}

// ============================================================================
//  PIXEL FILL — Fill a bounding rectangle with a solid color.
//
//  The fill rect is first intersected with the current clip rect, then
//  clamped to the framebuffer dimensions. Every pixel inside the
//  intersected region is set to `color` (premultiplied ARGB).
//  Same dual-pixel fill pattern as host_null.c.
// ============================================================================

static void x11_fill_rect(kt_Rect bounds, uint32_t color) {
    if (!g_pixels)
        return;

    kt_Rect clip = x11_clip_current();

    // Intersect bounds with clip rect (max of left/top, min of right/bottom)
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

    // Fast path: opaque color — memcpy whole rows
    if ((color & 0xFF000000) == 0xFF000000) {
        for (int y = iy1; y < iy2; y++) {
            uint32_t* row = g_pixels + (y * g_fb_width);
            for (int x = ix1; x < ix2; x++) {
                row[x] = color;
            }
        }
    } else if ((color & 0xFF000000) == 0) {
        // Fully transparent: skip entirely
        return;
    } else {
        // Premultiplied SrcOver blend (same as win32/null backends)
        uint32_t sa = (color >> 24) & 0xFF;
        uint32_t inv_a = 255 - sa;
        uint32_t src_rb = color & 0x00FF00FF;
        uint32_t src_g  = color & 0x0000FF00;

        for (int y = iy1; y < iy2; y++) {
            uint32_t* row = g_pixels + (y * g_fb_width);
            for (int x = ix1; x < ix2; x++) {
                uint32_t dst = row[x];
                uint32_t dst_rb = dst & 0x00FF00FF;
                uint32_t dst_g  = dst & 0x0000FF00;
                uint32_t out_rb = src_rb + kaintana__DIV255(dst_rb * inv_a);
                uint32_t out_g  = src_g  + kaintana__DIV255(dst_g  * inv_a);
                row[x] = (out_rb & 0x00FF00FF) | (out_g & 0x0000FF00)
                       | (0xFF000000 & ~(inv_a << 24));
            }
        }
    }
}

// ============================================================================
//  FILL ROUNDED RECT — Same branchless Quilez SDF pattern as win32 backend
// ============================================================================

static void x11_fill_rounded_rect(kt_Rect bounds, float radius, uint32_t color) {
    if (!g_pixels || radius <= 0.5f) {
        x11_fill_rect(bounds, color);
        return;
    }

    kt_Rect clip = x11_clip_current();

    // Intersect with clip rect
    float x1 = (bounds.x > clip.x) ? bounds.x : clip.x;
    float y1 = (bounds.y > clip.y) ? bounds.y : clip.y;
    float bx2 = bounds.x + bounds.w;
    float by2 = bounds.y + bounds.h;
    float cx2 = clip.x + clip.w;
    float cy2 = clip.y + clip.h;
    float x2 = (bx2 < cx2) ? bx2 : cx2;
    float y2 = (by2 < cy2) ? by2 : cy2;

    if (x2 <= x1 || y2 <= y1)
        return;

    int ix1 = (int)x1; if (ix1 < 0) ix1 = 0;
    int iy1 = (int)y1; if (iy1 < 0) iy1 = 0;
    int ix2 = (int)(x2 + 0.5f); if (ix2 > g_fb_width)  ix2 = g_fb_width;
    int iy2 = (int)(y2 + 0.5f); if (iy2 > g_fb_height) iy2 = g_fb_height;

    // Branchless Quilez SDF rounded rect
    float rx = bounds.x;
    float ry = bounds.y;
    float rw = bounds.w;
    float rh = bounds.h;
    float r  = radius;

    if ((color & 0xFF000000) == 0xFF000000) {
        // Opaque fast path
        for (int py = iy1; py < iy2; py++) {
            uint32_t* row = g_pixels + (py * g_fb_width);
            float fy = (float)py + 0.5f;
            for (int px = ix1; px < ix2; px++) {
                float fx = (float)px + 0.5f;
                // Quilez SDF: distance from rounded rect
                float dx = fx - rx;
                float dy = fy - ry;
                float qx = fmaxf(0.0f, fminf(dx, rw));
                float qy = fmaxf(0.0f, fminf(dy, rh));
                float ex = fabsf(dx - qx);
                float ey = fabsf(dy - qy);
                float dist = sqrtf(ex * ex + ey * ey) - r;
                if (dist <= 0.0f) {
                    row[px] = color;
                }
            }
        }
    } else if ((color & 0xFF000000) == 0) {
        return;  // Fully transparent
    } else {
        // Transparent blend path
        uint32_t sa = (color >> 24) & 0xFF;
        uint32_t inv_a = 255 - sa;
        uint32_t src_rb = color & 0x00FF00FF;
        uint32_t src_g  = color & 0x0000FF00;

        for (int py = iy1; py < iy2; py++) {
            uint32_t* row = g_pixels + (py * g_fb_width);
            float fy = (float)py + 0.5f;
            for (int px = ix1; px < ix2; px++) {
                float fx = (float)px + 0.5f;
                float dx = fx - rx;
                float dy = fy - ry;
                float qx = fmaxf(0.0f, fminf(dx, rw));
                float qy = fmaxf(0.0f, fminf(dy, rh));
                float ex = fabsf(dx - qx);
                float ey = fabsf(dy - qy);
                float dist = sqrtf(ex * ex + ey * ey) - r;
                if (dist <= 0.0f) {
                    uint32_t dst = row[px];
                    uint32_t dst_rb = dst & 0x00FF00FF;
                    uint32_t dst_g  = dst & 0x0000FF00;
                    uint32_t out_rb = src_rb + kaintana__DIV255(dst_rb * inv_a);
                    uint32_t out_g  = src_g  + kaintana__DIV255(dst_g  * inv_a);
                    row[px] = (out_rb & 0x00FF00FF) | (out_g & 0x0000FF00)
                            | (0xFF000000 & ~(inv_a << 24));
                }
            }
        }
    }
}

// ============================================================================
//  STROKE FILL — 4 thin rects (top, bottom, left, right)
// ============================================================================

static void x11_stroke_rect(kt_Rect bounds, float thickness, uint32_t color) {
    if (!g_pixels || thickness <= 0.0f)
        return;

    float t = thickness;
    float x = bounds.x;
    float y = bounds.y;
    float w = bounds.w;
    float h = bounds.h;

    // Top edge
    kt_Rect top  = { x, y, w, t };
    x11_fill_rect(top, color);

    // Bottom edge
    kt_Rect bot  = { x, y + h - t, w, t };
    x11_fill_rect(bot, color);

    // Left edge
    kt_Rect left = { x, y + t, t, h - 2.0f * t };
    x11_fill_rect(left, color);

    // Right edge
    kt_Rect right = { x + w - t, y + t, t, h - 2.0f * t };
    x11_fill_rect(right, color);
}

// ============================================================================
//  XIMAGE FRAMEBUFFER
//
//  We create an XImage wrapping our private pixel buffer (g_pixels).
//  XPutImage is called each frame to push pixels to the X server.
//
//  CRITICAL DESIGN NOTE:
//    XCreateImage allocates its own data buffer. We immediately free it
//    and set g_ximage->data to point to our g_pixels buffer. XDestroyImage
//    will NOT free g_pixels — we must free it manually in shutdown.
// ============================================================================

static int x11_fb_create(int width, int height) {
    if (width <= 0 || height <= 0) return -1;

    g_fb_width  = width;
    g_fb_height = height;

    // Allocate pixel buffer
    g_pixels = (uint32_t*)calloc(1, (size_t)(g_fb_width * g_fb_height * 4));
    if (!g_pixels) return -1;

    // Create XImage wrapping the buffer
    g_ximage = XCreateImage(g_display, DefaultVisual(g_display, g_screen),
                            DefaultDepth(g_display, g_screen),
                            ZPixmap, 0,
                            (char*)g_pixels, width, height, 32, width * 4);

    if (!g_ximage) {
        free(g_pixels);
        g_pixels = NULL;
        return -1;
    }

    // Overwrite XImage's auto-allocated data with our buffer
    // XCreateImage allocated its own; we free it and use ours.
    free(g_ximage->data);
    g_ximage->data = (char*)g_pixels;
    g_ximage->bytes_per_line = width * 4;

    return 0;
}

static void x11_fb_destroy(void) {
    // CRITICAL: Prevent XDestroyImage from freeing our buffer
    // We separate the XImage struct destroy from pixel free.
    if (g_ximage) {
        g_ximage->data = NULL;  // Prevent double-free of g_pixels
        XDestroyImage(g_ximage);
        g_ximage = NULL;
    }

    free(g_pixels);
    g_pixels      = NULL;
    g_fb_width    = 0;
    g_fb_height   = 0;
}

// ============================================================================
//  PRESENT — Push pixels to X server via XPutImage
// ============================================================================

static void x11_present_to_screen(void) {
    if (!g_display || !g_window || !g_ximage)
        return;

    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0,              // Source x, y (full image)
              0, 0,              // Dest x, y (top-left of window)
              (unsigned int)g_fb_width,
              (unsigned int)g_fb_height);

    XFlush(g_display);
}

// ============================================================================
//  DPI DETECTION
//
//  Priority chain:
//    1. Xft.dpi from X resources (most reliable)
//    2. GDK_SCALE env var
//    3. QT_SCALE_FACTOR env var
//    4. DisplayWidthMM back-calculation
//    5. Default 96 DPI
// ============================================================================

static void x11_detect_dpi(void) {
    float dpi = 96.0f;  // Default

    // Method 1: Xft.dpi via X resources (most common on modern desktops)
    XrmInitialize();
    char* rdb = XResourceManagerString(g_display);
    if (rdb) {
        XrmDatabase db = XrmGetStringDatabase(rdb);
        if (db) {
            XrmValue value;
            char* type;
            if (XrmGetResource(db, "Xft.dpi", "Xft.dpi", &type, &value)) {
                if (value.addr) {
                    float parsed = (float)atof(value.addr);
                    if (parsed > 0) dpi = parsed;
                }
            }
            XrmDestroyDatabase(db);
        }
    }

    // Method 2: GDK_SCALE env var (integer scaling, GTK apps)
    if (dpi <= 0) {
        const char* gdk = getenv("GDK_SCALE");
        if (gdk) {
            int scale = atoi(gdk);
            if (scale > 0) dpi = (float)scale * 96.0f;
        }
    }

    // Method 3: QT_SCALE_FACTOR env var (fractional scaling, Qt apps)
    if (dpi <= 0) {
        const char* qt = getenv("QT_SCALE_FACTOR");
        if (qt) {
            float factor = (float)atof(qt);
            if (factor > 0.0f) dpi = factor * 96.0f;
        }
    }

    // Method 4: DisplayWidthMM back-calculation (unreliable fallback)
    if (dpi <= 0 || dpi < 72.0f || dpi > 384.0f) {
        int w_pix = DisplayWidth(g_display, g_screen);
        int w_mm  = DisplayWidthMM(g_display, g_screen);
        if (w_mm > 0) {
            dpi = (float)w_pix / ((float)w_mm / 25.4f);
        }
    }

    // Clamp
    if (dpi < 72.0f)  dpi = 72.0f;
    if (dpi > 384.0f) dpi = 384.0f;

    g_dpi_scale_x = dpi / 96.0f;
    g_dpi_scale_y = dpi / 96.0f;
}

// ============================================================================
//  KEYBOARD — KeySym to Kaintana keycode mapping
//
//  Kaintana uses DOS/Windows scancode values (VK_*). This table maps
//  X11 KeySym values to the corresponding VK code.
// ============================================================================

static int x11_keysym_to_kain(KeySym ks) {
    // ASCII printable range (0x20-0x7E): direct mapping
    if (ks >= XK_space && ks <= XK_asciitilde)
        return (int)ks;  // Direct ASCII code = VK code for most

    // Special keys
    switch (ks) {
    case XK_Return:    case XK_KP_Enter:   return 13;   // VK_RETURN
    case XK_Tab:                           return 9;    // VK_TAB
    case XK_BackSpace:                     return 8;    // VK_BACK
    case XK_Escape:                        return 27;   // VK_ESCAPE
    case XK_Left:                          return 37;   // VK_LEFT
    case XK_Up:                            return 38;   // VK_UP
    case XK_Right:                         return 39;   // VK_RIGHT
    case XK_Down:                          return 40;   // VK_DOWN
    case XK_Shift_L:   case XK_Shift_R:    return 16;   // VK_SHIFT
    case XK_Control_L: case XK_Control_R:  return 17;   // VK_CONTROL
    case XK_Alt_L:     case XK_Alt_R:      return 18;   // VK_MENU
    case XK_Super_L:   case XK_Super_R:    return 91;   // VK_LWIN
    case XK_Delete:                        return 46;   // VK_DELETE
    case XK_Home:                          return 36;   // VK_HOME
    case XK_End:                           return 35;   // VK_END
    case XK_Page_Up:                       return 33;   // VK_PRIOR
    case XK_Page_Down:                     return 34;   // VK_NEXT
    case XK_Insert:                        return 45;   // VK_INSERT
    case XK_F1:                            return 112;  // VK_F1
    case XK_F2:                            return 113;
    case XK_F3:                            return 114;
    case XK_F4:                            return 115;
    case XK_F5:                            return 116;
    case XK_F6:                            return 117;
    case XK_F7:                            return 118;
    case XK_F8:                            return 119;
    case XK_F9:                            return 120;
    case XK_F10:                           return 121;
    case XK_F11:                           return 122;
    case XK_F12:                           return 123;
    default:                               return 0;
    }
}

// ============================================================================
//  INPUT HANDLERS
// ============================================================================

static void x11_handle_button_press(XButtonEvent* ev) {
    switch (ev->button) {
    case Button1: g_mouse_down[0] = true; break;  // Left
    case Button2: g_mouse_down[2] = true; break;  // Middle
    case Button3: g_mouse_down[1] = true; break;  // Right
    case Button4: g_scroll_dy += 1.0f; break;     // Scroll up
    case Button5: g_scroll_dy -= 1.0f; break;     // Scroll down
    case Button6: g_scroll_dx -= 1.0f; break;     // Scroll left
    case Button7: g_scroll_dx += 1.0f; break;     // Scroll right
    default: break;
    }
}

static void x11_handle_button_release(XButtonEvent* ev) {
    switch (ev->button) {
    case Button1: g_mouse_down[0] = false; break;
    case Button2: g_mouse_down[2] = false; break;
    case Button3: g_mouse_down[1] = false; break;
    // Button4-7 (scroll) are press-and-release, no sticky state
    default: break;
    }
}

static void x11_handle_key_press(XKeyEvent* ev) {
    KeySym ks = XLookupKeysym(ev, 0);
    int kc = x11_keysym_to_kain(ks);
    if (kc > 0 && kc < 256) g_keys[kc] = true;

    // Text input via XLookupString (ASCII)
    char buf[8] = {0};
    int len = XLookupString(ev, buf, (int)sizeof(buf), &ks, NULL);
    if (len > 0 && g_text_len + len < X11_TEXT_BUF_SIZE) {
        for (int i = 0; i < len; i++) {
            if (buf[i] >= 32) {  // Skip control characters
                g_text_buf[g_text_len++] = buf[i];
            }
        }
        g_text_buf[g_text_len] = '\0';
    }
}

static void x11_handle_key_release(XKeyEvent* ev) {
    KeySym ks = XLookupKeysym(ev, 0);
    int kc = x11_keysym_to_kain(ks);
    if (kc > 0 && kc < 256) g_keys[kc] = false;
}

static void x11_handle_resize(int w, int h) {
    if (w == g_win_width && h == g_win_height)
        return;

    g_win_width  = w;
    g_win_height = h;

    // Recreate framebuffer at new size
    int fb_w = (int)((float)w * g_dpi_scale_x);
    int fb_h = (int)((float)h * g_dpi_scale_y);

    x11_fb_destroy();
    x11_fb_create(fb_w, fb_h);
}

// ============================================================================
//  CLIPBOARD (simplified — static buffer)
//
//  P0: Store clipboard in a static buffer. Set selection ownership on copy.
//  On paste, trigger XConvertSelection and handle SelectionNotify/Request.
// ============================================================================

static void x11_clipboard_set_text(const char* text) {
    if (!text) return;
    int len = (int)strlen(text);
    if (len >= (int)sizeof(g_clipboard_text))
        len = (int)sizeof(g_clipboard_text) - 1;
    memcpy(g_clipboard_text, text, (size_t)len);
    g_clipboard_text[len] = '\0';
    g_clipboard_text_len = len;

    // Set clipboard selection ownership
    XSetSelectionOwner(g_display, g_clipboard_atom, g_window, CurrentTime);
}

// ============================================================================
//  EVENT PUMP
// ============================================================================

static void x11_handle_event(XEvent* ev) {
    switch (ev->type) {
    case ClientMessage:
        if ((Atom)ev->xclient.data.l[0] == g_wm_delete_window) {
            g_should_close = true;
        }
        break;

    case DestroyNotify:
        g_is_open = false;
        break;

    case ConfigureNotify:
        x11_handle_resize(ev->xconfigure.width, ev->xconfigure.height);
        break;

    case MotionNotify:
        g_mouse_x = (float)ev->xmotion.x;
        g_mouse_y = (float)ev->xmotion.y;
        break;

    case ButtonPress:
        x11_handle_button_press(&ev->xbutton);
        break;

    case ButtonRelease:
        x11_handle_button_release(&ev->xbutton);
        break;

    case KeyPress:
        x11_handle_key_press(&ev->xkey);
        break;

    case KeyRelease:
        x11_handle_key_release(&ev->xkey);
        break;

    case FocusIn:
        g_focus_gained = true;
        break;

    case FocusOut:
        g_focus_gained = false;
        memset(g_keys, 0, sizeof(g_keys));
        break;

    case Expose:
        // Repaint requested — handled by next kt_present()
        break;

    case SelectionRequest: {
        XSelectionRequestEvent* req = &ev->xselectionrequest;
        XEvent respond = {0};
        respond.xselection.type      = SelectionNotify;
        respond.xselection.display   = req->display;
        respond.xselection.requestor = req->requestor;
        respond.xselection.selection = req->selection;
        respond.xselection.target    = req->target;
        respond.xselection.property  = req->property;
        respond.xselection.time      = req->time;

        // Provide clipboard data
        if (req->target == g_utf8_string_atom || req->target == XA_STRING) {
            Atom actual_target = (req->target == g_utf8_string_atom)
                                 ? g_utf8_string_atom : XA_STRING;
            if (g_clipboard_text_len > 0) {
                XChangeProperty(g_display, req->requestor, req->property,
                                actual_target, 8, PropModeReplace,
                                (unsigned char*)g_clipboard_text,
                                g_clipboard_text_len);
            } else {
                respond.xselection.property = None;
            }
        } else if (req->target == g_clipboard_targets) {
            // Provide list of supported targets
            Atom targets[2] = { XA_STRING, g_utf8_string_atom };
            XChangeProperty(g_display, req->requestor, req->property,
                            XA_ATOM, 32, PropModeReplace,
                            (unsigned char*)targets, 2);
        } else {
            respond.xselection.property = None;
        }

        XSendEvent(g_display, req->requestor, False, NoEventMask, &respond);
        XFlush(g_display);
        break;
    }

    case SelectionNotify: {
        // Clipboard data received — read from property
        if (ev->xselection.property != None) {
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char* prop_data = NULL;
            if (XGetWindowProperty(g_display, ev->xselection.requestor,
                                   ev->xselection.property, 0, 4096, True,
                                   AnyPropertyType, &actual_type,
                                   &actual_format, &nitems, &bytes_after,
                                   &prop_data) == Success) {
                if (prop_data && nitems > 0) {
                    int copy_len = (int)nitems;
                    if (copy_len >= (int)sizeof(g_clipboard_text))
                        copy_len = (int)sizeof(g_clipboard_text) - 1;
                    memcpy(g_clipboard_text, prop_data, (size_t)copy_len);
                    g_clipboard_text[copy_len] = '\0';
                    g_clipboard_text_len = copy_len;
                    XFree(prop_data);
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

static void x11_pump_events(void) {
    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);
        x11_handle_event(&ev);
    }
}

// ============================================================================
//  PERFORMANCE TIMER (POSIX clock_gettime)
// ============================================================================

static void x11_timer_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_last_time);
    g_delta_seconds = 0.016;
}

static void x11_timer_tick(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double dt = (double)(now.tv_sec - g_last_time.tv_sec)
              + (double)(now.tv_nsec - g_last_time.tv_nsec) / 1.0e9;

    // Clamp delta to [0.001, 0.1] seconds to prevent spiral-of-death
    if (dt < 0.001) dt = 0.001;
    if (dt > 0.1)   dt = 0.016;

    g_delta_seconds = dt;
    g_last_time = now;
}

// ============================================================================
//  DISPLAY & WINDOW CREATION
// ============================================================================

static int x11_open_display(void) {
    g_display = XOpenDisplay(NULL);  // Uses DISPLAY env var
    if (!g_display) return -1;
    g_screen = DefaultScreen(g_display);
    return 0;
}

static int x11_create_window(const KaintanaBackendConfig* config) {
    Window root = RootWindow(g_display, g_screen);
    int w = (config->width  > 0) ? config->width  : X11_DEFAULT_WIDTH;
    int h = (config->height > 0) ? config->height : X11_DEFAULT_HEIGHT;

    XSetWindowAttributes attrs = {0};
    attrs.event_mask = ExposureMask | StructureNotifyMask
                     | ButtonPressMask | ButtonReleaseMask
                     | PointerMotionMask | KeyPressMask | KeyReleaseMask
                     | FocusChangeMask | EnterWindowMask | LeaveWindowMask;
    attrs.background_pixel = 0;  // Black background, no flash

    g_window = XCreateWindow(g_display, root,
                             0, 0, (unsigned int)w, (unsigned int)h, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWEventMask | CWBackPixel, &attrs);

    // Set title
    const char* title = config->title ? config->title : "Kaintana";
    XStoreName(g_display, g_window, title);

    // Set _NET_WM_NAME for modern WMs (UTF-8)
    g_net_wm_name = XInternAtom(g_display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(g_display, "UTF8_STRING", False);
    XChangeProperty(g_display, g_window, g_net_wm_name,
                    utf8_string, 8,
                    PropModeReplace, (unsigned char*)title, (int)strlen(title));

    // WM_DELETE_WINDOW protocol (close button)
    g_wm_delete_window = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete_window, 1);

    // Clipboard atoms
    g_clipboard_atom   = XInternAtom(g_display, "CLIPBOARD", False);
    g_clipboard_primary= XInternAtom(g_display, "PRIMARY", False);
    g_clipboard_targets= XInternAtom(g_display, "TARGETS", False);
    g_utf8_string_atom = utf8_string;

    // Create GC
    g_gc = XCreateGC(g_display, g_window, 0, NULL);

    // Show window
    XMapWindow(g_display, g_window);
    XFlush(g_display);

    g_win_width  = w;
    g_win_height = h;
    g_is_open    = true;

    return 0;
}

// ============================================================================
//  BACKEND LIFECYCLE — The 4-function KaintanaBackendVTable contract
// ============================================================================

static int x11_init(const KaintanaBackendConfig* config) {
    if (!config) return -1;

    // Store session pointer from config (set by kt_backend_select)
    g_x11_session = (kt_Session*)config->platform_handle;

    // Open X11 display
    if (x11_open_display() != 0) return -1;

    // Detect DPI
    x11_detect_dpi();

    // Report DPI to core
    if (g_x11_session) {
        kt_set_native_scale(g_x11_session, g_dpi_scale_x, g_dpi_scale_y);
    }

    // Create window
    if (x11_create_window(config) != 0) {
        XCloseDisplay(g_display);
        g_display = NULL;
        return -1;
    }

    // Create framebuffer at DPI-scaled size
    int fb_w = (int)((float)g_win_width  * g_dpi_scale_x);
    int fb_h = (int)((float)g_win_height * g_dpi_scale_y);
    if (x11_fb_create(fb_w, fb_h) != 0) {
        XDestroyWindow(g_display, g_window);
        XFreeGC(g_display, g_gc);
        XCloseDisplay(g_display);
        g_display = NULL;
        g_window  = 0;
        g_gc      = NULL;
        return -1;
    }

    // Initialize timer
    x11_timer_init();

    return 0;
}

static void x11_shutdown(void) {
    // Destroy framebuffer
    x11_fb_destroy();

    // Free X11 resources
    if (g_gc) {
        XFreeGC(g_display, g_gc);
        g_gc = NULL;
    }

    if (g_window) {
        XDestroyWindow(g_display, g_window);
        g_window = 0;
    }

    if (g_display) {
        XCloseDisplay(g_display);
        g_display = NULL;
    }

    g_is_open      = false;
    g_should_close = false;
    g_x11_session  = NULL;
    g_clip_depth   = -1;
}

static void x11_new_frame(void) {
    if (!g_is_open || !g_display) return;

    // Pump all pending X events
    x11_pump_events();

    // Update delta time
    x11_timer_tick();

    // Bridge input to session
    if (g_x11_session) {
        kt_input_mouse_move(g_x11_session, g_mouse_x, g_mouse_y);

        for (int b = 0; b < 5; b++) {
            if (g_mouse_down[b])
                kt_input_mouse_down(g_x11_session, b);
            else
                kt_input_mouse_up(g_x11_session, b);
        }

        if (g_scroll_dx != 0.0f || g_scroll_dy != 0.0f)
            kt_input_scroll(g_x11_session, g_scroll_dx, g_scroll_dy);

        for (int k = 0; k < 256; k++) {
            if (g_keys[k])
                kt_input_key_down(g_x11_session, k);
            else
                kt_input_key_up(g_x11_session, k);
        }

        if (g_text_len > 0) {
            kt_input_text(g_x11_session, g_text_buf);
        }
    }

    // Reset per-frame input accumulators
    g_scroll_dx = 0.0f;
    g_scroll_dy = 0.0f;
    g_text_len  = 0;

    // Clear framebuffer (already done by null_new_frame pattern)
    if (g_pixels) {
        memset(g_pixels, 0, (size_t)(g_fb_width * g_fb_height) * 4);
    }

    // Reset clip stack
    g_clip_depth = -1;
}

static void x11_render(const kt_DrawData* draw_data) {
    if (!g_pixels || !g_ximage) return;
    if (!draw_data || !draw_data->cmds || draw_data->cmd_count <= 0) return;

    for (int i = 0; i < draw_data->cmd_count; i++) {
        const kt_Cmd* cmd = &draw_data->cmds[i];

        switch (cmd->type) {
        case KT_CMD_FILL:
            if (cmd->radius > 0.5f)
                x11_fill_rounded_rect(cmd->bounds, cmd->radius, cmd->color);
            else
                x11_fill_rect(cmd->bounds, cmd->color);
            break;

        case KT_CMD_STROKE:
            x11_stroke_rect(cmd->bounds, cmd->thickness, cmd->color);
            break;

        case KT_CMD_CLIP:
            x11_clip_push(cmd->bounds);
            break;

        case KT_CMD_UNCLIP:
            x11_clip_pop();
            break;

        case KT_CMD_TEXT:
        case KT_CMD_IMAGE:
            // P0: Silently skipped. Text rendering via XDrawString or Xft
            // will be added in P1. Images deferred to P2.
            break;
        }
    }

    // Present to screen
    x11_present_to_screen();
}

// ============================================================================
//  BACKEND VTABLE SINGLETON
//
//  Register with the Kaintana session at startup:
//    extern const KaintanaBackendVTable kaintana_x11_backend;
//    kt_backend_register(s, "x11", &kaintana_x11_backend);
//    kt_backend_select(s, "x11");
// ============================================================================

const KaintanaBackendVTable kaintana_x11_backend = {
    .init       = x11_init,
    .shutdown   = x11_shutdown,
    .new_frame  = x11_new_frame,
    .render     = x11_render
};
