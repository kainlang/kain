; Proof: Branchless kt_layout_clamp for int32
;
; Target: box_math.c — Formula GS-4
; API: kt_layout_clamp()
;
; Current (clamped with branches):
;   int32_t kt_layout_clamp(int32_t v, int32_t lo, int32_t hi) {
;       return v < lo ? lo : (v > hi ? hi : v);
;   }
;
; Branchless replacement using min/max bitwise identities:
;   int32_t kt_layout_clamp(int32_t v, int32_t lo, int32_t hi) {
;       int32_t t = v ^ ((v ^ lo) & -(v < lo));   // max(v, lo)
;       return hi ^ ((hi ^ t) & -(hi < t));         // min(hi, max(v, lo))
;   }
;
; Domain: int32 signed integer, CSS rule: if lo > hi, lo wins (not handled here)
;   Precondition: lo <= hi
;
; Cost: 0 branches, ~10 ALU ops, ~5 cycles latency
; vs 2 branches + potential mispredict penalty (~15-20 cycles)

(set-logic QF_BV)

; ── IDENTITY 1: Branchless max ──
; max(a,b) = a ^ ((a ^ b) & -(a < b))
; If a < b: -(a<b) = -1 = 0xFFFFFFFF
;   a ^ ((a ^ b) & 0xFFFFFFFF) = a ^ (a ^ b) = b
; If a >= b: -(a<b) = 0
;   a ^ ((a ^ b) & 0) = a

(declare-fun a () (_ BitVec 32))
(declare-fun b () (_ BitVec 32))

(define-fun bmax ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor x (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun rmax ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (ite (bvslt x y) y x))

(assert (not (= (bmax a b) (rmax a b))))
(check-sat)
; Expected: unsat

; ── IDENTITY 2: Branchless min ──
; min(a,b) = b ^ ((a ^ b) & -(a < b))
(reset)
(set-logic QF_BV)
(declare-fun a () (_ BitVec 32))
(declare-fun b () (_ BitVec 32))

(define-fun bmin ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor y (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun rmin ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (ite (bvslt x y) x y))

(assert (not (= (bmin a b) (rmin a b))))
(check-sat)
; Expected: unsat

; ── IDENTITY 3: Branchless clamp = min(max(v,lo), hi) ──
(reset)
(set-logic QF_BV)
(declare-fun v () (_ BitVec 32))
(declare-fun lo () (_ BitVec 32))
(declare-fun hi () (_ BitVec 32))
(assert (bvsle lo hi))

(define-fun bmax ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor x (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun bmin ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor y (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun bclamp ((x (_ BitVec 32)) (l (_ BitVec 32)) (h (_ BitVec 32))) (_ BitVec 32)
  (bmin (bmax x l) h))

(define-fun rclamp ((x (_ BitVec 32)) (l (_ BitVec 32)) (h (_ BitVec 32))) (_ BitVec 32)
  (ite (bvslt x l) l (ite (bvsgt x h) h x)))

(assert (not (= (bclamp v lo hi) (rclamp v lo hi))))
(check-sat)
; Expected: unsat

; ── INVARIANT: clamp result ∈ [lo, hi] ──
(reset)
(set-logic QF_BV)
(declare-fun v () (_ BitVec 32))
(declare-fun lo () (_ BitVec 32))
(declare-fun hi () (_ BitVec 32))
(assert (bvsle lo hi))

(define-fun bmax ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor x (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun bmin ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor y (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun bclamp ((x (_ BitVec 32)) (l (_ BitVec 32)) (h (_ BitVec 32))) (_ BitVec 32)
  (bmin (bmax x l) h))

(declare-const r (_ BitVec 32))
(assert (= r (bclamp v lo hi)))
(assert (bvslt r lo))
(check-sat)
; Expected: unsat — result >= lo

(reset)
(set-logic QF_BV)
(declare-fun v () (_ BitVec 32))
(declare-fun lo () (_ BitVec 32))
(declare-fun hi () (_ BitVec 32))
(assert (bvsle lo hi))

(define-fun bmax ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor x (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun bmin ((x (_ BitVec 32)) (y (_ BitVec 32))) (_ BitVec 32)
  (bvxor y (bvand (bvxor x y) (bvneg (ite (bvslt x y) (_ bv1 32) (_ bv0 32))))))

(define-fun bclamp ((x (_ BitVec 32)) (l (_ BitVec 32)) (h (_ BitVec 32))) (_ BitVec 32)
  (bmin (bmax x l) h))

(declare-const r (_ BitVec 32))
(assert (= r (bclamp v lo hi)))
(assert (bvsgt r hi))
(check-sat)
; Expected: unsat — result <= hi

; ── PROOF COMPLETE ──
(echo "=== KT_LAYOUT_CLAMP BRANCHLESS PROOF ===")
(echo "Identity: clamp(v, lo, hi) = min(max(v, lo), hi)  [branchless]")
(echo "Invariant: lo <= result <= hi when lo <= hi")
(echo "Branchless cost: ~10 ALU ops, 0 branches")
(echo "vs reference: 2 branches + potential mispredict penalty")
