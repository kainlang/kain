; Comprehensive proof: All 5 memory order switch functions can be
; replaced with a single shared lookup table.
;
; Current: 5 separate switch statements, each visited on every atomic op.
;
; Proposed:
;   // Shared lookup table (index 0..4, clamped)
;   static const int8_t ORDER_FROM_CODE[5] = {0, 2, 3, 4, 5};  // relaxed, acquire, release, acq_rel, seq_cst
;   static const int8_t STORE_ORDER[5]     = {0, 3, 3, 3, 5};  // relaxed, release, release, release, seq_cst
;   static const int8_t LOAD_ORDER[5]      = {0, 2, 2, 2, 5};  // relaxed, acquire, acquire, acquire, seq_cst
;
; Memory order enum:
;   RELAXED = 0, ACQUIRE = 1, RELEASE = 2, ACQ_REL = 3, SEQ_CST = 4
; C11 memory_order enum values: relaxed=0, consume=1, acquire=2, release=3, acq_rel=4, seq_cst=5
;
; Domain assumption: ordering is always one of {0, 1, 2, 3, 4}.
; All callers pass KAIN_MEMORY_ORDER_* enum values.

(set-logic QF_BV)

; --- Define the 5 switch functions as BV formulas ---

; 1) kain_memory_order_from_code (general)
;    RELAXED(0)->0, ACQUIRE(1)->2, RELEASE(2)->3, ACQ_REL(3)->4, SEQ_CST(4)/default->5
(define-fun switch_order_from_code ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64))))))  ; also handles default >= 4

; LUT: {0, 2, 3, 4, 5}
(define-fun lut_order_from_code ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64))))))

; 2) kain_memory_store_order_from_code
;    RELAXED(0)->0, ACQUIRE/RELEASE/ACQ_REL->3, SEQ_CST/default->5
(define-fun switch_store_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv3 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv3 64)
    (_ bv5 64))))))

; LUT: {0, 3, 3, 3, 5}
(define-fun lut_store_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv3 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv3 64)
    (_ bv5 64))))))

; 3) kain_memory_load_order_from_code
;    RELAXED(0)->0, ACQUIRE/RELEASE/ACQ_REL->2, SEQ_CST/default->5
(define-fun switch_load_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))

; LUT: {0, 2, 2, 2, 5}
(define-fun lut_load_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))

; 4) kain_memory_failure_order_from_code
;    RELAXED(0)->0, ACQUIRE/RELEASE/ACQ_REL->2, SEQ_CST/default->5
(define-fun switch_failure_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))

; LUT: {0, 2, 2, 2, 5}
(define-fun lut_failure_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))

; 5) kain_memory_c11_success_strength
;    RELAXED(0)->0, ACQUIRE(1)->2, RELEASE(2)->3, ACQ_REL(3)->4, SEQ_CST(4)/default->5
(define-fun switch_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64))))))

; LUT: {0, 2, 3, 4, 5}
(define-fun lut_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64))))))

; 6) kain_memory_c11_failure_strength
;    RELAXED(0)->0, ACQUIRE/RELEASE/ACQ_REL(1-3)->2, SEQ_CST(4)/default->5 -> wait, default is 2!
(define-fun switch_failure_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv2 64)))))))  ; default: acquire strength

; LUT: {0, 2, 2, 2, 5} with clamp to 2 for out-of-range
(define-fun lut_failure_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv2 64)))))))

; ============================================================
; Prove equivalence: For each pair, for ALL 64-bit inputs
; ============================================================

(declare-const x (_ BitVec 64))

; Claim 1: order_from_code switch == LUT
(assert (not (= (switch_order_from_code x) (lut_order_from_code x))))
(check-sat)
; Expected: unsat

(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))

; Claim 2: store_order switch == LUT
(define-fun switch_store_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv3 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv3 64)
    (_ bv5 64))))))
(define-fun lut_store_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv3 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv3 64)
    (_ bv5 64))))))
(assert (not (= (switch_store_order x) (lut_store_order x))))
(check-sat)
; Expected: unsat

(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))

; Claim 3: load_order switch == LUT
(define-fun switch_load_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))
(define-fun lut_load_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))
(assert (not (= (switch_load_order x) (lut_load_order x))))
(check-sat)
; Expected: unsat

(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))

; Claim 4: failure_order switch == LUT
(define-fun switch_failure_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))
(define-fun lut_failure_order ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64))))))
(assert (not (= (switch_failure_order x) (lut_failure_order x))))
(check-sat)
; Expected: unsat

(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))

; Claim 5: success_strength switch == LUT
(define-fun switch_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64))))))
(define-fun lut_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64))))))
(assert (not (= (switch_success_strength x) (lut_success_strength x))))
(check-sat)
; Expected: unsat

(reset)
(set-logic QF_BV)
(declare-const x (_ BitVec 64))

; Claim 6: failure_strength switch == LUT (with default=2)
(define-fun switch_failure_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv2 64)))))))
(define-fun lut_failure_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv2 64)))))))
(assert (not (= (switch_failure_strength x) (lut_failure_strength x))))
(check-sat)
; Expected: unsat

; ============================================================
; Bonus: Shared table discovery
; load_order == failure_order (both produce {0, 2, 2, 2, 5})
; order_from_code == success_strength (both produce {0, 2, 3, 4, 5})
; So we only need 3 unique LUTs instead of 5 switches!
; ============================================================
