// ============================================================================
//  kaintana.h — THE one public header for the Kaintana UI substrate.
//
//  Architecture: 4-layer stack
//    L3: Kain Components & Widgets  (std::kaintana/*.kn — pure Kain)
//    L2: Vtable ABI Contract        (THIS HEADER — 24-slot KainComponentSurface)
//    L1: C Substrate                (tree.c, box_math.c, damage.c, draw_pixels.c)
//    L0: Platform Backends          (backends/ — host_null, host_win32, render_vulkan...)
//
//  Design tenets:
//    - One header only. No twin copies, no sync scripts.
//    - Zero platform headers in core types (no windows.h, no X11/, no GL/).
//    - Arena-only allocation. No malloc per node.
//    - 24-slot immutable vtable. Slots 24-31 reserved. Never reorder, only append.
//    - 10-Year-Old Rule: every function obvious from name alone.
//    - kt_ prefix (3-char Goldilocks zone — matches nk_, mu_).
//    - 34 public functions. 6 public types. 24 vtable slots. That's the entire API.
//
//  Section index:
//    1.  Version & Header Info
//    2.  Core Types (kt_Vec2, kt_Rect, kt_Color, kt_Matrix, kt_Session)
//    3.  Enum Types (kt_CmdType, KaintanaInputKind, KaintanaLayoutDir, etc.)
//    4.  Input Type (kt_Input — flat struct)
//    5.  Draw Command Types (kt_Cmd, kt_DrawData)
//    6.  Vtable (KainComponentSurface from component_surface.h)
//    7.  Backend VTable (4 functions)
//    8.  Session Lifecycle (kt_init, kt_make, kt_free)
//    9.  Frame Loop (kt_begin, kt_end, kt_present, kt_should_close)
//    10. Input Funnel (kt_input_mouse_move..kt_input_text)
//    11. Element Tree (kt_row, kt_end_row, kt_text)
//    11b. Interaction Query (kt_hovered, kt_clicked, kt_active)
//    13. Style Attributes (kt_fill, kt_stroke, kt_radius, kt_opacity, kt_font)
//    14. State Persistence (kt_put/get family — 6 functions)
//    15. Draw Output (kt_cmd_count, kt_cmd_get)
//    16. Backend Registry (kt_backend_register, kt_backend_select, kt_backend_probe)
//    17. Color Math — inline helpers
//      17A: Color Representations (from_u32, to_u32, parse_hex, premultiply, unpremultiply)
//      17B: Color Interpolation (lerp, gradient_sample)
//      17C: sRGB-Linear Conversion
//      17D: Porter-Duff Compositing (blend_compose, blend_mix_*)
//      17E: Luminance & Saturation
//      17F: Opacity Stack
//      17G: HSL Blend Modes (hue, saturation, color, luminosity)
//    18. Easing Functions — inline helpers
//      18A: Smoothstep (smoothstep, smootherstep)
//      18B: CSS Ease Curves (ease_in, ease_out, ease_in_out, cubic_bezier)
//      18C: Cubic Ease (cubic_in, cubic_out, cubic_in_out)
//    19. Internal Helpers (kaintana__* — NOT public API)
//      19A: Internal function declarations
//      19B: Utility macros (MIN, MAX, CLAMP, DIV255, ALIGN_UP, etc.)
// ============================================================================
#ifndef KAINTANA_H
#define KAINTANA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>           // fmaxf, fminf, powf, sqrtf, fabsf
#include "component_surface.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//  SECTION 1: VERSION & HEADER INFO
// ============================================================================
#define KT_VERSION_MAJOR    0
#define KT_VERSION_MINOR    1
#define KT_VERSION_PATCH    0
#define KT_VERSION          "0.1.0"
#define KT_API_VERSION      1           // Bump on ABI-breaking changes
#define KT_API              extern      // Export marker

// ============================================================================
//  SECTION 1B: Kaintana UI Diagnostic Error Codes
//  KAIN_DIAG_SUBSYSTEM_UI range 5000-5999
//
//  These extend the base runtime diagnostics with Kaintana-specific codes.
//  The base UI codes are in diagnostics.h (KAIN_DIAG_CODE_UI_BASE = 5000).
//  Kaintana-specific codes start at 5050 for headroom.
// ============================================================================
#define KT_DIAG_CODE_UI_BASE                    5050
#define KT_DIAG_CODE_UI_INVALID_ATTRIBUTE       (KT_DIAG_CODE_UI_BASE + 1)  // Unknown or malformed attribute name/value
#define KT_DIAG_CODE_UI_LAYOUT_OVERFLOW         (KT_DIAG_CODE_UI_BASE + 2)  // Node/layout capacity exhausted
#define KT_DIAG_CODE_UI_RENDER_ERROR            (KT_DIAG_CODE_UI_BASE + 3)  // Draw batch or render failure
#define KT_DIAG_CODE_UI_BACKEND_FAILURE         (KT_DIAG_CODE_UI_BASE + 4)  // Backend init/select failed
#define KT_DIAG_CODE_UI_HANDLE_ERROR            (KT_DIAG_CODE_UI_BASE + 5)  // Handle table acquire/resolve failure
#define KT_DIAG_CODE_UI_HASH_ERROR              (KT_DIAG_CODE_UI_BASE + 6)  // Hash table overflow or lookup failure

// ============================================================================
//  SECTION 2: CORE TYPES
// ============================================================================

// ── kt_Vec2: 2D point/vector, 8 bytes ──────────────────────────────────────
typedef struct { float x, y; }             kt_Vec2;

// ── kt_Rect: axis-aligned rectangle, 16 bytes ──────────────────────────────
typedef struct { float x, y, w, h; }       kt_Rect;

// ── kt_Color: float RGBA, 16 bytes ─────────────────────────────────────────
typedef struct { float r, g, b, a; }       kt_Color;

// ── kt_Matrix: 2D affine transform row-major m[6], 24 bytes ────────────────
//     [m[0] m[1] m[4]]    transform: x' = m[0]*x + m[1]*y + m[4]
//     [m[2] m[3] m[5]]               y' = m[2]*x + m[3]*y + m[5]
typedef struct { float m[6]; }             kt_Matrix;

// ── kt_Fixed8_8: fixed-point 8.8 (1/256 precision), 2 bytes ───────────────
//     Used by box_math.c and draw_pixels.c for sub-pixel corner radius.
typedef uint16_t                           kt_Fixed8_8;

// ── kt_Session: opaque session handle ──────────────────────────────────────
typedef struct kt_Session_t kt_Session;

// ── Surface registration name (used by both C init and Kain-side resolution) ─
#define KAINTANA_SURFACE_NAME          "kaintana"

