; Proof: Occupancy bitset composition with de Bruijn index
;
; The occupancy bitset uses 64 words of 64 bits each (total 4096 bits).
; The low-bit isolation + de Bruijn mapping finds the position of the
; lowest set bit in a word. Then the slot number is:
;   slot = word_index * 64 + bit_index
;
; This proof shows:
;   1. The low-bit isolation (v & -v) produces exactly one bit set
;      for any non-zero v.
;   2. The de Bruijn table maps each of the 64 possible one-hot values
;      to a unique index in [0, 63].
;   3. The composite (word_index * 64 + bit_index) produces a unique
;      slot for every possible (word_index, bit) pair.
;
(set-logic QF_BV)

; ---- Part 1: Low-bit isolation produces exactly one bit -------------
(define-fun isolate_low_bit ((v (_ BitVec 64))) (_ BitVec 64)
  (bvand v (bvneg v)))

(declare-const v (_ BitVec 64))
(assert (not (= v (_ bv0 64))))

; The result must be non-zero
(define-fun low_bit () (_ BitVec 64) (isolate_low_bit v))
(assert (= low_bit (_ bv0 64)))
(check-sat)
; If unsat: low_bit is always non-zero for non-zero v

(reset)
(set-logic QF_BV)
; The result must have exactly one bit set:
;   low_bit & (low_bit - 1) == 0
(declare-const v (_ BitVec 64))
(assert (not (= v (_ bv0 64))))
(define-fun low_bit () (_ BitVec 64) (bvand v (bvneg v)))
; Check that low_bit & (low_bit - 1) == 0 (i.e., exactly one bit)
(assert (not (= (bvand low_bit (bvsub low_bit (_ bv1 64))) (_ bv0 64))))
(check-sat)
; If unsat: low_bit always has exactly one bit set

; ---- Part 2: De Bruijn index is collision-free (64 distinct values) ---
(reset)
(set-logic QF_BV)
(define-fun debruijn_idx ((one_hot (_ BitVec 64))) (_ BitVec 6)
  ((_ extract 63 58) (bvmul one_hot #x03f79d71b4cb0a89)))

; Check that all 64 power-of-two values produce distinct indices
(declare-const i (_ BitVec 6))
(declare-const j (_ BitVec 6))
(assert (distinct i j))

(define-fun pi () (_ BitVec 64) (bvshl (_ bv1 64) ((_ zero_extend 58) i)))
(define-fun pj () (_ BitVec 64) (bvshl (_ bv1 64) ((_ zero_extend 58) j)))

(assert (= (debruijn_idx pi) (debruijn_idx pj)))
(check-sat)
; If unsat: de Bruijn indices are all distinct

; ---- Part 3: Slot composition is unique -------------------------------
(reset)
(set-logic QF_BV)
; Slot = word_index * 64 + bit_index
; Where word_index in [0, 63] and bit_index in [0, 63]
(declare-const wi0 (_ BitVec 6))
(declare-const bi0 (_ BitVec 6))
(declare-const wi1 (_ BitVec 6))
(declare-const bi1 (_ BitVec 6))

; Two different (word_index, bit_index) pairs produce distinct slots
(assert (not (and (= wi0 wi1) (= bi0 bi1))))
; Slot formula: (word_index << 6) | bit_index
; Since bit_index is 6 bits and word_index is 6 bits:
; slot = word_index * 64 + bit_index = (word_index << 6) + bit_index
(define-fun slot0 () (_ BitVec 12)
  (bvadd ((_ zero_extend 6) wi0) (bvshl ((_ zero_extend 6) wi0) (_ bv6 12))))
(define-fun slot1 () (_ BitVec 12)
  (bvadd ((_ zero_extend 6) wi1) (bvshl ((_ zero_extend 6) wi1) (_ bv6 12))))

; Wait, that's wrong. Let me fix:
; Actually: (wi << 6) | bi = wi * 64 + bi
; With wi as 6-bit in upper bits, bi in lower bits
(define-fun slot0_correct () (_ BitVec 12)
  (bvor (bvshl ((_ zero_extend 6) wi0) (_ bv6 12)) ((_ zero_extend 6) bi0)))
(define-fun slot1_correct () (_ BitVec 12)
  (bvor (bvshl ((_ zero_extend 6) wi1) (_ bv6 12)) ((_ zero_extend 6) bi1)))

(assert (= slot0_correct slot1_correct))
(check-sat)
; If unsat: each (word_index, bit_index) maps to a unique slot
