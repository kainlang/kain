; Experimental Z3 proof: native UI flag selector.
; Native source seam: kain_native_ui_flag_info.
; Claim: branchless flag bit and visible-bit selectors equal exact string classifier semantics.
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
; token hidden len=6 lo=#x00006e6564646968 hi=#x0000000000000000 state=#x85daa81451a55c7a -> bit=1 visible=0
; token visible len=7 lo=#x00656c6269736976 hi=#x0000000000000000 state=#x7f0f01206f964b92 -> bit=1 visible=1
; token focusable len=9 lo=#x6c62617375636f66 hi=#x0000000000000065 state=#x7a75024eba4e101f -> bit=2 visible=0
; token interactive len=11 lo=#x7463617265746e69 hi=#x0000000000657669 state=#x948038e6c1c6ea72 -> bit=4 visible=0
; token disabled len=8 lo=#x64656c6261736964 hi=#x0000000000000000 state=#x4f87286f47c95184 -> bit=8 visible=0
; token hovered len=7 lo=#x0064657265766f68 hi=#x0000000000000000 state=#x13bef354dde61301 -> bit=16 visible=0
; token pressed len=7 lo=#x0064657373657270 hi=#x0000000000000000 state=#x61f59c74a54f9887 -> bit=32 visible=0
(define-fun branchless-bit () (_ BitVec 64) (bvor (bvmul (match-token len lo hi state #x0000000000000006 #x00006e6564646968 #x0000000000000000 #x85daa81451a55c7a) #x0000000000000001) (bvmul (match-token len lo hi state #x0000000000000007 #x00656c6269736976 #x0000000000000000 #x7f0f01206f964b92) #x0000000000000001) (bvmul (match-token len lo hi state #x0000000000000009 #x6c62617375636f66 #x0000000000000065 #x7a75024eba4e101f) #x0000000000000002) (bvmul (match-token len lo hi state #x000000000000000b #x7463617265746e69 #x0000000000657669 #x948038e6c1c6ea72) #x0000000000000004) (bvmul (match-token len lo hi state #x0000000000000008 #x64656c6261736964 #x0000000000000000 #x4f87286f47c95184) #x0000000000000008) (bvmul (match-token len lo hi state #x0000000000000007 #x0064657265766f68 #x0000000000000000 #x13bef354dde61301) #x0000000000000010) (bvmul (match-token len lo hi state #x0000000000000007 #x0064657373657270 #x0000000000000000 #x61f59c74a54f9887) #x0000000000000020)))
(define-fun reference-bit () (_ BitVec 64) (ite (= (match-token len lo hi state #x0000000000000006 #x00006e6564646968 #x0000000000000000 #x85daa81451a55c7a) #x0000000000000001) #x0000000000000001 (ite (= (match-token len lo hi state #x0000000000000007 #x00656c6269736976 #x0000000000000000 #x7f0f01206f964b92) #x0000000000000001) #x0000000000000001 (ite (= (match-token len lo hi state #x0000000000000009 #x6c62617375636f66 #x0000000000000065 #x7a75024eba4e101f) #x0000000000000001) #x0000000000000002 (ite (= (match-token len lo hi state #x000000000000000b #x7463617265746e69 #x0000000000657669 #x948038e6c1c6ea72) #x0000000000000001) #x0000000000000004 (ite (= (match-token len lo hi state #x0000000000000008 #x64656c6261736964 #x0000000000000000 #x4f87286f47c95184) #x0000000000000001) #x0000000000000008 (ite (= (match-token len lo hi state #x0000000000000007 #x0064657265766f68 #x0000000000000000 #x13bef354dde61301) #x0000000000000001) #x0000000000000010 (ite (= (match-token len lo hi state #x0000000000000007 #x0064657373657270 #x0000000000000000 #x61f59c74a54f9887) #x0000000000000001) #x0000000000000020 #x0000000000000000))))))))
(define-fun branchless-visible () (_ BitVec 64) (bvmul (match-token len lo hi state #x0000000000000007 #x00656c6269736976 #x0000000000000000 #x7f0f01206f964b92) #x0000000000000001))
(define-fun reference-visible () (_ BitVec 64) (ite (= (match-token len lo hi state #x0000000000000006 #x00006e6564646968 #x0000000000000000 #x85daa81451a55c7a) #x0000000000000001) #x0000000000000000 (ite (= (match-token len lo hi state #x0000000000000007 #x00656c6269736976 #x0000000000000000 #x7f0f01206f964b92) #x0000000000000001) #x0000000000000001 (ite (= (match-token len lo hi state #x0000000000000009 #x6c62617375636f66 #x0000000000000065 #x7a75024eba4e101f) #x0000000000000001) #x0000000000000000 (ite (= (match-token len lo hi state #x000000000000000b #x7463617265746e69 #x0000000000657669 #x948038e6c1c6ea72) #x0000000000000001) #x0000000000000000 (ite (= (match-token len lo hi state #x0000000000000008 #x64656c6261736964 #x0000000000000000 #x4f87286f47c95184) #x0000000000000001) #x0000000000000000 (ite (= (match-token len lo hi state #x0000000000000007 #x0064657265766f68 #x0000000000000000 #x13bef354dde61301) #x0000000000000001) #x0000000000000000 (ite (= (match-token len lo hi state #x0000000000000007 #x0064657373657270 #x0000000000000000 #x61f59c74a54f9887) #x0000000000000001) #x0000000000000000 #x0000000000000000))))))))
(assert (or (not (= branchless-bit reference-bit)) (not (= branchless-visible reference-visible))))
(check-sat)
