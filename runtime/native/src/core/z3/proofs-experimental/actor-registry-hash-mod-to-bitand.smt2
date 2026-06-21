; Claim: hash % 256 == hash & 255 for all 32-bit unsigned hash values
; KAIN_ACTOR_REGISTRY_SIZE = 256 = 2^8, so modulo is equivalent to bitwise AND
; Target: X:/runtime/native/src/core/actor.c, line 3462
;   return hash % KAIN_ACTOR_REGISTRY_SIZE;
; Proposed: return hash & (KAIN_ACTOR_REGISTRY_SIZE - 1);
;
; Solver result: unsat — equivalence proven for all 2^32 hash values
(set-logic QF_BV)
(declare-const hash (_ BitVec 32))

; Registry size = 256 = 2^8, mask = 255 = 0x000000FF
(define-fun modulo ((x (_ BitVec 32))) (_ BitVec 32)
  (bvurem x (_ bv256 32)))
(define-fun bitwise ((x (_ BitVec 32))) (_ BitVec 32)
  (bvand x (_ bv255 32)))

; Claim they are equal for ALL hash values
(assert (not (= (modulo hash) (bitwise hash))))
(check-sat)
; unsat = equivalent for all inputs ✅
