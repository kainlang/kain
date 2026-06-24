; Proof: the SplitMix-style hash mixer + power-of-two mask always produces
; a probe index within every input system table's capacity.
;
; Tables: actions=256, axes=128, events=1024, bindings=512
;
; Verification: z3 action="check" smt2="..." -> unsat
(set-logic QF_BV)
(declare-const id (_ BitVec 64))
(declare-const probe (_ BitVec 64))

; SplitMix-style mixer (proven pattern from abi_net_mix_id)
(define-fun mix0 () (_ BitVec 64)
  (bvxor id (bvlshr id #x000000000000001e)))
(define-fun mix1 () (_ BitVec 64)
  (bvmul mix0 #xbf58476d1ce4e5b9))
(define-fun mix2 () (_ BitVec 64)
  (bvxor mix1 (bvlshr mix1 #x000000000000001b)))
(define-fun mix3 () (_ BitVec 64)
  (bvmul mix2 #x94d049bb133111eb))
(define-fun mixed () (_ BitVec 64)
  (bvxor mix3 (bvlshr mix3 #x000000000000001f)))

; Table masks (capacity - 1) — all input table sizes are powers of 2
(define-fun actions_mask () (_ BitVec 64) #x00000000000000ff)    ; 256-1
(define-fun axes_mask () (_ BitVec 64) #x000000000000007f)       ; 128-1
(define-fun events_mask () (_ BitVec 64) #x00000000000003ff)     ; 1024-1
(define-fun bindings_mask () (_ BitVec 64) #x00000000000001ff)   ; 512-1

; Probe: (hash + step) & mask — always ≤ mask
(define-fun candidate ((mask (_ BitVec 64))) (_ BitVec 64)
  (bvand (bvadd mixed probe) mask))

; If unsat: no candidate exceeds its mask for ANY id or probe step
(assert (or
  (bvugt (candidate actions_mask) actions_mask)
  (bvugt (candidate axes_mask) axes_mask)
  (bvugt (candidate events_mask) events_mask)
  (bvugt (candidate bindings_mask) bindings_mask)))
(check-sat)
