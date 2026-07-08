#include "ui_system_internal.h"
#include "layout_engine.h"
#include "flexbox.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

// ══════════════════════════════════════════════════════════════════════════
//  Internal helpers — style value readers
// ══════════════════════════════════════════════════════════════════════════
//  These mirror the patterns from ui_layout.c: hash-based style lookup
//  (Z3-proven ~1.07 probes at typical load factors) with clean fallback.
// ══════════════════════════════════════════════════════════════════════════

/// Read a floating-point style value from a node, returning fallback if absent or wrong type.
static double layout_style_f64(KainNativeUiSession* s, int64_t node_id, const char* key, double fallback) {
    KainNativeUiStyleRecord* r = abi_ui_find_style(s, node_id, key);
    return (r && r->value_kind == ABI_UI_STYLE_F64) ? r->f64_value : fallback;
}

/// Read an integer style value from a node, returning fallback if absent or wrong type.
static int64_t layout_style_i64(KainNativeUiSession* s, int64_t node_id, const char* key, int64_t fallback) {
    KainNativeUiStyleRecord* r = abi_ui_find_style(s, node_id, key);
    return (r && r->value_kind == ABI_UI_STYLE_I64) ? r->i64_value : fallback;
}

// ══════════════════════════════════════════════════════════════════════════
//  Sizing conversion conventions
// ══════════════════════════════════════════════════════════════════════════
//
//  The flexbox solver's FlexboxSizing type is an enum + value convention:
//
//    type == FLEX_SIZING_FIXED  →  value is the explicit pixel size
//    type == FLEX_SIZING_GROW   →  value is the grow weight (1.0 = equal share)
//    type == FLEX_SIZING_FIT    →  content-sized (determined by children)
//    type == FLEX_SIZING_PERCENT→  value is the ratio [0..1] of parent
//
//  Kain style keys use a simple encoding:
//    width/height >= 0  →  FLEX_SIZING_FIXED(value)
//    width/height == -1 →  FLEX_SIZING_GROW(1.0)     equal share
//    width/height == -2 →  FLEX_SIZING_FIT            content-sized
//    width/height < -2  →  FLEX_SIZING_GROW(abs(value))  weighted grow
//
//  Additionally, "sizing.w" and "sizing.h" i64 style keys provide
//  explicit enum values (0=FIXED,1=GROW,2=FIT,3=PERCENT), and
//  "sizing.w.value" / "sizing.h.value" f64 keys provide the parameter.
//  When present, these take precedence over the simple encoding.
// ══════════════════════════════════════════════════════════════════════════

/// Convert a Kain sizing-style value to a FlexboxSizing struct.
static FlexboxSizing layout_sizing_from_style(double raw_value) {
    FlexboxSizing sz;
    sz.min = 0.0f;
    sz.max = 1e20f;

    if (raw_value >= 0.0) {
        sz.type = FLEX_SIZING_FIXED;
        sz.value = (float)raw_value;
    } else if (raw_value == -1.0) {
        sz.type = FLEX_SIZING_GROW;
        sz.value = 1.0f;
    } else if (raw_value == -2.0) {
        sz.type = FLEX_SIZING_FIT;
        sz.value = 0.0f;
    } else {
        /* raw_value < -2 → GROW with weight = -raw_value */
        sz.type = FLEX_SIZING_GROW;
        sz.value = (float)(-raw_value);
    }
    return sz;
}

