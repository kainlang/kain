// ══════════════════════════════════════════════════════════════════════════
//  flexbox.c — Flexible box layout engine for the Kain native UI runtime
// ══════════════════════════════════════════════════════════════════════════
//  Single-pass O(n) flexbox layout: sizing (FIXED/GROW/PERCENT/FIT),
//  cross-axis alignment, main-axis justification, aspect-ratio constraint.
//
//  Algorithm derived from Clay (single-header C89 flexbox, MIT):
//    - sizing model: FIXED→PERCENT→GROW pipeline with min/max clamping
//    - alignment: cross-axis (START/CENTER/END/STRETCH) per-child
//    - justification: START/CENTER/END/SPACE_BETWEEN/SPACE_AROUND
//    - overflow: children exceeding container keep their size (caller clips)
//    - edge cases: zero children → padding-only box, negative space → 0
//
//  This file is a clean Kain-style rewrite. It does NOT copy Clay's code;
//  it extracts the algorithm and expresses it with flat structs and no macros.
// ══════════════════════════════════════════════════════════════════════════

#include "flexbox.h"
#include <string.h>
#include <math.h>

// ── Internal helpers ─────────────────────────────────────────────────────

// Clamp v to [lo, hi]. hi=0 means no upper bound.
static inline float flex_clamp(float v, float lo, float hi) {
    float r = v;
    if (r < lo) r = lo;
    if (hi > 0.0f && r > hi) r = hi;
    return r;
}

// Alias: clamp with the layout engine's field names (min/max)
static inline float flex_clamp_sz(float v, FlexboxSizing s) {
    return flex_clamp(v, s.min, s.max);
}

// Return max of two floats.
static inline float flex_max(float a, float b) {
    return (a > b) ? a : b;
}

// Return min of two floats.
static inline float flex_min(float a, float b) {
    return (a < b) ? a : b;
}

// ── Public: resolve a single sizing spec ────────────────────────────────

float flexbox_resolve_size(FlexboxSizing sizing, float available_space, float content_size) {
    float result;

    switch (sizing.type) {
        case FLEX_SIZING_FIXED:
            result = sizing.value;
            break;

        case FLEX_SIZING_GROW:
            // In standalone mode, GROW acts as a fraction of available space.
            result = available_space * sizing.value;
            break;

        case FLEX_SIZING_PERCENT:
            result = available_space * sizing.value;
            break;

        case FLEX_SIZING_FIT:
            result = content_size;
            break;

        default:
            result = 0.0f;
            break;
    }

    // Apply min/max constraints
    result = flex_clamp(result, sizing.min, sizing.max);
    return result;
}

// ── Public: compute layout for one container level ──────────────────────

