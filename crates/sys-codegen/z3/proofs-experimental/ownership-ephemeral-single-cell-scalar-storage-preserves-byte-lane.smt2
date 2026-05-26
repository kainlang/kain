(set-logic QF_BV)

; Experimental proof for the typed single-cell ephemeral lowering.
; For supported helper-owned single-cell storage widths (1/2/4/8 bytes), the
; new scalar-slot lane must preserve the exact low-byte observation that the
; old [N x i8] cell exposed, while the emitted load/store alignment stays
; clamped to both the access width and the stack slot's guaranteed alignment.

(declare-fun written_value () (_ BitVec 64))
(declare-fun storage_kind () (_ BitVec 2))
(declare-fun access_kind () (_ BitVec 2))

; 00 -> 1 byte, 01 -> 2 bytes, 10 -> 4 bytes, 11 -> 8 bytes.
(define-fun selected_bytes ((kind (_ BitVec 2))) (_ BitVec 64)
  (ite (= kind #b00)
       #x0000000000000001
       (ite (= kind #b01)
            #x0000000000000002
            (ite (= kind #b10)
                 #x0000000000000004
                 #x0000000000000008))))

(define-fun storage_bytes () (_ BitVec 64) (selected_bytes storage_kind))
(define-fun access_bytes () (_ BitVec 64) (selected_bytes access_kind))

(assert (bvule access_bytes storage_bytes))

(define-fun storage_alignment () (_ BitVec 64) storage_bytes)
(define-fun natural_alignment () (_ BitVec 64) access_bytes)
(define-fun lowered_alignment () (_ BitVec 64)
  (ite (bvule natural_alignment storage_alignment)
       natural_alignment
       storage_alignment))

(define-fun b0 () (_ BitVec 8) ((_ extract 7 0) written_value))
(define-fun b1 () (_ BitVec 8) ((_ extract 15 8) written_value))
(define-fun b2 () (_ BitVec 8) ((_ extract 23 16) written_value))
(define-fun b3 () (_ BitVec 8) ((_ extract 31 24) written_value))
(define-fun b4 () (_ BitVec 8) ((_ extract 39 32) written_value))
(define-fun b5 () (_ BitVec 8) ((_ extract 47 40) written_value))
(define-fun b6 () (_ BitVec 8) ((_ extract 55 48) written_value))
(define-fun b7 () (_ BitVec 8) ((_ extract 63 56) written_value))

(define-fun byte_lane_loaded () (_ BitVec 64)
  (ite (= access_kind #b00)
       ((_ zero_extend 56) b0)
       (ite (= access_kind #b01)
            ((_ zero_extend 48) (concat b1 b0))
            (ite (= access_kind #b10)
                 ((_ zero_extend 32) (concat b3 b2 b1 b0))
                 (concat b7 b6 b5 b4 b3 b2 b1 b0)))))

(define-fun scalar_lane_loaded () (_ BitVec 64)
  (ite (= access_kind #b00)
       ((_ zero_extend 56) ((_ extract 7 0) written_value))
       (ite (= access_kind #b01)
            ((_ zero_extend 48) ((_ extract 15 0) written_value))
            (ite (= access_kind #b10)
                 ((_ zero_extend 32) ((_ extract 31 0) written_value))
                 written_value))))

(assert
  (or
    (not (= byte_lane_loaded scalar_lane_loaded))
    (not (= storage_alignment storage_bytes))
    (not (bvule lowered_alignment storage_alignment))
    (not (bvule lowered_alignment natural_alignment))))

(check-sat)
