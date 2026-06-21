// vkvg_bridge.h — Flat C API over vkvg for reson8
//
// Wraps https://github.com/jpbruyere/vkvg (MIT, Vulkan 2D vector graphics)
// into a Kain-includable flat C surface. Kain never sees vkvg types.
//
// vkvg is a Cairo-like 2D vector drawing library using Vulkan for
// hardware acceleration. This bridge exposes the full drawing surface:
// rectangles, rounded rects, ellipses, arcs, bezier curves, text,
// gradients, transforms, clipping, and save/restore.
//
// The bridge expects an existing Vulkan instance/device/queue from the
// Kain-side Vulkan setup (see src/bridge/vulkan_ui.kn).

#ifndef KAIN_VKVG_BRIDGE_H
#define KAIN_VKVG_BRIDGE_H

#if defined(_WIN32) || defined(_WIN64)
#define KAIN_VKVG_EXPORT __declspec(dllexport)
#else
#define KAIN_VKVG_EXPORT
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Line cap constants (match vkvg_line_cap_t) ──
#define VKVG_CAP_BUTT   0
#define VKVG_CAP_ROUND  1
#define VKVG_CAP_SQUARE 2

// ── Line join constants (match vkvg_line_join_t) ──
#define VKVG_JOIN_MITER 0
#define VKVG_JOIN_ROUND 1
#define VKVG_JOIN_BEVEL 2

// ── Fill rule constants (match vkvg_fill_rule_t) ──
#define VKVG_FILL_EVEN_ODD 0
#define VKVG_FILL_NON_ZERO 1

// ── Operator constants (match vkvg_operator_t) ──
#define VKVG_OP_CLEAR      0
#define VKVG_OP_SOURCE     1
#define VKVG_OP_OVER       2
#define VKVG_OP_DIFFERENCE 3

// ── Status constants ──
#define VKVG_BRIDGE_OK               0
#define VKVG_BRIDGE_ERR_NOT_INIT     -1
#define VKVG_BRIDGE_ERR_NULL_HANDLE  -2
#define VKVG_BRIDGE_ERR_VKVG         -3

// ============================================================================
//  LIFECYCLE — Device from existing Vulkan
// ============================================================================

// Initialize vkvg with an existing Vulkan context. All handles are uint64_t
// (cast from Vulkan opaque pointers). Pass 0 for any handle to have vkvg
// create its own (not recommended — prefer sharing the existing device).
//
// Returns: 0 on success, negative on error.
KAIN_VKVG_EXPORT int32_t vkvg_bridge_init(
    uint64_t vk_instance,         // VkInstance (or 0 to auto-create)
    uint64_t vk_physical_device,  // VkPhysicalDevice (or 0 to auto-pick)
    uint64_t vk_device,           // VkDevice (or 0 to auto-create)
    uint32_t queue_family_index,  // Graphics queue family index
    uint32_t queue_index,         // Queue index (usually 0)
    uint32_t multisample          // 1 = no MSAA, 4 = 4x MSAA, etc.
);

// Destroy the vkvg device. All surfaces and contexts must be destroyed first.
KAIN_VKVG_EXPORT void vkvg_bridge_shutdown(void);

// Check if bridge is initialized.
KAIN_VKVG_EXPORT int32_t vkvg_bridge_is_init(void);

// ── Device queries ──
KAIN_VKVG_EXPORT void vkvg_bridge_set_dpy(int32_t hdpy, int32_t vdpy);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_get_hdpy(void);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_get_vdpy(void);

// ============================================================================
//  SURFACE
// ============================================================================

// Create a drawing surface. Returns opaque handle (0 on failure).
KAIN_VKVG_EXPORT uint64_t vkvg_bridge_surface_create(uint32_t width, uint32_t height);

// Destroy a surface.
KAIN_VKVG_EXPORT void vkvg_bridge_surface_destroy(uint64_t surface);

// Query surface dimensions.
KAIN_VKVG_EXPORT uint32_t vkvg_bridge_surface_get_width(uint64_t surface);
KAIN_VKVG_EXPORT uint32_t vkvg_bridge_surface_get_height(uint64_t surface);

// Get the underlying Vulkan image handle (for swapchain / presentation).
KAIN_VKVG_EXPORT uint64_t vkvg_bridge_surface_get_vk_image(uint64_t surface);

// Clear the surface (slow path — prefer context clear).
KAIN_VKVG_EXPORT void vkvg_bridge_surface_clear(uint64_t surface);

// Write surface content to PNG file.
KAIN_VKVG_EXPORT int32_t vkvg_bridge_surface_write_to_png(uint64_t surface, const char* path);

