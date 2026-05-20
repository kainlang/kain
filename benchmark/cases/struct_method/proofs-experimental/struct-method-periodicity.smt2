; struct-method-periodicity.smt2
; Prove the scalar per-iteration score is periodic with period 97 * 101.

(set-logic NIA)

(declare-const seed Int)

(define-fun pair_score ((value Int)) Int
  (+ (* 3 (mod value 97))
     (* 5 (mod (* value 7) 101))))

(assert (not (= (pair_score seed)
                (pair_score (+ seed 9797)))))

(check-sat)
