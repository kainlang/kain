; Experimental proof for the helper-owned ownership trace used by the benchmark
; low-level memory cases.
;
; Trace:
;   alloc -> begin_collapse -> end_collapse -> begin_observe -> end_observe -> decay
;
; Claim:
;   A successful helper-owned allocation is already registered, so the repeated
;   "if state == NOT_FOUND then register_imported" pre-checks emitted around
;   begin/end/decay are dead on this trace.
;
; This proof is intentionally scoped to the helper-owned benchmark fast path,
; not to arbitrary imported pointers or nested observer counts.
(set-logic QF_BV)

(define-fun STATE_IDLE () (_ BitVec 2) #b00)
(define-fun STATE_OBSERVED () (_ BitVec 2) #b01)
(define-fun STATE_COLLAPSED () (_ BitVec 2) #b10)
(define-fun STATE_DECAYED () (_ BitVec 2) #b11)

(define-fun STATUS_OK () (_ BitVec 4) #x0)
(define-fun STATUS_NOT_FOUND () (_ BitVec 4) #xe)
(define-fun STATUS_OBSERVED () (_ BitVec 4) #xc)
(define-fun STATUS_COLLAPSED () (_ BitVec 4) #xb)
(define-fun STATUS_DECAYED () (_ BitVec 4) #xa)
(define-fun STATUS_NOT_OBSERVED () (_ BitVec 4) #x8)
(define-fun STATUS_NOT_COLLAPSED () (_ BitVec 4) #x7)

; After a successful helper-owned __kain_alloc call.
(define-fun registered0 () Bool true)
(define-fun state0 () (_ BitVec 2) STATE_IDLE)
(define-fun observers0 () (_ BitVec 1) #b0)

(define-fun needs_import_before_collapse () Bool (not registered0))
(define-fun begin_collapse_status () (_ BitVec 4)
  (ite (not registered0) STATUS_NOT_FOUND
  (ite (= state0 STATE_DECAYED) STATUS_DECAYED
  (ite (= state0 STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= state0 STATE_OBSERVED) STATUS_OBSERVED
  (ite (= observers0 #b1) STATUS_OBSERVED STATUS_OK))))))
(define-fun registered1 () Bool registered0)
(define-fun state1 () (_ BitVec 2) (ite (= begin_collapse_status STATUS_OK) STATE_COLLAPSED state0))
(define-fun observers1 () (_ BitVec 1) observers0)

(define-fun end_collapse_status () (_ BitVec 4)
  (ite (not registered1) STATUS_NOT_FOUND
  (ite (= state1 STATE_COLLAPSED) STATUS_OK STATUS_NOT_COLLAPSED)))
(define-fun registered2 () Bool registered1)
(define-fun state2 () (_ BitVec 2) (ite (= end_collapse_status STATUS_OK) STATE_IDLE state1))
(define-fun observers2 () (_ BitVec 1) observers1)

(define-fun needs_import_before_observe () Bool (not registered2))
(define-fun begin_observe_status () (_ BitVec 4)
  (ite (not registered2) STATUS_NOT_FOUND
  (ite (= state2 STATE_DECAYED) STATUS_DECAYED
  (ite (= state2 STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= observers2 #b1) STATUS_OBSERVED STATUS_OK)))))
(define-fun registered3 () Bool registered2)
(define-fun state3 () (_ BitVec 2) (ite (= begin_observe_status STATUS_OK) STATE_OBSERVED state2))
(define-fun observers3 () (_ BitVec 1) (ite (= begin_observe_status STATUS_OK) #b1 observers2))

(define-fun end_observe_status () (_ BitVec 4)
  (ite (not registered3) STATUS_NOT_FOUND
  (ite (and (= state3 STATE_OBSERVED) (= observers3 #b1)) STATUS_OK STATUS_NOT_OBSERVED)))
(define-fun registered4 () Bool registered3)
(define-fun state4 () (_ BitVec 2) (ite (= end_observe_status STATUS_OK) STATE_IDLE state3))
(define-fun observers4 () (_ BitVec 1) (ite (= end_observe_status STATUS_OK) #b0 observers3))

(define-fun needs_import_before_decay () Bool (not registered4))
(define-fun decay_status () (_ BitVec 4)
  (ite (not registered4) STATUS_NOT_FOUND
  (ite (= state4 STATE_DECAYED) STATUS_DECAYED
  (ite (= state4 STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= observers4 #b1) STATUS_OBSERVED STATUS_OK)))))
(define-fun registered5 () Bool registered4)
(define-fun state5 () (_ BitVec 2) (ite (= decay_status STATUS_OK) STATE_DECAYED state4))

(assert
  (or
    needs_import_before_collapse
    needs_import_before_observe
    needs_import_before_decay
    (not (= begin_collapse_status STATUS_OK))
    (not (= end_collapse_status STATUS_OK))
    (not (= begin_observe_status STATUS_OK))
    (not (= end_observe_status STATUS_OK))
    (not (= decay_status STATUS_OK))
    (not registered5)
    (not (= state5 STATE_DECAYED))))

(check-sat)
