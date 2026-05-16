; Experimental proof for the inline ready-future payload tail in stdlib_abi.c.
; Claim: if a ready future allocates header_size + payload_size bytes and the
; payload begins immediately after the header, every copied payload byte stays
; inside the allocation.
(set-logic QF_BV)

(define-fun header_size () (_ BitVec 64) #x0000000000000020)

(declare-fun payload_size () (_ BitVec 64))
(declare-fun payload_index () (_ BitVec 64))

(assert (bvule payload_size (bvsub #xffffffffffffffff header_size)))
(assert (bvult payload_index payload_size))

(define-fun allocation_size () (_ BitVec 64)
  (bvadd header_size payload_size))

(define-fun payload_byte_offset () (_ BitVec 64)
  (bvadd header_size payload_index))

(assert (not (bvult payload_byte_offset allocation_size)))
(check-sat)
