; Proof: Simplify kain_alloc_cache_large_bucket hash function
;
; Current code (splitmix64):
;   uint64_t mixed = (uint64_t)payload_size * UINT64_C(11400714819323198485);
;   mixed ^= mixed >> 33u;
;   return (size_t)(mixed & (KAIN_ALLOC_CACHE_HASH_BUCKETS - 1u));
;
; Simplified (top-6-bits extract, no XOR-shift):
;   uint64_t mixed = (uint64_t)payload_size * UINT64_C(11400714819323198485);
;   return (size_t)(mixed >> 58);
;
; Domain: payload_size in [2048 .. 262144], 64 buckets.
;
; Why this works: extracting the TOP 6 bits (>> 58) instead of the
; BOTTOM 6 bits (& 63) avoids the power-of-two bias problem.
; For power-of-two payload_size = 2^k, the multiplication M * 2^k
; shifts M left by k; the top 6 bits of the truncated 64-bit result
; depend on k and M, giving good distribution. The XOR-shift step
; is unnecessary when using top-bit extraction.
;
; We prove:
; 1. No hash collision between any two power-of-two sizes in [2048..262144]
; 2. The hash of adjacent sizes differs (avalanche check)
; 3. For a representative sample of sizes, no clustering

(set-logic QF_BV)

(define-fun MAGIC () (_ BitVec 64) (_ bv11400714819323198485 64))

; Original: bottom 6 bits after XOR-shift 33
(define-fun orig_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (let ((mixed (bvmul x MAGIC)))
  (let ((mixed2 (bvxor mixed (bvlshr mixed (_ bv33 64)))))
    (bvand mixed2 (_ bv63 64)))))

; Simplified: top 6 bits of product (no XOR-shift)
(define-fun simp_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (bvlshr (bvmul x MAGIC) (_ bv58 64)))

; ================================================================
; Claim 1: For power-of-two sizes in [2048, 262144] = 2^11 .. 2^18,
; the simplified hash produces no collisions.
; ================================================================

; Check all pairs of distinct powers of two
(define-fun pow2 ((k (_ BitVec 64))) (_ BitVec 64)
  (bvshl (_ bv1 64) k))

(declare-const k1 (_ BitVec 64))
(declare-const k2 (_ BitVec 64))
(assert (bvuge k1 (_ bv11 64)))
(assert (bvule k1 (_ bv18 64)))
(assert (bvuge k2 (_ bv11 64)))
(assert (bvule k2 (_ bv18 64)))
(assert (not (= k1 k2)))

; Distinct power-of-two sizes should NOT collide in simplified hash
(assert (= (simp_hash (pow2 k1)) (simp_hash (pow2 k2))))
(check-sat)
; Expected: unsat (no collisions for powers of two)

(reset)

; ================================================================
; Claim 2: For all powers of two, the simplified hash produces
; non-zero values (no zero bias).
; ================================================================
(set-logic QF_BV)
(define-fun MAGIC () (_ BitVec 64) (_ bv11400714819323198485 64))
(define-fun simp_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (bvlshr (bvmul x MAGIC) (_ bv58 64)))

(define-fun pow2 ((k (_ BitVec 64))) (_ BitVec 64)
  (bvshl (_ bv1 64) k))

(declare-const k (_ BitVec 64))
(assert (bvuge k (_ bv11 64)))
(assert (bvule k (_ bv18 64)))
(assert (= (simp_hash (pow2 k)) (_ bv0 64)))
(check-sat)
; Expected: unsat (no power-of-two size hashes to zero)

(reset)

; ================================================================
; Claim 3: For the 256-size sample [2048, 2304) in steps of 1,
; the simplified hash has no more collisions than the original.
; We check: does any pair in this range collide in both hashes?
; ================================================================
(set-logic QF_BV)
(define-fun MAGIC () (_ BitVec 64) (_ bv11400714819323198485 64))
(define-fun orig_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (let ((mixed (bvmul x MAGIC)))
  (let ((mixed2 (bvxor mixed (bvlshr mixed (_ bv33 64)))))
    (bvand mixed2 (_ bv63 64)))))