FlexboxResult flexbox_compute_layout(
    float parent_w, float parent_h,
    const FlexboxConfig* config,
    const FlexboxConfig* child_configs, int child_count,
    FlexboxResult* out_results
) {
    int i;

    // ── 1. Zero-initialize outputs ─────────────────────────────────────
    if (child_count > 0 && out_results) {
        memset(out_results, 0, (size_t)child_count * sizeof(FlexboxResult));
    }

    // ── 2. Compute inner area (subtract padding) ──────────────────────
    float pad_left   = config->padding_left;
    float pad_right  = config->padding_right;
    float pad_top    = config->padding_top;
    float pad_bottom = config->padding_bottom;

    float inner_w = parent_w - pad_left - pad_right;
    float inner_h = parent_h - pad_top  - pad_bottom;
    if (inner_w < 0.0f) inner_w = 0.0f;
    if (inner_h < 0.0f) inner_h = 0.0f;

    bool is_row = (config->direction == FLEX_DIRECTION_ROW);
    float gap  = config->gap;
    int gap_count = (child_count > 0) ? child_count - 1 : 0;
    float total_gap = (float)gap_count * gap;

    // ── 3. Main-axis sizing: FIXED, PERCENT, FIT → GROW ───────────────
    // Phase 3a: resolve non-GROW children, collect GROW metadata

    float used_main  = 0.0f;   // space consumed by non-GROW children on main axis
    float avail_main = is_row ? inner_w : inner_h;

    // Indices of GROW children. 1024 is generous for any UI container.
    // The typical node limit is ABI_UI_MAX_NODES (4096), so this is safe.
    #define FLEX_GROW_MAX 1024
    int grow_indices[FLEX_GROW_MAX];
    int grow_count = 0;
    float grow_weight_sum = 0.0f;

    for (i = 0; i < child_count && i < FLEX_GROW_MAX; i++) {
        const FlexboxConfig* cc = &child_configs[i];
        const FlexboxSizing* ms = is_row ? &cc->width : &cc->height;
        float* out_m = is_row ? &out_results[i].width : &out_results[i].height;

        if (ms->type == FLEX_SIZING_GROW) {
            grow_indices[grow_count++] = i;
            grow_weight_sum += ms->value;
            continue;
        }

        float main_size = 0.0f;

        switch (ms->type) {
            case FLEX_SIZING_FIXED:
                main_size = ms->value;
                break;
            case FLEX_SIZING_PERCENT:
                main_size = avail_main * ms->value;
                break;
            case FLEX_SIZING_FIT:
                main_size = (ms->value >= 0.0f) ? ms->value : 0.0f;
                break;
            default:
                break;
        }

        main_size = flex_clamp(main_size, ms->min, ms->max);
        *out_m = main_size;
        used_main += main_size;
    }

    // Phase 3b: distribute remaining space to GROW children.
    // CLAY ALGORITHM: remaining = parent_inner - used_non_grow - total_gap

    float remaining = avail_main - used_main - total_gap;
    if (remaining < 0.0f) remaining = 0.0f;

    if (grow_count > 0 && remaining > 0.0f) {
        if (grow_weight_sum > 0.0f) {
            // Distribute proportionally by weight. Iterative redistribution
            // handles children hitting their max constraint.
            float redist_remaining = remaining;
            int redist_grow = grow_count;
            float redist_weight = grow_weight_sum;
            bool used_max[FLEX_GROW_MAX];
            memset(used_max, 0, sizeof(used_max));

            while (redist_remaining > 0.5f && redist_grow > 0) {
                float distributed = 0.0f;
                int next_grow = 0;
                float next_weight = 0.0f;

                for (int gi = 0; gi < grow_count; gi++) {
                    if (used_max[gi]) continue;
                    int idx = grow_indices[gi];
                    const FlexboxSizing* ms = is_row
                        ? &child_configs[idx].width
                        : &child_configs[idx].height;
                    float* out_m = is_row
                        ? &out_results[idx].width
                        : &out_results[idx].height;

                    float share = redist_remaining * (ms->value / redist_weight);
                    if (ms->max > 0.0f && share > ms->max) {
                        share = ms->max;
                        *out_m = share;
                        used_max[gi] = true;
                        distributed += share;
                    } else if (share < ms->min) {
                        share = ms->min;
                        *out_m = share;
                        used_max[gi] = true;
                        distributed += share;
                    } else {
                        *out_m = share;
                        distributed += share;
                        next_grow++;
                        next_weight += ms->value;
                    }
                }

                float leftover = redist_remaining - distributed;
                if (leftover > 0.5f && next_grow > 0 && leftover < redist_remaining) {
                    redist_remaining = leftover;
                    redist_grow = next_grow;
                    redist_weight = next_weight;
                } else {
                    break;  // no progress or all clamped
                }
            }
        } else {
            // Zero total weight: divide equally among GROW children
            float each = remaining / (float)grow_count;
            for (int gi = 0; gi < grow_count; gi++) {
                int idx = grow_indices[gi];
                const FlexboxSizing* ms = is_row
                    ? &child_configs[idx].width
                    : &child_configs[idx].height;
                float* out_m = is_row
                    ? &out_results[idx].width
                    : &out_results[idx].height;
                *out_m = flex_clamp(each, ms->min, ms->max);
            }
        }
    } else if (grow_count > 0) {
        // No remaining space: GROW children get their min size
        for (int gi = 0; gi < grow_count; gi++) {
            int idx = grow_indices[gi];
            const FlexboxSizing* ms = is_row
                ? &child_configs[idx].width
                : &child_configs[idx].height;
            float* out_m = is_row
                ? &out_results[idx].width
                : &out_results[idx].height;
            *out_m = ms->min;
        }
    }

    // ── 4. Cross-axis sizing ──────────────────────────────────────────
    // Each child resolves its cross-axis size from its config.
    // When the container's align is STRETCH, children fill available cross space.
    // The cross-axis "available" space is the container's inner size on that axis.
    //
    // CLAY ALGORITHM: non-layout axis sizing:
    //   - STRETCH container alignment → fill available cross space
    //   - GROW on cross axis → fill available cross space
    //   - FIXED → exact pixel size
    //   - PERCENT → fraction of cross available
    //   - FIT → content size

    float avail_cross = is_row ? inner_h : inner_w;
    bool stretch_cross = (config->align == FLEX_ALIGN_STRETCH);

    for (i = 0; i < child_count; i++) {
        const FlexboxConfig* cc = &child_configs[i];
        const FlexboxSizing* cs = is_row ? &cc->height : &cc->width;
        float* out_c = is_row ? &out_results[i].height : &out_results[i].width;

        if (stretch_cross) {
            // STRETCH container alignment: fill available cross space
            *out_c = avail_cross;
        } else {
            // Use explicit cross-axis sizing
            switch (cs->type) {
                case FLEX_SIZING_FIXED:
                    *out_c = cs->value;
                    break;
                case FLEX_SIZING_GROW:
                    // GROW on cross axis: fill available space
                    *out_c = avail_cross;
                    break;
                case FLEX_SIZING_PERCENT:
                    *out_c = avail_cross * cs->value;
                    break;
                case FLEX_SIZING_FIT:
                    *out_c = (cs->value >= 0.0f) ? cs->value : 0.0f;
                    break;
                default:
                    *out_c = 0.0f;
                    break;
            }

            // Apply min/max
            *out_c = flex_clamp(*out_c, cs->min, cs->max);
        }
    }

    // ── 5. Aspect-ratio constraint ────────────────────────────────────
    // If aspect_ratio > 0, the cross-axis size is derived from the main-axis size.
    // aspect_ratio = width / height.
    //
    // CLAY ALGORITHM: applied after both axes are sized; one axis is fixed,
    // the other is computed.

    for (i = 0; i < child_count; i++) {
        float ar = child_configs[i].aspect_ratio;
        if (ar > 0.0f) {
            if (is_row) {
                // Main axis = width, derive height from width
                out_results[i].height = out_results[i].width / ar;
            } else {
                // Main axis = height, derive width from height
                out_results[i].width = out_results[i].height * ar;
            }
        }
    }

    // ── 6. Main-axis positioning (justify) ────────────────────────────
    // Compute total used main-axis space (all children + gaps) to determine
    // how much extra space exists for justification.
    //
    // CLAY ALGORITHM: extraSpace = parent_inner - contentSize - padding
    //   - START:  children packed at start (no extra distribution)
    //   - CENTER: extra/2 before first child
    //   - END:    all extra before first child
    //   - SPACE_BETWEEN: gaps fill extra evenly between children
    //   - SPACE_AROUND:  gaps fill extra evenly including half at edges

    float total_children_main = 0.0f;
    for (i = 0; i < child_count; i++) {
        total_children_main += is_row ? out_results[i].width : out_results[i].height;
    }

    float extra = avail_main - total_children_main - total_gap;
    if (extra < 0.0f) extra = 0.0f;

    float cursor_main = 0.0f;
    float effective_gap = gap;

    switch (config->justify) {
        case FLEX_JUSTIFY_START:
        default:
            cursor_main = 0.0f;
            effective_gap = gap;
            break;

        case FLEX_JUSTIFY_CENTER:
            cursor_main = extra * 0.5f;
            effective_gap = gap;
            break;

        case FLEX_JUSTIFY_END:
            cursor_main = extra;
            effective_gap = gap;
            break;

        case FLEX_JUSTIFY_SPACE_BETWEEN:
            cursor_main = 0.0f;
            if (child_count > 1) {
                effective_gap = extra / (float)(child_count - 1);
            }
            break;

        case FLEX_JUSTIFY_SPACE_AROUND:
            if (child_count > 0) {
                float gap_unit = extra / (float)child_count;
                cursor_main = gap_unit * 0.5f;
                effective_gap = gap_unit;
            }
            break;
    }

    for (i = 0; i < child_count; i++) {
        float* out_m = is_row ? &out_results[i].x : &out_results[i].y;
        float child_m = is_row ? out_results[i].width : out_results[i].height;

        *out_m = cursor_main;

        // Advance cursor past this child and gap
        cursor_main += child_m + effective_gap;
    }

    // ── 7. Cross-axis positioning (align) ─────────────────────────────
    // Each child is positioned within the cross-axis space.
    // The container's `align` property controls alignment for all children.
    // Per-child alignment override is reserved for future use.
    //
    // CLAY ALGORITHM: whiteSpaceAroundChild = cross_available - child_cross_size
    //   - START:   child at cross-axis start
    //   - CENTER:  child offset by whiteSpaceAroundChild / 2
    //   - END:     child offset by whiteSpaceAroundChild
    //   - STRETCH: child sized to fill; position at start

    FlexboxAlignment container_align = config->align;
    for (i = 0; i < child_count; i++) {
        float* out_c = is_row ? &out_results[i].y : &out_results[i].x;
        float child_c = is_row ? out_results[i].height : out_results[i].width;

        switch (container_align) {
            case FLEX_ALIGN_START:
            default:
                *out_c = 0.0f;
                break;

            case FLEX_ALIGN_CENTER:
                *out_c = (avail_cross - child_c) * 0.5f;
                break;

            case FLEX_ALIGN_END:
                *out_c = avail_cross - child_c;
                break;

            case FLEX_ALIGN_STRETCH:
                *out_c = 0.0f;
                break;
        }

        // Guard against floating-point edge cases
        if (*out_c < 0.0f) *out_c = 0.0f;
    }

    // ── 8. Add padding offsets to all children ────────────────────────
    // Children are positioned relative to the container's inner content box.
    // We add the padding to shift them relative to the container's outer box.

    for (i = 0; i < child_count; i++) {
        out_results[i].x += pad_left;
        out_results[i].y += pad_top;
    }

    // ── 9. Return container bounds (children union + padding) ─────────
    // The returned box encompasses all children expanded by the container's
    // outer padding. For zero children, returns the padding-only box.
    //
    // The caller uses this to know the actual space occupied; the container's
    // own sizing (FIXED/GROW/PERCENT/FIT) is handled by the PARENT's layout call.

    FlexboxResult container;
    container.x = 0.0f;
    container.y = 0.0f;

    if (child_count == 0) {
        // No children: return padding box only
        container.width  = pad_left + pad_right;
        container.height = pad_top  + pad_bottom;
    } else {
        // Compute bounding box of all children + outer padding
        float min_x = out_results[0].x;
        float min_y = out_results[0].y;
        float max_x = out_results[0].x + out_results[0].width;
        float max_y = out_results[0].y + out_results[0].height;

        for (i = 1; i < child_count; i++) {
            float cx = out_results[i].x;
            float cy = out_results[i].y;
            float cw = out_results[i].width;
            float ch = out_results[i].height;

            if (cx < min_x) min_x = cx;
            if (cy < min_y) min_y = cy;
            if (cx + cw > max_x) max_x = cx + cw;
            if (cy + ch > max_y) max_y = cy + ch;
        }

        // The container's width includes the children's extent + right/bottom padding
        // (left/top padding is already included in min_x/min_y since we added pad_left/pad_top)
        float content_w = max_x - min_x;
        float content_h = max_y - min_y;

        // Adjust for the case where padding exceeds children extent
        container.width  = flex_max(content_w, pad_left + pad_right);
        container.height = flex_max(content_h, pad_top  + pad_bottom);
    }

    #undef FLEX_GROW_MAX
    return container;
}

