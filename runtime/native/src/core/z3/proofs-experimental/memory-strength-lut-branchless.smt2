; Proof: kain_memory_c11_success_strength branch removal
; and kain_memory_c11_failure_strength branch removal.
;
; Current code:
;   static int kain_memory_c11_success_strength(int64_t ordering) {
;       return ordering >= 5 ? 5 : KAIN_MEMORY_SUCCESS_STRENGTH_LUT[(size_t)ordering];
;   }
;   static int kain_memory_c11_failure_strength(int64_t ordering) {
;       return ordering >= 5 ? 2 : KAIN_MEMORY_FAILURE_STRENGTH_LUT[(size_t)ordering];
;   }
;
; Where:
;   KAIN_MEMORY_SUCCESS_STRENGTH_LUT[5] = {0, 2, 3, 4, 5};
;   KAIN_MEMORY_FAILURE_STRENGTH_LUT[5] = {0, 2, 2, 2, 5};
;
; With extended LUT (6 entries, entry 5 = clamp value):
;   static const int KAIN_MEMORY_SUCCESS_STRENGTH_LUT_EXT[6] = {0, 2, 3, 4, 5, 5};
;   static const int KAIN_MEMORY_FAILURE_STRENGTH_LUT_EXT[6] = {0, 2, 2, 2, 5, 2};
;
; Branchless (using clamp on index):
;   static int kain_memory_c11_success_strength(int64_t ordering) {
;       return KAIN_MEMORY_SUCCESS_STRENGTH_LUT_EXT[
;           (size_t)(ordering < 6 ? ordering : 5)];
;   }
;   static int kain_memory_c11_failure_strength(int64_t ordering) {
;       return KAIN_MEMORY_FAILURE_STRENGTH_LUT_EXT[
;           (size_t)(ordering < 6 ? ordering : 5)];
;   }
;
; Domain: ordering is int64_t in practice from Kain ABI (0..4),
; but the guard handles values outside the expected range.

(set-logic QF_BV)
(declare-const ordering (_ BitVec 64))

; ================================================================
; Success strength
; ================================================================

; Original
(define-fun orig_success ((o (_ BitVec 64))) (_ BitVec 64)
  (ite (bvuge o (_ bv5 64))
       (_ bv5 64)
       (ite (= o (_ bv0 64)) (_ bv0 64)
       (ite (= o (_ bv1 64)) (_ bv2 64)
       (ite (= o (_ bv2 64)) (_ bv3 64)
       (ite (= o (_ bv3 64)) (_ bv4 64)
       (ite (= o (_ bv4 64)) (_ bv5 64)
            (_ bv5 64))))))))

; Extended LUT: map index directly, clamp >= 6 to 5
(define-fun ext_success ((o (_ BitVec 64))) (_ BitVec 64)
  (let ((idx (ite (bvuge o (_ bv6 64)) (_ bv5 64) o)))
  (ite (= idx (_ bv0 64)) (_ bv0 64)
  (ite (= idx (_ bv1 64)) (_ bv2 64)
  (ite (= idx (_ bv2 64)) (_ bv3 64)
  (ite (= idx (_ bv3 64)) (_ bv4 64)
  (ite (= idx (_ bv4 64)) (_ bv5 64)
       (_ bv5 64))))))))

(assert (not (= (orig_success ordering) (ext_success ordering))))
(check-sat)
; Expected: unsat (equivalent for all 64-bit ordering values)

(reset)

; ================================================================
; Failure strength
; ================================================================
(set-logic QF_BV)
(declare-const ordering (_ BitVec 64))

; Original
(define-fun orig_failure ((o (_ BitVec 64))) (_ BitVec 64)
  (ite (bvuge o (_ bv5 64))
       (_ bv2 64)
       (ite (= o (_ bv0 64)) (_ bv0 64)
       (ite (= o (_ bv1 64)) (_ bv2 64)
       (ite (= o (_ bv2 64)) (_ bv2 64)
       (ite (= o (_ bv3 64)) (_ bv2 64)
       (ite (= o (_ bv4 64)) (_ bv5 64)
            (_ bv2 64))))))))

; Extended LUT
(define-fun ext_failure ((o (_ BitVec 64))) (_ BitVec 64)
  (let ((idx (ite (bvuge o (_ bv6 64)) (_ bv5 64) o)))
  (ite (= idx (_ bv0 64)) (_ bv0 64)
  (ite (= idx (_ bv1 64)) (_ bv2 64)
  (ite (= idx (_ bv2 64)) (_ bv2 64)
  (ite (= idx (_ bv3 64)) (_ bv2 64)
  (ite (= idx (_ bv4 64)) (_ bv5 64)
       (_ bv2 64))))))))

(assert (not (= (orig_failure ordering) (ext_failure ordering))))
(check-sat)
; Expected: unsat (equivalent for all 64-bit ordering values)
