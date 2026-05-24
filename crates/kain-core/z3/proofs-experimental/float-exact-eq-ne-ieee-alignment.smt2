(set-logic QF_FP)

(declare-const a (_ FloatingPoint 11 53))
(declare-const b (_ FloatingPoint 11 53))

(define-fun runtime_eq_after_fix () Bool (fp.eq a b))
(define-fun llvm_eq () Bool
  (and (not (fp.isNaN a)) (not (fp.isNaN b)) (fp.eq a b)))
(define-fun runtime_ne_after_fix () Bool (not (fp.eq a b)))
(define-fun llvm_ne_after_fix () Bool
  (or (fp.isNaN a) (fp.isNaN b) (not (fp.eq a b))))

(assert
  (or (xor runtime_eq_after_fix llvm_eq)
      (xor runtime_ne_after_fix llvm_ne_after_fix)))
(check-sat)
