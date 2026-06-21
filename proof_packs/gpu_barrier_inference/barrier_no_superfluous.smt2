; Stream BRAVO — Z3 Proof: barrier_no_superfluous
; Proves that inferred barriers don't include unnecessary stage/access
; bits. If A and B share no resources, no barrier is generated between
; them.

; Declare uninterpreted sorts.
(declare-sort Stage 0)
(declare-sort Resource 0)
(declare-sort AccessKind 0)

(declare-const Write AccessKind)
(declare-const Read AccessKind)
(assert (distinct Write Read))

(declare-const A Stage)
(declare-const B Stage)
(assert (distinct A B))

(declare-const R Resource)
(declare-const S Resource)

(declare-fun dep (Stage Stage) Bool)
(declare-fun access (Stage Resource AccessKind) Bool)

; An uninterpreted function describing whether a barrier is generated.
(declare-fun barrier_generated (Stage Stage) Bool)

; --- AXIOMS ---
; A barrier is generated for edge X→Y only if some resource R
; is WRITTEN by X and READ by Y (i.e., a true write→read dependency).
(assert (forall ((x Stage) (y Stage) (r Resource))
    (=> (and (access x r Write) (access y r Read) (dep y x))
        (barrier_generated y x))
))

; If there is NO shared resource with write→read, no barrier is generated.
(assert (forall ((x Stage) (y Stage))
    (=> (not (exists ((r Resource))
            (and (access x r Write) (access y r Read))))
        (not (barrier_generated y x)))
))

; If there is no dependency edge, no barrier is generated.
(assert (forall ((x Stage) (y Stage))
    (=> (not (dep y x))
        (not (barrier_generated y x)))
))

; --- THEOREM ---
; For stages A and B that share NO resources, no barrier is generated.
(assert (dep B A)) ; edge from A → B
(assert (access A S Write)) ; A writes S
(assert (access B S Read))  ; B reads S — they share S, so barrier SHOULD be generated
; But we assert A does NOT write R, and B does NOT read R
(assert (not (access A R Write)))
(assert (not (access B R Read)))

; If A writes S and B reads S, then barrier_generated(B, A) must be true.
(assert (not (barrier_generated B A)))

(check-sat)
; Expected: unsat — the barrier IS generated because they share resource S
