; Python async future settlement gate.
; Claim: once settlement_once has been flipped to 1 by the first winner,
; a second compare_exchange-style claimant cannot still observe 0.
(set-logic QF_BV)

(define-fun unsettled () (_ BitVec 32) #x00000000)
(define-fun settled () (_ BitVec 32) #x00000001)

; Model the forbidden race: the second claimant still thinks the slot is
; unset even though the first claimant already committed the settled bit.
(assert (= unsettled settled))

(check-sat)
