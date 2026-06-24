; converge-cache-key-odd-guarantee.smt2
;
; Prove: The tune cache key computed by kain_converge_cache_key is always odd
; (has LSB = 1). This is essential because the search stride is derived from
; this key — an odd stride modulo a power-of-two capacity guarantees the
; probe sequence visits all slots before repeating.
;
; Source: X:/runtime/native/src/core/converge.c
; Function: kain_converge_cache_key (static)
;
; Code:
;   static uint64_t kain_converge_cache_key(uint64_t converge_key, uint64_t shape_key) {
;       uint64_t cpu = abi_cpu_feature_fingerprint();
;       return kain_converge_mix64(converge_key ^ (shape_key + 0x9e3779b97f4a7c15ull) ^ cpu) | 1ull;
;   }
;
; stride = ((key >> 6) | 1ull) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u)
; The | 1ull guarantees the stride is always odd and non-zero, which means
; gcd(stride, 64) = 1, so the probe sequence visits all 64 cache slots.

(set-logic QF_BV)

; Inline the mix64 function from converge.c
(define-fun mix64 ((value (_ BitVec 64))) (_ BitVec 64)
  (let ((a (bvxor (bvadd value #x9e3779b97f4a7c15) (bvlshr (bvadd value #x9e3779b97f4a7c15) (_ bv30 64)))))
  (let ((b (bvmul a #xbf58476d1ce4e5b9)))
  (let ((c (bvxor b (bvlshr b (_ bv27 64)))))
  (let ((d (bvmul c #x94d049bb133111eb)))
  (bvxor d (bvlshr d (_ bv31 64))))))))

(define-fun cache_key ((input (_ BitVec 64))) (_ BitVec 64)
  (bvor (mix64 input) (_ bv1 64)))

(declare-const converge_key (_ BitVec 64))
(declare-const shape_key (_ BitVec 64))
(declare-const cpu_fp (_ BitVec 64))

(define-fun mix_input () (_ BitVec 64)
  (bvxor converge_key (bvxor (bvadd shape_key #x9e3779b97f4a7c15) cpu_fp)))

(define-fun key_result () (_ BitVec 64)
  (cache_key mix_input))

; ── Claim 1: The cache key is always odd (LSB = 1) ──
(push)
(assert (= ((_ extract 0 0) key_result) #b0))
(check-sat)
(pop)
; Expected: unsat — key_result always has LSB=1 ✓

; ── Claim 2: The cache key is never zero ──
(push)
(assert (= key_result (_ bv0 64)))
(check-sat)
(pop)
; Expected: unsat — |1 guarantees non-zero ✓

; ── Claim 3: mix64 actually avalanches (not identity for the magic constant) ──
(push)
(assert (= (bvor (mix64 #x9e3779b97f4a7c15) (_ bv1 64)) (_ bv1 64)))
(check-sat)
(pop)
; Expected: unsat — mix64 changes the value ✓
