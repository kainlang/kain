; Proof: errno-from-status switch can be replaced by table lookup
; Target function: kain_ownership_errno_from_status
; Status values: OK=0, ERR_INVALID=-1, ERR_NOT_FOUND=-2, ERR_CAPACITY=-3,
;   ERR_OBSERVED=-4, ERR_COLLAPSED=-5, ERR_DECAYED=-6, ERR_OVERFLOW=-7,
;   ERR_NOT_OBSERVED=-8, ERR_NOT_COLLAPSED=-9
; Expected errno: 0->0, -1->22(EINVAL), -2->22, -3->12(ENOMEM),
;   -4->16(EBUSY), -5->16, -6->22, -7->12, -8->22, -9->22
; Table index = -status for [0,9]
(set-logic QF_BV)
(declare-const raw_status (_ BitVec 32))
; raw_status in [-9, 0]
(assert (bvsle raw_status (_ bv0 32)))
(assert (bvsge raw_status (_ bv4294967287 32)))

; Reference: original switch statement
(define-fun ref ((s (_ BitVec 32))) (_ BitVec 32)
  (ite (= s (_ bv0 32)) (_ bv0 32)
  (ite (or (= s (_ bv4294967293 32)) (= s (_ bv4294967289 32))) (_ bv12 32)
  (ite (or (= s (_ bv4294967292 32)) (= s (_ bv4294967291 32))) (_ bv16 32)
  (_ bv22 32)))))
; -3 = 0xFFFFFFFD = 4294967293
; -7 = 0xFFFFFFF9 = 4294967289
; -4 = 0xFFFFFFFC = 4294967292
; -5 = 0xFFFFFFFB = 4294967291

; Table index: idx = -raw_status (range 0..9)
(define-fun idx () (_ BitVec 4) ((_ extract 3 0) (bvneg raw_status)))

; Candidate: table lookup
(define-fun candidate () (_ BitVec 32)
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

; Claim: reference and candidate produce same errno for all valid status values
(assert (not (= (ref raw_status) candidate)))
(check-sat)
