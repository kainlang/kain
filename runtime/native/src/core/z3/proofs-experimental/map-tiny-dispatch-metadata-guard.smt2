(set-logic QF_LIA)

(declare-fun s0 () Int)
(declare-fun s1 () Int)
(declare-fun s2 () Int)
(declare-fun s3 () Int)
(declare-fun q () Int)
(declare-fun m0 () Bool)
(declare-fun m1 () Bool)
(declare-fun m2 () Bool)
(declare-fun m3 () Bool)

(assert (<= 0 s0))
(assert (< s0 64))
(assert (<= 0 s1))
(assert (< s1 64))
(assert (<= 0 s2))
(assert (< s2 64))
(assert (<= 0 s3))
(assert (< s3 64))
(assert (<= 0 q))
(assert (< q 64))

(assert (distinct s0 s1 s2 s3))

; If the query is an exact key match for entry i, the tiny fingerprint must land
; on that entry's unique dispatch slot.
(assert (=> m0 (= q s0)))
(assert (=> m1 (= q s1)))
(assert (=> m2 (= q s2)))
(assert (=> m3 (= q s3)))

; Distinct slots mean at most one exact match can hold for a single query.
(assert (not (and m0 m1)))
(assert (not (and m0 m2)))
(assert (not (and m0 m3)))
(assert (not (and m1 m2)))
(assert (not (and m1 m3)))
(assert (not (and m2 m3)))

(define-fun selected_index () Int
  (ite (= q s0) 0
    (ite (= q s1) 1
      (ite (= q s2) 2
        (ite (= q s3) 3
          255)))))

(define-fun returns_match () Bool
  (ite (= selected_index 0) m0
    (ite (= selected_index 1) m1
      (ite (= selected_index 2) m2
        (ite (= selected_index 3) m3
          false)))))

(assert
  (or
    (and m0 (or (not (= selected_index 0)) (not returns_match)))
    (and m1 (or (not (= selected_index 1)) (not returns_match)))
    (and m2 (or (not (= selected_index 2)) (not returns_match)))
    (and m3 (or (not (= selected_index 3)) (not returns_match)))
    (and (not m0) (not m1) (not m2) (not m3) returns_match)))

(check-sat)
