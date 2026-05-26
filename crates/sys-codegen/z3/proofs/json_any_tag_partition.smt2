(set-logic QF_BV)

(declare-fun int_payload () (_ BitVec 64))
(declare-fun bool_payload () (_ BitVec 64))
(declare-fun string_ptr () (_ BitVec 64))
(declare-fun handle () (_ BitVec 64))

(define-fun low3 ((x (_ BitVec 64))) (_ BitVec 64)
  (bvand x #x0000000000000007))

(define-fun json_int_any () (_ BitVec 64)
  (bvor (bvshl int_payload #x0000000000000003) #x0000000000000001))

(define-fun json_bool_any () (_ BitVec 64)
  (bvor (bvshl bool_payload #x0000000000000003) #x0000000000000002))

(define-fun json_string_any () (_ BitVec 64)
  (bvor string_ptr #x0000000000000003))

; Native JSON handles and heap strings are 8-byte aligned in the runtime, so
; the low tag bits are available for the authored Any lanes.
(assert (= (low3 string_ptr) #x0000000000000000))
(assert (= (low3 handle) #x0000000000000000))

; There is no aligned assignment of payloads/handles where the JSON immediates
; lose their low-tag identity or collide with each other / an aligned handle.
(assert
  (or
    (not (= (low3 json_int_any) #x0000000000000001))
    (not (= (low3 json_bool_any) #x0000000000000002))
    (not (= (low3 json_string_any) #x0000000000000003))
    (= handle json_int_any)
    (= handle json_bool_any)
    (= handle json_string_any)
    (= json_int_any json_bool_any)
    (= json_int_any json_string_any)
    (= json_bool_any json_string_any)))

(check-sat)
