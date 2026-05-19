; Prove that non-negative 64-bit pointer indices below 2^60 can scale by
; eight with either multiply or left shift without changing the byte offset.

(set-logic QF_BV)

(declare-const offset (_ BitVec 64))

; 0 <= offset < 2^60 keeps offset * 8 within signed 64-bit range.
(assert (bvsge offset (_ bv0 64)))
(assert (bvslt offset (_ bv1152921504606846976 64)))

(define-fun scaled_mul () (_ BitVec 64)
  (bvmul offset (_ bv8 64)))

(define-fun scaled_shl () (_ BitVec 64)
  (bvshl offset (_ bv3 64)))

(assert (not (= scaled_mul scaled_shl)))
(check-sat)
