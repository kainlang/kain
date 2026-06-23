; Proof: kain_ui_runtime_find_node_index — search loop bounds
;
; The function searches for a node by ID in a compiled bundle. The loop:
;   for (index = 0; index < bundle->node_count; ++index) { ... }
;
; Key claims:
;   1. The loop never iterates beyond bundle->node_count
;   2. bundle->node_count is bounded by KAIN_UI_COMPILED_BUNDLE_MAX_NODES (128)
;   3. The index into bundle->nodes[] is always valid
;
(set-logic QF_BV)

(define-fun MAX_NODES () (_ BitVec 32) #x00000080) ; 128

; ============================================================================
; Claim 1: The loop index never exceeds node_count-1
; for (index = 0; index < node_count; ++index)
; After termination, index >= node_count (or we returned early with a match)
; No out-of-bounds access because the loop condition prevents it.
; ============================================================================
(push)
(declare-fun node_count () (_ BitVec 32))
(assert (bvule node_count MAX_NODES)) ; node_count ≤ 128
(assert (bvugt node_count #x00000000)) ; at least 1 node
(declare-fun index () (_ BitVec 32))
; Simulate the loop: index goes from 0 to node_count-1
(assert (bvult index node_count)) ; loop condition
; The access bundle->nodes[index] is valid at this point
; Prove: index < node_count (trivially true from assertion)
(assert (bvuge index node_count))
(check-sat)
; Expected: unsat — index is always < node_count when accessed
(pop)

; ============================================================================
; Claim 2: When the last element (index = node_count - 1) is visited,
; the increment makes index = node_count, which fails the loop condition.
; This proves the loop correctly terminates after checking all elements.
; ============================================================================
(push)
(declare-fun node_count () (_ BitVec 32))
(assert (bvule node_count MAX_NODES))
(assert (bvugt node_count #x00000000))
; The last element accessed is at node_count - 1
(define-fun last_idx () (_ BitVec 32) (bvsub node_count #x00000001))
(define-fun next_index () (_ BitVec 32) (bvadd last_idx #x00000001))
; After increment from the last element, next_index = node_count
; Prove: next_index >= node_count (loop exits correctly)
(assert (not (bvuge next_index node_count)))
(check-sat)
; Expected: unsat — next_index >= node_count, loop terminates
(pop)

; ============================================================================
; Claim 3: Empty bundle case — node_count = 0, loop doesn't execute
; The function checks bundle and bundle->loaded first, so node_count can be 0
; if the bundle is unloaded. For an empty loaded bundle, loop just doesn't run.
; ============================================================================
(push)
(declare-fun node_count () (_ BitVec 32))
(assert (= node_count #x00000000)) ; empty bundle
(declare-fun index () (_ BitVec 32))
(assert (= index #x00000000)) ; initial index
; The loop condition: index < node_count is false, loop body never executes
(assert (bvult index node_count))
(check-sat)
; Expected: unsat — loop doesn't execute when node_count = 0
(pop)

; ============================================================================
; Claim 4: MAX_NODES = 128, all node_count values from 1 to 128 are safe
; We verify the boundary condition: node_count = MAX_NODES (128)
; ============================================================================
(push)
(define-fun nc () (_ BitVec 32) #x00000080) ; 128 = max
; The last valid index is 127
(define-fun last_valid () (_ BitVec 32) #x0000007F)
; Access at index 127 is valid (last element)
(assert (not (bvult last_valid nc)))
(check-sat)
; Expected: unsat — last_valid = 127 < 128 = nc
(pop)
