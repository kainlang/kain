;; ============================================================
;; Proof: kain_size_add_checked branchless overflow detection
;;
;; Original: if (left > SIZE_MAX - right) return 0; else ...
;; Candidate: sum = left + right; return (sum >= left);
;;
;; For unsigned 64-bit: sum >= left iff no overflow.
;; Because:
;; - No overflow: sum = left + right >= left → true
;; - Overflow: sum = (left + right) mod 2^64 < left → false
;; ============================================================
(set-logic QF_BV)

(declare-const left  (_ BitVec 64))
(declare-const right (_ BitVec 64))

;; Original: explicit check before addition
(define-fun original () Bool
  (not (bvugt left (bvsub #xffffffffffffffff right))))

;; Candidate: add first, detect overflow from result
(define-fun candidate () Bool
  (bvuge (bvadd left right) left))

(assert (not (= original candidate)))
(check-sat)
