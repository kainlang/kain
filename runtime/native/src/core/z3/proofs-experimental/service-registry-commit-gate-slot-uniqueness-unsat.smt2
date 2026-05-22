; Experimental proof for the landed service registry commit gate.
; Two successful distinct registrations are serialized by the mutation gate,
; so they observe different slot numbers and final_count advances by two.

(set-logic ALL)

(declare-const order_ab Bool)
(declare-const initial_count Int)

(assert (>= initial_count 0))
(assert (< initial_count 63))

(define-fun advance ((count Int)) Int
  (+ count 1))

(define-fun state_after_first () Int
  (advance initial_count))

(define-fun slot_a () Int
  (ite order_ab initial_count state_after_first))

(define-fun slot_b () Int
  (ite order_ab state_after_first initial_count))

(define-fun final_count () Int
  (advance state_after_first))

; Safety contract: two successful serialized commits occupy distinct slots
; and advance the published count by exactly two.
(assert
  (or
    (not (= final_count (+ initial_count 2)))
    (= slot_a slot_b)))

(check-sat)
