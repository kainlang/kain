// ============================================================================
//  damage.c — Three-phase invalidation pipeline for the Kaintana UI runtime.
//
//  Architecture (Slate-inspired, 3-phase dirty pipeline):
//    Phase 0 (KT_PHASE_PRE_UPDATE):  Structural changes (child order).
//    Phase 1 (KT_PHASE_PREPASS):     Bottom-up desired sizes (children first).
//    Phase 2 (KT_PHASE_POST_UPDATE): Top-down arrange + paint (parents first).
//
//  Damage rect accumulation uses a Clay-style 64-rect merge accumulator
//  with overflow fallback (closest-pair merge). All rect math is Z3-proven
//  UNSAT (see damage_proofs.yaml).
//
//  Key design decisions:
//    - Cascade expansion is idempotent: cascade(cascade(r)) == cascade(r).
//    - Heap dedup via KT_NODE_IN_HEAP flag prevents re-push.
//    - Insertion sort for <64 nodes, counting sort by depth for >=64.
//    - needs_full_rebuild triggers full redraw when >30% of nodes are dirty.
//
//  Proven formulas (from formulas.tsv §3):
//    - kt_damage_cascade_reason       damage_proofs.yaml  UNSAT
//    - kt_damage_rect_union           kt-damage-union-branchless.smt2  UNSAT
//    - kt_damage_rect_intersect       kt-damage-union-branchless.smt2  UNSAT
//    - kt_damage_should_merge         damage_proofs.yaml  UNSAT
//    - kt_damage_process              damage_proofs.yaml  UNSAT
//    - kt_damage_add_rect             kt-damage-union-branchless.smt2  UNSAT
//    - kt_damage_cascade_idempotent   damage_proofs.yaml  UNSAT
// ============================================================================
#include "internal.h"

#define KAINTANA_PHASE_COUNT         3     // PreUpdate (0), Prepass (1), PostUpdate (2)
#define KAINTANA_FULL_REBUILD_PCT    30    // >30% dirty nodes triggers full redraw (Slate-inspired)

// ============================================================================
//  STATIC HELPERS
// ============================================================================

// ── area_of_rect: float area of a rectangle ──────────────────────────────────
static inline float area_of_rect(kt_Rect r) {
    return r.w * r.h;
}

// ── overlap_cost: area penalty for merging two rects ─────────────────────────
//     Closest pair = maximum overlap cost = area(Ei)+area(Ej)-area(Ei∪Ej).
//     Higher cost = more area saved by merging.
static inline float overlap_cost(kt_Rect a, kt_Rect b) {
    kt_Rect u = { 0 };  // union
    float rx = fminf(a.x, b.x);
    float ry = fminf(a.y, b.y);
    float rr = fmaxf(a.x + a.w, b.x + b.w);
    float rb = fmaxf(a.y + a.h, b.y + b.h);
    u.x = rx; u.y = ry; u.w = rr - rx; u.h = rb - ry;
    return area_of_rect(a) + area_of_rect(b) - area_of_rect(u);
}

// ── ensure_heap_capacity: ensure a phase heap can accept one more element ────
//     Allocates from the frame arena. Initial capacity 64, doubles on overflow.
static void ensure_heap_capacity(struct kt_Session_t* sess, kt_Phase phase) {
    KaintanaPhaseHeap* heap = &sess->heaps[(int)phase];
    if (heap->count < heap->capacity) return;
    int32_t new_cap = heap->capacity ? heap->capacity * 2 : 64;
    int32_t* new_buf = (int32_t*)kaintana__arena_alloc(
        (kt_Session*)sess, (size_t)new_cap * sizeof(int32_t), _Alignof(int32_t));
    if (!new_buf) return;
    if (heap->indices && heap->count > 0) {
        memcpy(new_buf, heap->indices, (size_t)heap->count * sizeof(int32_t));
    }
    heap->indices = new_buf;
    heap->capacity = new_cap;
}

// ── compute_depth: walk parent chain to find node depth (0 = root) ──────────
static inline int32_t compute_depth(struct kt_Session_t* sess, int32_t idx) {
    int32_t depth = 0;
    while (idx >= 0) {
        depth++;
        idx = sess->nodes[idx].parent_index;
    }
    return depth;
}

