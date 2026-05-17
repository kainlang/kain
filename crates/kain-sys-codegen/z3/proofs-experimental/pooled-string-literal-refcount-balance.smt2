(set-logic QF_LIA)

; Pooled function-scope string literals start with one root-scope reference.
; Binding that literal into a normal local retains once, then the local cleanup
; and root cleanup each release once. Returning the pooled literal retains once
; before root cleanup, so the caller still observes one live reference.

(declare-const root_initial Int)
(declare-const bound_after_retain Int)
(declare-const bound_after_local_cleanup Int)
(declare-const bound_final Int)
(declare-const returned_after_retain Int)
(declare-const returned_final Int)

(assert (= root_initial 1))
(assert (= bound_after_retain (+ root_initial 1)))
(assert (= bound_after_local_cleanup (- bound_after_retain 1)))
(assert (= bound_final (- bound_after_local_cleanup 1)))
(assert (= returned_after_retain (+ root_initial 1)))
(assert (= returned_final (- returned_after_retain 1)))

; It should be impossible for the bound-local path to finish anywhere except 0,
; and impossible for the return path to finish anywhere except 1.
(assert (or (not (= bound_final 0)) (not (= returned_final 1))))

(check-sat)
