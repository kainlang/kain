; Experimental proof for the de Bruijn low-bit decoder used by the ownership
; registry occupancy allocator in ownership.c.
; Claim: the top-6-bit hash of every 64-bit one-hot value under
; 0x03f79d71b4cb0a89 is collision-free.
(set-logic QF_BV)

(define-fun hash_00 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv0 64)) #x03f79d71b4cb0a89)))
(define-fun hash_01 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv1 64)) #x03f79d71b4cb0a89)))
(define-fun hash_02 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv2 64)) #x03f79d71b4cb0a89)))
(define-fun hash_03 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv3 64)) #x03f79d71b4cb0a89)))
(define-fun hash_04 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv4 64)) #x03f79d71b4cb0a89)))
(define-fun hash_05 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv5 64)) #x03f79d71b4cb0a89)))
(define-fun hash_06 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv6 64)) #x03f79d71b4cb0a89)))
(define-fun hash_07 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv7 64)) #x03f79d71b4cb0a89)))
(define-fun hash_08 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv8 64)) #x03f79d71b4cb0a89)))
(define-fun hash_09 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv9 64)) #x03f79d71b4cb0a89)))
(define-fun hash_10 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv10 64)) #x03f79d71b4cb0a89)))
(define-fun hash_11 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv11 64)) #x03f79d71b4cb0a89)))
(define-fun hash_12 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv12 64)) #x03f79d71b4cb0a89)))
(define-fun hash_13 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv13 64)) #x03f79d71b4cb0a89)))
(define-fun hash_14 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv14 64)) #x03f79d71b4cb0a89)))
(define-fun hash_15 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv15 64)) #x03f79d71b4cb0a89)))
(define-fun hash_16 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv16 64)) #x03f79d71b4cb0a89)))
(define-fun hash_17 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv17 64)) #x03f79d71b4cb0a89)))
(define-fun hash_18 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv18 64)) #x03f79d71b4cb0a89)))
(define-fun hash_19 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv19 64)) #x03f79d71b4cb0a89)))
(define-fun hash_20 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv20 64)) #x03f79d71b4cb0a89)))
(define-fun hash_21 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv21 64)) #x03f79d71b4cb0a89)))
(define-fun hash_22 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv22 64)) #x03f79d71b4cb0a89)))
(define-fun hash_23 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv23 64)) #x03f79d71b4cb0a89)))
(define-fun hash_24 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv24 64)) #x03f79d71b4cb0a89)))
(define-fun hash_25 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv25 64)) #x03f79d71b4cb0a89)))
(define-fun hash_26 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv26 64)) #x03f79d71b4cb0a89)))
(define-fun hash_27 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv27 64)) #x03f79d71b4cb0a89)))
(define-fun hash_28 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv28 64)) #x03f79d71b4cb0a89)))
(define-fun hash_29 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv29 64)) #x03f79d71b4cb0a89)))
(define-fun hash_30 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv30 64)) #x03f79d71b4cb0a89)))
(define-fun hash_31 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv31 64)) #x03f79d71b4cb0a89)))
(define-fun hash_32 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv32 64)) #x03f79d71b4cb0a89)))
(define-fun hash_33 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv33 64)) #x03f79d71b4cb0a89)))
(define-fun hash_34 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv34 64)) #x03f79d71b4cb0a89)))
(define-fun hash_35 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv35 64)) #x03f79d71b4cb0a89)))
(define-fun hash_36 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv36 64)) #x03f79d71b4cb0a89)))
(define-fun hash_37 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv37 64)) #x03f79d71b4cb0a89)))
(define-fun hash_38 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv38 64)) #x03f79d71b4cb0a89)))
(define-fun hash_39 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv39 64)) #x03f79d71b4cb0a89)))
(define-fun hash_40 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv40 64)) #x03f79d71b4cb0a89)))
(define-fun hash_41 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv41 64)) #x03f79d71b4cb0a89)))
(define-fun hash_42 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv42 64)) #x03f79d71b4cb0a89)))
(define-fun hash_43 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv43 64)) #x03f79d71b4cb0a89)))
(define-fun hash_44 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv44 64)) #x03f79d71b4cb0a89)))
(define-fun hash_45 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv45 64)) #x03f79d71b4cb0a89)))
(define-fun hash_46 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv46 64)) #x03f79d71b4cb0a89)))
(define-fun hash_47 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv47 64)) #x03f79d71b4cb0a89)))
(define-fun hash_48 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv48 64)) #x03f79d71b4cb0a89)))
(define-fun hash_49 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv49 64)) #x03f79d71b4cb0a89)))
(define-fun hash_50 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv50 64)) #x03f79d71b4cb0a89)))
(define-fun hash_51 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv51 64)) #x03f79d71b4cb0a89)))
(define-fun hash_52 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv52 64)) #x03f79d71b4cb0a89)))
(define-fun hash_53 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv53 64)) #x03f79d71b4cb0a89)))
(define-fun hash_54 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv54 64)) #x03f79d71b4cb0a89)))
(define-fun hash_55 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv55 64)) #x03f79d71b4cb0a89)))
(define-fun hash_56 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv56 64)) #x03f79d71b4cb0a89)))
(define-fun hash_57 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv57 64)) #x03f79d71b4cb0a89)))
(define-fun hash_58 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv58 64)) #x03f79d71b4cb0a89)))
(define-fun hash_59 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv59 64)) #x03f79d71b4cb0a89)))
(define-fun hash_60 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv60 64)) #x03f79d71b4cb0a89)))
(define-fun hash_61 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv61 64)) #x03f79d71b4cb0a89)))
(define-fun hash_62 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv62 64)) #x03f79d71b4cb0a89)))
(define-fun hash_63 () (_ BitVec 6) ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) (_ bv63 64)) #x03f79d71b4cb0a89)))

(assert
  (not
    (distinct
      hash_00 hash_01 hash_02 hash_03 hash_04 hash_05 hash_06 hash_07
      hash_08 hash_09 hash_10 hash_11 hash_12 hash_13 hash_14 hash_15
      hash_16 hash_17 hash_18 hash_19 hash_20 hash_21 hash_22 hash_23
      hash_24 hash_25 hash_26 hash_27 hash_28 hash_29 hash_30 hash_31
      hash_32 hash_33 hash_34 hash_35 hash_36 hash_37 hash_38 hash_39
      hash_40 hash_41 hash_42 hash_43 hash_44 hash_45 hash_46 hash_47
      hash_48 hash_49 hash_50 hash_51 hash_52 hash_53 hash_54 hash_55
      hash_56 hash_57 hash_58 hash_59 hash_60 hash_61 hash_62 hash_63)))
(check-sat)
