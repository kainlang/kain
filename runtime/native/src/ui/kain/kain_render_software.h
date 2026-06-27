#ifndef KAIN_RENDER_SOFTWARE_H
#define KAIN_RENDER_SOFTWARE_H

#include <stdint.h>
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ══════════════════════════════════════════════════════════════════════════
//  kain_render_software.h — Software rendering substrate for Kain UI
// ══════════════════════════════════════════════════════════════════════════
//  Backend-agnostic draw primitives. No tree-walking, no widgets, no layout.
//  The KainSoftwareRenderer owns a framebuffer and clip/transform stacks.
//  All coordinates are in float space; the renderer clips to the framebuffer.
// ══════════════════════════════════════════════════════════════════════════

// ── Software renderer context (opaque, defined in .c) ────────────────────
typedef struct KainSoftwareRenderer KainSoftwareRenderer;

// ── Lifecycle ────────────────────────────────────────────────────────────

// Create a renderer. Takes ownership of the framebuffer for its lifetime.
// framebuffer may be NULL to create an offscreen surface (caller must
// set_framebuffer before drawing).
KainSoftwareRenderer* kain_renderer_create(int fb_width, int fb_height, uint32_t* framebuffer);
void kain_renderer_destroy(KainSoftwareRenderer* r);

// Replace the backing framebuffer (e.g. after window resize).
void kain_renderer_set_framebuffer(KainSoftwareRenderer* r, uint32_t* fb, int w, int h);

// Set the UI session ID for font resource lookups (required for text rendering).
void kain_renderer_set_font_session(KainSoftwareRenderer* r, int64_t session_id);

// Set DPI scaling factor (logical → physical pixels).
// Default is 1.0 (auto-detected from the system on Windows).
// Call this to override the auto-detected value.
void kain_renderer_set_dpi_scale(KainSoftwareRenderer* r, float scale);

// Query the current framebuffer. out_stride is in uint32_t elements.
void kain_renderer_get_framebuffer(KainSoftwareRenderer* r, uint32_t** out_fb,
                                    int* out_w, int* out_h, int* out_stride);

// ── Frame lifecycle ──────────────────────────────────────────────────────

// Fill the entire framebuffer with a solid color (uses memcpy for strict aliasing).
void kain_renderer_clear(KainSoftwareRenderer* r, kainColor color);

// Flush the command buffer (no-op for software; GPU backends submit here).
void kain_renderer_submit(KainSoftwareRenderer* r);

// Present the framebuffer (no-op for software; host calls BitBlt or equivalent).
void kain_renderer_present(KainSoftwareRenderer* r);

// ── Draw primitives (backend-agnostic) ───────────────────────────────────

// Fill an axis-aligned rectangle with a solid color.
void kain_render_fill_rect(KainSoftwareRenderer* r, kainRect rect, kainColor color);

// Fill a rounded rectangle (corner radius). Falls back to fill_rect if radius <= 0.
void kain_render_fill_rounded_rect(KainSoftwareRenderer* r, kainRect rect,
                                    float radius, kainColor color);

// Draw a rectangle outline (border) with a given thickness.
void kain_render_stroke_rect(KainSoftwareRenderer* r, kainRect rect,
                              float thickness, kainColor color);

// Fill a circle centered at `center` with radius `radius`.
void kain_render_fill_circle(KainSoftwareRenderer* r, kainPoint center,
                              float radius, kainColor color);

// Draw a circle outline with a given thickness.
void kain_render_stroke_circle(KainSoftwareRenderer* r, kainPoint center,
                                float radius, float thickness, kainColor color);

// Blit a region from a texture (texture_id is a resource id).
void kain_render_blit(KainSoftwareRenderer* r, kainRect src_rect,
                       kainRect dst_rect, int64_t texture_id);

// Render a UTF-8 text string at `pos` (baseline) with the given font and color.
void kain_render_text(KainSoftwareRenderer* r, kainPoint pos, const char* text,
                       int64_t font_id, float size, kainColor color);

// Fill a rectangle with a linear horizontal gradient from left to right.
// `colors` and `stops` arrays must have `count` entries.
// stops[i] is the gradient position for colors[i] in [0..1].
void kain_render_gradient_rect(KainSoftwareRenderer* r, kainRect rect,
                                const kainColor* colors, const float* stops,
                                int count);

// Apply a box blur to a region of the framebuffer (approximate, constant radius).
void kain_render_blur(KainSoftwareRenderer* r, kainRect rect, float radius);

// ── Clip stack ───────────────────────────────────────────────────────────

// Push a clip rectangle onto the stack. Subsequent draws are clipped to the
// intersection of all active clips. Max depth: 16.
void kain_render_push_clip(KainSoftwareRenderer* r, kainRect rect);

// Pop the most recently pushed clip rectangle.
void kain_render_pop_clip(KainSoftwareRenderer* r);

// ── Transform stack ──────────────────────────────────────────────────────

// Push an affine transform onto the stack. Subsequent draws are transformed.
// Max depth: 16.
void kain_render_push_transform(KainSoftwareRenderer* r, kainMatrix matrix);

// Pop the most recently pushed transform.
void kain_render_pop_transform(KainSoftwareRenderer* r);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RENDER_SOFTWARE_H */
