; Bound the raw SIMD accumulator used by benchmark/cases/simd_lane_mix.
; The runtime SIMD lanes reduce at the end of one dot product instead of after
; every scalar add. This proof keeps that dirty path honest for the row's
; declared cells/value domain.
(set-logic QF_NIA)

(define-fun cells () Int 32768)
(define-fun max_left_after_bias () Int 1035)
(define-fun max_right () Int 511)
(define-fun max_product () Int (* max_left_after_bias max_right))
(define-fun max_total () Int (* cells max_product))
(define-fun max_vector_lane_total () Int (* (div cells 4) max_product))

(assert
  (not
    (and
      (< max_product 2147483647)
      (< max_vector_lane_total 9223372036854775807)
      (< max_total 9223372036854775807))))

(check-sat)
