// ============================================================================
//  kain_font.h — Font Subsystem
//  ============================================================================
//  Font loading, glyph access, and text measurement. Wraps the existing
//  abi_ui_font_load_ttf / abi_ui_font_get_glyph / abi_ui_text_measure_*
//  ABI from ui_system.c and ui_font.h.
//
//  Font path search (kain_font_load_default) probes platform-specific
//  system font directories, extracted from the widget library.
//
//  Part of the Kain UI substrate (KUIF Phase 1). Widget-free.
//  ============================================================================

#ifndef KAIN_FONT_H
#define KAIN_FONT_H

#include <stdint.h>
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Font metrics ──────────────────────────────────────────────────
typedef struct KainFontMetrics {
    int   ascent;     // pixels above baseline (positive)
    int   descent;    // pixels below baseline (negative)
    int   line_gap;   // recommended inter-line gap
    float scale;      // pixel scale factor (from stb_truetype)
} KainFontMetrics;

// ── Font lifecycle ────────────────────────────────────────────────

// Load a font from raw TTF bytes in memory.
// session_id: UI session ID
// ttf_data:   raw TTF file bytes (the implementation copies them)
// ttf_len:    length of ttf_data in bytes
// size:       pixel size (e.g. 16.0f)
// Returns font resource ID (> 0) on success, or 0 on failure.
int64_t kain_font_load(int64_t session_id, const uint8_t* ttf_data,
                       int64_t ttf_len, float size);

// Load a font from a .ttf file on disk.
// Returns font resource ID (> 0) on success, or 0 if the file
// cannot be read or is not a valid TTF.
int64_t kain_font_load_path(int64_t session_id, const char* filepath, float size);

// Load the platform default system font.
// Probes (in order):
//   1. KAIN_UI_FONT environment variable (explicit override)
//   2. Platform defaults:
//      Windows: C:/Windows/Fonts/segoeui.ttf → arial.ttf → tahoma.ttf → consola.ttf
//      macOS:   /System/Library/Fonts/Helvetica.ttc → SFNS.ttf → /Library/Fonts/Arial.ttf
//      Linux:   /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf → TTF/DejaVuSans.ttf
// Returns font resource ID (> 0) on success, or 0 if no system font found.
int64_t kain_font_load_default(int64_t session_id, float size);

// ── Glyph access (wraps abi_ui_font_get_glyph) ────────────────────
// Returns a pointer to a KainUiGlyph for the given codepoint.
// Caller must release with kain_font_release_glyph().
// Returns NULL if the codepoint is not found in this font.
void* kain_font_get_glyph(int64_t session_id, int64_t font_id, int codepoint);
void  kain_font_release_glyph(void* glyph);

// ── Text measurement ──────────────────────────────────────────────
float kain_font_measure_text(int64_t session_id, int64_t font_id, const char* text);
float kain_font_line_height(int64_t session_id, int64_t font_id);

// ── Metrics ───────────────────────────────────────────────────────
KainFontMetrics kain_font_get_metrics(int64_t session_id, int64_t font_id);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_FONT_H */