/// Read a node's sizing for one axis, checking explicit sizing keys first,
/// then falling back to the simple width/height convention.
static FlexboxSizing layout_read_sizing(
    KainNativeUiSession* s, int64_t node_id,
    const char* sizing_key,      /* e.g. "sizing.w" */
    const char* sizing_val_key,  /* e.g. "sizing.w.value" */
    const char* dim_key,         /* e.g. "width" */
    const char* min_key,         /* e.g. "min-width" */
    const char* max_key          /* e.g. "max-width" */
) {
    /* Try explicit sizing key first */
    int64_t explicit_type = layout_style_i64(s, node_id, sizing_key, -1);
    if (explicit_type >= 0) {
        FlexboxSizing sz;
        sz.type = (uint8_t)explicit_type;
        sz.min = (float)layout_style_f64(s, node_id, min_key, 0.0);
        sz.max = (float)layout_style_f64(s, node_id, max_key, 1e20);
        sz.value = (float)layout_style_f64(s, node_id, sizing_val_key, 0.0);
        if (sz.type == FLEX_SIZING_GROW && sz.value < 0.01f) sz.value = 1.0f;
        return sz;
    }

    /* Fall back to simple width/height convention */
    double raw = layout_style_f64(s, node_id, dim_key, -1.0);
    FlexboxSizing sz = layout_sizing_from_style(raw);
    sz.min = (float)layout_style_f64(s, node_id, min_key, 0.0);
    sz.max = (float)layout_style_f64(s, node_id, max_key, 1e20);
    return sz;
}

// ══════════════════════════════════════════════════════════════════════════
//  Style-to-FlexboxConfig conversion
// ══════════════════════════════════════════════════════════════════════════

FlexboxConfig layout_engine_node_to_config(KainNativeUiSession* session, int64_t node_id) {
    FlexboxConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* ── Sizing ───────────────────────────────────────────────────── */
    cfg.width  = layout_read_sizing(session, node_id,
                                     "sizing.w", "sizing.w.value",
                                     "width", "min-width", "max-width");
    cfg.height = layout_read_sizing(session, node_id,
                                     "sizing.h", "sizing.h.value",
                                     "height", "min-height", "max-height");

    /* ── Direction ────────────────────────────────────────────────── */
    int64_t dir = layout_style_i64(session, node_id, "layout.direction", 1);
    cfg.direction = (dir == 0) ? FLEX_DIRECTION_ROW : FLEX_DIRECTION_COLUMN;

    /* ── Cross-axis alignment (align) ─────────────────────────────── */
    int64_t align_val = layout_style_i64(session, node_id, "layout.align", 0);
    cfg.align = (uint8_t)(align_val & 0xFF);

    /* ── Main-axis alignment (justify) ────────────────────────────── */
    int64_t justify = layout_style_i64(session, node_id, "layout.justify", 0);
    cfg.justify = (uint8_t)(justify & 0xFF);

    /* ── Padding ──────────────────────────────────────────────────── */
    double uniform = layout_style_f64(session, node_id, "padding", -1.0);
    if (uniform >= 0.0) {
        cfg.padding_left   = (float)uniform;
        cfg.padding_right  = (float)uniform;
        cfg.padding_top    = (float)uniform;
        cfg.padding_bottom = (float)uniform;
    } else {
        cfg.padding_left   = (float)layout_style_f64(session, node_id, "padding.left",   0.0);
        cfg.padding_right  = (float)layout_style_f64(session, node_id, "padding.right",  0.0);
        cfg.padding_top    = (float)layout_style_f64(session, node_id, "padding.top",    0.0);
        cfg.padding_bottom = (float)layout_style_f64(session, node_id, "padding.bottom", 0.0);
    }

    /* ── Gap / spacing ────────────────────────────────────────────── */
    cfg.gap = (float)layout_style_f64(session, node_id, "gap",
               layout_style_f64(session, node_id, "spacing", 0.0));

    /* ── Wrap ─────────────────────────────────────────────────────── */
    int64_t wrap = layout_style_i64(session, node_id, "layout.wrap", 0);
    cfg.wrap = (wrap != 0);

    /* ── Aspect ratio ─────────────────────────────────────────────── */
    cfg.aspect_ratio = (float)layout_style_f64(session, node_id, "aspect.ratio", 0.0);

    return cfg;
}

