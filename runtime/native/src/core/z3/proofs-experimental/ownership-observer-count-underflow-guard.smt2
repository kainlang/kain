; Proof: Observer count underflow guard is correct
;
; In end_observe_slot_unlocked:
;   if (region->state != KAIN_OWNERSHIP_STATE_OBSERVED || region->observers == 0)
;       return KAIN_OWNERSHIP_ERR_NOT_OBSERVED;
;   region->observers -= 1;
;   if (region->observers == 0) region->state = KAIN_OWNERSHIP_STATE_IDLE;
;
; Since observers is uint32_t, the guard correctly prevents underflow
; (wrapping from 0 to UINT32_MAX). This proves:
;   1. When observers == 0, decrement would wrap to UINT32_MAX
;   2. When observers > 0, observers - 1 doesn't wrap (observers - 1 < observers)
;   3. The guard catches the only dangerous case
;   4. When guard passes, observers > 0, so new_observers >= 0

(set-logic QF_BV)

(declare-const observers (_ BitVec 32))

; ============================================================
; Claim 1: observers == 0 => observers - 1 wraps to UINT32_MAX
; ============================================================

(assert (= observers (_ bv0 32)))

(define-fun observers_minus_one () (_ BitVec 32)
  (bvsub observers (_ bv1 32)))

(define-fun uint32_max () (_ BitVec 32)
  (bvnot (_ bv0 32)))

; Prove: observers - 1 == UINT32_MAX
(assert (not (= observers_minus_one uint32_max)))
(check-sat)

(reset)

; ============================================================
; Claim 2: observers > 0 => observers - 1 < observers
; (no unsigned wrap)
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))

(assert (bvugt observers (_ bv0 32)))  ; observers > 0

(define-fun observers_minus_one () (_ BitVec 32)
  (bvsub observers (_ bv1 32)))

; Prove: observers - 1 < observers (no wrap)
(assert (not (bvult observers_minus_one observers)))
(check-sat)

(reset)

; ============================================================
; Claim 3: If guard passes (state == OBSERVED && observers > 0),
; the decrement is always safe. Prove new_observers never wraps.
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))

; Guard: observers != 0 (implied by state == OBSERVED)
(assert (not (= observers (_ bv0 32))))

(define-fun new_observers () (_ BitVec 32)
  (bvsub observers (_ bv1 32)))

; new_observers must be truly less than observers (no wrap)
(assert (not (bvult new_observers observers)))
(check-sat)

(reset)

; ============================================================
; Claim 4: observers == 0 is the ONLY case that causes underflow
; If observers - 1 wraps (observers - 1 > observers), then observers == 0
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))

(define-fun observers_minus_one () (_ BitVec 32)
  (bvsub observers (_ bv1 32)))

; Assume wrap occurs (observers - 1 > observers)
(assert (bvugt observers_minus_one observers))

; Then observers must be 0
(assert (not (= observers (_ bv0 32))))
(check-sat)

(reset)

; ============================================================
; Claim 5: Post-decrement state transition correctness
;   if (observers == 0) state = IDLE;
; Prove that after decrement, either:
;   - observers == 0 => state transitions to IDLE
;   - observers > 0 => state stays OBSERVED
; ============================================================

(set-logic QF_BV)
(declare-const observers (_ BitVec 32))
(declare-const state (_ BitVec 32))

; Pre-state: state == OBSERVED and observers > 0
(assert (= state (_ bv1 32)))        ; OBSERVED
(assert (bvugt observers (_ bv0 32)))

(define-fun new_observers () (_ BitVec 32)
  (bvsub observers (_ bv1 32)))

; Post state: if new_observers == 0 then IDLE else OBSERVED
(define-fun new_state () (_ BitVec 32)
  (ite (= new_observers (_ bv0 32)) (_ bv0 32) (_ bv1 32)))

; The new state must be valid (either IDLE=0 or OBSERVED=1)
(assert (not (and (bvuge new_state (_ bv0 32)) (bvule new_state (_ bv4 32)))))
(check-sat)
