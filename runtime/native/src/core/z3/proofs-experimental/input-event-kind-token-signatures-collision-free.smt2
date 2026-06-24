; Proof: the input system's event kind string token signatures are
; collision-free for the 9 known event kind / source kind strings.
;
; Token function: (len << 24) XOR (first << 16) XOR (second << 8) XOR last
;
; Strings: "*", "key_up", "pointer_up", "action_up", "action",
;          "action_down", "text", "axis", "agent.intent"
;
; Verification: z3 action="check" smt2="..." -> unsat
(set-logic QF_BV)
(define-fun sig ((len (_ BitVec 32)) (first (_ BitVec 32))
                 (second (_ BitVec 32)) (last (_ BitVec 32))) (_ BitVec 32)
  (bvxor (bvshl len #x00000018)
         (bvxor (bvshl first #x00000010)
                (bvxor (bvshl second #x00000008) last))))

; "*" — wildcard source kind
(define-fun t_wildcard () (_ BitVec 32)
  (sig #x00000001 #x0000002a #x0000002a #x0000002a))
; "key_up"
(define-fun t_key_up () (_ BitVec 32)
  (sig #x00000006 #x0000006b #x00000065 #x00000070))
; "pointer_up"
(define-fun t_pointer_up () (_ BitVec 32)
  (sig #x00000009 #x00000070 #x0000006f #x00000070))
; "action_up"
(define-fun t_action_up () (_ BitVec 32)
  (sig #x00000009 #x00000061 #x00000063 #x00000070))
; "action"
(define-fun t_action () (_ BitVec 32)
  (sig #x00000006 #x00000061 #x00000063 #x0000006e))
; "action_down"
(define-fun t_action_down () (_ BitVec 32)
  (sig #x0000000b #x00000061 #x00000063 #x0000006e))
; "text"
(define-fun t_text () (_ BitVec 32)
  (sig #x00000004 #x00000074 #x00000065 #x00000074))
; "axis"
(define-fun t_axis () (_ BitVec 32)
  (sig #x00000004 #x00000061 #x00000078 #x00000073))
; "agent.intent"
(define-fun t_agent_intent () (_ BitVec 32)
  (sig #x0000000c #x00000061 #x00000067 #x00000074))

; Negative assertion: NOT all distinct — expects unsat = collision-free
(assert (not (distinct
  t_wildcard t_key_up t_pointer_up t_action_up t_action
  t_action_down t_text t_axis t_agent_intent)))
(check-sat)
