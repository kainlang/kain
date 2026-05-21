(set-logic QF_BV)

; The direct-handle reply fast path no longer round-trips through
; kain_actor_table_ref_matches_locked(...), but it still checks the same
; generation-tagged ref fields against the reply-port state's current live ref.
; Under the bound-handle invariant that the live synthetic actor table entry and
; the reply-port state carry the same actor_id/generation/execution/locality,
; the old table-based accept predicate and the new state-based accept predicate
; cannot disagree.

(declare-const table_actor_id (_ BitVec 64))
(declare-const table_generation (_ BitVec 32))
(declare-const table_execution (_ BitVec 32))
(declare-const table_locality (_ BitVec 32))

(declare-const state_actor_id (_ BitVec 64))
(declare-const state_generation (_ BitVec 32))
(declare-const state_execution (_ BitVec 32))
(declare-const state_locality (_ BitVec 32))

(declare-const expected_actor_id (_ BitVec 64))
(declare-const expected_generation (_ BitVec 32))
(declare-const expected_execution (_ BitVec 32))
(declare-const expected_locality (_ BitVec 32))

(assert (= state_actor_id table_actor_id))
(assert (= state_generation table_generation))
(assert (= state_execution table_execution))
(assert (= state_locality table_locality))

(define-fun send_ref_accepts () Bool
  (and (= expected_actor_id table_actor_id)
       (= expected_generation table_generation)
       (= expected_execution table_execution)
       (= expected_locality table_locality)))

(define-fun send_handle_accepts () Bool
  (and (= expected_actor_id state_actor_id)
       (= expected_generation state_generation)
       (= expected_execution state_execution)
       (= expected_locality state_locality)))

(assert (xor send_ref_accepts send_handle_accepts))

(check-sat)
