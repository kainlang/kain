(set-logic QF_BV)

(declare-fun cap () (_ BitVec 64))
(declare-fun base () (_ BitVec 64))
(declare-fun addend () (_ BitVec 64))

(assert (bvugt cap #x0000000000000000))
(assert (= (bvand cap (bvsub cap #x0000000000000001)) #x0000000000000000))
(assert (bvult base cap))
(assert (bvult addend #x0000000000000008))

(define-fun idx () (_ BitVec 64)
  (bvand (bvadd base addend) (bvsub cap #x0000000000000001)))

; Prove every unrolled adjacent index remains inside the power-of-two table.
(assert (not (bvult idx cap)))

(check-sat)

