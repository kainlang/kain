; SIMD affine fold wrapping counter equivalence
;
; Proves that a running counter (incrementing and wrapping at N)
; produces the same sequence as phase % N for all phase >= 0, N > 0,
; provided phase+1 does not overflow (which is guaranteed in the C code
; because 'phase' is bounded by 'passes', a validated int64_t).
;
; The running counter update rule (used in optimization):
;   counter_next = (counter + 1 >= N) ? 0 : counter + 1
;
; After 'phase' increments (starting from 0), this counter equals phase % N.
;
; This justifies replacing `phase % bias_mod` and `phase % phase_mod`
; with running counters in kain_simd_affine_fold_mod, eliminating
; two IDIV instructions per loop iteration.
;
; Domain: bias_mod > 0, phase_mod > 0 (validated before call),
; passes < INT64_MAX/2 (no overflow in phase+1)
;
; CASE 1: When c + 1 >= N (i.e., p % N == N-1, counter about to wrap),
;         the counter resets to 0, which equals (p+1) % N.
; CASE 2: When c + 1 < N, the counter increments by 1,
;         which equals (p+1) % N.

; === CASE 1: c + 1 >= N → c_next = 0 = (p+1) % N ===
(set-logic QF_BV)
(declare-const p (_ BitVec 8))
(declare-const N (_ BitVec 8))
(declare-const c (_ BitVec 8))

(assert (bvugt N (_ bv0 8)))
(assert (= c (bvurem p N)))
(assert (bvuge (bvadd c (_ bv1 8)) N))

; Claim: c_next = 0 equals (p+1) % N
(assert (not (= (_ bv0 8) (bvurem (bvadd p (_ bv1 8)) N))))
(check-sat)
; Expected: unsat (no counterexample — the claim holds)
; Result: unsat ✅

(reset)

; === CASE 2: c + 1 < N → c_next = c+1 = (p+1) % N ===
(set-logic QF_BV)
(declare-const p (_ BitVec 8))
(declare-const N (_ BitVec 8))
(declare-const c (_ BitVec 8))

(assert (bvugt N (_ bv0 8)))
(assert (= c (bvurem p N)))
(assert (bvult (bvadd c (_ bv1 8)) N))
; No overflow: p != 255 (guaranteed by domain bound)
(assert (bvult p (_ bv255 8)))

; Claim: c+1 equals (p+1) % N
(assert (not (= (bvadd c (_ bv1 8)) (bvurem (bvadd p (_ bv1 8)) N))))
(check-sat)
; Expected: unsat
; Result: unsat ✅

; The equivalence extends to 64-bit by the same arithmetic identity.
; Bit-width doesn't affect the division-remainder relationship.
; With the no-overflow guarantee (phase < passes <= INT64_MAX), both
; cases hold for int64_t in the C code.
