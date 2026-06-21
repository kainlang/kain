; converge-eligible-mask-8bit.smt2
;
; Claim: Since KAIN_CONVERGE_LANE_MAX == 8, the eligible_mask only ever has
; bits set in positions 0..7. The check `lane < 64u && ((eligible_mask >> lane) & 1) != 0`
; is equivalent to `((eligible_mask & 0xFF) >> lane) & 1) != 0` for any lane in [0,7].
;
; Furthermore, `kain_converge_lowbit_lane` with an eligible_mask constrained to
; bits 0..7 always returns a lane index in [0,7] (or fallback_lane if mask==0).
;
; Proof: For any 64-bit mask whose bits 8..63 are all zero, the lowbit (CTZ)
; always returns a value < 8 for non-zero masks.

(set-logic QF_BV)

(declare-const mask (_ BitVec 64))
(declare-const lane (_ BitVec 64))

; ── Claim 1: lane check is equivalent with 8-bit mask ──
; Constraint: lane is in [0, 7]
(assert (bvult lane #x0000000000000008))
; Constraint: mask has only bottom 8 bits set
(assert (= ((_ extract 63 8) mask) #x00000000000000))

; Full check: lane < 64 && ((mask >> lane) & 1) != 0
(define-fun full_check () Bool
  (and (bvult lane #x0000000000000040)
       (not (= (bvand (bvlshr mask lane) #x0000000000000001) #x0000000000000000))))

; Masked check: ((mask & 0xFF) >> lane) & 1) != 0
(define-fun masked_check () Bool
  (not (= (bvand (bvlshr (bvand mask #x00000000000000FF) lane) #x0000000000000001)
          #x0000000000000000)))

; Negate equivalence
(push)
(assert (not (= full_check masked_check)))
(check-sat)
(pop)

; ── Claim 2: lowbit_lane always returns [0,7] for non-zero 8-bit mask ──
; Reference CTZ (same as in converge-debruijn-ctz-8bit.smt2)
(define-fun ctz ((m (_ BitVec 64))) (_ BitVec 6)
  (ite (= ((_ extract 0 0) m) #b1) #b000000
  (ite (= ((_ extract 1 1) m) #b1) #b000001
  (ite (= ((_ extract 2 2) m) #b1) #b000010
  (ite (= ((_ extract 3 3) m) #b1) #b000011
  (ite (= ((_ extract 4 4) m) #b1) #b000100
  (ite (= ((_ extract 5 5) m) #b1) #b000101
  (ite (= ((_ extract 6 6) m) #b1) #b000110
  (ite (= ((_ extract 7 7) m) #b1) #b000111
  #b001000
  )))))))))

(declare-const m (_ BitVec 64))
; non-zero 8-bit mask
(assert (= ((_ extract 63 8) m) #x00000000000000))
(assert (not (= m #x0000000000000000)))

; Result must be in [0, 7]
(assert (not (bvult (zero_extend 58 (ctz m)) #x0000000000000008)))
(check-sat)
; unsat = CTZ always returns < 8 for non-zero 8-bit masks ✅
; sat = counterexample where CTZ returns >= 8 ❌
