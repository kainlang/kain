; Proof: KAIN_MEMTYPE_LEGAL_MASK correctly identifies all legal
; memory type combinations. The bitmask test is equivalent to
; an equality-match against 6 legal values, but branchless.
;
; Legal mask: 0x9072 = bits 1,4,5,6,12,15 set
;
; Legal values (bit position in mask):
;   1  = GPU_LOCAL = KAIN_MEMTYPE_DEFAULT_GPU_RW
;   4  = CPU_WB = KAIN_MEMTYPE_DEFAULT
;   5  = CPU_WB | GPU_LOCAL
;   6  = CPU_WB | GPU_RO
;   12 = CPU_RO | CPU_WB
;   15 = CPU_RO | CPU_WB | GPU_RO | GPU_LOCAL = KAIN_MEMTYPE_DEFAULT_GPU_RO

(set-logic QF_BV)
(define-fun LEGAL_MASK () (_ BitVec 16) (_ bv36978 16))  ; 0x9072
(define-fun KAIN_MEMTYPE_COUNT () (_ BitVec 8) (_ bv16 8))

(define-fun bit_test ((mt (_ BitVec 8))) Bool
  (and (bvult mt KAIN_MEMTYPE_COUNT)
       (= ((_ extract 0 0) (bvlshr LEGAL_MASK ((_ zero_extend 8) mt))) (_ bv1 1))))

; Claim 1: All 6 legal values pass
(assert (not
  (and (bit_test (_ bv4 8))   ; CPU_WB = 4
       (bit_test (_ bv12 8))  ; CPU_RO|CPU_WB = 12
       (bit_test (_ bv6 8))   ; CPU_WB|GPU_RO = 6
       (bit_test (_ bv5 8))   ; CPU_WB|GPU_LOCAL = 5
       (bit_test (_ bv15 8))  ; CPU_RO|CPU_WB|GPU_RO|GPU_LOCAL = 15
       (bit_test (_ bv1 8))   ; GPU_LOCAL = 1
  )))
(check-sat)
; Expected: unsat (all legal values accepted)

(reset)

; Claim 2: No illegal value < 16 passes
(set-logic QF_BV)
(define-fun LEGAL_MASK () (_ BitVec 16) (_ bv36978 16))
(define-fun KAIN_MEMTYPE_COUNT () (_ BitVec 8) (_ bv16 8))
(define-fun bit_test ((mt (_ BitVec 8))) Bool
  (and (bvult mt KAIN_MEMTYPE_COUNT)
       (= ((_ extract 0 0) (bvlshr LEGAL_MASK ((_ zero_extend 8) mt))) (_ bv1 1))))

(declare-const mt (_ BitVec 8))
(assert (bvult mt KAIN_MEMTYPE_COUNT))
; Exclude the 6 legal values
(assert (not (= mt (_ bv1 8))))
(assert (not (= mt (_ bv4 8))))
(assert (not (= mt (_ bv5 8))))
(assert (not (= mt (_ bv6 8))))
(assert (not (= mt (_ bv12 8))))
(assert (not (= mt (_ bv15 8))))
(assert (bit_test mt))
(check-sat)
; Expected: unsat (no illegal value passes)

(reset)

; Claim 3: bit_test is equivalent to switch-style match
(set-logic QF_BV)
(define-fun LEGAL_MASK () (_ BitVec 16) (_ bv36978 16))
(define-fun KAIN_MEMTYPE_COUNT () (_ BitVec 8) (_ bv16 8))

(define-fun bit_test ((mt (_ BitVec 8))) Bool
  (and (bvult mt KAIN_MEMTYPE_COUNT)
       (= ((_ extract 0 0) (bvlshr LEGAL_MASK ((_ zero_extend 8) mt))) (_ bv1 1))))

(define-fun switch_test ((mt (_ BitVec 8))) Bool
  (or (= mt (_ bv1 8))
      (= mt (_ bv4 8))
      (= mt (_ bv5 8))
      (= mt (_ bv6 8))
      (= mt (_ bv12 8))
      (= mt (_ bv15 8))))

(declare-const mt (_ BitVec 8))
(assert (bvult mt KAIN_MEMTYPE_COUNT))
(assert (not (= (bit_test mt) (switch_test mt))))
(check-sat)
; Expected: unsat (bit_test == switch_test for entire domain 0..15)
