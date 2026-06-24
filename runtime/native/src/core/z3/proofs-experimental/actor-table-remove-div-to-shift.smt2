; Proof: actor_id / KAIN_ACTOR_TABLE_WORD_BITS == actor_id >> 6
;
; When KAIN_ACTOR_TABLE_WORD_BITS = 64 = 2^6, integer division by 64
; is equivalent to a logical right shift by 6 bits.
;
; Domain: KAIN_ACTOR_TABLE_SIZE = 1024, so actor_id in [0, 1023]
;
; Also proves: actor_id & (64-1) == actor_id % 64
;
; This already has a proof for the % case:
;   X:/runtime/native/src/core/z3/proofs-experimental/actor-occupancy-shift-mod-to-bitand.smt2
; This extends it to the / case.

(set-logic QF_BV)

(declare-const actor_id (_ BitVec 32))

; actor_id is in [0, 1023] for the actor table
(assert (bvult actor_id (_ bv1024 32)))

; Proposition 1: actor_id / 64 == actor_id >> 6
(define-fun div_result () (_ BitVec 32) (bvudiv actor_id (_ bv64 32)))
(define-fun shift_result () (_ BitVec 32) (bvlshr actor_id (_ bv6 32)))
(define-fun prop1 () (_ BitVec 1) (ite (= div_result shift_result) (_ bv1 1) (_ bv0 1)))

; Proposition 2: actor_id % 64 == actor_id & 63
(define-fun mod_result () (_ BitVec 32) (bvurem actor_id (_ bv64 32)))
(define-fun and_result () (_ BitVec 32) (bvand actor_id (_ bv63 32)))
(define-fun prop2 () (_ BitVec 1) (ite (= mod_result and_result) (_ bv1 1) (_ bv0 1)))

; Assert both are true
(assert (not (= prop1 (_ bv1 1))))
(check-sat)
; unsat = division-to-shift is safe for all table IDs

(reset)
(set-logic QF_BV)
(declare-const actor_id (_ BitVec 32))
(assert (bvult actor_id (_ bv1024 32)))
(assert (not (= (bvurem actor_id (_ bv64 32)) (bvand actor_id (_ bv63 32)))))
(check-sat)
; unsat = modulo-to-and is safe for all table IDs
