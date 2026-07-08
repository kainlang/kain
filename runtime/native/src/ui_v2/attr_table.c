// ============================================================================
//  attr_table.c — Data-driven attribute→invalidation mapping table.
//
//  Maps attribute name strings to KaintanaInvalidationReason bitmasks and
//  expected types (0=i64, 1=f64, 2=string).
//
//  Used by tree.c's v_element_set_attr_* vtable dispatchers to determine
//  which dirty flags to set and what type validation to apply.
//  Zero #define constants — data-driven approach enables binary search.
//
//  Table is alphabetically sorted by name. Linear scan for <50 entries;
//  binary search can be added later if needed.
// ============================================================================
#include "internal.h"

// ============================================================================
//  Attribute definition table
// ============================================================================
//  Each entry maps an attribute name (as used in ATTR_KV/ATTR_KF/ATTR_KS)
//  to the invalidation reason bitmask and expected type slot.
//
//  Layout changes       → KT_INVALIDATE_LAYOUT  (1 << 0) — full relayout
//  Paint/style changes  → KT_INVALIDATE_PAINT   (1 << 2) — redraw only
//  Visibility changes   → KT_INVALIDATE_VISIBILITY (1 << 4)
//  Interaction/volatile → KT_INVALIDATE_VOLATILITY (1 << 5)
//
//  Expected types:  0 = i64 (ATTR_KV),  1 = f64 (ATTR_KF),  2 = string (ATTR_KS)
//
//  Sentinel entry (name == NULL) terminates the table.
// ============================================================================
static const KaintanaAttrEntry kaintana_attr_table[] = {
    // ── Style attributes (paint-only — no relayout needed) ───────────
    {"fill",                KT_INVALIDATE_PAINT,        2, 0},
    {"font_family",         KT_INVALIDATE_PAINT,        2, 0},
    {"font_size",           KT_INVALIDATE_PAINT,        1, 0},

    // ── Interaction attributes (volatile — per-frame state) ─────────
    {"interactive",         KT_INVALIDATE_VOLATILITY,   0, 0},

    // ── Layout attributes (triggers full prepass + arrange) ─────────
    {"layout.align",        KT_INVALIDATE_LAYOUT,       0, 0},
    {"layout.dir",          KT_INVALIDATE_LAYOUT,       0, 0},
    {"layout.flex_basis",   KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.flex_grow",    KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.flex_shrink",  KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.gap",          KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.height",       KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.justify",      KT_INVALIDATE_LAYOUT,       0, 0},
    {"layout.margin",       KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.margin_bottom",KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.margin_left",  KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.margin_right", KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.margin_top",   KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.max_height",   KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.max_width",    KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.min_height",   KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.min_width",    KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.pad",          KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.pad_x",        KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.pad_y",        KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.width",        KT_INVALIDATE_LAYOUT,       1, 0},
    {"layout.wrap",         KT_INVALIDATE_LAYOUT,       0, 0},

    // ── Callback / event attributes (volatile) ──────────────────────
    {"on_click",            KT_INVALIDATE_VOLATILITY,   2, 0},
    {"on_hover",            KT_INVALIDATE_VOLATILITY,   2, 0},

    // ── Style attributes continued ───────────────────────────────────
    {"opacity",             KT_INVALIDATE_PAINT,        1, 0},

    // ── Style attributes (paint with geometry implications) ─────────
    {"radius",              KT_INVALIDATE_PAINT,        1, 0},

    // ── Stroke attributes ───────────────────────────────────────────
    {"stroke",              KT_INVALIDATE_PAINT,        2, 0},
    {"stroke_width",        KT_INVALIDATE_PAINT,        1, 0},

    // ── Text style ──────────────────────────────────────────────────
    {"text_align",          KT_INVALIDATE_PAINT,        0, 0},

    // ── Visibility ──────────────────────────────────────────────────
    {"visibility",          KT_INVALIDATE_VISIBILITY,   0, 0},

    // ── Sentinel ────────────────────────────────────────────────────
    {NULL, 0, 0, 0},
};

// ============================================================================
//  kaintana__attr_lookup  —  Find attribute index by name
// ============================================================================
//  Returns the index into kaintana_attr_table for the given attribute
//  name, or -1 if not found. Linear scan is fine for <50 entries;
//  the table is sorted alphabetically, so binary search is a valid
//  future optimization.
//
//  Called from tree.c's v_element_set_attr_* dispatchers.
// ============================================================================
int kaintana__attr_lookup(const char* key) {
    if (!key) return -1;
    for (int i = 0; kaintana_attr_table[i].name != NULL; i++) {
        if (strcmp(kaintana_attr_table[i].name, key) == 0)
            return i;
    }
    return -1;
}

// ============================================================================
//  kaintana__attr_get_entry  —  Get entry pointer by index
// ============================================================================
//  Returns a pointer to the table entry at the given index, or NULL if
//  the index is negative or past the last valid entry (before sentinel).
// ============================================================================
const KaintanaAttrEntry* kaintana__attr_get_entry(int index) {
    if (index < 0) return NULL;
    // Walk the table to find the sentinel position
    int count = 0;
    while (kaintana_attr_table[count].name != NULL) {
        count++;
    }
    return (index < count) ? &kaintana_attr_table[index] : NULL;
}

// ============================================================================
//  kaintana__attr_count  —  Number of entries in the table
// ============================================================================
//  Returns the count of valid entries, excluding the sentinel terminator.
//  Useful for enumeration or bounds checking.
// ============================================================================
int kaintana__attr_count(void) {
    int count = 0;
    while (kaintana_attr_table[count].name != NULL) {
        count++;
    }
    return count;
}
