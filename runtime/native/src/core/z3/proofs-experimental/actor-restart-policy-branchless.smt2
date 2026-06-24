; Proof: Branchless restart predicate == switch-based restart decision
;
; KainRestartPolicy: PERMANENT=0, TEMPORARY=1, TRANSIENT=2
; KainActorExitReason: NORMAL=0, SHUTDOWN=1, KILLED=2, CRASHED=3, SUPERVISOR_ESCALATION=4
;
; Switch rules:
;   PERMANENT:  always restart (1)
;   TEMPORARY:  never restart (0)
;   TRANSIENT:  restart only on abnormal exit
;     abnormal = exit_reason != NORMAL && exit_reason != SHUTDOWN
;   default:    0
;
; Branchless candidate:
;   abnormal = exit_reason > 1
;   return (policy == 0) | ((policy == 2) & abnormal)

(set-logic QF_BV)

(declare-const policy (_ BitVec 2))
(declare-const exit_reason (_ BitVec 3))

; Bound to valid values
(assert (bvule policy (_ bv2 2)))
(assert (bvule exit_reason (_ bv4 3)))

; Reference: switch-based
(define-fun reference () (_ BitVec 1)
  (ite (= policy (_ bv0 2)) (_ bv1 1)
    (ite (= policy (_ bv1 2)) (_ bv0 1)
      (ite (= policy (_ bv2 2))
        (ite (and (not (= exit_reason (_ bv0 3)))
                  (not (= exit_reason (_ bv1 3))))
          (_ bv1 1) (_ bv0 1))
        (_ bv0 1)))))

; Candidate: arithmetic predicate
(define-fun abnormal () (_ BitVec 1)
  (ite (bvugt exit_reason (_ bv1 3)) (_ bv1 1) (_ bv0 1)))

(define-fun candidate () (_ BitVec 1)
  (bvor
    (ite (= policy (_ bv0 2)) (_ bv1 1) (_ bv0 1))
    (bvand
      (ite (= policy (_ bv2 2)) (_ bv1 1) (_ bv0 1))
      abnormal)))

(assert (not (= reference candidate)))
(check-sat)
; unsat = branchless predicate is equivalent for all valid inputs
