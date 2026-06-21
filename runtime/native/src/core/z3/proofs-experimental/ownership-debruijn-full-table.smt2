; Proof: Comprehensive de Bruijn table injectivity
;
; The kain_ownership_low_bit_index_u64 function maps a one-hot 64-bit value
; to a bit index [0, 63] using a de Bruijn multiplication followed by
; a lookup table.
;
; This proof: no two distinct power-of-two inputs map to the same table index.
;
(set-logic QF_BV)
(define-fun debruijn_selector ((one_hot (_ BitVec 64))) (_ BitVec 6)
  ((_ extract 63 58) (bvmul one_hot #x03f79d71b4cb0a89)))

; For each of the 64 power-of-two values, compute the table index
; and assert they are all distinct.
; We use pairwise comparison: pick i, j and assert collision impossible.

(declare-const i (_ BitVec 6))
(declare-const j (_ BitVec 6))
(assert (distinct i j))

(define-fun pow2_i () (_ BitVec 64) (bvshl (_ bv1 64) ((_ zero_extend 58) i)))
(define-fun pow2_j () (_ BitVec 64) (bvshl (_ bv1 64) ((_ zero_extend 58) j)))

; De Bruijn selector collision
(assert (= (debruijn_selector pow2_i) (debruijn_selector pow2_j)))
(check-sat)
