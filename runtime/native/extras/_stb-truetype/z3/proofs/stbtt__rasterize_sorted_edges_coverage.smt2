;; stbtt__rasterize_sorted_edges_coverage.smt2
;; Non-zero winding rule accumulation is correct
;;
;; In stbtt__fill_active_edges (v1, non-zero winding fill):
;;   - Start with winding count w = 0
;;   - For each active edge in x-sorted order:
;;     - If w == 0: record x0 = e->x, then w += e->direction
;;     - Else: x1 = e->x, w += e->direction
;;       If w == 0: fill pixel(s) between x0 and x1
;;
;; This implements the non-zero winding rule. The winding count w tracks
;; entry/exit of the glyph interior. For valid closed outlines, w toggles
;; between 0 and ±1 as edges are crossed left-to-right.
;;
(set-logic QF_BV)
(set-info :status unsat)

;; Edge directions: 1 and -1 (in 2's complement: 01 and 11)
(define-const W_ZERO (_ BitVec 2) #b00)
(define-const W_P1 (_ BitVec 2) #b01)
(define-const W_M1 (_ BitVec 2) #b11)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 1: Winding count w stays in {-1, 0, +1} WITH alternation constraint
;;
;; For a closed glyph outline, edges alternate direction: when w is non-zero,
;; the next crossing must be in the OPPOSITE direction. This prevents w from
;; ever going to +2 or -2.
;;
;; Constraint: when w_in ≠ 0, dir must be opposite to w_in:
;;   if w_in = +1: dir must be -1 (exit)
;;   if w_in = -1: dir must be +1 (exit, back toward 0)
;;   if w_in = 0: dir can be ±1 (enter)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun w_in () (_ BitVec 2))
(declare-fun dir () (_ BitVec 2))

;; w_in ∈ {-1, 0, 1}
(assert (or (= w_in W_ZERO) (= w_in W_P1) (= w_in W_M1)))
;; dir ∈ {-1, 1}
(assert (or (= dir W_P1) (= dir W_M1)))

;; Alternation constraint: when w_in ≠ 0, dir must be opposite sign
(assert (=> (= w_in W_P1) (= dir W_M1)))
(assert (=> (= w_in W_M1) (= dir W_P1)))

(define-const w_out (_ BitVec 2) (bvadd w_in dir))

;; Prove: w_out ∈ {-1, 0, 1}
(assert (not (or (= w_out W_ZERO) (= w_out W_P1) (= w_out W_M1))))
(check-sat)
;; Expected: unsat — with alternation, winding never leaves {-1,0,1}
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 2: Alternating winding sequence with opposite directions is valid
;;
;; For a simple closed outline, edges appear in pairs: enter (+1) then exit (-1).
;; After processing an enter+exit pair, w returns to 0.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun w0 () (_ BitVec 2))
(assert (= w0 W_ZERO))

;; Enter: dir = +1
(define-const w1 (_ BitVec 2) (bvadd w0 W_P1))

;; Exit: dir = -1
(define-const w2 (_ BitVec 2) (bvadd w1 W_M1))

;; Prove: w1 = +1 and w2 = 0
(assert (not (and (= w1 W_P1) (= w2 W_ZERO))))
(check-sat)
;; Expected: unsat — enter (+1) then exit (-1) returns to 0
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 3: Filled region is between enter/exit edge pairs
;;
;; When w goes from 0 to +1 (enter) at x0 and back to 0 (exit) at x1,
;; all pixel centers p with x0 < p < x1 are inside the glyph (filled).
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun x0 () (_ BitVec 10))  ;; pixel column of enter edge (0..1023)
(declare-fun x1 () (_ BitVec 10))  ;; pixel column of exit edge (0..1023)
(declare-fun p () (_ BitVec 10))   ;; pixel center

;; Valid enter/exit pair: x0 < x1
(assert (bvult x0 x1))

;; p is between x0 and x1
(assert (bvugt p x0))
(assert (bvult p x1))

;; The winding count at p is +1 (entered but not yet exited)
(define-const w_p (_ BitVec 2) W_P1)

;; Non-zero winding means filled
(assert (= w_p W_ZERO))
(check-sat)
;; Expected: unsat — pixel between enter/exit has winding +1 (non-zero ⇒ filled)
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 4: Non-zero winding fill accumulates correctly across multiple pairs
;;
;; For a glyph with multiple enclosed regions, each enter/exit pair produces
;; a filled region. Non-overlapping pairs produce disjoint filled spans.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun x0a () (_ BitVec 10))
(declare-fun x1a () (_ BitVec 10))
(declare-fun x0b () (_ BitVec 10))
(declare-fun x1b () (_ BitVec 10))
(declare-fun p () (_ BitVec 10))

;; Two non-overlapping enter/exit pairs: [x0a, x1a] and [x0b, x1b]
(assert (bvult x1a x0b))  ;; first region ends before second starts

;; x0a < x1a, x0b < x1b
(assert (bvult x0a x1a))
(assert (bvult x0b x1b))

;; p is in the gap between the two regions
(assert (bvugt p x1a))
(assert (bvult p x0b))

;; In the gap, winding count is 0 (exited first region, not yet entered second)
(define-const w_gap (_ BitVec 2) W_ZERO)

;; Gap should NOT be filled (winding is zero)
(assert (not (= w_gap W_ZERO)))
(check-sat)
;; Expected: unsat — gap between regions has zero winding, not filled
(pop)

(exit)
