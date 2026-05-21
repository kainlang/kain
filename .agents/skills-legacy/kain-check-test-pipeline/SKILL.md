---
name: kain-check-test-pipeline
description: Use when adding, changing, debugging, validating, or reviewing Kain's source checking and testing pipeline, including crates/kain-check, crates/kain-test, the `kain check` and `kain test` CLI commands, compiletest-style `//@` directives, Z3 proof tests, ignored/known-bug cases, JSON reports, runtime `test` execution, and smoketest/kain-test fixtures.
---

# Kain Check/Test Pipeline

Use this skill for the Kain source validation stack. Keep Rust compiletest as a behavior reference, but keep Kain's architecture owned by `kain-check`, `kain-test`, `kain-core`, and the thin CLI shell.

## Source Of Truth

- `crates/kain-check/src/lib.rs`: file discovery, frontend validation, target names, item/test counts, required capabilities, `CheckReport`.
- `crates/kain-test/src/lib.rs`: directive parsing, pass/fail/proof modes, skipped/ignored cases, suite reports, runtime test dispatch, Z3 proof evidence.
- `stdlib/test.kn`: authored Kain-side test outcome vocabulary; keep source-facing outcome helpers here, not in host glue.
- `crates/kain-core/src/runtime.rs`: actual execution of Kain `test` items; nested module tests must execute, not just count.
- `crates/cli/src/main.rs`: `Check` / `Test` command flags, text summaries, stdin support, JSON report writing.
- `smoketest/kain-test/`: small CLI-facing directive suite.
- `docs/cli/check-and-test.md` and `ARCHITECTURE.md`: operator and future-agent docs.

## Workflow

1. Check current source before editing:
   - `rg -n "KainTestMode|KainTestStatus|run_check_command|run_test_command|run_tests" crates`
   - `target\\codex-check-test\\debug\\kain.exe test smoketest\\kain-test` if the binary already exists.
2. Put reusable semantics in crates, not the CLI:
   - checking semantics go in `kain-check`
   - suite/directive/report semantics go in `kain-test`
   - CLI should only parse flags, call crates, print summaries, and write JSON.
3. Preserve directive compatibility:
   - support `//@` and `#@`
   - current modes: `check-pass`, `check-fail`, `run-pass`, `run-fail`, `kain-test`, `prove-pass`, `prove-sat`
   - current metadata: `mode`, `target`, `error`, `expect-error`, `proof-expect`, `smt2`, `ignore`, `skip`, `known-bug`
4. Treat skipped cases as success-neutral:
   - skipped cases increment `skipped`, not `failed`
   - `--ignored` should execute ignored/known-bug cases and may fail the suite.
5. Keep report schemas explicit:
   - add fields intentionally and update docs/smokes when JSON changes
   - report `target: "test"` for Kain `test` execution and proof modes, and `target: "run"` for run modes.
   - proof cases should include `proof` evidence with solver, expected result, actual result, and SMT2 obligation line count.

## Validation

Use a focused validation ladder before broad workspace tests:

```powershell
rustfmt --edition 2021 crates\kain-check\src\lib.rs crates\kain-test\src\lib.rs crates\kain-core\src\runtime.rs crates\cli\src\main.rs
cargo test -p kain-check -p kain-test --target-dir target\codex-check-test
cargo test -p kain-core run_tests --target-dir target\codex-check-test
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-check-test
target\codex-check-test\debug\kain.exe check smoketest\kain-test\check_pass.kn
"fn main() -> Int:`n    return 0`n" | target\codex-check-test\debug\kain.exe check -
target\codex-check-test\debug\kain.exe test smoketest\kain-test --json target\codex-check-test\kain-test-report.json
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check
```

For `--ignored`, expect the smoke suite to fail if its ignored fixture is intentionally bad. Validate that intentionally:

```powershell
target\codex-check-test\debug\kain.exe test smoketest\kain-test --ignored
```

## Expansion Rules

- Add snapshot comparison, revisions, target-conditional directives, compiler-extracted proof obligations, and bless/update workflows inside `kain-test` first.
- Add CLI flags only after the library API can express the behavior.
- Add or update `smoketest/kain-test` fixtures for every new harness behavior.
- Update `docs/cli/check-and-test.md`, `docs/reference/command-matrix.md`, `docs/reference/feature-matrix.md`, `ARCHITECTURE.md`, and `MEMORY.md` for material pipeline changes.
- Do not duplicate harness behavior in ad hoc Python/PowerShell scripts unless the script is only a thin invoker over `kain test`.
