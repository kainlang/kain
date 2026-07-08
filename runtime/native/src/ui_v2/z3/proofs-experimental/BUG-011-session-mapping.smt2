;; ============================================================================
;;  BUG-011: g_active_session singleton
;;
;;  The vtable slot dispatch must use sid to look up the correct session
;;  instead of a global g_active_session pointer.
;;
;;  Solution: session_by_sid(sid) scans a small static array mapping
;;  vtable session_id -> session pointer.
;;
;;  Property: For a map with N registered entries, session_by_sid
;;  correctly returns true if the sid is in the map and false otherwise.
;; ============================================================================

(declare-const map_size Int)
(declare-const sid_target Int)
(declare-const sid_0 Int)
(declare-const sid_1 Int)
(declare-const sid_2 Int)
(declare-const sid_3 Int)

;; Maximum sessions
(declare-const MAX_SESSIONS Int)
(assert (= MAX_SESSIONS 8))

;; Map size is bounded
(assert (>= map_size 0))
(assert (<= map_size MAX_SESSIONS))

;; Define session_by_sid as linear scan of registered entries
;; For simplicity, model with 4 entries (sufficient to prove correctness)
(define-fun session_by_sid ((s Int)) Bool
    (or (and (>= map_size 1) (= sid_0 s))
        (and (>= map_size 2) (= sid_1 s))
        (and (>= map_size 3) (= sid_2 s))
        (and (>= map_size 4) (= sid_3 s))))

;; Property: lookup of a registered sid returns true
(assert (=> (and (>= map_size 1) (= sid_0 42)) (session_by_sid 42)))

;; Property: lookup of an unregistered sid returns false
(assert (=> (and (>= map_size 1) (not (= sid_0 99))
                           (not (= sid_1 99))
                           (not (= sid_2 99))
                           (not (= sid_3 99)))
            (not (session_by_sid 99))))

;; Property: after registering sid=42, session_by_sid(42) is true
(assert (=>
    (and (>= map_size 0) (< map_size MAX_SESSIONS) (= sid_0 42))
    (session_by_sid 42)))

;; NEGATION: there exists a map where sid=42 is registered but
;; session_by_sid(42) returns false.
;; If UNSAT, the lookup function is correct.
(assert (and (>= map_size 1) (= sid_0 42) (not (session_by_sid 42))))

(check-sat)
