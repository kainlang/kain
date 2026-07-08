// ============================================================================
//  color.c — Hex color parsing and gradient sampling for Kaintana UI.
//
//  Provides the two non-inline color functions declared in kaintana.h:
//    - kt_color_parse_hex     (§17A, line 457)
//    - kt_color_gradient_sample  (§17B, lines 508-509)
//
//  All other color operations (from_u32, to_u32, premultiply, unpremultiply,
//  lerp, srgb<->linear, blend compose/mix, luminance, opacity, HSL) live as
//  inline helpers in kaintana.h §17.
//
//  Z3 proofs (all UNSAT):
//    - kt-color-hex-nibble.smt2    (hex_nibble bounds)
//    - kt-color-lerp.smt2          (lerp + gradient_sample)
//    - kt-color-premultiply-proof.smt2
// ============================================================================
#include "kaintana.h"           // kt_Color, kt_color_from_u32, kt_color_to_u32,
                                // kt_color_lerp, uint32_t return types

#include <string.h>             // strlen, strcmp, memcpy
#include <stddef.h>             // NULL

// ============================================================================
//  SECTION 0: LOCAL HELPERS
// ============================================================================

// ── hex_nibble: Convert hex char to 0-15, return -1 for invalid ─────────────
//     Z3 UNSAT: kt-color-hex-nibble.smt2
//     BUG-014: Must return -1 for invalid chars so callers can detect malformed input.
static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
    return -1;
}

// ── char_lower: ASCII-only tolower, no locale dependency ─────────────────────
static inline int char_lower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

