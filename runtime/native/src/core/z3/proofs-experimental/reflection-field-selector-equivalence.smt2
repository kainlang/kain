; Experimental Z3 proof: reflection JSON field selector equivalence.
; Native source seam: reflection_field_from_string.
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
; token name len=4 lo=#x00000000656d616e hi=#x0000000000000000 state=#xbdbe7f7dcdf6ceea -> 1
; token item_id len=7 lo=#x0064695f6d657469 hi=#x0000000000000000 state=#x396873470de3a18d -> 2
; token type_id len=7 lo=#x0064695f65707974 hi=#x0000000000000000 state=#xa2e1fd958c48e7bf -> 3
; token kind len=4 lo=#x00000000646e696b hi=#x0000000000000000 state=#x85f92e94ef70fc1a -> 4
; token size_hint len=9 lo=#x6e69685f657a6973 hi=#x0000000000000074 state=#xd9ab85228acf82d8 -> 5
; token fields len=6 lo=#x000073646c656966 hi=#x0000000000000000 state=#x5606bb346200eebb -> 6
; token module_path len=11 lo=#x705f656c75646f6d hi=#x0000000000687461 state=#xdaa9949d1f3d885d -> 7
; token schema_version len=14 lo=#x765f616d65686373 hi=#x00006e6f69737265 state=#xcef0f89d5f9114e2 -> 8
; token types len=5 lo=#x0000007365707974 hi=#x0000000000000000 state=#xec24f923c8feccea -> 9
; token items len=5 lo=#x000000736d657469 hi=#x0000000000000000 state=#xd72884d7aee6376c -> 10
; token actors len=6 lo=#x000073726f746361 hi=#x0000000000000000 state=#x6db01821db700c91 -> 11
; token components len=10 lo=#x6e656e6f706d6f63 hi=#x0000000000007374 state=#xd48f0b56436d2bf4 -> 12
; token messages len=8 lo=#x736567617373656d hi=#x0000000000000000 state=#xc2fd8edf6348077f -> 13
(define-fun branchless () (_ BitVec 64) (bvor (bvmul (match-token len lo hi state #x0000000000000004 #x00000000656d616e #x0000000000000000 #xbdbe7f7dcdf6ceea) #x0000000000000001) (bvmul (match-token len lo hi state #x0000000000000007 #x0064695f6d657469 #x0000000000000000 #x396873470de3a18d) #x0000000000000002) (bvmul (match-token len lo hi state #x0000000000000007 #x0064695f65707974 #x0000000000000000 #xa2e1fd958c48e7bf) #x0000000000000003) (bvmul (match-token len lo hi state #x0000000000000004 #x00000000646e696b #x0000000000000000 #x85f92e94ef70fc1a) #x0000000000000004) (bvmul (match-token len lo hi state #x0000000000000009 #x6e69685f657a6973 #x0000000000000074 #xd9ab85228acf82d8) #x0000000000000005) (bvmul (match-token len lo hi state #x0000000000000006 #x000073646c656966 #x0000000000000000 #x5606bb346200eebb) #x0000000000000006) (bvmul (match-token len lo hi state #x000000000000000b #x705f656c75646f6d #x0000000000687461 #xdaa9949d1f3d885d) #x0000000000000007) (bvmul (match-token len lo hi state #x000000000000000e #x765f616d65686373 #x00006e6f69737265 #xcef0f89d5f9114e2) #x0000000000000008) (bvmul (match-token len lo hi state #x0000000000000005 #x0000007365707974 #x0000000000000000 #xec24f923c8feccea) #x0000000000000009) (bvmul (match-token len lo hi state #x0000000000000005 #x000000736d657469 #x0000000000000000 #xd72884d7aee6376c) #x000000000000000a) (bvmul (match-token len lo hi state #x0000000000000006 #x000073726f746361 #x0000000000000000 #x6db01821db700c91) #x000000000000000b) (bvmul (match-token len lo hi state #x000000000000000a #x6e656e6f706d6f63 #x0000000000007374 #xd48f0b56436d2bf4) #x000000000000000c) (bvmul (match-token len lo hi state #x0000000000000008 #x736567617373656d #x0000000000000000 #xc2fd8edf6348077f) #x000000000000000d)))
(define-fun reference () (_ BitVec 64) (ite (= (match-token len lo hi state #x0000000000000004 #x00000000656d616e #x0000000000000000 #xbdbe7f7dcdf6ceea) #x0000000000000001) #x0000000000000001 (ite (= (match-token len lo hi state #x0000000000000007 #x0064695f6d657469 #x0000000000000000 #x396873470de3a18d) #x0000000000000001) #x0000000000000002 (ite (= (match-token len lo hi state #x0000000000000007 #x0064695f65707974 #x0000000000000000 #xa2e1fd958c48e7bf) #x0000000000000001) #x0000000000000003 (ite (= (match-token len lo hi state #x0000000000000004 #x00000000646e696b #x0000000000000000 #x85f92e94ef70fc1a) #x0000000000000001) #x0000000000000004 (ite (= (match-token len lo hi state #x0000000000000009 #x6e69685f657a6973 #x0000000000000074 #xd9ab85228acf82d8) #x0000000000000001) #x0000000000000005 (ite (= (match-token len lo hi state #x0000000000000006 #x000073646c656966 #x0000000000000000 #x5606bb346200eebb) #x0000000000000001) #x0000000000000006 (ite (= (match-token len lo hi state #x000000000000000b #x705f656c75646f6d #x0000000000687461 #xdaa9949d1f3d885d) #x0000000000000001) #x0000000000000007 (ite (= (match-token len lo hi state #x000000000000000e #x765f616d65686373 #x00006e6f69737265 #xcef0f89d5f9114e2) #x0000000000000001) #x0000000000000008 (ite (= (match-token len lo hi state #x0000000000000005 #x0000007365707974 #x0000000000000000 #xec24f923c8feccea) #x0000000000000001) #x0000000000000009 (ite (= (match-token len lo hi state #x0000000000000005 #x000000736d657469 #x0000000000000000 #xd72884d7aee6376c) #x0000000000000001) #x000000000000000a (ite (= (match-token len lo hi state #x0000000000000006 #x000073726f746361 #x0000000000000000 #x6db01821db700c91) #x0000000000000001) #x000000000000000b (ite (= (match-token len lo hi state #x000000000000000a #x6e656e6f706d6f63 #x0000000000007374 #xd48f0b56436d2bf4) #x0000000000000001) #x000000000000000c (ite (= (match-token len lo hi state #x0000000000000008 #x736567617373656d #x0000000000000000 #xc2fd8edf6348077f) #x0000000000000001) #x000000000000000d #x0000000000000000))))))))))))))
(assert (not (= branchless reference)))
(check-sat)
