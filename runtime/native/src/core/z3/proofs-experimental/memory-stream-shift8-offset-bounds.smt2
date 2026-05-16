; Experimental proof for the memory_stream benchmark fast path.
;
; Domain:
; - element index i satisfies 0 <= i < 262144
; - element stride is 8 bytes
;
; Claims:
; 1. i * 8 is equivalent to i << 3
; 2. the resulting byte offset is always < 2097152 (2 MiB)
;
; This does not prove full pointer provenance. It proves the benchmark's offset
; arithmetic stays in the small, non-overflowing range where a shift-only
; specialization can replace the generic multiply-and-overflow helper path.
(set-logic QF_BV)

(declare-fun i () (_ BitVec 64))

(assert (bvult i #x0000000000040000)) ; 262144

(define-fun offset_mul () (_ BitVec 64) (bvmul i #x0000000000000008))
(define-fun offset_shl () (_ BitVec 64) (bvshl i #x0000000000000003))

(assert
  (or
    (not (= offset_mul offset_shl))
    (not (bvult offset_mul #x0000000000200000)))) ; 2097152

(check-sat)
