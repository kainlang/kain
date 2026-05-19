(set-logic NIA)

(define-fun blocks () Int 375000)
(define-fun modulus () Int 1000000007)
(define-fun expected () Int 632706747)

(define-fun sum_k () Int
  (div (* blocks (- blocks 1)) 2))

(define-fun sum_k2 () Int
  (div (* blocks (- blocks 1) (- (* 2 blocks) 1)) 6))

(define-fun closed_checksum () Int
  (mod (+ (* 64 sum_k2) (* 152 sum_k) (* 86 blocks)) modulus))

(assert (not (= closed_checksum expected)))
(check-sat)
