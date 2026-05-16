; Experimental proof for the ownership registry occupancy-word allocator in
; ownership.c.
; Claim: composing slot = word_index * 64 + bit_index stays inside the
; 4096-entry ownership-region table whenever the occupancy scan inputs are in
; their compiled ranges.
(set-logic QF_BV)

(declare-fun word_index () (_ BitVec 64))
(declare-fun bit_index () (_ BitVec 64))

(define-fun slot () (_ BitVec 64)
  (bvadd (bvmul word_index #x0000000000000040) bit_index))

(assert
  (and
    (bvule word_index #x000000000000003f)
    (bvule bit_index #x000000000000003f)
    (bvugt slot #x0000000000000fff)))
(check-sat)
