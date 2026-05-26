# Kain Benchmark Control Plane

This folder now uses a strict split between source, catalog, runners, and generated output so benchmark work can scale without turning into a dump ground.

UPDATE MAY25TH -- FUTURE BENCHMARKS SHOULD NOW RUN IN cases_v2 as we can now make multiple benchmarks in a single file 

## Layout

- `benchmark/bench.py`: unified orchestrator (`run`, `suite`, `list`, `report`, `compare`, `clean`, `doctor`).
- `benchmark/catalog/`: manifests, suite registry, retention policy.
  - `benchmarks.main.json`: full case catalog with `tags` + `suites` metadata.
  - `suites.json`: named suite definitions and defaults.
  - `retention.json`: cleanup policy profiles.
- `benchmark/cases/`: multi-language main-suite source cases.
- `benchmark/compiler/`: dedicated Kain-vs-Rust compile-time lane with generated workloads and separate history.
- `benchmark/runners/`: runner ownership namespace (current and future extracted modules).
- `benchmark/lanes/gpu/`: dedicated GPU/SPIR-V lane.
- `benchmark/lanes/wasm/`: dedicated WASM parity lane.
- `benchmark/lanes/ffi_boundary/`: dedicated FFI boundary lane.
- `benchmark/out/`: generated artifacts only (`reports/`, `build/`, `baselines/`, `history/`, `snapshots/`, etc).
- `benchmark/docs/`: benchmark docs and historical assessments.

## Main Commands

```powershell
python benchmark/bench.py list
python benchmark/bench.py run
python benchmark/bench.py run --tag semantic
python benchmark/bench.py run --suite smoke
python benchmark/bench.py suite full
python benchmark/bench.py suite compiler
python benchmark/bench.py suite gpu -- --case semantic_ping_pong --languages kain,cpp
python benchmark/bench.py report --stem latest
python benchmark/bench.py compare
python benchmark/bench.py clean --policy default --dry-run
python benchmark/bench.py clean --policy default
python benchmark/bench.py doctor
```

## Suite Model

Cases are now queryable by:

- `tags`: behavior/category labels (for example `core`, `semantic`, `systems`, `sim`, `net`, `ffi`, `gpu`).
- `suites`: named benchmark membership labels (for example `smoke`, `dev`, `full`, `nightly`).

Named suite presets are defined in `benchmark/catalog/suites.json`.

## Compatibility

These commands still work and forward into the reorganized structure:

```powershell
python benchmark/run.py
python benchmark/run_fast.py
python benchmark/run_compiler.py
python benchmark/run_sim.py
python benchmark/run_gpu.py
python benchmark/run_spirv.py
python benchmark/run_wasm.py
python benchmark/run_wrapper.py --list
```

## Output Hygiene

- Main snapshots now belong under `benchmark/out/snapshots/`.
- Full structured reports remain under `benchmark/out/reports/`.
- Use `bench.py clean` with `catalog/retention.json` profiles to prune stale artifacts and root clutter.
