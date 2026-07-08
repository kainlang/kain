// ============================================================================
//  ui_theme.c — Kain UI Theme System Implementation
//  ============================================================================
//  Default dark + light themes, loader, and env-var override API.
//
//  Dark theme values match the current Kain UI defaults exactly
//  (#1A1A24 backgrounds, #21D4A1 green accents, #E8E8F0 text).
//  Light theme is a clean inverted palette for readability.
//
//  Environment variable model:
//    KAIN_UI_THEME           → "dark" (default), "light", or .theme path
//    KAIN_UI_THEME_BG_PRIMARY → hex color override for any single field
// ============================================================================

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "ui_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

// ══════════════════════════════════════════════════════════════════════════
//  STATIC CONST DEFAULT THEMES
// ══════════════════════════════════════════════════════════════════════════
//  These are the canonical source of truth for all UI appearance values.
//  Code must never hardcode colors — always read from a UiTheme pointer.

// ── Dark theme (current Kain UI look) ─────────────────────────────────
//  Background: #1A1A24 navy-black
//  Accent:     #21D4A1 green / #4A90D9 blue
//  Text:       #E8E8F0 light gray
//  Surfaces:   layered dark blues (#252540 panels, #1E1E32 title bars)
static const UiTheme kDefaultDarkTheme = {
    .api_version = KAIN_UI_THEME_VERSION,

    // Backgrounds
    .bg_primary      = {0.102f, 0.102f, 0.141f, 1.0f},  // #1A1A24 — main window
    .bg_secondary    = {0.145f, 0.145f, 0.196f, 1.0f},  // #252540 — panels
    .bg_tertiary     = {0.180f, 0.180f, 0.282f, 1.0f},  // #2E2E48 — inputs, checkboxes
    .bg_overlay      = {0.100f, 0.100f, 0.140f, 0.95f}, // ~#191A23F2 — floating elements

    // Text
    .text_primary    = {0.910f, 0.910f, 0.941f, 1.0f},  // #E8E8F0 — body text
    .text_secondary  = {0.533f, 0.533f, 0.627f, 1.0f},  // #8888A0 — dim text
    .text_accent     = {0.400f, 0.700f, 0.900f, 1.0f},  // #66B3E6 — links, highlights
    .text_disabled   = {0.300f, 0.300f, 0.400f, 1.0f},  // #4D4D66 — disabled

    // Buttons
    .button_normal   = {0.188f, 0.188f, 0.314f, 1.0f},  // #303050
    .button_hover    = {0.251f, 0.251f, 0.408f, 1.0f},  // #404068
    .button_pressed  = {0.314f, 0.314f, 0.502f, 1.0f},  // #505080
    .button_text     = {0.910f, 0.910f, 0.941f, 1.0f},  // #E8E8F0

    // Accents
    .accent          = {0.129f, 0.831f, 0.631f, 1.0f},  // #21D4A1 — green accent
    .accent_hover    = {0.200f, 0.900f, 0.700f, 1.0f},  // #33E6B3 — lighter green
    .error           = {0.910f, 0.290f, 0.373f, 1.0f},  // #E84A5F — red / close
    .warning         = {0.910f, 0.710f, 0.102f, 1.0f},  // #E8B51A — amber
    .success         = {0.200f, 0.800f, 0.200f, 1.0f},  // #33CC33 — green

    // Slider / Progress
    .slider_track    = {0.227f, 0.227f, 0.361f, 1.0f},  // #3A3A5C — track, progress bg
    .slider_fill     = {0.290f, 0.565f, 0.886f, 1.0f},  // #4A90D9 — blue fill
    .slider_thumb    = {0.129f, 0.831f, 0.631f, 1.0f},  // #21D4A1 — green handle

    // Borders, separators, overlays
    .border          = {0.227f, 0.227f, 0.361f, 1.0f},  // #3A3A5C — default border
    .separator       = {0.180f, 0.180f, 0.290f, 1.0f},  // #2E2E4A — divider line
    .scrim           = {0.000f, 0.000f, 0.000f, 0.33f}, // #00000054 — modal backdrop

    // Sizing
    .corner_radius   = 6.0f,
    .border_width    = 1.0f,
};

