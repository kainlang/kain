(set-logic QF_BV)

(define-fun rotl13 ((x (_ BitVec 64))) (_ BitVec 64)
  (bvor (bvshl x #x000000000000000d) (bvlshr x #x0000000000000033)))

(define-fun rotl27 ((x (_ BitVec 64))) (_ BitVec 64)
  (bvor (bvshl x #x000000000000001b) (bvlshr x #x0000000000000025)))

(define-fun state
  ((w0 (_ BitVec 64)) (w1 (_ BitVec 64)) (w2 (_ BitVec 64)) (w3 (_ BitVec 64)) (len (_ BitVec 64)))
  (_ BitVec 64)
  (let ((m #x64170d358aa115a1))
    (let ((a (bvmul (bvxor w0 len) m))
          (b (bvmul (bvxor w1 (rotl13 m)) #x9e3779b97f4a7c15))
          (c (bvmul (bvxor w2 (rotl27 m)) #xbf58476d1ce4e5b9))
          (d (bvmul (bvxor w3 (bvxor m #x94d049bb133111eb)) #xd6e8feb86659fd93)))
      (let ((x (bvxor (bvxor a b) (bvxor c d))))
        (bvxor
          (bvmul (bvxor x (bvlshr x #x0000000000000021)) #xff51afd7ed558ccd)
          (bvlshr x #x000000000000001d))))))

; Static map keys discovered from current non-third-party .kn map_get/map_set call sites.
(define-fun s0 () (_ BitVec 64) (state #x000000006c6f6f42 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000004)) ; Bool
(define-fun s1 () (_ BitVec 64) (state #x00000074616f6c46 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000005)) ; Float
(define-fun s2 () (_ BitVec 64) (state #x0000000000746e49 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000003)) ; Int
(define-fun s3 () (_ BitVec 64) (state #x0000676e69727453 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000006)) ; String
(define-fun s4 () (_ BitVec 64) (state #x0000000074696e55 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000004)) ; Unit
(define-fun s5 () (_ BitVec 64) (state #x0000006570797464 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000005)) ; dtype
(define-fun s6 () (_ BitVec 64) (state #x0073746e65747865 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000007)) ; extents
(define-fun s7 () (_ BitVec 64) (state #x0000746867696568 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000006)) ; height
(define-fun s8 () (_ BitVec 64) (state #x656c636974726170 #x0000746e756f635f #x0000000000000000 #x0000000000000000 #x000000000000000e)) ; particle_count
(define-fun s9 () (_ BitVec 64) (state #x656c636974726170 #x0000000000000073 #x0000000000000000 #x0000000000000000 #x0000000000000009)) ; particles
(define-fun s10 () (_ BitVec 64) (state #x0000737569646172 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000006)) ; radius
(define-fun s11 () (_ BitVec 64) (state #x00000073676e6972 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000005)) ; rings
(define-fun s12 () (_ BitVec 64) (state #x745f656c706d6173 #x000000006c61746f #x0000000000000000 #x0000000000000000 #x000000000000000c)) ; sample_total
(define-fun s13 () (_ BitVec 64) (state #x736e6f6974636573 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000008)) ; sections
(define-fun s14 () (_ BitVec 64) (state #x0000736472616873 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000006)) ; shards
(define-fun s15 () (_ BitVec 64) (state #x7369766964627573 #x00000000736e6f69 #x0000000000000000 #x0000000000000000 #x000000000000000c)) ; subdivisions
(define-fun s16 () (_ BitVec 64) (state #x00000073746e6576 #x0000000000000000 #x0000000000000000 #x0000000000000000 #x0000000000000005)) ; vents
(define-fun s17 () (_ BitVec 64) (state #x775f776f646e6977 #x0000000068746469 #x0000000000000000 #x0000000000000000 #x000000000000000c)) ; window_width

; Prove no two discovered current keys collide under the deployed magic multiplier.
(assert
  (not
    (distinct s0 s1 s2 s3 s4 s5 s6 s7 s8 s9 s10 s11 s12 s13 s14 s15 s16 s17)))

(check-sat)

