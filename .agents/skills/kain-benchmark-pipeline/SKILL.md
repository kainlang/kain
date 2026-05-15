---
name: kain-benchmark-pipeline
description: Use when adding, changing, running, or reviewing the Kain vs Rust LLVM benchmark lane under benchmark/, including paired .kn/.rs cases, benchmark/benchmarks.json, benchmark/run.py, generated HTML reports, and fairness/maturity notes for Kain pressure tests.
---

# Kain Benchmark Pipeline

## Contract

- `benchmark/benchmarks.json` is the source of truth for the suite, cases, source paths, maturity labels, and fairness notes.
- Every benchmark case must have paired source files:
  - `benchmark/cases/<case>/main.kn`
  - `benchmark/cases/<case>/main.rs`
- Case programs must not use external language dependencies. Rust may use `std`; Kain may use language/runtime builtins and local imports.
- The Python runner may use the standard library for orchestration, timing, JSON, and HTML output.
- Generated outputs belong under `benchmark/out/` and should stay ignored except `benchmark/out/.gitignore`.

## Runner

- Main command: `python benchmark/run.py`
- Focus one case: `python benchmark/run.py --case contention_wall --runs 3 --warmups 1`
- Pin Kain compiler: `python benchmark/run.py --kain-exe D:\Kain-Lang\target\release\kain.exe`
- The runner prefers a direct Bazel-built release `kain.exe` because the Windows PowerShell launcher can mis-handle forwarded `-o`.
- Benchmark-native tuning defaults to `KAIN_NATIVE_PROFILE=benchmark-release` with `opt-level=3`, `target-cpu=native`, and no debug info unless you intentionally override it.
- Reports are written to:
  - `benchmark/out/reports/latest.html`
  - `benchmark/out/reports/latest.json`
  - timestamped `benchmark/out/reports/<stamp>.html`

## Case Design

- Keep benchmark constants local inside `main` or helper functions. Current Kain LLVM codegen may not resolve top-level `const` in small standalone benchmark functions.
- Include deterministic checksum/exit-code validation so benchmarked work cannot disappear silently.
- If Kain does not yet expose the exact runtime primitive needed, keep the case but mark `maturity` as `proxy`, `semantic-proxy`, or `dispatch-skeleton` in `benchmarks.json`.
- Never claim a proxy is a completed win. Use `fairness_note` to explain the semantic gap.

## Current Pressure Cases

- `contention_wall`: Rust uses 100 OS threads and `AtomicI64`; Kain currently uses a zero-lock `collapse` proxy over the same total increment count.
- `ghost_mirror`: Rust uses std TCP loopback for a 1 MiB payload; Kain uses entangle-backed in-process world mirroring plus helper-owned payload mutation.
- `evolutionary_loop`: Rust uses runtime feature detection; Kain uses `converge`/`orchestrate` dispatch syntax as the future autotuning slot.
- `ownership_memory`: direct `collapse`/`observe`/`decay` smoke against Rust `Box` ownership.

## Current Basic Edge Cases

- `branch_dispatch`: scalar branch-heavy dispatch. It uses `if` today because scalar `match` in the standalone hot loop built but trapped at runtime.
- `call_chain`: small function graph in a hot loop.
- `memory_stream`: sequential buffer write/read through Kain helper-owned memory versus Rust `Vec<i64>`.
- `alloc_churn`: many small allocation/write/read/lifetime-end cycles.
- `struct_method`: aggregate construction plus explicit `score_pair(pair)` field access. Avoid receiver method field access until that native codegen gap is fixed.
- `option_result`: Option/Result tagged value creation, branching, and unwrap paths.
- `scalar_mix`: top-level const lowering and a checksum guard.
- `recursive_sum`: recursion and call-stack lowering in a tight loop.
- `string_ops`: ASCII substring search plus string length/indexing over top-level string consts.
- `array_scan`: fixed-array indexing and weighted accumulation.

## Validation

- `python -m py_compile benchmark/run.py`
- `python benchmark/run.py --runs 3 --warmups 1`
- Inspect `benchmark/out/reports/latest.html` and `latest.json` before summarizing results.
