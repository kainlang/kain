; process-stdio-loop-checksum.smt2
; Prove the benchmark checksum guard stays fixed for 300 rounds of
; len("process-bench\r\n") + (index mod 11).

(set-logic QF_LIA)

(define-fun stdout_len () Int 15)
(define-fun rounds () Int 300)
(define-fun full_cycles () Int 27)
(define-fun cycle_sum () Int 55)
(define-fun tail_sum () Int 3)
(define-fun expected_checksum () Int 5988)

(define-fun computed_checksum () Int
  (+ (* rounds stdout_len)
     (* full_cycles cycle_sum)
     tail_sum))

(assert (not (= computed_checksum expected_checksum)))

(check-sat)
