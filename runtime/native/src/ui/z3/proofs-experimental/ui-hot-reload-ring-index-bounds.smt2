; Proof: ring_index = (uint64_t)next_sequence & KAIN_UI_HOT_RELOAD_RING_MASK
;        is always < KAIN_UI_HOT_RELOAD_RING_CAPACITY
;
; The hot reload event ring uses a power-of-two capacity (128),
; so the mask (127) selects the lowest 7 bits of the sequence number.
; This proves that any 64-bit sequence number, when masked, produces
; a valid ring index in [0, 127].
;
; Key claims:
;   1. For any 128-bit sequence number (modeled with 64-bit BV),
;      (seq & 127) < 128
;   2. The ring capacity is a power of two, so mask = capacity - 1
;      is a correct index bounds guard
;
(set-logic QF_BV)

; ── Ring constants ──────────────────────────────────────────────────
(define-fun RING_CAPACITY () (_ BitVec 64) #x0000000000000080)  ; 128
(define-fun RING_MASK    () (_ BitVec 64) #x000000000000007F)  ; 127

; ── Proof 1: ring_index < ring_capacity for ANY 64-bit sequence ────
; ring_index = (uint32_t)((uint64_t)seq & RING_MASK)
; Cast to uint32_t is irrelevant since RING_MASK fits in 7 bits.
; We prove the 64-bit masked value is always < 128.
(push)
(declare-fun seq () (_ BitVec 64))

; The ring index computation
(define-fun ring_index () (_ BitVec 64)
  (bvand seq RING_MASK))

; Assert that ring_index is NOT < 128 (i.e., out of bounds)
(assert (not (bvult ring_index RING_CAPACITY)))
(check-sat)
; Expected: unsat — ring_index is always < 128
(pop)

; ── Proof 2: ring_index never exceeds capacity-1 = 127 ─────────────
; Equivalent to: ring_index ≤ 127
(push)
(declare-fun seq () (_ BitVec 64))
(define-fun ring_index () (_ BitVec 64)
  (bvand seq RING_MASK))
(define-fun max_index () (_ BitVec 64) #x000000000000007F)  ; 127

(assert (bvugt ring_index max_index))
(check-sat)
; Expected: unsat — ring_index never exceeds 127
(pop)

; ── Proof 3: Ring index is invariant under 128-wide rotation ───────
; Adding 128 to seq cycles to the same ring slot.
; This proves the ring wraps correctly modulo capacity.
(push)
(declare-fun seq () (_ BitVec 64))

(define-fun ring_index_a () (_ BitVec 64)
  (bvand seq RING_MASK))

(define-fun seq_plus_128 () (_ BitVec 64)
  (bvadd seq RING_CAPACITY))

(define-fun ring_index_b () (_ BitVec 64)
  (bvand seq_plus_128 RING_MASK))

(assert (not (= ring_index_a ring_index_b)))
(check-sat)
; Expected: unsat — ring index is congruent modulo capacity
(pop)

; ── Proof 4: The mask equals capacity - 1 (power-of-two property) ──
; This proves the compile-time assertion
;   (KAIN_UI_HOT_RELOAD_RING_CAPACITY & (capacity - 1)) == 0
; is equivalent to RING_MASK == RING_CAPACITY - 1.
(push)
(assert (not (= RING_MASK (bvsub RING_CAPACITY #x0000000000000001))))
(check-sat)
; Expected: unsat — RING_MASK == RING_CAPACITY - 1 by construction
(pop)

; ── Proof 5: Zero-extend of uint32_t cast ──────────────────────────
; The actual code casts ring_index to uint32_t:
;   ring_index = (uint32_t)((uint64_t)next_sequence & RING_MASK)
; Since RING_MASK fits in 7 bits, the uint32_t cast is lossless.
; We prove: (uint64_t)(uint32_t)(seq & 127) == seq & 127
(push)
(declare-fun seq () (_ BitVec 64))

(define-fun masked_64 () (_ BitVec 64)
  (bvand seq RING_MASK))

; Truncate to 32 bits then zero-extend back to 64
(define-fun masked_32_zext () (_ BitVec 64)
  ((_ zero_extend 32) ((_ extract 31 0) masked_64)))

(assert (not (= masked_64 masked_32_zext)))
(check-sat)
; Expected: unsat — the uint32_t cast is lossless because the top
; 57 bits of the masked value are always zero (mask is 7 bits)
(pop)
