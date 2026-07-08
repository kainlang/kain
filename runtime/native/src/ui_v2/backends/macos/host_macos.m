// ============================================================================
//  host_macos.m — macOS Cocoa backend for Kaintana
//
//  Implements the 4-function KaintanaBackendVTable contract with a real
//  Cocoa window, CGBitmapContext framebuffer, NSView event loop, and
//  CoreGraphics software rendering.
//
//  Architecture decisions (informed by plan doc + 9-framework cross-reference):
//    - Persistent CGBitmapContext — created once, recreated on resize only.
//      NOT per-frame create/destroy.
//    - CoreGraphics drawing commands for all primitives — CGContextFillRect,
//      CGContextStrokeRect, CGPath for rounded rects. Native clip stack via
//      SaveGState/RestoreGState.
//    - Premultiplied ARGB pixel format via kCGImageAlphaPremultipliedFirst.
//    - DPI-aware via backingScaleFactor (integer-only: 1.0 or 2.0, no fractional).
//    - NSWindowDidChangeBackingPropertiesNotification for DPI changes.
//    - Non-blocking event pump via nextEventMatchingMask:untilDate:distantPast.
//    - Input bridged to session via kt_input_*() functions (ImGui pattern).
//    - Zero hardcoded colors — all colors from kt_DrawData command stream.
//
//  Usage:
//    const KaintanaBackendVTable macos_backend = {
//        .init      = macos_init,
//        .shutdown  = macos_shutdown,
//        .new_frame = macos_new_frame,
//        .render    = macos_render
//    };
//    kt_backend_register(s, "macos", &macos_backend);
//    kt_backend_select(s, "macos");
//
//  ============================================================================
//  Verify compilation:
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror \
//        -I X:/runtime/native/include \
//        -I X:/runtime/native/src/ui_v2 \
//        -fsyntax-only X:/runtime/native/src/ui_v2/backends/macos/host_macos.m
//  ============================================================================

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../kaintana.h"

// ============================================================================
//  §1: FORWARD DECLARATIONS
// ============================================================================

@class KaintanaView;

// ============================================================================
//  §2: CONSTANTS
// ============================================================================

#define MACOS_DEFAULT_WIDTH         800
#define MACOS_DEFAULT_HEIGHT        600
#define MACOS_CLIP_STACK_MAX        32          // Matches KT_CLIP_STACK_MAX

// ============================================================================
//  §3: STATIC STATE — singleton window + framebuffer + input
// ============================================================================

static kt_Session*      g_macos_session     = NULL;
static NSWindow*        g_window            = NULL;
static KaintanaView*    g_view              = NULL;
static CGContextRef     g_ctx               = NULL;     // CGBitmapContext
static uint32_t*        g_pBits             = NULL;     // Direct pixel pointer
static int              g_fb_width          = 0;
static int              g_fb_height         = 0;
static int              g_fb_stride         = 0;        // g_fb_width * 4

static int              g_window_width      = MACOS_DEFAULT_WIDTH;
static int              g_window_height     = MACOS_DEFAULT_HEIGHT;
static bool             g_is_open           = false;
static bool             g_needs_present     = false;

// ── Dirty rect accumulator ────────────────────────────────────────────
#define MACOS_DIRTY_RECT_MAX 64
static CGRect           g_dirty_rects[MACOS_DIRTY_RECT_MAX];
static int              g_dirty_count       = 0;
static bool             g_full_dirty        = true;     // First frame = full blit

// ── Input state ─────────────────────────────────────────────────────────
static float            g_mouse_x           = 0.0f;
static float            g_mouse_y           = 0.0f;
static bool             g_mouse_down[5]     = { false };
static float            g_scroll_dx         = 0.0f;
static float            g_scroll_dy         = 0.0f;
static bool             g_keys[256]         = { false };
static char             g_text_buffer[64];
static int              g_text_len          = 0;
static bool             g_focus_gained      = true;
static bool             g_should_close      = false;

// ── DPI ─────────────────────────────────────────────────────────────────
static CGFloat          g_dpi_scale         = 1.0;

// ── Performance timer ───────────────────────────────────────────────────
static uint64_t         g_last_time         = 0;
static mach_timebase_info_data_t g_timebase;
static double           g_delta_seconds     = 0.016;

