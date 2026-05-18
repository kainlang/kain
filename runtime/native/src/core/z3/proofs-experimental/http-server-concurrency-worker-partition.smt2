; Proves the fixed client-worker swarm partitions request indexes by
; index = slot + k * batch_size without duplicate ownership.
(set-logic QF_LIA)

(declare-const slot_a Int)
(declare-const slot_b Int)
(declare-const k_a Int)
(declare-const k_b Int)

(define-const batch_size Int 16)

(assert (<= 0 slot_a))
(assert (< slot_a batch_size))
(assert (<= 0 slot_b))
(assert (< slot_b batch_size))
(assert (<= 0 k_a))
(assert (<= 0 k_b))
(assert (= (+ slot_a (* k_a batch_size)) (+ slot_b (* k_b batch_size))))

; Negate uniqueness. If two workers produce the same request index, their
; slots must be identical.
(assert (not (= slot_a slot_b)))

(check-sat)
