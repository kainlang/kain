#ifndef ABI_UI_COLOR_H
#define ABI_UI_COLOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Color parsing: any format text → 0xAARRGGBB uint32_t ──────────────
//
// Supported formats:
//   "#RGB"         → 0xFFRRGGBB (4-bit per channel, expanded)
//   "#RRGGBB"      → 0xFFRRGGBB (full alpha)
//   "#RRGGBBAA"    → 0xAARRGGBB
//   "rgb(r,g,b)"   → 0xFFRRGGBB (0-255 integers or 0-100%)
//   "rgba(r,g,b,a)"→ 0xAARRGGBB (a is 0.0-1.0 or 0-255)
//   "transparent"  → 0x00000000
//   "black" etc.   → named colors (subset of CSS named colors)
//
// Returns 0 (fully transparent) on parse failure.

uint32_t ui_parse_color(const char* text);

// Low-level parsers (exposed for testing)
uint32_t ui_parse_color_hex(const char* hex);
uint32_t ui_parse_color_rgb(const char* text);
uint32_t ui_parse_color_named(const char* name);

// Convenience: extract R, G, B, A components from a parsed color
uint8_t ui_color_r(uint32_t color);
uint8_t ui_color_g(uint32_t color);
uint8_t ui_color_b(uint32_t color);
uint8_t ui_color_a(uint32_t color);

// Blend src over dst (premultiplied alpha if premul != 0)
uint32_t ui_color_blend(uint32_t src, uint32_t dst);

// Apply opacity factor (0.0-1.0) to a color
uint32_t ui_color_with_opacity(uint32_t color, double opacity);

#ifdef __cplusplus
}
#endif

#endif /* ABI_UI_COLOR_H */
