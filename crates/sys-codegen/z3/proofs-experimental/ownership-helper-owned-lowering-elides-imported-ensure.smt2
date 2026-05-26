; Experimental proof for the LLVM helper-owned ownership lowering.
;
; Target question:
;   If a pointer is known helper-owned and already registered by __kain_alloc,
;   can LLVM skip __kain_ownership_ensure_imported(...) and route
;   collapse/observe/decay through the helper-specific entrypoints without
;   changing the ownership-state trace?
;
; Scope:
;   This is intentionally narrow. It models the alloc_churn / ownership_memory
;   shape where a fresh helper-owned cell starts Idle with zero observers and
;   runs:
;     ensure_imported? -> begin_collapse -> end_collapse
;                      -> begin_observe  -> end_observe -> decay
;
; Claim:
;   Under the helper-owned precondition, the imported path and the helper path
;   produce the same statuses and the same final state, so the imported ensure
;   preamble is dead work for this trace.
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

; Fresh helper-owned allocation contract after __kain_alloc.
(define-fun registered0 () Bool true)
(define-fun helper_slot_valid0 () Bool true)
(define-fun state0 () (_ BitVec 2) STATE_IDLE)
(define-fun observers0 () (_ BitVec 2) #b00)

; Generic imported-side preamble. Because the region already exists, ensure_imported
; is required to be a no-op that returns OK.
(define-fun ensure_imported_status () (_ BitVec 4)
  (ite registered0 STATUS_OK STATUS_OK))
(define-fun registered1 () Bool registered0)
(define-fun helper_slot_valid1 () Bool helper_slot_valid0)
(define-fun state1 () (_ BitVec 2) state0)
(define-fun observers1 () (_ BitVec 2) observers0)

(define-fun generic_begin_collapse_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (registered Bool)) (_ BitVec 4)
  (ite (not registered) STATUS_NOT_FOUND
  (ite (= state STATE_DECAYED) STATUS_DECAYED
  (ite (= state STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= state STATE_OBSERVED) STATUS_OBSERVED
  (ite (distinct observers #b00) STATUS_OBSERVED STATUS_OK))))))

(define-fun helper_begin_collapse_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (helper_slot_valid Bool)) (_ BitVec 4)
  (ite (not helper_slot_valid) STATUS_NOT_FOUND
  (ite (= state STATE_DECAYED) STATUS_DECAYED
  (ite (= state STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= state STATE_OBSERVED) STATUS_OBSERVED
  (ite (distinct observers #b00) STATUS_OBSERVED STATUS_OK))))))

(define-fun generic_end_collapse_status ((state (_ BitVec 2)) (registered Bool)) (_ BitVec 4)
  (ite (not registered) STATUS_NOT_FOUND
  (ite (= state STATE_COLLAPSED) STATUS_OK STATUS_NOT_COLLAPSED)))

(define-fun helper_end_collapse_status ((state (_ BitVec 2)) (helper_slot_valid Bool)) (_ BitVec 4)
  (ite (not helper_slot_valid) STATUS_NOT_FOUND
  (ite (= state STATE_COLLAPSED) STATUS_OK STATUS_NOT_COLLAPSED)))

(define-fun generic_begin_observe_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (registered Bool)) (_ BitVec 4)
  (ite (not registered) STATUS_NOT_FOUND
  (ite (= state STATE_DECAYED) STATUS_DECAYED
  (ite (= state STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= observers #b11) STATUS_OBSERVED STATUS_OK)))))

(define-fun helper_begin_observe_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (helper_slot_valid Bool)) (_ BitVec 4)
  (ite (not helper_slot_valid) STATUS_NOT_FOUND
  (ite (= state STATE_DECAYED) STATUS_DECAYED
  (ite (= state STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (= observers #b11) STATUS_OBSERVED STATUS_OK)))))

(define-fun generic_end_observe_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (registered Bool)) (_ BitVec 4)
  (ite (not registered) STATUS_NOT_FOUND
  (ite (and (= state STATE_OBSERVED) (distinct observers #b00)) STATUS_OK STATUS_NOT_OBSERVED)))

(define-fun helper_end_observe_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (helper_slot_valid Bool)) (_ BitVec 4)
  (ite (not helper_slot_valid) STATUS_NOT_FOUND
  (ite (and (= state STATE_OBSERVED) (distinct observers #b00)) STATUS_OK STATUS_NOT_OBSERVED)))

(define-fun generic_decay_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (registered Bool)) (_ BitVec 4)
  (ite (not registered) STATUS_NOT_FOUND
  (ite (= state STATE_DECAYED) STATUS_DECAYED
  (ite (= state STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (distinct observers #b00) STATUS_OBSERVED STATUS_OK)))))

(define-fun helper_decay_status ((state (_ BitVec 2)) (observers (_ BitVec 2)) (helper_slot_valid Bool)) (_ BitVec 4)
  (ite (not helper_slot_valid) STATUS_NOT_FOUND
  (ite (= state STATE_DECAYED) STATUS_DECAYED
  (ite (= state STATE_COLLAPSED) STATUS_COLLAPSED
  (ite (distinct observers #b00) STATUS_OBSERVED STATUS_OK)))))

; begin_collapse
(define-fun generic_begin_collapse0 () (_ BitVec 4)
  (generic_begin_collapse_status state1 observers1 registered1))
(define-fun helper_begin_collapse0 () (_ BitVec 4)
  (helper_begin_collapse_status state1 observers1 helper_slot_valid1))
(define-fun generic_state2 () (_ BitVec 2)
  (ite (= generic_begin_collapse0 STATUS_OK) STATE_COLLAPSED state1))
(define-fun helper_state2 () (_ BitVec 2)
  (ite (= helper_begin_collapse0 STATUS_OK) STATE_COLLAPSED state1))
(define-fun generic_observers2 () (_ BitVec 2) observers1)
(define-fun helper_observers2 () (_ BitVec 2) observers1)

; end_collapse
(define-fun generic_end_collapse0 () (_ BitVec 4)
  (generic_end_collapse_status generic_state2 registered1))
(define-fun helper_end_collapse0 () (_ BitVec 4)
  (helper_end_collapse_status helper_state2 helper_slot_valid1))
(define-fun generic_state3 () (_ BitVec 2)
  (ite (= generic_end_collapse0 STATUS_OK) STATE_IDLE generic_state2))
(define-fun helper_state3 () (_ BitVec 2)
  (ite (= helper_end_collapse0 STATUS_OK) STATE_IDLE helper_state2))
(define-fun generic_observers3 () (_ BitVec 2) generic_observers2)
(define-fun helper_observers3 () (_ BitVec 2) helper_observers2)

; begin_observe
(define-fun generic_begin_observe0 () (_ BitVec 4)
  (generic_begin_observe_status generic_state3 generic_observers3 registered1))
(define-fun helper_begin_observe0 () (_ BitVec 4)
  (helper_begin_observe_status helper_state3 helper_observers3 helper_slot_valid1))
(define-fun generic_state4 () (_ BitVec 2)
  (ite (= generic_begin_observe0 STATUS_OK) STATE_OBSERVED generic_state3))
(define-fun helper_state4 () (_ BitVec 2)
  (ite (= helper_begin_observe0 STATUS_OK) STATE_OBSERVED helper_state3))
(define-fun generic_observers4 () (_ BitVec 2)
  (ite (= generic_begin_observe0 STATUS_OK) #b01 generic_observers3))
(define-fun helper_observers4 () (_ BitVec 2)
  (ite (= helper_begin_observe0 STATUS_OK) #b01 helper_observers3))

; end_observe
(define-fun generic_end_observe0 () (_ BitVec 4)
  (generic_end_observe_status generic_state4 generic_observers4 registered1))
(define-fun helper_end_observe0 () (_ BitVec 4)
  (helper_end_observe_status helper_state4 helper_observers4 helper_slot_valid1))
(define-fun generic_state5 () (_ BitVec 2)
  (ite (= generic_end_observe0 STATUS_OK) STATE_IDLE generic_state4))
(define-fun helper_state5 () (_ BitVec 2)
  (ite (= helper_end_observe0 STATUS_OK) STATE_IDLE helper_state4))
(define-fun generic_observers5 () (_ BitVec 2)
  (ite (= generic_end_observe0 STATUS_OK) #b00 generic_observers4))
(define-fun helper_observers5 () (_ BitVec 2)
  (ite (= helper_end_observe0 STATUS_OK) #b00 helper_observers4))

; decay
(define-fun generic_decay0 () (_ BitVec 4)
  (generic_decay_status generic_state5 generic_observers5 registered1))
(define-fun helper_decay0 () (_ BitVec 4)
  (helper_decay_status helper_state5 helper_observers5 helper_slot_valid1))
(define-fun generic_state6 () (_ BitVec 2)
  (ite (= generic_decay0 STATUS_OK) STATE_DECAYED generic_state5))
(define-fun helper_state6 () (_ BitVec 2)
  (ite (= helper_decay0 STATUS_OK) STATE_DECAYED helper_state5))

(assert
  (or
    (not (= ensure_imported_status STATUS_OK))
    (not (= generic_begin_collapse0 helper_begin_collapse0))
    (not (= generic_end_collapse0 helper_end_collapse0))
    (not (= generic_begin_observe0 helper_begin_observe0))
    (not (= generic_end_observe0 helper_end_observe0))
    (not (= generic_decay0 helper_decay0))
    (not (= generic_state2 helper_state2))
    (not (= generic_state3 helper_state3))
    (not (= generic_state4 helper_state4))
    (not (= generic_state5 helper_state5))
    (not (= generic_state6 helper_state6))
    (not (= generic_observers4 helper_observers4))
    (not (= generic_observers5 helper_observers5))))

(check-sat)
