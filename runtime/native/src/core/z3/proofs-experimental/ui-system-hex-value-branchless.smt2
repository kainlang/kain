; Z3 Proof: Branchless abi_ui_hex_value replacement
; Target: X:/runtime/native/src/ui/ui_system.c line 616
;
; Original: 3-branch if/else chain char→hex
; Candidate: arithmetic formula using only bitwise ops + arithmetic
;
; Claim: For all char values (0-255), the branchless formula
; produces exactly the same result as the original.
;
; Branchless C equivalent:
;   unsigned c = (unsigned char)ch;
;   unsigned c48 = c - 48u;                               // c - '0'
;   unsigned c87 = (c | 32u) - 87u;                       // (c|32) - 'a' + 10
;   unsigned id = (unsigned)(c48 <= 9u);
;   unsigned il = (unsigned)(((c | 32u) - 97u) <= 5u);
;   unsigned md = 0u - id;                                 // 0 or 0xFFFFFFFF
;   unsigned ml = 0u - il;                                 // 0 or 0xFFFFFFFF
;   return (int)((c48 & md) | (c87 & ml) | (0xFFFFFFFFu & ~(md | ml)));
;
; Result: unsat (equivalent for all 256 values)

(set-logic QF_BV)

(define-fun orig ((ch (_ BitVec 8))) (_ BitVec 32)
  (ite (and (bvuge ch #x30) (bvule ch #x39))
       ((_ zero_extend 24) (bvsub ch #x30))
       (ite (and (bvuge ch #x61) (bvule ch #x66))
            (bvadd ((_ zero_extend 24) (bvsub ch #x61)) #x0000000a)
            (ite (and (bvuge ch #x41) (bvule ch #x46))
                 (bvadd ((_ zero_extend 24) (bvsub ch #x41)) #x0000000a)
                 #xffffffff))))

(define-fun cand ((ch (_ BitVec 8))) (_ BitVec 32)
  (let ((c ((_ zero_extend 24) ch)))
  (let ((c48 (bvsub c #x00000030))
        (c87 (bvsub (bvor c #x00000020) #x00000057)))
  (let ((id (bvule c48 #x00000009))
        (il (bvule (bvsub (bvor c #x00000020) #x00000061) #x00000005)))
  (let ((md (bvneg (ite id #x00000001 #x00000000)))
        (ml (bvneg (ite il #x00000001 #x00000000))))
    (bvor (bvand c48 md)
          (bvor (bvand c87 ml)
                (bvand #xffffffff (bvnot (bvor md ml))))))))))

(declare-const ch (_ BitVec 8))
(assert (not (= (orig ch) (cand ch))))
(check-sat)
; Expected: unsat
