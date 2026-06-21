; Proof: kain_memory_c11_failure_strength(ordering) can be
; replaced with a lookup table.
;
; Claim: For all int64_t ordering values, the switch:
;   case RELAXED(0) -> 0, ACQUIRE(1) -> 2, RELEASE(2) -> 2,
;   ACQ_REL(3) -> 2, SEQ_CST(4) -> 5, default -> 2
;
; is equivalent to:
;   static const int8_t TABLE[] = {0, 2, 2, 2, 5};
;   return ordering >= 5 ? 2 : TABLE[ordering];
;
; C11 constraint: failure ordering must not be stronger than success ordering.
; This function returns the numeric strength for comparison in the clamp logic.
;
; Kain memory order enum:
;   RELAXED = 0, ACQUIRE = 1, RELEASE = 2, ACQ_REL = 3, SEQ_CST = 4

(set-logic QF_BV)
(declare-const ordering (_ BitVec 64))

; --- Reference: original switch ---
(define-fun switch_failure_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
  (ite (= x (_ bv4 64)) (_ bv5 64)
    (_ bv2 64)))))))  ; default: ACQUIRE strength

; --- Candidate: LUT with clamp ---
; TABLE[0]=0, TABLE[1]=2, TABLE[2]=2, TABLE[3]=2, TABLE[4]=5
(define-fun lut_failure_strength ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (bvugt x (_ bv4 64)) (_ bv2 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64)))))))

; Domain constraint: ordering can be any int64_t
; Both implementations clamp to {0..4} then map to strength

; Claim: LUT is equivalent to switch for ALL possible inputs
(assert (not (= (switch_failure_strength ordering)
                (lut_failure_strength ordering))))
(check-sat)
; Expected: unsat (equivalence proven)

; --- Compact implementation verification ---
(reset)
(set-logic QF_BV)

; The actual TABLE-based implementation would be:
; return (uint64_t)ordering > 4 ? 2 : (int[]){0,2,2,2,5}[ordering];
(define-fun compact_lut ((x (_ BitVec 64))) (_ BitVec 64)
  (ite (bvugt x (_ bv4 64)) (_ bv2 64)
  (ite (= x (_ bv0 64)) (_ bv0 64)
  (ite (= x (_ bv1 64)) (_ bv2 64)
  (ite (= x (_ bv2 64)) (_ bv2 64)
  (ite (= x (_ bv3 64)) (_ bv2 64)
    (_ bv5 64)))))))

(declare-const t (_ BitVec 64))

; Check specific values for the domain
(assert (not
  (and (= (compact_lut (_ bv0 64)) (_ bv0 64))
       (= (compact_lut (_ bv1 64)) (_ bv2 64))
       (= (compact_lut (_ bv2 64)) (_ bv2 64))
       (= (compact_lut (_ bv3 64)) (_ bv2 64))
       (= (compact_lut (_ bv4 64)) (_ bv5 64))
       (= (compact_lut (_ bv5 64)) (_ bv2 64))
       (= (compact_lut (_ bv999 64)) (_ bv2 64)))))
(check-sat)
; Expected: unsat (all values correct)
