; Stream BRAVO — Z3 Proof: parallel_no_barrier
; Proves that parallel (independent) stages in the orchestrate DAG
; produce no barriers between them, even if they access the same
; resource (since no execution ordering is guaranteed).

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

(declare-fun dep (Stage Stage) Bool)
(declare-fun access (Stage Resource AccessKind) Bool)
(declare-fun barrier_generated (Stage Stage) Bool)

; Barrier only generated when there IS a dependency edge AND a write→read conflict.
(assert (forall ((x Stage) (y Stage))
    (=> (barrier_generated y x) (dep y x))
))

; --- THEOREM ---
; Stages A and B have NO dependency edge (they are parallel).
; Even though A writes R and B reads R, no barrier should be generated.

; No dependency between A and B in either direction.
(assert (not (dep B A)))
(assert (not (dep A B)))

; A writes R, B reads R — a potential data conflict.
(assert (access A R Write))
(assert (access B R Read))

; Assert that a barrier IS generated (negation of the property we want to prove).
(assert (barrier_generated B A))

(check-sat)
; Expected: unsat — no barrier is generated for parallel stages
