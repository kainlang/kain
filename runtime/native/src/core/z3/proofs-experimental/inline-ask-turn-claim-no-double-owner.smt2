; Proves the scheduler dequeue ordering used by actor.c cannot expose a
; transient "queue = 0, turn = 0" state while handing an actor from the ready
; queue into an executing turn. The inline-ask fast path only steals a turn
; when queue = 0 and turn = 0, so this ordering rules out double ownership
; between a worker dequeue and a same-thread inline claim.

(set-logic QF_UF)

; Dequeue starts from an actor that is known to be ready but not yet executing.
(declare-const queue_queued Bool)
(declare-const turn_queued Bool)
(assert queue_queued)
(assert (not turn_queued))

; Scheduler worker claims the turn before clearing the ready-queue bit.
(define-fun queue_claimed () Bool queue_queued)
(define-fun turn_claimed () Bool true)
(define-fun queue_dequeued () Bool false)
(define-fun turn_dequeued () Bool turn_claimed)

; Inline ask may only steal the actor when it observes queue = 0 and turn = 0.
(define-fun inline_claim_eligible ((queue Bool) (turn Bool)) Bool
  (and (not queue) (not turn)))

; If the dequeue ordering is correct, neither intermediate scheduler-owned state
; can satisfy the inline-claim predicate.
(assert
  (or
    (inline_claim_eligible queue_claimed turn_claimed)
    (inline_claim_eligible queue_dequeued turn_dequeued)))

(check-sat)
