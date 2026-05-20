; Experimental proof for platform_library.c handle tokens.
;
; Claim: for every encodable platform-library handle, the low 16-bit token
; round-trips to a 0..127 table index, the generation lane round-trips inside
; Kain's positive Int domain, stale generations are rejected, and closed/empty
; slots cannot be accepted by slot_from_handle.
(set-logic QF_BV)

(define-fun INDEX_BITS () (_ BitVec 64) #x0000000000000010)
(define-fun INDEX_MASK () (_ BitVec 64) #x000000000000ffff)
(define-fun MAX_HANDLES () (_ BitVec 64) #x0000000000000080)
(define-fun MAX_INDEX () (_ BitVec 64) #x000000000000007f)
(define-fun INT64_MAX () (_ BitVec 64) #x7fffffffffffffff)
(define-fun MAX_GENERATION () (_ BitVec 64) #x00007fffffffffff)

(declare-const slot_index (_ BitVec 64))
(declare-const generation (_ BitVec 64))
(declare-const slot_generation (_ BitVec 64))
(declare-const slot_closed Bool)
(declare-const os_handle_missing Bool)

(define-fun slot_token () (_ BitVec 64)
  (bvadd slot_index #x0000000000000001))
(define-fun encoded_handle () (_ BitVec 64)
  (bvor (bvshl generation INDEX_BITS) slot_token))
(define-fun decoded_token () (_ BitVec 64)
  (bvand encoded_handle INDEX_MASK))
(define-fun decoded_index () (_ BitVec 64)
  (bvsub decoded_token #x0000000000000001))
(define-fun decoded_generation () (_ BitVec 64)
  (bvlshr encoded_handle INDEX_BITS))

(define-fun encodable_handle () Bool
  (and
    (bvule slot_index MAX_INDEX)
    (bvugt generation #x0000000000000000)
    (bvule generation MAX_GENERATION)
    (bvugt encoded_handle #x0000000000000000)
    (bvule encoded_handle INT64_MAX)))

(define-fun slot_from_handle_accepts () Bool
  (and
    (bvugt encoded_handle #x0000000000000000)
    (bvugt decoded_token #x0000000000000000)
    (bvule decoded_token MAX_HANDLES)
    (bvugt decoded_generation #x0000000000000000)
    (not slot_closed)
    (not os_handle_missing)
    (= slot_generation decoded_generation)))

(assert
  (or
    (and encodable_handle (not (= decoded_token slot_token)))
    (and encodable_handle (not (= decoded_index slot_index)))
    (and encodable_handle (not (= decoded_generation generation)))
    (and encodable_handle (bvugt decoded_index MAX_INDEX))
    (and encodable_handle (not (= slot_generation decoded_generation)) slot_from_handle_accepts)
    (and encodable_handle (or slot_closed os_handle_missing) (= slot_generation decoded_generation) slot_from_handle_accepts)))

(check-sat)
