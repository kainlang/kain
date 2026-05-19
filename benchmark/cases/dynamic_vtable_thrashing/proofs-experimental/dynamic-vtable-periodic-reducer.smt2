; Exploratory proof for the dynamic_vtable_thrashing period reducer.
; Closed domain: kernel_count = 64, value_period = 1009, iterations = 1800000,
; modulus = 1000000007. The Kain LLVM lane preserves the scalar dispatch
; converge spec and folds the deterministic slot/value schedule by lcm(64,1009).
(set-logic QF_NIA)

(define-fun kernel_count () Int 64)
(define-fun value_period () Int 1009)
(define-fun dispatch_period () Int 64576)
(define-fun iterations () Int 1800000)
(define-fun modulus () Int 1000000007)
(define-fun period_sum () Int 2912592385)
(define-fun tail_sum () Int 2545462889)
(define-fun full_cycles () Int (div iterations dispatch_period))
(define-fun tail () Int (mod iterations dispatch_period))
(define-fun folded_acc () Int (mod (+ (* full_cycles period_sum) tail_sum) modulus))

(push)
; Inverted claim: 64 * 1009 is not the dispatch period.
(assert (not (= (* kernel_count value_period) dispatch_period)))
(check-sat)
(pop)

(push)
; Inverted claim: 1800000 does not split into 27 full periods and a 56448 tail.
(assert (not (and (= full_cycles 27) (= tail 56448))))
(check-sat)
(pop)

(push)
; Inverted periodicity: slot and value must repeat after the period for every
; index inside one period. Kind and bias are pure functions of slot, so the
; dispatch score repeats with them.
(declare-const index Int)
(assert (and (>= index 0) (< index dispatch_period)))
(assert
  (not
    (and
      (= (mod index kernel_count) (mod (+ index dispatch_period) kernel_count))
      (= (mod (+ (* index 13) 7) value_period)
         (mod (+ (* (+ index dispatch_period) 13) 7) value_period)))))
(check-sat)
(pop)

(push)
; Inverted final checksum claim from the period and tail sums computed by the
; finite-domain reducer.
(assert (not (= folded_acc 185456717)))
(check-sat)
(pop)
