(set-logic QF_NIA)

; Proves the SIMD lane-mix affine-bias reduction:
; if dot_b = base + b * sum_r before appending one lane, then after
; appending lane (l, r), dot_b_next still equals base_next + b * sum_r_next.
; By induction this justifies replacing repeated dot(left + bias, right)
; passes with one base dot reduction plus one right-buffer sum reduction.

(declare-fun base () Int)
(declare-fun sum_r () Int)
(declare-fun dot_b () Int)
(declare-fun l () Int)
(declare-fun r () Int)
(declare-fun b () Int)

(assert (= dot_b (+ base (* b sum_r))))

(define-fun base_next () Int (+ base (* l r)))
(define-fun sum_r_next () Int (+ sum_r r))
(define-fun dot_b_next () Int (+ dot_b (* (+ l b) r)))

(assert (not (= dot_b_next (+ base_next (* b sum_r_next)))))

(check-sat)