(define-fun simp_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (bvlshr (bvmul x MAGIC) (_ bv58 64)))

(declare-const a (_ BitVec 64))
(declare-const b (_ BitVec 64))
(assert (bvuge a (_ bv2048 64)))
(assert (bvult a (_ bv2304 64)))
(assert (bvuge b (_ bv2048 64)))
(assert (bvult b (_ bv2304 64)))
(assert (not (= a b)))

; Check: if original has no collision between a and b,
; does simplified also have no collision?
; We want: (orig_hash a) != (orig_hash b) => (simp_hash a) != (simp_hash b)
; Equivalent: (simp_hash a) = (simp_hash b) AND (orig_hash a) != (orig_hash b)
; If unsat, then whenever original has no collision, simplified also has no collision
(assert (and (not (= (orig_hash a) (orig_hash b)))
             (= (simp_hash a) (simp_hash b))))
(check-sat)
; Expected: unsat (simplified hash is at least as good as original for this range)
; If sat with a counterexample, the simplified hash has an extra collision
; that original doesn't have.

(reset)

; ================================================================
; Claim 4: For power-of-two sizes, the simplified hash and original
; hash always agree on whether the values are distinct.
; ================================================================
(set-logic QF_BV)
(define-fun MAGIC () (_ BitVec 64) (_ bv11400714819323198485 64))
(define-fun orig_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (let ((mixed (bvmul x MAGIC)))
  (let ((mixed2 (bvxor mixed (bvlshr mixed (_ bv33 64)))))
    (bvand mixed2 (_ bv63 64)))))
(define-fun simp_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (bvlshr (bvmul x MAGIC) (_ bv58 64)))

(define-fun pow2 ((k (_ BitVec 64))) (_ BitVec 64)
  (bvshl (_ bv1 64) k))

(declare-const i (_ BitVec 64))
(declare-const j (_ BitVec 64))
(assert (bvuge i (_ bv11 64)))
(assert (bvule i (_ bv18 64)))
(assert (bvuge j (_ bv11 64)))
(assert (bvule j (_ bv18 64)))
(assert (not (= i j)))

; For every pair of distinct powers of two in the valid range,
; both hashes should agree: they're either both distinct or both colliding.
; Ideally, both should be distinct (which we proved in Claim 1 for simplified).
(assert (not (= (= (orig_hash (pow2 i)) (orig_hash (pow2 j)))
                (= (simp_hash (pow2 i)) (simp_hash (pow2 j))))))
(check-sat)
; Expected: unsat (both hashes agree on distinctness for power-of-two sizes)

(reset)

; ================================================================
; Claim 5: Check if XOR-shift ever changes the bottom 6 bits
; for any payload_size in the valid range.
; If orig_hash == simp_hash for all valid payload_sizes, then
; the XOR-shift is redundant even for the original bottom-6-bit extraction!
; ================================================================
(set-logic QF_BV)
(define-fun MAGIC () (_ BitVec 64) (_ bv11400714819323198485 64))
(define-fun orig_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (let ((mixed (bvmul x MAGIC)))
  (let ((mixed2 (bvxor mixed (bvlshr mixed (_ bv33 64)))))
    (bvand mixed2 (_ bv63 64)))))
(define-fun bottom6_hash ((x (_ BitVec 64))) (_ BitVec 64)
  (bvand (bvmul x MAGIC) (_ bv63 64)))

(declare-const ps (_ BitVec 64))
(assert (bvuge ps (_ bv2048 64)))
(assert (bvule ps (_ bv262144 64)))

; Find a payload_size where the XOR-shift actually changes the result
(assert (not (= (orig_hash ps) (bottom6_hash ps))))
(check-sat)
; If sat: there EXISTS a valid payload_size where XOR-shift matters
; If unsat: XOR-shift never changes the bottom 6 bits for any valid payload
; This would mean the XOR-shift is completely redundant for this domain!
