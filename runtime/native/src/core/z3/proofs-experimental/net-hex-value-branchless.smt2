;; Proof: abi_net_hex_value branchless replacement
;; 
;; Target: X:/runtime/native/src/core/net_system.c, line 672
;; 
;; Original (3 branches):
;;   if (c >= '0' && c <= '9') return c - '0';
;;   if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
;;   if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
;;   return -1;
;;
;; Branchless replacement:
;;   unsigned int x = (unsigned char)c;
;;   unsigned int is_digit = ((x - 48u) <= 9u);
;;   unsigned int is_letter = ((x - 65u) <= 5u) | ((x - 97u) <= 5u);
;;   unsigned int valid = is_digit | is_letter;
;;   unsigned int val = (x & 0x0fu) + (is_letter * 9u);
;;   unsigned int neg_valid = 0u - valid;
;;   return (int)((val & neg_valid) | ((unsigned int)-1 & ~neg_valid));
;;
;; Key insight: hex chars have specific bit patterns:
;;   '0'-'9' = 0x30-0x39 -> lower nibble = 0-9, is_letter=0 -> val = 0-9
;;   'A'-'F' = 0x41-0x46 -> lower nibble = 1-6, is_letter=1 -> val = 1-6+9 = 10-15
;;   'a'-'f' = 0x61-0x66 -> lower nibble = 1-6, is_letter=1 -> val = 1-6+9 = 10-15
;;   Everything else: valid=0 -> neg_valid=0 -> result = -1
;;
;; Domain: All 256 ASCII character values.
;; Result: Z3 proved UNSAT for all 256 values.

(set-logic QF_BV)
(set-option :produce-models true)

(declare-const c (_ BitVec 8))
(define-fun x () (_ BitVec 32) ((_ zero_extend 24) c))

;; Reference: original 3-branch ladder
(define-fun reference ((v (_ BitVec 32))) (_ BitVec 32)
  (ite (and (bvuge v (_ bv48 32)) (bvule v (_ bv57 32)))
       (bvsub v (_ bv48 32))
       (ite (and (bvuge v (_ bv97 32)) (bvule v (_ bv102 32)))
            (bvadd (bvsub v (_ bv97 32)) (_ bv10 32))
            (ite (and (bvuge v (_ bv65 32)) (bvule v (_ bv70 32)))
                 (bvadd (bvsub v (_ bv65 32)) (_ bv10 32))
                 (_ bv4294967295 32)))))

;; Candidate: branchless using lower nibble + letter*9
(define-fun candidate ((v (_ BitVec 32))) (_ BitVec 32)
  (let ((nibble (bvand v (_ bv15 32)))
        (is_digit (ite (bvule (bvsub v (_ bv48 32)) (_ bv9 32)) (_ bv1 32) (_ bv0 32)))
        (is_letter (ite (or (bvule (bvsub v (_ bv65 32)) (_ bv5 32))
                           (bvule (bvsub v (_ bv97 32)) (_ bv5 32)))
                        (_ bv1 32) (_ bv0 32))))
  (let ((val (bvadd nibble (bvmul is_letter (_ bv9 32))))
        (valid (bvor is_digit is_letter)))
    (let ((neg_valid (bvneg valid)))
      (bvor (bvand val neg_valid)
            (bvand (_ bv4294967295 32) (bvnot neg_valid)))))))

;; Prove equivalence for all c in [0,255]
(push)
(assert (not (= (reference x) (candidate x))))
(check-sat)
(get-model)
(pop)

;; Expected: unsat (equivalent for all 256 char values)