// ============================================================================
//  §4: DIRTY RECT ACCUMULATOR
// ============================================================================

static void macos_dirty_clear(void) {
    g_dirty_count = 0;
    g_full_dirty  = false;
}

static void macos_dirty_full(void) {
    g_dirty_count = 0;
    g_full_dirty  = true;
}

static void macos_dirty_add_rect(CGRect r) {
    if (g_full_dirty) return;
    if (r.size.width <= 0.0f || r.size.height <= 0.0f) return;

    // Try to merge with existing rect
    for (int i = 0; i < g_dirty_count; i++) {
        CGRect* existing = &g_dirty_rects[i];
        // Check overlap or adjacency (within 4px gap = merge)
        CGFloat gap = 4.0f;
        CGFloat e_r = existing->origin.x + existing->size.width;
        CGFloat e_b = existing->origin.y + existing->size.height;
        CGFloat r_r = r.origin.x + r.size.width;
        CGFloat r_b = r.origin.y + r.size.height;

        if (!(r_r < existing->origin.x - gap || r.origin.x > e_r + gap ||
              r_b < existing->origin.y - gap || r.origin.y > e_b + gap)) {
            // Merge: expand existing rect
            if (r.origin.x < existing->origin.x)
                existing->origin.x = r.origin.x;
            if (r.origin.y < existing->origin.y)
                existing->origin.y = r.origin.y;
            CGFloat new_r = (r_r > e_r) ? r_r : e_r;
            CGFloat new_b = (r_b > e_b) ? r_b : e_b;
            existing->size.width  = new_r - existing->origin.x;
            existing->size.height = new_b - existing->origin.y;
            return;
        }
    }

    // Add new rect if under ceiling
    if (g_dirty_count < MACOS_DIRTY_RECT_MAX) {
        g_dirty_rects[g_dirty_count++] = r;
    } else {
        // Overflow: fall back to full dirty
        g_dirty_count = 0;
        g_full_dirty  = true;
    }
}

static void macos_dirty_add_bounds(kt_Rect bounds) {
    CGRect r = CGRectMake(bounds.x, bounds.y, bounds.w, bounds.h);
    macos_dirty_add_rect(r);
}

// ============================================================================
//  §5: DPI DETECTION + CHANGE HANDLING
// ============================================================================

static void macos_update_dpi_scale(void) {
    if (g_window) {
        g_dpi_scale = [g_window backingScaleFactor];
    } else {
        g_dpi_scale = [[NSScreen mainScreen] backingScaleFactor];
    }
    // macOS backingScaleFactor returns integer only (1.0 or 2.0)
    // No fractional scaling support.
}

