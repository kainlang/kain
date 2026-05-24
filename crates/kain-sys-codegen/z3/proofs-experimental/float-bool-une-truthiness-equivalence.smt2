(set-logic QF_FP)

(declare-const x (_ FloatingPoint 11 53))

(define-fun zero () (_ FloatingPoint 11 53) (_ +zero 11 53))
(define-fun runtime_truthy () Bool (not (fp.eq x zero)))
(define-fun llvm_truthy_after_fix () Bool
  (or (fp.isNaN x) (not (fp.eq x zero))))

(assert (xor runtime_truthy llvm_truthy_after_fix))
(check-sat)
