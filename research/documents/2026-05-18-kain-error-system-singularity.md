# Kain Error System Singularity

- Date: 2026-05-18
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `kain-error-system-singularity`

## Research Question

How far can Kain's compiler and runtime error system be upgraded beyond conventional pretty diagnostics into a semantic, causal, proof-backed, agent-readable diagnostic substrate?

## Constraints

- Latency: diagnostic construction must be cheap on the success path; deep explanation can be lazy or gated behind verbose/JSON modes.
- Throughput: parser recovery must remain bounded by `MAX_ERRORS` and no-progress guards.
- Memory: compiler diagnostics can own arena-backed rich payloads; native runtime diagnostics need fixed-size ABI-safe records or handles.
- Platform: Rust compiler, native C runtime, CLI, LSP, benchmark, attrition, and future Kain-authored compiler lanes should consume the same semantic shape.
- Safety: span math, fix-it ranges, recovery progress, and native formatting bounds need Z3-backed invariants.
- Acceptable weirdness: high. Causal diagnostic graphs, proof witnesses, semantic repair synthesis, and agent-readable traces are all in scope.

## Hypothesis Lattice

### Baseline
- Mechanism: replace flat `message + span` paths with a unified `KainDiagnostic` model carrying stable code, severity, primary span, labels, notes, fix-its, docs key, and JSON rendering.
- Expected upside: immediate improvement in CLI/LSP quality; makes existing parser/runtime diagnostics consistent.
- Likely blocker: many call sites still construct `KainError::{Parser, Type, Codegen}` directly, and native runtime uses separate numeric subsystem families.
- Proof obligation: every emitted diagnostic has a stable code, bounded valid spans, deterministic JSON, and no formatter buffer escape.

### Unconventional
- Mechanism: emit a causal diagnostic graph instead of an ordered list. Nodes represent parse/type/effect/ownership/runtime facts; edges represent "caused by", "blocked by", "suggested repair", and "cascaded from".
- Expected upside: root-cause ranking can suppress noise and explain Kain-only semantics like `world`, `entangle`, `collapse`, `converge`, actors, FFI, and native ABI failures as connected chains.
- Likely blocker: the compiler currently throws away too much parse/type context once it formats strings.
- Proof obligation: the graph must be acyclic within one diagnostic batch, every displayed secondary span must be reachable from a primary cause, and recovery must make token-position progress.

### Moonshot
- Mechanism: build a proof-backed repair oracle. Each diagnostic can carry a small typed repair program or constraint problem: "minimal edit that restores grammar/type/effect/ownership invariants while preserving nearby intent."
- Expected upside: `kain check --explain --repair-plan` becomes an agent-grade compiler copilot instead of a message printer. The system can produce structured patches, proof witnesses, and attrition cases from failures.
- Likely blocker: false confidence. Repairs that compile but mutate intent would be worse than no repair.
- Proof obligation: repairs must prove local grammar/type/effect validity, span non-overlap, edit determinism, and optional semantic preservation predicates for constrained cases.

## Mathematical Model

- Variables: diagnostic nodes `D`, source spans `S`, parser/token position `P`, repair edits `E`, cause edges `C`, subsystem family `F`, severity `V`, confidence `Q`, cost `K`.
- Invariants: all spans satisfy `0 <= start <= end <= source_len`; recovery progress satisfies `P_next > P` or `at_end`; cause graph is acyclic; every diagnostic code maps to exactly one spec; edits are sorted and non-overlapping.
- Objective: maximize `actionability = root_cause_precision + repair_success + semantic_specificity + agent_readability - noise - latency_cost`.
- Bad states: invalid span; duplicate/unstable code; cascade error ranked before root cause; suggested fix overlaps another fix; runtime buffer truncates in a way that changes meaning; repair compiles while violating a declared semantic preservation predicate.
- Simplifying assumptions: first model one-file compiler diagnostics and native startup diagnostics separately, then bridge them through a shared schema.

## Z3 Claims

1. Existing span proofs should remain green: clamped span starts, newline offsets, and line-end math stay within source bounds.
2. New graph proof: for a diagnostic batch of bounded size `N`, edge relation `C` is acyclic and every displayed secondary node is reachable from at least one root.
3. New recovery proof: every parser recovery branch either consumes a token, reaches EOF, or reaches a synchronization token without an infinite loop.
4. New repair proof: generated fix-it edits are pairwise non-overlapping and stay within source bounds.
5. New native proof: fixed-size C diagnostic buffers always null-terminate and count severities without underflow/overflow.

## Evidence And Sources

- Local: `crates/core/src/diagnostics.rs` has span mapping and pretty rendering; `crates/core/src/error.rs` has `KainError::Enhanced`, `DiagnosticBuilder`, and readable token strings; `crates/core/src/diagnostic_registry.rs` has stable compiler codes; `crates/core/src/parser.rs` has recovery with `MAX_ERRORS`; `runtime/native/include/diagnostics.h` and `runtime/native/src/core/diagnostics.c` have runtime subsystem families and collectors.
- Local: `crates/core/z3/proofs/diagnostics-*.yaml` already proves parts of span/line arithmetic.
- Local Z3 attempt: `run_proof_pack` with diagnostic patterns refreshed analysis but matched zero cases, so the existing diagnostic cases may need proof-pack naming/index cleanup before this lane can report them as a focused subset.
- Shipped vertical slice:
  - `KainError::Rich(Box<DiagnosticReport>)` with severity, kind, stable code, primary span, labels, notes, help, fix-its, origin, and JSON.
  - `Diagnostics::format_error` renders rich reports with source context and structured notes/fix-its.
  - Parser `expect` now emits rich diagnostics for expected-token failures.
  - Missing-colon-before-newline diagnostics anchor to the previous significant token, preserve the old phrase for compatibility, avoid synthetic `<frontend-import-scan>:` fake locations, and offer an insert-`:` fix-it.
  - Durable proof: `crates/core/z3/proofs/parser-colon-fixit-zero-width-span-stays-in-source-bounds.yaml`.
- Validation:
  - `cargo test -p kain-core --test test_parser_error_format`: PASS, 5 tests.
  - `cargo test -p kain-core --test test_parser_error_quality test_missing_block_colon_reports_actionable_newline_hint`: PASS.
  - `cargo check -p kain-core`: PASS.
  - `run_proof_pack` lane `parser`: PASS, 4 proved, including `parser-colon-fixit-zero-width-span-stays-in-source-bounds`.
  - Broader `cargo test -p kain-core --test test_parser_error_quality` still has unrelated stale expectations where currently accepted syntax is expected to fail.
- External:

## Dead Ends

- None yet.

## Conclusion

Current thesis: Kain can get far beyond "better error strings." The first slice is now landed: parser diagnostics can carry structured semantic evidence, source labels, fix-its, and JSON. The next frontier is root-cause graph ranking across parser/type/runtime boundaries so import-scan, stdlib prelude, and downstream codegen failures can point to the true author-owned cause instead of the first subsystem that noticed smoke.