// ============================================================================
//  SECTION 3: ENUM TYPES
// ============================================================================

// ── kt_CmdType: draw command discriminator (6 primitives) ──────────────────
typedef enum kt_CmdType {
    KT_CMD_FILL     = 0,     // Filled rect (possibly rounded)
    KT_CMD_STROKE   = 1,     // Rect outline
    KT_CMD_TEXT     = 2,     // Text glyph rendering
    KT_CMD_IMAGE    = 3,     // Image blit
    KT_CMD_CLIP     = 4,     // Push scissor rect
    KT_CMD_UNCLIP   = 5      // Pop scissor rect
} kt_CmdType;

// ── KaintanaInputKind: input event discriminator ──────────────────────────
typedef enum KaintanaInputKind {
    KT_INPUT_NONE         = 0,
    KT_INPUT_MOUSE_MOVE   = 1,
    KT_INPUT_MOUSE_DOWN   = 2,
    KT_INPUT_MOUSE_UP     = 3,
    KT_INPUT_SCROLL       = 4,
    KT_INPUT_KEY_DOWN     = 5,
    KT_INPUT_KEY_UP       = 6,
    KT_INPUT_TEXT         = 7,
    KT_INPUT_COUNT        = 8
} KaintanaInputKind;

// ── KaintanaLayoutDir: flex main-axis direction ────────────────────────────
typedef enum KaintanaLayoutDir {
    KT_DIR_ROW                  = 0,
    KT_DIR_COLUMN               = 1,
    KT_DIR_ROW_REVERSE          = 2,
    KT_DIR_COLUMN_REVERSE       = 3
} KaintanaLayoutDir;

// ── KaintanaJustify: main-axis alignment ───────────────────────────────────
typedef enum KaintanaJustify {
    KT_JUSTIFY_FLEX_START       = 0,
    KT_JUSTIFY_CENTER           = 1,
    KT_JUSTIFY_FLEX_END         = 2,
    KT_JUSTIFY_SPACE_BETWEEN    = 3,
    KT_JUSTIFY_SPACE_AROUND     = 4,
    KT_JUSTIFY_SPACE_EVENLY     = 5
} KaintanaJustify;

// ── KaintanaAlign: cross-axis alignment ────────────────────────────────────
typedef enum KaintanaAlign {
    KT_ALIGN_STRETCH            = 0,
    KT_ALIGN_FLEX_START         = 1,
    KT_ALIGN_CENTER             = 2,
    KT_ALIGN_FLEX_END           = 3,
    KT_ALIGN_BASELINE           = 4,
    KT_ALIGN_AUTO               = 5
} KaintanaAlign;

// ── KaintanaWrap: flex-wrap mode ──────────────────────────────────────────
typedef enum KaintanaWrap {
    KT_WRAP_NO_WRAP             = 0,
    KT_WRAP_WRAP                = 1,
    KT_WRAP_WRAP_REVERSE        = 2
} KaintanaWrap;

// ── KaintanaUnit: length unit ─────────────────────────────────────────────
typedef enum KaintanaUnit {
    KT_UNIT_UNDEFINED           = 0,
    KT_UNIT_POINT               = 1,
    KT_UNIT_PERCENT             = 2,
    KT_UNIT_AUTO                = 3
} KaintanaUnit;

// ── KaintanaBlendMode: Porter-Duff + CSS mix blend modes ──────────────────
typedef enum KaintanaBlendMode {
    KT_BLEND_SRC_OVER           = 0,   // Default UI compositing
    KT_BLEND_SRC                = 1,   // Copy / replace
    KT_BLEND_DST                = 2,   // Mask read
    KT_BLEND_SRC_IN             = 3,   // Alpha mask source
    KT_BLEND_DST_IN             = 4,   // Alpha mask backdrop
    KT_BLEND_SRC_OUT            = 5,   // Erase masked region
    KT_BLEND_DST_OUT            = 6,   // Hold-out matte
    KT_BLEND_SRC_ATOP           = 7,   // Source over clipped to backdrop alpha
    KT_BLEND_DST_ATOP           = 8,   // Backdrop over clipped to source alpha
    KT_BLEND_XOR                = 9,   // Exclusive-or region
    KT_BLEND_PLUS               = 10,  // Additive (unclamped)
    KT_BLEND_PLUS_LIGHTER       = 11,  // Additive with clamping
    KT_BLEND_MULTIPLY           = 12,  // CSS multiply
    KT_BLEND_SCREEN             = 13,  // CSS screen
    KT_BLEND_OVERLAY            = 14,  // CSS overlay
    KT_BLEND_DARKEN             = 15,  // CSS darken
    KT_BLEND_LIGHTEN            = 16,  // CSS lighten
    KT_BLEND_COLOR_DODGE        = 17,  // CSS color-dodge
    KT_BLEND_COLOR_BURN         = 18,  // CSS color-burn
    KT_BLEND_HARD_LIGHT         = 19,  // CSS hard-light
    KT_BLEND_SOFT_LIGHT         = 20,  // CSS soft-light
    KT_BLEND_DIFFERENCE         = 21,  // CSS difference
    KT_BLEND_EXCLUSION          = 22,  // CSS exclusion
    KT_BLEND_HUE                = 23,  // HSL hue
    KT_BLEND_SATURATION         = 24,  // HSL saturation
    KT_BLEND_COLOR              = 25,  // HSL color
    KT_BLEND_LUMINOSITY         = 26,  // HSL luminosity
    KT_BLEND_COUNT              = 27
} KaintanaBlendMode;

// ============================================================================
//  SECTION 4: INPUT TYPE
// ============================================================================
//  Flat struct matching ImGuiIO style. Backend fills this each frame.
// ============================================================================

// ── kt_Input: per-frame input state ────────────────────────────────────────
typedef struct kt_Input {
    float   mouse_x, mouse_y;       // Pointer position (screen space)
    float   scroll_dx, scroll_dy;   // Scroll delta this frame
    int     keys_down[256];          // 1 = currently pressed
    int     mouse_down[5];           // 1 = currently pressed (0=left,1=right,2=middle)
    char    text_input[32];          // UTF-8 text input this frame
    int     text_len;                // Length of text_input in bytes
    double  time_ms;                 // Frame timestamp in ms
} kt_Input;

// ============================================================================
//  SECTION 5: DRAW COMMAND TYPES
// ============================================================================

