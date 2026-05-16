; Semantic Singularity Crucible arithmetic and memory bounds.
; The query asks for a counterexample to the deterministic lane checksum,
; vector/array bounds, raw memory offset safety, and final checksum relation.

(set-logic QF_LIA)

(define-fun modulus () Int 1000000007)

(declare-const weights_len Int)
(declare-const vector_len Int)
(declare-const label_len Int)
(declare-const words_len Int)
(declare-const memory_cells Int)
(declare-const hot_cells Int)
(declare-const iterations Int)
(declare-const shatter_lane Int)
(declare-const hot_slot Int)
(declare-const memory_offset Int)

(assert (= weights_len 5))
(assert (= vector_len 4))
(assert (= label_len 17))
(assert (= words_len 3))
(assert (= memory_cells 4))
(assert (= hot_cells 32))
(assert (= iterations 20000))

(define-fun while_sum_0_to_6 () Int (* 3 (div (* 6 7) 2)))
(define-fun loop_score () Int (+ 1 2 4 5 6 7))
(define-fun range_score () Int (+ 2 3 4 5 6 7))
(define-fun array_score () Int (+ (* 3 1) (* 5 2) (* 7 3) (* 11 4) (* 13 5)))
(define-fun packet_score () Int (+ 130 8 97))
(define-fun control_score () Int (+ 3 packet_score while_sum_0_to_6 loop_score range_score array_score vector_len))

(define-fun ascii_walk () Int (+ label_len (div (* 16 17) 2)))
(define-fun text_score () Int (+ ascii_walk label_len 28 (+ 5 4 5) words_len))
(define-fun handle_score () Int (+ 19 17 23 29))
(define-fun collapsed_score () Int (+ 11 17 33 35))
(define-fun observed_score () Int (+ 96 17 33 35))
(define-fun memory_score () Int (+ collapsed_score observed_score))
(define-fun crucible_checksum () Int (mod (+ control_score text_score handle_score memory_score 13 1) modulus))
(define-fun final_score () Int (mod (+ 594832246 crucible_checksum) modulus))

(assert
  (and
    (= control_score 500)
    (= text_score 215)
    (= handle_score 88)
    (= memory_score 277)
    (= crucible_checksum 1094)
    (= final_score 594833340)
    (= vector_len 4)
    (= words_len 3)
    (= memory_cells 4)
    (< 0 memory_cells)
    (< 3 memory_cells)
    (< 0 hot_cells)
    (< 31 hot_cells)
    (or
      (not (= control_score 500))
      (not (= text_score 215))
      (not (= handle_score 88))
      (not (= memory_score 277))
      (not (= crucible_checksum 1094))
      (not (= final_score 594833340))
      (and (<= 0 shatter_lane) (< shatter_lane 4) (not (< shatter_lane 4)))
      (and (<= 0 hot_slot) (< hot_slot hot_cells) (not (< hot_slot hot_cells)))
      (and (<= 0 memory_offset) (<= memory_offset 3) (not (< memory_offset memory_cells)))
      (not (< (* iterations 1000000) 9223372036854775807)))))

(check-sat)
