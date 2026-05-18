; Proves the in-place HTTP completion scanner cannot underflow the
; Content-Length value slice, and that the checked add guard prevents a
; wrapped required request length.
(set-logic QF_BV)

(declare-fun header_length () (_ BitVec 64))
(declare-fun content_length () (_ BitVec 64))
(declare-fun line_end () (_ BitVec 64))
(declare-fun colon () (_ BitVec 64))

(define-fun umax () (_ BitVec 64) #xffffffffffffffff)
(define-fun value_slice_length () (_ BitVec 64)
  (bvsub (bvsub line_end colon) #x0000000000000001))
(define-fun required_length () (_ BitVec 64)
  (bvadd header_length content_length))

; C path preconditions:
; - colon < line_end, so line_end - colon - 1 is a valid slice length.
; - content_length <= SIZE_MAX - header_length, matching
;   abi_net_size_add_overflow().
(assert (bvult colon line_end))
(assert (bvule content_length (bvsub umax header_length)))

; No slice underflow and no wrapped total length can occur.
(assert
  (not
    (and
      (bvule value_slice_length line_end)
      (bvuge required_length header_length)
      (bvuge required_length content_length))))

(check-sat)
