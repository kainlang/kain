; Proof: Packed flag BLOCKED_MASK equivalence
;
; KainAsyncTaskRecord packs 9 boolean flags into a single uint32_t.
;
; Bit positions:
;   IN_USE=0 (0x01), CANCEL_REQUESTED=1 (0x02), RUN_ENQUEUED=2 (0x04),
;   COMPLETION_ENQUEUED=3 (0x08), COMPLETION_FIRED=4 (0x10),
;   COMPLETION_DEFERRED=5 (0x20), CONTINUATION_BLOCKED=6 (0x40),
;   CHILD_WAIT_ACTIVE=7 (0x80), DEPENDENCY_WAIT_ACTIVE=8 (0x100)
;
; Original is_blocked (4 branch checks):
;   continuation_blocked || dependency_wait_active || child_wait_active || completion_deferred
;
; BLOCKED_MASK = bits 5|6|7|8 = 0x1E0
;
; Candidate (single mask test):
;   (flags & BLOCKED_MASK) != 0
;
; Result: unsat — no counterexample exists.
;   The single mask test is equivalent to the 4-way OR for all 9-bit flag configurations.

(set-logic QF_BV)

(define-fun blocked_original ((f (_ BitVec 32))) Bool
  (or (not (= (bvand f #x00000040) #x00000000))     ; bit 6 = continuation_blocked
      (not (= (bvand f #x00000100) #x00000000))     ; bit 8 = dependency_wait_active
      (not (= (bvand f #x00000080) #x00000000))     ; bit 7 = child_wait_active
      (not (= (bvand f #x00000020) #x00000000))))    ; bit 5 = completion_deferred

(define-fun blocked_candidate ((f (_ BitVec 32))) Bool
  (not (= (bvand f #x000001E0) #x00000000)))         ; mask = 0x1E0 = bits 5-8

(declare-const f (_ BitVec 32))

; No bit constraints — test ALL 32-bit inputs (full coverage)

(assert (not (= (blocked_original f) (blocked_candidate f))))

(check-sat)
(get-info :all-statistics)