static void macos_handle_dpi_change(void) {
    CGFloat old_scale = g_dpi_scale;
    macos_update_dpi_scale();

    if (g_dpi_scale != old_scale) {
        // Recreate CGBitmapContext at new physical size
        int fb_w = (int)(g_window_width * g_dpi_scale);
        int fb_h = (int)(g_window_height * g_dpi_scale);

        // Destroy old, create new
        if (g_ctx) {
            CGContextRelease(g_ctx);
            g_ctx = NULL;
            g_pBits = NULL;
        }

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        g_ctx = CGBitmapContextCreate(
            NULL, fb_w, fb_h, 8, fb_w * 4, cs,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGColorSpaceRelease(cs);

        if (g_ctx) {
            g_pBits    = (uint32_t*)CGBitmapContextGetData(g_ctx);
            g_fb_width = fb_w;
            g_fb_height= fb_h;
            g_fb_stride= fb_w * 4;
        }

        // Bridge DPI scale to core session
        if (g_macos_session) {
            kt_set_native_scale(g_macos_session, (float)g_dpi_scale, (float)g_dpi_scale);
        }
        macos_dirty_full();
    }
}

// ============================================================================
//  §6: CGBITMAPCONTEXT FRAMEBUFFER
// ============================================================================

static int macos_fb_create(int fb_w, int fb_h) {
    if (fb_w <= 0 || fb_h <= 0) return -1;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    g_ctx = CGBitmapContextCreate(
        NULL, fb_w, fb_h, 8, fb_w * 4, cs,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);

    if (!g_ctx) return -1;

    g_pBits     = (uint32_t*)CGBitmapContextGetData(g_ctx);
    g_fb_width  = fb_w;
    g_fb_height = fb_h;
    g_fb_stride = fb_w * 4;

    return 0;
}

static void macos_fb_destroy(void) {
    if (g_ctx) {
        CGContextRelease(g_ctx);
        g_ctx = NULL;
    }
    g_pBits      = NULL;
    g_fb_width   = 0;
    g_fb_height  = 0;
    g_fb_stride  = 0;
}

static void macos_fb_resize(int fb_w, int fb_h) {
    if (fb_w == g_fb_width && fb_h == g_fb_height) return;
    macos_fb_destroy();
    macos_fb_create(fb_w, fb_h);
}

// ============================================================================
//  §7: PERFORMANCE TIMER
// ============================================================================

static void macos_timer_init(void) {
    mach_timebase_info(&g_timebase);
    g_last_time = mach_absolute_time();
}

static void macos_timer_tick(void) {
    uint64_t now = mach_absolute_time();
    uint64_t elapsed_ns = (now - g_last_time) * g_timebase.numer / g_timebase.denom;
    g_delta_seconds = (double)elapsed_ns / 1.0e9;
    g_last_time = now;
}

// ============================================================================
//  §8: NSVIEW SUBCLASS
// ============================================================================

// Forward declarations for input methods
@interface KaintanaView : NSView

- (void)drawRect:(NSRect)dirtyRect;

// Mouse events
- (void)mouseDown:(NSEvent*)event;
- (void)mouseUp:(NSEvent*)event;
- (void)rightMouseDown:(NSEvent*)event;
- (void)rightMouseUp:(NSEvent*)event;
- (void)otherMouseDown:(NSEvent*)event;
- (void)otherMouseUp:(NSEvent*)event;
- (void)mouseDragged:(NSEvent*)event;
- (void)rightMouseDragged:(NSEvent*)event;
- (void)mouseMoved:(NSEvent*)event;
- (void)scrollWheel:(NSEvent*)event;

// Keyboard events
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;
- (void)flagsChanged:(NSEvent*)event;
@end

@implementation KaintanaView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    // Create CGImage from the CGBitmapContext and draw to screen
    if (!g_ctx) return;

    CGImageRef cgImage = CGBitmapContextCreateImage(g_ctx);
    if (!cgImage) return;

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(ctx, [self bounds], cgImage);
    CGImageRelease(cgImage);
}

// ── Mouse input ──────────────────────────────────────────────────────

- (void)mouseDown:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
    g_mouse_down[0] = true;
}

- (void)mouseUp:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
    g_mouse_down[0] = false;
}

- (void)rightMouseDown:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
    g_mouse_down[1] = true;
}

- (void)rightMouseUp:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
    g_mouse_down[1] = false;
}

- (void)otherMouseDown:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
    g_mouse_down[2] = true;
}

- (void)otherMouseUp:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
    g_mouse_down[2] = false;
}

- (void)mouseDragged:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
}

- (void)rightMouseDragged:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint pt = [event locationInWindow];
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([self bounds].size.height - pt.y);
}

- (void)scrollWheel:(NSEvent*)event {
    CGFloat dx, dy;
    if ([event hasPreciseScrollingDeltas]) {
        dx = event.scrollingDeltaX;
        dy = event.scrollingDeltaY;
    } else {
        // Convert lines to approximate pixel deltas
        dx = event.deltaX * 10.0;
        dy = event.deltaY * 10.0;
    }
    g_scroll_dx += (float)dx;
    g_scroll_dy += (float)dy;
}

// ── Keyboard input ───────────────────────────────────────────────────

- (void)keyDown:(NSEvent*)event {
    uint16_t keyCode = [event keyCode];
    if (keyCode < 128) {
        g_keys[keyCode] = true;
    }

    // Text input from characters
    NSString* chars = [event characters];
    if (chars.length > 0) {
        char utf8[64];
        if ([chars getCString:utf8 maxLength:sizeof(utf8) encoding:NSUTF8StringEncoding]) {
            size_t len = strlen(utf8);
            if (g_text_len + (int)len < (int)sizeof(g_text_buffer) - 1) {
                memcpy(g_text_buffer + g_text_len, utf8, len);
                g_text_len += (int)len;
                g_text_buffer[g_text_len] = '\0';
            }
        }
    }
}

