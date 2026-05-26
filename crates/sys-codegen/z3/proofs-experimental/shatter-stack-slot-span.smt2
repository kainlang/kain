; Closed-lane stack-backed shatter locals keep each field lane in an
; entry-block `[element_count x i64]` buffer and address slot `index` at
; `8 * index` bytes. This proof shows any field payload up to 8 bytes stays
; within the lane buffer whenever `0 <= index < element_count`.

(set-logic QF_LIA)

(declare-const element_count Int)
(declare-const index Int)
(declare-const access_width Int)

(assert (>= element_count 1))
(assert (>= index 0))
(assert (< index element_count))
(assert (>= access_width 1))
(assert (<= access_width 8))

(define-fun byte_offset () Int (* 8 index))
(define-fun lane_span_bytes () Int (* 8 element_count))

; Violation query: a valid slot access would overrun the lane buffer.
(assert (> (+ byte_offset access_width) lane_span_bytes))

(check-sat)
