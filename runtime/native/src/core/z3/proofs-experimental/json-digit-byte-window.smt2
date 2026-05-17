; Prove the branchless JSON digit window used by benchmark/cases/json_manual_roundtrip:
; digit = byte - '0'; digit <= 9  <=>  '0' <= byte <= '9'
(set-logic QF_BV)

(declare-fun byte_value () (_ BitVec 8))

(define-fun ascii_zero () (_ BitVec 8) #x30)
(define-fun ascii_nine () (_ BitVec 8) #x39)
(define-fun digit_window () (_ BitVec 8) (bvsub byte_value ascii_zero))

(define-fun branchless_ascii_digit () Bool
  (bvule digit_window #x09))

(define-fun ranged_ascii_digit () Bool
  (and (bvuge byte_value ascii_zero) (bvule byte_value ascii_nine)))

(assert (xor branchless_ascii_digit ranged_ascii_digit))
(check-sat)
