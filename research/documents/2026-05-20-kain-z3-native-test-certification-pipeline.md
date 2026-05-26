# Kain Z3 Native Test Certification Pipeline

- Date: 2026-05-20
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `kain-z3-native-test-certification-pipeline`

## Research Question

How should Kain evolve beyond cargo-test-style execution into a build.kn-native certification pipeline where kain-test can express solver-shaped intent, runtime tests, attrition, and benchmark evidence for LLM-first development?

## Constraints

- `crates/test` is currently a compiletest-style harness with modes for `check-pass`, `check-fail`, `run-pass`, `run-fail`, `kain-test`, `prove-pass`, and `prove-sat`. Proof mode today is raw `//@ smt2:` text piped to `z3`, not Kain-authored proof intent.
- `stdlib/test.kn` currently only models authored outcome values (`test_pass`, `test_fail`, `test_skip`, `test_proved`, `test_witness`). It does not yet expose solver construction, symbolic variables, assumptions, witnesses, or proof families.
- `crates/core/src/runtime.rs` already streams per-runtime-test names (`test foo ... ok`), but `crates/cli/src/main.rs` only prints a final suite summary for `kain test`; it does not yet surface cargo-style suite progress like `17/54`.
- `build.kn` and `[[build.tasks]]` already exist as the workspace authority layer, but explicit task kinds only cover `check`, Cargo, C shared library, GPU, Fabric, Node, and Bun. There is no first-class `test`, `proof`, `benchmark`, `attrition`, or aggregate `certify` task kind yet.
- The repo already has durable Z3 proof packs under `crates/core/z3`, `crates/sys-codegen/z3`, `crates/build/z3`, `crates/ownership/z3`, `crates/foreign-abi/z3`, `crates/gpu/z3`, and `runtime/native/src/core/z3`. Any new test story must compose with those instead of replacing them.
- `benchmark/` and `attrition/` are already rich telemetry lanes with reports, replay artifacts, and machine-readable outputs. The missing piece is orchestration and shared certification vocabulary, not raw measurement capability.
- Z3 is not currently a first-class runtime/compiler subsystem in the language itself. That is good. The testing pipeline should use solver power without making ordinary Kain program execution depend on solver presence.

## Hypothesis Lattice

### Baseline
- Mechanism: keep the current compiletest model, add suite progress streaming, snapshot/revision features, and maybe more raw `//@ smt2:` directives.
- Expected upside: fast incremental improvement, little architectural risk, easy compatibility with Rust compiletest mental models.
- Likely blocker: this still makes Z3 feel bolted on. Authored Kain tests stay runtime-centric, while proofs live in comment strings or external proof packs.
- Proof obligation: show that baseline improvements reduce friction without fragmenting truth between runtime tests, proof packs, benchmark scripts, and attrition scripts.

### Unconventional
- Mechanism: make `kain-test` understand a Kain-authored solver vocabulary exposed through `std::test`, then lower that vocabulary into Z3 or proof-pack execution. The harness becomes proof-fluent rather than raw-SMT-aware only.
- Expected upside: Kain authors write proof intent in Kain terms: symbolic integers, bounded domains, assumptions, expected `unsat` or `sat`, model extraction, optimization, and domain helpers like pointer bounds or layout checks. This is the "kain-test speaks Z3" lane.
- Likely blocker: a raw dump of the Z3 C API into `std::test` would be huge, ugly, unstable, and would leak solver internals into ordinary authored code. The right abstraction is a curated Kain proof DSL plus an escape hatch, not "every Z3 function ever".
- Proof obligation: prove the lowering preserves meaning for the supported fragment. If `std::test::z3.range(0, span).assert(offset >= 0).assert(offset < span).expect_unsat(...)` lowers to SMT, the emitted solver result must match direct solver execution for the same obligation family.

### Moonshot
- Mechanism: elevate `build.kn` into a certification DAG where `test`, `proof`, `benchmark`, `attrition`, and `certify` are first-class task kinds. A blade or workspace declares its own certification recipe. `kain test` becomes one operator inside a larger Kain-native validation graph.
- Expected upside: the repo stops thinking in terms of "a tests folder" and starts thinking in terms of "evidence lanes". Every serious blade can declare: prove these invariants, run these runtime tests, run this attrition profile, hit these benchmark floors, emit LLM-readable telemetry.
- Likely blocker: orchestration complexity. The build DAG needs report schemas, cache keys, replay semantics, failure policy, and opt-in policy for expensive lanes.
- Proof obligation: the certification DAG must stay acyclic, deterministic, cache-valid, and compositional. Existing `crates/build/z3` DAG proofs already give a foothold for this.

## Mathematical Model

- Variables:
  - `R`: set of runtime test cases
  - `P`: set of solver obligations
  - `A`: set of attrition lanes
  - `B`: set of benchmark lanes
  - `E(c)`: emitted evidence for certification case `c`
  - `Policy(c)`: required evidence classes for case `c`
