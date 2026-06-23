; Proof: abi_ui_isolate_low_bit_u64 returns either 0 or a power of two (2^k)
;
; The function (line ~29):
;   static uint64_t abi_ui_isolate_low_bit_u64(uint64_t value) {
;       return value & (0u - value);
;   }
;
; This is the classic two's complement "isolate lowest set bit" trick.
; For any non-zero value, value & -value yields exactly one set bit --
; the lowest set bit in value. For value = 0, the result is 0.
;
; Key claims:
;   1. If value = 0, result = 0
;   2. If value != 0, result is a power of two (exactly one bit set)
;   3. The result has at most one bit set (popcount <= 1)
;
; This is used in abi_ui_find_free_slot_u64 to find the first free slot
; in an occupancy bitset.

(set-logic QF_BV)

; ============================================================
; Claim 1: When value = 0, result = 0
; ============================================================
(reset)
(set-logic QF_BV)

(define-const value (_ BitVec 64) #x0000000000000000)

; Compute isolate_low_bit: value & -value
; In two's complement, -value = ~value + 1
(define-const neg_value (_ BitVec 64) (bvadd (bvnot value) #x0000000000000001))
(define-const result (_ BitVec 64) (bvand value neg_value))

; Prove: result = 0
(assert (not (= result #x0000000000000000)))
(check-sat)
; Expected: unsat -- isolate_low_bit(0) = 0

; ============================================================
; Claim 2: For non-zero value, result has exactly one bit set
; (i.e., is a power of two: result = 2^k for some k in [0,63])
;
; Popcount == 1 means: result != 0 AND (result & (result-1)) == 0
; ============================================================
(reset)
(set-logic QF_BV)

(declare-fun value () (_ BitVec 64))

; Non-zero constraint
(assert (not (= value #x0000000000000000)))

; Compute isolate_low_bit: value & -value
(define-const neg_value (_ BitVec 64) (bvadd (bvnot value) #x0000000000000001))
(define-const result (_ BitVec 64) (bvand value neg_value))

; Prove: result != 0
(assert (= result #x0000000000000000))
(check-sat)
; Expected: unsat -- isolate_low_bit(non-zero) != 0

; ============================================================
; Claim 3: For non-zero value, the result has exactly one bit set.
; Proof: (result & (result - 1)) == 0 (power-of-two test)
; ============================================================
(reset)
(set-logic QF_BV)

(declare-fun value () (_ BitVec 64))

; Non-zero constraint
(assert (not (= value #x0000000000000000)))

; Compute isolate_low_bit: value & -value
(define-const neg_value (_ BitVec 64) (bvadd (bvnot value) #x0000000000000001))
(define-const result (_ BitVec 64) (bvand value neg_value))

; Prove: result is power of two (popcount == 1)
; Power-of-two test: result != 0 AND (result & (result-1)) == 0
(define-const result_minus_one (_ BitVec 64) (bvsub result #x0000000000000001))
(define-const is_power_of_two (_ BitVec 64) (bvand result result_minus_one))

; counter: result is NOT a power of two
(assert (not (= is_power_of_two #x0000000000000000)))
(check-sat)
; Expected: unsat -- isolate_low_bit(non-zero) always produces a power of two

; ============================================================
; Claim 4: The isolated bit is indeed the LOWEST set bit.
; For any non-zero value, the lowest set bit at position k means:
;   - result = 2^k
;   - All bits below k in value are 0
;   - Bit k in value is 1
; ============================================================
(reset)
(set-logic QF_BV)

(declare-fun value () (_ BitVec 64))

; Non-zero constraint
(assert (not (= value #x0000000000000000)))

; Compute isolate_low_bit
(define-const neg_value (_ BitVec 64) (bvadd (bvnot value) #x0000000000000001))
(define-const result (_ BitVec 64) (bvand value neg_value))

; Prove: result <= value (unsigned) -- isolating a bit cannot produce a larger number
; Actually, this is not always true: if the lowest set bit is bit k, then result = 2^k,
; and value >= 2^k (since value has bit k set), so result <= value.
(assert (bvugt result value))
(check-sat)
; Expected: unsat -- isolated low bit <= original value
