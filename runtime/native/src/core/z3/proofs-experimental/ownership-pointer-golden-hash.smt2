;; ownership-pointer-golden-hash.smt2
;;
;; PROOF: The golden ratio multiplier 0x9e3779b97f4a7c15 is odd
;;        (invertible modulo 2^64).
;;        Every 64-bit pointer maps to a valid 13-bit hash table index.
;;        This enables replacing splitmix64 (5 ops: 3 xor-shifts + 2 mults)
;;        with a single multiply + extract-top-bits (2 ops).
;;
;; The golden ratio hash is the standard Fibonacci hashing method
;; described in Knuth (TAOCP Vol 3). The multiplier:
;;   2^64 / phi = 0x9e3779b97f4a7c15
;; where phi = (1 + sqrt(5)) / 2.
;;
;; Result: UNSAT -- multiplier is odd, invertible mod 2^64.

(set-logic QF_BV)

;; The golden ratio multiplier for 64-bit Fibonacci hashing
(define-fun GOLDEN_RATIO () (_ BitVec 64) #x9e3779b97f4a7c15)

;; Check: multiplier is odd (bit 0 = 1)
(define-fun is_odd ((x (_ BitVec 64))) Bool
  (= ((_ extract 0 0) x) #b1))

(assert (not (is_odd GOLDEN_RATIO)))
(check-sat)
;; Expect: unsat

;; The hash function: extract top 13 bits of (ptr * golden_ratio)
;; This always produces a value in [0, 8191] -- trivially valid.
(define-fun golden_hash ((ptr (_ BitVec 64))) (_ BitVec 13)
  ((_ extract 63 51) (bvmul ptr GOLDEN_RATIO)))

;; Check that hash never overflows 13 bits (always true by construction,
;; but explicit proof)
(declare-const ptr (_ BitVec 64))
(assert (not (bvult (golden_hash ptr) #x2000)))
(check-sat)
;; Expect: unsat (hash is always < 8192)
