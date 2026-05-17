(set-logic QF_BV)

(declare-fun requested () (_ BitVec 64))

(define-fun stack_cap () (_ BitVec 64) (_ bv4096 64))
(define-fun max_cap () (_ BitVec 64) (_ bv1048576 64))
(define-fun selected_cap () (_ BitVec 64)
  (ite (bvule requested stack_cap) stack_cap requested))

(assert (bvuge requested (_ bv1 64)))
(assert (bvule requested max_cap))

; The selected buffer capacity must never undershoot the requested chunk
; and must stay inside the pre-existing 1 MiB runtime cap.
(assert
  (or
    (bvult selected_cap requested)
    (bvugt selected_cap max_cap)))

(check-sat)
