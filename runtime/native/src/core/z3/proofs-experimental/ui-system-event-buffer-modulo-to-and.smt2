; Z3 Proof: Event ring buffer modulo → bitwise AND
; Target: X:/runtime/native/src/ui/ui_system.c lines 1498, 1514
;
; Original: event_tail = (event_tail + 1) % ABI_UI_MAX_EVENTS
;           event_head = (event_head + 1) % ABI_UI_MAX_EVENTS
;
; ABI_UI_MAX_EVENTS = 1024 = 2^10
; event_head and event_tail are always in [0, 1023] by invariant
;
; Replacement: (x + 1) & (ABI_UI_MAX_EVENTS - 1)
;            = (x + 1) & 1023
;
; Claim: For all x in [0, 1023], (x+1) % 1024 == (x+1) & 1023
;
; Result: unsat (equivalent)

(set-logic QF_BV)
(declare-const x (_ BitVec 64))

; x is in [0, 1023]
(assert (bvule x #x00000000000003ff))

(define-fun orig ((v (_ BitVec 64))) (_ BitVec 64)
  (bvurem (bvadd v #x0000000000000001) #x0000000000000400))

(define-fun candidate ((v (_ BitVec 64))) (_ BitVec 64)
  (bvand (bvadd v #x0000000000000001) #x00000000000003ff))

(assert (not (= (orig x) (candidate x))))
(check-sat)
; Expected: unsat