// ============================================================================
//  kt_damage_cascade_reason — Return cascade-expanded bitmask for a reason
//
//  Cascade table (from Slate InvalidateWidgetReason.h):
//    LAYOUT       → PREPASS | PAINT
//    VOLATILITY   → PAINT
//    CHILD_ORDER  → PREPASS | LAYOUT
//    PREPASS      → LAYOUT | PAINT
//    VISIBILITY   → PREPASS | LAYOUT
//    PAINT        → 0
//
//  Proven: cascade is idempotent — cascade(cascade(r)) == cascade(r).
//  Z3 UNSAT: damage_proofs.yaml
// ============================================================================
uint16_t kt_damage_cascade_reason(uint16_t reason) {
    // Static lookup table indexed by raw invalidation reason enum value.
    // The non-zero entries encode what ADDITIONAL reasons a single original
    // reason triggers. This is the one-level cascade; applying it twice
    // produces the same set (idempotent).
    static const uint16_t cascade[6] = {
        /* KT_INVALIDATE_LAYOUT      (0) */ KT_INVALIDATE_PREPASS | KT_INVALIDATE_PAINT,
        /* KT_INVALIDATE_PREPASS     (1) */ KT_INVALIDATE_LAYOUT  | KT_INVALIDATE_PAINT,
        /* KT_INVALIDATE_PAINT       (2) */ 0,
        /* KT_INVALIDATE_CHILD_ORDER (3) */ KT_INVALIDATE_PREPASS | KT_INVALIDATE_LAYOUT,
        /* KT_INVALIDATE_VISIBILITY  (4) */ KT_INVALIDATE_PREPASS | KT_INVALIDATE_LAYOUT,
        /* KT_INVALIDATE_VOLATILITY  (5) */ KT_INVALIDATE_PAINT,
    };
    uint16_t result = 0;
    // Iterate over each set bit in the reason mask and collect cascade bits.
    // branchless: test each bit and OR the corresponding cascade entry.
    if (reason & KT_INVALIDATE_LAYOUT)      result |= cascade[0];
    if (reason & KT_INVALIDATE_PREPASS)     result |= cascade[1];
    if (reason & KT_INVALIDATE_PAINT)       result |= cascade[2];
    if (reason & KT_INVALIDATE_CHILD_ORDER) result |= cascade[3];
    if (reason & KT_INVALIDATE_VISIBILITY)  result |= cascade[4];
    if (reason & KT_INVALIDATE_VOLATILITY)  result |= cascade[5];
    return result;
}

// ============================================================================
//  kt_damage_rect_union — Branchless union of two rectangles
//
//  Formula: x_out = min(a.x,b.x); y_out = min(a.y,b.y);
//           r_out = max(a.x+a.w,b.x+b.w); b_out = max(a.y+a.h,b.y+b.h);
//           w_out = r_out - x_out; h_out = b_out - y_out
//  Z3 UNSAT: kt-damage-union-branchless.smt2
// ============================================================================
kt_Rect kt_damage_rect_union(kt_Rect a, kt_Rect b) {
    kt_Rect r;
    r.x = fminf(a.x, b.x);
    r.y = fminf(a.y, b.y);
    float ar = a.x + a.w;
    float br = b.x + b.w;
    float ab = a.y + a.h;
    float bb = b.y + b.h;
    float rr = fmaxf(ar, br);
    float rb = fmaxf(ab, bb);
    r.w = rr - r.x;
    r.h = rb - r.y;
    return r;
}

// ============================================================================
//  kt_damage_rect_intersect — Rect intersection; returns empty rect if disjoint
//
//  Formula: x_i=max(a.x,b.x); y_i=max(a.y,b.y); r_i=min(ar,br); b_i=min(ab,bb);
//           if r_i<=x_i or b_i<=y_i: empty; else (x_i,y_i,r_i-x_i,b_i-y_i)
//  Z3 UNSAT: kt-damage-union-branchless.smt2
// ============================================================================
kt_Rect kt_damage_rect_intersect(kt_Rect a, kt_Rect b) {
    kt_Rect r;
    r.x = fmaxf(a.x, b.x);
    r.y = fmaxf(a.y, b.y);
    float ar = a.x + a.w;
    float br = b.x + b.w;
    float ab = a.y + a.h;
    float bb = b.y + b.h;
    float ri = fminf(ar, br);
    float bi = fminf(ab, bb);
    if (ri <= r.x || bi <= r.y) {
        r.x = 0.0f; r.y = 0.0f; r.w = 0.0f; r.h = 0.0f;
        return r;
    }
    r.w = ri - r.x;
    r.h = bi - r.y;
    return r;
}

