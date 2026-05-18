(set-logic QF_UFLIA)

; If a cached string length is used only while the original parameter binding
; is still current, and the lowering recomputes length after reassignment,
; the hoisted cache agrees with a fresh runtime len() call.

(declare-sort Ptr 0)
(declare-fun len (Ptr) Int)

(declare-const entry_ptr Ptr)
(declare-const reassigned Bool)
(declare-const reassigned_ptr Ptr)

(define-fun current_ptr () Ptr
  (ite reassigned reassigned_ptr entry_ptr))

(define-fun cached_len () Int
  (len entry_ptr))

(define-fun emitted_len () Int
  (ite reassigned
    (len current_ptr)
    cached_len))

(assert (not (= emitted_len (len current_ptr))))

(check-sat)