// ── kt_Cmd: single draw command ────────────────────────────────────────────
typedef struct kt_Cmd {
    kt_CmdType  type;               // KT_CMD_FILL, _STROKE, _TEXT, etc.
    kt_Rect     bounds;             // Position and size
    uint32_t    color;              // Premultiplied ARGB primary
    uint32_t    color_b;            // Secondary (gradient end, etc.)
    float       radius;             // Corner radius (rounded rects)
    float       thickness;          // Stroke width (outlines, text weight)
    int         text_id;            // Glyph/texture ID (-1 = none)
    int         image_id;           // Image/texture handle (-1 = none)
} kt_Cmd;

// ── kt_DrawData: per-frame render output ──────────────────────────────────
typedef struct kt_DrawData {
    const kt_Cmd*   cmds;           // Flat draw command array
    int             cmd_count;      // Number of commands
} kt_DrawData;

// ============================================================================
//  SECTION 6: VTABLE (KainComponentSurface from component_surface.h)
// ============================================================================
//  THE VTABLE IS DEFINED IN component_surface.h — this header includes it
//  as the single source of truth. The Kain compiler's LLVM codegen emits
//  calls by slot index; any reordering silently corrupts compiled code.
//
//  KaintanaComponentSurface is a direct alias for KainComponentSurface.
//  Slot layout (24 conceptual slots; slots 24-31 are a range reservation
//  convention, NOT struct fields — they exist in internal.h's extended type):
//
//    0-1     Session lifecycle
//    2-4     Element tree
//    5-7     Attribute setters
//    8-9     State persistence (i64)
//    10-12   Frame lifecycle
//    13-14   Event pump
//    15-16   Window lifecycle
//    17      Platform attachment
//    18      GPU extension discovery
//    19-22   Expanded state (f64, string)
//    23      Event callback binding
//    24-31   [Range reservation — not struct fields; see internal.h
//             for the extended KaintanaVtable type with NULL slots.]
//
//  Registration: kain_component_surface_register("kaintana", &vtable)
//  at startup via kt_init().
// ============================================================================

// KaintanaComponentSurface IS KainComponentSurface — 24 slots, no add-ons.
// The compiler emits calls by slot index against this exact layout.
// The 8 reserved slots (24-31) exist as a Kaintana-internal convention
// managed in internal.h; they are NOT part of this ABI struct.
typedef KainComponentSurface KaintanaComponentSurface;

// Surface registry functions declared in component_surface.h:
//   void kain_component_surface_register(const char* name, const KainComponentSurface* surface);
//   const KainComponentSurface* kain_component_surface_resolve(const char* name);

// ============================================================================
//  SECTION 7: BACKEND VTABLE
// ============================================================================
//  Every platform/renderer backend implements exactly 4 functions.
//  If more are needed, the concern is leaking into the wrong layer.
// ============================================================================

typedef struct KaintanaBackendConfig {
    const char*     title;
    int             width;
    int             height;
    int             fullscreen;
    void*           platform_handle;
} KaintanaBackendConfig;

typedef struct KaintanaBackendVTable {
    int  (*init)(const KaintanaBackendConfig* config);
    void (*shutdown)(void);
    void (*new_frame)(void);
    void (*render)(const kt_DrawData* draw_data);
} KaintanaBackendVTable;

// ============================================================================
//  SECTION 8: SESSION LIFECYCLE
// ============================================================================

// ── kt_init: Initialize the Kaintana system. Call once at startup. ─────────
KT_API void     kt_init(void);

// ── kt_make: Create a UI session. Returns opaque handle or NULL. ──────────
KT_API kt_Session* kt_make(const char* name, int w, int h);

// ── kt_free: Destroy a UI session. Call at program exit. ──────────────────
KT_API void     kt_free(kt_Session* s);

// ============================================================================
//  SECTION 9: FRAME LOOP
// ============================================================================

// ── kt_begin: Start a new frame. delta_ms = time since last frame. ─────────
KT_API void     kt_begin(kt_Session* s, double delta_ms);

// ── kt_end: Conclude command recording for this frame. ─────────────────────
KT_API void     kt_end(kt_Session* s);

// ── kt_present: Put pixels on screen. ─────────────────────────────────────
KT_API void     kt_present(kt_Session* s);

// ── kt_should_close: Returns 1 if close requested, 0 otherwise. ───────────
KT_API int      kt_should_close(kt_Session* s);

// ============================================================================
//  SECTION 10: INPUT FUNNEL
// ============================================================================
//  Feed input BEFORE kt_begin(). Backend calls these from OS event loop.
// ============================================================================

KT_API void kt_input_mouse_move(kt_Session* s, float x, float y);
KT_API void kt_input_mouse_down(kt_Session* s, int button);
KT_API void kt_input_mouse_up(kt_Session* s, int button);
KT_API void kt_input_scroll(kt_Session* s, float dx, float dy);
KT_API void kt_input_key_down(kt_Session* s, int key);
KT_API void kt_input_key_up(kt_Session* s, int key);
KT_API void kt_input_text(kt_Session* s, const char* text);

// ============================================================================
//  SECTION 11: ELEMENT TREE
// ============================================================================
//  Call between kt_begin() and kt_end(). Elements are parent-ordered
//  via kt_row/kt_end_row pairs.
// ============================================================================

// ── kt_row: Begin a new element. kind = "box", "text", "stack", etc.
//     stable_key = unique name like "login_button" ("" for auto).
//     Returns element ID for setting attributes.
KT_API int  kt_row(kt_Session* s, int parent, const char* kind, const char* key);

// ── kt_end_row: Close the most recently opened element. ───────────────────
KT_API void kt_end_row(kt_Session* s);

// ── kt_text: Set text content on an element. ──────────────────────────────
KT_API void kt_text(kt_Session* s, int elem, const char* text);

// ── kt_hovered: Returns 1 if the pointer is over this element this frame. ──
KT_API int  kt_hovered(kt_Session* s, int elem);

// ── kt_clicked: Returns 1 if this element was clicked this frame. ──────────
KT_API int  kt_clicked(kt_Session* s, int elem);

// ── kt_active: Returns 1 if this element has pointer capture. ─────────────
KT_API int  kt_active(kt_Session* s, int elem);

// ============================================================================
//  SECTION 12: LAYOUT ATTRIBUTES
// ============================================================================

KT_API void kt_width     (kt_Session* s, int elem, float w);
KT_API void kt_height    (kt_Session* s, int elem, float h);
KT_API void kt_pad       (kt_Session* s, int elem, float all);
KT_API void kt_pad_xy    (kt_Session* s, int elem, float x, float y);
KT_API void kt_gap       (kt_Session* s, int elem, float gap);
KT_API void kt_direction (kt_Session* s, int elem, int dir);   // 0=row, 1=col, 2=row-reverse, 3=col-reverse

