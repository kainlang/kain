; Proof: Branchless kain_handle_nonzero_magic is equivalent
;
; Original:
;   static uint32_t kain_handle_nonzero_magic(uint32_t magic) {
;       magic &= 0x00ffffffu;
;       return magic == 0u ? 1u : magic;
;   }
;
; Branchless replacement:
;   static uint32_t kain_handle_nonzero_magic(uint32_t magic) {
;       uint32_t m = magic & 0x00ffffffu;
;       return m | (uint32_t)(m == 0u);
;   }
;
; Key insight:
;   If m == 0: m | 1 = 0 | 1 = 1  (same as original)
;   If m != 0: m | 0 = m          (same as original)
;
; The expression (m == 0u) evaluates to 1 (true) or 0 (false) as uint32_t.
; The bitwise OR with (m == 0) gives 1 when m is 0, and m when m is non-zero.
; This eliminates a branch — the compiler emits SETcc + OR instead of CMP + Jcc.
;
; Called on every handle_acquire and handle_release — hot path.

(set-logic QF_BV)

(declare-const magic (_ BitVec 32))

(define-fun masked () (_ BitVec 32)
  (bvand magic (_ bv16777215 32)))  ; 0x00FFFFFF

; Original: masked == 0 ? 1 : masked
(define-fun orig () (_ BitVec 32)
  (ite (= masked (_ bv0 32))
       (_ bv1 32)
       masked))

; Branchless: masked | (uint32_t)(masked == 0)
; (masked == 0) evaluates to 1 (true) in BV: (bvcomp masked (_ bv0 32))
(define-fun branchless () (_ BitVec 32)
  (bvor masked
        ((_ zero_extend 31) (bvcomp masked (_ bv0 32)))))  ; bvcomp returns 1 if equal, 0 otherwise

; Prove: orig == branchless for all 32-bit magic values
(assert (not (= orig branchless)))
(check-sat)