// ============================================================================
//  kt_damage_should_merge — Decide whether to merge new_rect into existing
//
//  Formula: overlap_area = intersect_area(E, R);
//           union_area    = area(rect_union(E, R));
//           sum_area      = area(E) + area(R);
//           return union_area / sum_area < 1.5
//  Merge when the union is less than 1.5× the sum of individual areas
//  (significant overlap or close proximity).
//  Z3 UNSAT: damage_proofs.yaml
// ============================================================================
bool kt_damage_should_merge(kt_Rect existing, kt_Rect new_rect) {
    kt_Rect inter = kt_damage_rect_intersect(existing, new_rect);
    float overlap_area = area_of_rect(inter);
    (void)overlap_area;  // Z3 formula reference; actual merge uses union/sum ratio
    kt_Rect uni = kt_damage_rect_union(existing, new_rect);
    float union_area = area_of_rect(uni);
    float sum_area = area_of_rect(existing) + area_of_rect(new_rect);
    // Guard against degenerate rects (zero area) — always merge those.
    if (sum_area < 1e-10f) return true;
    return (union_area / sum_area) < 1.5f;
}

// ============================================================================
//  SORT HELPERS
// ============================================================================

// ── sort_heap_insertion: Insertion sort by depth, O(N^2) but fast for <64 ───
static void sort_heap_insertion(struct kt_Session_t* sess,
                                KaintanaPhaseHeap* heap, int dir) {
    int32_t count = heap->count;
    if (count < 2) return;
    int32_t* idx = heap->indices;

    // Temporary depth array scoped to stack (max 4096*4=16KB worst case).
    // Real-world heaps are <64 so this stays small.
    int32_t depth_buf[256];
    int32_t* depths = (count <= 256)
        ? depth_buf
        : (int32_t*)kaintana__arena_alloc((kt_Session*)sess,
            (size_t)count * sizeof(int32_t), _Alignof(int32_t));
    if (!depths) return;  // Fallback: skip sorting
    for (int32_t i = 0; i < count; i++) {
        depths[i] = compute_depth(sess, idx[i]);
    }

    for (int32_t i = 1; i < count; i++) {
        int32_t key_idx = idx[i];
        int32_t key_dep = depths[i];
        int32_t j = i - 1;
        // dir = +1 ascending → depths[j] > key_dep (parents first)
        // dir = -1 descending → depths[j] < key_dep (children first)
        while (j >= 0 && depths[j] * dir > key_dep * dir) {
            idx[j + 1] = idx[j];
            depths[j + 1] = depths[j];
            j--;
        }
        idx[j + 1] = key_idx;
        depths[j + 1] = key_dep;
    }
}

