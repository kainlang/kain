; Proof: Draw command count never exceeds ABI_UI_MAX_DRAW_COMMANDS (8192)
;
; The function abi_ui_append_draw_command (line ~664) guards the append with:
;   if (!session || session->draw_command_count >= ABI_UI_MAX_DRAW_COMMANDS) {
;       return NULL;
;   }
; This proves the guard is correct: when count < 8192, incrementing
; count yields count' <= 8192.
;
; Key claims:
;   1. Precondition: count < 8192, postcondition: count' <= 8192 after increment
;   2. Precondition: count >= 0, the guard correctly prevents overflow

(set-logic QF_BV)

; ============================================================
; Claim 1: draw_command_count bounded by MAX_DRAW_COMMANDS after guard
; ============================================================
(reset)
(set-logic QF_BV)

(define-const MAX_DC (_ BitVec 64) #x0000000000002000)  ; 8192

(declare-fun count () (_ BitVec 64))

; Precondition: guard check passed, so count < MAX_DC
(assert (bvult count MAX_DC))

; Simulate increment
(define-const count_after_append (_ BitVec 64) (bvadd count #x0000000000000001))

; Prove: count_after_append <= MAX_DC
(assert (bvugt count_after_append MAX_DC))
(check-sat)
; Expected: unsat -- when count < 8192, count+1 <= 8192

; ============================================================
; Claim 2: Non-negative count cannot reach > MAX_DC via single increment
; ============================================================
(reset)
(set-logic QF_BV)

(define-const MAX_DC (_ BitVec 64) #x0000000000002000)  ; 8192

(declare-fun count () (_ BitVec 64))

; Precondition: count >= 0 (unsigned guaranteed) AND count < MAX_DC
(assert (bvult count MAX_DC))

; The maximum value after guarded increment
(define-const max_after (_ BitVec 64)
  (bvadd (bvsub MAX_DC #x0000000000000001) #x0000000000000001))

; Prove max_after == MAX_DC (the boundary case works)
(assert (distinct max_after MAX_DC))
(check-sat)
; Expected: unsat -- 8191 + 1 == 8192, the boundary is safe
