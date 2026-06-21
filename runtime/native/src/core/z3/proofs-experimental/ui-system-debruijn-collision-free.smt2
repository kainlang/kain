; Experimental proof: ui_system.c low-bit isolation + de Bruijn index.
; Target: abi_ui_isolate_low_bit_u64 + abi_ui_low_bit_index_u64
; Claim: For any 64-bit input with exactly one bit set (one-hot),
; the composition of isolate_low_bit (value & (0 - value)) followed by
; de Bruijn index (one_hot * 0x03f79d71b4cb0a89 >> 58) produces an index
; in [0, 63] with no collisions across all 64 possible one-hot values.
;
; Equivalent to: the de Bruijn table is collision-free for this constant.
; This proof mirrors ownership-debruijn-low-bit-distinct.smt2 but
; targets the ui_system.c instance.
(set-logic QF_BV)

; Generate hash for one-hot at position n (0..63)
(define-fun hash ((n (_ BitVec 64))) (_ BitVec 6)
  ((_ extract 63 58) (bvmul (bvshl (_ bv1 64) n) #x03f79d71b4cb0a89)))

; Assert NO two positions 0..63 produce the same hash
(assert
  (not
    (forall ((i (_ BitVec 64)) (j (_ BitVec 64)))
      (=> (and (bvult i (_ bv64 64)) (bvult j (_ bv64 64)) (not (= i j)))
          (not (= (hash i) (hash j)))))))

(check-sat)
