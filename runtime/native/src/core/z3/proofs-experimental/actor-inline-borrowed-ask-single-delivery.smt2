(set-logic QF_LIA)

; Borrowed inline asks are only armed when the queue is empty and the slot is clear.
(declare-const queue_before Int)
(declare-const pending_before Int)

; Arming the borrowed slot must not mutate queue depth.
(declare-const queue_after_arm Int)
(declare-const pending_after_arm Int)

; Consuming the borrowed slot must deliver exactly one message and clear the slot.
(declare-const queue_after_consume Int)
(declare-const pending_after_consume Int)
(declare-const delivered_count Int)

(assert (= queue_before 0))
(assert (= pending_before 0))

(assert (= queue_after_arm queue_before))
(assert (= pending_after_arm 1))

(assert (= queue_after_consume queue_after_arm))
(assert (= pending_after_consume 0))
(assert (= delivered_count 1))

; No valid execution may both follow the arm/consume contract and violate
; the single-delivery + zero-queue-mutation postcondition.
(assert
  (or
    (not (= queue_after_consume 0))
    (not (= pending_after_consume 0))
    (not (= delivered_count 1))))

(check-sat)