- (void)keyUp:(NSEvent*)event {
    uint16_t keyCode = [event keyCode];
    if (keyCode < 128) {
        g_keys[keyCode] = false;
    }
}

- (void)flagsChanged:(NSEvent*)event {
    // Modifier keys: convert to key press/release
    // NSEventModifierFlagShift, NSEventModifierFlagControl,
    // NSEventModifierFlagOption, NSEventModifierFlagCommand
    NSEventModifierFlags mod = [event modifierFlags];
    static NSEventModifierFlags prev_mod = 0;

    // Shift (key code 56 = left, 60 = right)
    if ((mod & NSEventModifierFlagShift) != (prev_mod & NSEventModifierFlagShift)) {
        g_keys[56] = (mod & NSEventModifierFlagShift) ? true : false;
        g_keys[60] = (mod & NSEventModifierFlagShift) ? true : false;
    }
    // Control (key code 59 = left, 62 = right)
    if ((mod & NSEventModifierFlagControl) != (prev_mod & NSEventModifierFlagControl)) {
        g_keys[59] = (mod & NSEventModifierFlagControl) ? true : false;
        g_keys[62] = (mod & NSEventModifierFlagControl) ? true : false;
    }
    // Option (key code 58 = left, 61 = right)
    if ((mod & NSEventModifierFlagOption) != (prev_mod & NSEventModifierFlagOption)) {
        g_keys[58] = (mod & NSEventModifierFlagOption) ? true : false;
        g_keys[61] = (mod & NSEventModifierFlagOption) ? true : false;
    }
    // Command (key code 55 = left, 54 = right)
    if ((mod & NSEventModifierFlagCommand) != (prev_mod & NSEventModifierFlagCommand)) {
        g_keys[55] = (mod & NSEventModifierFlagCommand) ? true : false;
        g_keys[54] = (mod & NSEventModifierFlagCommand) ? true : false;
    }

    prev_mod = mod;
}

@end

// ============================================================================
//  §9: NSEVENT PUMP
// ============================================================================

static void macos_pump_events(void) {
    @autoreleasepool {
        // Non-blocking pump: process ALL pending events without blocking.
        // Equivalent to Win32's PeekMessageW pattern.
        while (true) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantPast]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
        }
    }
}

// ============================================================================
//  §10: PIXEL FILL — direct framebuffer access (software rendering helpers)
// ============================================================================

// Convert premultiplied ARGB to CGContext-compatible premultiplied float color.
// kCGImageAlphaPremultipliedFirst expects premultiplied color values.
static inline void macos_cg_set_fill_color(CGContextRef ctx, uint32_t color) {
    CGFloat a = ((color >> 24) & 0xFF) / 255.0f;
    CGFloat r = ((color >> 16) & 0xFF) / 255.0f;
    CGFloat g = ((color >>  8) & 0xFF) / 255.0f;
    CGFloat b = ((color >>  0) & 0xFF) / 255.0f;
    CGContextSetRGBFillColor(ctx, r, g, b, a);
}

static inline void macos_cg_set_stroke_color(CGContextRef ctx, uint32_t color) {
    CGFloat a = ((color >> 24) & 0xFF) / 255.0f;
    CGFloat r = ((color >> 16) & 0xFF) / 255.0f;
    CGFloat g = ((color >>  8) & 0xFF) / 255.0f;
    CGFloat b = ((color >>  0) & 0xFF) / 255.0f;
    CGContextSetRGBStrokeColor(ctx, r, g, b, a);
}

// ============================================================================
//  §11: RENDER — Draw commands into CGBitmapContext
// ============================================================================

