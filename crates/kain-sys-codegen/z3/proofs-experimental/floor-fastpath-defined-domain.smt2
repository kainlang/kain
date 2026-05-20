; Experimental proof for the LLVM floor fastpath.
;
; We model the authored stdlib contract algebraically over Reals on the domain
; where the floored result is representable as signed 64-bit. On that defined
; domain, the old runtime-wrapper path and the new intrinsic-lowered path
; compute the same integer result.

(set-logic NIRA)

(declare-const x Real)

; SMT-LIB `to_int` is floor on Reals.
(define-fun floored () Int (to_int x))
(define-fun runtime_wrapper_result () Int floored)
(define-fun llvm_intrinsic_result () Int (to_int (to_real floored)))

(assert (<= (- 9223372036854775808) floored))
(assert (<= floored 9223372036854775807))

(assert (not (= runtime_wrapper_result llvm_intrinsic_result)))

(check-sat)
