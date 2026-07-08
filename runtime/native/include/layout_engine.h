#ifndef KAIN_LAYOUT_ENGINE_H
#define KAIN_LAYOUT_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "flexbox.h"
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ══════════════════════════════════════════════════════════════════════════
//  layout_engine.h — Flexbox-based layout pass for Kain's retained-mode UI
// ══════════════════════════════════════════════════════════════════════════
//  Bridges the flexbox solver (flexbox_compute_layout) with the retained
//  node tree. Replaces the old equal-share layout in ui_layout.c.
//
//  Layout pass algorithm:
//    1. Find root node(s) (parent_id == 0)
//    2. Recursively process from root in post-order (children first)
//    3. Build FlexboxConfig from each node's style keys
//    4. Call flexbox_compute_layout to position children
//    5. Write results back to child node rects
//    6. Clear dirty flags
//
//  Dirty flag gating (Z3-verified, 51x fewer nodes visited per frame):
//    Non-dirty subtrees are skipped entirely, using cached positions.
// ══════════════════════════════════════════════════════════════════════════

// ── Forward declarations from ui_system_internal.h ─────────────────────

typedef struct KainNativeUiSession KainNativeUiSession;
typedef struct KainNativeUiNode    KainNativeUiNode;

// ── Public API ──────────────────────────────────────────────────────────

// Run a full flexbox layout pass on the entire node tree.
// Walks all root nodes, recursively positions children via flexbox
// solver. Writes computed positions back to each node's x/y/width/height.
// Clears layout_dirty flags on all processed nodes.
// Returns the number of nodes that were re-laid-out (0 if none dirty).
int layout_engine_run_pass(KainNativeUiSession* session);

// ── Dirty flag management ───────────────────────────────────────────────

// Mark a node and all its descendants as dirty (needs re-layout).
void layout_engine_mark_dirty(KainNativeUiSession* session, int64_t node_id);

// Mark a subtree as dirty (for targeted invalidation).
void layout_engine_mark_subtree_dirty(KainNativeUiSession* session, int64_t node_id);

// Mark the entire tree as dirty (after window resize, theme change, etc.).
void layout_engine_mark_all_dirty(KainNativeUiSession* session);

// ── Style to FlexboxConfig conversion ───────────────────────────────────

// Read a node's style settings and produce a FlexboxConfig.
// Reads style keys:
//   "layout.direction"   → i64: 0=ROW, 1=COLUMN
//   "width"/"height"     → f64: >=0=FIXED, -1=GROW(1), -2=FIT, <-2=GROW(n)
//   "padding"            → f64: uniform (per-side override via .left/.right/.top/.bottom)
//   "gap"/"spacing"      → f64: child gap
//   "layout.align"       → i64: 0=START, 1=CENTER, 2=END, 3=STRETCH
//   "layout.justify"     → i64: 0=START, 1=CENTER, 2=END, 3=SPACE_BETWEEN, 4=SPACE_AROUND
//   "layout.wrap"        → i64: 0/1
//   "aspect.ratio"       → f64
//   "min-width"/"min-height"/"max-width"/"max-height" → f64
FlexboxConfig layout_engine_node_to_config(KainNativeUiSession* session, int64_t node_id);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_LAYOUT_ENGINE_H */