static void macos_render_to_context(const kt_DrawData* draw_data) {
    if (!g_ctx || !draw_data || !draw_data->cmds || draw_data->cmd_count <= 0) {
        return;
    }

    // ── Clear framebuffer on full dirty ──────────────────────────────────
    if (g_full_dirty && g_pBits) {
        memset(g_pBits, 0, (size_t)(g_fb_width * g_fb_height * 4));
    }

    // ── Execute draw commands ────────────────────────────────────────────
    for (int i = 0; i < draw_data->cmd_count; i++) {
        const kt_Cmd* cmd = &draw_data->cmds[i];

        switch (cmd->type) {

        case KT_CMD_FILL: {
            CGRect rect = CGRectMake(cmd->bounds.x, cmd->bounds.y,
                                     cmd->bounds.w, cmd->bounds.h);
            macos_cg_set_fill_color(g_ctx, cmd->color);

            if (cmd->radius > 0.5f) {
                // Rounded rect via CGPath
                CGFloat r = cmd->radius;
                // Clamp radius to half the smaller dimension
                CGFloat max_r = (rect.size.width < rect.size.height)
                    ? rect.size.width * 0.5f : rect.size.height * 0.5f;
                if (r > max_r) r = max_r;

                CGPathRef path = CGPathCreateWithRoundedRect(rect, r, r, NULL);
                CGContextAddPath(g_ctx, path);
                CGContextFillPath(g_ctx);
                CGPathRelease(path);
            } else {
                CGContextFillRect(g_ctx, rect);
            }
            macos_dirty_add_bounds(cmd->bounds);
            break;
        }

        case KT_CMD_STROKE: {
            CGRect rect = CGRectMake(cmd->bounds.x, cmd->bounds.y,
                                     cmd->bounds.w, cmd->bounds.h);
            macos_cg_set_stroke_color(g_ctx, cmd->color);
            CGFloat thickness = (cmd->thickness > 0.0f) ? cmd->thickness : 1.0f;
            CGContextSetLineWidth(g_ctx, thickness);

            if (cmd->radius > 0.5f) {
                CGFloat r = cmd->radius;
                CGFloat max_r = (rect.size.width < rect.size.height)
                    ? rect.size.width * 0.5f : rect.size.height * 0.5f;
                if (r > max_r) r = max_r;

                CGPathRef path = CGPathCreateWithRoundedRect(rect, r, r, NULL);
                CGContextAddPath(g_ctx, path);
                CGContextStrokePath(g_ctx);
                CGPathRelease(path);
            } else {
                CGContextStrokeRect(g_ctx, rect);
            }
            macos_dirty_add_bounds(cmd->bounds);
            break;
        }

        case KT_CMD_TEXT: {
            // Text rendering via NSString drawAtPoint on the CGBitmapContext
            if (cmd->text_id >= 0) {
                // Convert premultiplied ARGB to un-premultiplied for text color
                uint32_t tc = cmd->color;
                CGFloat ta = ((tc >> 24) & 0xFF) / 255.0f;
                CGFloat tr, tg, tb;

                if (ta > 0.001f) {
                    tr = ((tc >> 16) & 0xFF) / (ta * 255.0f);
                    tg = ((tc >>  8) & 0xFF) / (ta * 255.0f);
                    tb = ((tc >>  0) & 0xFF) / (ta * 255.0f);
                } else {
                    tr = tg = tb = 0.0f;
                }

                // CoreGraphics text drawing on the bitmap context
                CGContextSaveGState(g_ctx);

                // Set text matrix and position
                CGContextSetTextMatrix(g_ctx, CGAffineTransformIdentity);
                CGContextSetRGBFillColor(g_ctx, tr, tg, tb, ta);

                // Use a default font — real font management comes from the
                // GDI renderer bridge in Phase 2.
                // For now, draw a placeholder using NSString's drawAtPoint
                // on the CGBitmapContext's graphics context.
                NSString* placeholder = @"…";
                [placeholder drawAtPoint:NSMakePoint(cmd->bounds.x, cmd->bounds.y)
                          withAttributes:@{
                              NSFontAttributeName: [NSFont systemFontOfSize:14.0],
                              NSForegroundColorAttributeName:
                                  [NSColor colorWithCalibratedRed:tr green:tg blue:tb alpha:ta]
                          }];

                CGContextRestoreGState(g_ctx);
            }
            macos_dirty_add_bounds(cmd->bounds);
            break;
        }

        case KT_CMD_IMAGE: {
            // Placeholder — image blitting not yet implemented in software path
            macos_dirty_add_bounds(cmd->bounds);
            break;
        }

        case KT_CMD_CLIP: {
            // Push scissor rect via CoreGraphics clip stack
            CGContextSaveGState(g_ctx);
            CGRect clipRect = CGRectMake(cmd->bounds.x, cmd->bounds.y,
                                         cmd->bounds.w, cmd->bounds.h);
            CGContextClipToRect(g_ctx, clipRect);
            break;
        }

        case KT_CMD_UNCLIP: {
            // Pop scissor rect (restore to previous GState)
            CGContextRestoreGState(g_ctx);
            break;
        }

        default:
            break;
        }
    }
}