// ── Light theme ───────────────────────────────────────────────────────
//  Background: #F2F2F5 off-white
//  Accent:     #0080CC blue
//  Text:       #1A1A1F near-black
static const UiTheme kDefaultLightTheme = {
    .api_version = KAIN_UI_THEME_VERSION,

    // Backgrounds
    .bg_primary      = {0.950f, 0.950f, 0.960f, 1.0f},  // #F2F2F5
    .bg_secondary    = {0.900f, 0.900f, 0.920f, 1.0f},  // #E6E6EB
    .bg_tertiary     = {0.850f, 0.850f, 0.880f, 1.0f},  // #D9D9E0
    .bg_overlay      = {0.950f, 0.950f, 0.960f, 0.95f}, // ~#F2F2F5F2

    // Text
    .text_primary    = {0.100f, 0.100f, 0.120f, 1.0f},  // #1A1A1F
    .text_secondary  = {0.400f, 0.400f, 0.450f, 1.0f},  // #666673
    .text_accent     = {0.000f, 0.400f, 0.700f, 1.0f},  // #0066B3
    .text_disabled   = {0.600f, 0.600f, 0.650f, 1.0f},  // #9999A6

    // Buttons
    .button_normal   = {0.850f, 0.850f, 0.870f, 1.0f},  // #D9D9DD
    .button_hover    = {0.800f, 0.800f, 0.830f, 1.0f},  // #CCCCD4
    .button_pressed  = {0.750f, 0.750f, 0.780f, 1.0f},  // #BFBFC7
    .button_text     = {0.100f, 0.100f, 0.120f, 1.0f},  // #1A1A1F

    // Accents
    .accent          = {0.000f, 0.500f, 0.800f, 1.0f},  // #0080CC
    .accent_hover    = {0.000f, 0.600f, 0.900f, 1.0f},  // #0099E6
    .error           = {0.800f, 0.100f, 0.100f, 1.0f},  // #CC1A1A
    .warning         = {0.800f, 0.600f, 0.000f, 1.0f},  // #CC9900
    .success         = {0.100f, 0.700f, 0.100f, 1.0f},  // #1AB31A

    // Slider / Progress
    .slider_track    = {0.750f, 0.750f, 0.780f, 1.0f},  // #BFBFC7
    .slider_fill     = {0.000f, 0.500f, 0.800f, 1.0f},  // #0080CC
    .slider_thumb    = {0.000f, 0.500f, 0.800f, 1.0f},  // #0080CC

    // Borders, separators, overlays
    .border          = {0.650f, 0.650f, 0.680f, 1.0f},  // #A6A6AD
    .separator       = {0.700f, 0.700f, 0.730f, 1.0f},  // #B3B3BA
    .scrim           = {0.000f, 0.000f, 0.000f, 0.20f}, // #00000033

    // Sizing
    .corner_radius   = 6.0f,
    .border_width    = 1.0f,
};

// ══════════════════════════════════════════════════════════════════════════
//  BUILT-IN THEME ACCESSORS
// ══════════════════════════════════════════════════════════════════════════

const UiTheme* ui_theme_dark(void) {
    return &kDefaultDarkTheme;
}

const UiTheme* ui_theme_light(void) {
    return &kDefaultLightTheme;
}

// ══════════════════════════════════════════════════════════════════════════
//  WORKING THEME (mutable copy for overrides)
// ══════════════════════════════════════════════════════════════════════════
//  ui_theme_load() copies the selected base theme into this struct,
//  then applies any KAIN_UI_THEME_* environment variable overrides.
//  Subsequent calls to ui_theme_apply_override() also modify this copy.

static UiTheme s_working_theme;
static bool    s_working_initialized = false;

// ── Field type enum (for the override lookup table) ────────────────────

typedef enum {
    FIELD_TYPE_COLOR,   // kainColor field — parsed from hex string
    FIELD_TYPE_FLOAT,   // float field — parsed from numeric string
} ThemeFieldType;

// ── Field lookup table ────────────────────────────────────────────────
//  Maps environment variable suffix → (type, offset) for all overridable
//  theme fields. This is the single source of truth for which fields
//  can be overridden at runtime.

typedef struct {
    const char*    suffix;   // Env-var suffix after "KAIN_UI_THEME_"
    ThemeFieldType type;     // COLOR or FLOAT
    size_t         offset;   // offsetof(UiTheme, field)
} ThemeFieldEntry;

