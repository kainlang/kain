(set-logic QF_BV)

(declare-fun stride_kind () (_ BitVec 2))
(declare-fun element_count () (_ BitVec 64))
(declare-fun element_index () (_ BitVec 64))

(define-fun selected_stride ((kind (_ BitVec 2))) (_ BitVec 64)
  (ite (= kind #b00)
       #x0000000000000001
       (ite (= kind #b01)
            #x0000000000000002
            (ite (= kind #b10)
                 #x0000000000000004
                 #x0000000000000008))))

(define-fun stride () (_ BitVec 64) (selected_stride stride_kind))

; Helper-local stack erasure only accepts bounded fixed layouts.
(assert (not (= element_count #x0000000000000000)))
(assert (bvule element_count #x0000000000002000))
(assert (bvult element_index element_count))

(define-fun byte_len () (_ BitVec 64) (bvmul element_count stride))
(define-fun byte_offset () (_ BitVec 64) (bvmul element_index stride))
(define-fun typed_slot () (_ BitVec 64) (bvudiv byte_offset stride))
(define-fun typed_remainder () (_ BitVec 64) (bvurem byte_offset stride))
(define-fun access_bytes () (_ BitVec 64) stride)
(define-fun storage_alignment () (_ BitVec 64) stride)
(define-fun lowered_alignment () (_ BitVec 64)
  (ite (bvule access_bytes storage_alignment)
       access_bytes
       storage_alignment))

(assert
  (or
    (not (= typed_slot element_index))
    (not (= typed_remainder #x0000000000000000))
    (not (bvule (bvadd byte_offset access_bytes) byte_len))
    (not (= lowered_alignment stride))))

(check-sat)