// ══════════════════════════════════════════════════════════════════════════
//  flexbox_position_floating — Position a child at an attach point + offset
// ══════════════════════════════════════════════════════════════════════════
// Clay-derived algorithm: the child is positioned by matching one of 9 attach
// points on the parent box to the corresponding point on the child box, then
// shifting by (offset_x, offset_y).

FlexboxResult flexbox_position_floating(
    float parent_x, float parent_y, float parent_w, float parent_h,
    float child_w, float child_h,
    FlexboxAttachPoint attach, float offset_x, float offset_y
) {
    FlexboxResult r;
    r.width  = child_w;
    r.height = child_h;

    // Compute the child's top-left origin for the given attach point.
    // We first find where the child's CORNER (or center-edge) sits relative
    // to the parent, then offset.
    float x = 0.0f, y = 0.0f;

    switch (attach) {
        // ── TOP row ───────────────────────────────────────────────────
        case FLEX_ATTACH_TOP_LEFT:
            x = parent_x;
            y = parent_y;
            break;
        case FLEX_ATTACH_TOP_CENTER:
            x = parent_x + parent_w * 0.5f - child_w * 0.5f;
            y = parent_y;
            break;
        case FLEX_ATTACH_TOP_RIGHT:
            x = parent_x + parent_w - child_w;
            y = parent_y;
            break;

        // ── CENTER row ────────────────────────────────────────────────
        case FLEX_ATTACH_CENTER_LEFT:
            x = parent_x;
            y = parent_y + parent_h * 0.5f - child_h * 0.5f;
            break;
        case FLEX_ATTACH_CENTER:
            x = parent_x + parent_w * 0.5f - child_w * 0.5f;
            y = parent_y + parent_h * 0.5f - child_h * 0.5f;
            break;
        case FLEX_ATTACH_CENTER_RIGHT:
            x = parent_x + parent_w - child_w;
            y = parent_y + parent_h * 0.5f - child_h * 0.5f;
            break;

        // ── BOTTOM row ────────────────────────────────────────────────
        case FLEX_ATTACH_BOTTOM_LEFT:
            x = parent_x;
            y = parent_y + parent_h - child_h;
            break;
        case FLEX_ATTACH_BOTTOM_CENTER:
            x = parent_x + parent_w * 0.5f - child_w * 0.5f;
            y = parent_y + parent_h - child_h;
            break;
        case FLEX_ATTACH_BOTTOM_RIGHT:
            x = parent_x + parent_w - child_w;
            y = parent_y + parent_h - child_h;
            break;
    }

    // Apply offset (positive = right/down)
    r.x = x + offset_x;
    r.y = y + offset_y;

    return r;
}

