; Exploratory proof for the array_scan finite-domain reducer.
; Closed domain: VALUES = [1,2,3,4,5,6,7,8], ITERATIONS = 500000,
; MODULUS = 1000000007. The scalar inner loop contributes the same weighted
; sum each round, and the only round-varying term is i % 7.
(set-logic QF_NIA)

(define-fun weighted_inner () Int
  (+ (* 1 1) (* 2 2) (* 3 3) (* 4 4) (* 5 5) (* 6 6) (* 7 7) (* 8 8)))
(define-fun iterations () Int 500000)
(define-fun modulus () Int 1000000007)
(define-fun residue_period () Int 7)
(define-fun full_cycles () Int (div iterations residue_period))
(define-fun tail () Int (mod iterations residue_period))
(define-fun residue_period_sum () Int (+ 0 1 2 3 4 5 6))
(define-fun tail_residue_sum () Int (div (* tail (- tail 1)) 2))
(define-fun folded_unmod () Int
  (+ (* full_cycles (+ (* weighted_inner residue_period) residue_period_sum))
     (* tail weighted_inner)
     tail_residue_sum))
(define-fun folded_acc () Int (mod folded_unmod modulus))

(push)
; Inverted claim: the literal weighted scan sum is not 204.
(assert (not (= weighted_inner 204)))
(check-sat)
(pop)

(push)
; Inverted claim: the seven-round residue cycle is not 21.
(assert (not (= residue_period_sum 21)))
(check-sat)
(pop)

(push)
; Inverted claim: 500000 does not split into 71428 full seven-round cycles
; plus a four-round tail.
(assert (not (and (= full_cycles 71428) (= tail 4))))
(check-sat)
(pop)

(push)
; Inverted claim: the tail residue sum for residues 0,1,2,3 is not 6.
(assert (not (= tail_residue_sum 6)))
(check-sat)
(pop)

(push)
; Inverted claim: the scalar loop can wrap the modulus in this benchmark.
(assert (not (< folded_unmod modulus)))
(check-sat)
(pop)

(push)
; Inverted correctness claim: if unsat, the periodic reducer yields the same
; checksum guard as the scalar benchmark contract.
(assert (not (= folded_acc 103499994)))
(check-sat)
(pop)

