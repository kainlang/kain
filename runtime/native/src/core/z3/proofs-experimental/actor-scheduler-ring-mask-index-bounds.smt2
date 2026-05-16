; Experimental proof for the actor scheduler ring queue in
; actor.c.
; Claim: masking arbitrary enqueue/dequeue cursors with 1023 always produces an
; in-bounds ring slot for the 1024-entry scheduler queue.
(set-logic QF_BV)

(declare-fun enqueue_cursor () (_ BitVec 64))
(declare-fun dequeue_cursor () (_ BitVec 64))

(define-fun enqueue_slot () (_ BitVec 64)
  (bvand enqueue_cursor #x00000000000003ff))
(define-fun dequeue_slot () (_ BitVec 64)
  (bvand dequeue_cursor #x00000000000003ff))

(assert
  (or
    (bvugt enqueue_slot #x00000000000003ff)
    (bvugt dequeue_slot #x00000000000003ff)))
(check-sat)