// ══════════════════════════════════════════════════════════════════════════
//  flexbox_wrap_text — Word-wrapping text layout with measure callback
// ══════════════════════════════════════════════════════════════════════════
//  Algorithm (Clay-derived, without string mutation):
//    For each character in the input text:
//      1. Track word boundaries (space-delimited)
//      2. Measure accumulated text from line_start to current word_end
//      3. If it fits, add the word to the current line and continue
//      4. If it doesn't fit, emit the current line (without the new word)
//         and start a new line with the overflowing word
//      5. Newline characters force a line break
//      6. Single words wider than available_width are emitted alone
//
//  The measure callback receives a pointer + length pair, so no string
//  mutation is needed (unlike the naive null-termination approach).

int flexbox_wrap_text(
    const char* text, float available_width, float font_size,
    FlexboxMeasureTextFn measure, void* user_data,
    FlexboxResult* out_lines, int max_lines
) {
    if (!text || !measure || max_lines <= 0 || available_width <= 0.0f) {
        return 0;
    }

    int line_count = 0;
    float y = 0.0f;

    const char* p = text;

    while (*p && line_count < max_lines) {
        // ── Start of a new line ───────────────────────────────────────
        const char* line_start = p;

        // Skip leading spaces on continuation lines (but not the first line)
        if (line_count > 0) {
            while (*p == ' ') p++;
        }
        line_start = p;

        // ── Scan forward to find the best break point ─────────────────
        const char* best_break = p;       // last safe break position
        float line_width = 0.0f;          // width of content from line_start to best_break
        float height = 0.0f;

        // Measure a single character to get line height
        measure("A", 1, font_size, available_width, NULL, &height, user_data);
        if (height <= 0.0f) height = font_size * 1.2f;  // fallback line height

        int has_content = 0;

        while (*p && *p != '\n') {
            // Find end of current word (space-delimited)
            const char* word_end = p;
            while (*word_end && *word_end != ' ' && *word_end != '\n') {
                word_end++;
            }

            // Measure this word alone
            int word_len = (int)(word_end - p);
            float ww = 0.0f, wh = 0.0f;
            measure(p, word_len, font_size, available_width, &ww, &wh, user_data);

            // Measure the line including this word
            int line_len = (int)(word_end - line_start);
            float lw = 0.0f, lh = 0.0f;
            measure(line_start, line_len, font_size, available_width, &lw, &lh, user_data);

            // ── Decide whether the word fits ──────────────────────────
            if (lw <= available_width) {
                // Word fits on current line
                line_width = lw;
                best_break = word_end;
                has_content = 1;

                // Skip the word and any trailing spaces for the next scan
                p = word_end;
                while (*p == ' ') p++;
            } else if (!has_content) {
                // Single word is wider than available width.
                // Clay behavior: place it on its own line anyway (it overflows).
                line_width = ww;
                best_break = word_end;
                has_content = 1;
                p = word_end;
                while (*p == ' ') p++;
            } else {
                // Word doesn't fit — break here, word will start next line.
                break;
            }
        }

        // ── Handle newline as explicit break ──────────────────────────
        if (*p == '\n') {
            // Emit current line up to the newline
            if (has_content) {
                // Line already captured above; advance past newline
            } else {
                // Sizing for an intentional blank line
                line_width = 0.0f;
            }
            p++;  // consume the newline
        }

        // ── Emit the line ─────────────────────────────────────────────
        out_lines[line_count].x = 0.0f;
        out_lines[line_count].y = y;
        out_lines[line_count].width = line_width;
        out_lines[line_count].height = height;
        line_count++;
        y += height;

        // Advance p past the best break if we broke mid-line
        if (*p && *p != '\n' && best_break > line_start && best_break > p) {
            p = best_break;
            while (*p == ' ') p++;
        }
    }

    return line_count;
}
