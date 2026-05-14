; Experimental proof for the process spec/handle index sidecars in
; kain_native_process_system.c.
; Claim: the SplitMix-style id mixer plus masked linear probe can never address
; outside the 128-entry handle-index tables.
(set-logic QF_BV)

(declare-fun id () (_ BitVec 64))
(declare-fun probe () (_ BitVec 64))

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
(define-fun start_index () (_ BitVec 64)
  (bvand mixed #x000000000000007f))
(define-fun candidate_index () (_ BitVec 64)
  (bvand (bvadd start_index probe) #x000000000000007f))

(assert (bvugt candidate_index #x000000000000007f))
(check-sat)
