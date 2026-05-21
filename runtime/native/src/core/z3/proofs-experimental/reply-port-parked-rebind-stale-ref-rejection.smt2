; Proof sketch for the parked synthetic reply-port fast path:
; after destroy/unbind, the next live ref rebinds with a distinct generation,
; so an old generation-tagged reply-port ref cannot match the rebound actor.
(set-logic QF_BV)

(declare-fun old_generation () (_ BitVec 32))
(assert (not (= old_generation #x00000000)))

(define-fun next_generation () (_ BitVec 32)
  (let ((incremented (bvadd old_generation #x00000001)))
    (ite (= incremented #x00000000) #x00000001 incremented)))

; A stale ref would only survive a parked rebind if the new live generation
; could equal the previously issued generation.
(assert (= next_generation old_generation))

(check-sat)
