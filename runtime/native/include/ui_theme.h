// ============================================================================
//  ui_theme.h — Kain UI Theme System
//  ============================================================================
//  Data-driven theme struct and API for the Kain native UI runtime.
//  All appearance values live in a flat `UiTheme` struct — no macros, no
//  hardcoded inline colors, no code duplication.
//
//  Design (inspired by clay.h):
//    • All appearance is DATA (struct fields), not code (macros/constants)
//    • Theme struct is flat — no inheritance, no cascading, no path resolution
//    • Dark + Light built-in themes provided as static const defaults
//    • Environment variable overrides via KAIN_UI_THEME_* prefixed vars
//    • Zero dependencies beyond stdint/stdbool and kain_geometry.h
//
//  Usage:
//    const UiTheme* theme = ui_theme_load();  // checks KAIN_UI_THEME env var
//    render_button(theme->button_normal, ...);
//
//  Environment overrides:
//    set KAIN_UI_THEME=light          → use built-in light theme
//    set KAIN_UI_THEME_BG_PRIMARY="#2A2A35"  → override single color
// ============================================================================

#ifndef KAIN_UI_THEME_H
#define KAIN_UI_THEME_H

#include <stdint.h>
#include <stdbool.h>
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Version guard (increase when struct layout changes) ────────────────
#define KAIN_UI_THEME_VERSION 1

// ══════════════════════════════════════════════════════════════════════════
//  UiTheme — One flat struct with all UI appearance values
// ══════════════════════════════════════════════════════════════════════════
//  Every color, radius, and sizing value that controls the look of
//  Kain UI components lives here. No hardcoded #defines, no scattered
//  literals — all code reads from an instance of this struct.
//
//  Fields are organized by role (backgrounds, text, buttons, etc.) so
//  that theme authors can quickly find and customize what they need.
// ============================================================================

typedef struct UiTheme {
    // ── Schema version (must equal KAIN_UI_THEME_VERSION) ──────────────
    int32_t api_version;

    // ── Background / Surface colors ─────────────────────────────────────
    kainColor bg_primary;      // Main window background
    kainColor bg_secondary;    // Panels, sidebars, surfaces below content
    kainColor bg_tertiary;     // Input fields, text areas, nested surfaces
    kainColor bg_overlay;      // Floating elements, dialogs, tooltips

    // ── Text colors ────────────────────────────────────────────────────
    kainColor text_primary;    // Main body text
    kainColor text_secondary;  // Dimmed / helper / secondary text
    kainColor text_accent;     // Links, highlighted terms, active labels
    kainColor text_disabled;   // Disabled controls, placeholder text

    // ── Button colors ──────────────────────────────────────────────────
    kainColor button_normal;   // Default (idle) button fill
    kainColor button_hover;    // Hovered button fill
    kainColor button_pressed;  // Pressed button fill
    kainColor button_text;     // Button label text

    // ── Semantic accent colors ─────────────────────────────────────────
    kainColor accent;          // Selection, focus rings, active indicator
    kainColor accent_hover;    // Accent variant for hover states
    kainColor error;           // Error / destructive / close button
    kainColor warning;         // Warning / caution indicator
    kainColor success;         // Success / positive indicator

    // ── Slider / Progress colors ───────────────────────────────────────
    kainColor slider_track;    // Slider background track, progress bg
    kainColor slider_fill;     // Slider filled / active portion
    kainColor slider_thumb;    // Slider drag handle

    // ── Borders, separators, overlays ──────────────────────────────────
    kainColor border;          // Default border for panels, buttons, inputs
    kainColor separator;       // Thin dividing line between sections
    kainColor scrim;           // Translucent overlay (modal backdrop)

    // ── Sizing (theme-controlled layout constants) ──────────────────────
    float     corner_radius;   // Default corner radius for buttons, panels
    float     border_width;    // Default border stroke width

} UiTheme;

// ══════════════════════════════════════════════════════════════════════════
//  Built-in theme accessors
// ══════════════════════════════════════════════════════════════════════════

// Returns the built-in dark theme (current Kain UI defaults).
// Thread-safe: returns pointer to a static const struct.
const UiTheme* ui_theme_dark(void);

// Returns the built-in light theme (inverted, clean, readable).
// Thread-safe: returns pointer to a static const struct.
const UiTheme* ui_theme_light(void);

// ══════════════════════════════════════════════════════════════════════════
//  Theme loading and override API
// ══════════════════════════════════════════════════════════════════════════

// Load the active theme based on the KAIN_UI_THEME environment variable.
//
//   KAIN_UI_THEME=     | Result
//   -------------------|---------------------------------------------------
//   unset / empty      | dark theme
//   "dark"             | dark theme
//   "light"            | light theme
//   <path>.theme       | (future) load from file; currently falls back to dark
//   anything else      | dark theme with a warning printed to stderr
//
// The returned pointer aliases a mutable working copy that subsequent
// calls to ui_theme_apply_override() will modify. The pointer remains
// valid for the lifetime of the process.
const UiTheme* ui_theme_load(void);

// Apply a single theme field override by environment-variable key.
//
//   key    — Full env-var name, e.g. "KAIN_UI_THEME_BG_PRIMARY"
//   value  — Hex color string, e.g. "#2A2A35"
//
// Supported value formats:
//   "#RRGGBB"     → fully opaque color
//   "#RRGGBBAA"   → color with alpha
//
// Returns true on success, false if the key is unknown or value
// cannot be parsed.
bool ui_theme_apply_override(const char* key, const char* value);

// Parse a hex color string into a kainColor.
//
// Accepts:
//   "#RRGGBB"     → alpha defaults to 1.0
//   "#RRGGBBAA"   → explicit alpha
//   "#RGB"        → 4-bit per channel, expanded (e.g. "#F0F" → "#FF00FF")
//
// Returns KAIN_COLOR_TRANSPARENT (0,0,0,0) on parse failure or
// for genuine transparent input — callers cannot distinguish the two
// cases at this level.
kainColor ui_theme_parse_color(const char* hex);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_UI_THEME_H */
