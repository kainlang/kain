; Proves a successful HTTP batch pump cannot exceed the fixed request table.
; The C implementation increments the batch count only after
; abi_net_alloc_request() succeeds; therefore successful_pumped is bounded by
; the free request slots at entry.
(set-logic QF_LIA)

(declare-const live_requests Int)
(declare-const max_requests Int)
(declare-const successful_pumped Int)

(define-const capacity Int 64)

(assert (<= 0 live_requests))
(assert (<= live_requests capacity))
(assert (<= 1 max_requests))
(assert (<= max_requests capacity))
(assert (<= 0 successful_pumped))
(assert (<= successful_pumped max_requests))
(assert (<= successful_pumped (- capacity live_requests)))

; Negate the safety claim: after all successful accepts, occupancy is still in
; [0, capacity].
(assert
  (not
    (and
      (<= 0 (+ live_requests successful_pumped))
      (<= (+ live_requests successful_pumped) capacity))))

(check-sat)
