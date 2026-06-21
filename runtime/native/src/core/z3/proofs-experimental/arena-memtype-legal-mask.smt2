; Proof: KAIN_MEMTYPE_LEGAL_MASK correctly identifies all legal
; memory type combinations, and kain_memtype_is_legal is
; equivalent to a simple bitmask test.
;
; Current code:
;   int kain_memtype_is_legal(uint8_t memtype) {
;     if (memtype >= KAIN_MEMTYPE_COUNT) return 0;
;     return ((KAIN_MEMTYPE_LEGAL_MASK >> memtype) & 1u) != 0;
;   }
;
; This is already branchless and optimal for the memtype -> bit test.
;
; The legal mask combines these bit positions:
;   KAIN_MEMTYPE_DEFAULT = 0x4
;   KAIN_MEMTYPE_CPU_RO | KAIN_MEMTYPE_CPU_WB = 0x8 | 0x4 = 0xC
;   KAIN_MEMTYPE_CPU_WB | KAIN_MEMTYPE_GPU_RO = 0x4 | 0x2 = 0x6
;   KAIN_MEMTYPE_CPU_WB | KAIN_MEMTYPE_GPU_LOCAL = 0x4 | 0x1 = 0x5
;   KAIN_MEMTYPE_DEFAULT_GPU_RO = 0xF
;   KAIN_MEMTYPE_DEFAULT_GPU_RW = 0x1
;
; Proof verifies:
;   1. The mask only accepts memtype values < KAIN_MEMTYPE_COUNT (16)
;   2. Each legal combination maps to a unique bit position
;   3. No illegal memtype values pass the test

(set-logic QF_BV)

; Define constants
(define-fun KAIN_MEMTYPE_COUNT () (_ BitVec 8) (_ bv16 8))
(define-fun CPU_RO () (_ BitVec 8) (_ bv8 8))
(define-fun CPU_WB () (_ BitVec 8) (_ bv4 8))
(define-fun GPU_RO () (_ BitVec 8) (_ bv2 8))
(define-fun GPU_LOCAL () (_ BitVec 8) (_ bv1 8))

; Legal mask bits
(define-fun LEGAL_MASK () (_ BitVec 16)
  (let ((m (_ bv0 16)))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) CPU_WB)))))       ; bit 4
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) (bvor CPU_RO CPU_WB)))))) ; bit 12
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) (bvor CPU_WB GPU_RO)))))) ; bit 6
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) (bvor CPU_WB GPU_LOCAL)))))) ; bit 5
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) (bvor CPU_RO (bvor CPU_WB (bvor GPU_RO GPU_LOCAL)))))))) ; bit 15
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) GPU_LOCAL))))))))) ; bit 1
  m))

; Function: is_legal(memtype) = ((LEGAL_MASK >> memtype) & 1) != 0
(define-fun is_legal ((mt (_ BitVec 8))) Bool
  (and (bvult mt KAIN_MEMTYPE_COUNT)
       (= ((_ extract 0 0) (bvlshr LEGAL_MASK ((_ zero_extend 8) mt))) (_ bv1 1))))

(declare-const mt (_ BitVec 8))

; Claim 1: For the 6 legal values, is_legal returns true
(assert (not
  (and (is_legal CPU_WB)                                    ; 4
       (is_legal (bvor CPU_RO CPU_WB))                      ; 12
       (is_legal (bvor CPU_WB GPU_RO))                      ; 6
       (is_legal (bvor CPU_WB GPU_LOCAL))                    ; 5
       (is_legal (bvor CPU_RO (bvor CPU_WB (bvor GPU_RO GPU_LOCAL))))  ; 15
       (is_legal GPU_LOCAL))))                               ; 1
(check-sat)
; Expected: unsat (all 6 legal values pass)

(reset)

; ============================================================
; Claim 2: For all OTHER memtype values < 16, is_legal returns false
; ============================================================
(set-logic QF_BV)
(define-fun KAIN_MEMTYPE_COUNT () (_ BitVec 8) (_ bv16 8))
(define-fun CPU_RO () (_ BitVec 8) (_ bv8 8))
(define-fun CPU_WB () (_ BitVec 8) (_ bv4 8))
(define-fun GPU_RO () (_ BitVec 8) (_ bv2 8))
(define-fun GPU_LOCAL () (_ BitVec 8) (_ bv1 8))