static const ThemeFieldEntry kThemeFieldTable[] = {
    // Backgrounds
    {"BG_PRIMARY",      FIELD_TYPE_COLOR, offsetof(UiTheme, bg_primary)},
    {"BG_SECONDARY",    FIELD_TYPE_COLOR, offsetof(UiTheme, bg_secondary)},
    {"BG_TERTIARY",     FIELD_TYPE_COLOR, offsetof(UiTheme, bg_tertiary)},
    {"BG_OVERLAY",      FIELD_TYPE_COLOR, offsetof(UiTheme, bg_overlay)},
    // Text
    {"TEXT_PRIMARY",    FIELD_TYPE_COLOR, offsetof(UiTheme, text_primary)},
    {"TEXT_SECONDARY",  FIELD_TYPE_COLOR, offsetof(UiTheme, text_secondary)},
    {"TEXT_ACCENT",     FIELD_TYPE_COLOR, offsetof(UiTheme, text_accent)},
    {"TEXT_DISABLED",   FIELD_TYPE_COLOR, offsetof(UiTheme, text_disabled)},
    // Buttons
    {"BUTTON_NORMAL",   FIELD_TYPE_COLOR, offsetof(UiTheme, button_normal)},
    {"BUTTON_HOVER",    FIELD_TYPE_COLOR, offsetof(UiTheme, button_hover)},
    {"BUTTON_PRESSED",  FIELD_TYPE_COLOR, offsetof(UiTheme, button_pressed)},
    {"BUTTON_TEXT",     FIELD_TYPE_COLOR, offsetof(UiTheme, button_text)},
    // Accents
    {"ACCENT",          FIELD_TYPE_COLOR, offsetof(UiTheme, accent)},
    {"ACCENT_HOVER",    FIELD_TYPE_COLOR, offsetof(UiTheme, accent_hover)},
    {"ERROR",           FIELD_TYPE_COLOR, offsetof(UiTheme, error)},
    {"WARNING",         FIELD_TYPE_COLOR, offsetof(UiTheme, warning)},
    {"SUCCESS",         FIELD_TYPE_COLOR, offsetof(UiTheme, success)},
    // Slider / Progress
    {"SLIDER_TRACK",    FIELD_TYPE_COLOR, offsetof(UiTheme, slider_track)},
    {"SLIDER_FILL",     FIELD_TYPE_COLOR, offsetof(UiTheme, slider_fill)},
    {"SLIDER_THUMB",    FIELD_TYPE_COLOR, offsetof(UiTheme, slider_thumb)},
    // Borders
    {"BORDER",          FIELD_TYPE_COLOR, offsetof(UiTheme, border)},
    {"SEPARATOR",       FIELD_TYPE_COLOR, offsetof(UiTheme, separator)},
    {"SCRIM",           FIELD_TYPE_COLOR, offsetof(UiTheme, scrim)},
    // Sizing
    {"CORNER_RADIUS",   FIELD_TYPE_FLOAT, offsetof(UiTheme, corner_radius)},
    {"BORDER_WIDTH",    FIELD_TYPE_FLOAT, offsetof(UiTheme, border_width)},
};

static const int kThemeFieldCount = (int)(sizeof(kThemeFieldTable) / sizeof(kThemeFieldTable[0]));

// ══════════════════════════════════════════════════════════════════════════
//  COLOR PARSING (self-contained hex → kainColor)
// ══════════════════════════════════════════════════════════════════════════
//  Parses "#RGB", "#RRGGBB", and "#RRGGBBAA" hex strings into a float-
//  based kainColor. Returns transparent (0,0,0,0) on parse failure.

kainColor ui_theme_parse_color(const char* hex) {
    if (!hex || hex[0] != '#') return KAIN_COLOR_TRANSPARENT;

    const char* p = hex + 1;
    int len = (int)strlen(p);

    // Validate length
    if (len != 3 && len != 6 && len != 8) return KAIN_COLOR_TRANSPARENT;

    // Validate hex characters
    for (int i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)p[i])) return KAIN_COLOR_TRANSPARENT;
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

    return kain_color_rgba(
        (float)r / 255.0f,
        (float)g / 255.0f,
        (float)b / 255.0f,
        (float)a / 255.0f
    );
}

// ══════════════════════════════════════════════════════════════════════════
//  ENV-VAR FIELD LOOKUP
// ══════════════════════════════════════════════════════════════════════════

// The prefix stripped from env-var keys in ui_theme_apply_override
#define KAIN_UI_THEME_PREFIX "KAIN_UI_THEME_"
#define KAIN_UI_THEME_PREFIX_LEN 14  // strlen("KAIN_UI_THEME_")

// Find a field entry by its env-var suffix (case-insensitive).
// Returns index into kThemeFieldTable, or -1 if not found.
static int find_field_by_suffix(const char* suffix) {
    for (int i = 0; i < kThemeFieldCount; i++) {
        if (strcmp(suffix, kThemeFieldTable[i].suffix) == 0) {
            return i;
        }
    }
    // Case-insensitive fallback for user convenience
    for (int i = 0; i < kThemeFieldCount; i++) {
        const char* a = suffix;
        const char* b = kThemeFieldTable[i].suffix;
        while (*a && *b) {
            if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) break;
            a++; b++;
        }
        if (*a == '\0' && *b == '\0') return i;
    }
    return -1;
}

