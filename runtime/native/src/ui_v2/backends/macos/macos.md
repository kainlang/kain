# macOS Cocoa Backend Implementation Plan

**Date:** 2026-06-28
**Status:** Planning — P0 implementation not started
**Cross-Reference:** `MASTER_OS_AND_CONTRACT.md`, `MASTER_PLATFORM.md`, `MASTER_RENDERER.md`, `MASTER_DPI_AND_SCALING.md` §3.2, `MASTER_CONTRACT.md`
**Reference Backend:** `backends/win32/host_win32.c` (38.6 KB, 1,010 lines)
**Existing Stub:** `backends/macos/host_macos.m` (339 B, 8 lines)
**Build System:** Bazel (`native_core_runtime.toml`), CMake (`backends/CMakeLists.txt`)

---

## 1. Architecture

**File:** `backends/macos/host_macos.m`
**Language:** Objective-C (.m) — required for Cocoa runtime message dispatch. Cannot be compiled as pure C11.
**VTable Contract:** 4-function `KaintanaBackendVTable`
**Rendering Path:** Software renderer via CGBitmapContext → CGImage → NSView drawRect: (Phase 1). CAMetalLayer-backed rendering (Phase 2).

### File Structure (Estimated: 600-800 lines)

```
host_macos.m:
  §1  – Imports + Forward Declarations      (30 lines)
  §2  – Constants + Static State             (40 lines)
  §3  – DPI Detection + Change Handling      (50 lines)
  §4  – Performance Timer                    (20 lines)
  §5  – NSView Subclass                      (80 lines)
  §6  – NSWindow Delegate                    (40 lines)
  §7  – NSApplication Run Loop Bridge        (30 lines)
  §8  – Input Handling                       (120 lines)
  §9  – CGBitmapContext Framebuffer           (60 lines)
  §10 – Render (software + Metal)            (70 lines)
  §11 – Backend Lifecycle (the 4 functions)  (80 lines)
  §12 – Backend VTable Singleton              (10 lines)
```

### Dependency Graph

```
kaintana.h (24-slot vtable, types)
    └── internal.h (KaintanaSession, KaintanaNode)
         └── host_macos.m
              ├── Cocoa/AppKit.framework
              └── CoreGraphics.framework
```

### Integration Points (from `contract.tsv`)

| ID | Integration Point | Priority | Status |
|----|------------------|----------|--------|
| P0-4 | Software framebuffer access (CGBitmapContext) | P0 | Not started |
| P0-6 | Platform detection via `kain_platform_current_kind()` | P0 | Not started |
| P0-8 | Input event query (abi_input_push_event) | P0 | Not started |
| P0-11 | Diagnostics via `KAIN_DIAG_SUBSYSTEM_UI` | P0 | Not started |
| P1-26 | Service availability gates | P1 | Not started |
| P1-32 | ABI version check at startup | P1 | Not started |
| BACKEND_DPI-19 | macOS DPI (dpi.tsv §BACKEND_DPI row 19) | P0 | Not started |

---

## 2. The 4-Function VTable Contract

```objc
// host_macos.m — the entire public surface
const KaintanaBackendVTable kaintana_macos_backend = {
    .init      = macos_init,
    .shutdown  = macos_shutdown,
    .new_frame = macos_new_frame,
    .render    = macos_render
};
```

### 2.1 `macos_init` (Estimated: 80 lines)

**Purpose:** Create NSApplication, NSWindow, NSView, initialize DPI, create framebuffer.

```
Sequence:
  1. Store session pointer from config->platform_handle → g_macos_session
  2. Create autorelease pool
  3. Ensure NSApp is ready ([NSApplication sharedApplication])
  4. Set app activation policy (NSApplicationActivationPolicyRegular)
  5. Register NSView subclass + NSWindowDelegate class
  6. Calculate window frame (centered, respecting config->width/height)
  7. Create NSWindow with styleMask (NSWindowStyleMaskTitled|Closable|Miniaturizable|Resizable)
     - backingStoreType: NSBackingStoreBuffered
     - defer: NO
  8. Create NSView subclass instance, set as contentView
  9. Query backingScaleFactor
  10. Create CGBitmapContext at physical pixel size (width × scale, height × scale)
  11. Call kt_set_native_scale(g_macos_session, scale, scale)
  12. Show window, make key and order front
  13. Initialize performance timer (mach_absolute_time)
  14. Return 0 on success, -1 on failure
```

