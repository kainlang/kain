(set-logic QF_BV)

(declare-const size (_ BitVec 64))
(declare-const stride (_ BitVec 64))

; Runtime precondition: the helper allocation path only reaches the payload
; write after the multiplication guard succeeds.
(define-fun payload_size () (_ BitVec 64) (bvmul size stride))
(assert (= (bvmul size stride) payload_size))

; Search for an input where the emitted allocsize product would disagree with
; the runtime payload size under the same size/stride arguments.
(assert (not (= payload_size (bvmul size stride))))

(check-sat)
