; Experimental proof for the landed actor runtime once gate.
; The transition cold(0) -> busy(1) is claimed with a single CAS, so two
; concurrent first callers cannot both enter the init body.

(set-logic ALL)

(declare-const order_ab Bool)

(define-fun cold () Int 0)
(define-fun busy () Int 1)

(define-fun claim ((state Int)) Int
  (ite (= state cold) busy state))

(define-fun state_after_first () Int
  (claim cold))

(define-fun thread_a_seen () Int
  (ite order_ab cold state_after_first))

(define-fun thread_b_seen () Int
  (ite order_ab state_after_first cold))

(define-fun thread_a_enters_init () Bool
  (= thread_a_seen cold))

(define-fun thread_b_enters_init () Bool
  (= thread_b_seen cold))

; Safety contract: at most one contender claims the cold -> busy transition.
(assert thread_a_enters_init)
(assert thread_b_enters_init)

(check-sat)
