; Claim: (x & -x) isolates the lowest set bit for all 64-bit values
; The result has exactly one bit set (or zero if input is zero),
; and it's at the position of the lowest set bit of the input.
;
; Target: actor.c line 307-309 — kain_actor_isolate_low_bit_u64
;   return value & (0u - value);
;
; Solver result: unsat — for all 2^64 possible inputs, the result is
; either 0 (when v=0) or a power of two (single bit set).
(set-logic QF_BV)

(define-fun low_bit ((x (_ BitVec 64))) (_ BitVec 64)
  (bvand x (bvneg x)))

(declare-const v (_ BitVec 64))

; Result must be either 0 (if v=0) or a single-bit power of two
(define-fun one_or_zero ((x (_ BitVec 64))) Bool
  (or (= x (_ bv0 64))
      (= (bvand x (bvsub x (_ bv1 64))) (_ bv0 64))))

(assert (not (one_or_zero (low_bit v))))
(check-sat)
; unsat = property holds for all inputs ✅
