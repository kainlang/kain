; converge-telemetry-ring-bounds.smt2
;
; Claim: The telemetry ring buffer index computed as `cursor & (CAP - 1)` is
; always in [0, CAP-1] for all 64-bit cursor values, where CAP = 64.
;
; Also prove that the mask-based indexing is equivalent to modulo for all
; cursor values (not just non-negative ones), since cursor is uint64_t.

(set-logic QF_BV)

(define-const CAPACITY (_ BitVec 64) #x0000000000000040)  ; 64
(define-const MASK (_ BitVec 64) #x000000000000003F)      ; CAP - 1 = 63

; ── Claim 1: ring_index is always < CAPACITY ──
(declare-const cursor (_ BitVec 64))
(define-fun ring_index () (_ BitVec 64) (bvand cursor MASK))

(push)
(assert (not (bvult ring_index CAPACITY)))
(check-sat)
(pop)
; unsat = ring_index always < 64 ✅

; ── Claim 2: bvand cursor 63 == bvurem cursor 64 for all uint64_t ──
; Since CAPACITY is a power of two (64 = 2^6), AND with (CAP-1) is equivalent
; to modulo CAP for unsigned bitvectors. This is a well-known identity but we
; prove it formally.
(push)
(assert (not (= (bvand cursor MASK) (bvurem cursor CAPACITY))))
(check-sat)
(pop)
; unsat = AND-mask == modulo for all cursor ✅

; ── Claim 3: Even after billions of records, the ring buffer stays bounded ──
; After 2^64 - 1 records, cursor wraps to 0 (uint64_t overflow in the
; atomic_fetch_add). The ring_index computation still produces valid [0,63].
(define-const MAX_U64 (_ BitVec 64) #xFFFFFFFFFFFFFFFF)
(define-fun ring_at_max () (_ BitVec 64) (bvand MAX_U64 MASK))
(push)
(assert (not (bvult ring_at_max CAPACITY)))
(check-sat)
(pop)
; unsat = even at max cursor value, ring index < 64 ✅
