; Proof: errno-from-status switch can be replaced by lookup table
; Target function: kain_ownership_errno_from_status
;
; Status values: OK=0, ERR_INVALID=-1, ERR_NOT_FOUND=-2, ERR_CAPACITY=-3,
;   ERR_OBSERVED=-4, ERR_COLLAPSED=-5, ERR_DECAYED=-6, ERR_OVERFLOW=-7,
;   ERR_NOT_OBSERVED=-8, ERR_NOT_COLLAPSED=-9
;
; Table index = -status for [0,9]:
;   0->0, 1->EINVAL(22), 2->22, 3->ENOMEM(12), 4->EBUSY(16),
;   5->16, 6->22, 7->12, 8->22, 9->22
;
; Result: unsat — table lookup equivalent to switch for all status in [-9, 0]
(set-logic QF_BV)
(declare-const s (_ BitVec 32))
; raw_status in [-9, 0]
(assert (bvsle s (_ bv0 32)))
(assert (bvsge s (_ bv4294967287 32))) ; 2^32-9

; Reference: original switch logic
(define-fun ref ((x (_ BitVec 32))) (_ BitVec 32)
  (ite (= x (_ bv0 32)) (_ bv0 32)
  (ite (or (= x (_ bv4294967293 32)) (= x (_ bv4294967289 32))) (_ bv12 32) ; -3, -7 -> ENOMEM
  (ite (or (= x (_ bv4294967292 32)) (= x (_ bv4294967291 32))) (_ bv16 32) ; -4, -5 -> EBUSY
  (_ bv22 32))))) ; everything else -> EINVAL

; Table index
(define-fun idx () (_ BitVec 4) ((_ extract 3 0) (bvneg s)))

; Candidate: table lookup
(define-fun cand () (_ BitVec 32)
  (ite (= idx (_ bv0 4)) (_ bv0 32)
  (ite (= idx (_ bv1 4)) (_ bv22 32)
  (ite (= idx (_ bv2 4)) (_ bv22 32)
  (ite (= idx (_ bv3 4)) (_ bv12 32)
  (ite (= idx (_ bv4 4)) (_ bv16 32)
  (ite (= idx (_ bv5 4)) (_ bv16 32)
  (ite (= idx (_ bv6 4)) (_ bv22 32)
  (ite (= idx (_ bv7 4)) (_ bv12 32)
  (ite (= idx (_ bv8 4)) (_ bv22 32)
  (ite (= idx (_ bv9 4)) (_ bv22 32)
  (_ bv22 32)))))))))))

(assert (not (= (ref s) cand)))
(check-sat)
