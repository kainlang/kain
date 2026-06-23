; Proof: Token zero/nonzero bit encoding consistency
;
; The functions abi_ui_token_nonzero_bit and abi_ui_token_zero_bit (lines ~137-141):
;   static uint64_t abi_ui_token_nonzero_bit(uint64_t value) {
;       return ((value | (UINT64_C(0) - value)) >> 63u) & UINT64_C(1);
;   }
;   static uint64_t abi_ui_token_zero_bit(uint64_t value) {
;       return abi_ui_token_nonzero_bit(value) ^ UINT64_C(1);
;   }
;
; abi_ui_token_nonzero_bit(value) returns:
;   1 if value != 0 (any bit set)
;   0 if value == 0
;
; abi_ui_token_zero_bit(value) returns the complement:
;   1 if value == 0
;   0 if value != 0
;
; These are used in abi_ui_token_match_bit to compare 16-byte tokens
; using bitwise equality checks.
;
; Key claims:
;   1. nonzero_bit(0) == 0
;   2. nonzero_bit(non-zero) == 1
;   3. zero_bit(v) == 1 iff v == 0
;   4. zero_bit(v) = 1 - nonzero_bit(v) (complement)

(set-logic QF_BV)

; ============================================================
; Claim 1: token_nonzero_bit(0) == 0
; ============================================================
(reset)
(set-logic QF_BV)

(define-const ZERO (_ BitVec 64) #x0000000000000000)
(define-const ONE (_ BitVec 64) #x0000000000000001)

; abi_ui_token_nonzero_bit(value):
;   return ((value | (0 - value)) >> 63) & 1;
(define-const neg_zero (_ BitVec 64) (bvsub ZERO ZERO))  ; 0 - 0 = 0
(define-const or_result (_ BitVec 64) (bvor ZERO neg_zero))
(define-const shifted (_ BitVec 64) (bvlshr or_result #x000000000000003F))  ; >> 63
(define-const nonzero_bit (_ BitVec 64) (bvand shifted ONE))

(assert (not (= nonzero_bit ZERO)))
(check-sat)
; Expected: unsat -- nonzero_bit(0) == 0

; ============================================================
; Claim 2: token_nonzero_bit(non-zero) == 1
; ============================================================
(reset)
(set-logic QF_BV)

(define-const ONE (_ BitVec 64) #x0000000000000001)

(declare-fun value () (_ BitVec 64))

; value != 0
(assert (not (= value #x0000000000000000)))

; abi_ui_token_nonzero_bit(value)
(define-const neg_value (_ BitVec 64) (bvsub #x0000000000000000 value))  ; 0 - value
(define-const or_result (_ BitVec 64) (bvor value neg_value))
(define-const shifted (_ BitVec 64) (bvlshr or_result #x000000000000003F))  ; >> 63
(define-const nonzero_bit (_ BitVec 64) (bvand shifted ONE))

; Prove: nonzero_bit == 1 for any non-zero value
(assert (not (= nonzero_bit ONE)))
(check-sat)
; Expected: unsat -- nonzero_bit(non-zero) == 1

; ============================================================
; Claim 3: token_zero_bit(value) == 1 iff value == 0
;
; token_zero_bit(value) = nonzero_bit(value) ^ 1
; ============================================================
(reset)
(set-logic QF_BV)

(define-const ONE (_ BitVec 64) #x0000000000000001)

(declare-fun value () (_ BitVec 64))

; Compute nonzero_bit as above
(define-const neg_value (_ BitVec 64) (bvsub #x0000000000000000 value))
(define-const or_result (_ BitVec 64) (bvor value neg_value))
(define-const shifted (_ BitVec 64) (bvlshr or_result #x000000000000003F))
(define-const nonzero_bit (_ BitVec 64) (bvand shifted ONE))

; zero_bit = nonzero_bit ^ 1
(define-const zero_bit (_ BitVec 64) (bvxor nonzero_bit ONE))

; Prove: zero_bit == 1 iff value == 0
; Direction 1: zero_bit == 1 => value == 0
(assert (= zero_bit ONE))
(assert (not (= value #x0000000000000000)))
(check-sat)
; Expected: unsat -- zero_bit==1 implies value==0

; ============================================================
; Claim 4: zero_bit(value) == 1 - nonzero_bit(value) [complement]
; ============================================================
(reset)
(set-logic QF_BV)

(define-const ONE (_ BitVec 64) #x0000000000000001)

(declare-fun value () (_ BitVec 64))

; Compute nonzero_bit
(define-const neg_value (_ BitVec 64) (bvsub #x0000000000000000 value))
(define-const or_result (_ BitVec 64) (bvor value neg_value))
(define-const shifted (_ BitVec 64) (bvlshr or_result #x000000000000003F))
(define-const nonzero_bit (_ BitVec 64) (bvand shifted ONE))

; zero_bit = nonzero_bit ^ 1
(define-const zero_bit (_ BitVec 64) (bvxor nonzero_bit ONE))

; Prove: zero_bit + nonzero_bit == 1
(define-const sum (_ BitVec 64) (bvadd zero_bit nonzero_bit))
(assert (not (= sum ONE)))
(check-sat)
; Expected: unsat -- zero_bit and nonzero_bit are always complements
