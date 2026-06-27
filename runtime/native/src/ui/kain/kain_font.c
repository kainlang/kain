// ============================================================================
//  kain_font.c — Font Subsystem Implementation
//  ============================================================================
//  Font loading (from bytes, path, platform defaults), glyph access,
//  text measurement, and metrics. Wraps the existing abi_ui_font_* ABI
//  from ui_system.c and ui_font.h.
//
//  Platform font path search is extracted from:
//    - widgets/ui_widget.c  (ui_widget_load_default_font)
//    - native_ui_surface.c  (native_ui_load_default_font)
//
//  All font loading gracefully handles missing files (returns 0, no crash).
//
//  Part of the Kain UI substrate (KUIF Phase 1).
//  ============================================================================

#include "kain_font.h"
#include "../../include/ui_font.h"
#include "../../include/ui_system.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ── Font load from raw bytes ──────────────────────────────────────

int64_t kain_font_load(int64_t session_id, const uint8_t* ttf_data,
                       int64_t ttf_len, float size) {
    if (!ttf_data || ttf_len <= 0) return 0;
    return abi_ui_font_load_ttf(
        session_id,
        "kain_substrate",     // key
        "system",              // family
        (double)size,
        ttf_data,
        ttf_len
    );
}

// ── Font load from file path ──────────────────────────────────────

int64_t kain_font_load_path(int64_t session_id, const char* filepath, float size) {
    if (!filepath || !filepath[0]) return 0;

    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > 64 * 1024 * 1024) {  // sanity: max 64 MB
        fclose(f);
        return 0;
    }

    uint8_t* data = (uint8_t*)malloc((size_t)len);
    if (!data) {
        fclose(f);
        return 0;
    }

    size_t nread = fread(data, 1, (size_t)len, f);
    fclose(f);

    if (nread != (size_t)len) {
        free(data);
        return 0;
    }

    int64_t id = kain_font_load(session_id, data, (int64_t)len, size);
    free(data);  // abi_ui_font_load_ttf copies the data internally
    return id;
}

// ── Platform default font ─────────────────────────────────────────

int64_t kain_font_load_default(int64_t session_id, float size) {
    // 1. Environment variable override
    const char* env_path = getenv("KAIN_UI_FONT");
    if (env_path && env_path[0]) {
        int64_t id = kain_font_load_path(session_id, env_path, size);
        if (id > 0) return id;
    }

    // 2. Platform-specific font paths
#ifdef _WIN32
    const char* paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/consola.ttf",
        NULL
    };
#elif defined(__APPLE__)
    const char* paths[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf",
        "/Library/Fonts/Arial.ttf",
        NULL
    };
#else // Linux / POSIX
    const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        NULL
    };
#endif

    for (int i = 0; paths[i] != NULL; i++) {
        int64_t id = kain_font_load_path(session_id, paths[i], size);
        if (id > 0) return id;
    }

    return 0;
}

// ── Glyph access ──────────────────────────────────────────────────

void* kain_font_get_glyph(int64_t session_id, int64_t font_id, int codepoint) {
    return (void*)abi_ui_font_get_glyph(session_id, font_id, codepoint);
}

void kain_font_release_glyph(void* glyph) {
    if (glyph) {
        abi_ui_font_release_glyph((KainUiGlyph*)glyph);
    }
}

// ── Text measurement ──────────────────────────────────────────────

float kain_font_measure_text(int64_t session_id, int64_t font_id, const char* text) {
    if (!text || !text[0]) return 0.0f;
    return (float)abi_ui_text_measure_width(session_id, font_id, text);
}

float kain_font_line_height(int64_t session_id, int64_t font_id) {
    // Use a single "X" character as a proxy for line height measurement.
    // The underlying stb_truetype implementation reads the font's vertical
    // metrics (ascent - descent + line_gap) scaled to pixel size, so the
    // text content only matters for distinguishing multi-line strings.
    return (float)abi_ui_text_measure_height(session_id, font_id, "X");
}

// ── Metrics ───────────────────────────────────────────────────────

KainFontMetrics kain_font_get_metrics(int64_t session_id, int64_t font_id) {
    KainFontMetrics m;
    memset(&m, 0, sizeof(m));

    int ascent = 0, descent = 0, line_gap = 0;
    int result = kain_ui_font_get_vmetrics(session_id, font_id,
                                           &ascent, &descent, &line_gap);
    if (result == 0) {
        m.ascent   = ascent;
        m.descent  = descent;
        m.line_gap = line_gap;
        m.scale    = 0.0f;  // scale is internal to stb_truetype, not exposed
    }
    return m;
}