// ── str_equals_ci: Case-insensitive string compare (ASCII only) ─────────────
static int str_equals_ci(const char* a, const char* b) {
    while (*a && *b) {
        if (char_lower((unsigned char)*a) != char_lower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

// ============================================================================
//  SECTION 1: NAMED COLOR TABLE
// ============================================================================

typedef struct {
    const char* name;
    uint32_t    color;          // 0xAARRGGBB
} NamedColor;

// Theme colors match the default dark theme used by Kaintana.
// Names are stored in lowercase for case-insensitive matching.
static const NamedColor kt_named_colors[] = {
    // ── Semantic / special ──────────────────────────────────
    { "transparent",   0x00000000 },
    { "black",         0xFF000000 },
    { "white",         0xFFFFFFFF },
    { "red",           0xFFFF0000 },
    { "green",         0xFF00FF00 },
    { "blue",          0xFF0000FF },
    { "yellow",        0xFFFFFF00 },
    { "cyan",          0xFF00FFFF },
    { "magenta",       0xFFFF00FF },
    { "gray",          0xFF808080 },
    { "grey",          0xFF808080 },
};

static const int kt_named_color_count =
    (int)(sizeof(kt_named_colors) / sizeof(kt_named_colors[0]));

// ============================================================================
//  SECTION 2: kt_color_parse_hex — Parse hex/named color string to ARGB
// ============================================================================
//
//  Supports:
//    #RGB        — expand each nibble: R→RR, G→GG, B→BB, alpha=FF
//    #RRGGBB     — parse 3 hex pairs, alpha=FF
//    #RRGGBBAA   — parse 4 hex pairs
//    Named color — case-insensitive lookup (see kt_named_colors above)
//    Unrecognized — returns 0x00000000 (transparent)
//
//  Z3 UNSAT: kt-color-hex-nibble.smt2
// ============================================================================
uint32_t kt_color_parse_hex(const char* hex) {
    if (!hex || hex[0] == '\0')
        return 0x00000000;

    // ── Named color lookup (case-insensitive) ────────────────
    if (hex[0] != '#') {
        for (int i = 0; i < kt_named_color_count; i++) {
            if (str_equals_ci(hex, kt_named_colors[i].name))
                return kt_named_colors[i].color;
        }
        return 0x00000000;          // unrecognized → black
    }

    // ── #RRGGBB[AA] parse ────────────────────────────────────
    size_t len = strlen(hex + 1);   // length after '#'

    if (len == 3) {
        // #RGB → expand: 0xRRGGBBFF
        // BUG-014: validate each nibble; hex_nibble returns -1 for invalid chars
        int rn = hex_nibble(hex[1]);
        int gn = hex_nibble(hex[2]);
        int bn = hex_nibble(hex[3]);
        if (rn < 0 || gn < 0 || bn < 0)
            return 0x00000000;
        uint8_t r = (uint8_t)(rn * 16 + rn);  // R → RR (each nibble expanded)
        uint8_t g = (uint8_t)(gn * 16 + gn);
        uint8_t b = (uint8_t)(bn * 16 + bn);
        return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    if (len >= 6) {
        // BUG-014: validate each nibble before shifting; hex_nibble returns -1 for invalid
        int r_hi = hex_nibble(hex[1]);
        int r_lo = hex_nibble(hex[2]);
        int g_hi = hex_nibble(hex[3]);
        int g_lo = hex_nibble(hex[4]);
        int b_hi = hex_nibble(hex[5]);
        int b_lo = hex_nibble(hex[6]);
        if (r_hi < 0 || r_lo < 0 || g_hi < 0 || g_lo < 0 || b_hi < 0 || b_lo < 0)
            return 0x00000000;
        uint8_t r = (uint8_t)(r_hi << 4 | r_lo);
        uint8_t g = (uint8_t)(g_hi << 4 | g_lo);
        uint8_t b = (uint8_t)(b_hi << 4 | b_lo);
        uint8_t a = 0xFF;

        if (len >= 8) {
            // #RRGGBBAA
            int a_hi = hex_nibble(hex[7]);
            int a_lo = hex_nibble(hex[8]);
            if (a_hi < 0 || a_lo < 0) return 0x00000000;
            a = (uint8_t)(a_hi << 4 | a_lo);
        }

        return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    // Malformed (e.g. just "#" or "#X") → transparent
    return 0x00000000;
}

// ============================================================================
//  SECTION 3: kt_color_gradient_sample — Sample N-stop gradient at position x
// ============================================================================
//
//  Finds the segment containing x, interpolates with kt_color_lerp, and
//  returns packed uint32_t ARGB.
//
//  Search strategy:
//    n_stops <= 4   → linear scan   O(N)
//    n_stops >  4   → binary search O(log N)
//
//  Z3 UNSAT: kt-color-lerp.smt2
// ============================================================================
uint32_t kt_color_gradient_sample(const uint32_t* stops,
                                  const float*    positions,
                                  int             n_stops,
                                  float           x)
{
    // ── Degenerate cases ────────────────────────────────────
    if (n_stops <= 0 || !stops || !positions)
        return 0x00000000;

    if (n_stops == 1)
        return stops[0];

    // ── Clamp x to gradient domain ───────────────────────────
    if (x <= positions[0])
        return stops[0];

    if (x >= positions[n_stops - 1])
        return stops[n_stops - 1];

    // ── Find segment containing x ─────────────────────────────
    int i = 0;

    if (n_stops > 4) {
        // Binary search for the segment where positions[i] <= x < positions[i+1]
        int lo = 0;
        int hi = n_stops - 1;
        while (hi - lo > 1) {
            int mid = lo + (hi - lo) / 2;
            if (x < positions[mid])
                hi = mid;
            else
                lo = mid;
        }
        i = lo;
    } else {
        // Linear scan for small arrays (cache-friendly)
        for (i = 0; i < n_stops - 1; i++) {
            if (x <= positions[i + 1])
                break;
        }
        if (i >= n_stops - 1)
            i = n_stops - 2;       // safety: use last segment
    }

    // ── Interpolate ──────────────────────────────────────────
    float p0 = positions[i];
    float p1 = positions[i + 1];
    float t;

    // Guard against division by zero (coincident positions)
    if (p1 <= p0)
        return stops[i];

    t = (x - p0) / (p1 - p0);
    // t is guaranteed to be in [0, 1] because x was clamped above

    return kt_color_to_u32(
        kt_color_lerp(
            kt_color_from_u32(stops[i]),
            kt_color_from_u32(stops[i + 1]),
            t));
}
