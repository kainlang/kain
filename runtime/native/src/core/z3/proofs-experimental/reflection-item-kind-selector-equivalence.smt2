; Experimental Z3 proof: reflection item-kind selector equivalence.
; Native source seam: reflection_item_kind_from_string.
; Claim: branchless bitwise selector equals exact strcmp-style classifier for all 64-bit token tuples.
(set-logic QF_BV)
(define-fun nonzero-bit ((x (_ BitVec 64))) (_ BitVec 64)
  (bvand (bvlshr (bvor x (bvneg x)) #x000000000000003f) #x0000000000000001))
(define-fun zero-bit ((x (_ BitVec 64))) (_ BitVec 64)
  (bvxor (nonzero-bit x) #x0000000000000001))
(define-fun match-token ((len (_ BitVec 64)) (lo (_ BitVec 64)) (hi (_ BitVec 64)) (state (_ BitVec 64))
                         (want_len (_ BitVec 64)) (want_lo (_ BitVec 64)) (want_hi (_ BitVec 64)) (want_state (_ BitVec 64))) (_ BitVec 64)
  (bvand (zero-bit (bvxor len want_len))
    (bvand (zero-bit (bvxor lo want_lo))
      (bvand (zero-bit (bvxor hi want_hi)) (zero-bit (bvxor state want_state))))))
(declare-const len (_ BitVec 64))
(declare-const lo (_ BitVec 64))
(declare-const hi (_ BitVec 64))
(declare-const state (_ BitVec 64))
; token function len=8 lo=#x6e6f6974636e7566 hi=#x0000000000000000 state=#xd6a68da987f03e7a -> 1
; token struct len=6 lo=#x0000746375727473 hi=#x0000000000000000 state=#x85e2349084f91fcd -> 2
; token enum len=4 lo=#x000000006d756e65 hi=#x0000000000000000 state=#x90375b34f50a79ea -> 3
; token actor len=5 lo=#x000000726f746361 hi=#x0000000000000000 state=#x7f9eb4e3bc9d4474 -> 4
; token component len=9 lo=#x6e656e6f706d6f63 hi=#x0000000000000074 state=#xd56fea0c726645b2 -> 5
; token message len=7 lo=#x006567617373656d hi=#x0000000000000000 state=#xd2f837f41e8abcb6 -> 6
; token service len=7 lo=#x0065636976726573 hi=#x0000000000000000 state=#x730d81792e48e264 -> 7
; token module len=6 lo=#x0000656c75646f6d hi=#x0000000000000000 state=#xc32071eee8a4630a -> 8
(define-fun branchless () (_ BitVec 64) (bvor (bvmul (match-token len lo hi state #x0000000000000008 #x6e6f6974636e7566 #x0000000000000000 #xd6a68da987f03e7a) #x0000000000000001) (bvmul (match-token len lo hi state #x0000000000000006 #x0000746375727473 #x0000000000000000 #x85e2349084f91fcd) #x0000000000000002) (bvmul (match-token len lo hi state #x0000000000000004 #x000000006d756e65 #x0000000000000000 #x90375b34f50a79ea) #x0000000000000003) (bvmul (match-token len lo hi state #x0000000000000005 #x000000726f746361 #x0000000000000000 #x7f9eb4e3bc9d4474) #x0000000000000004) (bvmul (match-token len lo hi state #x0000000000000009 #x6e656e6f706d6f63 #x0000000000000074 #xd56fea0c726645b2) #x0000000000000005) (bvmul (match-token len lo hi state #x0000000000000007 #x006567617373656d #x0000000000000000 #xd2f837f41e8abcb6) #x0000000000000006) (bvmul (match-token len lo hi state #x0000000000000007 #x0065636976726573 #x0000000000000000 #x730d81792e48e264) #x0000000000000007) (bvmul (match-token len lo hi state #x0000000000000006 #x0000656c75646f6d #x0000000000000000 #xc32071eee8a4630a) #x0000000000000008)))
(define-fun reference () (_ BitVec 64) (ite (= (match-token len lo hi state #x0000000000000008 #x6e6f6974636e7566 #x0000000000000000 #xd6a68da987f03e7a) #x0000000000000001) #x0000000000000001 (ite (= (match-token len lo hi state #x0000000000000006 #x0000746375727473 #x0000000000000000 #x85e2349084f91fcd) #x0000000000000001) #x0000000000000002 (ite (= (match-token len lo hi state #x0000000000000004 #x000000006d756e65 #x0000000000000000 #x90375b34f50a79ea) #x0000000000000001) #x0000000000000003 (ite (= (match-token len lo hi state #x0000000000000005 #x000000726f746361 #x0000000000000000 #x7f9eb4e3bc9d4474) #x0000000000000001) #x0000000000000004 (ite (= (match-token len lo hi state #x0000000000000009 #x6e656e6f706d6f63 #x0000000000000074 #xd56fea0c726645b2) #x0000000000000001) #x0000000000000005 (ite (= (match-token len lo hi state #x0000000000000007 #x006567617373656d #x0000000000000000 #xd2f837f41e8abcb6) #x0000000000000001) #x0000000000000006 (ite (= (match-token len lo hi state #x0000000000000007 #x0065636976726573 #x0000000000000000 #x730d81792e48e264) #x0000000000000001) #x0000000000000007 (ite (= (match-token len lo hi state #x0000000000000006 #x0000656c75646f6d #x0000000000000000 #xc32071eee8a4630a) #x0000000000000001) #x0000000000000008 #x0000000000000000)))))))))
(assert (not (= branchless reference)))
(check-sat)
