; AVX2/AVX-512 _mm*_mul_epu32 multiplies even 32-bit lanes.
; For Kain's SIMD benchmark domain, each i64 cell stores a nonnegative i32
; value, so the low 32-bit lane product is the same product as scalar i64 math.
(set-logic QF_BV)

(declare-fun left () (_ BitVec 64))
(declare-fun right () (_ BitVec 64))
(declare-fun bias () (_ BitVec 64))

(define-fun biased_left () (_ BitVec 64)
  (bvadd left bias))

(define-fun avx_even_dword_product () (_ BitVec 64)
  (bvmul
    ((_ zero_extend 32) ((_ extract 31 0) biased_left))
    ((_ zero_extend 32) ((_ extract 31 0) right))))

(define-fun scalar_i64_product () (_ BitVec 64)
  (bvmul biased_left right))

(assert (bvule left #x000000007fffffff))
(assert (bvule right #x000000007fffffff))
(assert (bvule bias #x000000007fffffff))
(assert (bvule biased_left #x000000007fffffff))

(assert (not (= avx_even_dword_product scalar_i64_product)))

(check-sat)
