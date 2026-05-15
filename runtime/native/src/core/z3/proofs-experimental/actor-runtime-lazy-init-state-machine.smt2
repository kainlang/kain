; Actor runtime lazy-init state machine:
; starting from a reachable state where the actor-runtime and scheduler flags
; agree, one ensure-init or shutdown transition cannot produce a mixed state.

(set-logic QF_UF)

(declare-fun init0 () Bool)
(declare-fun scheduler0 () Bool)
(declare-fun do_ensure () Bool)
(declare-fun do_shutdown () Bool)
(declare-fun init1 () Bool)
(declare-fun scheduler1 () Bool)

; Reachable pre-states keep both flags aligned.
(assert (= init0 scheduler0))

; Model exactly one public transition.
(assert (xor do_ensure do_shutdown))

; ensure_initialized() only flips false -> true and otherwise leaves state as-is.
(assert
  (=> do_ensure
      (and
        (= init1 (ite init0 true init0))
        (= scheduler1 (ite scheduler0 true scheduler0)))))

; runtime_shutdown() only flips true -> false and otherwise leaves state as-is.
(assert
  (=> do_shutdown
      (and
        (= init1 (ite init0 false init0))
        (= scheduler1 (ite scheduler0 false scheduler0)))))

; A mixed post-state would violate the lazy-init contract.
(assert (xor init1 scheduler1))

(check-sat)
