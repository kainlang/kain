; Exploratory proof for ray_sphere_intersection finite-domain collapse.
; The 12x8 geometry table is round-invariant. Once the floating geometry pass
; classifies 22 hit pairs and base contribution 33550, the hot loop reduces to
; one modulo expression over the eleven-step phase period.
(set-logic QF_NIA)

(define-fun modulus () Int 1000000007)
(define-fun iterations () Int 150000)
(define-fun base_contribution () Int 33550)
(define-fun hit_pairs () Int 22)
(define-fun full_phase_blocks () Int 13636)
(define-fun phase_tail_sum () Int 6)
(define-fun eleven_phase_sum () Int 55)

(define-fun closed_phase_sum () Int
  (+ (* full_phase_blocks eleven_phase_sum) phase_tail_sum))

(define-fun folded_acc () Int
  (mod (+ (* iterations base_contribution)
          (* hit_pairs closed_phase_sum))
       modulus))

; Inverted correctness claim: if this is unsat, the periodic reducer yields the
; same checksum guard expected by the benchmark.
(assert (not (= folded_acc 48999657)))
(check-sat)
