; Branchless power-of-2-minus-1 mask check
;
; Proves that:
;   branchless = (~sign_bit_64 & ~((x & (x+1)) | -(x & (x+1)))) >> 63
; is equivalent to:
;   original = mask >= 0 && (unsigned_mask & (unsigned_mask + 1)) == 0
;
; Where x = um = (uint64_t)mask
;
; The branchless form replaces the && (which may compile to a branch)
; with straight-line bit arithmetic.
;
; Part 1: ~(x | -x) >> 63 is 1 when x == 0, 0 otherwise
;   For x=0: x|-x = 0, ~0 = UINT64_MAX, >>63 = 1
;   For x!=0: x|-x has MSB set (since -x is negative), ~ flips to 0, >>63 = 0

(set-logic QF_BV)

; === Part 1: Prove ~(x | -x) >> 63 = 1 iff x == 0 ===
(declare-const x (_ BitVec 64))

(define-fun is_zero_branchless ((v (_ BitVec 64)) (_ BitVec 64))
  (bvlshr (bvnot (bvor v (bvneg v))) (_ bv63 64)))

(define-fun reference ((v (_ BitVec 64)) (_ BitVec 64))
  (ite (= v (_ bv0 64)) (_ bv1 64) (_ bv0 64)))

(assert (not (= (is_zero_branchless x) (reference x))))
(check-sat)

(reset)

; === Part 2: Full mask check equivalence ===
; original: uint64_t mask >= 0 (sign bit clear) AND x & (x+1) == 0
;   where x = (uint64_t)mask
; branchless: ((~sign_bit) & ~(x & (x+1) | -(x & (x+1)))) >> 63

(declare-const mask (_ BitVec 64))
(define-fun um () (_ BitVec 64) mask)

; sign_ok = all-1s if mask >= 0 (MSB clear), all-0s if mask < 0
(define-fun sign_ok () (_ BitVec 64)
  (bvnot (bvlshr um (_ bv63 64))))

; x = um & (um + 1), call this t
(define-fun t () (_ BitVec 64)
  (bvand um (bvadd um (_ bv1 64))))

; pow2_ok = all-1s if t == 0, all-0s otherwise
(define-fun pow2_ok () (_ BitVec 64)
  (bvnot (bvor t (bvneg t))))

; branchless result (extract bit 0)
(define-fun branchless_result () (_ BitVec 1)
  ((_ extract 0 0) (bvlshr (bvand sign_ok pow2_ok) (_ bv63 64))))

; reference result: mask >= 0 AND (um & (um+1)) == 0
(define-fun mask_ge_0 () Bool
  (= ((_ extract 63 63) mask) (_ bv0 1)))

(define-fun ref_result () (_ BitVec 1)
  (ite (and mask_ge_0 (= t (_ bv0 64)))
       (_ bv1 1)
       (_ bv0 1)))

(assert (not (= branchless_result ref_result)))
(check-sat)
; Expected: unsat ✅
