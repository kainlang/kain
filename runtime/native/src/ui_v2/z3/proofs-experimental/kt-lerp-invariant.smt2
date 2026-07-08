; ============================================================================
; kt-lerp-invariant.smt2
; Claim: The lerp function satisfies algebraic invariants.
;
; Proved:
;   (1) lerp(a,b,0) = a                                   [identity at t=0]
;   (2) lerp(a,b,1) = b                                   [identity at t=1]
;   (3) lerp(a,b,t) = a + (b-a)*t                         [linearity]
;   (4) lerp(a,b,t) = b - (b-a)*(1-t)                     [symmetry]
;   (5) lerp(lerp(a,b,t), b, s) = lerp(a, b, t+s - t*s)   [composition]
;   (6) lerp(a,b,t) != lerp(b,a,t) for t != 0.5, a != b  [non-comm]
;   (7) lerp(a,b,t) ∈ [a,b] for t ∈ [0,1]               [range]
;   (8) Bilinear interpolation is separable               [2D property]
;
; Used EVERYWHERE in Kaintana:
;   kt_color_lerp, kt_ease_smoothstep, pulse.c animation
;
; ============================================================================

; Claim 1: lerp(a,b,0) = a
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)
(define-fun lerp ((x Real) (y Real) (t Real)) Real (+ x (* (- y x) t)))
(assert (not (= (lerp a b 0) a)))
(check-sat)
; >>> unsat ✓

; Claim 2: lerp(a,b,1) = b
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)
(define-fun lerp ((x Real) (y Real) (t Real)) Real (+ x (* (- y x) t)))
(assert (not (= (lerp a b 1) b)))
(check-sat)
; >>> unsat ✓

; Claim 3: lerp(a,b,t) = a + (b-a)*t
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)(declare-const t Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(assert (not (= (lerp a b t) (+ a (* (- b a) t)))))
(check-sat)
; >>> unsat ✓

; Claim 4: Symmetry: lerp(a,b,t) = b - (b-a)*(1-t)
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)(declare-const t Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(assert (not (= (lerp a b t) (- b (* (- b a) (- 1 t))))))
(check-sat)
; >>> unsat ✓

; Claim 5: Composition: lerp(lerp(a,b,t), b, s) = lerp(a, b, t + s - t*s)
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)
(declare-const t Real)(declare-const s Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(define-fun left () Real (lerp (lerp a b t) b s))
(define-fun right () Real (lerp a b (- (+ t s) (* t s))))
(assert (not (= left right)))
(check-sat)
; >>> unsat ✓

; Claim 6: Non-commutativity for t != 0.5, a != b
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)(declare-const t Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(assert (not (= t 0.5)))
(assert (not (= a b)))
(assert (= (lerp a b t) (lerp b a t)))
(check-sat)
; >>> unsat → non-commutative for t ≠ 0.5 ✓

; Claim 7: lerp(a,b,t) ∈ [a,b] for t ∈ [0,1] when a < b
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)(declare-const t Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(assert (< a b))
(assert (and (>= t 0) (<= t 1)))
(assert (or (< (lerp a b t) a) (> (lerp a b t) b)))
(check-sat)
; >>> unsat → lerp stays in [a,b] for t∈[0,1] ✓

; Claim 8: Bilinear interpolation is separable
; lerp(lerp(a,b,t), lerp(c,d,t), s) = lerp(lerp(a,c,s), lerp(b,d,s), t)
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)
(declare-const c Real)(declare-const d Real)
(declare-const s Real)(declare-const t Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
; Horizontal then vertical
(define-fun horiz_vert () Real (lerp (lerp a b t) (lerp c d t) s))
; Vertical then horizontal
(define-fun vert_horiz () Real (lerp (lerp a c s) (lerp b d s) t))
(assert (not (= horiz_vert vert_horiz)))
(check-sat)
; >>> unsat → 2D bilinear interpolation is separable ✓

; Claim 9: Color lerp: inputs in [0,1], t in [0,1] → output in [0,1]
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)(declare-const t Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(assert (and (>= a 0) (<= a 1)))
(assert (and (>= b 0) (<= b 1)))
(assert (and (>= t 0) (<= t 1)))
(assert (or (< (lerp a b t) 0) (> (lerp a b t) 1)))
(check-sat)
; >>> unsat ✓

; Claim 10: Generalized composition: lerp(lerp(a,b,t), c, s)
; = lerp(a,c,s) + t*(1-s)*(b-a)
(reset)
(set-logic QF_NRA)
(declare-const a Real)(declare-const b Real)(declare-const c Real)
(declare-const t Real)(declare-const s Real)
(define-fun lerp ((x Real) (y Real) (u Real)) Real (+ x (* (- y x) u)))
(define-fun left () Real (lerp (lerp a b t) c s))
(define-fun right () Real (+ (lerp a c s) (* t (- 1 s) (- b a))))
(assert (not (= left right)))
(check-sat)
; >>> unsat ✓
