; Experimental proof for the actor-table occupancy bitset path in
; actor.c.
; Claim: composing slot = word_index * 64 + bit_index stays in the live actor
; table range when word_index is in [0, 15], bit_index is in [0, 63], and the
; reserved invalid slot bit 0 is excluded for word 0.
(set-logic QF_BV)

(declare-fun word_index () (_ BitVec 64))
(declare-fun bit_index () (_ BitVec 64))

(define-fun slot () (_ BitVec 64)
  (bvadd (bvmul word_index #x0000000000000040) bit_index))

(assert
  (and
    (bvule word_index #x000000000000000f)
    (bvule bit_index #x000000000000003f)
    (=> (= word_index #x0000000000000000)
        (bvuge bit_index #x0000000000000001))
    (or
      (bvult slot #x0000000000000001)
      (bvugt slot #x00000000000003ff))))
(check-sat)
