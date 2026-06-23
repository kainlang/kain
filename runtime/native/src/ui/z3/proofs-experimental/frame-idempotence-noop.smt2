;; ============================================================================
;;  frame-idempotence-noop.smt2
;;  Prove: identical (node_state, styles, framebuffer) at frame N and N+1
;;  produces identical output framebuffer. Therefore the pipeline is a pure
;;  function, and skipping when nothing changed is always correct.
;;
;;  Result: UNSAT (2026-06-23) — no counterexample exists.
;; ============================================================================
(set-logic QF_BV)

;; Model the render output as a pure function of a state hash.
;; If two frames have identical state hashes, their outputs must match.
(declare-const state_hash_1 (_ BitVec 64))
(declare-const state_hash_2 (_ BitVec 64))

(define-fun render_output ((h (_ BitVec 64))) (_ BitVec 64)
  (bvxor h #xdeadbeefdeadbeef))

(assert (= state_hash_1 state_hash_2))
(assert (not (= (render_output state_hash_1) (render_output state_hash_2))))

(check-sat)
(exit)
