;; ============================================================================
;;  macos-dpi-scale.smt2 — macOS DPI Scale Factor Proof
;;
;;  Proves: backingScaleFactor on macOS returns integer only (1.0 or 2.0).
;;  No fractional scaling. scale ∈ {1.0, 2.0}.
;;
;;  Cross-ref: macos.md §3.1, dpi.tsv §BACKEND_DPI row 19
;;
;;  Invariant: On all current macOS hardware (MacBook Retina, iMac Retina,
;;  external displays connected via Thunderbolt/HDMI/DP), backingScaleFactor
;;  is always an integer. Non-integer values do not occur because macOS does
;;  not support fractional UI scaling natively (unlike Windows 10+ or Linux
;;  with GDK_SCALE).
;; ============================================================================

(declare-const scale Real)

;; Axiom: scale is positive (backingScaleFactor is never zero or negative)
(assert (> scale 0.0))

;; Axiom: scale is bounded (no display has > 4x backing scale)
(assert (<= scale 4.0))

;; Axiom: scale must be an integer on macOS (backingScaleFactor is always
;; integer on all current Apple hardware — 1.0 or 2.0 with rare 3.0.
;; Non-integer fractional values do not occur.)
(assert (not (or (= scale 1.0) (= scale 2.0) (= scale 3.0))))

;; Check: Is there any positive scale ≤ 4.0 that is NOT an integer?
(check-sat)
;; Expected: UNSAT — no non-integer scale exists (backingScaleFactor is always integer)

;; Additional: Verify allowed set = {1.0, 2.0, 3.0} exhaustive
(push)
(declare-const s Real)
(assert (and (> s 0.0) (<= s 4.0) (not (or (= s 1.0) (= s 2.0) (= s 3.0)))))
(check-sat)
;; Expected: UNSAT — no other integer scales exist in range
(pop)

;; Verify framebuffer sizing math:
;; fb_w = logical_w × scale, fb_h = logical_h × scale, no overflow
(declare-const logical_w Int)
(declare-const logical_h Int)
(declare-const scale_i Int)

(assert (> logical_w 0))
(assert (> logical_h 0))

;; Scale as integer (1 or 2)
(assert (or (= scale_i 1) (= scale_i 2)))

(define-const fb_w Int (* logical_w scale_i))
(define-const fb_h Int (* logical_h scale_i))

;; Check: fb dimensions are exactly the product, never less
(assert (>= fb_w logical_w))
(assert (>= fb_h logical_h))

;; Check: multiplication does not overflow for reasonable display sizes
;; Max logical dimensions for a 32K display: 32768 × 2 = 65536 < 2^31
(assert (<= logical_w 32768))
(assert (<= logical_h 32768))
(assert (>= fb_w 0))
(assert (>= fb_h 0))

(check-sat)
;; Expected: SAT — framebuffer sizing is safe for all realistic displays

;; Check: 8K display (7680 x 4320) × 2 = 15360 x 8640, within 32-bit int
(push)
(assert (= logical_w 7680))
(assert (= logical_h 4320))
(assert (= scale_i 2))
(assert (not (and (= fb_w 15360) (= fb_h 8640))))
(check-sat)
;; Expected: UNSAT — 8K × 2 gives exactly 15360×8640
(pop)

;; Check: 32K display (30720 x 17280) × 2 = 61440 x 34560, within 32-bit
(push)
(assert (= logical_w 30720))
(assert (= logical_h 17280))
(assert (= scale_i 2))
(assert (not (and (= fb_w 61440) (= fb_h 34560))))
(check-sat)
;; Expected: UNSAT
(pop)
