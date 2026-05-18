(set-logic ALL)

(define-fun depth () Int 128)
(define-fun iterations () Int 5000)
(define-fun modulus () Int 1000000007)
(define-fun expected () Int 41280000)

(define-fun-rec recursive_sum ((value Int)) Int
  (ite (<= value 0)
       0
       (+ value (recursive_sum (- value 1)))))

(define-fun triangular_sum ((value Int)) Int
  (div (* value (+ value 1)) 2))

(assert
  (not
    (and
      (= (recursive_sum depth) (triangular_sum depth))
      (= (mod (* iterations (triangular_sum depth)) modulus) expected))))

(check-sat)
