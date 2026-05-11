# Check And Test

`kain check` and `kain test` are the source-level validation pipeline for Kain.
They are backed by reusable crates instead of CLI-only code:

- `crates/kain-check` owns source discovery, frontend validation, target-aware capability summaries, and structured check reports.
- `crates/kain-test` owns Rust-inspired `//@` directives, pass/fail modes, ignored cases, test-item execution, and structured suite reports.
- `crates/cli` parses flags, prints summaries, and writes optional JSON reports.

## Commands

```powershell
kain check path\to\file.kn
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

`kain check` discovers `.kn` and `.ks` files, skips generated/build folders, and runs the normal Kain frontend for the selected target without emitting backend artifacts.

The check report records:

- file path
- target name
- pass/fail status
- typed item count
- typed test count
- required runtime capabilities from the runtime-contract bundle
- frontend error text when checking fails

Use `--target` when the same source needs to be checked against a specific backend profile. The default is `run`, matching the interpreter-oriented local authoring loop.

## Test Modes

`kain test` uses `kain-check` for check modes and the Kain runtime for run/test modes.

| Mode | Directive | Meaning |
| --- | --- | --- |
| `check-pass` | `//@ check-pass` | Frontend validation must pass for the selected target. |
| `check-fail` | `//@ check-fail` | Frontend validation must fail. |
| `run-pass` | `//@ run-pass` | Interpret-mode execution must pass. |
| `run-fail` | `//@ run-fail` | Interpret-mode execution must fail. |
| `kain-test` | `//@ kain-test` | Run Kain `test` items through the runtime test lane. |

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
- `//@ mode: check-pass`
- `//@ target: rust`
- `//@ error: expected diagnostic substring`
- `//@ expect-error: expected diagnostic substring`
- `//@ ignore`
- `//@ ignore: reason`
- `//@ skip: reason`
- `//@ known-bug: issue or reason`

Ignored, skipped, and known-bug cases are reported as `skipped` and do not make the suite fail unless `--ignored` is passed.

## Fixture Rule

Put harness-level proof fixtures under `smoketest/kain-test` or a focused suite-specific folder. Prefer directives inside source files over sidecar metadata until the suite needs snapshots, revisions, or target matrices.

## Expansion Path

The next natural layers are snapshot comparison, revision matrices, target-conditional directives, and bless/update workflows. Add those to `kain-test` first, then expose them through CLI flags. Do not grow ad hoc validation scripts that duplicate harness semantics.
