; Proves the CUDA/PTX runtime dispatch-group formula keeps the exact contract we
; rely on at launch time:
;   1. raw zero values are sanitized to one
;   2. q = floor((safe_dispatch - 1) / safe_workgroup)
;   3. runtime_group_count = q + 1 is positive, covers the dispatch, and is
;      the smallest such positive group count

(set-logic QF_NIA)

(declare-const raw_dispatch Int)
(declare-const raw_workgroup Int)
(declare-const q Int)
(declare-const r Int)

(assert (>= raw_dispatch 0))
(assert (>= raw_workgroup 0))
(assert (>= q 0))
(assert (>= r 0))

(define-fun safe_dispatch () Int
  (ite (< raw_dispatch 1) 1 raw_dispatch))

(define-fun safe_workgroup () Int
  (ite (< raw_workgroup 1) 1 raw_workgroup))

(assert (< r safe_workgroup))
(assert (= (- safe_dispatch 1) (+ (* safe_workgroup q) r)))

(define-fun runtime_group_count () Int (+ q 1))

; Negate the contract: either the result is non-positive, does not cover the
; requested dispatch, or is not minimal. Unsat proves all three cannot happen.
(assert
  (or
    (< runtime_group_count 1)
    (< (* runtime_group_count safe_workgroup) safe_dispatch)
    (>= (* (- runtime_group_count 1) safe_workgroup) safe_dispatch)))

(check-sat)
