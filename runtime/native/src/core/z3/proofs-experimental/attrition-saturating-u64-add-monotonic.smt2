(set-logic QF_BV)

(declare-fun left () (_ BitVec 64))
(declare-fun right () (_ BitVec 64))

(define-fun sum () (_ BitVec 64) (bvadd left right))
(define-fun overflow () Bool (bvult sum left))
(define-fun sat () (_ BitVec 64)
  (ite overflow
       (_ bv18446744073709551615 64)
       sum))

; The helper must never wrap backward. In the overflow branch it clamps to all-ones.
(assert
  (or
    (bvult sat left)
    (bvult sat right)))

(check-sat)