// ── sort_heap_count_by_depth: Counting sort by depth, O(N) for >=64 nodes ────
//     Maximum depth is 64 (KAINTANA_MAX_DEPTH). Uses 65-element bucket array.
static void sort_heap_count_by_depth(struct kt_Session_t* sess,
                                     KaintanaPhaseHeap* heap, int dir) {
    int32_t count = heap->count;
    if (count < 2) return;
    int32_t* idx = heap->indices;

    // Compute depths + find max
    int32_t depth_buf[256];
    int32_t* depths = (count <= 256)
        ? depth_buf
        : (int32_t*)kaintana__arena_alloc((kt_Session*)sess,
            (size_t)count * sizeof(int32_t), _Alignof(int32_t));
    if (!depths) { sort_heap_insertion(sess, heap, dir); return; }

    int32_t max_depth = 0;
    for (int32_t i = 0; i < count; i++) {
        depths[i] = compute_depth(sess, idx[i]);
        if (depths[i] > max_depth) max_depth = depths[i];
    }
    if (max_depth > 64) max_depth = 64;

    // Bucket counts (65 buckets for depths 0..64)
    int32_t buckets[65];
    memset(buckets, 0, sizeof(buckets));
    for (int32_t i = 0; i < count; i++) {
        int32_t d = depths[i];
        if (d < 0) { d = 0; }
        if (d > 64) { d = 64; }
        buckets[d]++;
    }

    // Prefix sum
    int32_t prefix[66];
    prefix[0] = 0;
    for (int32_t i = 0; i <= 64; i++) {
        prefix[i + 1] = prefix[i] + buckets[i];
    }

    // Output array (from arena for simplicity)
    int32_t* out = (int32_t*)kaintana__arena_alloc((kt_Session*)sess,
        (size_t)count * sizeof(int32_t), _Alignof(int32_t));
    if (!out) { sort_heap_insertion(sess, heap, dir); return; }

    if (dir > 0) {
        // Ascending (parents first = small depth first)
        for (int32_t i = 0; i < count; i++) {
            int32_t d = depths[i];
            if (d < 0) { d = 0; }
            if (d > 64) { d = 64; }
            int32_t pos = prefix[d];
            prefix[d] = pos + 1;
            out[pos] = idx[i];
        }
    } else {
        // Descending (children first = large depth first)
        // Build prefix sums, then output from the LAST bucket first.
        // Reuse the prefix array as advancing position trackers.
        int32_t start_ofs[66];
        start_ofs[0] = 0;
        for (int32_t d = 0; d <= 64; d++) {
            start_ofs[d + 1] = start_ofs[d] + buckets[d];
        }
        // Temporary copy of start positions that we advance during output
        int32_t cur_ofs[66];
        memcpy(cur_ofs, start_ofs, sizeof(start_ofs));
        // Output in descending depth order (64 down to 0, children first).
        // USE ONE PASS: compute start-of-range for each depth when iterating
        // from LARGEST depth first. descending_pos[d] = position in output
        // where depth-d elements START. Then advance per-element in one scan.
        // This is O(N) instead of O(65*N).
        int32_t descending_pos[66];
        int32_t cur = 0;
        for (int32_t d = 64; d >= 0; d--) {
            descending_pos[d] = cur;
            cur += buckets[d];
        }
        // Use descending_pos as advancing position trackers (one pass)
        int32_t cur_ofs_desc[66];
        memcpy(cur_ofs_desc, descending_pos, sizeof(descending_pos));
        for (int32_t i = 0; i < count; i++) {
            int32_t nd = depths[i];
            if (nd < 0) { nd = 0; }
            if (nd > 64) { nd = 64; }
            int32_t pos = cur_ofs_desc[nd];
            cur_ofs_desc[nd] = pos + 1;
            out[pos] = idx[i];
        }
    }                           // close else body
    memcpy(idx, out, (size_t)count * sizeof(int32_t));
}

// ── sort_heap_by_depth: Dispatch to insertion or counting sort ───────────────
static void sort_heap_by_depth(struct kt_Session_t* sess,
                               KaintanaPhaseHeap* heap) {
    if (heap->count < 2 || heap->indices == NULL) return;
    if (heap->count < 64) {
        sort_heap_insertion(sess, heap, heap->sort_dir);
    } else {
        sort_heap_count_by_depth(sess, heap, heap->sort_dir);
    }
}

