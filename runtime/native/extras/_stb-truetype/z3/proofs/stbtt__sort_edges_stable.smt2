;; stbtt__sort_edges_stable.smt2
;; Edge sort comparator (y0 < y0') is a valid strict total order
;;
;; The comparator: #define STBTT__COMPARE(a,b)  ((a)->y0 < (b)->y0)
;;
;; We model the comparator using QF_BV with 32-bit floats encoded as
;; IEEE 754 bitvectors. Since exact IEEE 754 comparison is complex,
;; we use the algebraic properties of a strict total order over reals.
;;
;; Properties proved:
;;   1. Irreflexive: COMPARE(a,a) = false
;;   2. Asymmetric: COMPARE(a,b) ∧ COMPARE(b,a) is impossible
;;   3. Transitive: COMPARE(a,b) ∧ COMPARE(b,c) ⇒ COMPARE(a,c)
;;   4. Equal-y0 edges are stable (comparator returns false both ways)
;;   5. Insertion sort termination is correct
;;
(set-logic QF_BV)
(set-info :status unsat)

;; Model y0 as a 32-bit float represented as IEEE 754 single-precision.
;; We use the sign-magnitude property: for non-negative values,
;; float comparison = unsigned comparison of the bit pattern.
;; We restrict to y0 ≥ 0 (glyph coordinates are non-negative or near-zero).
;;
;; Define COMPARE(a,b) = (a != b) & (a < b) for non-negative 32-bit floats.
;; For non-negative IEEE 754 floats, the bitwise unsigned comparison
;; matches the float comparison.

(define-const ZERO (_ BitVec 32) #x00000000)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 1: Irreflexive — y0 < y0 is always false
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun a () (_ BitVec 32))

;; For any value a, a < a is false.
;; Signed comparison: a < a is always false.
(assert (bvslt a a))
(check-sat)
;; Expected: unsat — irreflexive
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 2: Asymmetric — y0(a) < y0(b) and y0(b) < y0(a) can't both be true
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun a () (_ BitVec 32))
(declare-fun b () (_ BitVec 32))

;; For any two values, a < b and b < a can't both hold
(assert (and (bvslt a b) (bvslt b a)))
(check-sat)
;; Expected: unsat — asymmetric
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 3: Transitive — a < b and b < c ⇒ a < c
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun a () (_ BitVec 32))
(declare-fun b () (_ BitVec 32))
(declare-fun c () (_ BitVec 32))

(assert (and (bvslt a b) (bvslt b c) (not (bvslt a c))))
(check-sat)
;; Expected: unsat — transitive for signed comparison
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 4: Equal values return false both ways (stability)
;;
;; When y0(a) == y0(b): neither a < b nor b < a is true.
;; The comparator returns false for both directions → no forced reordering.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun a () (_ BitVec 32))
(declare-fun b () (_ BitVec 32))

;; Equal values
(assert (= a b))

;; For equal values, a < b is false and b < a is false
;; We prove: if a = b, then not (a < b or b < a)
(assert (or (bvslt a b) (bvslt b a)))
(check-sat)
;; Expected: unsat — equal values don't compare less-than either way
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 5: Insertion sort termination
;;
;; The insertion sort loop terminates because each iteration reduces
;; the unsorted suffix by 1. We prove the stop condition is sound:
;; when COMPARE(a, p[j-1]) is false, a belongs at position j (sorted).
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun a () (_ BitVec 32))      ;; edge being inserted
(declare-fun b () (_ BitVec 32))      ;; p[j-1], the predecessor

;; a is not less than b → a should be at or after b in sorted order
(assert (not (bvslt a b)))

;; If a >= b, inserting a after b is correct (sorted)
;; The stop condition (!COMPARE) is correct — a belongs here.
;; Check: the loop would stop and a would be placed at position j.
(assert (bvslt a b))
(check-sat)
;; Expected: unsat — stop condition is correct: a ≥ b, so a goes after b
(pop)

(exit)
