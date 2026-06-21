; Z3 Proof: De Bruijn constant collision-free for all 64 one-hot values
; Target: X:/runtime/native/src/ui/ui_system.c line 35
;
; The constant 0x03f79d71b4cb0a89 maps each one-hot (power-of-two)
; 64-bit value to a unique 6-bit index via (x * magic) >> 58.
;
; Result: unsat (no collisions)

(set-logic QF_BV)

(define-fun debruijn_idx ((x (_ BitVec 64))) (_ BitVec 6)
  ((_ extract 63 58) (bvmul x #x03f79d71b4cb0a89)))

(declare-fun i () (_ BitVec 64))
(declare-fun j () (_ BitVec 64))

; i and j are one-hot values (powers of two)
(assert (and
  (not (= i (_ bv0 64)))
  (not (= j (_ bv0 64)))
  (= (bvand i (bvsub i (_ bv1 64))) (_ bv0 64))
  (= (bvand j (bvsub j (_ bv1 64))) (_ bv0 64))
  (not (= i j))
  (= (debruijn_idx i) (debruijn_idx j))))

(check-sat)
; Expected: unsat
