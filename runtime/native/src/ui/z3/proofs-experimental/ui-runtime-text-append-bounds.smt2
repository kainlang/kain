; Proof: kain_ui_runtime_append_text — text buffer append never overflows capacity
;
; The function appends `text` to `component->value` (char array of size 320).
; It computes: room = sizeof(value) - 1 - current_len
; If room > 0: append_len is clamped to room, then:
;   memcpy(value + current_len, text, append_len)
;   value[current_len + append_len] = '\0'
;
; Key claims:
;   1. room = 319 - current_len (never underflows for valid current_len ≤ 319)
;   2. After clamping: append_len ≤ room, so current_len + append_len ≤ 319
;   3. Null terminator at [current_len + append_len] is within buffer (≤ 319)
;
(set-logic QF_BV)

; Constants
(define-fun VALUE_SIZE () (_ BitVec 64) #x0000000000000140) ; 320 = KAIN_UI_COMPILED_BUNDLE_MAX_TEXT
(define-fun MAX_IDX () (_ BitVec 64) #x000000000000013F) ; 319 = last writable index

; ============================================================================
; Claim 1: room never underflows for valid current_len
; room = sizeof(value) - 1 - current_len = 319 - current_len
; For any current_len ∈ [0, 319], room = 319 - current_len >= 0
;
; We prove: for current_len <= MAX_IDX, room = VALUE_SIZE - 1 - current_len
; does not wrap around (unsigned underflow).
; ============================================================================
(push)
(declare-fun current_len () (_ BitVec 64))
(assert (bvule current_len MAX_IDX)) ; current_len ∈ [0, 319]
(define-fun room () (_ BitVec 64) (bvsub (bvsub VALUE_SIZE #x0000000000000001) current_len))
; Prove: room does not wrap = room <= VALUE_SIZE
(assert (bvugt room VALUE_SIZE)) ; If room > 320, that means underflow
(check-sat)
; Expected: unsat — no underflow for any valid current_len
(pop)

; ============================================================================
; Claim 2: After clamping, append_len ≤ room
; If room > 0 and (original) append_len > room, append_len is clamped to room.
; So append_len ≤ room always.
; We prove: 
;   append_len_final = (append_len_orig > room) ? room : append_len_orig
;   → append_len_final ≤ room
; ============================================================================
(push)
(declare-fun current_len () (_ BitVec 64))
(assert (bvule current_len MAX_IDX)) ; valid current_len
(define-fun room2 () (_ BitVec 64) (bvsub (bvsub VALUE_SIZE #x0000000000000001) current_len))
(declare-fun append_len_orig () (_ BitVec 64))
; room > 0 (function returns early if room == 0)
(assert (bvugt room2 #x0000000000000000))
; append_len_final = min(append_len_orig, room2)
(define-fun append_len_final () (_ BitVec 64)
  (ite (bvugt append_len_orig room2) room2 append_len_orig))
; Prove: append_len_final ≤ room2
(assert (not (bvule append_len_final room2)))
(check-sat)
; Expected: unsat — clamping always ensures append_len ≤ room
(pop)

; ============================================================================
; Claim 3: current_len + append_len_final ≤ MAX_IDX (319) = sizeof(value) - 1
; This proves the null terminator at value[current_len + append_len_final]
; is within bounds.
; ============================================================================
(push)
(declare-fun current_len () (_ BitVec 64))
(assert (bvule current_len MAX_IDX))
(define-fun r () (_ BitVec 64) (bvsub (bvsub VALUE_SIZE #x0000000000000001) current_len))
(declare-fun append_len_orig () (_ BitVec 64))
(assert (bvugt r #x0000000000000000)) ; room > 0
(define-fun a_final () (_ BitVec 64)
  (ite (bvugt append_len_orig r) r append_len_orig))
; The destination offset for the null terminator
(define-fun dest () (_ BitVec 64) (bvadd current_len a_final))
; Prove: dest ≤ 319 (within the buffer)
(assert (not (bvule dest MAX_IDX)))
(check-sat)
; Expected: unsat — the null terminator is always within the buffer
(pop)

; ============================================================================
; Claim 4: The memcpy destination is also within bounds
; value + current_len to value + current_len + append_len_final - 1
; The last byte accessed is at current_len + append_len_final - 1
; which must be ≤ 318 (since the null goes at 319)
; ============================================================================
(push)
(declare-fun current_len () (_ BitVec 64))
(assert (bvule current_len MAX_IDX))
(define-fun r4 () (_ BitVec 64) (bvsub (bvsub VALUE_SIZE #x0000000000000001) current_len))
(declare-fun append_len_orig () (_ BitVec 64))
(assert (bvugt r4 #x0000000000000000)) ; room > 0
(define-fun a_final4 () (_ BitVec 64)
  (ite (bvugt append_len_orig r4) r4 append_len_orig))
; Last byte accessed by memcpy (if a_final4 > 0)
; dest > 0 case: last byte = current_len + a_final4 - 1
(assert (bvugt a_final4 #x0000000000000000)) ; at least 1 byte to copy
(define-fun last_byte () (_ BitVec 64) (bvsub (bvadd current_len a_final4) #x0000000000000001))
; Prove: last_byte ≤ MAX_IDX - 1 = 318 (one position before null)
(define-fun LAST_BYTE_MAX () (_ BitVec 64) #x000000000000013E) ; 318
(assert (not (bvule last_byte LAST_BYTE_MAX)))
(check-sat)
; Expected: unsat — memcpy never reads past buffer boundary
(pop)

; ============================================================================
; Claim 5: Edge case — empty text input (function returns 0 early)
; The guard `if (!text || !text[0])` catches empty inputs.
; So append_len is always > 0 when we reach the memcpy.
; But the code handles append_len = 0 correctly (memcpy with count 0 is no-op).
; ============================================================================
(push)
(declare-fun current_len () (_ BitVec 64))
(assert (bvule current_len MAX_IDX))
(define-fun r5 () (_ BitVec 64) (bvsub (bvsub VALUE_SIZE #x0000000000000001) current_len))
(assert (bvugt r5 #x0000000000000000))
(declare-fun a_final5 () (_ BitVec 64))
(assert (bvule a_final5 r5)) ; clamp invariant
; Even with append_len = 0, current_len + 0 ≤ 319
(define-fun dest5 () (_ BitVec 64) (bvadd current_len a_final5))
(assert (not (bvule dest5 MAX_IDX)))
(check-sat)
; Expected: unsat — destination is always in bounds even with append_len=0
(pop)

; ============================================================================
; Claim 6: Full buffer edge case — current_len = MAX_IDX = 319
; When the buffer is full (319 chars + null), room = 319 - 319 = 0
; The guard `if (room == 0) return 0;` prevents overflow.
; We prove: when current_len = MAX_IDX, room = 0
; ============================================================================
(push)
(define-fun current_len_full () (_ BitVec 64) #x000000000000013F) ; 319
(define-fun room_full () (_ BitVec 64) (bvsub (bvsub VALUE_SIZE #x0000000000000001) current_len_full))
(assert (not (= room_full #x0000000000000000)))
(check-sat)
; Expected: unsat — room is 0 when buffer is full
(pop)
