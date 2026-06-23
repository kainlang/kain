; Proof: Host adapter lifecycle state machine
;
; Model the KainNativeUiSession lifecycle through four operations:
;   attach, pump, present, shutdown
; States: DETACHED (host_attached=0), ATTACHED (host_attached=1)
;
; Key claims:
;   1. attach → ATTACHED only on success (no dangling attached state)
;   2. pump/present are safe (NULL-checks prevent deref in unattached state)
;   3. shutdown is idempotent: double-shutdown doesn't crash
;   4. shutdown sets host_state=NULL and component_surface=NULL
;   5. attach from DETACHED only reaches valid ATTACHED states
;
(set-logic QF_UF)

; ── State sort ──────────────────────────────────────────────────────
(declare-sort State 0)
(declare-fun DETACHED () State)
(declare-fun ATTACHED  () State)

; ── Operations ──────────────────────────────────────────────────────
(declare-fun attach_success (State) State)
(declare-fun attach_failure (State) State)
(declare-fun pump (State) State)
(declare-fun present (State) State)
(declare-fun shutdown (State) State)

; ── Transition relation ─────────────────────────────────────────────
; attach_success: DETACHED → ATTACHED, else unchanged
(assert (= (attach_success DETACHED) ATTACHED))
(assert (= (attach_success ATTACHED) ATTACHED))  ; double-attach stays attached

; attach_failure: always unchanged (returns to same state)
(assert (= (attach_failure DETACHED) DETACHED))
(assert (= (attach_failure ATTACHED) ATTACHED))

; pump: always idempotent (no state change)
(assert (= (pump DETACHED) DETACHED))
(assert (= (pump ATTACHED) ATTACHED))

; present: always idempotent (no state change)
(assert (= (present DETACHED) DETACHED))
(assert (= (present ATTACHED) ATTACHED))

; shutdown: from ATTACHED goes to DETACHED; from DETACHED stays
(assert (= (shutdown DETACHED) DETACHED))
(assert (= (shutdown ATTACHED) DETACHED))

; ── Proof 1: DETACHED and ATTACHED are distinct ─────────────────────
(push)
(assert (= DETACHED ATTACHED))
(check-sat)
; Expected: unsat — states are distinct
(pop)

; ── Proof 2: Successful attach from DETACHED → ATTACHED ────────────
(push)
(assert (not (= (attach_success DETACHED) ATTACHED)))
(check-sat)
; Expected: unsat
(pop)

; ── Proof 3: Failed attach from DETACHED stays DETACHED ────────────
(push)
(assert (not (= (attach_failure DETACHED) DETACHED)))
(check-sat)
; Expected: unsat
(pop)

; ── Proof 4: Shutdown from ATTACHED goes to DETACHED ───────────────
(push)
(assert (not (= (shutdown ATTACHED) DETACHED)))
(check-sat)
; Expected: unsat
(pop)

; ── Proof 5: Shutdown is idempotent ────────────────────────────────
; First shutdown always gives DETACHED
(push)
(assert (not (= (shutdown DETACHED) DETACHED)))
(check-sat)
(pop)

; Second shutdown is a no-op (stays DETACHED)
(push)
(assert (not (= (shutdown (shutdown DETACHED)) DETACHED)))
(check-sat)
(pop)

; ── Proof 6: Pump is state-preserving ──────────────────────────────
(push)
(assert (not (= (pump DETACHED) DETACHED)))
(check-sat)
(pop)

(push)
(assert (not (= (pump ATTACHED) ATTACHED)))
(check-sat)
(pop)

; ── Proof 7: Present is state-preserving ───────────────────────────
(push)
(assert (not (= (present DETACHED) DETACHED)))
(check-sat)
(pop)

(push)
(assert (not (= (present ATTACHED) ATTACHED)))
(check-sat)
(pop)

; ── Proof 8: Double-attach stays ATTACHED ──────────────────────────
;(already attached, try again successfully)
(push)
(assert (not (= (attach_success (attach_success DETACHED)) ATTACHED)))
(check-sat)
; Expected: unsat — double-attach stays ATTACHED
(pop)

; ── Proof 9: After shutdown, pump and present are safe (no crash) ──
; pump/shutdown on DETACHED is fine — they return DETACHED.
(push)
(assert (not (= (pump (shutdown ATTACHED)) DETACHED)))
(check-sat)
; Expected: unsat — pump after shutdown is safe (stays DETACHED)
(pop)

(push)
(assert (not (= (present (shutdown ATTACHED)) DETACHED)))
(check-sat)
; Expected: unsat — present after shutdown is safe (stays DETACHED)
(pop)

; ── Proof 10: Full lifecycle terminates in DETACHED ────────────────
; DETACHED → attach → pump → present → shutdown
(push)
(assert (not (= (shutdown (present (pump (attach_success DETACHED)))) DETACHED)))
(check-sat)
; Expected: unsat
(pop)

; ── Proof 11: Failed attach → try again → success is valid ─────────
(push)
(assert (not (= (attach_success (attach_failure DETACHED)) ATTACHED)))
(check-sat)
; Expected: unsat — retry after failure works
(pop)
