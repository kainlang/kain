(set-logic QF_FPBV)

(declare-const x (_ FloatingPoint 11 53))

(define-fun min_i64 () (_ FloatingPoint 11 53)
  ((_ to_fp 11 53) RNE (- 9223372036854775808.0)))
(define-fun max_i64_exclusive () (_ FloatingPoint 11 53)
  ((_ to_fp 11 53) RNE 9223372036854775808.0))
(define-fun sat_contract_i64 () (_ BitVec 64)
  (ite (fp.isNaN x)
       #x0000000000000000
       (ite (fp.leq x min_i64)
            #x8000000000000000
            (ite (fp.geq x max_i64_exclusive)
                 #x7fffffffffffffff
                 ((_ fp.to_sbv 64) RTZ x)))))

(assert (not (fp.isNaN x)))
(assert (not (fp.isInfinite x)))
(assert (fp.geq x min_i64))
(assert (fp.lt x max_i64_exclusive))
(assert (not (= sat_contract_i64 ((_ fp.to_sbv 64) RTZ x))))
(check-sat)
