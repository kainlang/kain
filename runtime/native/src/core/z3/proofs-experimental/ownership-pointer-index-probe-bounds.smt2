; Experimental proof for the ownership pointer-index sidecar in
; ownership.c.
; Claim: the SplitMix-style pointer mixer plus masked linear probe can never
; address outside the 8192-entry pointer-index table.
(set-logic QF_BV)

(declare-fun ptr () (_ BitVec 64))
(declare-fun probe () (_ BitVec 64))

(define-fun mix0 () (_ BitVec 64)
  (bvxor ptr (bvlshr ptr #x000000000000001e)))
(define-fun mix1 () (_ BitVec 64)
  (bvmul mix0 #xbf58476d1ce4e5b9))
(define-fun mix2 () (_ BitVec 64)
  (bvxor mix1 (bvlshr mix1 #x000000000000001b)))
(define-fun mix3 () (_ BitVec 64)
  (bvmul mix2 #x94d049bb133111eb))
(define-fun mixed () (_ BitVec 64)
  (bvxor mix3 (bvlshr mix3 #x000000000000001f)))
(define-fun start_index () (_ BitVec 64)
  (bvand mixed #x0000000000001fff))
(define-fun candidate_index () (_ BitVec 64)
  (bvand (bvadd start_index probe) #x0000000000001fff))

(assert (bvugt candidate_index #x0000000000001fff))
(check-sat)
