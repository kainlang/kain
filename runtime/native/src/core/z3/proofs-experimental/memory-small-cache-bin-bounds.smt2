(set-logic QF_BV)

; Prove the direct small-cache bin selector stays within
; [0, KAIN_ALLOC_CACHE_SMALL_BIN_COUNT) for every payload that the fast path
; admits: 16-byte aligned, at least one pointer wide, and no larger than 8192.

(declare-fun payload () (_ BitVec 64))

(define-fun small_bin () (_ BitVec 64)
  (bvsub (bvlshr payload (_ bv4 64)) (_ bv1 64)))

(assert (bvuge payload (_ bv16 64)))
(assert (bvule payload (_ bv8192 64)))
(assert (= (bvand payload (_ bv15 64)) (_ bv0 64)))

; Refute the bad state where the selector escapes the 512-bin table.
(assert (not (bvult small_bin (_ bv512 64))))

(check-sat)
