(set-logic QF_NIA)

(define-fun full_cycles () Int 303)
(define-fun cycle_checksum () Int 6226000)
(define-fun tail_checksum () Int 188635)
(define-fun modulus () Int 1000000007)
(define-fun expected () Int 886666628)

(assert
  (not
    (= (mod (+ (* full_cycles cycle_checksum) tail_checksum) modulus)
       expected)))

(check-sat)
