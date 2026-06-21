; Claim: restart_count >= KAIN_SUPERVISION_MAX_RESTARTS (5) limit check
; is NOT monotonic under unsigned wraparound.
;
; When restart_count = UINT64_MAX (0xFFFFFFFFFFFFFFFF), the check
; restart_count >= 5 is TRUE, but incrementing wraps to 0 which is < 5,
; so the limit can be "un-hit" after wraparound.
;
; In practice, this is safe because:
; 1. restart_count is reset every KAIN_SUPERVISION_RESTART_WINDOW_SECONDS (60s)
; 2. With max 5 restarts per window, it would take 3.7e18 restarts to wrap
; 3. restart_attempt_count (lifetime counter) can wrap but is diagnostic-only
;
; However, this is a REAL correctness edge case if the window mechanism fails
; or if the system runs for billions of years.
;
; Mitigation: add saturating increment or leave as-is (academic edge case).
;
; Solver result: sat with counterexample 0xFFFFFFFFFFFFFFFF
(set-logic QF_BV)
(declare-const restart_count (_ BitVec 64))

; The increment: restart_count = restart_count + 1
(define-fun incremented ((rc (_ BitVec 64))) (_ BitVec 64)
  (bvadd rc (_ bv1 64)))

; The check: rc >= 5 triggers limit
(define-fun limit_hit ((rc (_ BitVec 64))) Bool
  (bvuge rc (_ bv5 64)))

; Find: limit_hit(old) but !limit_hit(new) — wraparound breaks monotonicity
(assert (and (limit_hit restart_count)
             (not (limit_hit (incremented restart_count)))))
(check-sat)
(get-model)
; sat: restart_count = 0xFFFFFFFFFFFFFFFF
;   limit_hit(0xFF..FF) = true  (since >= 5)
;   incremented(0xFF..FF) = 0  (wraparound)
;   limit_hit(0) = false
;   → invariant broken on wraparound ⚠️
