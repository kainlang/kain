#ifndef KAIN_FLEXBOX_H
#define KAIN_FLEXBOX_H

// ══════════════════════════════════════════════════════════════════════════
//  flexbox.h — Flexible box layout engine for the Kain native UI runtime
// ══════════════════════════════════════════════════════════════════════════
//  Algorithm derived from Clay (single-header C89 flexbox layout, MIT).
//  Single-pass O(n) sizing + positioning: FIXED, GROW, PERCENT, FIT sizing,
//  cross-axis alignment, main-axis justification, aspect-ratio constraint.
//
//  Usage:
//    // Container-level config
//    FlexboxConfig parent = {
//        .direction = FLEX_DIRECTION_ROW,
//        .gap = 10.0f,
//        .justify = FLEX_JUSTIFY_START,
//        .align = FLEX_ALIGN_START,
//        .padding_left = 8, .padding_right = 8, .padding_top = 8, .padding_bottom = 8,
//    };
//
//    // Per-child config
//    FlexboxConfig children[2] = {
//        { .width = {FLEX_SIZING_GROW, 1, 0, 0}, .height = {FLEX_SIZING_FIXED, 50, 0, 0} },
//        { .width = {FLEX_SIZING_GROW, 2, 0, 0}, .height = {FLEX_SIZING_FIXED, 50, 0, 0} },
//    };
//
//    FlexboxResult results[2];
//    FlexboxResult container = flexbox_compute_layout(
//        400.0f, 100.0f, &parent, children, 2, results
//    );
//    // results[0] = {0, 8, 130, 50},  results[1] = {140, 8, 260, 50}
// ══════════════════════════════════════════════════════════════════════════

#include <stdint.h>
#include <stdbool.h>
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Sizing model (Clay-derived) ──────────────────────────────────────────
// FIXED:     exact pixel size
// GROW:      fills available space, weight-based sharing
// PERCENT:   fraction of parent container size (0.0 - 1.0)
// FIT:       wraps to content size (caller provides via .value)

typedef enum FlexboxSizingType {
    FLEX_SIZING_FIXED   = 0,
    FLEX_SIZING_GROW    = 1,
    FLEX_SIZING_PERCENT = 2,
    FLEX_SIZING_FIT     = 3,
} FlexboxSizingType;

typedef struct FlexboxSizing {
    FlexboxSizingType type;
    float value;             // FIXED = pixels, PERCENT = 0.0-1.0, GROW = weight, FIT = content size
    float min;               // minimum allowed size (pixels), applied after value resolution
    float max;               // maximum allowed size (pixels), 0 = unlimited
} FlexboxSizing;

// ── Layout direction ─────────────────────────────────────────────────────

typedef enum FlexboxDirection {
    FLEX_DIRECTION_ROW    = 0,    // left-to-right
    FLEX_DIRECTION_COLUMN = 1,    // top-to-bottom
} FlexboxDirection;

// Backward-compatible aliases (for existing call sites)
#define FLEX_DIR_ROW    FLEX_DIRECTION_ROW
#define FLEX_DIR_COLUMN FLEX_DIRECTION_COLUMN

// ── Cross-axis alignment (per-child override of container's align) ───────

typedef enum FlexboxAlignment {
    FLEX_ALIGN_START   = 0, // default: pack at cross-axis start
    FLEX_ALIGN_CENTER  = 1, // center within cross-axis space
    FLEX_ALIGN_END     = 2, // pack at cross-axis end
    FLEX_ALIGN_STRETCH = 3, // fill available cross-axis space (overrides cross-axis sizing)
} FlexboxAlignment;

// ── Main-axis justification (container-level) ────────────────────────────

typedef enum FlexboxJustify {
    FLEX_JUSTIFY_START         = 0, // pack at main-axis start
    FLEX_JUSTIFY_CENTER        = 1, // center within main-axis space
    FLEX_JUSTIFY_END           = 2, // pack at main-axis end
    FLEX_JUSTIFY_SPACE_BETWEEN = 3, // even gaps between children, no gap at edges
    FLEX_JUSTIFY_SPACE_AROUND  = 4, // even gaps including half-gap at each edge
} FlexboxJustify;

// ── Floating / absolute positioning (Clay-derived) ───────────────────────
// 9 attach points position a child relative to its parent's bounding box,
// plus an (offset_x, offset_y) shift in pixels. Combined with is_floating,
// this provides absolute positioning within any container.

typedef enum FlexboxAttachPoint {
    FLEX_ATTACH_TOP_LEFT,       // default
    FLEX_ATTACH_TOP_CENTER,
    FLEX_ATTACH_TOP_RIGHT,
    FLEX_ATTACH_CENTER_LEFT,
    FLEX_ATTACH_CENTER,
    FLEX_ATTACH_CENTER_RIGHT,
    FLEX_ATTACH_BOTTOM_LEFT,
    FLEX_ATTACH_BOTTOM_CENTER,
    FLEX_ATTACH_BOTTOM_RIGHT,
} FlexboxAttachPoint;

// ── Scroll container data ───────────────────────────────────────────────
// Describes the scroll state of a scrolling container. When clip_to_parent
// is true and scroll offsets are set, children are clipped and positioned
// with the scroll offset applied.

typedef struct FlexboxScrollData {
    float content_width;      // total scrollable content size (pixels)
    float content_height;
    float scroll_offset_x;    // current scroll offset from origin (pixels)
    float scroll_offset_y;
    bool  vertical;           // scrollable on vertical axis
    bool  horizontal;         // scrollable on horizontal axis
} FlexboxScrollData;

