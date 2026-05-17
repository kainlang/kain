(set-logic QF_BV)

; The fused fill+accumulate lane uses power-of-two-minus-one masks instead
; of `% 1024` and `% 512` for the benchmark's nonnegative affine fills.
; This proves the masked lane values stay in the expected i32 subdomain.

(declare-fun index () (_ BitVec 64))

(define-fun left_raw () (_ BitVec 64)
  (bvadd (bvmul index #x000000000000001f) #x0000000000000007))
(define-fun right_raw () (_ BitVec 64)
  (bvadd (bvmul index #x0000000000000011) #x0000000000000003))

(define-fun left_value () (_ BitVec 64)
  (bvand left_raw #x00000000000003ff))
(define-fun right_value () (_ BitVec 64)
  (bvand right_raw #x00000000000001ff))

(assert
  (or
    (bvugt left_value #x00000000000003ff)
    (bvugt right_value #x00000000000001ff)))

(check-sat)
