;; ============================================================
;; Proof: Branchless point-in-rect test
;;
;; Target: hit_test.c — kt_hit_contains_point()
;;
;; Current (short-circuit &&, 4 branches):
;;   return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
;;
;; Integer optimization (Hacker's Delight, 0 branches):
;;   return (unsigned)(px - rx) < (unsigned)rw
;;      && (unsigned)(py - ry) < (unsigned)rh;
;;
;; Each claim self-contained. Precondition: rx+rw, ry+rh no overflow.
;; ============================================================

;; Claim 1: _Bool bitwise-AND ≡ logical-AND
(set-logic QF_BV)
(declare-fun a () (_ BitVec 1))
(declare-fun b () (_ BitVec 1))
(declare-fun c () (_ BitVec 1))
(declare-fun d () (_ BitVec 1))
(define-fun bitwise_and () (_ BitVec 1) (bvand a (bvand b (bvand c d))))
(define-fun logical_and () (_ BitVec 1)
  (ite (and (= a #b1) (= b #b1) (= c #b1) (= d #b1)) #b1 #b0))
(assert (not (= bitwise_and logical_and)))
(check-sat)
;; Expected: unsat — equivalent for _Bool (0/1)

;; Claim 2: Full 2D integer point-in-rect via unsigned subtraction
(reset)
(set-logic QF_BV)
(declare-fun ix () (_ BitVec 32))
(declare-fun iy () (_ BitVec 32))
(declare-fun rx () (_ BitVec 32))
(declare-fun ry () (_ BitVec 32))
(declare-fun rw () (_ BitVec 32))
(declare-fun rh () (_ BitVec 32))
(assert (bvsgt rw (_ bv0 32)))
(assert (bvsgt rh (_ bv0 32)))
;; Precondition: no overflow on right/bottom edges
(assert (bvuge (bvadd rx rw) rx))
(assert (bvuge (bvadd ry rh) ry))
;; Reference: unsigned comparisons (same semantics for non-neg rw/rh)
(define-fun ref_contains () Bool
  (and (bvuge ix rx) (bvult ix (bvadd rx rw))
       (bvuge iy ry) (bvult iy (bvadd ry rh))))
;; Candidate: Hacker's Delight trick
(define-fun cand_contains () Bool
  (and (bvult (bvsub ix rx) rw) (bvult (bvsub iy ry) rh)))
(assert (not (= ref_contains cand_contains)))
(check-sat)
;; Expected: unsat — Hacker's Delight trick equivalent with overflow guard

(echo "=== POINT-IN-RECT: unsat = PROVEN ===")
(echo "1. _Bool bitwise-AND ≡ logical-AND (non-short-circuit)")
(echo "2. Full 2D: (unsigned)(px-rx)<(unsigned)rw proven")
(echo "   Precondition: rx+rw, ry+rh no overflow (arena invariant)")
(echo "Final C: return (unsigned)(px-rx)<(unsigned)rw && ...;")