// ============================================================================
//  SECTION 13: STYLE ATTRIBUTES
// ============================================================================
//  Colors passed as hex strings ("#21D4A1") or named theme colors
//  ("bg", "accent", "text", "button", etc.). C substrate is a dumb pipe.
// ============================================================================

KT_API void kt_fill   (kt_Session* s, int elem, const char* color);
KT_API void kt_stroke (kt_Session* s, int elem, const char* color, float w);
KT_API void kt_radius (kt_Session* s, int elem, float r);
KT_API void kt_opacity(kt_Session* s, int elem, float a);
KT_API void kt_font   (kt_Session* s, int elem, float size);

// ============================================================================
//  SECTION 14: STATE PERSISTENCE
// ============================================================================
//  Values survive across frames. Scoped to session.
// ============================================================================

KT_API void        kt_put  (kt_Session* s, const char* key, int64_t v);
KT_API void        kt_put_f(kt_Session* s, const char* key, double v);
KT_API void        kt_put_s(kt_Session* s, const char* key, const char* v);
KT_API int64_t     kt_get  (kt_Session* s, const char* key, int64_t fallback);
KT_API double      kt_get_f(kt_Session* s, const char* key, double fallback);
KT_API const char* kt_get_s(kt_Session* s, const char* key, const char* fallback);

// ============================================================================
//  SECTION 15: DRAW OUTPUT
// ============================================================================
//  Read after kt_end() to get the frame's render commands.
// ============================================================================

KT_API int     kt_cmd_count(kt_Session* s);
KT_API kt_Cmd  kt_cmd_get  (kt_Session* s, int index);

// ============================================================================
//  SECTION 16: BACKEND REGISTRY
// ============================================================================

KT_API int  kt_backend_register(kt_Session* s, const char* name,
                                 const KaintanaBackendVTable* vtable);
KT_API int  kt_backend_select(kt_Session* s, const char* name);
KT_API int  kt_backend_probe(kt_Session* s);

// ============================================================================
//  SECTION 17: COLOR MATH (inline helpers)
// ============================================================================

// ---------------------------------------------------------------------------
//  17A: COLOR REPRESENTATIONS
// ---------------------------------------------------------------------------

// kt_color_from_u32: Unpack 0xAARRGGBB to float kt_Color
//     Proven Z3 UNSAT: kt-color-lerp.smt2
static inline kt_Color kt_color_from_u32(uint32_t col) {
    kt_Color c;
    c.r = (float)((col >> 16) & 0xFF) / 255.0f;
    c.g = (float)((col >>  8) & 0xFF) / 255.0f;
    c.b = (float)((col >>  0) & 0xFF) / 255.0f;
    c.a = (float)((col >> 24) & 0xFF) / 255.0f;
    return c;
}

