; Claim: de Bruijn table maps all 64 single-bit values to unique indices [0,63]
; The existing table in actor.c (kain_actor_low_bit_index_u64, line 313-328)
; uses the standard 0x03f79d71b4cb0a89 64-bit de Bruijn constant.
;
; Proof: for every pair of distinct single-bit (power-of-two) values,
; the de Bruijn index is different. This guarantees the lookup table
; has no collisions for any valid low-bit isolation result.
;
; Solver result: unsat — all 64 single-bit values map to unique indices
(set-logic QF_BV)

(define-fun debruijn_mul ((v (_ BitVec 64))) (_ BitVec 64)
  (bvmul v #x03f79d71b4cb0a89))
(define-fun debruijn_idx ((v (_ BitVec 64))) (_ BitVec 6)
  ((_ extract 63 58) (debruijn_mul v)))

; For every pair of distinct single-bit values, indices must differ
(declare-const a (_ BitVec 64))
(declare-const b (_ BitVec 64))

; Constrain to single-bit values (power-of-two)
(assert (not (= a (_ bv0 64))))
(assert (= (bvand a (bvsub a (_ bv1 64))) (_ bv0 64)))
(assert (not (= b (_ bv0 64))))
(assert (= (bvand b (bvsub b (_ bv1 64))) (_ bv0 64)))
(assert (not (= a b)))

; If they're different single-bit values, they must have different indices
(assert (= (debruijn_idx a) (debruijn_idx b)))
(check-sat)
; unsat = no collisions ✅
