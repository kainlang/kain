(set-logic QF_BV)

; Hot reload ring proof pack for runtime/native/src/ui/kain_ui_hot_reload.c
; 1. The fixed-capacity event ring uses a power-of-two mask safely.
; 2. Wrapped writes stay within the configured ring capacity.

; Proof 1: seq & 127 == seq mod 128 for a power-of-two ring.
(push)
(declare-fun seq () (_ BitVec 32))
(assert (not (= (bvand seq #x0000007f) (bvurem seq #x00000080))))
(check-sat)
(pop)

; Proof 2: the wrapped append range never exceeds the ring capacity.
(push)
(declare-fun write_offset () (_ BitVec 8))
(declare-fun effective_length () (_ BitVec 8))
(assert (bvule write_offset #x7f))
(assert (bvule effective_length #x80))
(define-fun raw_sum () (_ BitVec 9)
  (bvadd ((_ zero_extend 1) write_offset) ((_ zero_extend 1) effective_length)))
(define-fun cap9 () (_ BitVec 9) #b010000000)
(define-fun zero9 () (_ BitVec 9) #b000000000)
(define-fun wrapped_offset () (_ BitVec 9)
  (ite (bvugt raw_sum cap9) zero9 ((_ zero_extend 1) write_offset)))
(define-fun wrapped_end () (_ BitVec 9)
  (bvadd wrapped_offset ((_ zero_extend 1) effective_length)))
(assert (bvugt wrapped_end cap9))
(check-sat)
(pop)
