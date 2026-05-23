# Check And Test

`kain check` and `kain test` are the source-level validation pipeline for Kain.
They are backed by reusable crates instead of CLI-only code:

- `crates/kain-check` owns source discovery, frontend validation, target-aware capability summaries, and structured check reports.
- `crates/kain-test` owns Rust-inspired `//@` directives, pass/fail modes, ignored cases, test-item execution, and structured suite reports.
- `crates/cli` parses flags, prints summaries, and writes optional JSON reports.

## Commands

```powershell
kain check path\to\file.kn
kain check path\to\portable_capsule.kn --target llvm
kain check path\to\suite --target rust --fail-fast --json target\check-report.json
Get-Content path\to\file.kn | kain check -

kain test smoketest\kain-test
kain test tests\ui --mode check-pass --fail-fast
kain test tests\ui --ignored --json target\kain-test-report.json
```

`kn` accepts the same commands:

```powershell
kn check src\main.kn
kn test smoketest\kain-test
```

## Check Pipeline

`kain check` discovers `.kn` and `.ks` files and runs the normal Kain frontend
for the selected target without emitting backend artifacts. If the input is a
capsule `.kn` file created by `kain amalgamate`, the CLI materializes it under
`.kain/cache/amalgamate/<state-hash>/workspace` first and then checks the
extracted entry or workspace tree. If sibling companion capsules with the same
capsule-set are present next to the primary capsule, they are merged into that
materialized workspace automatically before checking.

The check report records:

- file path
- target name
- pass/fail status
- typed item count
- typed test count
- required runtime capabilities from the runtime-contract bundle
- frontend error text when checking fails

Use `--target` when the same source needs to be checked against a specific backend profile. The default is `run`, matching the interpreter-oriented local authoring loop.

Directory-wide `kain check` skips generated cache roots such as `.kain`,
`target`, and `generated` so workspace checks stay focused on authored source.
If you intentionally want to validate a generated Kain file, pass that file
path directly.

## Test Modes

`kain test` uses `kain-check` for check modes, the Kain runtime for run/test modes,
and Z3 for embedded proof obligations.

| Mode | Directive | Meaning |
| --- | --- | --- |
| `check-pass` | `//@ check-pass` | Frontend validation must pass for the selected target. |
| `check-fail` | `//@ check-fail` | Frontend validation must fail. |
| `run-pass` | `//@ run-pass` | Interpret-mode execution must pass. |
| `run-fail` | `//@ run-fail` | Interpret-mode execution must fail. |
| `kain-test` | `//@ kain-test` | Run Kain `test` items through the runtime test lane. |
| `prove-pass` | `//@ prove-pass` | Run embedded SMT2 through Z3 and require `unsat`. |
| `prove-sat` | `//@ prove-sat` | Run embedded SMT2 through Z3 and require `sat` witnessability. |

If no mode directive is present, files containing a `test` item default to `kain-test`; other files default to `check-pass`.

## Directives

Directives use the Rust compiletest-style `//@` prefix. Kain also accepts `#@` for script-like files.

```kain
//@ check-pass
//@ target: rust

fn main() -> Int:
    return 0
```

Supported directives:

- `//@ check-pass`
- `//@ check-fail`
- `//@ run-pass`
- `//@ run-fail`
- `//@ kain-test`
- `//@ prove-pass`
- `//@ prove-sat`
- `//@ mode: check-pass`
- `//@ target: rust`
- `//@ error: expected diagnostic substring`
- `//@ expect-error: expected diagnostic substring`
- `//@ proof-expect: unsat`
- `//@ smt2: (assert false)`
- `//@ ignore`
- `//@ ignore: reason`
- `//@ skip: reason`
- `//@ known-bug: issue or reason`

Ignored, skipped, and known-bug cases are reported as `skipped` and do not make the suite fail unless `--ignored` is passed.

## Proof Tests

Proof tests are the first native step beyond Cargo-style example execution. A
`prove-pass` case treats the SMT2 as a negated safety claim and passes only when
Z3 returns `unsat`, meaning no counterexample exists under the encoded bounds. A
`prove-sat` case is useful for witness and synthesis lanes where a satisfying
model should exist.

```kain
//@ prove-pass
//@ smt2: (set-logic QF_LIA)
//@ smt2: (declare-const offset Int)
//@ smt2: (declare-const span Int)
//@ smt2: (assert (>= offset 0))
//@ smt2: (assert (< offset span))
//@ smt2: (assert (or (< offset 0) (>= offset span)))
```

The harness uses `z3` from `PATH`, or `KAIN_Z3` when a specific solver binary is
required. Reports include proof evidence with solver name, expected result,
actual result, and SMT2 obligation line count.

## Fixture Rule

Put harness-level proof fixtures under `smoketest/kain-test` or a focused suite-specific folder. Prefer directives inside source files over sidecar metadata until the suite needs snapshots, revisions, or target matrices.

## Expansion Path

The next natural layers are snapshot comparison, revision matrices,
target-conditional directives, bless/update workflows, compiler-extracted proof
obligations, and native LLVM proof/test bundle execution. Add those to
`kain-test` first, then expose them through CLI flags. Do not grow ad hoc
validation scripts that duplicate harness semantics.