// kt_color_to_u32: Pack float kt_Color to 0xAARRGGBB
//     Proven Z3 UNSAT: kt-color-lerp.smt2 (clamp always produces [0,1])
//     Branchless via fmaxf/fminf (SSE maxss/minss)
static inline uint32_t kt_color_to_u32(kt_Color c) {
    uint8_t r = (uint8_t)(fmaxf(0.0f, fminf(1.0f, c.r)) * 255.0f + 0.5f);
    uint8_t g = (uint8_t)(fmaxf(0.0f, fminf(1.0f, c.g)) * 255.0f + 0.5f);
    uint8_t b = (uint8_t)(fmaxf(0.0f, fminf(1.0f, c.b)) * 255.0f + 0.5f);
    uint8_t a = (uint8_t)(fmaxf(0.0f, fminf(1.0f, c.a)) * 255.0f + 0.5f);
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// kt_color_parse_hex: Parse hex/named color string to uint32 ARGB
//     Supports: #RGB, #RRGGBB, #RRGGBBAA, named colors.
//     Proven Z3 UNSAT: kt-color-hex-nibble.smt2
//     NOTE: Implementation belongs in color.c (not draw_pixels.c) to avoid
//     cross-file dependency between tree.c (which needs hex parsing for
//     kt_fill/kt_stroke) and the renderer.
uint32_t kt_color_parse_hex(const char* hex);   // in color.c

// kt_color_premultiply: Straight -> premultiplied (float)
//     Proven Z3 UNSAT: kt-color-premultiply-proof.smt2
static inline kt_Color kt_color_premultiply(kt_Color c) {
    kt_Color r = { c.r * c.a, c.g * c.a, c.b * c.a, c.a };
    return r;
}

// kt_color_premultiply_u8: Straight -> premultiplied (integer 8-bit)
//     Proven Z3 UNSAT: kt-color-premultiply-proof.smt2
static inline uint32_t kt_color_premultiply_u8(uint32_t argb) {
    uint32_t a_val = (argb >> 24) & 0xFF;
    uint32_t r_val = ((argb >> 16) & 0xFF) * a_val;
    uint32_t g_val = ((argb >>  8) & 0xFF) * a_val;
    uint32_t b_val = (argb >>  0) & 0xFF;
    b_val = b_val * a_val;  // multiply after masking
    uint32_t div = 1 + (a_val >> 8);
    return (a_val << 24)
         | (((r_val + div) >> 8) << 16)
         | (((g_val + div) >> 8) << 8)
         | ((b_val + div) >> 8);
}

// kt_color_unpremultiply: Premultiplied -> straight (NaN-guarded)
//     Proven Z3 UNSAT: kt-color-premultiply-proof.smt2
static inline kt_Color kt_color_unpremultiply(kt_Color c) {
    float inv_a = (c.a > 1e-15f) ? 1.0f / c.a : 0.0f;
    kt_Color r = { c.r * inv_a, c.g * inv_a, c.b * inv_a, c.a };
    return r;
}

// ---------------------------------------------------------------------------
//  17B: COLOR INTERPOLATION
// ---------------------------------------------------------------------------

// kt_color_lerp: sRGB linear interpolation. out = a + (b - a) * t
//     Proven Z3 UNSAT: kt-color-lerp.smt2 (t=0 gives a, t=1 gives b)
static inline kt_Color kt_color_lerp(kt_Color a, kt_Color b, float t) {
    kt_Color r;
    r.r = a.r + (b.r - a.r) * t;
    r.g = a.g + (b.g - a.g) * t;
    r.b = a.b + (b.b - a.b) * t;
    r.a = a.a + (b.a - a.a) * t;
    return r;
}

// kt_color_gradient_sample: Sample N-stop gradient at position x
//     Returns uint32_t ARGB. O(log N) binary search for n>4, linear for n<=4.
//     Proven Z3 UNSAT: kt-color-lerp.smt2
//     NOTE: Implementation belongs in color.c (not draw_pixels.c).
uint32_t kt_color_gradient_sample(const uint32_t* stops, const float* positions,
                                   int n_stops, float x);  // in color.c

// ---------------------------------------------------------------------------
//  17C: sRGB -> LINEAR CONVERSION
// ---------------------------------------------------------------------------

// kt_color_srgb_to_linear: sRGB byte [0,1] to linear float
static inline float kt_color_srgb_to_linear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

// kt_color_linear_to_srgb: Linear float to sRGB [0,1]
static inline float kt_color_linear_to_srgb(float c) {
    if (c <= 0.0031308f) return c * 12.92f;
    return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

// ---------------------------------------------------------------------------
//  17D: PORTER-DUFF COMPOSITING
// ---------------------------------------------------------------------------

// kt_blend_compose: General Porter-Duff with SRC_OVER fast path
//     mode 0 (SRC_OVER) fast path: cs + cb * (1 - as) — handles 99% of calls
//     Proven Z3 UNSAT: kt-blend-srcover-proof.smt2
static inline kt_Color kt_blend_compose(kt_Color cs, float as,
                                         kt_Color cb, float ab, int mode) {
    if (mode == 0) {
        float inv_sa = 1.0f - as;
        kt_Color out;
        out.r = cs.r + cb.r * inv_sa;
        out.g = cs.g + cb.g * inv_sa;
        out.b = cs.b + cb.b * inv_sa;
        out.a = as + ab * inv_sa;
        return out;
    }
    // Porter-Duff coefficients for premultiplied compositing:
    //   out.rgb = fa * cs.rgb + fb * cb.rgb
    //   out.a   = fa * as     + fb * ab
    // cs and cb are premultiplied, so no extra as/ab multiplication needed.
    float fa, fb;
    switch (mode) {
        case 1:  fa = 1.0f;        fb = 0.0f;            break; // SRC
        case 2:  fa = 0.0f;        fb = 1.0f;            break; // DST
        case 3:  fa = ab;          fb = 0.0f;            break; // SRC_IN
        case 4:  fa = 0.0f;        fb = as;              break; // DST_IN
        case 5:  fa = 1.0f - ab;   fb = 0.0f;            break; // SRC_OUT
        case 6:  fa = 0.0f;        fb = 1.0f - as;       break; // DST_OUT
        case 7:  fa = ab;          fb = 1.0f - as;       break; // SRC_ATOP
        case 8:  fa = 1.0f - ab;   fb = as;              break; // DST_ATOP
        case 9:  fa = 1.0f - ab;   fb = 1.0f - as;       break; // XOR
        case 10: fa = 1.0f;        fb = 1.0f;            break; // PLUS
        case 11: fa = 1.0f;        fb = 1.0f;            break; // PLUS_LIGHTER
        default: fa = 1.0f;        fb = 1.0f - as;       break; // SRC_OVER fallback
    }
    kt_Color out;
    out.r = fa * cs.r + fb * cb.r;
    out.g = fa * cs.g + fb * cb.g;
    out.b = fa * cs.b + fb * cb.b;
    out.a = fa * as + fb * ab;
    return out;
}

// CSS mix blend modes (single-channel helpers)
static inline float kt_blend_mix_normal(float cb, float cs)       { (void)cb; return cs; }
static inline float kt_blend_mix_multiply(float cb, float cs)     { return cb * cs; }
static inline float kt_blend_mix_screen(float cb, float cs)       { return cb + cs - cb * cs; }
static inline float kt_blend_mix_darken(float cb, float cs)       { return cb < cs ? cb : cs; }
static inline float kt_blend_mix_lighten(float cb, float cs)      { return cb > cs ? cb : cs; }
static inline float kt_blend_mix_difference(float cb, float cs)   { float d = cb - cs; return d < 0.0f ? -d : d; }
static inline float kt_blend_mix_exclusion(float cb, float cs)    { return cb + cs - 2.0f * cb * cs; }
static inline float kt_blend_mix_color_dodge(float cb, float cs)  { if (cb == 0.0f) return 0.0f; if (cs >= 1.0f) return 1.0f; float d = cb / (1.0f - cs); return d < 1.0f ? d : 1.0f; }
static inline float kt_blend_mix_color_burn(float cb, float cs)   { if (cb >= 1.0f) return 1.0f; if (cs == 0.0f) return 0.0f; float d = (1.0f - cb) / cs; return 1.0f - (d < 1.0f ? d : 1.0f); }
static inline float kt_blend_mix_hard_light(float cb, float cs)   { if (cs <= 0.5f) return 2.0f * cb * cs; return cb + (2.0f * cs - 1.0f) - cb * (2.0f * cs - 1.0f); }
static inline float kt_blend_mix_overlay(float cb, float cs)      { if (cb <= 0.5f) return 2.0f * cs * cb; return 1.0f - 2.0f * (1.0f - cs) * (1.0f - cb); }

// kt_blend_mix_soft_light: Vello blend.wgsl:35-43
static inline float kt_blend_mix_soft_light(float cb, float cs) {
    float d = (cb <= 0.25f)
        ? ((16.0f * cb - 12.0f) * cb + 4.0f) * cb
        : sqrtf(cb);
    if (cs <= 0.5f)
        return cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
    return cb + (2.0f * cs - 1.0f) * (d - cb);
}

// kt_blend_mix: Dispatch to CSS blend mode by number

// kt_blend_mix: Dispatch to CSS blend mode by number
static inline float kt_blend_mix(float cb, float cs, int mode) {
    switch (mode) {
        case 12: return kt_blend_mix_multiply(cb, cs);
        case 13: return kt_blend_mix_screen(cb, cs);
        case 14: return kt_blend_mix_overlay(cb, cs);
        case 15: return kt_blend_mix_darken(cb, cs);
        case 16: return kt_blend_mix_lighten(cb, cs);
        case 17: return kt_blend_mix_color_dodge(cb, cs);
        case 18: return kt_blend_mix_color_burn(cb, cs);
        case 19: return kt_blend_mix_hard_light(cb, cs);
        case 20: return kt_blend_mix_soft_light(cb, cs);
        case 21: return kt_blend_mix_difference(cb, cs);
        case 22: return kt_blend_mix_exclusion(cb, cs);
        default: return cs;
    }
}

// ---------------------------------------------------------------------------
//  17E: LUMINANCE & SATURATION
// ---------------------------------------------------------------------------

// kt_color_luminance: Rec. 601 via FMA
//     Proven Z3 UNSAT: kt-color-luminance-proof.smt2
static inline float kt_color_luminance(kt_Color c) {
    return fmaf(0.299f, c.r, fmaf(0.587f, c.g, 0.114f * c.b));
}

// kt_luminance_u8: Integer luminance from packed ARGB
//     Proven Z3 UNSAT: kt-color-luminance-proof.smt2
static inline uint8_t kt_luminance_u8(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >>  8) & 0xFF;
    uint32_t b = (rgb >>  0) & 0xFF;
    return (uint8_t)((r * 77 + g * 150 + b * 29 + 128) >> 8);
}

// kt_color_saturation: max(R,G,B) - min(R,G,B) (branchless via fmaxf/fminf)
//     Proven Z3 UNSAT: kt-color-luminance-proof.smt2
static inline float kt_color_saturation(kt_Color c) {
    float mx = fmaxf(c.r, fmaxf(c.g, c.b));
    float mn = fminf(c.r, fminf(c.g, c.b));
    return mx - mn;
}

// kt_saturation_u8: Integer saturation from packed ARGB
//     Proven Z3 UNSAT: kt-color-luminance-proof.smt2
static inline uint8_t kt_saturation_u8(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >>  8) & 0xFF;
    uint32_t b = (rgb >>  0) & 0xFF;
    uint32_t mx = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    uint32_t mn = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    return (uint8_t)(mx - mn);
}

