; The owner-thread inline reply fast path keeps the direct-token reply-port
; identity check and only changes how the owning thread reads back the completed
; payload. A stale direct token from the previous ask must still fail the
; ref-match guard after the TLS reply-port state rearms the next generation.

(set-logic QF_BV)

(define-fun next_generation ((previous (_ BitVec 32))) (_ BitVec 32)
  (let ((incremented (bvadd previous #x00000001)))
    (ite (= incremented #x00000000) #x00000001 incremented)))

(declare-fun previous_generation () (_ BitVec 32))

; Prepared direct tokens are never left at generation zero.
(assert (not (= previous_generation #x00000000)))

(define-fun stale_actor_id () (_ BitVec 64) #x0000000000000000)
(define-fun live_actor_id () (_ BitVec 64) #x0000000000000000)
(define-fun stale_execution_class () (_ BitVec 32) #x00000006)
(define-fun live_execution_class () (_ BitVec 32) #x00000006)
(define-fun stale_locality_class () (_ BitVec 32) #x00000001)
(define-fun live_locality_class () (_ BitVec 32) #x00000001)
(define-fun stale_generation () (_ BitVec 32) previous_generation)
(define-fun live_generation () (_ BitVec 32) (next_generation previous_generation))

(define-fun same_identity () Bool
  (and (= stale_actor_id live_actor_id)
       (= stale_execution_class live_execution_class)
       (= stale_locality_class live_locality_class)
       (= stale_generation live_generation)))

; Ask the solver for a stale previous token that would still match the rearmed
; owner-inline direct token.
(assert same_identity)

(check-sat)
