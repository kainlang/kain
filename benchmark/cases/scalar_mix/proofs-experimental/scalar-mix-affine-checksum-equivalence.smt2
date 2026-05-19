(set-logic NIA)

(declare-const i Int)
(define-fun offset () Int 22)
(define-fun modulus () Int 1000000007)
(define-fun iterations () Int 2000000)
(define-fun expected () Int 42986000)

(assert (>= i 0))

(define-fun triangular_before () Int
  (div (* i (- i 1)) 2))
(define-fun triangular_after () Int
  (div (* (+ i 1) i) 2))
(define-fun closed_before () Int
  (mod (+ (* i offset) triangular_before) modulus))
(define-fun closed_after () Int
  (mod (+ (* (+ i 1) offset) triangular_after) modulus))
(define-fun scalar_step_after () Int
  (mod (+ closed_before i offset) modulus))
(define-fun benchmark_closed () Int
  (mod (+ (* iterations offset)
          (div (* iterations (- iterations 1)) 2))
       modulus))

(assert
  (or
    (not (= closed_after scalar_step_after))
    (not (= benchmark_closed expected))))

(check-sat)
