# 2026-05-21 Direct Ask Prepare + Harness Fix Benchmark Assessment

## Source of truth

- full report: `benchmark/out/reports/latest_full_after_direct_ask_harness_fix.llm.md`
- full json: `benchmark/out/reports/latest_full_after_direct_ask_harness_fix.json`
- focused hygiene probe: `benchmark/out/reports/latest_benchmark_hygiene_probe.llm.md`

## What this run proved

- the actor ask path speedup is real
- the prior full-suite `FAIL` was benchmark hygiene noise, not a real semantic regression
- the clean full suite now passes end-to-end

## Material improvements

- `actor_ownership_backpressure`
  - `459.963 ms -> 302.735 ms`
  - `1.52x` faster
- `semantic_fabric_relay`
  - `114.693 ms -> 89.066 ms`
  - `1.29x` faster
- `pulse_teleport_decay_mesh`
  - `109.894 ms -> 93.344 ms`
  - `1.18x` faster

## Current honest frontier

### Highest-value gaps vs fastest competitor

1. `actor_ownership_backpressure`
   - Kain `302.735 ms`
   - C++ `18.340 ms`
   - `16.51x` gap
2. `recursive_sum`
   - Kain `112.582 ms`
   - C++ `9.657 ms`
   - `11.66x` gap
3. `semantic_fabric_relay`
   - Kain `89.066 ms`
   - C++ `10.658 ms`
   - `8.36x` gap
4. `pulse_teleport_decay_mesh`
   - Kain `93.344 ms`
   - C++ `16.827 ms`
   - `5.55x` gap
5. `semantic_host_bridge_fusion`
   - Kain `1136.830 ms`
   - C++ `855.081 ms`
   - `1.33x` gap

### Highest-value implemented rows still worth attacking

1. `recursive_sum`
2. `ownership_memory`
3. `memory_stream`
4. `sim_uv_velocity_grid`
5. `option_result`

## Suggested next work

### First choice

Keep attacking local actor ask/reply completion:

- direct same-turn completion for inline local asks
- fewer wait-slot writes
- less generation/ref bookkeeping on the hot success path

This is still the best leverage point because it can move three of the largest semantic rows together.

### Second choice

Attack `recursive_sum` in LLVM lowering:

- inspect emitted IR
- normalize pure self recursion into a loop when the shape is solver-provable

That row is too loud for an implemented benchmark and should be able to move by multiple x if the lowering is repaired.

### Third choice

Profile `semantic_host_bridge_fusion` for bridge-state hoisting:

- precompute invariant host/process metadata
- cut per-round bridge string and handle churn

## Fairness / honesty note

This run is cleaner than the prior one because both benchmark lies were fixed:

- stale native-runtime temp file cleanup no longer aborts valid builds
- Windows executable visibility lag no longer gets reported as a build failure

So the frontier above is a real speedup map, not a cache-lock artifact.
