#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/ui_color.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ── Hex parsing ────────────────────────────────────────────────────────

uint32_t ui_parse_color_hex(const char* hex) {
    if (!hex || hex[0] != '#') return 0;

    const char* p = hex + 1;
    int len = (int)strlen(p);
    if (len != 3 && len != 6 && len != 8) return 0;

    // Validate all hex chars
    int i;
    for (i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)p[i])) return 0;
    }

    unsigned int r = 0, g = 0, b = 0, a = 255;

    if (len == 3) {
        // #RGB → #RRGGBB (expand each nibble)
        sscanf(p, "%1x%1x%1x", &r, &g, &b);
        r = r * 17;  // 0xF → 0xFF
        g = g * 17;
        b = b * 17;
    } else if (len == 6) {
        sscanf(p, "%2x%2x%2x", &r, &g, &b);
    } else if (len == 8) {
        sscanf(p, "%2x%2x%2x%2x", &r, &g, &b, &a);
    }

    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// ── RGB / RGBA function notation ───────────────────────────────────────

uint32_t ui_parse_color_rgb(const char* text) {
    if (!text) return 0;

    int is_rgba = 0;
    const char* prefix;
    if (strncmp(text, "rgba(", 5) == 0) {
        is_rgba = 1;
        prefix = text + 5;
    } else if (strncmp(text, "rgb(", 4) == 0) {
        prefix = text + 4;
    } else {
        return 0;
    }

    double vals[4] = {0, 0, 0, 1.0};
    int count = 0;
    const char* p = prefix;

    while (*p && count < 4) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t') p++;

        // Parse number
        char* end;
        double v = strtod(p, &end);
        if (end == p) break;  // no number parsed

        // If followed by '%', treat as percentage
        if (*end == '%') {
            v = (v / 100.0) * 255.0;
            end++;
        } else if (count == 3) {
            // Alpha: if no %, treat as 0.0-1.0 (but if >1, treat as 0-255)
            if (v > 1.0) v = v / 255.0;
        } else {
            // RGB channels: treat as 0-255
            // already correct
        }

        vals[count] = v;
        count++;
        p = end;

        // Skip whitespace and comma or slash
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ',' || *p == '/') p++;
    }

    if (is_rgba && count < 4) return 0;
    if (!is_rgba && count < 3) return 0;

    int r = (int)(vals[0] + 0.5);
    int g = (int)(vals[1] + 0.5);
    int b = (int)(vals[2] + 0.5);
    int a = is_rgba ? (int)(vals[3] * 255.0 + 0.5) : 255;

    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    if (a < 0) a = 0; if (a > 255) a = 255;

    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// ── Named colors ───────────────────────────────────────────────────────

typedef struct {
    const char* name;
    uint32_t color;
} NamedColor;

static const NamedColor named_colors[] = {
    {"transparent",     0x00000000},
    {"black",           0xFF000000},
    {"white",           0xFFFFFFFF},
    {"red",             0xFFFF0000},
    {"green",           0xFF008000},
    {"blue",            0xFF0000FF},
    {"yellow",          0xFFFFFF00},
    {"cyan",            0xFF00FFFF},
    {"magenta",         0xFFFF00FF},
    {"gray",            0xFF808080},
    {"grey",            0xFF808080},
    {"silver",          0xFFC0C0C0},
    {"maroon",          0xFF800000},
    {"purple",          0xFF800080},
    {"navy",            0xFF000080},
    {"teal",            0xFF008080},
    {"olive",           0xFF808000},
    {"lime",            0xFF00FF00},
    {"orange",          0xFFFFA500},
    {"pink",            0xFFFFC0CB},
    {"brown",           0xFFA52A2A},
    {"gold",            0xFFFFD700},
    {"coral",           0xFFFF7F50},
    {"salmon",          0xFFFA8072},
    {"turquoise",       0xFF40E0D0},
    {"indigo",          0xFF4B0082},
    {"violet",          0xFFEE82EE},
    {"tan",             0xFFD2B48C},
    {"ivory",           0xFFFFFFF0},
    {"azure",           0xFFF0FFFF},
    {"lavender",        0xFFE6E6FA},
    {"khaki",           0xFFF0E68C},
    {"crimson",         0xFFDC143C},
    {"chocolate",       0xFFD2691E},
    {"darkgray",        0xFFA9A9A9},
    {"darkgrey",        0xFFA9A9A9},
    {"lightgray",       0xFFD3D3D3},
    {"lightgrey",       0xFFD3D3D3},
    {"dimgray",         0xFF696969},
    {"dimgrey",         0xFF696969},
    {"slategray",       0xFF708090},
    {"slategrey",       0xFF708090},
    {NULL, 0}
};

uint32_t ui_parse_color_named(const char* name) {
    if (!name) return 0;

    // Lowercase for comparison
    char lower[64];
    int i;
    for (i = 0; name[i] && i < 63; i++) {
        lower[i] = (char)tolower((unsigned char)name[i]);
    }
    lower[i] = '\0';

    for (i = 0; named_colors[i].name; i++) {
        if (strcmp(lower, named_colors[i].name) == 0) {
            return named_colors[i].color;
        }
    }
    return 0;
}

// ── Top-level dispatch ─────────────────────────────────────────────────

uint32_t ui_parse_color(const char* text) {
    if (!text || !text[0]) return 0;

    if (text[0] == '#') {
        return ui_parse_color_hex(text);
    }
    if (strncmp(text, "rgb", 3) == 0) {
        return ui_parse_color_rgb(text);
    }
    return ui_parse_color_named(text);
}

// ── Component accessors ────────────────────────────────────────────────

uint8_t ui_color_r(uint32_t color) { return (uint8_t)((color >> 16) & 0xFF); }
uint8_t ui_color_g(uint32_t color) { return (uint8_t)((color >> 8)  & 0xFF); }
uint8_t ui_color_b(uint32_t color) { return (uint8_t)((color)       & 0xFF); }
uint8_t ui_color_a(uint32_t color) { return (uint8_t)((color >> 24) & 0xFF); }

// ── Alpha blending (src OVER dst, straight alpha) ─────────────────────

uint32_t ui_color_blend(uint32_t src, uint32_t dst) {
    uint8_t sa = ui_color_a(src);
    if (sa == 0) return dst;
    if (sa == 255) return src;

    uint8_t sr = ui_color_r(src), sg = ui_color_g(src), sb = ui_color_b(src);
    uint8_t dr = ui_color_r(dst), dg = ui_color_g(dst), db = ui_color_b(dst);

    int inv_a = 255 - sa;
    uint8_t r = (uint8_t)((sr * sa + dr * inv_a) / 255);
    uint8_t g = (uint8_t)((sg * sa + dg * inv_a) / 255);
    uint8_t b = (uint8_t)((sb * sa + db * inv_a) / 255);

    return ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ── Opacity application ────────────────────────────────────────────────

uint32_t ui_color_with_opacity(uint32_t color, double opacity) {
    if (opacity >= 1.0) return color;
    if (opacity <= 0.0) return 0;
    uint8_t a = ui_color_a(color);
    uint8_t new_a = (uint8_t)(a * opacity + 0.5);
    return (color & 0x00FFFFFF) | ((uint32_t)new_a << 24);
}
