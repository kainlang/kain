; actor-reply-port-copy-bounds.smt2
; The generic reply-port wait helper copies at most min(reply_size, out_capacity)
; bytes from the stored mailbox payload into the caller-owned output slot.
; This proof shows the selected byte count never exceeds either bound.

(set-logic QF_BV)

(declare-fun reply_size () (_ BitVec 64))
(declare-fun out_capacity () (_ BitVec 64))

(define-fun bytes_to_copy () (_ BitVec 64)
  (ite (bvult reply_size out_capacity) reply_size out_capacity))

(assert
  (not
    (and
      (bvule bytes_to_copy reply_size)
      (bvule bytes_to_copy out_capacity))))

(check-sat)
