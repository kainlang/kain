(set-logic QF_BV)

; The LLVM byte_at fast path only enters the load block when:
;   text != null
;   index >= 0
;   index < text_len
; with text_len coming from the native len(...) helper, so it is non-negative.
; Prove the signed guard implies the unsigned in-range fact needed for the load.

(declare-fun index () (_ BitVec 64))
(declare-fun text_len () (_ BitVec 64))

(define-fun index_non_negative () Bool
  (bvsge index (_ bv0 64)))

(define-fun text_len_non_negative () Bool
  (bvsge text_len (_ bv0 64)))

(define-fun index_below_len_signed () Bool
  (bvslt index text_len))

(define-fun load_guard () Bool
  (and index_non_negative text_len_non_negative index_below_len_signed))

(assert load_guard)
(assert (not (bvult index text_len)))

(check-sat)
