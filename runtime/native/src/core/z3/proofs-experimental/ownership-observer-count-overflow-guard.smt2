; Proof: Observer count overflow guard is correct
;
; In begin_observe_slot_unlocked:
;   if (region->observers == UINT32_MAX) return KAIN_OWNERSHIP_ERR_OVERFLOW;
;   region->observers += 1;
;
; Since observers is uint32_t, the guard correctly prevents wrapping to 0
; when observers == UINT32_MAX. This proves that:
;   1. When observers == UINT32_MAX, observers + 1 would overflow (wrap to 0)
;   2. When observers < UINT32_MAX, observers + 1 doesn't overflow
;   3. The guard catches the only dangerous case

(set-logic QF_BV)

(declare-const observers (_ BitVec 32))

; ============================================================
; Claim 1: observers == UINT32_MAX => observers + 1 wraps to 0
; ============================================================

(assert (= observers (bvnot (_ bv0 32))))  ; UINT32_MAX

; Prove: observers + 1 == 0 (the overflow case)
(define-fun observers_plus_one () (_ BitVec 32)
  (bvadd observers (_ bv1 32)))

(assert (not (= observers_plus_one (_ bv0 32))))
(check-sat)

(reset)

; ============================================================
; Claim 2: observers < UINT32_MAX => observers + 1 doesn't overflow
; (i.e., observers + 1 > observers)
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))

(assert (bvult observers (bvnot (_ bv0 32))))  ; observers < UINT32_MAX

(define-fun observers_plus_one () (_ BitVec 32)
  (bvadd observers (_ bv1 32)))

; When no unsigned wrap occurs: observers + 1 > observers
(assert (not (bvugt observers_plus_one observers)))
(check-sat)

(reset)

; ============================================================
; Claim 3: If guard passes (observers != UINT32_MAX), the increment is safe
; Simulate the full guard logic:
;   if (observers == UINT32_MAX) return ERR_OVERFLOW;
;   observers += 1;
;   state = OBSERVED;
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))

; Guard: observers != UINT32_MAX
(assert (not (= observers (bvnot (_ bv0 32)))))

(define-fun new_observers () (_ BitVec 32)
  (bvadd observers (_ bv1 32)))

; After increment: observers > 0 (since we started >= 0 and added 1)
; Actually we need: observers >= 0, so new_observers >= 1
(assert (bvult new_observers (_ bv1 32)))
(check-sat)

(reset)

; ============================================================
; Claim 4: observers < UINT32_MAX is the ONLY condition for safe increment
; (if observers == UINT32_MAX, increment wraps to 0 which is unsafe)
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))

; If observers + 1 overflowed (result <= observers), then observers must be UINT32_MAX
(define-fun observers_plus_one () (_ BitVec 32)
  (bvadd observers (_ bv1 32)))

; Assume overflow occurs (result <= observers, meaning wrap)
(assert (bvule observers_plus_one observers))

; Then observers must be UINT32_MAX
(assert (not (= observers (bvnot (_ bv0 32)))))
(check-sat)
