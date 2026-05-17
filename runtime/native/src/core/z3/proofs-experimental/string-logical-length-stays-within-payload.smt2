(set-logic QF_BV)

(declare-fun payload_size () (_ BitVec 64))
(declare-fun logical_length () (_ BitVec 64))

(assert (bvuge payload_size (_ bv1 64)))
(assert (bvule logical_length (bvsub payload_size (_ bv1 64))))

; If the cached logical string length never exceeds payload_size - 1,
; then a memcmp over logical_length bytes stays inside the RC payload.
(assert (not (bvule (bvadd logical_length (_ bv1 64)) payload_size)))

(check-sat)