// ── Text wrapping callback ──────────────────────────────────────────────
// User-provided callback that measures a string of known length for the
// given font size and maximum width. Returns the rendered width and height
// for the text segment.
//
//   text      - pointer to the text to measure (not null-terminated)
//   length    - number of characters to measure
//   font_size - the font size in pixels
//   max_width - the maximum available width (can limit measurement)
//   out_w     - receives the measured width
//   out_h     - receives the measured height
//   user_data - opaque pointer passed through from the caller

typedef void (*FlexboxMeasureTextFn)(const char* text, int length, float font_size,
                                      float max_width, float* out_w, float* out_h,
                                      void* user_data);

// ── Layout config for one element (container or child) ──────────────────
// For the container: .direction, .gap, .align, .justify, .padding* control layout
// For each child: .width/.height sizing, .align, .aspect_ratio control positioning

typedef struct FlexboxConfig {
    FlexboxSizing     width;
    FlexboxSizing     height;
    FlexboxDirection  direction;        // only meaningful for containers
    FlexboxAlignment  align;            // cross-axis alignment (container default or per-child override)
    FlexboxJustify    justify;          // main-axis justification (container-level)
    float             padding_left;
    float             padding_right;
    float             padding_top;
    float             padding_bottom;
    float             gap;              // space between children (main axis)
    bool              wrap;             // multi-line layout (reserved, not yet implemented)
    float             aspect_ratio;     // 0 = no constraint; width / height

    // ── Floating / absolute positioning ───────────────────────────────
    bool              is_floating;      // if true, position via attach_point + offset
                                        //   (child does NOT participate in parent flex layout)
    FlexboxAttachPoint attach_point;    // attach location relative to parent bounding box
    float             offset_x;         // offset from attach point (pixels, positive = right/down)
    float             offset_y;
    int16_t           z_index;          // draw order (higher = on top of lower)
    bool              clip_to_parent;   // clip children to this element's bounding box

    // ── Scrolling ─────────────────────────────────────────────────────
    FlexboxScrollData scroll;           // scroll state (content size, offsets, enabled axes)

    // ── Text wrapping ─────────────────────────────────────────────────
    FlexboxMeasureTextFn measure_text;  // callback for measuring text segments
    void*               measure_user_data; // user data passed to measure callback
    const char*         text_content;   // text to wrap (if this is a text element)
    float               text_font_size; // font size for text
} FlexboxConfig;

// ── Layout result for one child element ──────────────────────────────────
// x, y are relative to the container's padding-box origin.
// width, height are the final resolved size.

typedef struct FlexboxResult {
    float x, y, width, height;
} FlexboxResult;

// ══════════════════════════════════════════════════════════════════════════
//  Public API
// ══════════════════════════════════════════════════════════════════════════

// Compute layout for one container with N children.
//   parent_w, parent_h — the container's available size (full box, before padding)
//   config             — the container's flexbox config (direction, gap, padding, justify, align)
//   child_configs      — array of N child sizing configs
//   child_count        — number of children
//   out_results        — receives the positions/sizes of all children (must hold N elements)
// Returns the total occupied bounds of the children + container padding.
//   For zero children, returns the padding-only box.
//   For non-empty children, returns the union of all child bounds + outer padding.
//
// The caller is responsible for recursing the tree — this function handles
// one container level with flat children.

FlexboxResult flexbox_compute_layout(
    float parent_w, float parent_h,
    const FlexboxConfig* config,
    const FlexboxConfig* child_configs, int child_count,
    FlexboxResult* out_results
);

// Resolve a single sizing spec against available space and content size.
//   sizing          — the sizing specification
//   available_space — parent inner space on this axis
//   content_size    — content size (used for FIT sizing; ignored for other types)
// Returns the resolved pixel size, clamped to [min, max].
//   FIXED:   returns sizing.value
//   GROW:    returns available_space * sizing.value  (weight as fraction; useful standalone)
//   PERCENT: returns available_space * sizing.value
//   FIT:     returns content_size

float flexbox_resolve_size(FlexboxSizing sizing, float available_space, float content_size);

// ── Floating / absolute positioning ─────────────────────────────────────
// Position a floating child within parent bounds using an attach point and offset.
// This is an absolute-positioning helper: the child's final position is
// determined solely by the parent box, attach point, child size, and offset
// — it does NOT participate in flex layout.
//
//   parent_x, parent_y, parent_w, parent_h — parent bounding box (screen space)
//   child_w, child_h                       — child's final size (already resolved)
//   attach                                 — which point on parent to attach to
//   offset_x, offset_y                     — pixel offset from the attach point
//
// Returns the positioned child bounds.

FlexboxResult flexbox_position_floating(
    float parent_x, float parent_y, float parent_w, float parent_h,
    float child_w, float child_h,
    FlexboxAttachPoint attach, float offset_x, float offset_y
);

// ── Text wrapping ───────────────────────────────────────────────────────
// Compute wrapped text layout: break a text string into lines that fit
// within available_width, using the provided measure callback.
//
//   text             — null-terminated UTF-8 text to wrap
//   available_width  — maximum line width in pixels
//   font_size        — font size for measurement
//   measure          — callback to measure text segments
//   user_data        — opaque pointer passed to measure callback
//   out_lines        — receives positioned line bounding boxes
//   max_lines        — capacity of out_lines array
//
// Returns the number of lines produced (<= max_lines). Each out_lines[i]
// has .x=0, .y = line_top, .width = measured content width, .height = line height.
// Caller should set out_lines[i].y from the returned line count to position
// them in vertical layout.
//
// Empty text or null measure returns 0. Text longer than max_lines is
// truncated (last line gets the remaining text).

int flexbox_wrap_text(
    const char* text, float available_width, float font_size,
    FlexboxMeasureTextFn measure, void* user_data,
    FlexboxResult* out_lines, int max_lines
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_FLEXBOX_H */
