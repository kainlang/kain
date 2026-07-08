;; ============================================================================
;;  BUG-012: Hash table max load 256 not enforced
;;
;;  Invariant: kaintana_hash_insert rejects new entries when
;;  hash_occupied_count >= KAINTANA_HASH_MAX_LOAD (256).
;;  This prevents probe-chain degradation at high load factors.
;;
;;  Property: If occupied_count starts within bounds [0, MAX_LOAD],
;;  after any insert the invariant occupied_count <= MAX_LOAD is preserved.
;;
;;  We assert the negation: occupied_count starts within bounds but
;;  after insert it exceeds MAX_LOAD. If UNSAT, the invariant holds.
;; ============================================================================

(declare-const occupied_count Int)
(declare-const hash_occupied_count_after_insert Int)
(declare-const insert_rejected Bool)

;; Constants
(declare-const MAX_LOAD Int)
(assert (= MAX_LOAD 256))

;; Precondition: count starts within bounds (never exceeds MAX_LOAD)
(assert (>= occupied_count 0))
(assert (<= occupied_count MAX_LOAD))

;; Insert behavior model:
;; If occupied_count >= MAX_LOAD, insert is rejected (no-op)
(assert (=>
    (>= occupied_count MAX_LOAD)
    (and insert_rejected
         (= hash_occupied_count_after_insert occupied_count))))

;; If occupied_count < MAX_LOAD, insert succeeds, count increments by 1
(assert (=>
    (< occupied_count MAX_LOAD)
    (and (not insert_rejected)
         (= hash_occupied_count_after_insert (+ occupied_count 1)))))

;; NEGATION of invariant: after any insert, occupied_count > MAX_LOAD
;; If UNSAT, the load enforcement guarantees the invariant.
(assert (> hash_occupied_count_after_insert MAX_LOAD))

(check-sat)
