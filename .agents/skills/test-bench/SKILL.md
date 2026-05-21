---
name: test-bench
description: Use when running, extending, debugging, validating, or reviewing Kain's benchmark certification lane under `benchmark/`, including `benchmark/benchmarks.json`, `benchmark/run.py`, wrapper lanes, report artifacts, history tracking, fairness and maturity notes, specialized GPU/WASM/FFI suites, and the benchmark blade console. Use this to prove or analyze performance claims, not to implement compiler, runtime, or authored Kain features directly.
---

# Test Bench

Use this skill for Kain's performance-proof lane. It owns benchmark manifests, runner behavior, fairness notes, telemetry/reporting, and operator workflows for performance evidence.

## Trigger Surface

- A task asks for benchmark runs, benchmark regression analysis, or performance evidence.
- `benchmark/run.py`, `run_fast.py`, `run_sim.py`, `run_wrapper.py`, or wrapper configs need changes.
- A benchmark report, baseline-cache policy, telemetry field, or fairness note needs to change.
- A new benchmark category belongs in the certification lane rather than in compiler/runtime implementation.

## Ownership Boundary

- This skill owns `benchmark/benchmarks.json`, the Python runners and wrappers, report generation, history capture, and the benchmark blade console.
- If the benchmark shows a slow compiler or runtime path, keep the reproduction and report work here, then hand the implementation fix to the owning `bootstrap-*` or `runtime-*` skill.
- If a task needs new authored Kain benchmark rows, co-trigger `lang-authoring`, `lang-semantics`, `lang-gpu`, or `lang-c-abi-ffi` as appropriate. This skill still owns the benchmark contract around that row.
- If the work is repo release gating rather than benchmark execution/reporting, co-trigger `tool-release-readiness`.

## Source Of Truth

- `benchmark/benchmarks.json`: case ids, language subsets, maturity labels, fairness notes, telemetry metadata, runtime manifest overrides.
- `benchmark/run.py`: main suite runner, report writer, baseline-cache policy, history persistence.
- `benchmark/run_fast.py`, `benchmark/run_sim.py`, `benchmark/run_wrapper.py`: reduced or specialized wrapper entrypoints.
- `benchmark/wrappers/*.json`: data-driven wrapper plugin configuration.
- `benchmark/wasm/wasm_cases.json` and `benchmark/gpu/gpu_cases.json`: dedicated WASM and GPU suite manifests.
- `benchmark/latest.md` and `benchmark/out/reports/latest.llm.md`: live benchmark truth before historical timestamp dives.
- `benchmark/out/history/benchmark_history.sqlite3`: comparable-run regression warehouse.
- `benchmark/blades/kain-benchmark/src/*.kn` and `benchmark/kain-benchmark.exe`: native benchmark console.

## Working Rules

1. Start from `benchmark/latest.md` and `benchmark/out/reports/latest.llm.md` before browsing timestamped history.
2. Keep benchmark behavior data-driven in manifests or wrapper configs. Prefer manifest metadata over hardcoded Python branches.
3. Every row needs a deterministic checksum or exit-code guard so the benchmarked work cannot disappear silently.
4. Be honest with `maturity`, `fairness_note`, and `language_notes`. Proxy wins are evidence, not final victory.
5. When a row needs a case-local `KAIN.toml` plus `use c::...`, compile from the case directory so manifest lookup stays honest.
6. Specialized suites stay specialized:
   - WASM parity under `benchmark/wasm/`
   - ABI-tax probes under `benchmark/ffi_boundary/`
   - SPIR-V and hardware telemetry under `benchmark/gpu/`

## Validation

```powershell
python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py
python benchmark/run.py --case scalar_mix,branch_dispatch,native_map_lookup --runs 1 --warmups 0 --latest-stem latest_cache_probe --minimal-name latest_cache_probe.md
python benchmark/run.py --case semantic_singularity_crucible --languages kain --runs 1 --warmups 0 --timeout 900
python benchmark/run_wrapper.py fast --case actor_mailbox_erlang --runs 1 --warmups 0 --timeout 900
python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300
py -3 tools/bazel/sync_native_runtime_builds.py --check
```

Run the cache probe twice when touching baseline reuse. The second pass should show foreign baseline hits. Inspect `benchmark/latest.md` and `benchmark/out/reports/latest.llm.md` before making any performance claim.