**Config handling:**
```objc
int w = (config->width  > 0) ? config->width  : WIN32_DEFAULT_WIDTH;
int h = (config->height > 0) ? config->height : WIN32_DEFAULT_HEIGHT;
NSString* title = config->title
    ? [NSString stringWithUTF8String:config->title]
    : @"Kaintana";
```

**Framebuffer creation:**
```objc
CGFloat scale = [NSScreen mainScreen].backingScaleFactor;
int fb_w = (int)(w * scale);
int fb_h = (int)(h * scale);

CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
CGContextRef ctx = CGBitmapContextCreate(
    NULL, fb_w, fb_h, 8, fb_w * 4,
    colorspace,
    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
);
// pixels = CGBitmapContextGetData(ctx) — uint32_t* ARGB
```

### 2.2 `macos_shutdown` (Estimated: 25 lines)

```
Sequence:
  1. Release CGBitmapContext (CFRelease)
  2. Release colorspace
  3. Close NSWindow
  4. Release NSView, NSWindow
  5. Drain autorelease pool
  6. Clear static state globals
```

### 2.3 `macos_new_frame` (Estimated: 60 lines)

**Purpose:** Pump Cocoa event loop, update timing, bridge input to session.

```
Sequence:
  1. if (!g_is_open) return
  2. Drain autorelease pool
  3. Pump NSApp events: 
     while (NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES]) {
         [NSApp sendEvent:event];
     }
  4. Update timer (mach_absolute_time → delta_seconds)
  5. Reset per-frame scratch input (scroll deltas, text buffer)
  6. Bridge accumulated input to session:
     - kt_input_mouse_move(g_macos_session, g_mouse_x, g_mouse_y)
     - for each button: kt_input_mouse_down/up
     - if scroll: kt_input_scroll
     - for each key: kt_input_key_down/up
     - if text: kt_input_text (UTF-8)
```

**Event loop difference from Win32:**
Win32 uses `PeekMessageW` which is non-blocking. macOS uses `nextEventMatchingMask:untilDate:` with `[NSDate distantPast]` to achieve the same non-blocking behavior. For blocking event loops (standalone apps), use `[NSApp run]` with `[NSApp stop:nil]` for quit.

### 2.4 `macos_render` (Estimated: 70 lines)

**Purpose:** Execute draw commands into CGBitmapContext, present to screen.

```
Sequence:
  1. if empty cmd list → return (preserve framebuffer)
  2. if full dirty → clear entire CGBitmapContext
  3. For each cmd in draw_data:
     - KT_CMD_FILL: CGContextSetFillColor + CGContextFillRect / CGContextFillPath (rounded)
     - KT_CMD_STROKE: CGContextStrokeRect with line width
     - KT_CMD_TEXT: NSString drawAtPoint:withAttributes: (NSFont + NSForegroundColorAttributeName)
     - KT_CMD_CLIP: CGContextSaveGState + CGContextClipToRect
     - KT_CMD_UNCLIP: CGContextRestoreGState
     - KT_CMD_IMAGE: CGContextDrawImage (from cached CGImageRef)
  4. Create CGImage from CGBitmapContext:
     CGImageRef cgImage = CGBitmapContextCreateImage(g_ctx);
  5. Set image on NSView's layer contents:
     [g_view setNeedsDisplay:YES];  // then in drawRect:
     // or for layer-backed: g_view.layer.contents = (__bridge id)cgImage;
  6. Release CGImage
  7. Mark dirty rects for damage tracking
```

**Phase 2 — CAMetalLayer:**
```objc
// Replace CGBitmapContext with CAMetalLayer
CAMetalLayer* layer = [CAMetalLayer layer];
layer.device = MTLCreateSystemDefaultDevice();
layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
layer.drawableSize = CGSizeMake(fb_w, fb_h);
g_view.layer = layer;

// Render via Metal command buffer
id<CAMetalDrawable> drawable = [layer nextDrawable];
// ... render kt_DrawData into drawable.texture via compute shader ...
[commandBuffer presentDrawable:drawable];
[commandBuffer commit];
```

---

## 3. DPI Detection

### 3.1 Scale Factor Query

