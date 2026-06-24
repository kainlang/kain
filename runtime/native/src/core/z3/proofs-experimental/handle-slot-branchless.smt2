; Proof: Branchless kain_handle_slot is equivalent
;
; Original:
;   uint32_t kain_handle_slot(KainRuntimeHandle handle) {
;       uint32_t encoded = (uint32_t)(handle & KAIN_HANDLE_SLOT_MASK);
;       return encoded == 0u ? UINT32_MAX : encoded - 1u;
;   }
;
; Branchless replacement:
;   uint32_t kain_handle_slot(KainRuntimeHandle handle) {
;       return ((uint32_t)(handle & KAIN_HANDLE_SLOT_MASK)) - 1u;
;   }
;
; The key insight: for unsigned 32-bit arithmetic, 0 - 1 wraps to UINT32_MAX (0xFFFFFFFF),
; which is exactly the sentinel value returned when encoded == 0.
; For all non-zero encoded values, encoded - 1 is correct.
;
; This eliminates a branch (cmp + jne/cmov) on every handle extraction path.

(set-logic QF_BV)

(declare-const encoded (_ BitVec 32))

; Original: encoded == 0 ? 0xFFFFFFFF : encoded - 1
(define-fun orig () (_ BitVec 32)
  (ite (= encoded (_ bv0 32))
       (_ bv4294967295 32)       ; UINT32_MAX = 0xFFFFFFFF
       (bvsub encoded (_ bv1 32))))

; Branchless: encoded - 1 (wraps on underflow)
(define-fun branchless () (_ BitVec 32)
  (bvsub encoded (_ bv1 32)))

; Prove: orig == branchless for all 32-bit encoded values
(assert (not (= orig branchless)))
(check-sat)
