(set-logic QF_LIA)

(declare-fun rc0 () Int)

; Returning an owned JSON local now retains once before the function epilogue
; releases local ownership. Starting from any live RC state (rc0 >= 1), the
; caller still receives a live handle after cleanup.
(assert (>= rc0 1))

(define-fun rc_after_transfer () Int
  (- (+ rc0 1) 1))

(assert (< rc_after_transfer 1))

(check-sat)