```objc
// macOS §3.2: backingScaleFactor returns integer only (1.0 or 2.0)
CGFloat scale = [[NSScreen mainScreen] backingScaleFactor];
// Per-window variant:
CGFloat win_scale = [g_window backingScaleFactor];
// Both return 1.0 on standard displays, 2.0 on Retina
// No fractional scaling (no 1.25x, 1.5x, 1.75x support)
```

### 3.2 DPI Change Detection

```objc
// Register for NSWindowDidChangeBackingPropertiesNotification
[[NSNotificationCenter defaultCenter]
    addObserver:self
    selector:@selector(backingPropertiesChanged:)
    name:NSWindowDidChangeBackingPropertiesNotification
    object:g_window];
```

**Handler:**
```objc
- (void)backingPropertiesChanged:(NSNotification*)note {
    NSDictionary* userInfo = [note userInfo];
    NSNumber* oldScale = userInfo[NSBackingPropertyOldScaleFactorKey];
    CGFloat newScale = [g_window backingScaleFactor];
    
    if (oldScale.doubleValue != newScale) {
        // 1. Recreate CGBitmapContext at new physical size
        int fb_w = (int)(g_window_width * newScale);
        int fb_h = (int)(g_window_height * newScale);
        macos_fb_recreate(fb_w, fb_h);
        
        // 2. Bridge DPI scale change to core session
        kt_set_native_scale(g_macos_session, (float)newScale, (float)newScale);
        
        // 3. Mark full dirty for repaint
        g_full_dirty = true;
    }
}
```

### 3.3 Input Coordinate Conversion

```objc
// NSView mouse events give coordinates in logical (point) space
// macOS automatically handles Retina input conversion
// No manual division by scale needed for mouse coordinates

// For explicit conversion:
// NSPoint backingPoint = [g_view convertPointToBacking:viewPoint];
// NSPoint viewPoint = [g_view convertPointFromBacking:backingPoint];
```

### 3.4 Framebuffer Sizing

```objc
CGFloat scale = [g_window backingScaleFactor];  // 1.0 or 2.0
int fb_width  = (int)(g_logical_width  * scale);
int fb_height = (int)(g_logical_height * scale);

// CGBitmapContext at fb_width × fb_height
// Retina: logical 800×600 → framebuffer 1600×1200
// Standard: logical 800×600 → framebuffer 800×600
```

---

## 4. Input Handling

### 4.1 NSView Input Methods

The NSView subclass handles all input events. Methods to override:

| NSView Method | Input Produced | Kaintana Function |
|--------------|----------------|-------------------|
| `mouseDown:` | Mouse press (left) | `kt_input_mouse_down(s, 0)` |
| `mouseUp:` | Mouse release (left) | `kt_input_mouse_up(s, 0)` |
| `rightMouseDown:` | Mouse press (right) | `kt_input_mouse_down(s, 1)` |
| `rightMouseUp:` | Mouse release (right) | `kt_input_mouse_up(s, 1)` |
| `otherMouseDown:` | Mouse press (middle) | `kt_input_mouse_down(s, 2)` |
| `otherMouseUp:` | Mouse release (middle) | `kt_input_mouse_up(s, 2)` |
| `mouseDragged:` | Mouse move (while dragging) | `kt_input_mouse_move(s, x, y)` |
| `rightMouseDragged:` | Right drag | `kt_input_mouse_move(s, x, y)` |
| `mouseMoved:` | Mouse move | `kt_input_mouse_move(s, x, y)` |
| `scrollWheel:` | Scroll delta | `kt_input_scroll(s, dx, dy)` |
| `keyDown:` | Key press + text input | `kt_input_key_down(s, key)` + `kt_input_text(s, chars)` |
| `keyUp:` | Key release | `kt_input_key_up(s, key)` |
| `flagsChanged:` | Modifier key changes | `kt_input_key_down/up(s, modifier)` |

### 4.2 Key Mapping

