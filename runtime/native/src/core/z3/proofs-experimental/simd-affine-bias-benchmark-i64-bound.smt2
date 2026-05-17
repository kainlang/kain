(set-logic QF_NIA)

; Bounds for the fused SIMD lane-mix affine path in the benchmark domain:
; cells <= 32768, 0 <= left <= 1023, 0 <= right <= 511, 0 <= bias < 13.
; This proves the raw factored inner expression stays inside signed i64.

(define-fun I64_MAX () Int 9223372036854775807)
(define-fun CELLS_MAX () Int 32768)
(define-fun LEFT_MAX () Int 1023)
(define-fun RIGHT_MAX () Int 511)
(define-fun BIAS_MAX () Int 12)

(declare-fun base_dot () Int)
(declare-fun sum_right () Int)
(declare-fun bias () Int)

(assert (<= 0 base_dot))
(assert (<= base_dot (* CELLS_MAX LEFT_MAX RIGHT_MAX)))
(assert (<= 0 sum_right))
(assert (<= sum_right (* CELLS_MAX RIGHT_MAX)))
(assert (<= 0 bias))
(assert (<= bias BIAS_MAX))

(define-fun inner_raw () Int (+ base_dot (* bias sum_right)))

(assert (not (and (<= 0 inner_raw) (<= inner_raw I64_MAX))))

(check-sat)
