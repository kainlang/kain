; Proof: Pointer index hash probe always stays within array bounds
;
; The pointer index table uses open-addressing with linear probing:
;   index = hash(ptr) & INDEX_MASK
;   probe loop: candidate_index = (index + probe) & INDEX_MASK
;
; Since INDEX_CAPACITY = 8192 = 2^13 and INDEX_MASK = INDEX_CAPACITY - 1 = 8191,
; the bitwise AND with INDEX_MASK guarantees each candidate is in [0, 8191].
;
; This proves that:
;   1. (index + probe) & INDEX_MASK < INDEX_CAPACITY for all probe values
;   2. (index + probe) & INDEX_MASK is always a valid array index
;   3. The mask approach is equivalent to modulo for power-of-two capacities

(set-logic QF_BV)

(declare-const index (_ BitVec 32))
(declare-const probe (_ BitVec 32))
(declare-const index_capacity (_ BitVec 32))
(declare-const index_mask (_ BitVec 32))

; Constants
(assert (= index_capacity (_ bv8192 32)))
(assert (= index_mask (_ bv8191 32)))

; index is in [0, 8191] (result of hash & mask)
(assert (bvule index index_mask))
; probe is in [0, 8191] (limited to number of probes)
(assert (bvule probe index_mask))

(define-fun candidate_index () (_ BitVec 32)
  (bvand (bvadd index probe) index_mask))

; Claim 1: candidate_index < index_capacity
(assert (not (bvult candidate_index index_capacity)))
(check-sat)

(reset)

; ============================================================
; Claim 2: candidate_index is always a valid 13-bit index
; (candidate_index & ~INDEX_MASK) == 0, i.e., top 19 bits are clear
; ============================================================
(set-logic QF_BV)
(declare-const index (_ BitVec 32))
(declare-const probe (_ BitVec 32))
(declare-const index_capacity (_ BitVec 32))
(declare-const index_mask (_ BitVec 32))

(assert (= index_capacity (_ bv8192 32)))
(assert (= index_mask (_ bv8191 32)))
(assert (bvule index index_mask))
(assert (bvule probe index_mask))

(define-fun candidate_index () (_ BitVec 32)
  (bvand (bvadd index probe) index_mask))

; If top 19 bits are clear, then candidate_index & ~INDEX_MASK == 0
; where ~INDEX_MASK is the bitwise NOT of 8191 (i.e., 0xFFFFE000)
(assert (not (= (bvand candidate_index (bvnot index_mask)) (_ bv0 32))))
(check-sat)

(reset)

; ============================================================
; Claim 3: (index + probe) & mask == (index + probe) % capacity
; for power-of-two capacity
; ============================================================
(set-logic QF_BV)
(declare-const index (_ BitVec 32))
(declare-const probe (_ BitVec 32))
(declare-const capacity (_ BitVec 32))

; capacity = 8192 = 2^13
(assert (= capacity (_ bv8192 32)))
; Power-of-two invariant: capacity & (capacity - 1) == 0
(assert (= (bvand capacity (bvsub capacity (_ bv1 32))) (_ bv0 32)))
; index and probe are within [0, capacity-1]
(assert (bvult index capacity))
(assert (bvult probe capacity))
; But sum can exceed capacity, so test correct
(define-fun sum () (_ BitVec 32) (bvadd index probe))
(define-fun mask_result () (_ BitVec 32) (bvand sum (bvsub capacity (_ bv1 32))))
(define-fun mod_result () (_ BitVec 32) (bvurem sum capacity))

(assert (not (= mask_result mod_result)))
(check-sat)
