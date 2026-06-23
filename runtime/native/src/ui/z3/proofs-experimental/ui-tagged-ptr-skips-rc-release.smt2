; ─────────────────────────────────────────────────────────────
;  Claim: Tagged pointer with low bit 1 always has (ptr & 7) != 0
;  → rc_release / rc_retain are no-ops (heap_owned_i8_guard skips)
; ─────────────────────────────────────────────────────────────
;  The LLVM codegen's heap_owned_i8_guard generates:
;    low_bits = ptrtoint i8* val to i64
;    low_bits = and i64 low_bits, 7
;    should_call = icmp eq i64 low_bits, 0
;    br i1 should_call, call @rc_release, skip
;
;  If we tag our return pointer with bit 0 (value 1),
;  then low_bits = (ptr | 1) & 7 = 1 (since ptr is aligned,
;  its low 3 bits are 0, so OR-ing 1 gives 1).
;  → should_call is false → rc_release skipped.
; ─────────────────────────────────────────────────────────────

(set-logic QF_BV)

; ── Parameters ──────────────────────────────────────────────
(define-const PTR_TAG (_ BitVec 64) #x0000000000000001)  ; bit 0 = arena/static tag
(define-const RC_MASK (_ BitVec 64) #x0000000000000007)  ; low 3 bits

; ── Quantified proof ───────────────────────────────────────
; Claim: For any 64-bit pointer with low 3 bits = 0 (aligned),
;        (ptr | 1) & 7 != 0  [always true, no counterexample]

(declare-const ptr (_ BitVec 64))

; Precondition: ptr is 8-byte aligned (heap pointers always are)
(assert (= ((_ extract 2 0) ptr) #b000))

; Tag the pointer
(define-const tagged (_ BitVec 64) (bvor ptr PTR_TAG))

; Extract low 3 bits after tagging
(define-const low_bits (_ BitVec 64) (bvand tagged RC_MASK))

; Claim: low_bits == 0 → this would make rc_release proceed on a non-heap ptr
(assert (= low_bits #x0000000000000000))

(check-sat)
; Expected: UNSAT — no aligned pointer when OR'd with 1 can have low 3 bits = 0
; ─────────────────────────────────────────────────────────────