```objc
// macOS key codes → Kaintana scancodes (0-255)
static const uint8_t macos_key_map[128] = {
    // Map Mac HID key codes to Kaintana virtual key codes
    // 0x00 = 'a', 0x01 = 's', 0x02 = 'd', 0x03 = 'f', ...
    // 0x24 = return, 0x30 = tab, 0x31 = space, 0x33 = backspace
    // 0x35 = escape, 0x36 = cmd, 0x37 = shift, 0x38 = caps, 0x39 = option
    // 0x7B = left arrow, 0x7C = right arrow, 0x7D = down arrow, 0x7E = up arrow
};

- (void)keyDown:(NSEvent*)event {
    uint16_t keyCode = [event keyCode];
    if (keyCode < 128 && macos_key_map[keyCode]) {
        kt_input_key_down(g_macos_session, macos_key_map[keyCode]);
    }
    
    // Text input from characters
    NSString* chars = [event characters];
    if (chars.length > 0) {
        char utf8[32];
        [chars getCString:utf8 maxLength:32 encoding:NSUTF8StringEncoding];
        kt_input_text(g_macos_session, utf8);
    }
}
```

### 4.3 Scroll Wheel Normalization

```objc
- (void)scrollWheel:(NSEvent*)event {
    // macOS provides scroll delta in points (not lines)
    // NSEvent has scrollingDeltaX/Y (precise, continuous scroll)
    // and deltaX/Y (line-based, legacy)
    
    CGFloat dx, dy;
    if ([event hasPreciseScrollingDeltas]) {
        dx = event.scrollingDeltaX;
        dy = event.scrollingDeltaY;
    } else {
        // Convert lines to approximate pixel deltas
        dx = event.deltaX * 10.0;
        dy = event.deltaY * 10.0;
    }
    
    kt_input_scroll(g_macos_session, (float)dx, (float)dy);
}
```

### 4.4 Mouse Coordinate Conversion

```objc
- (void)mouseMoved:(NSEvent*)event {
    // locationInWindow gives coordinates in points (logical)
    NSPoint pt = [event locationInWindow];
    
    // Convert from NSView coordinate system (y up) to screen (y down)
    // Not needed for kt_input_* — hit testing uses session's coordinate system
    g_mouse_x = (float)pt.x;
    g_mouse_y = (float)([g_view bounds].size.height - pt.y);
}
```

### 4.5 Input Coordinate Spaces (from dpi.tsv §SCALE_MODEL)

| Rule | Description | macOS Application |
|------|-------------|-------------------|
| Rule 6 | Input ÷ scale → logical | NOT needed — macOS returns points directly |
| Rule 3 | Per-window scale | `[g_window backingScaleFactor]` |

---

## 5. Message Loop

### 5.1 Non-Blocking Pump (for `macos_new_frame`)

Used when the application owns the event loop and calls `kt_begin`/`kt_end`/`kt_present` manually.

```objc
static void macos_pump_events(void) {
    @autoreleasepool {
        // Process ALL pending events without blocking
        while (true) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantPast]  // ← non-blocking
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
        }
    }
}
```

### 5.2 Blocking Run Loop (for standalone apps)

```objc
// Called once from main() — blocks until quit
static void macos_run_event_loop(void) {
    [NSApp run];  // Blocks until [NSApp stop:nil] is called
}
```

### 5.3 Quit Handling

```objc
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

// Or via NSWindowDelegate:
- (BOOL)windowShouldClose:(NSNotification*)note {
    g_should_close = true;
    return YES;
}
```

---

## 6. Rendering

### 6.1 Software Rendering (Phase 1)

**CGBitmapContext pipeline:**

```
kt_DrawData → CoreGraphics drawing commands → CGBitmapContext
    → CGImage → NSAction layer setContents → screen
```

**Pixel format considerations:**
- CGBitmapContext default pixel layout is ARGB (8-8-8-8)
- bytesPerRow = fb_width × 4 (always 32bpp)
- Kaintana expects premultiplied ARGB throughout
- Use `kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little` (ARGB with alpha premultiplied)

```objc
// Create CGContext for software rendering
CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
g_ctx = CGBitmapContextCreate(
    NULL,                    // auto-allocate pixel data
    fb_w, fb_h,             // physical pixel dimensions
    8,                      // bits per component
    fb_w * 4,               // bytesPerRow
    cs,
    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
);
CGColorSpaceRelease(cs);

// Pixel pointer for direct framebuffer access
g_pBits = CGBitmapContextGetData(g_ctx);
```

### 6.2 CAMetalLayer Rendering (Phase 2)

