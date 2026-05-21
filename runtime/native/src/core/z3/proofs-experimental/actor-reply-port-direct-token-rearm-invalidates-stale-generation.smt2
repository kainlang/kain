; Compiler-lowered direct asks no longer need a live synthetic actor-table slot
; to reject stale replies. The direct handle lane rearms the TLS reply-port
; state by keeping actor_id invalid and bumping only the generation token that
; send_handle(...) must match. This proof checks that a stale direct token can
; never equal the freshly rearmed token.

(set-logic QF_BV)

(define-fun next_generation ((previous (_ BitVec 32))) (_ BitVec 32)
  (let ((incremented (bvadd previous #x00000001)))
    (ite (= incremented #x00000000) #x00000001 incremented)))

(declare-fun previous_generation () (_ BitVec 32))

; Direct reply-port generations are never left at zero after preparation.
(assert (not (= previous_generation #x00000000)))

(define-fun stale_actor_id () (_ BitVec 64) #x0000000000000000)
(define-fun live_actor_id () (_ BitVec 64) #x0000000000000000)
(define-fun stale_execution_class () (_ BitVec 32) #x00000006)
(define-fun live_execution_class () (_ BitVec 32) #x00000006)
(define-fun stale_locality_class () (_ BitVec 32) #x00000001)
(define-fun live_locality_class () (_ BitVec 32) #x00000001)
(define-fun stale_generation () (_ BitVec 32) previous_generation)
(define-fun live_generation () (_ BitVec 32) (next_generation previous_generation))

; Ask the solver for a stale token that still matches the rearmed live token.
(assert (= stale_actor_id live_actor_id))
(assert (= stale_execution_class live_execution_class))
(assert (= stale_locality_class live_locality_class))
(assert (= stale_generation live_generation))

(check-sat)