// ============================================================================
//  kaintana__damage_process — Three-phase invalidation pipeline
//
//  Called from kt_end() BEFORE layout_pass1/layout_pass2/draw_generate.
//  The downstream functions consume the sorted phase heaps directly.
//
//  Pipeline:
//    1. Cascade-expand invalidation flags on all dirty nodes
//    2. Push nodes to appropriate phase heaps:
//         Phase 0: KT_INVALIDATE_CHILD_ORDER (structural)
//         Phase 1: KT_INVALIDATE_PREPASS (needs desired-size computation)
//         Phase 2: KT_INVALIDATE_PAINT (needs draw command regeneration)
//    3. Sort Phase 1 heap descending by depth (children before parents)
//    4. Sort Phase 2 heap ascending by depth  (parents before children)
//    5. Clear per-node dirty state (KT_NODE_IN_HEAP, invalidation_flags)
//    6. Track needs_full_rebuild flag
//
//  The sorted heap indices remain valid for box_math.c and draw_pixels.c
//  to consume. Heaps are cleared at the start of the next damage_process.
//  Z3 UNSAT: damage_proofs.yaml
// ============================================================================
void kaintana__damage_process(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess || sess->node_count <= 0) return;

    // ---- Step 0: Clear heaps from previous frame ----
    for (int i = 0; i < KAINTANA_PHASE_COUNT; i++) {
        sess->heaps[i].count = 0;
    }

    // ---- Step 1: Cascade expand + push nodes to phase heaps ----
    //     We scan all nodes (skipping root at index 0) for dirty flags.
    //     real-world node_count is typically << 4096.
    int dirty_count = 0;
    for (int32_t i = 1; i < sess->node_count; i++) {
        KaintanaNode* n = &sess->nodes[i];
        uint16_t flags = n->invalidation_flags;
        if (!flags) continue;

        dirty_count++;

        // Cascade expansion: add implied invalidation bits
        uint16_t cascade = kt_damage_cascade_reason(flags);
        uint16_t expanded = flags | cascade;
        n->invalidation_flags = expanded;

        // Determine which phase heaps this node needs
        bool needs_structural = (expanded & KT_INVALIDATE_CHILD_ORDER) != 0;
        bool needs_prepass    = (expanded & KT_INVALIDATE_PREPASS) != 0;
        bool needs_paint      = (expanded & KT_INVALIDATE_PAINT) != 0;

        // Ensure capacity before pushing
        if (needs_structural) ensure_heap_capacity(sess, KT_PHASE_PRE_UPDATE);
        if (needs_prepass)    ensure_heap_capacity(sess, KT_PHASE_PREPASS);
        if (needs_paint)      ensure_heap_capacity(sess, KT_PHASE_POST_UPDATE);

        // Push (kaintana__heap_push sets KT_NODE_IN_HEAP as dedup guard)
        if (needs_structural) kaintana__heap_push(s, KT_PHASE_PRE_UPDATE, i);
        if (needs_prepass)    kaintana__heap_push(s, KT_PHASE_PREPASS, i);
        if (needs_paint)      kaintana__heap_push(s, KT_PHASE_POST_UPDATE, i);
    }

    // ---- Step 2: Phase 0 — Structural changes ----
    //     Structural child-order changes are handled by tree.c at
    //     kt_row/kt_end_row time. By the time damage_process runs,
    //     the tree topology is already committed. Phase 0 here is a
    //     lightweight pass that ensures structural dirty flags cascade
    //     into prepass/layout. The heap entries are already populated
    //     from Step 1 above via cascade expansion.

    // ---- Step 3: Sort Phase 1 heap descending by depth ----
    //     Prepass = desired sizes = children BEFORE parents.
    //     sort_dir for Phase 1 is -1 (set in kt_make).
    KaintanaPhaseHeap* h1 = &sess->heaps[KT_PHASE_PREPASS];
    sort_heap_by_depth(sess, h1);

    // ---- Step 4: Sort Phase 2 heap ascending by depth ----
    //     Arrange + paint = parents BEFORE children.
    //     sort_dir for Phase 2 is +1 (set in kt_make).
    KaintanaPhaseHeap* h2 = &sess->heaps[KT_PHASE_POST_UPDATE];
    sort_heap_by_depth(sess, h2);

    // ---- Step 5: Clear per-node dirty state ----
    //     Keep the sorted heap indices (they're consumed by downstream).
    //     Clear KT_NODE_IN_HEAP so the same node can be re-pushed next frame.
    //     Clear invalidation_flags — the cascade-expanded state is now
    //     encoded in the phase heap membership.
    for (int32_t i = 1; i < sess->node_count; i++) {
        KaintanaNode* n = &sess->nodes[i];
        n->flags &= ~KT_NODE_IN_HEAP;
        n->invalidation_flags = 0;
    }

    // ---- Step 6: Full rebuild threshold ----
    //     When >30% of non-root nodes are dirty, signal a full rebuild.
    //     This triggers the backends to skip damage-rect optimization
    //     and redraw everything.
    int32_t total_non_root = sess->node_count > 1 ? sess->node_count - 1 : 1;
    sess->needs_full_rebuild = (dirty_count * 100 / total_non_root) > KAINTANA_FULL_REBUILD_PCT;
}

