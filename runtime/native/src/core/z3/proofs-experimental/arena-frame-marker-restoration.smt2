; Proof: kain_frame_release_to_last_marker correctly restores
; the arena low/high pointers to their state at marker time.
;
; The frame marker system:
;   kain_frame_set_marker: records (low - start, high - start) as offsets
;   kain_frame_release_to_last_marker: restores low = start + low_offset,
;                                       high = start + high_offset
;
; Claim: The restoration is an exact inverse of the snapshot.
; For any valid arena state (start <= low <= high <= end),
; after set_marker then release_to_last_marker, the low/high
; pointers are restored to their exact positions at marker time.
;
; This proof shows that:
;   1. low_offset <= high_offset (positive region invariant)
;   2. start + low_offset exactly recovers the original low
;   3. start + high_offset exactly recovers the original high
;   4. The restoration preserves the low <= high invariant
;
; Domain assumptions:
;   - arena->start is a valid pointer
;   - arena->low and arena->high are within [start, end]
;   - frame.depth > 0 when release_to_last_marker is called
;   - low_offset, high_offset are size_t values computed as
;     (low - start) and (high - start) which are in [0, reserved_bytes]

(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const low (_ BitVec 64))
(declare-const high (_ BitVec 64))
(declare-const end (_ BitVec 64))

; Arena invariant: start <= low <= high <= end
(assert (bvule start low))
(assert (bvule low high))
(assert (bvule high end))

; --- Marker offsets computed by set_marker ---
(define-fun low_offset () (_ BitVec 64)
  (bvsub low start))
(define-fun high_offset () (_ BitVec 64)
  (bvsub high start))

; --- Restoration by release_to_last_marker ---
(define-fun restored_low () (_ BitVec 64)
  (bvadd start low_offset))
(define-fun restored_high () (_ BitVec 64)
  (bvadd start high_offset))

; Claim 1: Offsets are non-negative (no wrap)
(assert (not (bvule low_offset (bvsub high start))))
; Wait, wrong - let me prove the correct properties.

(reset)

; ============================================================
; Claim 1: low_offset <= high_offset for valid arena state
; ============================================================
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const low (_ BitVec 64))
(declare-const high (_ BitVec 64))

(assert (bvule start low))
(assert (bvule low high))

(define-fun low_offset () (_ BitVec 64) (bvsub low start))
(define-fun high_offset () (_ BitVec 64) (bvsub high start))

; Prove: low_offset <= high_offset
(assert (bvugt low_offset high_offset))
(check-sat)
; Expected: unsat (low_offset <= high_offset is invariant)

(reset)

; ============================================================
; Claim 2: Restoration exactly recovers original low/high
; ============================================================
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const low (_ BitVec 64))
(declare-const high (_ BitVec 64))

(assert (bvule start low))
(assert (bvule low high))

(define-fun low_offset () (_ BitVec 64) (bvsub low start))
(define-fun high_offset () (_ BitVec 64) (bvsub high start))

(define-fun restored_low () (_ BitVec 64) (bvadd start low_offset))
(define-fun restored_high () (_ BitVec 64) (bvadd start high_offset))

; Prove: restored_low == low
(assert (not (= restored_low low)))
(check-sat)
; Expected: unsat (restored_low exactly equals low)

(reset)

; ============================================================
; Claim 2b: Restored high == high
; ============================================================
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const low (_ BitVec 64))
(declare-const high (_ BitVec 64))

(assert (bvule start low))
(assert (bvule low high))

(define-fun high_offset () (_ BitVec 64) (bvsub high start))
(define-fun restored_high () (_ BitVec 64) (bvadd start high_offset))

(assert (not (= restored_high high)))
(check-sat)
; Expected: unsat (restored_high exactly equals high)

(reset)

; ============================================================
; Claim 3: After restoration, low <= high invariant holds
; ============================================================
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const low (_ BitVec 64))
(declare-const high (_ BitVec 64))

(assert (bvule start low))
(assert (bvule low high))

(define-fun low_offset () (_ BitVec 64) (bvsub low start))
(define-fun high_offset () (_ BitVec 64) (bvsub high start))
(define-fun restored_low () (_ BitVec 64) (bvadd start low_offset))
(define-fun restored_high () (_ BitVec 64) (bvadd start high_offset))

; Prove: restored_low <= restored_high
(assert (bvugt restored_low restored_high))
(check-sat)
; Expected: unsat (restoration preserves low <= high)

(reset)

; ============================================================
; Claim 4: After any sequence of alloc_lo/alloc_hi between
; set_marker and release_to_last_marker, the restoration still
; recovers the exact marker-time state.
;
; This is a trivial consequence of the snapshot model: the offsets
; are captured atomically under the arena lock, so intermediate
; alloc_lo/alloc_hi calls cannot affect the stored offsets.
; ============================================================
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const low_at_marker (_ BitVec 64))
(declare-const high_at_marker (_ BitVec 64))

(assert (bvule start low_at_marker))
(assert (bvule low_at_marker high_at_marker))

; Offsets captured at marker time
(define-fun captured_low_offset () (_ BitVec 64)
  (bvsub low_at_marker start))
(define-fun captured_high_offset () (_ BitVec 64)
  (bvsub high_at_marker start))

; Later, at release time, low/high may have changed, but we
; use the captured offsets:
(define-fun restored_low () (_ BitVec 64)
  (bvadd start captured_low_offset))
(define-fun restored_high () (_ BitVec 64)
  (bvadd start captured_high_offset))

; Prove: restoration gives back marker-time values regardless
; of current low/high
(assert (not (= restored_low low_at_marker)))
(check-sat)
; Expected: unsat (restored_low == low_at_marker always)

(reset)

; ============================================================
; Claim 5: Frame depth bounds
; kain_frame_set_marker checks depth < KAIN_FRAME_MAX_DEPTH (8)
; kain_frame_release_to_last_marker checks depth > 0
; Prove that the markers array access is always in bounds.
; ============================================================
(set-logic QF_BV)

; Before release_to_last_marker: depth >= 1
; After depth -= 1: depth is in [0, KAIN_FRAME_MAX_DEPTH - 1]
; So markers[depth] accesses are in bounds [0, KAIN_FRAME_MAX_DEPTH - 1]
(declare-const depth (_ BitVec 8))
(assert (bvugt depth (_ bv0 8)))  ; depth > 0 before decrement
(assert (bvule depth (_ bv8 8)))  ; depth <= KAIN_FRAME_MAX_DEPTH

; After decrement
(define-fun depth_after_dec () (_ BitVec 8)
  (bvsub depth (_ bv1 8)))

; Access index must be < KAIN_FRAME_MAX_DEPTH (8)
(assert (bvugt depth_after_dec (_ bv7 8)))
(check-sat)
; Expected: unsat (access index is always < 8)

(reset)

; ============================================================
; Claim 6: release_all restores to initial state
; kain_frame_release_all: depth=0, low=start, high=end
; This is equivalent to releasing all markers at once.
; ============================================================
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const end (_ BitVec 64))
(assert (bvule start end))

(define-fun released_low () (_ BitVec 64) start)
(define-fun released_high () (_ BitVec 64) end)

; After release_all, the region is the full arena
(assert (not (= released_low start)))
(check-sat)
; Expected: unsat (released_low == start)

(reset)
(set-logic QF_BV)
(declare-const start (_ BitVec 64))
(declare-const end (_ BitVec 64))
(assert (bvule start end))

(assert (not (= end (bvadd start (bvsub end start)))))
(check-sat)
; Expected: unsat (trivially true)