// ============================================================================
//  CONTEXT (drawing state machine)
// ============================================================================

// Create a drawing context bound to a surface. Returns opaque handle.
KAIN_VKVG_EXPORT uint64_t vkvg_bridge_context_create(uint64_t surface);

// Destroy a context (flushes pending operations first).
KAIN_VKVG_EXPORT void vkvg_bridge_context_destroy(uint64_t ctx);

// Get context status (0 = OK).
KAIN_VKVG_EXPORT int32_t vkvg_bridge_context_status(uint64_t ctx);

// Flush all pending drawing operations to the surface.
KAIN_VKVG_EXPORT void vkvg_bridge_flush(uint64_t ctx);

// ============================================================================
//  SOURCE COLOR / PATTERN
// ============================================================================

// Set solid RGBA source color (components 0.0–1.0).
KAIN_VKVG_EXPORT void vkvg_bridge_set_source_rgba(uint64_t ctx, float r, float g, float b, float a);

// Set solid RGB source color (alpha = 1.0).
KAIN_VKVG_EXPORT void vkvg_bridge_set_source_rgb(uint64_t ctx, float r, float g, float b);

// Set source to a 32-bit packed RGBA color (0xAABBGGRR).
KAIN_VKVG_EXPORT void vkvg_bridge_set_source_color(uint64_t ctx, uint32_t rgba);

// Set source from another surface (for image patterns).
KAIN_VKVG_EXPORT void vkvg_bridge_set_source_surface(uint64_t ctx, uint64_t surf, float x, float y);

// ============================================================================
//  STROKE CONFIGURATION
// ============================================================================

