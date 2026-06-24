; SIMD affine fold wrapping counter equivalence
;
; Proves that a running counter (incrementing and wrapping at N)
; produces the same sequence as phase % N for all phase >= 0, N > 0.
;
; The running counter update rule:
;   counter_next = (counter + 1 >= N) ? 0 : counter + 1
;
; After 'phase' increments (starting from 0), this counter equals phase % N.
;
; This justifies replacing `phase % bias_mod` and `phase % phase_mod`
; with running counters in kain_simd_affine_fold_mod, eliminating
; two IDIV instructions per loop iteration.
;
; Domain: bias_mod > 0, phase_mod > 0 (validated before call)

(set-logic QF_BV)

; -----------------------------------------------------------------------
; Induction step: prove that if counter = phase % N, then after one
; increment step: counter' = (phase+1) % N
; -----------------------------------------------------------------------

(declare-const counter (_ BitVec 64))
(declare-const phase (_ BitVec 64))
(declare-const N (_ BitVec 64))

; N > 0 (valid modulus)
(assert (bvugt N (_ bv0 64)))

; Precondition: counter == phase % N
(assert (= counter (bvurem phase N)))

; The running counter's next value:
; if counter + 1 >= N then 0 else counter + 1
(define-fun counter_next ((c (_ BitVec 64)) (n (_ BitVec 64))) (_ BitVec 64)
  (ite (bvuge (bvadd c (_ bv1 64)) n)
       (_ bv0 64)
       (bvadd c (_ bv1 64))))

; Expected: (phase + 1) % N
(define-fun phase_plus_one_mod_N () (_ BitVec 64)
  (bvurem (bvadd phase (_ bv1 64)) N))

; Claim: counter_next(counter, N) = (phase + 1) % N
(assert (not (= (counter_next counter N) phase_plus_one_mod_N)))

(check-sat)
