(set-logic QF_BV)

; Negated safety/equivalence search for the LLVM hot-path rewrite used by
; zero_copy_binary_wire and array_scan:
; - signed i64 / and % by positive powers of two become lshr/and only when the
;   dividend is proven non-negative;
; - packed zero-copy header fields round-trip under the benchmark domains;
; - packet*4 word stores/loads stay inside the 2048-byte fixed buffer.

(define-fun nonneg_i64 ((x (_ BitVec 64))) Bool
  (bvult x #x8000000000000000))

(declare-const x (_ BitVec 64))
(declare-const round (_ BitVec 64))
(declare-const packet (_ BitVec 64))

(define-fun p2_divrem_bad ((v (_ BitVec 64)) (pow (_ BitVec 64)) (mask (_ BitVec 64)) (shift (_ BitVec 64))) Bool
  (or
    (not (= (bvsrem v pow) (bvand v mask)))
    (not (= (bvsdiv v pow) (bvlshr v shift)))))

(define-fun header_seq () (_ BitVec 64)
  (bvadd (bvmul round (_ bv64 64)) packet))
(define-fun header_version () (_ BitVec 64)
  (bvadd (bvand packet (_ bv3 64)) (_ bv1 64)))
(define-fun header_kind () (_ BitVec 64)
  (bvand (bvadd (bvmul packet (_ bv3 64)) round) (_ bv7 64)))
(define-fun header_flags () (_ BitVec 64)
  (bvand (bvadd round packet) (_ bv15 64)))
(define-fun header_route () (_ BitVec 64)
  (bvand (bvadd (bvmul packet (_ bv5 64)) (_ bv7 64)) (_ bv63 64)))
(define-fun header_payload () (_ BitVec 64)
  (bvand (bvadd (bvadd (bvmul header_seq (_ bv13 64)) (bvmul header_route (_ bv17 64))) (_ bv19 64)) (_ bv4095 64)))

(define-fun word0 () (_ BitVec 64)
  (bvadd (bvadd (bvadd (bvshl header_seq (_ bv12 64)) (bvshl header_kind (_ bv8 64))) (bvshl header_flags (_ bv4 64))) header_version))
(define-fun word1 () (_ BitVec 64)
  (bvadd (bvshl header_payload (_ bv7 64)) header_route))
(define-fun word2 () (_ BitVec 64)
  (bvadd (bvadd (bvshl (bvurem header_seq (_ bv97 64)) (_ bv11 64)) (bvshl (bvurem header_payload (_ bv127 64)) (_ bv4 64))) header_flags))

(define-fun header_roundtrip_bad () Bool
  (or
    (not (= (bvand word0 (_ bv15 64)) header_version))
    (not (= (bvand (bvlshr word0 (_ bv4 64)) (_ bv15 64)) header_flags))
    (not (= (bvand (bvlshr word0 (_ bv8 64)) (_ bv15 64)) header_kind))
    (not (= (bvlshr word0 (_ bv12 64)) header_seq))
    (not (= (bvand word1 (_ bv127 64)) header_route))
    (not (= (bvlshr word1 (_ bv7 64)) header_payload))
    (not (= (bvlshr word2 (_ bv11 64)) (bvurem header_seq (_ bv97 64))))))

(define-fun byte_end_for_word ((word_index (_ BitVec 64))) (_ BitVec 64)
  (bvadd (bvshl word_index (_ bv3 64)) (_ bv7 64)))

(define-fun buffer_bounds_bad () Bool
  (let ((base (bvshl packet (_ bv2 64))))
    (or
      (not (bvult (byte_end_for_word base) (_ bv2048 64)))
      (not (bvult (byte_end_for_word (bvadd base (_ bv1 64))) (_ bv2048 64)))
      (not (bvult (byte_end_for_word (bvadd base (_ bv2 64))) (_ bv2048 64)))
      (not (bvult (byte_end_for_word (bvadd base (_ bv3 64))) (_ bv2048 64))))))

(assert
  (or
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv4 64) (_ bv3 64) (_ bv2 64)))
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv8 64) (_ bv7 64) (_ bv3 64)))
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv16 64) (_ bv15 64) (_ bv4 64)))
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv64 64) (_ bv63 64) (_ bv6 64)))
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv128 64) (_ bv127 64) (_ bv7 64)))
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv2048 64) (_ bv2047 64) (_ bv11 64)))
    (and (nonneg_i64 x) (p2_divrem_bad x (_ bv4096 64) (_ bv4095 64) (_ bv12 64)))
    (and (bvult round (_ bv200000 64)) (bvult packet (_ bv64 64)) header_roundtrip_bad)
    (and (bvult packet (_ bv64 64)) buffer_bounds_bad)))

(check-sat)
