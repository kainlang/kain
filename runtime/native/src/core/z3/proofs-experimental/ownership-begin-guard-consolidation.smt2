; Proof: Begin-observe/collapse/share shared guard consolidation
;
; All three functions share the same guard pattern:
;   if (state == DECAYED)   return ERR_DECAYED;
;   if (state == SHARED)    return ERR_COLLAPSED;
;   if (state == COLLAPSED) return ERR_COLLAPSED;
;   if (state == OBSERVED || observers != 0) return ERR_OBSERVED;
;
; Optimization: extract this common pattern and reuse it.
; The only difference between the three is what they do after the guards pass.
;
; Prove that the shared guard function is equivalent to the inline guards.
;
(set-logic QF_BV)
(declare-const state (_ BitVec 32))
(declare-const o (_ BitVec 32))
(assert (bvule state (_ bv4 32)))
(assert (bvule o (_ bv1000 32)))
; Invariant: observers > 0 => state == OBSERVED
(assert (=> (not (= o (_ bv0 32))) (= state (_ bv1 32))))

; Shared guard: returns ERR status code if busy, or OK(0) if allowed
(define-fun shared_guard () (_ BitVec 32)
  (ite (= state (_ bv4 32)) (_ bv4294967290 32) ; ERR_DECAYED
  (ite (= state (_ bv3 32)) (_ bv4294967291 32) ; ERR_COLLAPSED
  (ite (= state (_ bv2 32)) (_ bv4294967291 32) ; ERR_COLLAPSED
  (ite (= state (_ bv1 32)) (_ bv4294967292 32) ; ERR_OBSERVED
  (ite (not (= o (_ bv0 32))) (_ bv4294967292 32) ; ERR_OBSERVED
  (_ bv0 32))))))) ; OK

; Reference: begin_observe inline guard (returns ERR_* or OK allowing proceed)
(define-fun guard_observe () (_ BitVec 32)
  (ite (= state (_ bv4 32)) (_ bv4294967290 32)
  (ite (= state (_ bv3 32)) (_ bv4294967291 32)
  (ite (= state (_ bv2 32)) (_ bv4294967291 32)
  (ite (or (= state (_ bv1 32)) (not (= o (_ bv0 32)))) (_ bv4294967292 32)
  (_ bv0 32))))))

; Reference: begin_collapse inline guard
(define-fun guard_collapse () (_ BitVec 32)
  (ite (= state (_ bv4 32)) (_ bv4294967290 32)
  (ite (= state (_ bv3 32)) (_ bv4294967291 32)
  (ite (= state (_ bv2 32)) (_ bv4294967291 32)
  (ite (= state (_ bv1 32)) (_ bv4294967292 32)
  (ite (not (= o (_ bv0 32))) (_ bv4294967292 32)
  (_ bv0 32)))))))

; Wait - begin_collapse checks OBSERVED first, then observers separately.
; Let me re-read the original:
;   if (state == OBSERVED || region->observers != 0) { return ERR_OBSERVED; }
; They use || so it's equivalent to what I wrote for guard_observe.
; Let me fix guard_collapse to match reference exactly:
(define-fun guard_collapse_ref () (_ BitVec 32)
  (ite (= state (_ bv4 32)) (_ bv4294967290 32)
  (ite (= state (_ bv3 32)) (_ bv4294967291 32)
  (ite (= state (_ bv2 32)) (_ bv4294967291 32)
  (ite (or (= state (_ bv1 32)) (not (= o (_ bv0 32)))) (_ bv4294967292 32)
  (_ bv0 32))))))

; Actually all three begin_* functions use the same guard pattern:
;   if (DECAYED) -> ERR_DECAYED
;   if (SHARED) -> ERR_COLLAPSED
;   if (COLLAPSED) -> ERR_COLLAPSED
;   if (OBSERVED || observers != 0) -> ERR_OBSERVED
;   else OK (allow transition)
; Let me verify shared_guard matches all three:
(assert (not (= shared_guard guard_observe)))
(check-sat)
