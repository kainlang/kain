;; ============================================================
;; Proof: kain_map_entry_match_bit — fully branchless
;;
;; Original has a ternary (branch):
;;   exact_match = metadata_match ? (ptr_match || memcmp_match) : 0
;;
;; Candidate (fully branchless):
;;   key_match = ptr_match | memcmp_match  (bitwise OR replaces ||)
;;   exact_match = metadata_match & key_match
;;
;; Key insight: metadata_match is 0 or 1 (boolean).
;; When metadata_match = 0: original returns 0, cand returns 0 & anything = 0
;; When metadata_match = 1: original returns ptr||memcmp, cand returns 1 & (ptr|memcmp)
;; The bitwise OR of 0/1 booleans is equivalent to logical OR.
;; ============================================================
(set-logic QF_BV)

(declare-const metadata_match (_ BitVec 8))  ; 0 or 1
(declare-const ptr_match (_ BitVec 8))       ; 0 or 1
(declare-const memcmp_match (_ BitVec 8))    ; 0 or 1

;; All inputs are boolean (0 or 1)
(assert (or (= metadata_match (_ bv0 8)) (= metadata_match (_ bv1 8))))
(assert (or (= ptr_match (_ bv0 8)) (= ptr_match (_ bv1 8))))
(assert (or (= memcmp_match (_ bv0 8)) (= memcmp_match (_ bv1 8))))

;; Original: branch-laden ternary + logical OR
(define-fun original-res () (_ BitVec 8)
  (ite (= metadata_match (_ bv1 8))
    (ite (or (= ptr_match (_ bv1 8)) (= memcmp_match (_ bv1 8)))
      (_ bv1 8)
      (_ bv0 8))
    (_ bv0 8)))

;; Candidate: branchless bitwise AND + OR
(define-fun candidate-res () (_ BitVec 8)
  (bvand metadata_match (bvor ptr_match memcmp_match)))

;; Prove equivalence
(assert (not (= original-res candidate-res)))
(check-sat)