// ══════════════════════════════════════════════════════════════════════════
//  SINGLE OVERRIDE APPLICATION
// ══════════════════════════════════════════════════════════════════════════

// Apply an override to the working theme.
// The working theme must be initialized (s_working_initialized == true).
static bool apply_override_unchecked(const char* suffix, const char* value) {
    int idx = find_field_by_suffix(suffix);
    if (idx < 0) return false;

    const ThemeFieldEntry* entry = &kThemeFieldTable[idx];
    char* field_base = (char*)&s_working_theme;

    if (entry->type == FIELD_TYPE_COLOR) {
        kainColor parsed = ui_theme_parse_color(value);
        // Accept any non-zero-alpha parse OR an explicit transparent parse
        // (check that the first hex digit after '#' is valid to avoid
        // treating invalid strings as transparent)
        if (parsed.a == 0.0f && parsed.r == 0.0f && parsed.g == 0.0f && parsed.b == 0.0f) {
            // Could be genuine transparent (#00000000) or parse failure.
            // Accept if the input starts with a valid-looking hex pattern.
            if (value[0] == '#' && isxdigit((unsigned char)value[1])) {
                // Accept transparent
            } else {
                return false;
            }
        }
        *(kainColor*)(field_base + entry->offset) = parsed;
        return true;
    }

    if (entry->type == FIELD_TYPE_FLOAT) {
        char* end = NULL;
        float fval = (float)strtod(value, &end);
        if (end == value || (*end != '\0' && !isspace((unsigned char)*end))) {
            return false;  // No digits parsed or trailing junk
        }
        *(float*)(field_base + entry->offset) = fval;
        return true;
    }

    return false;
}

// ── Apply all KAIN_UI_THEME_* environment variable overrides ──────────
//  Iterates the field table, checks each env var, and applies if set.
//  Called by ui_theme_load() after copying the base theme.
static void apply_all_env_overrides(void) {
    for (int i = 0; i < kThemeFieldCount; i++) {
        char env_name[128];
        int n = snprintf(env_name, sizeof(env_name), "%s%s",
                         KAIN_UI_THEME_PREFIX, kThemeFieldTable[i].suffix);
        if (n < 0 || n >= (int)sizeof(env_name)) continue;

        const char* val = getenv(env_name);
        if (val && val[0] != '\0') {
            if (!apply_override_unchecked(kThemeFieldTable[i].suffix, val)) {
                fprintf(stderr, "ui_theme: warning — failed to parse %s=\"%s\"\n", env_name, val);
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  PUBLIC API
// ══════════════════════════════════════════════════════════════════════════

const UiTheme* ui_theme_load(void) {
    const char* env = getenv("KAIN_UI_THEME");
    const UiTheme* base = &kDefaultDarkTheme;

    if (env && env[0] != '\0') {
        if (strcmp(env, "light") == 0) {
            base = &kDefaultLightTheme;
        } else if (strcmp(env, "dark") == 0) {
            base = &kDefaultDarkTheme;
        } else if (strstr(env, ".theme") != NULL) {
            // Future: .theme file loading
            fprintf(stderr, "ui_theme: .theme file loading not yet implemented ('%s'), using dark\n", env);
            base = &kDefaultDarkTheme;
        } else {
            fprintf(stderr, "ui_theme: unknown theme '%s', using dark\n", env);
            base = &kDefaultDarkTheme;
        }
    }

    // Copy base theme into the mutable working copy
    memcpy(&s_working_theme, base, sizeof(UiTheme));
    s_working_initialized = true;

    // Apply individual env-var overrides (KAIN_UI_THEME_BG_PRIMARY, etc.)
    apply_all_env_overrides();

    return &s_working_theme;
}

bool ui_theme_apply_override(const char* key, const char* value) {
    if (!key || !value || value[0] == '\0') return false;

    // Ensure the working copy exists
    if (!s_working_initialized) {
        memcpy(&s_working_theme, &kDefaultDarkTheme, sizeof(UiTheme));
        s_working_initialized = true;
    }

    // Strip "KAIN_UI_THEME_" prefix
    const char* suffix = key;
    if (strncmp(key, KAIN_UI_THEME_PREFIX, KAIN_UI_THEME_PREFIX_LEN) == 0) {
        suffix = key + KAIN_UI_THEME_PREFIX_LEN;
    }
    // Also accept unprefixed field names (e.g. "BG_PRIMARY" without prefix)
    // for programmatic callers who don't want to repeat the prefix

    return apply_override_unchecked(suffix, value);
}