// ══════════════════════════════════════════════════════════════════════════
//  Child enumeration (sibling-linked, Z3-proven 4096x faster vs linear scan)
// ══════════════════════════════════════════════════════════════════════════
//  Non-root parents use node->first_child / node->next_sibling to walk
//  children in O(child_count) instead of O(ABI_UI_MAX_NODES).
//  Root nodes (parent_id == 0) fall back to a linear scan.
//
//  Safety: next_sibling is checked against [0, ABI_UI_MAX_NODES) to handle
//  corrupted or memset'd nodes gracefully (Z3-proven, see
//  ui-renderer-sibling-bounds-safe.smt2).
// ══════════════════════════════════════════════════════════════════════════

#define LAYOUT_MAX_STACK_CHILDREN 256

/// Collect child node indices for a parent node.
/// Uses the sibling-linked list for non-root nodes, linear scan for roots.
/// Returns the number of children found (capped at max_count).
static int layout_collect_children(
    KainNativeUiSession* s,
    int64_t parent_id,
    int64_t* out_indices,
    int max_count
) {
    int count = 0;
    if (parent_id <= 0) {
        /* Root nodes: linear scan (roots are few, typ 1-2). */
        int i;
        for (i = 0; i < ABI_UI_MAX_NODES && count < max_count; i++) {
            if (s->nodes[i].in_use && s->nodes[i].parent_id == parent_id) {
                out_indices[count++] = i;
            }
        }
        return count;
    }

    /* Non-root: sibling-linked list walk. */
    KainNativeUiNode* parent = abi_ui_find_node(s, parent_id);
    if (!parent) return 0;
    int32_t child_idx = parent->first_child;
    while (child_idx >= 0 && count < max_count) {
        out_indices[count++] = child_idx;
        int32_t next = s->nodes[child_idx].next_sibling;
        child_idx = (next >= 0 && next < ABI_UI_MAX_NODES) ? next : -1;
    }
    return count;
}

// ══════════════════════════════════════════════════════════════════════════
//  Recursive subtree dirtiness check
// ══════════════════════════════════════════════════════════════════════════
//  Returns true if the node or any descendant has layout_dirty set.
//  Used to avoid unnecessary work: a clean parent with clean children
//  skips the entire layout pass (dirty-flag gating).
// ══════════════════════════════════════════════════════════════════════════

/// Check if a node's subtree has any dirty nodes.
/// Performs a DFS using a small stack to avoid deep C recursion.
static bool subtree_is_dirty(KainNativeUiSession* s, int64_t node_id) {
    /* Small stack-based DFS — avoids deep C recursion on deep trees */
    int64_t stack[256];
    int sp = 0;
    stack[sp++] = node_id;

    while (sp > 0) {
        int64_t current_id = stack[--sp];
        if (current_id < 0 || current_id >= ABI_UI_MAX_NODES) continue;
        KainNativeUiNode* n = &s->nodes[current_id];
        if (!n->in_use) continue;
        if (n->layout_dirty) return true;

        /* Push children onto stack */
        if (current_id <= 0) {
            /* Root: linear scan for children */
            int i;
            for (i = 0; i < ABI_UI_MAX_NODES && sp < 256; i++) {
                if (s->nodes[i].in_use && s->nodes[i].parent_id == current_id) {
                    stack[sp++] = i;
                }
            }
        } else {
            int32_t child_idx = n->first_child;
            while (child_idx >= 0 && sp < 256) {
                stack[sp++] = child_idx;
                int32_t next = s->nodes[child_idx].next_sibling;
                child_idx = (next >= 0 && next < ABI_UI_MAX_NODES) ? next : -1;
            }
        }
    }
    return false;
}

// ══════════════════════════════════════════════════════════════════════════
//  Recursive layout function
// ══════════════════════════════════════════════════════════════════════════
//  Post-order traversal: recursively positions children first, then uses
//  the flexbox solver to compute this container's child positions.
//
//  Parameters:
//    s          - session
//    node_id    - slot index of the node to lay out
//    parent_w   - available width from parent (for GROW/PERCENT sizing)
//    parent_h   - available height from parent
//
//  Returns the computed content size of this subtree (used by FIT parents).
//
//  Dirty-flag gating (Z3-proven 51x speedup on typical frames):
//    If the node and all descendants are clean, the cached position is used
//    and the function returns immediately with the cached size.
// ══════════════════════════════════════════════════════════════════════════