```objc
// NSView subclass override
+ (Class)layerClass {
    return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        CAMetalLayer* layer = (CAMetalLayer*)self.layer;
        layer.device = MTLCreateSystemDefaultDevice();
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
    }
    return self;
}
```

### 6.3 Dirty Rect Optimization

Adapt the Win32 dirty-rect pattern to NSView:

```objc
// Instead of InvalidateRect, use:
[g_view setNeedsDisplayInRect:NSMakeRect(dx, dy, dw, dh)];

// In drawRect:
- (void)drawRect:(NSRect)dirtyRect {
    CGImageRef cgImage = CGBitmapContextCreateImage(g_ctx);
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(ctx, [g_view bounds], cgImage);
    CGImageRelease(cgImage);
}
```

### 6.4 Clip Stack

CoreGraphics has native clip stack support:

```objc
// KT_CMD_CLIP:
CGContextSaveGState(g_ctx);
CGContextClipToRect(g_ctx, CGRectMake(x, y, w, h));

// KT_CMD_UNCLIP:
CGContextRestoreGState(g_ctx);
```

This is simpler than the Win32 DC SaveDC/RestoreDC pattern since CoreGraphics manages a proper clip stack natively.

---

## 7. Clipboard

### 7.1 Text Copy (Pasteboard)

```objc
// Set clipboard text
static void macos_set_clipboard(const char* text) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSString* str = [NSString stringWithUTF8String:text];
        if (str) [pb setString:str forType:NSPasteboardTypeString];
    }
}

// Get clipboard text (caller must free)
static char* macos_get_clipboard(void) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (str) {
            return strdup([str UTF8String]);
        }
        return NULL;
    }
}
```

### 7.2 IME Support (NSTextInputClient)

For full text input (IME composition, CJK, emoji):
```objc
// NSView subclass must conform to NSTextInputClient protocol
// This enables:
//   - IME candidate window
//   - Marked text (underline during composition)
//   - Reading window (for Japanese/Chinese)
//   - Emoji picker integration
```

---

## 8. Cursor Management

```objc
// NSView cursor rects
- (void)cursorUpdate:(NSEvent*)event {
    // Standard cursor types:
    // NSCursor.arrowCursor — default
    // NSCursor.IBeamCursor — text selection
    // NSCursor.pointingHandCursor — clickable elements
    // NSCursor.resizeLeftRightCursor — horizontal resize
    // NSCursor.resizeUpDownCursor — vertical resize
    // NSCursor.crosshairCursor — precise placement
    [[NSCursor arrowCursor] set];
}
```

---

## 9. Static State

```objc
// Global state (singleton, one window per process)
static kt_Session*     g_macos_session     = NULL;
static NSWindow*       g_window            = NULL;
static KaintanaView*   g_view              = NULL;
static CGContextRef    g_ctx               = NULL;     // CGBitmapContext
static void*           g_pBits             = NULL;     // pixel data pointer
static int             g_fb_width          = 0;
static int             g_fb_height         = 0;
static int             g_window_width      = 800;
static int             g_window_height     = 600;
static float           g_mouse_x           = 0.0f;
static float           g_mouse_y           = 0.0f;
static bool            g_mouse_down[5]     = { false };
static float           g_scroll_dx         = 0.0f;
static float           g_scroll_dy         = 0.0f;
static bool            g_keys[256]         = { false };
static wchar_t         g_text_buffer[32]   = { 0 };
static int             g_text_len          = 0;
static bool            g_focus_gained      = true;
static bool            g_is_open           = false;
static bool            g_should_close      = false;
static bool            g_full_dirty        = true;
static float           g_dpi_scale         = 1.0f;
static double          g_delta_seconds     = 0.016;
static uint64_t        g_last_time         = 0;
static mach_timebase_info_data_t g_timebase;
```

---

## 10. Dependencies

### Required Frameworks
| Framework | Usage | Status |
|-----------|-------|--------|
| `Cocoa.framework` | NSApplication, NSWindow, NSView, NSEvent | Always linked |
| `CoreGraphics.framework` | CGBitmapContext, CGImage, CGContext drawing | Always linked |
| `Metal.framework` | CAMetalLayer (Phase 2) | Optional, weak-link |

