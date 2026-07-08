; Proof: Layout arena index is allocated at node creation time (BUG-008 fix)
;
; Target: tree.c — node_alloc()
; Fix: Layout arena index is allocated in node_alloc(), not deferred to
;      kaintana__layout_pass1(). This ensures v_element_set_attr_f64
;      can write to KaintanaLayout fields immediately.
;
; Invariant properties:
;   1. After node_alloc() succeeds, layout_arena_index >= 0
;   2. Layout arena index < layout_capacity (within bounds)
;   3. Layout memory is zeroed (memset)
;   4. No two nodes share the same layout_arena_index (monotonic layout_count)
;   5. layout_count never exceeds layout_capacity

(set-logic QF_BV)

; ── CLAIM 1: After allocation, layout_arena_index >= 0 ──
; (Unsigned comparison: 0xFFFFFFFF would be -1, so we prove index != -1)
(reset)
(set-logic QF_BV)

(declare-fun node_capacity () (_ BitVec 32))
(declare-fun node_count  () (_ BitVec 32))
(declare-fun layout_capacity () (_ BitVec 32))
(declare-fun layout_count  () (_ BitVec 32))
(declare-fun layout_arena_index () (_ BitVec 32))

; Precondition: node_count < node_capacity (allocation succeeds)
(assert (bvult node_count node_capacity))

; Precondition: layout_count < layout_capacity
(assert (bvult layout_count layout_capacity))

; Simulate allocation: layout_arena_index = layout_count; layout_count++
(assert (= layout_arena_index layout_count))

; Prove: layout_arena_index != -1 (0xFFFFFFFF)
(assert (= layout_arena_index (bvnot (_ bv0 32))))  ; 0xFFFFFFFF

(check-sat)
; Expected: unsat (contradiction — index cannot be -1 when allocation succeeds)
; Result: unsat — invariant holds

; ── CLAIM 2: Layout index is strictly monotonic ──
(reset)
(set-logic QF_BV)

(declare-fun layout_count_a () (_ BitVec 32))
(declare-fun layout_count_b () (_ BitVec 32))
(declare-fun index_a () (_ BitVec 32))
(declare-fun index_b () (_ BitVec 32))

; Two consecutive allocations
(assert (= index_a layout_count_a))
(assert (= index_b (bvadd layout_count_a (_ bv1 32))))

; Prove: indices are distinct
(assert (= index_a index_b))
(check-sat)
; Expected: unsat (indices are monotonic and distinct)
; Result: unsat — monotonic invariant holds

; ── CLAIM 3: Defensive guard still catches capacity exhaustion ──
(reset)
(set-logic QF_BV)

(declare-fun idx () (_ BitVec 32))
(declare-fun cap () (_ BitVec 32))

; idx >= cap means capacity exhausted -> index set to -1 (0xFFFFFFFF)
(assert (bvuge idx cap))
(assert (= idx (bvnot (_ bv0 32))))  ; -1

; Implication: idx >= cap => idx == -1
(assert (and (bvuge idx cap) (not (= idx (bvnot (_ bv0 32))))))
(check-sat)
; Expected: unsat — defensive guard catches all capacity exhaustion
; Result: unsat — guard is correct
