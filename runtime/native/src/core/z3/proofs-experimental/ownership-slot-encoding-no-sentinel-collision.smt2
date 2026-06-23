; Proof: Slot encoding never collides with tombstone or empty sentinel
;
; The pointer index table uses 0 for empty, INDEX_TOMBSTONE (UINT32_MAX) for deleted,
; and encodes slot as slot+1. This proves that for all valid slot values [0, 4095]:
;   1. slot+1 != 0 (no collision with empty sentinel)
;   2. slot+1 != INDEX_TOMBSTONE (no collision with tombstone)
;   3. slot+1 > 0 (always positive, distinct from empty)
;   4. slot+1 in [1, 4096] (fits in uint32_t without wrapping)
;
; This is safety-critical: a collision with empty or tombstone would cause
; the pointer hash table to misinterpret queries or skip slots.

(set-logic QF_BV)

(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(declare-const index_tombstone (_ BitVec 32))

; Constants from ownership.c
(assert (= max_regions (_ bv4096 32)))
(assert (= index_tombstone (bvnot (_ bv0 32))))  ; UINT32_MAX

; Valid slot range
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun encoded_slot () (_ BitVec 32)
  (bvadd slot (_ bv1 32)))

; Claim 1: encoded_slot != 0 (empty sentinel)
(assert (= encoded_slot (_ bv0 32)))
(check-sat)

(reset)

; ============================================================
; Claim 2: encoded_slot != INDEX_TOMBSTONE (UINT32_MAX)
; ============================================================
(set-logic QF_BV)
(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(assert (= max_regions (_ bv4096 32)))
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun encoded_slot () (_ BitVec 32)
  (bvadd slot (_ bv1 32)))
(define-fun index_tombstone () (_ BitVec 32)
  (bvnot (_ bv0 32)))

(assert (= encoded_slot index_tombstone))
(check-sat)

(reset)

; ============================================================
; Claim 3: encoded_slot > 0 (strictly positive)
; ============================================================
(set-logic QF_BV)
(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(assert (= max_regions (_ bv4096 32)))
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun encoded_slot () (_ BitVec 32)
  (bvadd slot (_ bv1 32)))

(assert (not (bvugt encoded_slot (_ bv0 32))))
(check-sat)

(reset)

; ============================================================
; Claim 4: encoded_slot never wraps (fits in uint32_t)
; Prove: encoded_slot <= 4096 < 2^32, so no overflow
; ============================================================
(set-logic QF_BV)
(declare-const slot (_ BitVec 32))
(declare-const max_regions (_ BitVec 32))
(assert (= max_regions (_ bv4096 32)))
(assert (bvuge slot (_ bv0 32)))
(assert (bvult slot max_regions))

(define-fun encoded_slot () (_ BitVec 32)
  (bvadd slot (_ bv1 32)))

; If there's no overflow, encoded_slot > slot
(assert (bvule encoded_slot slot))
(check-sat)