### Build Flags (CMake)
```cmake
# Phases 1: Add host_macos.m only
add_library(kaintana_macos_backend OBJECT backends/macos/host_macos.m)
target_link_libraries(kaintana_macos_backend
    "-framework Cocoa"
    "-framework CoreGraphics"
)

# Phase 2: Add Metal support
target_link_libraries(kaintana_macos_backend
    "-framework Cocoa"
    "-framework CoreGraphics"
    "-framework Metal"
    "-framework QuartzCore"
)
```

### Bazel Integration
```python
# In native_core_runtime.toml:
# Add under apple_sources:
#   "src/ui_v2/backends/macos/host_macos.m"
# Run: py -3 scripts/python/update_runtime.py
```

**Line count estimate:** ~600-800 lines for Phase 1 (software rendering). Adding CAMetalLayer + Metal rendering would add ~300-400 lines.

---

## 11. Risks and Mitigations

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|-----------|--------|-----------|
| 1 | Obj-C requirement (.m files) | Certain | Medium | Build system must compile .m files with Obj-C compiler. CMake handles via `LANGUAGE` property. |
| 2 | ARC vs MRC memory management | Low | Medium | Use `-fobjc-arc` flag for compile. ARC is standard for modern macOS (10.8+). Avoid manual retain/release. |
| 3 | Sandbox restrictions | Medium | Low | App Sandbox (macOS App Store requirement) restricts file system, network, hardware access. Kaintana doesn't need special entitlements for basic UI. Clipboard requires `com.apple.security.personal-information` entitlement. |
| 4 | No cross-compile from Linux | Certain | Medium | Must build on macOS. CI should use GitHub Actions macOS runners. |
| 5 | Autorelease pool exhaustion | Low | Medium | Create autorelease pool at top of each frame (.m syntax: `@autoreleasepool { ... }`). Drain at frame end. |
| 6 | Retina-only fractional scaling (no 1.25x) | Low | Low | macOS doesn't support fractional UI scaling natively. Game engines must use CAMetalLayer manually. Kaintana follows platform convention. |
| 7 | Dark/Light mode appearance | Medium | Low | Listen for `AppleInterfaceThemeChangedNotification`. Kaintana processes theme via attribute table — no hardcoded colors. |
| 8 | Notch / Dynamic Island safe areas | Medium | Low | NSWindow.safeAreaInsets / NSView.safeAreaInsets. Pass via attribute table. |
| 9 | Stage Manager / window tiling | Low | Low | macOS 13 Ventura+ Stage Manager. NSWindowDelegate `windowWillUseStandardFrame:defaultFrame:` for custom tiling behavior. |

---

## 12. Testing Strategy

### 12.1 Non-Mac CI — Null Backend Equivalence

Since macOS backends cannot run on non-Mac CI:

```c
// test_macos_compat.c — verify the backend compiles and its vtable is valid
void test_macos_vtable(void) {
    assert(kaintana_macos_backend.init != NULL);
    assert(kaintana_macos_backend.shutdown != NULL);
    assert(kaintana_macos_backend.new_frame != NULL);
    assert(kaintana_macos_backend.render != NULL);
}
```

### 12.2 macOS-Specific Tests

Run on macOS CI (GitHub Actions):

```bash
# Compile check
gcc -std=c11 -Wall -Wextra -pedantic -Werror \
    -I X:/runtime/native/include \
    -I X:/runtime/native/src/ui_v2 \
    -fsyntax-only host_macos.m

# Functional test (headless, no window server)
./test_macos_null  # Uses null backend, tests that kt_DrawData is valid

# Integration test (with window server — macOS CI)
./test_macos_window  # Creates window, verifies DPI query, renders test pattern
```

### 12.3 Z3 Proof Packs

| Proof | File | What It Proves |
|-------|------|---------------|
| DPI scale | `macos-dpi-scale.smt2` | backingScaleFactor is integer-only, scale ∈ {1.0, 2.0} |
| FB sizing | `macos-fb-sizing.smt2` | fb_w = logical_w × scale, fb_h = logical_h × scale, no overflow |
| Input conversion | `macos-input-coords.smt2` | NSView point coordinates don't need ÷scale for logical pixels |

### 12.4 Golden Image Comparison

```bash
# Render known scene to macOS framebuffer, compare with expected hash
./test_macos_golden --scene basic_ui --render software
./test_macos_golden --scene complex_layout --render software

# Cross-backend validation
./test_macos_cross --backend software --reference null
# Expected: macOS software output == null backend output (pixel-identical)
```

