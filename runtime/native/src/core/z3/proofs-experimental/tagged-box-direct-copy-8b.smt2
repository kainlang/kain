; Direct 8-byte payload copy proof for native tagged boxes.
; If a payload is stored as 8 contiguous bytes and reloaded little-endian,
; the reconstructed 64-bit value is identical.

(set-logic QF_BV)

(declare-fun payload () (_ BitVec 64))

(define-fun loaded () (_ BitVec 64)
  (concat
    ((_ extract 63 56) payload)
    ((_ extract 55 48) payload)
    ((_ extract 47 40) payload)
    ((_ extract 39 32) payload)
    ((_ extract 31 24) payload)
    ((_ extract 23 16) payload)
    ((_ extract 15 8) payload)
    ((_ extract 7 0) payload)))

(assert (not (= loaded payload)))
(check-sat)
