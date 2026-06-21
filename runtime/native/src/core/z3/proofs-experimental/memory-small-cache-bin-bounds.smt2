; Proof: kain_alloc_cache_small_bin(payload_size) stays within valid
; small_bins array bounds.
;
; Current code:
;   return (payload_size >> 4u) - 1u;
;
; Small bin array size:
;   KAIN_ALLOC_CACHE_SMALL_BIN_COUNT =
;     KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD / KAIN_ALLOC_CACHE_SMALL_QUANTUM
;   = 8192 / 16 = 512 bins
;
; Valid payload_size range (checked by kain_alloc_cache_small_eligible):
;   sizeof(KainAllocHeader*) <= payload_size <= KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD
;   AND (payload_size & (KAIN_ALLOC_CACHE_SMALL_QUANTUM - 1)) == 0
;   AND (flags & VIRTUAL) == 0
;
; sizeof(KainAllocHeader*) = 8 (64-bit) or 4 (32-bit). We prove for 8.
; payload_size must be a multiple of 16 (KAIN_ALLOC_CACHE_SMALL_QUANTUM)
;
; So valid payload_size values are: 16, 32, 48, ..., 8192
; (16 is the first value >= sizeof(KainAllocHeader*) = 8 that's also a mult of 16)
;
; For payload_size = 16: bin = (16 >> 4) - 1 = 1 - 1 = 0
; For payload_size = 8192: bin = (8192 >> 4) - 1 = 512 - 1 = 511
;
; So bin range: [0, 511] which is within small_bins[memtype][0..511]
; Bin count = 512, so max index = 511. ✓

(set-logic QF_BV)
(declare-const payload_size (_ BitVec 64))

; Constants
(define-fun QUANTUM () (_ BitVec 64) (_ bv16 64))
(define-fun MIN_PAYLOAD () (_ BitVec 64) (_ bv16 64))  ; >= 8 and mult of 16
(define-fun MAX_PAYLOAD () (_ BitVec 64) (_ bv8192 64))
(define-fun BIN_COUNT () (_ BitVec 64) (_ bv512 64))

; Constraints: eligible payload
(assert (bvuge payload_size MIN_PAYLOAD))
(assert (bvule payload_size MAX_PAYLOAD))
; Must be quantum-aligned: (payload_size & 15) == 0
(assert (= (bvand payload_size (_ bv15 64)) (_ bv0 64)))

; Compute bin
(define-fun bin () (_ BitVec 64)
  (bvsub (bvlshr payload_size (_ bv4 64)) (_ bv1 64)))

; Prove: bin >= 0 (always true for unsigned, but check conceptually)
; and bin < BIN_COUNT
(assert (bvuge bin BIN_COUNT))  ; bin >= 512 => out of bounds
(check-sat)
; Expected: unsat (bin is always < 512)

(reset)

; ============================================================
; Claim 2: For the minimum valid payload (16), bin is 0
; ============================================================
(set-logic QF_BV)
(define-fun bin_min () (_ BitVec 64)
  (bvsub (bvlshr (_ bv16 64) (_ bv4 64)) (_ bv1 64)))
(assert (not (= bin_min (_ bv0 64))))
(check-sat)
; Expected: unsat

(reset)

; ============================================================
; Claim 3: For the maximum valid payload (8192), bin is 511
; ============================================================
(set-logic QF_BV)
(define-fun bin_max () (_ BitVec 64)
  (bvsub (bvlshr (_ bv8192 64) (_ bv4 64)) (_ bv1 64)))
(assert (not (= bin_max (_ bv511 64))))
(check-sat)
; Expected: unsat

(reset)

; ============================================================
; Claim 4: Bin formula is monotonic: larger payload -> larger bin
; ============================================================
(set-logic QF_BV)
(declare-const a (_ BitVec 64))
(declare-const b (_ BitVec 64))

(assert (bvuge a MIN_PAYLOAD))
(assert (bvule a MAX_PAYLOAD))
(assert (= (bvand a (_ bv15 64)) (_ bv0 64)))
(assert (bvuge b MIN_PAYLOAD))
(assert (bvule b MAX_PAYLOAD))
(assert (= (bvand b (_ bv15 64)) (_ bv0 64)))
(assert (bvugt b a))  ; b > a

(define-fun bin_a () (_ BitVec 64)
  (bvsub (bvlshr a (_ bv4 64)) (_ bv1 64)))
(define-fun bin_b () (_ BitVec 64)
  (bvsub (bvlshr b (_ bv4 64)) (_ bv1 64)))

; Prove: bin_b > bin_a
(assert (not (bvugt bin_b bin_a)))
(check-sat)
; Expected: unsat (monotonicity holds)

(reset)

; ============================================================
; Claim 5: Every valid payload maps to a unique bin (injective)
; ============================================================
(set-logic QF_BV)
(declare-const x (_ BitVec 64))
(declare-const y (_ BitVec 64))

(assert (bvuge x MIN_PAYLOAD))
(assert (bvule x MAX_PAYLOAD))
(assert (= (bvand x (_ bv15 64)) (_ bv0 64)))
(assert (bvuge y MIN_PAYLOAD))
(assert (bvule y MAX_PAYLOAD))
(assert (= (bvand y (_ bv15 64)) (_ bv0 64)))
(assert (not (= x y)))

(define-fun bin_x () (_ BitVec 64)
  (bvsub (bvlshr x (_ bv4 64)) (_ bv1 64)))
(define-fun bin_y () (_ BitVec 64)
  (bvsub (bvlshr y (_ bv4 64)) (_ bv1 64)))

; Prove: x != y => bin_x != bin_y
(assert (= bin_x bin_y))
(check-sat)
; Expected: unsat (bijection between payload and bin for the aligned range)

(reset)

; ============================================================
; Claim 6: The bin formula is equivalent to (payload_size / 16) - 1
; for the eligible range. Since payload_size is always a multiple
; of 16, the shift by 4 is equivalent to division.
; ============================================================
(set-logic QF_BV)
(declare-const ps (_ BitVec 64))
(assert (= (bvand ps (_ bv15 64)) (_ bv0 64)))  ; mult of 16

(define-fun shift_result () (_ BitVec 64)
  (bvsub (bvlshr ps (_ bv4 64)) (_ bv1 64)))

; For values that are multiples of 16: shift-right by 4 == divide by 16
; We verify this when there's no overflow
(assert (not (= (bvlshr ps (_ bv4 64)) (bvudiv ps (_ bv16 64)))))
(check-sat)
; Expected: unsat (when ps is a multiple of 16)

(reset)

; ============================================================
; Claim 7: For power-of-two multiple, x/16 == x>>4 always
; (Even without the alignment constraint, for 64-bit values)
; ============================================================
(set-logic QF_BV)
(declare-const x (_ BitVec 64))
(assert (not (= (bvlshr x (_ bv4 64)) (bvudiv x (_ bv16 64)))))
(check-sat)
; Expected: sat with counterexample (when x is not a multiple of 16)
; This proves that the alignment check IS needed for correctness
