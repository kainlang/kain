; Kain helper ABI contract check:
; __kain_alloc(size, stride, zeroed) allocates (size * stride) bytes.
; For a single Int cell on the current 64-bit lane, stride = 8 and the only
; bounded element count that yields exactly one Int payload is size = 1.

(set-logic QF_BV)

(define-fun stride_bytes () (_ BitVec 64) #x0000000000000008)
(define-fun single_int_payload_bytes () (_ BitVec 64) #x0000000000000008)

(declare-fun element_count () (_ BitVec 64))
(define-fun payload_bytes () (_ BitVec 64) (bvmul element_count stride_bytes))

; Bounded benchmark-style domain: positive, small, no overflow games.
(assert (bvuge element_count #x0000000000000001))
(assert (bvule element_count #x0000000000000400))
(assert (= payload_bytes single_int_payload_bytes))
(assert (not (= element_count #x0000000000000001)))

(check-sat)
