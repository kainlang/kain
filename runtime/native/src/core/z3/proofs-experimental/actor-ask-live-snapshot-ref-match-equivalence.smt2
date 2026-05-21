(set-logic QF_BV)

; kain_actor_ask_send_ref(...) used to validate a target ref through
; kain_actor_table_ref_matches_locked(...). The new hot path instead checks the
; already-loaded actor snapshot against the same generation-tagged ref fields
; before it touches the mailbox lock.
;
; Under the stable-snapshot invariant that:
; - the loaded actor is the live table occupant for the ref's slot, and
; - the slot-local actor_id matches the slot index/ref actor_id,
; the old locked accept predicate and the new snapshot predicate cannot
; disagree.

(declare-const ref_actor_id (_ BitVec 64))
(declare-const ref_generation (_ BitVec 32))
(declare-const ref_execution (_ BitVec 32))
(declare-const ref_locality (_ BitVec 32))

(declare-const table_generation (_ BitVec 32))
(declare-const actor_slot_id (_ BitVec 64))
(declare-const actor_generation (_ BitVec 32))
(declare-const actor_execution (_ BitVec 32))
(declare-const actor_locality (_ BitVec 32))

; Stable live-slot invariant.
(assert (= actor_slot_id ref_actor_id))

(define-fun locked_ref_accepts () Bool
  (and (= table_generation ref_generation)
       (= actor_generation ref_generation)
       (= actor_execution ref_execution)
       (= actor_locality ref_locality)))

(define-fun live_snapshot_accepts () Bool
  (and (= actor_slot_id ref_actor_id)
       (= table_generation ref_generation)
       (= actor_generation ref_generation)
       (= actor_execution ref_execution)
       (= actor_locality ref_locality)))

(assert (xor locked_ref_accepts live_snapshot_accepts))

(check-sat)
