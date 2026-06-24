; Proof: Task terminal state equivalence
;
; KainTaskState enum: PENDING=0, READY=1, RUNNING=2, COMPLETED=3, CANCELLED=4, FAILED=5
;
; Original (3-way OR):
;   state == KAIN_TASK_STATE_COMPLETED ||
;   state == KAIN_TASK_STATE_CANCELLED ||
;   state == KAIN_TASK_STATE_FAILED
;
; Candidate (single comparison):
;   state > KAIN_TASK_STATE_RUNNING
;   i.e., state >= 3
;
; Domain: state ∈ {0,1,2,3,4,5}
;
; Result: unsat — no counterexample exists.
;   The candidate is equivalent to the original for all valid states.

(set-logic QF_BV)

(define-fun terminal_original ((s (_ BitVec 64))) Bool
  (or (= s #x0000000000000003)
      (= s #x0000000000000004)
      (= s #x0000000000000005)))

(define-fun terminal_candidate ((s (_ BitVec 64))) Bool
  (bvugt s #x0000000000000002))

(declare-const s (_ BitVec 64))

; Constrain to only valid enum values [0,5]
(assert (bvule s #x0000000000000005))

; Claim: original and candidate are always equal → if NOT equal, counterexample exists
(assert (not (= (terminal_original s) (terminal_candidate s))))

(check-sat)
(get-info :all-statistics)
