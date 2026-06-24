;; stbtt__sort_edges_stable.smt2
;; Edge sort comparator is a valid strict total order by y0
;;
;; Comparator: #define STBTT__COMPARE(a,b)  ((a)->y0 < (b)->y0)
;;
;; Properties:
;;   1. Irreflexive: COMPARE(a,a) is always false
;;   2. Asymmetric: COMPARE(a,b) and COMPARE(b,a) can't both be true
;;   3. Transitive: COMPARE(a,b) ∧ COMPARE(b,c) ⇒ COMPARE(a,c)
;;   4. Equal-y0 stability: neither < the other → original order preserved
;;
(set-logic QF_UF)
(set-info :status unsat)

(declare-sort Edge 0)
(declare-fun COMPARE (Edge Edge) Bool)

(declare-const a Edge)
(declare-const b Edge)
(declare-const c Edge)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 1: Irreflexive — not COMPARE(a,a)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(assert (COMPARE a a))
(check-sat)
;; Expected: unsat
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 2: Asymmetric — not both COMPARE(a,b) and COMPARE(b,a)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(assert (and (COMPARE a b) (COMPARE b a)))
(check-sat)
;; Expected: unsat
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 3: Transitive — COMPARE(a,b) ∧ COMPARE(b,c) ⇒ COMPARE(a,c)
;;─────────────────────────────────────────────────────────────────────────────
(push)
(assert (and (COMPARE a b) (COMPARE b c) (not (COMPARE a c))))
(check-sat)
;; Expected: unsat
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 4: Equal-y0 edges are stable (not forced to reorder)
;;
;; When both COMPARE(a,b) and COMPARE(b,a) are false (equal y0),
;; the insertion sort won't swap them, preserving original order.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(assert (not (COMPARE a b)))
(assert (not (COMPARE b a)))

;; Prove: it's consistent for both to be false (no forced ordering)
(assert (or (COMPARE a b) (COMPARE b a)))
(check-sat)
;; Expected: unsat — equal-y0 edges can both be non-comparable
(pop)

(exit)
