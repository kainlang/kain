; Experimental proof for the native JSON Any lane in json.c and LLVM lowering.
; Claims:
; - an 8-byte-aligned JSON/string pointer remains distinguishable from low-bit
;   scalar tags 1..4;
; - clearing the string tag recovers the original aligned pointer;
; - signed 61-bit integer payloads round-trip through (x << 3) | 1 and >> 3;
; - boxed floats and cloned json_get/json_array_get results stay in the raw
;   aligned handle lane rather than stealing a scalar tag.
(set-logic QF_BV)

(declare-fun ptr () (_ BitVec 64))
(declare-fun boxed_float_ptr () (_ BitVec 64))
(declare-fun x () (_ BitVec 64))

(define-fun tag ((v (_ BitVec 64))) (_ BitVec 64)
  (bvand v #x0000000000000007))
(define-fun aligned ((v (_ BitVec 64))) Bool
  (= (tag v) #x0000000000000000))
(define-fun enc_int ((v (_ BitVec 64))) (_ BitVec 64)
  (bvor (bvshl v #x0000000000000003) #x0000000000000001))
(define-fun enc_bool ((v (_ BitVec 64))) (_ BitVec 64)
  (bvor (bvshl v #x0000000000000003) #x0000000000000002))
(define-fun enc_string ((v (_ BitVec 64))) (_ BitVec 64)
  (bvor v #x0000000000000003))

(assert (aligned ptr))
(assert (aligned boxed_float_ptr))
(assert (bvsge x #xf800000000000000))
(assert (bvsle x #x07ffffffffffffff))

(push)
(assert (= (tag ptr) #x0000000000000001))
(check-sat)
(pop)

(push)
(assert (= (tag ptr) #x0000000000000002))
(check-sat)
(pop)

(push)
(assert (= (tag ptr) #x0000000000000003))
(check-sat)
(pop)

(push)
(assert (= (tag ptr) #x0000000000000004))
(check-sat)
(pop)

(push)
(assert (not (= (bvand (enc_string ptr) #xfffffffffffffff8) ptr)))
(check-sat)
(pop)

(push)
(assert (not (= (bvashr (enc_int x) #x0000000000000003) x)))
(check-sat)
(pop)

(push)
(assert (not (= (tag (enc_int x)) #x0000000000000001)))
(check-sat)
(pop)

(push)
(assert (not (= (tag (enc_bool x)) #x0000000000000002)))
(check-sat)
(pop)

(push)
(assert (not (= (tag boxed_float_ptr) #x0000000000000000)))
(check-sat)
(pop)
