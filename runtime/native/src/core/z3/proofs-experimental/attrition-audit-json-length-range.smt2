(set-logic QF_BV)

; Proof target:
; `kain_attrition_runtime_write_audit_json(...)` in attrition.c returns:
;   - 0                     when snprintf reports an error
;   - capacity - 1         when snprintf would truncate
;   - written              otherwise
;
; Under the only caller-relevant precondition `capacity > 0`, prove that the
; returned length is always strictly less than capacity, so the caller in
; `abi_attrition_capture_write_report(...)` never treats an out-of-range length
; as an in-bounds JSON payload size.

(declare-fun capacity () (_ BitVec 64))
(declare-fun written32 () (_ BitVec 32))

(define-fun written64 () (_ BitVec 64) ((_ zero_extend 32) written32))
(define-fun written_negative () Bool (bvslt written32 (_ bv0 32)))
(define-fun written_truncates () Bool (bvuge written64 capacity))
(define-fun returned_length () (_ BitVec 64)
  (ite written_negative
       (_ bv0 64)
       (ite written_truncates
            (bvsub capacity (_ bv1 64))
            written64)))

; Precondition from attrition.c: the helper returns early when capacity == 0.
(assert (not (= capacity (_ bv0 64))))

; Search for a counterexample where the returned length is not strictly below capacity.
(assert (not (bvult returned_length capacity)))

(check-sat)
