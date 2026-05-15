---
name: kain-benchmark-pipeline
description: Use when adding, changing, running, or reviewing the multi-language benchmark lane under benchmark/, including Kain/Rust/JavaScript/Python cases, benchmark/benchmarks.json, benchmark/run.py, LLM-readable reports, the benchmark blade console, and fairness/maturity notes for Kain pressure tests.
---

# Kain Benchmark Pipeline

## Contract

- `benchmark/benchmarks.json` is the source of truth for the suite, cases, source paths, maturity labels, language notes, and fairness notes.
- Every normal benchmark case must have dependency-free paired source files:
  - `benchmark/cases/<case>/main.kn`
  - `benchmark/cases/<case>/main.rs`
  - `benchmark/cases/<case>/main.js`
  - `benchmark/cases/<case>/main.py`
- Case programs must not use external language dependencies. Rust may use `std`; JavaScript may use Node builtins; Python may use the standard library; Kain may use language/runtime builtins and local imports.
- The Python runner may use the standard library for orchestration, timing, JSON, and Markdown report output.
- Generated outputs belong under `benchmark/out/` and should stay ignored except `benchmark/out/.gitignore`.
- The native benchmark console lives under `benchmark/blades/kain-benchmark`, and the user-facing executable is `benchmark/kain-benchmark.exe`.

## Runner

- Main command: `python benchmark/run.py`
- Focus one case: `python benchmark/run.py --case contention_wall --runs 3 --warmups 1`
- Run a language subset: `python benchmark/run.py --languages js,py --runs 1 --warmups 0`
- Pin Kain compiler: `python benchmark/run.py --kain-exe D:\Kain-Lang\target\release\kain.exe`
- The runner prefers a direct Bazel-built release `kain.exe` because the Windows PowerShell launcher can mis-handle forwarded `-o`.
- Benchmark-native tuning defaults to `KAIN_NATIVE_PROFILE=benchmark-release` with `opt-level=3`, `target-cpu=native`, no debug info, and `KAIN_RUNTIME_MANIFEST_PATH=runtime/native_core_runtime.toml` unless you intentionally override it in code for an app/vendor benchmark.
- Reports are written to:
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest.json`
  - timestamped `benchmark/out/reports/<stamp>.llm.md`
  - timestamped `benchmark/out/reports/<stamp>.json`
- HTML is no longer a report format. If `benchmark/out/reports/latest.html` exists after a run, treat that as stale-output cleanup debt.

## Blade Console

- Source: `benchmark/blades/kain-benchmark/src/*.kn`
- Executable: `benchmark/kain-benchmark.exe`
- Build proof from repo root:

```powershell
.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName D:\Kain-Lang\benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm
```

- Validate UI with `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH` under `benchmark/blades/kain-benchmark/.kain/run/` and `KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES`.
- The Win32/GL BMP readback may appear flipped/mirrored in viewers; still assert non-empty screenshot size and inspect enough to prove the UI surface rendered.
- `KAIN_BENCH_AUTORUN=quick` runs the scalar multi-language smoke at startup. Buttons use the same process runner path.

## Case Design

- Keep benchmark constants local inside `main` or helper functions unless the case is explicitly exercising top-level const behavior.
- Include deterministic checksum/exit-code validation so benchmarked work cannot disappear silently.
- If Kain does not yet expose the exact runtime primitive needed, keep the case but mark `maturity` as `proxy`, `semantic-proxy`, or `dispatch-skeleton` in `benchmarks.json`.
- Never claim a proxy is a completed win. Use `fairness_note` and `language_notes` to explain semantic gaps.
- JavaScript and Python lanes should mirror algorithmic shape where possible, but avoid importing npm/pip dependencies or measuring unrelated framework overhead.

## Current Pressure Cases

- `contention_wall`: Rust uses 100 OS threads and `AtomicI64`; Kain currently uses a zero-lock `collapse` proxy; JavaScript/Python use scalar proxies to avoid worker/GIL overhead dominating the comparison.
- `ghost_mirror`: Rust/JavaScript/Python use stdlib TCP loopback for a 1 MiB payload; Kain uses entangle-backed in-process world mirroring plus helper-owned payload mutation.
- `evolutionary_loop`: Rust/JavaScript/Python use runtime feature detection or equivalent branch dispatch; Kain uses `converge`/`orchestrate` dispatch syntax as the future autotuning slot.
- `ownership_memory`: direct `collapse`/`observe`/`decay` smoke against ordinary boxed/object ownership lanes.

## Current Basic Edge Cases

- `branch_dispatch`: scalar branch-heavy dispatch. It uses `if` today because scalar `match` in the standalone hot loop built but trapped at runtime.
- `call_chain`: small function graph in a hot loop.
- `memory_stream`: sequential buffer write/read through Kain helper-owned memory versus ordinary language arrays/vectors.
- `alloc_churn`: many small allocation/write/read/lifetime-end cycles.
- `struct_method`: aggregate construction plus explicit `score_pair(pair)` field access. Avoid receiver method field access until that native codegen gap is fixed.
- `option_result`: Option/Result tagged value creation, branching, and unwrap paths.
- `scalar_mix`: top-level const lowering and a checksum guard.
- `recursive_sum`: recursion and call-stack lowering in a tight loop.
- `string_ops`: ASCII substring search plus string length/indexing over top-level string consts.
- `array_scan`: fixed-array indexing and weighted accumulation.

## Validation

- `python -m py_compile benchmark/run.py`
- Syntax-check all JS/Python case files.
- `python benchmark/run.py --languages javascript,python --runs 1 --warmups 0 --timeout 300`
- `python benchmark/run.py --case scalar_mix --languages kain,rust,javascript,python --runs 1 --warmups 0 --timeout 300`
- Build `benchmark/kain-benchmark.exe` with the blade compile helper and capture a non-empty native UI screenshot.
- Inspect `benchmark/out/reports/latest.llm.md` and `latest.json` before summarizing results.