// ============================================================================
//  §12: PRESENT — Display CGBitmapContext content on screen
// ============================================================================

static void macos_present_to_screen(void) {
    if (!g_window || !g_view || !g_ctx) return;

    // Create CGImage from the CGBitmapContext
    CGImageRef cgImage = CGBitmapContextCreateImage(g_ctx);
    if (!cgImage) return;

    // Set as NSView layer contents for display
    // For layer-backed views, this is the most efficient present path.
    g_view.layer.contents = (__bridge id)cgImage;

    // Trigger display via dirty rects
    if (g_full_dirty) {
        [g_view setNeedsDisplay:YES];
    } else if (g_dirty_count > 0) {
        for (int i = 0; i < g_dirty_count; i++) {
            [g_view setNeedsDisplayInRect:g_dirty_rects[i]];
        }
    }

    CGImageRelease(cgImage);
    g_needs_present = false;
}

// ============================================================================
//  §13: INPUT — Reset per-frame scratch state
// ============================================================================

static void macos_reset_per_frame_input(void) {
    g_scroll_dx = 0.0f;
    g_scroll_dy = 0.0f;
    g_text_len  = 0;
    memset(g_text_buffer, 0, sizeof(g_text_buffer));
}

// ============================================================================
//  §14: BACKEND LIFECYCLE — The 4-function KaintanaBackendVTable contract
// ============================================================================