// ---------------------------------------------------------------------------
//  17F: OPACITY STACK
// ---------------------------------------------------------------------------
//  Opacity is multiplicative and commutative. Z3-proven:
//    - Identity:    opacity 1.0 == x*1.0 == x
//    - Zero:        opacity 0.0 == transparent black
//    - Commutative: apply(p, apply(q, c)) == apply(q, apply(p, c))
//    - Associative: apply(p*q, c) == apply(p, apply(q, c)) within +/- 1 ULP
//  Proof: kt-opacity-stack-proof.smt2
// ---------------------------------------------------------------------------

// kt_apply_opacity: Apply opacity to float kt_Color (premultiplied)
static inline kt_Color kt_apply_opacity(kt_Color c, float opacity) {
    kt_Color r;
    r.r = c.r * opacity;  r.g = c.g * opacity;
    r.b = c.b * opacity;  r.a = c.a * opacity;
    return r;
}

// kt_apply_opacity_u32: Apply opacity to packed uint32 (premultiplied)
static inline uint32_t kt_apply_opacity_u32(uint32_t color, uint8_t opacity_255) {
    uint32_t div = 1 + (opacity_255 >> 8);
    uint32_t a = (color >> 24) & 0xFF;
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >>  8) & 0xFF;
    uint32_t b = (color >>  0) & 0xFF;
    uint32_t out_a = (a * opacity_255 + div) >> 8;
    uint32_t out_r = (r * opacity_255 + div) >> 8;
    uint32_t out_g = (g * opacity_255 + div) >> 8;
    uint32_t out_b = (b * opacity_255 + div) >> 8;
    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

// ---------------------------------------------------------------------------
//  17G: HSL BLEND MODES (non-separable, full-color)
// ---------------------------------------------------------------------------
//  These operate on kt_Color because they need luminance and saturation
//  across all three channels. Forward decls below resolve to definitions
//  in Section 19 (internal helpers).
// ---------------------------------------------------------------------------

static inline kt_Color kt_blend_hsl_clip(kt_Color c);
static inline kt_Color kt_blend_hsl_set_lum(kt_Color c, float l);
static inline kt_Color kt_blend_hsl_set_sat(kt_Color c, float s);

static inline kt_Color kt_blend_mix_hue(kt_Color cb, kt_Color cs) {
    return kt_blend_hsl_set_lum(
        kt_blend_hsl_set_sat(cs, kt_color_saturation(cb)),
        kt_color_luminance(cb));
}
static inline kt_Color kt_blend_mix_saturation(kt_Color cb, kt_Color cs) {
    return kt_blend_hsl_set_lum(
        kt_blend_hsl_set_sat(cb, kt_color_saturation(cs)),
        kt_color_luminance(cb));
}
static inline kt_Color kt_blend_mix_color(kt_Color cb, kt_Color cs) {
    return kt_blend_hsl_set_lum(cs, kt_color_luminance(cb));
}
static inline kt_Color kt_blend_mix_luminosity(kt_Color cb, kt_Color cs) {
    return kt_blend_hsl_set_lum(cb, kt_color_luminance(cs));
}

// ============================================================================
//  SECTION 18: EASING FUNCTIONS (inline helpers)
// ============================================================================
//  Proven Z3 UNSAT: kt-cubic-bezier-ease.smt2, kt-smoothstep-derivative.smt2,
//                   kt-lerp-invariant.smt2
// ============================================================================

// ---------------------------------------------------------------------------
//  18A: SMOOTHSTEP EASING
// ---------------------------------------------------------------------------

