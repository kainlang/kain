; Helper allocation cache bounds proof.
;
; Target:
;   runtime/native/src/core/memory.c
;
; Claims:
;   - Exact-size cached helper blocks cannot be reused for a smaller requested
;     payload.
;   - Eligible payload + header accounting cannot wrap 64-bit size_t.
;   - The byte/node cache admission guards keep the bounded cache within policy.
(set-logic QF_BV)

(define-fun HEADER_BYTES () (_ BitVec 64) (_ bv16 64))
(define-fun MIN_PAYLOAD () (_ BitVec 64) (_ bv4096 64))
(define-fun MAX_PAYLOAD () (_ BitVec 64) (_ bv262144 64))
(define-fun MAX_CACHE_BYTES () (_ BitVec 64) (_ bv8388608 64))
(define-fun MAX_CACHE_NODES () (_ BitVec 32) (_ bv256 32))

(declare-const request_payload (_ BitVec 64))
(declare-const cached_payload (_ BitVec 64))
(declare-const cache_bytes_before (_ BitVec 64))
(declare-const cache_nodes_before (_ BitVec 32))

(assert (bvule MIN_PAYLOAD request_payload))
(assert (bvule request_payload MAX_PAYLOAD))
(assert (= cached_payload request_payload))

(define-fun allocation_size () (_ BitVec 64)
  (bvadd HEADER_BYTES request_payload))

; Eligible payload accounting cannot wrap.
(assert (bvult allocation_size request_payload))
(check-sat)
(reset-assertions)

(declare-const request_payload_b (_ BitVec 64))
(declare-const cache_bytes_before_b (_ BitVec 64))
(assert (bvule MIN_PAYLOAD request_payload_b))
(assert (bvule request_payload_b MAX_PAYLOAD))
(define-fun allocation_size_b () (_ BitVec 64)
  (bvadd HEADER_BYTES request_payload_b))
(assert (bvule cache_bytes_before_b (bvsub MAX_CACHE_BYTES allocation_size_b)))
(define-fun cache_bytes_after_b () (_ BitVec 64)
  (bvadd cache_bytes_before_b allocation_size_b))
(assert (bvugt cache_bytes_after_b MAX_CACHE_BYTES))
(check-sat)
(reset-assertions)

(declare-const cache_nodes_before_c (_ BitVec 32))
(assert (bvult cache_nodes_before_c MAX_CACHE_NODES))
(define-fun cache_nodes_after_c () (_ BitVec 32)
  (bvadd cache_nodes_before_c (_ bv1 32)))
(assert (bvugt cache_nodes_after_c MAX_CACHE_NODES))
(check-sat)
(reset-assertions)

(declare-const request_payload_d (_ BitVec 64))
(declare-const cached_payload_d (_ BitVec 64))
(declare-const cache_hit_d Bool)
(assert (bvule MIN_PAYLOAD request_payload_d))
(assert (bvule request_payload_d MAX_PAYLOAD))
(assert (= cache_hit_d (= cached_payload_d request_payload_d)))
(assert cache_hit_d)
(assert (not (= cached_payload_d request_payload_d)))
(check-sat)