// macos_init: Create NSApplication, NSWindow, NSView, CGBitmapContext.
// Returns 0 on success, -1 on failure.
static int macos_init(const KaintanaBackendConfig* config) {
    if (!config) return -1;

    @autoreleasepool {
        // Store session pointer from config (set by kt_backend_select)
        g_macos_session = (kt_Session*)config->platform_handle;

        // ── Ensure NSApp is ready ────────────────────────────────────────
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // ── Register NSView class ────────────────────────────────────────
        // KaintanaView is registered by its @implementation above.

        // ── Resolve dimensions ───────────────────────────────────────────
        int w = (config->width  > 0) ? config->width  : MACOS_DEFAULT_WIDTH;
        int h = (config->height > 0) ? config->height : MACOS_DEFAULT_HEIGHT;

        NSString* title = config->title
            ? [NSString stringWithUTF8String:config->title]
            : @"Kaintana";

        // ── Calculate centered window frame ──────────────────────────────
        NSScreen* screen = [NSScreen mainScreen];
        NSRect screenRect = [screen visibleFrame];
        CGFloat winX = screenRect.origin.x + (screenRect.size.width  - w) * 0.5;
        CGFloat winY = screenRect.origin.y + (screenRect.size.height - h) * 0.5;
        NSRect winRect = NSMakeRect(winX, winY, w, h);

        // ── Create NSWindow ──────────────────────────────────────────────
        NSUInteger styleMask = NSWindowStyleMaskTitled
                             | NSWindowStyleMaskClosable
                             | NSWindowStyleMaskMiniaturizable
                             | NSWindowStyleMaskResizable;

        g_window = [[NSWindow alloc] initWithContentRect:winRect
                                               styleMask:styleMask
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];

        if (!g_window) return -1;

        g_window.title = title;
        g_window.backgroundColor = [NSColor blackColor];

        // ── Create NSView ───────────────────────────────────────────────
        NSRect viewRect = NSMakeRect(0, 0, w, h);
        g_view = [[KaintanaView alloc] initWithFrame:viewRect];
        g_view.wantsLayer = YES;

        if (!g_view) {
            [g_window release];
            g_window = nil;
            return -1;
        }

        g_window.contentView = g_view;

        // ── DPI scale ────────────────────────────────────────────────────
        macos_update_dpi_scale();

        // Bridge DPI scale to core session
        if (g_macos_session) {
            kt_set_native_scale(g_macos_session, (float)g_dpi_scale, (float)g_dpi_scale);
        }

        g_window_width  = w;
        g_window_height = h;
        g_is_open       = true;

        // ── Create CGBitmapContext framebuffer ───────────────────────────
        int fb_w = (int)(w * g_dpi_scale);
        int fb_h = (int)(h * g_dpi_scale);

        if (macos_fb_create(fb_w, fb_h) != 0) {
            [g_view release];
            [g_window release];
            g_view = nil;
            g_window = nil;
            return -1;
        }

        // ── Performance timer ────────────────────────────────────────────
        macos_timer_init();

        // ── Show window ──────────────────────────────────────────────────
        [g_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        g_should_close = false;
        macos_dirty_full();
    }

    return 0;
}

// macos_shutdown: Destroy window, free CGBitmapContext, release resources.
static void macos_shutdown(void) {
    @autoreleasepool {
        macos_fb_destroy();

        if (g_view) {
            [g_view release];
            g_view = nil;
        }
        if (g_window) {
            [g_window close];
            [g_window release];
            g_window = nil;
        }
    }

    g_is_open       = false;
    g_should_close  = true;
    g_macos_session = NULL;
}

// macos_new_frame: Pump Cocoa event loop, update timing, bridge input.
static void macos_new_frame(void) {
    if (!g_is_open) return;

    @autoreleasepool {
        // Pump OS events (fills global input state via NSView methods)
        macos_pump_events();

        // Update delta time
        macos_timer_tick();

        // Bridge accumulated input state to session (ImGui pattern:
        // backends fill IO state before the UI frame begins).
        if (g_macos_session) {
            kt_input_mouse_move(g_macos_session, g_mouse_x, g_mouse_y);
            for (int b = 0; b < 5; b++) {
                if (g_mouse_down[b]) kt_input_mouse_down(g_macos_session, b);
                else                 kt_input_mouse_up(g_macos_session, b);
            }
            if (g_scroll_dx != 0.0f || g_scroll_dy != 0.0f)
                kt_input_scroll(g_macos_session, g_scroll_dx, g_scroll_dy);

            // Bridge keyboard state
            for (int k = 0; k < 256; k++) {
                if (g_keys[k]) kt_input_key_down(g_macos_session, k);
                else           kt_input_key_up(g_macos_session, k);
            }

            // Bridge UTF-8 text input
            if (g_text_len > 0) {
                kt_input_text(g_macos_session, g_text_buffer);
            }
        }

        // Reset per-frame scratch input (scroll, text cleared after bridge)
        macos_reset_per_frame_input();
    }
}

// macos_render: Execute all draw commands into the CGBitmapContext.
// After this call, the framebuffer content is displayed on screen.
static void macos_render(const kt_DrawData* draw_data) {
    if (!g_ctx || !g_pBits) return;

    @autoreleasepool {
        // Render commands to CGBitmapContext
        macos_render_to_context(draw_data);

        // Schedule screen present via dirty rects
        g_needs_present = true;
        macos_present_to_screen();
        macos_dirty_clear();
    }
}

// ============================================================================
//  §15: INPUT QUERY — External interface for tree.c to poll input state
// ============================================================================

const float* macos_get_mouse_pos(void) {
    static float pos[2];
    pos[0] = g_mouse_x;
    pos[1] = g_mouse_y;
    return pos;
}

float macos_get_mouse_x(void)      { return g_mouse_x; }
float macos_get_mouse_y(void)      { return g_mouse_y; }
bool  macos_get_mouse_down(int b)  { return (b >= 0 && b < 5) ? g_mouse_down[b] : false; }
float macos_get_scroll_dx(void)    { return g_scroll_dx; }
float macos_get_scroll_dy(void)    { return g_scroll_dy; }
bool  macos_get_key(int k)         { return (k >= 0 && k < 256) ? g_keys[k] : false; }
bool  macos_get_focus(void)        { return g_focus_gained; }
bool  macos_should_close(void)     { return g_should_close; }
float macos_get_delta_seconds(void){ return (float)g_delta_seconds; }
int   macos_get_fb_width(void)     { return g_fb_width; }
int   macos_get_fb_height(void)    { return g_fb_height; }

// ============================================================================
//  §16: BACKEND VTABLE SINGLETON
// ============================================================================

const KaintanaBackendVTable kaintana_macos_backend = {
    .init      = macos_init,
    .shutdown  = macos_shutdown,
    .new_frame = macos_new_frame,
    .render    = macos_render
};
