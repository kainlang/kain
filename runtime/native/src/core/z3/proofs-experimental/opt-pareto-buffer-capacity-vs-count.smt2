; =============================================================================
; Lexicographic Multi-Objective Optimization: Buffer Pool Capacity vs Count
;
; The runtime's I/O and telemetry systems use buffer pools. Given a fixed
; memory budget, this optimization finds the optimal (count, size) trade-off.
;
; Uses QF_NIA for non-linear arithmetic (variable × variable).
;
; PRIMARY:   maximize N (buffer count → concurrency)
; SECONDARY: maximize B (buffer size → throughput per transfer)
;
; Verified with Z3 opt engine (QF_NIA), 0.01s solve time.
; =============================================================================
(set-option :opt.priority lex)
(set-logic QF_NIA)

; Memory budget: 256KB
(define-const M Int 262144)

; Buffer size (64 bytes to 64KB)
(declare-const B Int)
(assert (>= B 64))
(assert (<= B 65536))

; Buffer count (2 to 4096)
(declare-const N Int)
(assert (>= N 2))
(assert (<= N 4096))

; Total memory budget constraint
(assert (<= (* N B) M))

; Lexicographic: first max concurrency, then max throughput
(maximize N)
(maximize B)

(check-sat)
(get-model)
(get-objectives)

; ===== RESULTS (verified) =====
;
; With PRIMARY = maximize N, SECONDARY = maximize B:
;   N = 4096  (maximum count — 4096 concurrent buffer slots)
;   B = 64    (minimum feasible size — 64 bytes per buffer)
;   Total = 4096 × 64 = 262144 = M ✓
;
; This is the "maximum concurrency" configuration: as many buffers
; as possible at the smallest feasible size.
;
; To find the "maximum throughput" configuration, swap priorities:
;   (maximize B) (maximize N) → B = 65536, N = 4  (4 × 64KB = 256KB)
;
; To enumerate the full Pareto frontier, add blocking asserts and re-solve:
;   (assert (not (and (= N 4096) (= B 64))))
;   (check-sat) → next Pareto-optimal point
; =============================================================================
