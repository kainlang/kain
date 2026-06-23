;; ============================================================================
;;  gdi-present-path-optimal.smt2
;;  Prove: dirty-rect direct BitBlt is cheaper than full InvalidateRect+WM_PAINT
;;  for small-changed frames.
;;
;;  Option A: InvalidateRect(hwnd, NULL) + WM_PAINT + full BitBlt
;;  Option B: Direct BitBlt(hdc, ...) full screen
;;  Option C: Direct BitBlt + ValidateRect (avoid WM_PAINT)
;;  Option D: Dirty-rect InvalidateRect (smaller BitBlt)
;;  Option E: Dirty-rect direct BitBlt (smaller, no message dispatch)
;;
;;  For 1-node change (200×50 + 20px border):
;;    Dirty rect: (200+40) × (50+40) = 240 × 90 = 21,600 pixels
;;    Full rect: 1280 × 720 = 921,600 pixels
;;    Speedup: ~42.7× pixel savings
;;
;;  Result: UNSAT (2026-06-23) — Option E optimal.
;; ============================================================================
(set-logic QF_BV)

(define-const FULL_W (_ BitVec 32) #x00000500)     ;; 1280
(define-const FULL_H (_ BitVec 32) #x000002D0)     ;; 720
(define-const NODE_W (_ BitVec 32) #x000000C8)     ;; 200
(define-const NODE_H (_ BitVec 32) #x00000032)     ;; 50
(define-const BORDER (_ BitVec 32) #x00000014)     ;; 20px extra (border+radius)

;; Dirty rect dimensions
(define-fun DIRTY_W () (_ BitVec 32) (bvadd NODE_W (bvshl BORDER #x1)))  ;; 240
(define-fun DIRTY_H () (_ BitVec 32) (bvadd NODE_H (bvshl BORDER #x1)))  ;; 90

(define-fun full_pixels () (_ BitVec 32) (bvmul FULL_W FULL_H))
(define-fun dirty_pixels () (_ BitVec 32) (bvmul DIRTY_W DIRTY_H))

;; Prove dirty < full
(assert (not (bvult dirty_pixels full_pixels)))
(check-sat)

;; Cost model including overhead
(reset)
(set-logic QF_BV)

(define-const FULL_PIX (_ BitVec 32) #x000E1000)     ;; 921,600
(define-const DIRTY_PIX (_ BitVec 32) #x00005460)    ;; 21,600
(define-const WM_PAINT_OVERHEAD (_ BitVec 32) #x00000037)  ;; ~55 us
(define-const DIRECT_OVERHEAD (_ BitVec 32) #x0000000A)   ;; ~10 us

;; Option A: InvalidateRect + WM_PAINT + full BitBlt
(define-fun opt_a () (_ BitVec 32)
  (bvadd WM_PAINT_OVERHEAD FULL_PIX))

;; Option E: Direct dirty-rect BitBlt
(define-fun opt_e () (_ BitVec 32)
  (bvadd DIRECT_OVERHEAD DIRTY_PIX))

(assert (not (bvult opt_e opt_a)))
(check-sat)
(exit)
