(set-logic QF_BV)

; The native reply-port rebind path reuses an actor-table slot but advances the
; 32-bit generation with the same rule as kain_actor_table_insert:
;   next = old + 1;
;   if next == 0 then next = 1;
; This proof shows a rebound slot can never mint the exact same generation as
; the stale ref it is replacing, including the wrap-from-0xFFFFFFFF-to-1 case.

(define-fun next_generation ((old_generation (_ BitVec 32))) (_ BitVec 32)
  (let ((incremented (bvadd old_generation #x00000001)))
    (ite (= incremented #x00000000) #x00000001 incremented)))

(declare-const old_generation (_ BitVec 32))

; If the solver can satisfy this assertion, the runtime could accidentally reuse
; a stale generation for a rebound slot. We require UNSAT.
(assert (= (next_generation old_generation) old_generation))

(check-sat)
