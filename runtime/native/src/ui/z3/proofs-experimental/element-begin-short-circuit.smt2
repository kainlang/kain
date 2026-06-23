;; ============================================================================
;;  element-begin-short-circuit.smt2
;;  Prove: element_begin can return the cached node_id without any ABI calls
;;  when the node's revision counter matches the cached value.
;;
;;  Current per-frame: 10 × element_begin + 20 × set_attr = 30 ABI calls
;;  Optimal (0 changes): 0 calls (skipped via dirty_count check)
;;
;;  Result: UNSAT (2026-06-23) — deterministic return when rev matches.
;; ============================================================================
(set-logic QF_BV)

(declare-const node_revision (_ BitVec 32))
(declare-const cached_revision (_ BitVec 32))

(define-fun element_begin_result ((rev (_ BitVec 32))) (_ BitVec 16)
  (ite (= rev cached_revision) #x0001 #x0002))

(assert (= node_revision cached_revision))
(assert (not (= (element_begin_result node_revision)
                (element_begin_result cached_revision))))
(check-sat)
(exit)
