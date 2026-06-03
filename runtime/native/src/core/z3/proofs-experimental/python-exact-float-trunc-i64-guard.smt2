; Python exact-float fast truncation guard for the f64 -> i64 hot lane.
; Runtime contract:
;   if an exact Python float result is finite and
;      -2^63 <= x < 2^63
;   then truncation toward zero fits in signed i64.
;
; The first query finds a witness above the upper guard to show the bound is
; necessary. The second query proves there is no in-guard witness whose
; truncation escapes the signed i64 range.
(set-logic ALL)

(define-fun min_i64_real () Real (- 9223372036854775808.0))
(define-fun max_i64_exclusive_real () Real 9223372036854775808.0)
(define-fun min_i64_int () Int (- 9223372036854775808))
(define-fun max_i64_exclusive_int () Int 9223372036854775808)

(declare-const x Real)

; Python int(float) semantics for finite values: truncate toward zero.
(define-fun py_trunc () Int
  (ite (>= x 0.0)
       (to_int x)
       (- (to_int (- x)))))

; Boundary witness: above the exclusive upper bound, truncation can leave i64.
(push)
(assert (>= x max_i64_exclusive_real))
(assert (>= py_trunc max_i64_exclusive_int))
(check-sat)
(pop)

; Safety proof: inside the guard, truncation cannot leave signed i64.
(push)
(assert (>= x min_i64_real))
(assert (< x max_i64_exclusive_real))
(assert (or (< py_trunc min_i64_int)
            (>= py_trunc max_i64_exclusive_int)))
(check-sat)
(pop)
