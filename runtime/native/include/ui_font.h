// ============================================================================
//  ui_font.h — Kain Native UI Font ABI
//  ============================================================================
//  stb_truetype-based font loading and glyph rasterization for the Kain UI
//  runtime. Provides the ABI contract between the widget library / renderer
//  and the font subsystem.
//
//  Glyph bitmap format:
//    Alpha mask only (1 byte per pixel, 0 = transparent, 255 = opaque).
//    Render by blending the alpha channel over the destination framebuffer
//    pixel: out = (src_alpha * src_color + (255 - src_alpha) * dst) / 255.
//
//  Reference: 21 Z3 proof packs at extras/_stb-truetype/
// ============================================================================

#ifndef ABI_UI_FONT_H
#define ABI_UI_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Glyph structure ─────────────────────────────────────────────────────
// Returned by abi_ui_font_get_glyph(). Must be released with
// abi_ui_font_release_glyph() after use.
//
//   bitmap   – alpha mask (width × height bytes, row-major, 1 byte/pixel)
//              Each byte is the glyph coverage at that pixel (0=transparent,
//              255=fully opaque). Blend as: out = (src_color * alpha / 255)
//              over destination.
//   width    – bitmap width  in pixels
//   height   – bitmap height in pixels
//   x_offset – x offset from glyph origin (baseline start) to left edge of
//              bitmap. Typically negative for leftward protrusion.
//   y_offset – y offset from baseline to top edge of bitmap.
//              Typically negative (glyph extends above baseline).
//   advance  – horizontal advance in pixels. Add to pen_x after rendering
//              this glyph to position the next one.
//
// Layout in framebuffer:
//   pixel_x = pen_x + x_offset + col   (0 <= col < width)
//   pixel_y = pen_y + y_offset + row   (0 <= row < height)
typedef struct KainUiGlyph {
    const uint8_t* bitmap;    /* alpha mask (width * height bytes), row-major */
    int width;                /* bitmap width in pixels */
    int height;               /* bitmap height in pixels */
    int x_offset;             /* x origin offset (pen_x + x_offset = left edge) */
    int y_offset;             /* y baseline offset (baseline + y_offset = top edge) */
    int advance;              /* horizontal advance in scaled pixels */
} KainUiGlyph;

// ── Font ABI ────────────────────────────────────────────────────────────

// Load TTF data into a font resource.
//   session_id – UI session identifier
//   key        – lookup key for this font (e.g. "default")
//   family     – font family name (e.g. "Segoe UI")
//   size       – pixel size (e.g. 14.0)
//   ttf_data   – raw TTF file bytes (WILL BE COPIED internally)
//   ttf_len    – length of ttf_data in bytes
// Returns font resource ID (> 0) on success, <= 0 on failure.
int64_t abi_ui_font_load_ttf(
    int64_t session_id,
    const char* key,
    const char* family,
    double size,
    const uint8_t* ttf_data,
    int64_t ttf_len
);

// Get a glyph for a single codepoint from a loaded font.
//   session_id – UI session identifier
//   font_id    – font resource ID returned by abi_ui_font_load_ttf
//   codepoint  – Unicode codepoint (0-0x10FFFF)
// Returns a KainUiGlyph* that must be released with
//   abi_ui_font_release_glyph(), or NULL if invalid.
KainUiGlyph* abi_ui_font_get_glyph(
    int64_t session_id,
    int64_t font_id,
    int codepoint
);

// Release a glyph returned by abi_ui_font_get_glyph.
// After calling this, the glyph pointer must not be dereferenced.
void abi_ui_font_release_glyph(KainUiGlyph* glyph);

// Get font vertical metrics (scaled to pixel size).
//   session_id      – UI session identifier
//   font_resource_id– font resource ID
//   ascent          – [out] pixels above baseline (positive)
//   descent         – [out] pixels below baseline (negative)
//   line_gap        – [out] recommended line spacing gap
// Returns 0 on success, negative on error.
int kain_ui_font_get_vmetrics(
    int64_t session_id,
    int64_t font_resource_id,
    int* ascent,
    int* descent,
    int* line_gap
);

// ── Style key for font resource ID ────────────────────────────────────
// The font resource ID is stored on a node via:
//   abi_ui_node_set_style_i64(session, node_id, "font", font_resource_id);
// The renderer reads this key to select the font for text rendering.

#ifdef __cplusplus
}
#endif

#endif /* ABI_UI_FONT_H */
