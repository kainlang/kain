;; stbtt__rasterize_scanline_aa.smt2
;; Anti-aliased scanline coverage value is always in [0, max_weight] ⊆ [0, 255]
;;
;; This proof verifies that the coverage contribution added to each pixel
;; in stbtt__fill_active_edges (RASTERIZER_VERSION==1) never exceeds max_weight,
;; which is ≤ 255. This ensures the cast to stbtt_uint8 is safe for each
;; individual coverage contribution.
;;
;; Three cases for a pair of edge crossings (x0, x1) in fixed-point (10 fraction bits):
;;   1. Same pixel:   ((x1 - x0) * max_weight) >> 10   where (x1 - x0) < 1024
;;   2. Left partial: ((FIX - (x0 & FIXMASK)) * max_weight) >> 10   where partial < 1024
;;   3. Right partial: ((x1 & FIXMASK) * max_weight) >> 10   where masked < 1024
;;   4. Full pixel: max_weight directly
;;
(set-logic QF_BV)
(set-info :status unsat)

(define-const FIXSHIFT (_ BitVec 32) #x0000000a)  ;; 10 bits fractional
(define-const FIX (_ BitVec 32) #x00000400)       ;; 1 << 10 = 1024
(define-const FIXMASK (_ BitVec 32) #x000003ff)   ;; 1023
(define-const MAX_WT (_ BitVec 32) #x000000ff)    ;; 255
(define-const ZERO (_ BitVec 32) #x00000000)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 1: Same-pixel coverage ≤ MAX_WT
;;
;; When x0 and x1 are in the same pixel, coverage = ((x1 - x0) * max_weight) >> 10
;; Since 0 ≤ x1 - x0 < FIX = 1024: coverage ≤ (1023 * 255) / 1024 < 255 = MAX_WT.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun x0 () (_ BitVec 32))
(declare-fun x1 () (_ BitVec 32))
(assert (bvuge x1 x0))
(assert (bvult (bvsub x1 x0) FIX))

(define-const diff (_ BitVec 32) (bvsub x1 x0))
(define-const coverage1 (_ BitVec 32)
  (bvlshr (bvmul diff MAX_WT) FIXSHIFT))

(assert (bvugt coverage1 MAX_WT))
(check-sat)
;; Expected: unsat — coverage never exceeds max_weight for same-pixel fill
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 2: Left partial pixel coverage ≤ MAX_WT
;;
;; coverage = ((FIX - (x0 & FIXMASK)) * max_weight) >> 10
;; partial = FIX - (x0 & FIXMASK) ∈ [1, FIX]
;; So coverage ≤ (1024 * 255) / 1024 = 255 = MAX_WT.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun x0 () (_ BitVec 32))
(define-const partial (_ BitVec 32) (bvsub FIX (bvand x0 FIXMASK)))
(assert (bvuge partial #x00000001))   ;; at least 1/1024th pixel covered
(assert (bvule partial FIX))

(define-const coverage2 (_ BitVec 32)
  (bvlshr (bvmul partial MAX_WT) FIXSHIFT))

(assert (bvugt coverage2 MAX_WT))
(check-sat)
;; Expected: unsat — left partial coverage never exceeds max_weight
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 3: Right partial pixel coverage ≤ MAX_WT
;;
;; coverage = ((x1 & FIXMASK) * max_weight) >> 10
;; frac = x1 & FIXMASK ∈ [1, FIXMASK] = [1, 1023]
;; So coverage ≤ (1023 * 255) / 1024 < 255.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun x1 () (_ BitVec 32))
(define-const frac (_ BitVec 32) (bvand x1 FIXMASK))
(assert (bvuge frac #x00000001))       ;; at least one fractional unit
(assert (bvule frac FIXMASK))

(define-const coverage3 (_ BitVec 32)
  (bvlshr (bvmul frac MAX_WT) FIXSHIFT))

(assert (bvugt coverage3 MAX_WT))
(check-sat)
;; Expected: unsat — right partial coverage never exceeds max_weight
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 4: Full pixel coverage = max_weight ≤ 255 (trivial)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(assert (bvugt MAX_WT #x000000ff))
(check-sat)
;; Expected: unsat — 255 is not > 255
(pop)

(exit)
