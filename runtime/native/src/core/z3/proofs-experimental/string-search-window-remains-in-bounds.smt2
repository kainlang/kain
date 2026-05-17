(set-logic QF_BV)

(declare-fun haystack_len () (_ BitVec 64))
(declare-fun start () (_ BitVec 64))
(declare-fun needle_len () (_ BitVec 64))

(assert (bvule start haystack_len))
(assert (not (= needle_len (_ bv0 64))))

(define-fun remaining () (_ BitVec 64) (bvsub haystack_len start))
(assert (bvule needle_len remaining))

; The memchr search window in kain_find_substring_bytes is
; remaining - needle_len + 1. It must stay inside the remaining haystack.
(define-fun search_window () (_ BitVec 64)
  (bvadd (bvsub remaining needle_len) (_ bv1 64)))

(assert
  (or
    (bvugt search_window remaining)
    (bvugt (bvadd start search_window) haystack_len)))

(check-sat)
