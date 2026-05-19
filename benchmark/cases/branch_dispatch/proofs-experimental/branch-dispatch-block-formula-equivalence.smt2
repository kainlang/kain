(set-logic NIA)

(declare-const k Int)
(assert (>= k 0))

(define-fun block_scalar ((k Int)) Int
  (+ (+ (* 8 k) 1)
     (+ (* (+ (* 8 k) 1) 3) 7)
     (- (+ (* 8 k) 2) 5)
     (+ (* (+ (* 8 k) 3) (+ (* 8 k) 3)) 11)
     (+ (+ (* 8 k) 4) 17)
     (- (* (+ (* 8 k) 5) 5) 13)
     (+ (+ (* 8 k) 6) 23)
     (- (+ (* 8 k) 7) 11)))

(define-fun block_formula ((k Int)) Int
  (+ (* 64 k k) (* 152 k) 86))

(assert (not (= (block_scalar k) (block_formula k))))
(check-sat)
