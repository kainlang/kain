; Soundness sketch for Kain launcher LLVM native-runtime elision.
;
; The launcher first computes the direct-call reachability closure from @main
; over defined LLVM functions. It only elides the native runtime when every
; reachable function has zero non-intrinsic external call targets. This proof
; checks the core invariant for a bounded 4-function graph: if the reachable
; set is closed under internal call edges and the analyzer-safe predicate holds,
; a reachable external call is impossible.

(set-logic QF_UF)

(declare-const r0 Bool) ; @main
(declare-const r1 Bool)
(declare-const r2 Bool)
(declare-const r3 Bool)

(declare-const e00 Bool)
(declare-const e01 Bool)
(declare-const e02 Bool)
(declare-const e03 Bool)
(declare-const e10 Bool)
(declare-const e11 Bool)
(declare-const e12 Bool)
(declare-const e13 Bool)
(declare-const e20 Bool)
(declare-const e21 Bool)
(declare-const e22 Bool)
(declare-const e23 Bool)
(declare-const e30 Bool)
(declare-const e31 Bool)
(declare-const e32 Bool)
(declare-const e33 Bool)

(declare-const x0 Bool) ; function 0 has a non-intrinsic external call target
(declare-const x1 Bool)
(declare-const x2 Bool)
(declare-const x3 Bool)

; @main is the root.
(assert r0)

; Reachability is closed under internal call edges.
(assert (=> (and r0 e00) r0))
(assert (=> (and r0 e01) r1))
(assert (=> (and r0 e02) r2))
(assert (=> (and r0 e03) r3))
(assert (=> (and r1 e10) r0))
(assert (=> (and r1 e11) r1))
(assert (=> (and r1 e12) r2))
(assert (=> (and r1 e13) r3))
(assert (=> (and r2 e20) r0))
(assert (=> (and r2 e21) r1))
(assert (=> (and r2 e22) r2))
(assert (=> (and r2 e23) r3))
(assert (=> (and r3 e30) r0))
(assert (=> (and r3 e31) r1))
(assert (=> (and r3 e32) r2))
(assert (=> (and r3 e33) r3))

; Analyzer-safe condition used by the launcher before runtime elision.
(assert (=> r0 (not x0)))
(assert (=> r1 (not x1)))
(assert (=> r2 (not x2)))
(assert (=> r3 (not x3)))

; Counterexample query: some reachable function still has an external call.
(assert (or (and r0 x0) (and r1 x1) (and r2 x2) (and r3 x3)))

(check-sat)
