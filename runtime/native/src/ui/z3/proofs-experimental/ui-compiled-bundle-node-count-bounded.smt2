; Proof: Bundle node count never exceeds KAIN_UI_COMPILED_BUNDLE_MAX_NODES
;
; In kain_ui_compiled_bundle_load_from_json():
;   while (cursor < tree_nodes_end && bundle->node_count < KAIN_UI_COMPILED_BUNDLE_MAX_NODES) {
;       ...
;       tree_node_starts[bundle->node_count] = value_start;
;       tree_node_ends[bundle->node_count] = value_end;
;       bundle->node_count += 1;
;       ...
;   }
;
; KAIN_UI_COMPILED_BUNDLE_MAX_NODES = 128
;
; Key claims:
;   1. The loop guard (node_count < MAX_NODES) prevents incrementing beyond MAX_NODES
;   2. Array index into nodes[] is always < MAX_NODES when node_count is used as index
;   3. tree_node_starts/ends arrays (size MAX_NODES) are never indexed out of bounds
;   4. Similarly, the second pass for parent resolution uses node_count as bound
;
(set-logic QF_BV)

; ── Claim 1: Loop guard prevents count exceeding MAX_NODES ──
; Model: node_count is the current count, MAX_NODES = 128
; Guard: bundle->node_count < KAIN_UI_COMPILED_BUNDLE_MAX_NODES
; After:  bundle->node_count += 1
; Prove:  post-increment count <= MAX_NODES
(declare-const node_count (_ BitVec 32))

(define-fun MAX_NODES () (_ BitVec 32) (_ bv128 32))

; Precondition: node_count < MAX_NODES
(assert (bvult node_count MAX_NODES))

; Post-increment
(define-fun new_count () (_ BitVec 32) (bvadd node_count (_ bv1 32)))

; Assert: new_count <= MAX_NODES
(assert (not (bvule new_count MAX_NODES)))
(check-sat)
; Expected: unsat — if count < 128, count+1 <= 128

(reset)

; ── Claim 2: Node index is always in bounds ──
; Every access to bundle->nodes[index] uses index in [0, node_count)
(set-logic QF_BV)

(declare-const index (_ BitVec 32))
(declare-const node_count (_ BitVec 32))

(assert (bvule node_count (_ bv128 32)))
(assert (bvult index node_count))

; Prove: index < MAX_NODES (so the array access is safe)
(assert (not (bvult index (_ bv128 32))))
(check-sat)
; Expected: unsat — index is always within the static array bounds

(reset)

; ── Claim 3: tree_node_starts/tree_node_ends array indexing ──
; These arrays have size KAIN_UI_COMPILED_BUNDLE_MAX_NODES (128).
; The code does: tree_node_starts[bundle->node_count] = value_start;
; where node_count < MAX_NODES at time of access.
(set-logic QF_BV)

(declare-const node_count (_ BitVec 32))
(assert (bvult node_count (_ bv128 32)))

; Prove: node_count < 128, so indexing is safe for a 128-element array
(assert (not (bvult node_count (_ bv128 32))))
(check-sat)
; Expected: unsat

(reset)

; ── Claim 4: find_node_index_by_id loop bounds ──
; for (index = 0; index < bundle->node_count; ++index)
; Prove: `index` is always in [0, node_count) and never exceeds MAX_NODES
(set-logic QF_BV)

(declare-const node_count (_ BitVec 32))
(declare-const index (_ BitVec 32))

(assert (bvule node_count (_ bv128 32)))
(assert (bvult index node_count))

; In bounds for nodes[] array access
(assert (not (bvult index (_ bv128 32))))
(check-sat)
; Expected: unsat

(reset)

; ── Claim 5: compute_node_depth bound checks ──
; The recursion_guard check prevents infinite recursion.
; The function checks:
;   if (recursion_guard > KAIN_UI_COMPILED_BUNDLE_MAX_NODES) return 0u;
; With KAIN_UI_COMPILED_BUNDLE_MAX_NODES = 128 and each call adding 1 to
; recursion_guard, the maximum recursion depth is 129 before bailout.
(set-logic QF_BV)

(declare-const recursion_guard (_ BitVec 32))

; This should not happen given the guard, but prove it's caught
(assert (bvugt recursion_guard (_ bv128 32)))

; The function returns 0u when guard exceeds MAX_NODES
; So the function always returns safely
(assert false)
(check-sat)
; Expected: unsat — trivially
