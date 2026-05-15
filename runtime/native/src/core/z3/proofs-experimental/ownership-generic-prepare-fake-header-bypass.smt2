(set-logic ALL)

; Old generic prepare contract:
; - helper-looking header returns success immediately
; - imported registration only happens when the helper-header check fails
; This witness proves there exists a state where prepare succeeds but the later
; ownership op still cannot resolve the pointer.

(declare-const header_valid Bool)
(declare-const already_registered Bool)
(declare-const upsert_succeeds Bool)
(declare-const helper_slot_matches Bool)

(define-fun old_prepare_attempts_register () Bool
  (and (not header_valid) (not already_registered)))

(define-fun imported_registered_after_prepare () Bool
  (or already_registered
      (and old_prepare_attempts_register upsert_succeeds)))

(define-fun old_prepare_ok () Bool
  (or header_valid imported_registered_after_prepare))

(define-fun old_begin_observe_ok () Bool
  (or helper_slot_matches imported_registered_after_prepare))

(assert header_valid)
(assert (not already_registered))
(assert (not upsert_succeeds))
(assert (not helper_slot_matches))
(assert old_prepare_ok)
(assert (not old_begin_observe_ok))

(check-sat)
(get-model)
