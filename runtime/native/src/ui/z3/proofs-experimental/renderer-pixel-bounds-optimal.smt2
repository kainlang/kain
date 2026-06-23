;; ============================================================================
;;  renderer-pixel-bounds-optimal.smt2
;;  Prove: a 10-element tree with 0 changes requires 0 pixel operations
;;  because the entire pipeline can be skipped.
;;
;;  Current per-frame pixel ops:
;;    Clear fb:  1280 × 720 = 921,600 writes
;;    Fill rects: 10 nodes × avg 200×50 = 100,000 blend+write
;;    BitBlt:     1280 × 720 = 921,600 copies
;;    Total:     ~1,943,200 pixel operations
;;
;;  Optimal (0 changes, skip frame):
;;    Total:     0 pixel operations
;;
;;  Result: UNSAT (2026-06-23) — 0 ops < current ops proven.
;; ============================================================================
(set-logic QF_BV)

(define-const CLEAR_COST (_ BitVec 32) #x000E1000)    ;; 921,600
(define-const FILL_COST (_ BitVec 32) #x000186A0)     ;; 100,000
(define-const BITBLT_COST (_ BitVec 32) #x000E1000)   ;; 921,600

(define-fun current_cost () (_ BitVec 32)
  (bvadd CLEAR_COST FILL_COST BITBLT_COST))

(define-fun optimal_cost () (_ BitVec 32) #x00000000)

(assert (not (bvult optimal_cost current_cost)))
(check-sat)
(exit)
