; Proof: kain_memory_c11_success_strength(ordering) can be
; replaced with a 5-element lookup table + clamp.
;
; Claim: For all int64_t ordering values, the switch statement:
;   case RELAXED(0) -> 0, ACQUIRE(1) -> 2, RELEASE(2) -> 3,
;   ACQ_REL(3) -> 4, SEQ_CST(4) / default -> 5
;
; is equivalent to:
;   static const int8_t TABLE[] = {0, 2, 3, 4, 5};
;   return ordering >= 5 ? 5 : TABLE[ordering];
;
; Domain assumption: ordering is always one of {0, 1, 2, 3, 4}
; as these are the only KAIN_MEMORY_ORDER_* values defined.
; For safety the clamp handles values outside this range.
;
; Kain memory order enum:
;   RELAXED = 0, ACQUIRE = 1, RELEASE = 2, ACQ_REL = 3, SEQ_CST = 4

(set-logic QF_BV)
(declare-const ordering (_ BitVec 64))

; --- Reference: original switch ---
(define-fun switch_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv5 64)))))))  ; default: SEQ_CST

; --- Candidate: lookup table with clamp ---
; TABLE[0]=0, TABLE[1]=2, TABLE[2]=3, TABLE[3]=4, TABLE[4]=5
(define-fun lut_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (bvugt x (_ bv4 64)) (_ bv5 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
    (_ bv5 64)))))))

; --- Candidate: branchless arithmetic for domain {0..4} ---
; formula: ordering + (ordering != 0)
; For domain [0,4]: 0->0, 1->2, 2->3, 3->4, 4->5
(define-fun formula_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (bvugt x (_ bv4 64)) (_ bv5 64)
    (bvadd x (ite (= x (_ bv0 64)) (_ bv0 64) (_ bv1 64)))))

; Domain constraint: ordering is one of {0,1,2,3,4} or any other value
; (we clamp out-of-range in both implementations)

; Claim: LUT is equivalent to switch for ALL possible inputs
(assert (not (= (switch_success_strength ordering)
                (lut_success_strength ordering))))
(check-sat)
; Expected: unsat (equivalence proven)

(reset)

; Second claim: Formula is equivalent to switch for ALL possible inputs  
(set-logic QF_BV)
(declare-const ordering (_ BitVec 64))

(define-fun switch_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv3 64)
  (ite (= x (_ bv3 64)) (_ bv4 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv5 64)))))))

(define-fun formula_success_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (bvugt x (_ bv4 64)) (_ bv5 64)
    (bvadd x (ite (= x (_ bv0 64)) (_ bv0 64) (_ bv1 64)))))

(assert (not (= (switch_success_strength ordering)
                (formula_success_strength ordering))))
(check-sat)
; Expected: unsat (equivalence proven)

; --- Verification trace for domain {0..4} ---
(reset)
(set-logic QF_BV)
(define-fun formula ((x (_ BitVec 64))) (_ BitVec 64)
  (bvadd x (ite (= x (_ bv0 64)) (_ bv0 64) (_ bv1 64))))
(declare-const test (_ BitVec 64))
(assert (bvult test (_ bv5 64)))
; Prove formula gives correct results for domain {0,1,2,3,4}
(assert (not
  (and (= (formula (_ bv0 64)) (_ bv0 64))
       (= (formula (_ bv1 64)) (_ bv2 64))
       (= (formula (_ bv2 64)) (_ bv3 64))
       (= (formula (_ bv3 64)) (_ bv4 64))
       (= (formula (_ bv4 64)) (_ bv5 64)))))
(check-sat)
; Expected: unsat (all 5 values correct)
