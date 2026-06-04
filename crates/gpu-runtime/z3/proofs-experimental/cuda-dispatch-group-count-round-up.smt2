; Proves the CUDA/PTX runtime dispatch-group formula is the positive-domain
; ceil-division we intend, while still preserving the legacy "zero means one"
; fallback used by sidecar-free callers.

(set-logic QF_BV)

(declare-const dispatch (_ BitVec 32))
(declare-const workgroup (_ BitVec 32))

(define-fun safe_dispatch32 () (_ BitVec 32)
  (ite (= dispatch #x00000000) #x00000001 dispatch))

(define-fun safe_workgroup32 () (_ BitVec 32)
  (ite (= workgroup #x00000000) #x00000001 workgroup))

(define-fun safe_dispatch () (_ BitVec 64)
  ((_ zero_extend 32) safe_dispatch32))

(define-fun safe_workgroup () (_ BitVec 64)
  ((_ zero_extend 32) safe_workgroup32))

(define-fun runtime_group_count () (_ BitVec 64)
  (bvadd (bvudiv (bvsub safe_dispatch #x0000000000000001) safe_workgroup)
         #x0000000000000001))

(define-fun ceil_div_group_count () (_ BitVec 64)
  (bvudiv (bvadd safe_dispatch (bvsub safe_workgroup #x0000000000000001))
          safe_workgroup))

; The runtime formula must equal ceil(safe_dispatch / safe_workgroup) and the
; result must stay positive.
(assert
  (or
    (distinct runtime_group_count ceil_div_group_count)
    (bvult runtime_group_count #x0000000000000001)))

(check-sat)
