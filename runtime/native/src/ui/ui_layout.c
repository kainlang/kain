#include "../../include/ui_layout.h"
#include "ui_system_internal.h"

#include <string.h>

// ── Style value helpers ────────────────────────────────────────────────

static double ui_layout_style_f64(KainNativeUiSession* s, int64_t node_id, const char* key, double fallback) {
    int i;
    for (i = 0; i < ABI_UI_MAX_STYLES; i++) {
        if (s->styles[i].in_use && s->styles[i].node_id == node_id) {
            if (strcmp(s->styles[i].key, key) == 0) {
                if (s->styles[i].value_kind == ABI_UI_STYLE_F64) {
                    return s->styles[i].f64_value;
                }
                break;  // key found but wrong type
            }
        }
    }
    return fallback;
}

static int64_t ui_layout_style_i64(KainNativeUiSession* s, int64_t node_id, const char* key, int64_t fallback) {
    int i;
    for (i = 0; i < ABI_UI_MAX_STYLES; i++) {
        if (s->styles[i].in_use && s->styles[i].node_id == node_id) {
            if (strcmp(s->styles[i].key, key) == 0) {
                if (s->styles[i].value_kind == ABI_UI_STYLE_I64) {
                    return s->styles[i].i64_value;
                }
                break;
            }
        }
    }
    return fallback;
}

// ── Child enumeration ──────────────────────────────────────────────────

// Build an array of child node indices for a given parent.
// Returns child count. Writes up to max_children indices into out_indices.
static int64_t ui_layout_collect_children(
    KainNativeUiSession* s,
    int64_t parent_id,
    int64_t* out_indices,
    int64_t max_children
) {
    int64_t count = 0;
    int i;
    for (i = 0; i < ABI_UI_MAX_NODES && count < max_children; i++) {
        if (s->nodes[i].in_use && s->nodes[i].parent_id == parent_id) {
            out_indices[count++] = i;
        }
    }
    return count;
}

// ── Recursive layout ───────────────────────────────────────────────────

static void ui_layout_node(KainNativeUiSession* s, int64_t node_idx, double parent_x, double parent_y,
                           double parent_w, double parent_h) {
    if (!s || node_idx < 0 || node_idx >= ABI_UI_MAX_NODES) return;
    KainNativeUiNode* node = &s->nodes[node_idx];
    if (!node->in_use) return;

    // ── Read layout styles ─────────────────────────────────────────
    double padding_left   = ui_layout_style_f64(s, node->id, "padding.left", 0.0);
    double padding_top    = ui_layout_style_f64(s, node->id, "padding.top", 0.0);
    double padding_right  = ui_layout_style_f64(s, node->id, "padding.right", 0.0);
    double padding_bottom = ui_layout_style_f64(s, node->id, "padding.bottom", 0.0);
    double uniform_pad    = ui_layout_style_f64(s, node->id, "padding", -1.0);
    if (uniform_pad >= 0.0) {
        padding_left = padding_top = padding_right = padding_bottom = uniform_pad;
    }

    double spacing = ui_layout_style_f64(s, node->id, "spacing",
                    ui_layout_style_f64(s, node->id, "gap", 0.0));

    int64_t direction = ui_layout_style_i64(s, node->id, "layout.direction", 1); // 0=H, 1=V

    double explicit_w = ui_layout_style_f64(s, node->id, "width", -1.0);
    double explicit_h = ui_layout_style_f64(s, node->id, "height", -1.0);

    // ── Compute node rect ──────────────────────────────────────────
    // Use explicit sizes, or fill parent, or keep existing
    double node_w = (explicit_w >= 0.0) ? explicit_w : parent_w;
    double node_h = (explicit_h >= 0.0) ? explicit_h : parent_h;

    node->x = parent_x;
    node->y = parent_y;
    node->width = node_w;
    node->height = node_h;

    // ── Collect children ───────────────────────────────────────────
    int64_t child_indices[ABI_UI_MAX_NODES];
    int64_t child_count = ui_layout_collect_children(s, node->id, child_indices, ABI_UI_MAX_NODES);
    if (child_count == 0) return;

    // ── Layout children ────────────────────────────────────────────
    double avail_w = node_w - padding_left - padding_right;
    double avail_h = node_h - padding_top - padding_bottom;
    if (avail_w < 0.0) avail_w = 0.0;
    if (avail_h < 0.0) avail_h = 0.0;

    double cursor_x = node->x + padding_left;
    double cursor_y = node->y + padding_top;

    int64_t i;
    for (i = 0; i < child_count; i++) {
        int64_t child_idx = child_indices[i];
        KainNativeUiNode* child = &s->nodes[child_idx];

        double child_w = ui_layout_style_f64(s, child->id, "width", -1.0);
        double child_h = ui_layout_style_f64(s, child->id, "height", -1.0);

        if (direction == 0) {
            // Horizontal layout — each child gets equal share if no explicit size
            double share_w = (child_w >= 0.0) ? child_w : (avail_w / (double)child_count);
            double share_h = (child_h >= 0.0) ? child_h : avail_h;

            child->x = cursor_x;
            child->y = cursor_y;
            child->width = share_w;
            child->height = share_h;

            cursor_x += share_w + spacing;
        } else {
            // Vertical layout (default)
            double share_w = (child_w >= 0.0) ? child_w : avail_w;
            double share_h = (child_h >= 0.0) ? child_h : 0.0;

            child->x = cursor_x;
            child->y = cursor_y;
            child->width = share_w;
            // If no explicit height, all children split remaining space equally
            if (child_h < 0.0) {
                // Count remaining children without explicit height
                int64_t remaining = 0;
                int64_t j;
                for (j = i; j < child_count; j++) {
                    double h = ui_layout_style_f64(s, s->nodes[child_indices[j]].id, "height", -1.0);
                    if (h < 0.0) remaining++;
                }
                share_h = (remaining > 0) ? ((avail_h - (cursor_y - node->y - padding_top)) / (double)remaining) : 0.0;
            }
            child->height = share_h;

            cursor_y += share_h + spacing;
        }

        // Recurse into child
        ui_layout_node(s, child_idx, child->x, child->y, child->width, child->height);
    }
}

// ── Public entry point ─────────────────────────────────────────────────

int64_t ui_layout_resolve(KainNativeUiSession* session) {
    if (!session) return -1;

    // Find root nodes (parent_id == 0)
    int i;
    for (i = 0; i < ABI_UI_MAX_NODES; i++) {
        if (session->nodes[i].in_use && session->nodes[i].parent_id == 0) {
            // Root fills the session area
            double root_w = session->width > 0 ? (double)session->width : 1280.0;
            double root_h = session->height > 0 ? (double)session->height : 720.0;
            ui_layout_node(session, i, 0.0, 0.0, root_w, root_h);
        }
    }

    return 0;
}
