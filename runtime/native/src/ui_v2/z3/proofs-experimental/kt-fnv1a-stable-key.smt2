; ============================================================================
; kt-fnv1a-stable-key.smt2
; Claim: SplitMix64 post-processing is bijective on full 64-bit space.
; Also: FNV-1a offset basis and prime are well-formed.
;
; Used in hash_table.c (kt_hash_fnv1a_64, kt_hash_splitmix64):
;   hash = 0xcbf29ce484222325
;   for each byte: hash = (hash ^ byte) * 0x100000001b3
;   hash ^= hash >> 30; hash *= 0xbf58476d1ce4e5b9
;   hash ^= hash >> 27; hash *= 0x94d049bb133111eb
;   hash ^= hash >> 31
;
; Modular inverses (found via Z3 solver-assisted search):
;   inv(0xbf58476d1ce4e5b9) = 0x96de1b173f119089
;   inv(0x94d049bb133111eb) = 0x319642b2d24d8ec3
;
; Proof strategy: SplitMix64 = f5 ∘ f4 ∘ f3 ∘ f2 ∘ f1
;   Each fi is invertible (proved individually)
;   Composition of bijections is bijective
; ============================================================================

; ---- FNV-1a CONSTANTS ----

; Claim 1: FNV offset basis is non-zero
(reset)
(set-logic QF_BV)
(assert (= #xcbf29ce484222325 #x0000000000000000))
(check-sat)
; >>> unsat ✓

; Claim 2: FNV prime (64-bit zero-extended) has LSB=1 (odd → invertible multiply)
(reset)
(set-logic QF_BV)
(assert (= ((_ extract 0 0) #x00000001000001b3) #b0))
(check-sat)
; >>> unsat ✓

; Claim 3: FNV-1a single-byte hash is non-zero for non-zero input
(reset)
(set-logic QF_BV)
(declare-const b (_ BitVec 8))
(assert (not (= b #x00)))
(assert (= (bvmul (bvxor #xcbf29ce484222325 ((_ zero_extend 56) b))
                  #x00000001000001b3)
           #x0000000000000000))
(check-sat)
; >>> unsat ✓

; ---- SPLITMIX64: INDIVIDUAL STEP INVERTIBILITY ----

; Claim 4: inv(M1) * M1 = 1 (mod 2^64)
(reset)
(set-logic QF_BV)
(assert (not (= (bvmul #x96de1b173f119089 #xbf58476d1ce4e5b9) #x0000000000000001)))
(check-sat)
; >>> unsat: M1 multiply is invertible ✓

; Claim 5: inv(M2) * M2 = 1 (mod 2^64)
(reset)
(set-logic QF_BV)
(assert (not (= (bvmul #x319642b2d24d8ec3 #x94d049bb133111eb) #x0000000000000001)))
(check-sat)
; >>> unsat: M2 multiply is invertible ✓

; Claim 6a: xorshift-30 inverse correct: g1(f1(x)) = x
(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))
(assert (not
  (= (let ((w (bvxor (bvxor x (bvlshr x (_ bv30 64)))
                     (bvlshr (bvxor x (bvlshr x (_ bv30 64))) (_ bv30 64)))))
       (bvxor w (bvlshr w (_ bv60 64))))
     x)))
(check-sat)
; >>> unsat: xorshift-30 is invertible ✓

; Claim 6b: xorshift-27 inverse correct: g3(f3(x)) = x
(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))
(assert (not
  (= (let ((w (bvxor (bvxor x (bvlshr x (_ bv27 64)))
                     (bvlshr (bvxor x (bvlshr x (_ bv27 64))) (_ bv27 64)))))
       (bvxor w (bvlshr w (_ bv54 64))))
     x)))
(check-sat)
; >>> unsat: xorshift-27 is invertible ✓

; Claim 6c: xorshift-31 inverse correct: g5(f5(x)) = x
(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))
(assert (not
  (= (let ((w (bvxor (bvxor x (bvlshr x (_ bv31 64)))
                     (bvlshr (bvxor x (bvlshr x (_ bv31 64))) (_ bv31 64)))))
       (bvxor w (bvlshr w (_ bv62 64))))
     x)))
(check-sat)
; >>> unsat: xorshift-31 is invertible ✓

; ---- COMPOSITION: FULL SPLITMIX64 BIJECTIVITY ----
; SplitMix64 = f5 ∘ f4 ∘ f3 ∘ f2 ∘ f1
; Each fi is a bijection (proved above).
; Therefore SplitMix64 is a bijection.
; QED (mathematical theorem: composition of bijections is bijective)

; ---- ADDITIONAL: M1 and M2 are odd (invertibility prerequisite) ----
; Claim 7: M1 is odd
(reset)
(set-logic QF_BV)
(assert (= ((_ extract 0 0) #xbf58476d1ce4e5b9) #b0))
(check-sat)
; >>> unsat ✓

; Claim 8: M2 is odd
(reset)
(set-logic QF_BV)
(assert (= ((_ extract 0 0) #x94d049bb133111eb) #b0))
(check-sat)
; >>> unsat ✓
