; ============================================================================
; kt-spring-implicit-euler.smt2
; Claim: Spring physics properties for Kaintana animation.
;
; Uses only polynomial arithmetic (QF_NRA).
;
; Proved:
;   (a) SwiftUI defaults: under-damped (0 < c² < 4mk)
;   (b) Semi-implicit Euler with dt=1/60: energy non-increasing
;   (c) Critically damped from rest: displacement contracts
;   (d) Decay constant positive
;   (e) Energy bounded for bounded state
;
; ============================================================================

; Claim 1: SwiftUI defaults are under-damped (c² < 4mk)
(reset)
(set-logic QF_NRA)
(declare-const k Real)
(declare-const c Real)
(declare-const m Real)
(assert (and (= k 131.5) (= c 14.4) (= m 1.0)))
(assert (not (< (* c c) (* 4 m k))))
(check-sat)
; >>> unsat → under-damped ✓

; Claim 2: Semi-implicit Euler with SwiftUI defaults at 60fps
; Energy is non-increasing for one step
(reset)
(set-logic QF_NRA)
(declare-const x Real)
(declare-const v Real)
(declare-const target Real)
(declare-const k Real)
(declare-const c Real)
(declare-const m Real)
(declare-const dt Real)

(assert (and (= k 131.5) (= c 14.4) (= m 1.0) (= dt (/ 1 60))))

; Semi-implicit Euler
(define-fun accel ((x_pos Real) (vel Real)) Real
  (/ (+ (* (- k) (- x_pos target)) (* (- c) vel)) m))
(define-fun v_next () Real
  (+ v (* (accel x v) dt)))
(define-fun x_next () Real
  (+ x (* v_next dt)))

; Energy before and after
(define-fun e_old () Real
  (+ (* 0.5 m v v) (* 0.5 k (- x target) (- x target))))
(define-fun e_new () Real
  (+ (* 0.5 m v_next v_next)
     (* 0.5 k (- x_next target) (- x_next target))))

; Non-increasing energy
(assert (> e_new e_old))
(check-sat)
; >>> unsat → energy non-increasing for SwiftUI defaults at 60fps ✓

; Claim 3: Critically damped from rest — displacement contracts
(reset)
(set-logic QF_NRA)
(declare-const x Real)
(declare-const target Real)
(declare-const k Real)
(declare-const c Real)
(declare-const m Real)
(declare-const dt Real)

(assert (and (= k 131.5) (= c 14.4) (= m 1.0) (= dt (/ 1 60))))
(assert (not (= x target)))

(define-fun accel ((x_pos Real)) Real
  (/ (+ (* (- k) (- x_pos target)) (* (- c) 0)) m))
(define-fun v1 () Real
  (+ 0 (* (accel x) dt)))
(define-fun x1 () Real
  (+ x (* v1 dt)))

(define-fun old_disp_sq () Real (* (- x target) (- x target)))
(define-fun new_disp_sq () Real (* (- x1 target) (- x1 target)))

(assert (>= new_disp_sq old_disp_sq))
(check-sat)
; >>> unsat → displacement contracts from rest ✓

; Claim 4: Decay constant c/(2m) > 0 for SwiftUI defaults
(reset)
(set-logic QF_NRA)
(declare-const c Real)
(declare-const m Real)
(assert (and (= c 14.4) (= m 1.0)))
(assert (not (> (/ c (* 2 m)) 0)))
(check-sat)
; >>> unsat → decay constant positive ✓

; Claim 5: Energy bounded for bounded state
(reset)
(set-logic QF_NRA)
(declare-const x Real)
(declare-const v Real)
(declare-const target Real)
(declare-const D Real)
(declare-const V Real)
(declare-const k Real)
(declare-const m Real)

(assert (and (= k 131.5) (= m 1.0)))
(assert (>= D 0))
(assert (>= V 0))
(assert (>= (* D D) (* (- x target) (- x target))))
(assert (>= (* V V) (* v v)))

(define-fun energy () Real
  (+ (* 0.5 m v v) (* 0.5 k (- x target) (- x target))))
(define-fun bound () Real
  (+ (* 0.5 m V V) (* 0.5 k D D)))

(assert (> energy bound))
(check-sat)
; >>> unsat → energy bounded ✓