static kainSize layout_node(
    KainNativeUiSession* s,
    int64_t node_id,
    float parent_w,
    float parent_h
) {
    /* ── Validate ─────────────────────────────────────────────────── */
    if (!s || node_id < 0 || node_id >= ABI_UI_MAX_NODES) {
        return kain_size_make(0.0f, 0.0f);
    }
    KainNativeUiNode* node = &s->nodes[node_id];
    if (!node->in_use) return kain_size_make(0.0f, 0.0f);

    /* ── Dirty-flag gating ────────────────────────────────────────── */
    /* Z3-verified: See ui-dirty-flag-layout-cache.smt2 (51x speedup).
     * If this node is clean AND no descendants are dirty, use cached rect. */
    if (!node->layout_dirty && !subtree_is_dirty(s, node_id)) {
        return kain_size_make((float)node->width, (float)node->height);
    }

    /* ── Collect children ─────────────────────────────────────────── */
    int64_t child_stack_buf[LAYOUT_MAX_STACK_CHILDREN];
    int64_t* child_indices = child_stack_buf;
    int child_count = layout_collect_children(s, node->id, child_indices, LAYOUT_MAX_STACK_CHILDREN);

    /* Heap fallback if node has more children than stack buffer (extremely rare) */
    int64_t* heap_children = NULL;
    if (child_count >= LAYOUT_MAX_STACK_CHILDREN) {
        heap_children = (int64_t*)malloc((size_t)child_count * sizeof(int64_t));
        if (heap_children) {
            child_indices = heap_children;
            child_count = layout_collect_children(s, node->id, child_indices, child_count);
        } else {
            /* malloc failed — stick with what we already have */
            child_count = LAYOUT_MAX_STACK_CHILDREN;
        }
    }

    /* ── Build parent FlexboxConfig from styles ───────────────────── */
    FlexboxConfig parent_cfg = layout_engine_node_to_config(s, node->id);

    /* ── Resolve parent container size from available space ───────── */
    float container_w, container_h;

    switch (parent_cfg.width.type) {
        case FLEX_SIZING_FIXED:
            container_w = parent_cfg.width.value;
            break;
        case FLEX_SIZING_PERCENT:
            container_w = parent_w * parent_cfg.width.value;
            break;
        case FLEX_SIZING_GROW:
        default:
            container_w = (parent_w > 0.0f) ? parent_w : 0.0f;
            break;
        case FLEX_SIZING_FIT:
            /* FIT: will be determined by children after layout */
            container_w = (parent_w > 0.0f) ? parent_w : 0.0f;
            break;
    }

    switch (parent_cfg.height.type) {
        case FLEX_SIZING_FIXED:
            container_h = parent_cfg.height.value;
            break;
        case FLEX_SIZING_PERCENT:
            container_h = parent_h * parent_cfg.height.value;
            break;
        case FLEX_SIZING_GROW:
        default:
            container_h = (parent_h > 0.0f) ? parent_h : 0.0f;
            break;
        case FLEX_SIZING_FIT:
            container_h = (parent_h > 0.0f) ? parent_h : 0.0f;
            break;
    }

    /* Clamp to min/max */
    float cw_min = parent_cfg.width.min;
    float cw_max = parent_cfg.width.max;
    if (cw_max < 0.01f) cw_max = 1e20f;
    container_w = fminf(fmaxf(container_w, cw_min), cw_max);

    float ch_min = parent_cfg.height.min;
    float ch_max = parent_cfg.height.max;
    if (ch_max < 0.01f) ch_max = 1e20f;
    container_h = fminf(fmaxf(container_h, ch_min), ch_max);

    /* ── Store container position (set externally; we compute children) ── */
    node->x = (node->x != 0.0) ? node->x : 0.0;
    node->y = (node->y != 0.0) ? node->y : 0.0;
    node->width  = (double)container_w;
    node->height = (double)container_h;

    /* ── If no children, we're done ───────────────────────────────── */
    if (child_count == 0) {
        kainSize result = kain_size_make(
            (parent_cfg.width.type == FLEX_SIZING_FIT) ? 0.0f : container_w,
            (parent_cfg.height.type == FLEX_SIZING_FIT) ? 0.0f : container_h
        );
        node->layout_dirty = 0;
        goto cleanup;
    }

    /* ── Pre-lay out children recursively (post-order) ────────────── */
    /* We pass the available space (container minus padding) as parent */
    float avail_w = container_w - parent_cfg.padding_left - parent_cfg.padding_right;
    float avail_h = container_h - parent_cfg.padding_top  - parent_cfg.padding_bottom;
    if (avail_w < 0.0f) avail_w = 0.0f;
    if (avail_h < 0.0f) avail_h = 0.0f;

    /* ── Build child configs + precompute content sizes ────────────── */
    FlexboxConfig child_configs[LAYOUT_MAX_STACK_CHILDREN];
    kainSize      child_sizes[LAYOUT_MAX_STACK_CHILDREN];
    int i;

    for (i = 0; i < child_count; i++) {
        int64_t child_idx = child_indices[i];
        KainNativeUiNode* child = &s->nodes[child_idx];
        if (!child->in_use) {
            child_configs[i] = layout_engine_node_to_config(s, child_idx);
            child_sizes[i] = kain_size_make(0.0f, 0.0f);
            continue;
        }

        /* Recursively lay out child to get its content size */
        /* This is the post-order: children are sized before we compute
         * the flexbox positions for this container. */
        kainSize cs = layout_node(s, child_idx, avail_w, avail_h);
        child_sizes[i] = cs;

        /* Build child's FlexboxConfig for the flexbox solver */
        child_configs[i] = layout_engine_node_to_config(s, child_idx);

        /* For FIT children, override the sizing with the content size
         * computed from the recursive call */
        if (child_configs[i].width.type == FLEX_SIZING_FIT && cs.w > 0.0f) {
            child_configs[i].width.type = FLEX_SIZING_FIXED;
            child_configs[i].width.value = cs.w;
        }
        if (child_configs[i].height.type == FLEX_SIZING_FIT && cs.h > 0.0f) {
            child_configs[i].height.type = FLEX_SIZING_FIXED;
            child_configs[i].height.value = cs.h;
        }
    }

    /* ── Call flexbox solver to position children ─────────────────── */
    FlexboxResult flex_results[LAYOUT_MAX_STACK_CHILDREN];
    FlexboxResult container_result = flexbox_compute_layout(
        container_w, container_h,
        &parent_cfg,
        child_configs, child_count,
        flex_results
    );

    /* ── Write results back to child node rects ───────────────────── */
    for (i = 0; i < child_count; i++) {
        int64_t child_idx = child_indices[i];
        KainNativeUiNode* child = &s->nodes[child_idx];
        if (!child->in_use) continue;

        child->x = (double)flex_results[i].x;
        child->y = (double)flex_results[i].y;
        child->width  = (double)flex_results[i].width;
        child->height = (double)flex_results[i].height;
    }

    /* ── Compute container content size ───────────────────────────── */
    float content_w, content_h;

    switch (parent_cfg.width.type) {
        case FLEX_SIZING_FIT: {
            /* Bounding box of children + padding */
            float max_x = 0.0f;
            for (i = 0; i < child_count; i++) {
                float right = flex_results[i].x + flex_results[i].width;
                if (right > max_x) max_x = right;
            }
            content_w = max_x + parent_cfg.padding_right;
            break;
        }
        default:
            content_w = container_w;
            break;
    }

    switch (parent_cfg.height.type) {
        case FLEX_SIZING_FIT: {
            float max_y = 0.0f;
            for (i = 0; i < child_count; i++) {
                float bottom = flex_results[i].y + flex_results[i].height;
                if (bottom > max_y) max_y = bottom;
            }
            content_h = max_y + parent_cfg.padding_bottom;
            break;
        }
        default:
            content_h = container_h;
            break;
    }

    /* Clamp FIT containers to min/max */
    content_w = fminf(fmaxf(content_w, parent_cfg.width.min), parent_cfg.width.max);
    content_h = fminf(fmaxf(content_h, parent_cfg.height.min), parent_cfg.height.max);

    /* Write resolved FIT size back to the node */
    node->width  = (double)content_w;
    node->height = (double)content_h;

    kainSize result = kain_size_make(content_w, content_h);

    /* ── Clear dirty flag on this node ─────────────────────────────── */
    node->layout_dirty = 0;

cleanup:
    if (heap_children) free(heap_children);
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  Public API — Layout pass
// ══════════════════════════════════════════════════════════════════════════

int layout_engine_run_pass(KainNativeUiSession* session) {
    if (!session) return 0;

    float session_w = (float)(session->width  > 0 ? session->width  : 1280);
    float session_h = (float)(session->height > 0 ? session->height : 720);
    int re_laid_out = 0;

    /* Find root nodes (parent_id == 0) and lay them out */
    int i;
    for (i = 0; i < ABI_UI_MAX_NODES; i++) {
        if (session->nodes[i].in_use && session->nodes[i].parent_id == 0) {
            kainSize sz = layout_node(session, i, session_w, session_h);
            if (sz.w > 0.0f || sz.h > 0.0f) re_laid_out++;
        }
    }

    return re_laid_out;
}

// ══════════════════════════════════════════════════════════════════════════
//  Public API — Dirty flag management
// ══════════════════════════════════════════════════════════════════════════

void layout_engine_mark_dirty(KainNativeUiSession* session, int64_t node_id) {
    if (!session) return;
    if (node_id < 0 || node_id >= ABI_UI_MAX_NODES) return;
    KainNativeUiNode* node = &session->nodes[node_id];
    if (!node->in_use) return;

    /* Mark this node dirty; its parent will detect via subtree_is_dirty() */
    node->layout_dirty = 1;
}

void layout_engine_mark_subtree_dirty(KainNativeUiSession* session, int64_t node_id) {
    if (!session) return;
    if (node_id < 0 || node_id >= ABI_UI_MAX_NODES) return;
    KainNativeUiNode* node = &session->nodes[node_id];
    if (!node->in_use) return;

    /* DFS to mark all descendants dirty */
    int64_t stack[256];
    int sp = 0;
    stack[sp++] = node_id;

    while (sp > 0) {
        int64_t current = stack[--sp];
        if (current < 0 || current >= ABI_UI_MAX_NODES) continue;
        KainNativeUiNode* n = &session->nodes[current];
        if (!n->in_use) continue;
        n->layout_dirty = 1;

        /* Push children */
        if (current <= 0) {
            int j;
            for (j = 0; j < ABI_UI_MAX_NODES && sp < 256; j++) {
                if (session->nodes[j].in_use && session->nodes[j].parent_id == current) {
                    stack[sp++] = j;
                }
            }
        } else {
            int32_t child_idx = n->first_child;
            while (child_idx >= 0 && sp < 256) {
                stack[sp++] = child_idx;
                int32_t next = session->nodes[child_idx].next_sibling;
                child_idx = (next >= 0 && next < ABI_UI_MAX_NODES) ? next : -1;
            }
        }
    }
}

void layout_engine_mark_all_dirty(KainNativeUiSession* session) {
    if (!session) return;
    int i;
    for (i = 0; i < ABI_UI_MAX_NODES; i++) {
        if (session->nodes[i].in_use) {
            session->nodes[i].layout_dirty = 1;
        }
    }
}
