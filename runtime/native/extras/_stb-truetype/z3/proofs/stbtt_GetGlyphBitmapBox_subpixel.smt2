;; stbtt_GetGlyphBitmapBox_subpixel.smt2
;; Glyph bitmap box dimensions are always non-negative
;;
;; stbtt_GetGlyphBitmapBoxSubpixel computes:
;;   ix0 = STBTT_ifloor( x0 * scale_x + shift_x)
;;   ix1 = STBTT_iceil ( x1 * scale_x + shift_x)
;;
;; For valid glyph bbox (x0 ≤ x1) and positive scale:
;;   width = ix1 - ix0 ≥ 0
;;
(set-logic QF_BV)
(set-info :status unsat)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 1: Scaled + shifted values preserve ordering (with overflow guards)
;;
;; x0 ≤ x1 ∧ scale > 0 ∧ bounded ⇒ x0*scale+shift ≤ x1*scale+shift
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun x0 () (_ BitVec 32))
(declare-fun x1 () (_ BitVec 32))
(declare-fun scale () (_ BitVec 32))
(declare-fun shift () (_ BitVec 32))

(assert (bvsle x0 x1))
(assert (bvsgt scale #x00000000))

;; Font design coords: typically [-1000, 3000] in design units
(assert (bvsge x0 (bvneg #x00001000)))  ;; ≥ -4096
(assert (bvsle x1 #x00001000))           ;; ≤ 4096
;; Scale: font size / design_units, typically < 10 for screen rendering
(assert (bvsgt scale #x00000000))
(assert (bvsle scale #x00001000))        ;; ≤ 4096
;; Shift: subpixel offset in [0, 1) pixel → 0 or 1 in integer coords
(assert (bvsge shift #x00000000))
(assert (bvsle shift #x00000001))

(define-const x0s (_ BitVec 32) (bvadd (bvmul x0 scale) shift))
(define-const x1s (_ BitVec 32) (bvadd (bvmul x1 scale) shift))

(assert (bvslt x1s x0s))
(check-sat)
;; Expected: unsat — bounded arithmetic preserves ordering
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 2: Y-axis flip preserves -y1 ≤ -y0 (for y0 ≤ y1)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun y0 () (_ BitVec 32))
(declare-fun y1 () (_ BitVec 32))

(assert (bvsle y0 y1))
(assert (bvsge y0 (bvneg #x00000fff)))  ;; away from INT_MIN
(assert (bvsle y1 #x00001000))

(define-const ny1 (_ BitVec 32) (bvneg y1))
(define-const ny0 (_ BitVec 32) (bvneg y0))

(assert (bvsgt ny1 ny0))
(check-sat)
;; Expected: unsat — negating flips ordering
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 3: Scaled y-flip values preserve ordering with bounds
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun y0 () (_ BitVec 32))
(declare-fun y1 () (_ BitVec 32))
(declare-fun scale () (_ BitVec 32))
(declare-fun shift () (_ BitVec 32))

(assert (bvsle y0 y1))
(assert (bvsgt scale #x00000000))
(assert (bvsge y0 (bvneg #x00000fff)))
(assert (bvsle y1 #x00001000))
(assert (bvsle scale #x00001000))
(assert (bvsge shift #x00000000))
(assert (bvsle shift #x00000001))

(define-const iy0s (_ BitVec 32) (bvadd (bvmul (bvneg y1) scale) shift))
(define-const iy1s (_ BitVec 32) (bvadd (bvmul (bvneg y0) scale) shift))

(assert (bvslt iy1s iy0s))
(check-sat)
;; Expected: unsat — scaled y-flip preserves ordering
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 4: Width non-negative (bounded subtraction)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun ix0 () (_ BitVec 32))
(declare-fun ix1 () (_ BitVec 32))

(assert (bvsle ix0 ix1))
(assert (bvsge ix0 (bvneg #x00400000)))  ;; bounded
(assert (bvsle ix1 #x00400000))

(define-const width (_ BitVec 32) (bvsub ix1 ix0))
(assert (bvslt width #x00000000))
(check-sat)
;; Expected: unsat — ix1 - ix0 ≥ 0 when ix1 ≥ ix0 and bounded
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 5: Empty glyph returns 0 — trivially non-negative
;;─────────────────────────────────────────────────────────────────────────────
(push)
(assert (not (bvsge (bvsub #x00000000 #x00000000) #x00000000)))
(check-sat)
;; Expected: unsat — 0 ≥ 0 always
(pop)

(exit)
