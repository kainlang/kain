; Exploratory proof for the UI occupancy-bit fast path.
; It checks that the de Bruijn multiplier used by
; `kain_native_ui_low_bit_index_u64` produces a distinct 6-bit signature
; for every 64-bit one-hot input before the lookup table is applied.

(set-logic QF_BV)

(declare-fun i () (_ BitVec 6))
(declare-fun j () (_ BitVec 6))

(define-fun one_hot ((bit (_ BitVec 6))) (_ BitVec 64)
  (bvshl #x0000000000000001 ((_ zero_extend 58) bit)))

(define-fun debruijn_signature ((value (_ BitVec 64))) (_ BitVec 6)
  ((_ extract 63 58) (bvmul value #x03f79d71b4cb0a89)))

(assert (distinct i j))
(assert (= (debruijn_signature (one_hot i))
           (debruijn_signature (one_hot j))))

(check-sat)
