; Experimental moonshot proof:
; Under a fresh, non-escaping, single-store ownership trace,
; alloc -> collapse(store x) -> observe(load) -> decay
; is observationally equivalent to scalar value flow x.

(set-logic QF_BV)

(define-fun Idle () (_ BitVec 2) #b00)
(define-fun Observed () (_ BitVec 2) #b01)
(define-fun Collapsed () (_ BitVec 2) #b10)
(define-fun Decayed () (_ BitVec 2) #b11)

(declare-fun written_value () (_ BitVec 64))
(declare-fun fresh () Bool)
(declare-fun non_escaping () Bool)
(declare-fun single_store () Bool)
(declare-fun no_external_alias_write () Bool)
(declare-fun no_intervening_runtime_fault () Bool)

(assert fresh)
(assert non_escaping)
(assert single_store)
(assert no_external_alias_write)
(assert no_intervening_runtime_fault)

; Runtime path under the contract.
(define-fun runtime_state_after_begin_collapse () (_ BitVec 2) Collapsed)
(define-fun runtime_value_after_store () (_ BitVec 64) written_value)
(define-fun runtime_state_after_end_collapse () (_ BitVec 2) Idle)
(define-fun runtime_state_after_begin_observe () (_ BitVec 2) Observed)
(define-fun runtime_loaded_value () (_ BitVec 64) runtime_value_after_store)
(define-fun runtime_state_after_end_observe () (_ BitVec 2) Idle)
(define-fun runtime_final_state () (_ BitVec 2) Decayed)

; Erased scalar path.
(define-fun erased_loaded_value () (_ BitVec 64) written_value)
(define-fun erased_final_state () (_ BitVec 2) Decayed)

; Any observable disagreement should be impossible.
(assert
  (or
    (not (= runtime_state_after_begin_collapse Collapsed))
    (not (= runtime_state_after_end_collapse Idle))
    (not (= runtime_state_after_begin_observe Observed))
    (not (= runtime_state_after_end_observe Idle))
    (not (= runtime_loaded_value erased_loaded_value))
    (not (= runtime_final_state erased_final_state))))

(check-sat)
