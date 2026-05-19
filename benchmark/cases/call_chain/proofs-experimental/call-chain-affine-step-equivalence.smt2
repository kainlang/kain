(set-logic NIA)

(define-fun modulus () Int 1000000007)
(declare-const value Int)

(define-fun step_a ((v Int)) Int
  (mod (+ (* v 3) 1) modulus))

(define-fun step_b ((v Int)) Int
  (mod (* (+ (step_a v) 5) 7) modulus))

(define-fun step_c ((v Int)) Int
  (mod (+ (step_b v) (step_a (+ v 11)) 13) modulus))

(define-fun step_d ((v Int)) Int
  (mod (+ (* (step_c v) 3) (step_b (+ v 17)) 19) modulus))

(define-fun affine_step ((v Int)) Int
  (mod (+ (* v 93) 685) modulus))

(assert (not (= (step_d value) (affine_step value))))
(check-sat)
