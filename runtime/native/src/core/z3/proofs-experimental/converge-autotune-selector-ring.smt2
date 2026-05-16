; Experimental proof sketch for the proposed converge autotune substrate.
; Claims:
; 1. Power-of-two telemetry ring indexing with `cursor & 63` is always < 64.
; 2. A selected eligible lane bit must be both present and capable.
; 3. Odd-stride probing over 64 tuning-cache slots is injective, so the probe
;    sequence can cover the table without cycling early.
(set-logic QF_BV)

(declare-fun cursor () (_ BitVec 64))
(define-fun ring_index () (_ BitVec 64) (bvand cursor #x000000000000003f))

; Negate bound claim: ring_index > 63.
(push)
(assert (bvugt ring_index #x000000000000003f))
(check-sat)
(pop)

; Phase-1 compiled converge tables are intentionally tiny: <= 8 fast lanes.
(declare-fun lane_index () (_ BitVec 64))
(declare-fun eligible_mask () (_ BitVec 64))
(declare-fun present_mask () (_ BitVec 64))
(declare-fun capable_mask () (_ BitVec 64))
(assert (bvult lane_index #x0000000000000008))
(assert (= eligible_mask (bvand present_mask capable_mask)))
(define-fun selected_bit () (_ BitVec 64) (bvshl #x0000000000000001 lane_index))

; Negate claim: selected is eligible but not both present and capable.
(push)
(assert (not (= (bvand eligible_mask selected_bit) #x0000000000000000)))
(assert (or (= (bvand present_mask selected_bit) #x0000000000000000)
            (= (bvand capable_mask selected_bit) #x0000000000000000)))
(check-sat)
(pop)

; Odd stride visits power-of-two cache slots as a permutation modulo 64.
(declare-fun a () (_ BitVec 6))
(declare-fun b () (_ BitVec 6))
(declare-fun stride () (_ BitVec 6))
(assert (not (= a b)))
(assert (= ((_ extract 0 0) stride) #b1))

; Negate injectivity of x -> x * odd_stride modulo 64.
(push)
(assert (= (bvmul a stride) (bvmul b stride)))
(check-sat)
(pop)
