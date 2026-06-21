; Claim: The (word_index, bit_position) pair uniquely identifies each actor slot
; word_index = actor_id / 64
; bit_mask   = 1 << (actor_id % 64)
;
; For any two distinct actor IDs in [1, 1023] (KAIN_ACTOR_TABLE_SIZE = 1024,
; ID 0 is reserved as KAIN_ACTOR_ID_INVALID), either word_index differs or
; bit_position differs. This ensures occupancy words can't alias.
;
; Solver result: unsat — decomposition is bijective for all valid IDs
(set-logic QF_BV)
(declare-const id (_ BitVec 64))
(declare-const id2 (_ BitVec 64))

; Constrain to valid actor ID range [1, 1023] (0 = INVALID)
(assert (and (bvugt id (_ bv0 64)) (bvult id (_ bv1024 64))))
(assert (and (bvugt id2 (_ bv0 64)) (bvult id2 (_ bv1024 64))))
(assert (not (= id id2)))

; word_index = id / 64, bit_position = id % 64
(define-fun w ((i (_ BitVec 64))) (_ BitVec 64)
  (bvlshr i (_ bv6 64)))
(define-fun b ((i (_ BitVec 64))) (_ BitVec 64)
  (bvand i (_ bv63 64)))

; Both word index AND bit position must match for collision
(assert (and (= (w id) (w id2)) (= (b id) (b id2))))
(check-sat)
; unsat = no two distinct IDs share both word and bit position ✅