// kt_ease_smoothstep: t^2(3-2t). C1 continuous, zero slope at t=0, t=1.
//     Proven Z3 UNSAT: kt-smoothstep-derivative.smt2
static inline float kt_ease_smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// kt_ease_smootherstep: t^3(t(6t-15)+10). C2 continuous.
//     Proven Z3 UNSAT: kt-smoothstep-derivative.smt2
static inline float kt_ease_smootherstep(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// ---------------------------------------------------------------------------
//  18B: CSS EASE CURVES
// ---------------------------------------------------------------------------
//  Closed-form equivalents of CSS cubic-bezier(). No Newton iteration needed
//  for standard eases. Proven Z3 UNSAT: kt-cubic-bezier-ease.smt2
// ---------------------------------------------------------------------------

// kt_ease_in: CSS ease-in ~= cubic-bezier(0.42, 0, 1.0, 1.0) ~= t^3
static inline float kt_ease_in(float t)   { return t * t * t; }

// kt_ease_out: CSS ease-out ~= cubic-bezier(0, 0, 0.58, 1.0) ~= 1-(1-t)^3
static inline float kt_ease_out(float t)  { float u = 1.0f - t; return 1.0f - u * u * u; }

// kt_ease_in_out: CSS ease-in-out ~= cubic-bezier(0.42, 0, 0.58, 1.0)
static inline float kt_ease_in_out(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float u = 2.0f - 2.0f * t;
    return 1.0f - u * u * u * 0.5f;
}

// kt_ease_cubic_bezier: Custom cubic-bezier via Newton iteration
//     Given P1=(x1,y1), P2=(x2,y2), solve Bx(t)=x for t, return By(t).
//     Newton converges in 5 iterations. dBx/dt > 0.87 > 0 on [0,1].
//     Proven Z3 UNSAT: kt-cubic-bezier-ease.smt2 (16 claims)
static inline float kt_ease_cubic_bezier(float x1, float y1,
                                          float x2, float y2, float x) {
    float t = 0.5f;
    for (int i = 0; i < 5; i++) {
        float u = 1.0f - t;
        float Bx  = 3.0f * u * u * t * x1 + 3.0f * u * t * t * x2 + t * t * t;
        float dBx = 3.0f * u * u * x1 + 6.0f * u * t * (x2 - x1)
                  + 3.0f * t * t * (1.0f - x2);
        if (dBx == 0.0f) break;
        t = (t - (Bx - x) / dBx);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    float u = 1.0f - t;
    return 3.0f * u * u * t * y1 + 3.0f * u * t * t * y2 + t * t * t;
}

// ---------------------------------------------------------------------------
//  18C: CUBIC EASE (simple powers)
// ---------------------------------------------------------------------------

static inline float kt_ease_cubic_in(float t)     { return t * t * t; }
static inline float kt_ease_cubic_out(float t)    { float u = 1.0f - t; return 1.0f - u * u * u; }
static inline float kt_ease_cubic_in_out(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float u = 2.0f - 2.0f * t;
    return 1.0f - u * u * u * 0.5f;
}

// ============================================================================
//  SECTION 19: INTERNAL HELPERS (kaintana__* -- NOT public API)
// ============================================================================
//  Used by tree.c, box_math.c, damage.c, draw_pixels.c.
//  NEVER call from application or Kain code. Double-underscore = internal.
// ============================================================================

// -- 19A: Internal function declarations -----------------------------------

// tree.c / attr_table.c internals
int     kaintana__node_find       (kt_Session* s, const char* stable_key);
void    kaintana__node_mark_dirty (kt_Session* s, int idx, int reason);
int     kaintana__attr_lookup     (const char* key);
struct KaintanaAttrEntry;
const struct KaintanaAttrEntry* kaintana__attr_get_entry(int index);
int     kaintana__attr_count      (void);

// box_math.c internals
void    kaintana__layout_pass1    (kt_Session* s);   // bottom-up desired sizes
void    kaintana__layout_pass2    (kt_Session* s);   // top-down arrange

// damage.c internals
void    kaintana__damage_process  (kt_Session* s);   // three-phase pipeline
void    kaintana__damage_add      (kt_Session* s, kt_Rect r);

// hit_test.c internals (in tree.c)
void    kaintana__hit_test        (kt_Session* s);   // pointer→node matching

// draw_pixels.c internals
void    kaintana__draw_generate   (kt_Session* s);   // walk, emit commands
void    kaintana__draw_merge      (kt_Session* s);   // auto-merge at insertion

// arena.c internals
void*   kaintana__arena_push      (kt_Session* s, size_t bytes);
void    kaintana__arena_reset     (kt_Session* s);

// hash_table.c internals
int     kaintana__hash_lookup     (kt_Session* s, uint64_t hash);
void    kaintana__hash_insert     (kt_Session* s, uint64_t hash, int idx);

// -- HSL helpers (defined here, used by 17D inlines) -----------------------

static inline kt_Color kt_blend_hsl_clip(kt_Color c) {
    float l = kt_color_luminance(c);
    float mn = (c.r < c.g) ? ((c.r < c.b) ? c.r : c.b) : ((c.g < c.b) ? c.g : c.b);
    float mx = (c.r > c.g) ? ((c.r > c.b) ? c.r : c.b) : ((c.g > c.b) ? c.g : c.b);
    if (mn < 0.0f) {
        float n = (l - mn) / l;
        c.r = l + (c.r - l) * n; c.g = l + (c.g - l) * n; c.b = l + (c.b - l) * n;
    }
    if (mx > 1.0f) {
        float n = (1.0f - l) / (mx - l);
        c.r = l + (c.r - l) * n; c.g = l + (c.g - l) * n; c.b = l + (c.b - l) * n;
    }
    return c;
}

static inline kt_Color kt_blend_hsl_set_lum(kt_Color c, float l) {
    float d = l - kt_color_luminance(c);
    kt_Color r = { c.r + d, c.g + d, c.b + d, c.a };
    return kt_blend_hsl_clip(r);
}

static inline kt_Color kt_blend_hsl_set_sat(kt_Color c, float s) {
    float mx = (c.r > c.g) ? ((c.r > c.b) ? c.r : c.b) : ((c.g > c.b) ? c.g : c.b);
    float mn = (c.r < c.g) ? ((c.r < c.b) ? c.r : c.b) : ((c.g < c.b) ? c.g : c.b);
    float mid = (mx + mn) * 0.5f;
    float range = mx - mn;
    if (range < 1e-10f) return c;
    float scale = s / range;
    float r_adj, g_adj, b_adj;
    if (c.r >= c.g && c.r >= c.b) {
        r_adj = s * (c.r - mid) / (mx - mid) + mid;
        if (c.g <= c.b) {
            float m_val = (c.b - c.g) * 0.5f * scale;
            b_adj = mid + m_val; g_adj = mid - m_val;
        } else {
            float m_val = (c.g - c.b) * 0.5f * scale;
            g_adj = mid + m_val; b_adj = mid - m_val;
        }
    } else if (c.g >= c.r && c.g >= c.b) {
        g_adj = s * (c.g - mid) / (mx - mid) + mid;
        if (c.r <= c.b) {
            float m_val = (c.b - c.r) * 0.5f * scale;
            b_adj = mid + m_val; r_adj = mid - m_val;
        } else {
            float m_val = (c.r - c.b) * 0.5f * scale;
            r_adj = mid + m_val; b_adj = mid - m_val;
        }
    } else {
        b_adj = s * (c.b - mid) / (mx - mid) + mid;
        if (c.r <= c.g) {
            float m_val = (c.g - c.r) * 0.5f * scale;
            g_adj = mid + m_val; r_adj = mid - m_val;
        } else {
            float m_val = (c.r - c.g) * 0.5f * scale;
            r_adj = mid + m_val; g_adj = mid - m_val;
        }
    }
    kt_Color res;
    res.r = r_adj < 0.0f ? 0.0f : (r_adj > 1.0f ? 1.0f : r_adj);
    res.g = g_adj < 0.0f ? 0.0f : (g_adj > 1.0f ? 1.0f : g_adj);
    res.b = b_adj < 0.0f ? 0.0f : (b_adj > 1.0f ? 1.0f : b_adj);
    res.a = c.a;
    return res;
}

// -- 19B: Utility macros ----------------------------------------------------

#define kaintana__MIN(a, b)         ((a) < (b) ? (a) : (b))
#define kaintana__MAX(a, b)         ((a) > (b) ? (a) : (b))
#define kaintana__CLAMP(v, lo, hi)  kaintana__MIN(kaintana__MAX((v), (lo)), (hi))

// kaintana__DIV255: fast u8 division by 255 (error +/- 0.5)
//     Proven Z3 UNSAT: kt-div255-proof.smt2 (7 phases)
#define kaintana__DIV255(x)         (((x) + 1 + ((x) >> 8)) >> 8)

// kaintana__ALIGN_UP: alignment up to power-of-2 boundary
#define kaintana__ALIGN_UP(v, a)    (((v) + (a) - 1) & ~((size_t)(a) - 1))

// kaintana__ALIGN_DOWN: alignment down to power-of-2 boundary
#define kaintana__ALIGN_DOWN(v, a)  ((v) & ~((size_t)(a) - 1))

// kaintana__SIZEOF_ARRAY: array element count
#define kaintana__SIZEOF_ARRAY(arr) (sizeof(arr) / sizeof((arr)[0]))

// Static assert macro
#ifndef KT_STATIC_ASSERT
#define KT_STATIC_ASSERT(cond, msg) typedef char kaintana__assert_##msg[(cond) ? 1 : -1]
#endif

// Type size assertions (frozen — must match geometry_types.tsv)
KT_STATIC_ASSERT(sizeof(kt_Vec2)   == 8,  kt_vec2_size_8);
KT_STATIC_ASSERT(sizeof(kt_Rect)   == 16, kt_rect_size_16);
KT_STATIC_ASSERT(sizeof(kt_Color)  == 16, kt_color_size_16);
KT_STATIC_ASSERT(sizeof(kt_Matrix) == 24, kt_matrix_size_24);

// Vtable size assertion: 24 active slots from the core runtime
KT_STATIC_ASSERT(sizeof(KainComponentSurface) == 24 * sizeof(void*),
                 kain_vtable_size_24_slots);
// Extended vtable (24 active + 8 reserved) lives in internal.h
KT_STATIC_ASSERT(sizeof(KaintanaComponentSurface) == sizeof(KainComponentSurface),
                 kaintana_vtable_is_kain_vtable);

// ============================================================================
//  SECTION 20: DPI & SCALE FACTOR API
// ============================================================================
//  Backends report OS DPI via kt_set_native_scale(). Kain code sets user zoom
//  via kt_set_zoom(). Effective scale = native_scale x user_zoom.
//  All kt_Rect/kt_Vec2 coordinates are in logical pixels. Backends multiply
//  by scale factor at render time.
// ============================================================================

// -- Constants ----------------------------------------------------------------

#define KT_DEFAULT_SCALE       1.0f    // 96 DPI = 100% scaling
#define KT_ZOOM_MIN            0.2f    // Minimum user zoom factor
#define KT_ZOOM_MAX            5.0f    // Maximum user zoom factor
#define KT_ZOOM_STEP           0.1f    // Keyboard zoom granularity
#define KT_ROUND_UI_FACTOR     64.0f   // 1/64 logical point for layout stability
#define KT_DPI_BASELINE        96.0f   // Standard DPI reference (scale = monitor_dpi / 96)

// -- Scale factor accessors ---------------------------------------------------

/// Effective horizontal scale = native_scale_x x user_zoom.
/// This is the value backends use for rendering. Returns 1.0f on NULL.
KT_API float kt_scale_factor_x(kt_Session* s);

/// Effective vertical scale = native_scale_y x user_zoom.
/// Separate axis for non-square pixels (rare, but supported). Returns 1.0f on NULL.
KT_API float kt_scale_factor_y(kt_Session* s);

/// OS-reported horizontal DPI scale only (no user zoom).
/// Use for font rasterization density. Returns 1.0f on NULL.
KT_API float kt_native_scale_x(kt_Session* s);

/// OS-reported vertical DPI scale only (no user zoom).
/// Returns 1.0f on NULL.
KT_API float kt_native_scale_y(kt_Session* s);

// -- Setters ------------------------------------------------------------------

/// Called by backends at init and on DPI change (e.g. WM_DPICHANGED).
/// Sets scale_changed=true for the next frame to process invalidation.
/// Values are clamped to [0.1, 10.0].
KT_API void kt_set_native_scale(kt_Session* s, float sx, float sy);

/// Called by Kain code. Sets user zoom factor. Clamped to KT_ZOOM_MIN-KT_ZOOM_MAX.
/// Deferred to next frame (egui pattern -- avoids jitter).
KT_API void kt_set_zoom(kt_Session* s, float zoom);

// -- Pixel-snap inline helpers ------------------------------------------------

/// Snap a logical coordinate to the nearest physical pixel boundary.
/// roundf(logical x scale) / scale
/// Use at tessellation time, not during layout.
static inline float kt_round_to_pixel_x(float logical, float scale) {
    return roundf(logical * scale) / scale;
}

/// Same for Y axis.
static inline float kt_round_to_pixel_y(float logical, float scale) {
    return roundf(logical * scale) / scale;
}

/// Snap to the center of a physical pixel for 1px-wide lines.
/// (floorf(logical x scale) + 0.5f) / scale
/// Prevents 1px lines from spanning 2 physical pixels.
static inline float kt_round_to_pixel_center_x(float logical, float scale) {
    return (floorf(logical * scale) + 0.5f) / scale;
}

/// Express 1 physical pixel in logical coordinate space.
/// 1.0f / scale
/// Xilem Divider pattern: at 2x scale, 1 physical pixel = 0.5 logical points.
static inline float kt_one_physical_pixel(float scale) {
    return 1.0f / scale;
}

/// Round to 1/64 logical point for numerical stability during layout.
/// roundf(x x KT_ROUND_UI_FACTOR) / KT_ROUND_UI_FACTOR
/// egui uses 1/32; we use 1/64 for finer precision on 4K+ displays.
static inline float kt_round_ui(float x) {
    return roundf(x * KT_ROUND_UI_FACTOR) / KT_ROUND_UI_FACTOR;
}

// ============================================================================
//  SECTION 21: TEST HELPER ACCESSORS (used by Kaintana Test Runner)
// ============================================================================
// These functions expose the active backend's framebuffer for golden-file
// comparison in CI. The implementations live in tests/kaintana_test_helpers.c
// and reference the null backend's kaintana_null_fb/kaintana_null_width/
// kaintana_null_height globals (defined in backends/null/host_null.c).
// ============================================================================

/// Return pointer to the active backend's framebuffer (row-major uint32_t ARGB).
KT_API uint32_t* kaintana_test_get_fb_ptr(void);

/// Return the framebuffer width in pixels.
KT_API int kaintana_test_get_fb_width(void);

/// Return the framebuffer height in pixels.
KT_API int kaintana_test_get_fb_height(void);

#ifdef __cplusplus
}
#endif

#endif // KAINTANA_H
