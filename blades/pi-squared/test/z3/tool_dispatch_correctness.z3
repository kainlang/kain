; =============================================================================
; Tool Dispatch Correctness
; Proves: converge spec-vs-fast-lane equivalence for tool dispatch.
;
; Model: pi-squared dispatches tool calls through a converge block.
; The spec lane routes by name. Fast lanes optimize for known tools.
; We prove that for all registered tools, all lanes return the same result.
;
; Tool registry: 5 tools indexed 0..4 with tool kind and expected behavior.
; =============================================================================

; --- Part 1: Spec lane correctness ---
; The spec lane accepts any tool name and routes it.
; Spec returns: 0 = success, 1 = unknown tool, 2 = permissions error
(push)
(declare-const tool_id Int)
(declare-const tool_kind Int) ; 0 = read, 1 = write, 2 = execute, 3 = query, 4 = control
(declare-const has_perm Bool)

; Spec lane: check name in registry, check permissions, dispatch
(define-fun spec_dispatch ((id Int) (kind Int) (perm Bool)) Int
  (ite (or (< id 0) (>= id 5)) 1    ; unknown tool
  (ite (not perm) 2                   ; permissions error
  0)))                                ; success

; Fast lane "read_cache": handles read (kind=0) and query (kind=3) tools
(define-fun fast_read_cache ((id Int) (kind Int) (perm Bool)) Int
  (ite (and (or (= kind 0) (= kind 3)) perm) 0
  (ite (not perm) 2
  (ite (or (< id 0) (>= id 5)) 1
  0))))

; Fast lane "write_buffer": handles write (kind=1) and control (kind=4)
(define-fun fast_write_buffer ((id Int) (kind Int) (perm Bool)) Int
  (ite (and (or (= kind 1) (= kind 4)) perm) 0
  (ite (not perm) 2
  (ite (or (< id 0) (>= id 5)) 1
  0))))

; Fast lane "executor": handles execute (kind=2)
(define-fun fast_executor ((id Int) (kind Int) (perm Bool)) Int
  (ite (and (= kind 2) perm) 0
  (ite (not perm) 2
  (ite (or (< id 0) (>= id 5)) 1
  0))))

; --- Invariant: For all valid tool inputs, spec == selected fast lane ---
(define-fun converge_eq ((id Int) (kind Int) (perm Bool)) Bool
  (let ((spec_result (spec_dispatch id kind perm)))
    (ite (and (or (= kind 0) (= kind 3)) perm)
         (= spec_result (fast_read_cache id kind perm))
    (ite (and (or (= kind 1) (= kind 4)) perm)
         (= spec_result (fast_write_buffer id kind perm))
    (ite (and (= kind 2) perm)
         (= spec_result (fast_executor id kind perm))
    (= spec_result (spec_dispatch id kind perm))))))) ; fallback to spec

; Assert: for all tool_ids 0..4, all kinds 0..4, converge_eq holds
(assert (not (and
  (forall ((id Int) (kind Int) (perm Bool))
    (=> (and (>= id 0) (< id 5) (>= kind 0) (< kind 5))
        (converge_eq id kind perm))))))

; UNSAT confirms all lanes are equivalent to spec for their domain
(check-sat)
(pop)

; --- Part 2: Fast lane selection is deterministic ---
; For a given tool kind, exactly one fast lane should match.
(push)
(declare-const tool_kind2 Int)
(assert (and (>= tool_kind2 0) (< tool_kind2 5)))

; Count how many fast lanes match
(define-fun match_count () Int
  (+ (ite (or (= tool_kind2 0) (= tool_kind2 3)) 1 0)   ; read_cache
     (ite (or (= tool_kind2 1) (= tool_kind2 4)) 1 0)   ; write_buffer
     (ite (= tool_kind2 2) 1 0)))                        ; executor

; Exactly one fast lane should match for every valid kind
(assert (not (= match_count 1)))

; UNSAT confirms exactly one lane matches per kind
(check-sat)
(pop)

; --- Part 3: Error propagation ---
; If spec returns an error, the converge block must also return an error.
(push)
(declare-const id3 Int)
(declare-const kind3 Int)
(declare-const perm3 Bool)
(assert (and (>= id3 0) (< id3 5) (>= kind3 0) (< kind3 5)))

; Case: no permission → both should return 2
(assert (not (=> (not perm3)
  (and (= (spec_dispatch id3 kind3 false) 2)
       (=> (or (= kind3 0) (= kind3 3))
           (= (fast_read_cache id3 kind3 false) 2))
       (=> (or (= kind3 1) (= kind3 4))
           (= (fast_write_buffer id3 kind3 false) 2))
       (=> (= kind3 2)
           (= (fast_executor id3 kind3 false) 2))))))

; UNSAT confirms error propagation
(check-sat)
(pop)

(exit)
