; Proof: status_for_busy_region branch ladder → observer-check + table lookup
; Domain invariant: observers > 0 ⇒ state == OBSERVED
; (enforced by begin_observe/end_observe state machine transitions)
;
; Reference original:
;   if (state == DECAYED)   return ERR_DECAYED;
;   if (state == SHARED)    return ERR_COLLAPSED;
;   if (state == COLLAPSED) return ERR_COLLAPSED;
;   if (state == OBSERVED || observers != 0) return ERR_OBSERVED;
;   return ERR_INVALID;
;
; Candidate:
;   if (observers != 0) return ERR_OBSERVED;  // under invariant, state must be OBSERVED
;   static const int TABLE[] = {ERR_INVALID, ERR_OBSERVED, ERR_COLLAPSED,
;                                ERR_COLLAPSED, ERR_DECAYED};
;   if (state <= 4) return TABLE[state];
;   return ERR_INVALID;
;
(set-logic QF_BV)
(declare-const state (_ BitVec 32))
(declare-const o (_ BitVec 32))
(assert (bvule state (_ bv4 32)))
(assert (bvule o (_ bv1000 32)))
; Domain invariant
(assert (=> (not (= o (_ bv0 32))) (= state (_ bv1 32))))

; Reference: original branch ladder
(define-fun ref () (_ BitVec 32)
  (ite (= state (_ bv4 32)) (_ bv4294967290 32)
  (ite (= state (_ bv3 32)) (_ bv4294967291 32)
  (ite (= state (_ bv2 32)) (_ bv4294967291 32)
  (ite (or (= state (_ bv1 32)) (not (= o (_ bv0 32)))) (_ bv4294967292 32)
  (_ bv4294967295 32))))))

; Candidate: observer check then 5-element table lookup
(define-fun cand () (_ BitVec 32)
  (ite (not (= o (_ bv0 32))) (_ bv4294967292 32)
  (ite (= state (_ bv0 32)) (_ bv4294967295 32)
  (ite (= state (_ bv1 32)) (_ bv4294967292 32)
  (ite (= state (_ bv2 32)) (_ bv4294967291 32)
  (ite (= state (_ bv3 32)) (_ bv4294967291 32)
  (ite (= state (_ bv4 32)) (_ bv4294967290 32)
  (_ bv4294967295 32))))))))

(assert (not (= ref cand)))
(check-sat)