KAIN_VKVG_EXPORT void vkvg_bridge_set_line_width(uint64_t ctx, float width);
KAIN_VKVG_EXPORT float vkvg_bridge_get_line_width(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_set_line_cap(uint64_t ctx, int32_t cap);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_get_line_cap(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_set_line_join(uint64_t ctx, int32_t join);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_get_line_join(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_set_miter_limit(uint64_t ctx, float limit);

KAIN_VKVG_EXPORT void vkvg_bridge_set_opacity(uint64_t ctx, float opacity);
KAIN_VKVG_EXPORT float vkvg_bridge_get_opacity(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_set_fill_rule(uint64_t ctx, int32_t rule);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_get_fill_rule(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_set_operator(uint64_t ctx, int32_t op);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_get_operator(uint64_t ctx);

// ── Dashes ──
KAIN_VKVG_EXPORT void vkvg_bridge_set_dash(uint64_t ctx, const float* dashes, uint32_t count, float offset);
KAIN_VKVG_EXPORT uint32_t vkvg_bridge_get_dash_count(uint64_t ctx);
KAIN_VKVG_EXPORT float vkvg_bridge_get_dash_offset(uint64_t ctx);

// ============================================================================
//  PATH CONSTRUCTION
// ============================================================================

KAIN_VKVG_EXPORT void vkvg_bridge_new_path(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_new_sub_path(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_close_path(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_move_to(uint64_t ctx, float x, float y);
KAIN_VKVG_EXPORT void vkvg_bridge_rel_move_to(uint64_t ctx, float dx, float dy);

KAIN_VKVG_EXPORT void vkvg_bridge_line_to(uint64_t ctx, float x, float y);
KAIN_VKVG_EXPORT void vkvg_bridge_rel_line_to(uint64_t ctx, float dx, float dy);

// Cubic bezier: (x1,y1) and (x2,y2) are control points, (x3,y3) is end.
KAIN_VKVG_EXPORT void vkvg_bridge_curve_to(uint64_t ctx, float x1, float y1, float x2, float y2, float x3, float y3);
KAIN_VKVG_EXPORT void vkvg_bridge_rel_curve_to(uint64_t ctx, float x1, float y1, float x2, float y2, float x3, float y3);

// Quadratic bezier: (x1,y1) is control point, (x2,y2) is end.
KAIN_VKVG_EXPORT void vkvg_bridge_quadratic_to(uint64_t ctx, float x1, float y1, float x2, float y2);
KAIN_VKVG_EXPORT void vkvg_bridge_rel_quadratic_to(uint64_t ctx, float x1, float y1, float x2, float y2);

// Arc: centered at (xc,yc), radius r, from angle a1 to a2 (radians, clockwise).
KAIN_VKVG_EXPORT void vkvg_bridge_arc(uint64_t ctx, float xc, float yc, float radius, float a1, float a2);
// Arc counter-clockwise.
KAIN_VKVG_EXPORT void vkvg_bridge_arc_negative(uint64_t ctx, float xc, float yc, float radius, float a1, float a2);

// ── Shapes (add closed sub-paths) ──
KAIN_VKVG_EXPORT int32_t vkvg_bridge_rectangle(uint64_t ctx, float x, float y, float w, float h);
KAIN_VKVG_EXPORT int32_t vkvg_bridge_rounded_rectangle(uint64_t ctx, float x, float y, float w, float h, float radius);
KAIN_VKVG_EXPORT void vkvg_bridge_rounded_rectangle2(uint64_t ctx, float x, float y, float w, float h, float rx, float ry);
KAIN_VKVG_EXPORT void vkvg_bridge_ellipse(uint64_t ctx, float rx, float ry, float x, float y, float rotation);

// ── Path query ──
KAIN_VKVG_EXPORT int32_t vkvg_bridge_has_current_point(uint64_t ctx);

// ============================================================================
//  STROKE / FILL / PAINT
// ============================================================================

KAIN_VKVG_EXPORT void vkvg_bridge_stroke(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_stroke_preserve(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_fill(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_fill_preserve(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_paint(uint64_t ctx);

// Clear surface using render pass load op (faster than surface_clear).
KAIN_VKVG_EXPORT void vkvg_bridge_clear(uint64_t ctx);

// ============================================================================
//  CLIPPING
// ============================================================================

KAIN_VKVG_EXPORT void vkvg_bridge_clip(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_clip_preserve(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_reset_clip(uint64_t ctx);

// ============================================================================
//  TRANSFORM
// ============================================================================

KAIN_VKVG_EXPORT void vkvg_bridge_save(uint64_t ctx);
KAIN_VKVG_EXPORT void vkvg_bridge_restore(uint64_t ctx);

KAIN_VKVG_EXPORT void vkvg_bridge_translate(uint64_t ctx, float dx, float dy);
KAIN_VKVG_EXPORT void vkvg_bridge_scale(uint64_t ctx, float sx, float sy);
KAIN_VKVG_EXPORT void vkvg_bridge_rotate(uint64_t ctx, float radians);
KAIN_VKVG_EXPORT void vkvg_bridge_identity_matrix(uint64_t ctx);

// ============================================================================
//  TEXT
// ============================================================================

// Select font by family name (requires FontConfig).
KAIN_VKVG_EXPORT void vkvg_bridge_select_font_face(uint64_t ctx, const char* name);

// Load font from file path, register under a short name.
KAIN_VKVG_EXPORT void vkvg_bridge_load_font_from_path(uint64_t ctx, const char* path, const char* name);

// Set font size in points.
KAIN_VKVG_EXPORT void vkvg_bridge_set_font_size(uint64_t ctx, uint32_t size);

// Draw text at current point.
KAIN_VKVG_EXPORT void vkvg_bridge_show_text(uint64_t ctx, const char* utf8);

// Get text extents width (before drawing, for layout).
KAIN_VKVG_EXPORT float vkvg_bridge_text_extents_width(uint64_t ctx, const char* utf8);

// Get text extents height.
KAIN_VKVG_EXPORT float vkvg_bridge_text_extents_height(uint64_t ctx, const char* utf8);

// ============================================================================
//  LINEAR GRADIENT
// ============================================================================

// Create a linear gradient pattern from (x0,y0) to (x1,y1).
// Returns opaque handle (0 on failure).
KAIN_VKVG_EXPORT uint64_t vkvg_bridge_gradient_create_linear(float x0, float y0, float x1, float y1);

// Create a radial gradient pattern.
KAIN_VKVG_EXPORT uint64_t vkvg_bridge_gradient_create_radial(float cx0, float cy0, float r0, float cx1, float cy1, float r1);

// Add a color stop at offset (0.0–1.0).
KAIN_VKVG_EXPORT int32_t vkvg_bridge_gradient_add_stop(uint64_t pat, float offset, float r, float g, float b, float a);

// Set pattern extend mode: 0=NONE, 1=REPEAT, 2=REFLECT, 3=PAD.
KAIN_VKVG_EXPORT void vkvg_bridge_pattern_set_extend(uint64_t pat, int32_t extend);

// Destroy a pattern.
KAIN_VKVG_EXPORT void vkvg_bridge_pattern_destroy(uint64_t pat);

// Set a pattern as the current source.
KAIN_VKVG_EXPORT void vkvg_bridge_set_source(uint64_t ctx, uint64_t pat);

#ifdef __cplusplus
}
#endif

#endif // KAIN_VKVG_BRIDGE_H
