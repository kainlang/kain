; Proves the fused Python region buffer-view checksum used by
; py_region_buffer_view_checksum37 is equivalent to the per-iteration benchmark
; accounting for the fixed residue schedule (i % 37).
(set-logic LIA)

(declare-const q Int)
(declare-const r Int)
(declare-const base Int)
(declare-const modulus Int)

(assert (>= q 0))
(assert (>= r 0))
(assert (< r 37))
(assert (>= base 0))
(assert (> modulus 0))

(define-fun iterations () Int (+ (* 37 q) r))

; Slow residue prefix for r in [0, 36], equivalent to sum_{i=0}^{r-1} i.
(define-fun prefix_slow ((x Int)) Int
  (ite (= x 0) 0
  (ite (= x 1) 0
  (ite (= x 2) 1
  (ite (= x 3) 3
  (ite (= x 4) 6
  (ite (= x 5) 10
  (ite (= x 6) 15
  (ite (= x 7) 21
  (ite (= x 8) 28
  (ite (= x 9) 36
  (ite (= x 10) 45
  (ite (= x 11) 55
  (ite (= x 12) 66
  (ite (= x 13) 78
  (ite (= x 14) 91
  (ite (= x 15) 105
  (ite (= x 16) 120
  (ite (= x 17) 136
  (ite (= x 18) 153
  (ite (= x 19) 171
  (ite (= x 20) 190
  (ite (= x 21) 210
  (ite (= x 22) 231
  (ite (= x 23) 253
  (ite (= x 24) 276
  (ite (= x 25) 300
  (ite (= x 26) 325
  (ite (= x 27) 351
  (ite (= x 28) 378
  (ite (= x 29) 406
  (ite (= x 30) 435
  (ite (= x 31) 465
  (ite (= x 32) 496
  (ite (= x 33) 528
  (ite (= x 34) 561
  (ite (= x 35) 595
  (ite (= x 36) 630
       666))))))))))))))))))))))))))))))))))))))

(define-fun period_sum () Int 666)
(define-fun prefix_fast () Int (div (* r (- r 1)) 2))

; Slow benchmark: per iteration lane = base + (i % 37), then add one opened
; and one released counter per iteration.
(define-fun slow_checksum () Int
  (mod (+ (* iterations base)
          (* q period_sum)
          (prefix_slow r)
          (* 2 iterations))
       modulus))

; Fused benchmark: metadata base is constant, so the loop becomes
; iterations * (base + 2) + closed-form residue sum.
(define-fun fast_checksum () Int
  (mod (+ (* iterations (+ base 2))
          (* q period_sum)
          prefix_fast)
       modulus))

(assert (not (= (prefix_slow r) prefix_fast)))
(check-sat)

(reset)
(set-logic LIA)
(declare-const q Int)
(declare-const r Int)
(declare-const base Int)
(declare-const modulus Int)
(assert (>= q 0))
(assert (>= r 0))
(assert (< r 37))
(assert (>= base 0))
(assert (> modulus 0))
(define-fun iterations () Int (+ (* 37 q) r))
(define-fun prefix_slow ((x Int)) Int
  (ite (= x 0) 0
  (ite (= x 1) 0
  (ite (= x 2) 1
  (ite (= x 3) 3
  (ite (= x 4) 6
  (ite (= x 5) 10
  (ite (= x 6) 15
  (ite (= x 7) 21
  (ite (= x 8) 28
  (ite (= x 9) 36
  (ite (= x 10) 45
  (ite (= x 11) 55
  (ite (= x 12) 66
  (ite (= x 13) 78
  (ite (= x 14) 91
  (ite (= x 15) 105
  (ite (= x 16) 120
  (ite (= x 17) 136
  (ite (= x 18) 153
  (ite (= x 19) 171
  (ite (= x 20) 190
  (ite (= x 21) 210
  (ite (= x 22) 231
  (ite (= x 23) 253
  (ite (= x 24) 276
  (ite (= x 25) 300
  (ite (= x 26) 325
  (ite (= x 27) 351
  (ite (= x 28) 378
  (ite (= x 29) 406
  (ite (= x 30) 435
  (ite (= x 31) 465
  (ite (= x 32) 496
  (ite (= x 33) 528
  (ite (= x 34) 561
  (ite (= x 35) 595
  (ite (= x 36) 630
       666))))))))))))))))))))))))))))))))))))))
(define-fun period_sum () Int 666)
(define-fun prefix_fast () Int (div (* r (- r 1)) 2))
(define-fun slow_checksum () Int
  (mod (+ (* iterations base)
          (* q period_sum)
          (prefix_slow r)
          (* 2 iterations))
       modulus))
(define-fun fast_checksum () Int
  (mod (+ (* iterations (+ base 2))
          (* q period_sum)
          prefix_fast)
       modulus))
(assert (not (= slow_checksum fast_checksum)))
(check-sat)