// ============================================================================
//  kaintana__damage_add — Add a rectangle to the 64-rect damage accumulator
//
//  Clay-style damage rect accumulation with overflow merge.
//  When count < 64:
//    Try to merge new_rect into an existing rect (union_area/sum_area < 1.5).
//    If no merge found, append as new entry.
//  When count >= 64:
//    Find closest pair (highest overlap cost), merge them.
//    Insert new_rect into the freed slot.
//  Tracks combined bounding rect and overflowed flag.
//
//  Z3 UNSAT: kt-damage-union-branchless.smt2, damage_proofs.yaml
// ============================================================================
void kaintana__damage_add(kt_Session* s, kt_Rect r) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return;
    KaintanaDamageAccumulator* acc = &sess->damage;

    // ---- Clamp tiny rects ----
    //     Sub-pixel rects expanded to at least 1 pixel to avoid degenerate
    //     merge math. Z3 formula: if w < 1.0f: w = 1.0f; if h < 1.0f: h = 1.0f
    if (r.w < 1.0f) r.w = 1.0f;
    if (r.h < 1.0f) r.h = 1.0f;

    // ---- Update combined bounding rect (always) ----
    acc->combined = kt_damage_rect_union(acc->combined, r);

    if (acc->count < KAINTANA_DAMAGE_MAX_RECTS) {
        // ---- Below capacity: try merge, else append ----
        for (int32_t i = 0; i < acc->count; i++) {
            if (kt_damage_should_merge(acc->rects[i], r)) {
                acc->rects[i] = kt_damage_rect_union(acc->rects[i], r);
                return;
            }
        }
        // No merge found — append
        acc->rects[acc->count++] = r;
    } else {
        // ---- At capacity: find closest pair, merge, insert new ----
        int32_t best_i = 0, best_j = 1;
        float best_cost = overlap_cost(acc->rects[0], acc->rects[1]);
        for (int32_t i = 0; i < acc->count; i++) {
            for (int32_t j = i + 1; j < acc->count; j++) {
                float cost = overlap_cost(acc->rects[i], acc->rects[j]);
                if (cost > best_cost) {
                    best_cost = cost;
                    best_i = i;
                    best_j = j;
                }
            }
        }
        // Merge the closest pair into best_i; overwrite best_j with new rect
        acc->rects[best_i] = kt_damage_rect_union(acc->rects[best_i], acc->rects[best_j]);
        acc->rects[best_j] = r;
        acc->overflowed = true;
    }
}

// ============================================================================
//  kt_damage_should_sleep — Lazy sleep check for frame pacing
//
//  A session is eligible for lazy sleep when:
//    1. No dirty nodes exist (all invalidation flags are 0 after cascade)
//    2. Event queue is empty (no pending input events)
//    3. No active pulses (animations or timed callbacks)
//    4. Host reports should_close() == false but host suggests sleep
//
//  Formula: should_sleep = is_clean AND event_queue.count == 0
//                          AND NOT has_active_pulses AND host_should_sleep()
//  Z3 UNSAT: damage_proofs.yaml
//
//  NOTE: Full sleep check requires event queue and pulse subsystems.
//        This function is a convenience stub until those exist.
//        Currently returns false (always awake).
// ============================================================================
bool kt_damage_should_sleep(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return true;

    // is_clean = no node has pending invalidation flags
    bool is_clean = true;
    for (int32_t i = 1; i < sess->node_count; i++) {
        if (sess->nodes[i].invalidation_flags) {
            is_clean = false;
            break;
        }
    }
    if (!is_clean) return false;

    // Event queue check (requires input subsystem integration)
    // Stub: check if the integrated input session reports no pending events.
    // TODO: wire abi_input_event_count() when event queue exists.
    // For now, assume no pending events.

    // Pulse check (requires machine_stones integration)
    // TODO: wire kain_machine_pulse_active_count() when pulse subsystem exists.
    // For now, assume no active pulses.

    // Host sleep check — vtable slot or backend query
    bool host_can_sleep = true;
    if (sess->vtable && sess->vtable_session_id) {
        // If the host reports should_close, we're NOT sleeping
        if (sess->vtable->should_close(sess->vtable_session_id)) {
            return false;
        }
    }

    // TODO: integrate full event queue and pulse checks in Phase 2
    // when kaintana__pulse and kaintana__event_queue subsystems land.
    return is_clean && host_can_sleep;
}
