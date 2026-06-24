;; ============================================================
;; Proof: kain_clampd branchless via fmax/fmin
;;
;; Original: 2-branch ladder
;; Candidate: fmax(fmin(v, hi), lo)
;;
;; We prove: fmax(lo, fmin(v, hi)) ≡ clamp(v, lo, hi)
;; Domain: all finite doubles with lo <= hi, no NaN
;; ============================================================
(set-logic QF_FP)

(declare-const v  (_ FloatingPoint 11 53))
(declare-const lo (_ FloatingPoint 11 53))
(declare-const hi (_ FloatingPoint 11 53))

;; Preconditions: lo <= hi, all finite (no NaN/inf)
(assert (not (fp.isNaN v)))
(assert (not (fp.isNaN lo)))
(assert (not (fp.isNaN hi)))
(assert (not (fp.isInfinite v)))
(assert (not (fp.isInfinite lo)))
(assert (not (fp.isInfinite hi)))
(assert (not (fp.gt lo hi)))  ; lo <= hi

;; Original: branch ladder
(define-fun orig () (_ FloatingPoint 11 53)
  (ite (fp.lt v lo) lo
  (ite (fp.gt v hi) hi
    v)))

;; Candidate: fmax(fmin(v, hi), lo)  — 0 branches
;; IEEE 754-2008: maxNum/minNum semantics (or max/min)
(define-fun cand () (_ FloatingPoint 11 53)
  (fp.max lo (fp.min v hi)))

;; Prove equivalence
(assert (not (fp.eq orig cand)))

(check-sat)
(get-model)