- Invariants:
  - A certification case only passes if every required evidence class satisfies its acceptance predicate.
  - `runtime_pass(r)` means the Kain execution path completed without failed assertions or runtime errors.
  - `proof_pass(p)` means the solver returned the expected class, usually `unsat` for safety or `sat` for witness search.
  - `attrition_pass(a)` means invariants and teardown-closure predicates hold under the declared stress profile.
  - `benchmark_pass(b)` means declared metrics meet floor/shape predicates, not just "the executable ran".
- Objective:
  - Maximize semantic coverage per authored validation case while minimizing duplicated truth across runtime tests, proof packs, and external scripts.
- Bad states:
  - A runtime test passes while the stronger solver negation is satisfiable.
  - A solver obligation passes in a comment-only lane but cannot be replayed or attached to authored Kain code.
  - A blade claims readiness without benchmark or attrition evidence even though those lanes are required by policy.
  - A `build.kn` certification graph has hidden sidecar logic outside the declared task DAG.
- Simplifying assumptions:
  - The first solver-native authored surface should target quantifier-light fragments and existing proof helpers before attempting the full Z3 feature universe.
  - The first `build.kn` certification graph can be local/workspace-only and opt-in for heavy lanes.

## Z3 Claims

1. The right first-class authored surface is not raw Z3 FFI; it is a Kain proof DSL with a raw SMT/FFI escape hatch.
2. `std::test` should expose solver-shaped test intent, while durable subsystem proof packs remain the place for long-lived low-level arithmetic and ABI invariants.
3. `kain-test` should support two proof backends:
   - inline proof lowering from Kain-authored `std::test::z3` constructs
   - proof-pack / raw-SMT replay for existing durable cases
4. `build.kn` should gain first-class `test`, `proof`, `benchmark`, `attrition`, and `certify` task kinds instead of forcing these lanes into ad hoc scripts or folder conventions.
5. Cargo-style progress UX is worth adding, but only as a shell improvement. The real frontier is certification semantics, not prettier imitation.

## Evidence And Sources

- Local:
  - `crates/test/src/lib.rs`: current directive modes, raw `//@ smt2:` proof handling, and proof evidence report shape.
  - `stdlib/test.kn`: current authored `TestOutcome` vocabulary with no solver binding surface yet.
  - `crates/core/src/runtime.rs`: runtime `test foo ... ok` per-test streaming already exists for typed Kain tests.
  - `crates/cli/src/main.rs`: `kain test` currently prints only the final suite summary and failure list.
  - `crates/blades/src/lib.rs`: `KainBuildTaskSection` already provides a generic task schema suitable for certification-lane expansion.
  - `crates/build/src/workspace.rs`: build task kinds are currently missing first-class test/proof/benchmark/attrition/certify variants.
  - `benchmark/README.md`: benchmark already has rich telemetry, report generation, case manifests, and wrapper workflows.
  - `attrition/README.md`: attrition already has deterministic replay, sabotage profiles, minimization, and certification-shaped telemetry.
  - `runtime/native/src/core/z3/README.md`: runtime already treats durable Z3 packs as first-class substrate verification.
- External:
  - None needed for the core thesis. This is mostly a repo-shape question, not a market-survey question.

## Dead Ends

- Dumping the entire Z3 C API into `std::test` would create an unreadable, solver-leaking authored surface and turn every proof into host-FFI soup.
- Treating solver obligations as only `//@ smt2:` comments keeps proof power external to the language and prevents reusable Kain-side proof combinators.
- Treating `benchmark/` and `attrition/` as forever-separate scripts would miss the chance to make `build.kn` the certification authority surface.

## Conclusion

Current thesis:

- Yes, `kain-test` should absolutely "speak the language of Z3".
- No, that should not mean "ship every raw Z3 binding into normal user code" as the primary surface.
- The right evolution is:
  1. `std::test::z3` as a curated authored proof vocabulary
  2. `kain-test` lowering that vocabulary into solver runs and model/witness reports
  3. `build.kn` promotion into a certification DAG that unifies tests, proofs, attrition, and benchmarks

Proposed authored shape:

```kn
use std::test

test ptr_offset_stays_in_bounds:
    let goal = test::z3.goal("ptr-offset-stays-in-bounds")
    goal.int("packet")
    goal.int("slot")
    goal.int("total_words")
    goal.assume("packet >= 0")
    goal.assume("packet < 64")
    goal.assume("slot >= 0")
    goal.assume("slot < 4")
    goal.assume("total_words == 256")
    goal.assert("((packet * 4) + slot) >= total_words")
    return test::z3.expect_unsat(goal)
```

That exact syntax is only a sketch, but the semantic target is clear: authored Kain proof intent, lowered by `kain-test`, with raw-SMT and proof-pack escape hatches still available for alien math and subsystem packs.
