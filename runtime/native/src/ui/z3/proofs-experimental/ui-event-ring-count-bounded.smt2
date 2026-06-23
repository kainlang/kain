; Proof: Event ring buffer count never exceeds ABI_UI_MAX_EVENTS (1024)
;
; The function abi_ui_push_event (line ~1502) guards the ring buffer push with
;   if (session->event_count >= ABI_UI_MAX_EVENTS) { return ABI_UI_CAPACITY_EXCEEDED; }
; This proves the guard is correct: when the guard passes (count < 1024),
; incrementing count cannot exceed 1024.
;
; Similarly, abi_ui_poll_event decrements event_count only when > 0,
; proving count never underflows.
;
; Key claims:
;   1. Precondition: count < 1024, postcondition: count' <= 1024 after increment
;   2. Precondition: count > 0, postcondition: count' >= 0 after decrement
;   3. event_head AND-mask always produces indices in [0, 1023]
;   4. event_tail AND-mask always produces indices in [0, 1023]

(set-logic QF_BV)

; ============================================================
; Claim 1: event_count bounded by MAX_EVENTS after guarded push
; ============================================================
(reset)
(set-logic QF_BV)

(define-const MAX_EVENTS (_ BitVec 64) #x0000000000000400)  ; 1024

(declare-fun count () (_ BitVec 64))

; Precondition: guard check passed, so count < MAX_EVENTS
(assert (bvult count MAX_EVENTS))

; Simulate increment
(define-const count_after_push (_ BitVec 64) (bvadd count #x0000000000000001))

; Prove: count_after_push <= MAX_EVENTS
(assert (bvugt count_after_push MAX_EVENTS))
(check-sat)
; Expected: unsat -- when count < 1024, count+1 <= 1024

; ============================================================
; Claim 2: event_count never underflows (decrement guarded by > 0)
; ============================================================
(reset)
(set-logic QF_BV)

(declare-fun count () (_ BitVec 64))

; Precondition: guard check passed, so count > 0
(assert (bvugt count #x0000000000000000))

; Simulate decrement
(define-const count_after_poll (_ BitVec 64) (bvsub count #x0000000000000001))

; Prove: count_after_poll does NOT wrap (unsigned underflow)
; If count > 0, then count-1 < count (no wrap)
(assert (bvuge count_after_poll count))
(check-sat)
; Expected: unsat -- count >= 0 after decrement

; ============================================================
; Claim 3: event_head AND-mask always produces index in [0, MAX_EVENTS-1]
; ============================================================
(reset)
(set-logic QF_BV)

(define-const MASK (_ BitVec 64) #x00000000000003FF)  ; 1024 - 1 = 1023
(define-const MAX_EVENTS (_ BitVec 64) #x0000000000000400)  ; 1024

(declare-fun head () (_ BitVec 64))

; Compute head after increment with AND-mask wrapping
(define-const head_after_inc (_ BitVec 64)
  (bvand (bvadd head #x0000000000000001) MASK))

; Prove: head_after_inc < MAX_EVENTS
(assert (bvuge head_after_inc MAX_EVENTS))
(check-sat)
; Expected: unsat -- AND with (MAX-1) always produces value < MAX

; ============================================================
; Claim 4: event_tail AND-mask always produces index in [0, MAX_EVENTS-1]
; ============================================================
(reset)
(set-logic QF_BV)

(define-const MASK (_ BitVec 64) #x00000000000003FF)  ; 1023
(define-const MAX_EVENTS (_ BitVec 64) #x0000000000000400)  ; 1024

(declare-fun tail () (_ BitVec 64))

(define-const tail_after_push (_ BitVec 64)
  (bvand (bvadd tail #x0000000000000001) MASK))

(assert (bvuge tail_after_push MAX_EVENTS))
(check-sat)
; Expected: unsat