---

## 13. Task List

### P0 — Required for Minimum Viable Backend

| # | Task | Est. Lines | Dependencies |
|---|------|-----------|-------------|
| 1 | Create `host_macos.m` with vtable + static state | 100 | — |
| 2 | Implement `macos_init`: NSApplication, NSWindow, NSView creation | 80 | Task 1 |
| 3 | Implement DPI detection: `backingScaleFactor` query on init | 30 | Task 2 |
| 4 | Implement `macos_shutdown`: ordered teardown | 25 | Task 2 |
| 5 | Implement CGBitmapContext framebuffer creation | 60 | Task 2 |
| 6 | Implement software render path: kt_DrawData → CGContext drawing commands | 70 | Task 5 |
| 7 | Implement non-blocking event pump for `macos_new_frame` | 30 | Task 2 |
| 8 | Implement mouse input handling (mouseDown/Up/Dragged/Moved) | 40 | Task 7 |
| 9 | Implement keyboard input handling (keyDown/Up, flagsChanged) | 40 | Task 7 |
| 10 | Implement scroll wheel handling | 20 | Task 7 |
| 11 | Bridge DPI change via NSWindowDidChangeBackingPropertiesNotification | 30 | Task 3 |
| 12 | Wire input → session via kt_input_*() functions | 40 | Tasks 8-10 |
| 13 | Register DPI scale via kt_set_native_scale() on init + DPI change | 10 | Task 3 |
| 14 | Compile clean: gcc -Wall -Wextra -pedantic -Werror | — | All above |

### P1 — Important

| # | Task | Est. Lines | Dependencies |
|---|------|-----------|-------------|
| 15 | Dirty rect accumulator (64-rect, merge pattern from host_win32.c) | 50 | Task 6 |
| 16 | Text rendering with NSFont + NSString drawAtPoint | 40 | Task 6 |
| 17 | Glyph quad rendering (kt_Cmd KT_CMD_TEXT with text_id) | 30 | Task 16 |
| 18 | Fullscreen toggle | 20 | Task 2 |
| 19 | Clipboard support (NSPasteboard generalPasteboard) | 30 | Task 1 |
| 20 | Cursor management (NSCursor per element type) | 30 | Task 1 |
| 21 | Performance timer (mach_absolute_time / mach_timebase_info) | 20 | Task 1 |

### P2 — Nice to Have

| # | Task | Est. Lines | Dependencies |
|---|------|-----------|-------------|
| 22 | CAMetalLayer backing (Phase 2 GPU path) | 200 | Task 5 |
| 23 | Metal compute shader for kt_DrawData rendering | 200 | Task 22 |
| 24 | IME support (NSTextInputClient protocol) | 80 | Task 9 |
| 25 | Drag and drop (NSDraggingDestination) | 60 | Task 1 |
| 26 | Multi-window support | 80 | Task 14 |
| 27 | Window state persistence (frame restoration) | 30 | Task 2 |
| 28 | Safe area insets (notch / Dynamic Island) | 20 | Task 2 |

### P3 — Future

| # | Task | Est. Lines | Dependencies |
|---|------|-----------|-------------|
| 29 | Accessibility (NSAccessibility protocol) | 150 | — |
| 30 | Touch Bar support (NSTouchBar) | 80 | — |
| 31 | MetalFX upscaling (macOS 13+, for high-framerate rendering) | 100 | Task 23 |

---

## 14. References

- `MASTER_OS_AND_CONTRACT.md` §1 (4-function contract), §2 (Input funnel), §7 (DPI math)
- `MASTER_PLATFORM.md` §15 (Kaintana backend architecture)
- `MASTER_RENDERER.md` §8 (Backend consumption patterns)
- `MASTER_DPI_AND_SCALING.md` §3.2 (macOS DPI), §4 (Universal DPI pipeline)
- `dpi.tsv` §BACKEND_DPI row 19 (macOS DPI stub), §PLATFORM_MACOS (4 reference rows)
- `backends/win32/host_win32.c` (reference implementation — same 4-function contract)
- `backends/null/host_null.c` (testing reference — ~256 lines)
- `contract.tsv` P0-4 (software framebuffer), P0-6 (platform detection), BACKEND_DPI-19
