; Claim: generation wraparound never produces 0
; In actor_table_insert (line 1866):
;   unsigned int next_generation = g_actor_table.generations[id] + 1u;
;   if (next_generation == 0u) { next_generation = 1u; }
;
; Proof: for all 32-bit generation values (0..0xFFFFFFFF), the corrected
; value is never 0. This ensures KAIN_ACTOR_ID_INVALID (0) generation
; is never assigned to a live actor, making generation-0 always stale.
;
; Solver result: unsat — invariant holds for all 2^32 generation values
(set-logic QF_BV)
(declare-const gen (_ BitVec 32))

; The actual computation from actor.c:
(define-fun next ((g (_ BitVec 32))) (_ BitVec 32)
  (let ((g_inc (bvadd g (_ bv1 32))))
    (ite (= g_inc (_ bv0 32)) (_ bv1 32) g_inc)))

; Assert: next(gen) is never 0 for any gen
(assert (= (next gen) (_ bv0 32)))
(check-sat)
; unsat = corrected generation is never 0 ✅
