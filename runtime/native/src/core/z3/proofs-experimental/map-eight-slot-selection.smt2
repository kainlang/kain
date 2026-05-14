(set-logic QF_BV)

(declare-fun m0 () (_ BitVec 64))
(declare-fun m1 () (_ BitVec 64))
(declare-fun m2 () (_ BitVec 64))
(declare-fun m3 () (_ BitVec 64))
(declare-fun m4 () (_ BitVec 64))
(declare-fun m5 () (_ BitVec 64))
(declare-fun m6 () (_ BitVec 64))
(declare-fun m7 () (_ BitVec 64))
(declare-fun v0 () (_ BitVec 64))
(declare-fun v1 () (_ BitVec 64))
(declare-fun v2 () (_ BitVec 64))
(declare-fun v3 () (_ BitVec 64))
(declare-fun v4 () (_ BitVec 64))
(declare-fun v5 () (_ BitVec 64))
(declare-fun v6 () (_ BitVec 64))
(declare-fun v7 () (_ BitVec 64))

(define-fun bit ((m (_ BitVec 64))) Bool
  (or (= m #x0000000000000000) (= m #x0000000000000001)))

(assert (and (bit m0) (bit m1) (bit m2) (bit m3) (bit m4) (bit m5) (bit m6) (bit m7)))

(define-fun pop () (_ BitVec 64)
  (bvadd m0 m1 m2 m3 m4 m5 m6 m7))

(assert (bvule pop #x0000000000000001))

(define-fun mask ((m (_ BitVec 64))) (_ BitVec 64)
  (bvsub #x0000000000000000 m))

(define-fun selected () (_ BitVec 64)
  (bvor
    (bvor
      (bvor (bvand v0 (mask m0)) (bvand v1 (mask m1)))
      (bvor (bvand v2 (mask m2)) (bvand v3 (mask m3))))
    (bvor
      (bvor (bvand v4 (mask m4)) (bvand v5 (mask m5)))
      (bvor (bvand v6 (mask m6)) (bvand v7 (mask m7))))))

(assert
  (not
    (and
      (=> (= pop #x0000000000000000) (= selected #x0000000000000000))
      (=> (= m0 #x0000000000000001) (= selected v0))
      (=> (= m1 #x0000000000000001) (= selected v1))
      (=> (= m2 #x0000000000000001) (= selected v2))
      (=> (= m3 #x0000000000000001) (= selected v3))
      (=> (= m4 #x0000000000000001) (= selected v4))
      (=> (= m5 #x0000000000000001) (= selected v5))
      (=> (= m6 #x0000000000000001) (= selected v6))
      (=> (= m7 #x0000000000000001) (= selected v7)))))

(check-sat)

