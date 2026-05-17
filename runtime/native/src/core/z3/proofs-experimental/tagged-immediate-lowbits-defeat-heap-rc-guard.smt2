(set-logic QF_BV)

; Any immediate tagged handle in Kain is formed by OR-ing a nonzero 3-bit tag
; into a carrier whose low 3 bits were zero beforehand:
;   - aligned borrowed pointer handles: raw = aligned_ptr
;   - integer immediates: raw = payload << 3
; The heap-only RC guard used by LLVM should therefore never classify such a
; handle as a heap RC pointer, because (handle & 7) cannot be zero.

(declare-fun raw () (_ BitVec 64))
(declare-fun tag () (_ BitVec 64))

(define-fun low3 ((value (_ BitVec 64))) (_ BitVec 64)
  (bvand value #x0000000000000007))

(assert (= (low3 raw) #x0000000000000000))
(assert (or (= tag #x0000000000000001)
            (= tag #x0000000000000002)
            (= tag #x0000000000000003)))

(define-fun tagged_handle () (_ BitVec 64)
  (bvor raw tag))

; Negation of the desired invariant: a tagged immediate still looks heap-aligned.
(assert (= (low3 tagged_handle) #x0000000000000000))

(check-sat)