; The set of legal values
(define-fun legal1 () (_ BitVec 8) CPU_WB)                                    ; 4
(define-fun legal2 () (_ BitVec 8) (bvor CPU_RO CPU_WB))                      ; 12
(define-fun legal3 () (_ BitVec 8) (bvor CPU_WB GPU_RO))                      ; 6
(define-fun legal4 () (_ BitVec 8) (bvor CPU_WB GPU_LOCAL))                    ; 5
(define-fun legal5 () (_ BitVec 8) (bvor CPU_RO (bvor CPU_WB (bvor GPU_RO GPU_LOCAL))))  ; 15
(define-fun legal6 () (_ BitVec 8) GPU_LOCAL)                                 ; 1

(define-fun LEGAL_MASK () (_ BitVec 16)
  (let ((m (_ bv0 16)))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal1)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal2)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal3)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal4)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal5)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal6)))))
  m)))))))

(define-fun is_legal ((mt (_ BitVec 8))) Bool
  (and (bvult mt KAIN_MEMTYPE_COUNT)
       (= ((_ extract 0 0) (bvlshr LEGAL_MASK ((_ zero_extend 8) mt))) (_ bv1 1))))

(declare-const mt (_ BitVec 8))

; Assert: mt is NOT one of the 6 legal values
(assert (bvult mt KAIN_MEMTYPE_COUNT))
(assert (not (= mt legal1)))
(assert (not (= mt legal2)))
(assert (not (= mt legal3)))
(assert (not (= mt legal4)))
(assert (not (= mt legal5)))
(assert (not (= mt legal6)))

; Claim: is_legal returns false
(assert (is_legal mt))
(check-sat)
; Expected: unsat (no illegal value passes the test)

(reset)

; ============================================================
; Claim 3: The bitwise test is equivalent to a series of
; equality checks (verifying no branch is needed)
; ============================================================
(set-logic QF_BV)
(define-fun KAIN_MEMTYPE_COUNT () (_ BitVec 8) (_ bv16 8))
(define-fun CPU_RO () (_ BitVec 8) (_ bv8 8))
(define-fun CPU_WB () (_ BitVec 8) (_ bv4 8))
(define-fun GPU_RO () (_ BitVec 8) (_ bv2 8))
(define-fun GPU_LOCAL () (_ BitVec 8) (_ bv1 8))
(define-fun legal1 () (_ BitVec 8) CPU_WB)
(define-fun legal2 () (_ BitVec 8) (bvor CPU_RO CPU_WB))
(define-fun legal3 () (_ BitVec 8) (bvor CPU_WB GPU_RO))
(define-fun legal4 () (_ BitVec 8) (bvor CPU_WB GPU_LOCAL))
(define-fun legal5 () (_ BitVec 8) (bvor CPU_RO (bvor CPU_WB (bvor GPU_RO GPU_LOCAL))))
(define-fun legal6 () (_ BitVec 8) GPU_LOCAL)

(define-fun LEGAL_MASK () (_ BitVec 16)
  (let ((m (_ bv0 16)))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal1)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal2)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal3)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal4)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal5)))))
  (let ((m (bvadd m (bvshl (_ bv1 16) ((_ zero_extend 8) legal6)))))
  m)))))))

; Reference: branchless bit test
(define-fun bit_test ((mt (_ BitVec 8))) Bool
  (and (bvult mt KAIN_MEMTYPE_COUNT)
       (= ((_ extract 0 0) (bvlshr LEGAL_MASK ((_ zero_extend 8) mt))) (_ bv1 1))))

; Switch-style reference (for comparison)
(define-fun switch_test ((mt (_ BitVec 8))) Bool
  (or (= mt legal1) (= mt legal2) (= mt legal3)
      (= mt legal4) (= mt legal5) (= mt legal6)))

(declare-const mt (_ BitVec 8))

; Prove: bit_test == switch_test for ALL valid memtype values
(assert (bvult mt KAIN_MEMTYPE_COUNT))
(assert (not (= (bit_test mt) (switch_test mt))))
(check-sat)
; Expected: unsat (both are equivalent for domain [0, 15])
