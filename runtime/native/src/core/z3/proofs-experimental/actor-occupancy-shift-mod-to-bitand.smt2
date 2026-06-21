; Claim: 1ULL << (actor_id % 64) == 1ULL << (actor_id & 63)
; Since KAIN_ACTOR_TABLE_WORD_BITS = 64 = 2^6, the modulo is a power-of-two
; and can be replaced with bitwise AND.
;
; Used in actor_table_remove (line 1916):
;   bit_mask = 1ULL << (unsigned int)(actor_id % KAIN_ACTOR_TABLE_WORD_BITS);
;
; Proposed replacement:
;   bit_mask = 1ULL << (unsigned int)(actor_id & (KAIN_ACTOR_TABLE_WORD_BITS - 1));
;
; Solver result: unsat — equivalent for all 64-bit actor_id values
(set-logic QF_BV)
(declare-const actor_id (_ BitVec 64))

; KAIN_ACTOR_TABLE_WORD_BITS = 64
(define-fun shift_mod ((id (_ BitVec 64))) (_ BitVec 64)
  (bvshl (_ bv1 64) (bvurem id (_ bv64 64))))
(define-fun shift_and ((id (_ BitVec 64))) (_ BitVec 64)
  (bvshl (_ bv1 64) (bvand id (_ bv63 64))))

(assert (not (= (shift_mod actor_id) (shift_and actor_id))))
(check-sat)
; unsat = equivalent for all inputs ✅
