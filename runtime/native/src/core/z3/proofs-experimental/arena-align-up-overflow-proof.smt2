; Proof: kain_align_up_size overflow detection is correct.
;
; The function:
;   size_t kain_align_up_size(size_t value, size_t alignment, int* overflowed) {
;     if (overflowed) *overflowed = 0;
;     if (alignment == 0) alignment = 1;
;     if (!is_power_of_two(alignment)) { *overflowed = 1; return 0; }
;     size_t mask = alignment - 1;
;     if (value > SIZE_MAX - mask) { *overflowed = 1; return 0; }
;     return (value + mask) & ~mask;
;   }
;
; Key claims to prove:
;   1. When value > SIZE_MAX - mask, (value + mask) would overflow
;   2. When value <= SIZE_MAX - mask, (value + mask) & ~mask is safe and correct
;   3. The result is always a multiple of alignment
;   4. If value is already aligned, result == value
;   5. result >= value (align_up never shrinks)

(set-logic QF_BV)
(declare-const value (_ BitVec 64))
(declare-const alignment (_ BitVec 64))

; Power-of-two alignment: exactly one bit set
(assert (= (bvand alignment (bvsub alignment (_ bv1 64))) (_ bv0 64)))
(assert (bvugt alignment (_ bv0 64)))

(define-fun mask () (_ BitVec 64) (bvsub alignment (_ bv1 64)))
(define-fun can_add () Bool (bvule value (bvsub (bvnot (_ bv0 64)) mask)))
(define-fun result () (_ BitVec 64)
  (bvand (bvadd value mask) (bvnot mask)))

; Claim 1: If can_add is false, (value + mask) would overflow
(assert (not can_add))
(assert (not (bvult (bvadd value mask) value)))  ; overflow check would fail
; Actually overflow in unsigned is: result < value (wrapping)
(check-sat)

(reset)

; ============================================================
; Claim 2: When can_add is true, result is correct
; ============================================================
(set-logic QF_BV)
(declare-const value (_ BitVec 64))
(declare-const alignment (_ BitVec 64))

(assert (= (bvand alignment (bvsub alignment (_ bv1 64))) (_ bv0 64)))
(assert (bvugt alignment (_ bv0 64)))

(define-fun mask () (_ BitVec 64) (bvsub alignment (_ bv1 64)))
(define-fun can_add () Bool (bvule value (bvsub (bvnot (_ bv0 64)) mask)))

; Add constraint: no overflow
(assert can_add)

(define-fun result () (_ BitVec 64)
  (bvand (bvadd value mask) (bvnot mask)))

; Claim 2a: result is a multiple of alignment
; (result & mask) == 0
(assert (not (= (bvand result mask) (_ bv0 64))))
(check-sat)
; Expected: unsat

(reset)

; ============================================================
; Claim 2b: result >= value (align_up never shrinks)
; ============================================================
(set-logic QF_BV)
(declare-const value (_ BitVec 64))
(declare-const alignment (_ BitVec 64))

(assert (= (bvand alignment (bvsub alignment (_ bv1 64))) (_ bv0 64)))
(assert (bvugt alignment (_ bv0 64)))

(define-fun mask () (_ BitVec 64) (bvsub alignment (_ bv1 64)))
(define-fun can_add () Bool (bvule value (bvsub (bvnot (_ bv0 64)) mask)))
(assert can_add)

(define-fun result () (_ BitVec 64)
  (bvand (bvadd value mask) (bvnot mask)))

(assert (bvult result value))
(check-sat)
; Expected: unsat

(reset)

; ============================================================
; Claim 3: If value is already aligned, result == value
; ============================================================
(set-logic QF_BV)
(declare-const value (_ BitVec 64))
(declare-const alignment (_ BitVec 64))

(assert (= (bvand alignment (bvsub alignment (_ bv1 64))) (_ bv0 64)))
(assert (bvugt alignment (_ bv0 64)))

(define-fun mask () (_ BitVec 64) (bvsub alignment (_ bv1 64)))
(define-fun can_add () Bool (bvule value (bvsub (bvnot (_ bv0 64)) mask)))
(assert can_add)

; value is already aligned: (value & mask) == 0
(assert (= (bvand value mask) (_ bv0 64)))

(define-fun result () (_ BitVec 64)
  (bvand (bvadd value mask) (bvnot mask)))

(assert (not (= result value)))
(check-sat)
; Expected: unsat

(reset)

; ============================================================
; Claim 4: align_down_size is correct inverse
; align_down: value & ~mask
; align_up: (value + mask) & ~mask
; For any aligned result R = align_up(value):
;   align_down(R) == R (already aligned)
; ============================================================
(set-logic QF_BV)
(declare-const value (_ BitVec 64))
(declare-const alignment (_ BitVec 64))

(assert (= (bvand alignment (bvsub alignment (_ bv1 64))) (_ bv0 64)))
(assert (bvugt alignment (_ bv0 64)))

(define-fun mask () (_ BitVec 64) (bvsub alignment (_ bv1 64)))
(define-fun can_add () Bool (bvule value (bvsub (bvnot (_ bv0 64)) mask)))
(assert can_add)

(define-fun aligned_up () (_ BitVec 64)
  (bvand (bvadd value mask) (bvnot mask)))

; align_down(aligned_up(value)) == aligned_up(value)
(define-fun align_down ((v (_ BitVec 64))) (_ BitVec 64)
  (bvand v (bvnot mask)))

(assert (not (= (align_down aligned_up) aligned_up)))
(check-sat)
; Expected: unsat

(reset)

; ============================================================
; Claim 5: Value-equivalence between shift and mask for power-of-two
; align_up(x, 2^N) can also be computed as:
;   ((x - 1) >> N + 1) << N   (for x > 0)
; ============================================================
(set-logic QF_BV)
(declare-const value (_ BitVec 64))
(declare-const shift (_ BitVec 64))  ; log2(alignment)

; alignment = 1 << shift, shift in [0, 63]
; For this proof, let's use a concrete shift value of 4 (alignment = 16)
(define-fun alignment () (_ BitVec 64) (_ bv16 64))
(define-fun mask () (_ BitVec 64) (_ bv15 64))
(define-fun can_add () Bool (bvule value (bvsub (bvnot (_ bv0 64)) mask)))
(assert can_add)

(define-fun mask_result () (_ BitVec 64)
  (bvand (bvadd value mask) (bvnot mask)))

(define-fun shift_result () (_ BitVec 64)
  (bvshl (bvadd (bvlshr (bvsub value (_ bv1 64)) (_ bv4 64)) (_ bv1 64)) (_ bv4 64)))

(assert (not (= mask_result shift_result)))
(check-sat)
; Expected: unsat (for power-of-two alignment, both formulas match)
