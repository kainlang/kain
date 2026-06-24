; Proof: branchless kain_alloc_cache_small_eligible
;
; Current code (short-circuit &&):
;   return (flags & KAIN_ALLOC_HEADER_FLAG_VIRTUAL) == 0u &&
;       payload_size >= sizeof(KainAllocHeader*) &&
;       payload_size <= KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD &&
;       (payload_size & (KAIN_ALLOC_CACHE_SMALL_QUANTUM - 1u)) == 0u;
;
; Branchless form (& instead of &&):
;   return ((flags & KAIN_ALLOC_HEADER_FLAG_VIRTUAL) == 0u)
;        & (payload_size >= sizeof(KainAllocHeader*))
;        & (payload_size <= KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD)
;        & ((payload_size & (KAIN_ALLOC_CACHE_SMALL_QUANTUM - 1u)) == 0u);
;
; Key insight: C comparisons return int 0 or 1. Bitwise AND of {0,1} values
; is equivalent to logical AND.
; Proof: for all a,b in {0,1}: a && b == a & b

(set-logic QF_BV)

(declare-const flags (_ BitVec 8))
(declare-const payload_size (_ BitVec 64))

(define-fun VIRTUAL_FLAG () (_ BitVec 8) (_ bv1 8))
(define-fun PTR_SIZE () (_ BitVec 64) (_ bv8 64))
(define-fun MAX_PAYLOAD () (_ BitVec 64) (_ bv8192 64))
(define-fun QUANTUM_MASK () (_ BitVec 64) (_ bv15 64))

; Compute each condition as 0 or 1
(define-fun c1 () (_ BitVec 64)
  (ite (= (bvand flags VIRTUAL_FLAG) (_ bv0 8)) (_ bv1 64) (_ bv0 64)))
(define-fun c2 () (_ BitVec 64)
  (ite (bvuge payload_size PTR_SIZE) (_ bv1 64) (_ bv0 64)))
(define-fun c3 () (_ BitVec 64)
  (ite (bvule payload_size MAX_PAYLOAD) (_ bv1 64) (_ bv0 64)))
(define-fun c4 () (_ BitVec 64)
  (ite (= (bvand payload_size QUANTUM_MASK) (_ bv0 64)) (_ bv1 64) (_ bv0 64)))

; Logical AND (short-circuit): returns 1 iff all four are 1
(define-fun short_circuit () (_ BitVec 64)
  (ite (and (= c1 (_ bv1 64)) (= c2 (_ bv1 64)) (= c3 (_ bv1 64)) (= c4 (_ bv1 64)))
       (_ bv1 64)
       (_ bv0 64)))

; Branchless: bitwise AND of all four 0/1 values
(define-fun branchless () (_ BitVec 64)
  (bvand (bvand c1 c2) (bvand c3 c4)))

; Prove equivalence
(assert (not (= short_circuit branchless)))
(check-sat)
; Expected: unsat (equivalent for all 8-bit flags * 64-bit payload_size)
