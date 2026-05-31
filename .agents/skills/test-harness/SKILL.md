---
name: test-harness
description: Use when running, extending, debugging, validating, or reviewing Kain's source certification harness, including `crates/check`, `crates/test`, the `kain check` and `kain test` CLI commands, compiletest-style directives, ignored or known-bug handling, proof evidence, stdout JSON reports, file-backed JSON reports, and `smoketest/kain-test` fixtures. Use this for harness, fixture, and report behavior, not for authored Kain feature work or runtime implementation.
---

# Test Harness

Use this skill for the repo's source-certification lane. It owns harness semantics, fixture behavior, report shape, and operator workflows around `kain check` and `kain test`.

## Trigger Surface

- `kain check` or `kain test` behaves incorrectly.
- A new directive, report field, ignore mode, or proof mode needs to be added.
- `smoketest/kain-test` needs new coverage for an existing certification behavior.
- A task is about harness compatibility, report output, or CLI-facing test certification rather than the underlying compiler/runtime bug.

## Ownership Boundary

- This skill owns `crates/check`, `crates/test`, the thin CLI shell for `check` and `test`, and the smoke fixtures under `smoketest/test/`.
- If a failing test reveals a parser, typechecker, lowering, or semantic-engine bug, preserve the repro here and hand the fix to `bootstrap-core`.
- If runtime `test` execution or teardown is broken, keep the harness repro here and co-trigger `runtime-core`.
- If the issue is authored Kain test vocabulary in `stdlib/test.kn`, co-trigger `lang-stdlib`.
- Do not turn this skill into a general "fix whatever test exposed" lane. Its job is certification plumbing.

## Source Of Truth

- `crates/check/src/lib.rs`: source discovery, validation flow, target naming, counts, capabilities, `CheckReport`.
- `crates/test/src/lib.rs`: directive parsing, pass/fail/proof modes, skipped and ignored semantics, suite reporting, runtime dispatch.
- `crates/core/src/runtime.rs`: execution of Kain `test` items once the harness dispatches them.
- `crates/cli/src/main.rs`: `Check` and `Test` command flags, stdin support, summaries, stdout JSON, and JSON writing.
- `stdlib/test.kn`: authored test outcome vocabulary.
- `smoketest/test/`: small CLI-facing certification suite.
- `docs/cli/check-and-test.md`: operator contract.

## Working Rules

1. Reproduce with the smallest smoke case before touching the harness.
2. Keep reusable semantics in `kain-check` and `kain-test`, not in the CLI.
3. Preserve directive compatibility:
   - support `//@` and `#@`
   - current modes: `check-pass`, `check-fail`, `run-pass`, `run-fail`, `kain-test`, `prove-pass`, `prove-sat`
   - current metadata: `mode`, `target`, `error`, `expect-error`, `proof-expect`, `smt2`, `ignore`, `skip`, `known-bug`
4. Treat skipped cases as success-neutral. `--ignored` executes ignored and known-bug cases and may fail the suite.
5. Keep report schemas explicit. If JSON changes, update fixtures and docs in the same task.
6. Every new harness behavior needs at least one focused `smoketest/kain-test` repro.

## Validation

```powershell
cargo test -p kain-check -p kain-test --target-dir target\codex-test-harness
cargo test -p kain-core run_tests --target-dir target\codex-test-harness
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-test-harness
target\codex-test-harness\debug\kain.exe check smoketest\test\check_pass.kn
"fn main() -> Int:`n    return 0`n" | target\codex-test-harness\debug\kain.exe check -
target\codex-test-harness\debug\kain.exe check smoketest\test\check_pass.kn --json
target\codex-test-harness\debug\kain.exe test smoketest\kain-test --json
target\codex-test-harness\debug\kain.exe test smoketest\kain-test --json-out target\codex-test-harness\kain-test-report.json
target\codex-test-harness\debug\kain.exe test smoketest\kain-test --ignored
```

Inspect the JSON report and smoke counts before summarizing. If a harness change exposed a deeper bug, keep the repro fixture here and move the implementation fix to the owning `bootstrap-*` or `runtime-*` skill.
