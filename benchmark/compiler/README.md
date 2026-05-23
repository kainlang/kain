# Compiler Benchmark Lane

This lane answers a different question than the runtime benchmark suite:
how fast can Kain and Rust compile comparable authored workloads on this workstation?

## Goals

- Use a fresh release `kain` binary by default.
- Measure clean compile and warm rebuild timings separately.
- Emit telemetry from declaration counts, source size, native runtime cache reuse, and artifact size.
- Keep generated workload sources under `benchmark/out/build/compiler/`.
- Persist dedicated run history to `benchmark/out/history/compiler_history.sqlite3`.

## Commands

```powershell
python benchmark/run_compiler.py
python benchmark/run_compiler.py --case single_file_small --runs 1 --warmups 0
python benchmark/bench.py suite compiler
python benchmark/bench.py suite compiler -- --case module_fanout_large --runs 2 --warmups 0
```

## Workload Model

Cases live in `benchmark/compiler/cases.json`.

- `single_file_mesh`: one file per language with many structs, helper functions, and dispatch stages.
- `module_fanout`: many authored modules plus a shared helper module to pressure import and resolver work.

Each case generates deterministic Kain and Rust sources, compiles them, runs the resulting artifact once to validate the checksum, and then records:

- clean compile timings
- warm rebuild timings
- Kain native runtime object/archive reuse counts
- declaration/function/module counts
- actual source lines and bytes per language
- final artifact size

Each Kain sample gets its own `KAIN_RUNTIME_CACHE_DIR` under `benchmark/out/build/compiler/runtime_cache/` so cold and warm measurements stay reproducible instead of inheriting ambient cache state from the workload directory.

## Reports

- Snapshot: `benchmark/out/snapshots/latest_compiler.md`
- Full markdown: `benchmark/out/reports/latest_compiler.llm.md`
- Structured JSON: `benchmark/out/reports/latest_compiler.json`
- History DB: `benchmark/out/history/compiler_history.sqlite3`
