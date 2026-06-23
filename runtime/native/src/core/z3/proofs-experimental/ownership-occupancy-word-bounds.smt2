; Proof: Occupancy word indexing stays within array bounds
;
; The occupancy tracking uses 64-bit words:
;   word_index = slot / KAIN_OWNERSHIP_WORD_BITS
;   word_bit   = slot % KAIN_OWNERSHIP_WORD_BITS
;   KAIN_OWNERSHIP_OCCUPANCY_WORDS[word_index] &= ~(1 << word_bit)
;
; With MAX_REGIONS = 4096, WORD_BITS = 64, WORD_COUNT = 64:
;   slot in [0, 4095]
;   word_index = slot / 64 in [0, 63]
;   word_bit   = slot % 64 in [0, 63]
;   WORD_COUNT = 64, so word_index < WORD_COUNT

(set-logic QF_BV)

(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(declare-const word_count (_ BitVec 32))
(declare-const word_bits (_ BitVec 32))

(assert (= max_regions (_ bv4096 32)))
(assert (= word_count (_ bv64 32)))
(assert (= word_bits (_ bv64 32)))

; Valid slot range
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun word_index () (_ BitVec 32)
  (bvudiv slot word_bits))

(define-fun word_bit () (_ BitVec 32)
  (bvurem slot word_bits))

; Claim 1: word_index < word_count
(assert (not (bvult word_index word_count)))
(check-sat)

(reset)

; ============================================================
; Claim 2: word_bit < word_bits
; ============================================================
(set-logic QF_BV)
(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(declare-const word_bits (_ BitVec 32))

(assert (= max_regions (_ bv4096 32)))
(assert (= word_bits (_ bv64 32)))
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun word_bit () (_ BitVec 32)
  (bvurem slot word_bits))

(assert (not (bvult word_bit word_bits)))
(check-sat)

(reset)

; ============================================================
; Claim 3: The bit mask (1 << word_bit) for word_bit < 64 doesn't overflow uint64_t
; i.e., 1 << word_bit is always a valid uint64_t value
; ============================================================
(set-logic QF_BV)
(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(declare-const word_bits (_ BitVec 32))

(assert (= max_regions (_ bv4096 32)))
(assert (= word_bits (_ bv64 32)))
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun word_bit () (_ BitVec 32)
  (bvurem slot word_bits))

; Encode 1 << word_bit as 64-bit bitvector
(define-fun bit_mask (_ BitVec 64)
  (bvshl (_ bv1 64) ((_ zero_extend 32) word_bit)))

; Claim: bit_mask != 0 (a valid single-bit mask)
(assert (= bit_mask (_ bv0 64)))
(check-sat)

(reset)

; ============================================================
; Claim 4: The bit_mask has exactly one bit set for all valid word_bit values
; (power-of-two property)
; ============================================================
(set-logic QF_BV)
(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(declare-const word_bits (_ BitVec 32))

(assert (= max_regions (_ bv4096 32)))
(assert (= word_bits (_ bv64 32)))
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun word_bit () (_ BitVec 32)
  (bvurem slot word_bits))

(define-fun bit_mask (_ BitVec 64)
  (bvshl (_ bv1 64) ((_ zero_extend 32) word_bit)))

; Power-of-two property: bit_mask & (bit_mask - 1) == 0
(define-fun is_power_of_two ((x (_ BitVec 64))) Bool
  (= (bvand x (bvsub x (_ bv1 64))) (_ bv0 64)))

(assert (not (is_power_of_two bit_mask)))
(check-sat)
