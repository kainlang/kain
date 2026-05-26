; Experimental proof for the packed two-byte substring fast path used by
; compile_known_length_find_substring_inline_static_two_byte_needle over the
; current benchmark/cases/string_ops/main.kn shape.
;
; Domain:
; - text length is 12 bytes
; - needle length is 2 bytes
; - start index is 0
;
; Candidate:
; - compare each packed 16-bit window against a packed 16-bit needle
; - select the first matching index with one-hot first-match guards
; - return 12 when there is no match
;
; Claim:
; The packed-window selector returns the same first-match index as the readable
; left-to-right substring scan for every possible 12-byte text and 2-byte
; needle.
(set-logic QF_BV)

(declare-fun t0 () (_ BitVec 8))
(declare-fun t1 () (_ BitVec 8))
(declare-fun t2 () (_ BitVec 8))
(declare-fun t3 () (_ BitVec 8))
(declare-fun t4 () (_ BitVec 8))
(declare-fun t5 () (_ BitVec 8))
(declare-fun t6 () (_ BitVec 8))
(declare-fun t7 () (_ BitVec 8))
(declare-fun t8 () (_ BitVec 8))
(declare-fun t9 () (_ BitVec 8))
(declare-fun t10 () (_ BitVec 8))
(declare-fun t11 () (_ BitVec 8))
(declare-fun n0 () (_ BitVec 8))
(declare-fun n1 () (_ BitVec 8))

(define-fun needle16 () (_ BitVec 16) (concat n1 n0))
(define-fun w0 () (_ BitVec 16) (concat t1 t0))
(define-fun w1 () (_ BitVec 16) (concat t2 t1))
(define-fun w2 () (_ BitVec 16) (concat t3 t2))
(define-fun w3 () (_ BitVec 16) (concat t4 t3))
(define-fun w4 () (_ BitVec 16) (concat t5 t4))
(define-fun w5 () (_ BitVec 16) (concat t6 t5))
(define-fun w6 () (_ BitVec 16) (concat t7 t6))
(define-fun w7 () (_ BitVec 16) (concat t8 t7))
(define-fun w8 () (_ BitVec 16) (concat t9 t8))
(define-fun w9 () (_ BitVec 16) (concat t10 t9))
(define-fun w10 () (_ BitVec 16) (concat t11 t10))

(define-fun m0 () Bool (= w0 needle16))
(define-fun m1 () Bool (= w1 needle16))
(define-fun m2 () Bool (= w2 needle16))
(define-fun m3 () Bool (= w3 needle16))
(define-fun m4 () Bool (= w4 needle16))
(define-fun m5 () Bool (= w5 needle16))
(define-fun m6 () Bool (= w6 needle16))
(define-fun m7 () Bool (= w7 needle16))
(define-fun m8 () Bool (= w8 needle16))
(define-fun m9 () Bool (= w9 needle16))
(define-fun m10 () Bool (= w10 needle16))

(define-fun first0 () Bool m0)
(define-fun first1 () Bool (and (not m0) m1))
(define-fun first2 () Bool (and (not m0) (not m1) m2))
(define-fun first3 () Bool (and (not m0) (not m1) (not m2) m3))
(define-fun first4 () Bool (and (not m0) (not m1) (not m2) (not m3) m4))
(define-fun first5 () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) m5))
(define-fun first6 () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) m6))
(define-fun first7 () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) m7))
(define-fun first8 () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) m8))
(define-fun first9 () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) (not m8) m9))
(define-fun first10 () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) (not m8) (not m9) m10))
(define-fun no_match () Bool (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) (not m8) (not m9) (not m10)))

(define-fun packed_result () (_ BitVec 64)
  (bvadd
    (ite first0 (_ bv0 64) (_ bv0 64))
    (ite first1 (_ bv1 64) (_ bv0 64))
    (ite first2 (_ bv2 64) (_ bv0 64))
    (ite first3 (_ bv3 64) (_ bv0 64))
    (ite first4 (_ bv4 64) (_ bv0 64))
    (ite first5 (_ bv5 64) (_ bv0 64))
    (ite first6 (_ bv6 64) (_ bv0 64))
    (ite first7 (_ bv7 64) (_ bv0 64))
    (ite first8 (_ bv8 64) (_ bv0 64))
    (ite first9 (_ bv9 64) (_ bv0 64))
    (ite first10 (_ bv10 64) (_ bv0 64))
    (ite no_match (_ bv12 64) (_ bv0 64))))

(assert
  (not
    (and
      (=> m0 (= packed_result (_ bv0 64)))
      (=> (and (not m0) m1) (= packed_result (_ bv1 64)))
      (=> (and (not m0) (not m1) m2) (= packed_result (_ bv2 64)))
      (=> (and (not m0) (not m1) (not m2) m3) (= packed_result (_ bv3 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) m4) (= packed_result (_ bv4 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) (not m4) m5) (= packed_result (_ bv5 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) m6) (= packed_result (_ bv6 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) m7) (= packed_result (_ bv7 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) m8) (= packed_result (_ bv8 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) (not m8) m9) (= packed_result (_ bv9 64)))
      (=> (and (not m0) (not m1) (not m2) (not m3) (not m4) (not m5) (not m6) (not m7) (not m8) (not m9) m10) (= packed_result (_ bv10 64)))
      (=> no_match (= packed_result (_ bv12 64))))))

(check-sat)
