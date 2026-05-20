; Experimental proof for the fixed-domain HTTP concurrency benchmark lane in
; runtime/native/src/core/net_system.c.
;
; Claims:
; - The canonical benchmark request and response frames fit the helper's
;   fixed 256-byte stack read buffers.
; - The cached response frame fits the server context's 192-byte frame cache.
; - The benchmark checksum for 240 requests with a 13-byte body is exactly 5695.
(set-logic QF_LIA)

(define-fun request_length () Int 93)
(define-fun response_frame_length () Int 70)
(define-fun response_cache_capacity () Int 192)
(define-fun stack_buffer_capacity () Int 256)
(define-fun rounds () Int 240)
(define-fun body_length () Int 13)
(define-fun modulus () Int 1000000007)
(define-fun expected_checksum () Int 5695)

; 240 = 10 * 23 + 10, sum(0..22) = 253, sum(0..9) = 45.
(define-fun residue_sum_0_to_239_mod_23 () Int (+ (* 10 253) 45))
(define-fun checksum () Int
  (mod (+ (* rounds body_length) residue_sum_0_to_239_mod_23) modulus))

(declare-fun request_offset () Int)
(declare-fun response_offset () Int)

(assert
  (or
    (not (and (> request_length 0) (<= request_length stack_buffer_capacity)))
    (not (and (> response_frame_length 0) (<= response_frame_length stack_buffer_capacity)))
    (not (<= response_frame_length response_cache_capacity))
    (and (<= 0 request_offset) (< request_offset request_length) (>= request_offset stack_buffer_capacity))
    (and (<= 0 response_offset) (< response_offset response_frame_length) (>= response_offset stack_buffer_capacity))
    (not (= checksum expected_checksum))))

(check-sat)
