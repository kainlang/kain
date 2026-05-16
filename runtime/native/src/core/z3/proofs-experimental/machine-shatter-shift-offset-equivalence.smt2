; Compiler shatter direct-field lowering replaces element_index * 8 with
; element_index << 3. Over the proved in-bounds domain, the shift is both
; multiplication-equivalent and lossless.
(set-logic QF_BV)

(declare-const index (_ BitVec 64))

(define-fun max_lossless_slot_index () (_ BitVec 64) #x1fffffffffffffff)
(define-fun slot_width () (_ BitVec 64) #x0000000000000008)
(define-fun shift_amount () (_ BitVec 64) #x0000000000000003)
(define-fun shifted () (_ BitVec 64) (bvshl index shift_amount))
(define-fun multiplied () (_ BitVec 64) (bvmul index slot_width))

(assert (bvule index max_lossless_slot_index))
(assert
  (not
    (and
      (= shifted multiplied)
      (= (bvlshr shifted shift_amount) index))))

(check-sat)
