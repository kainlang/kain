---
name: kain-attrition-pipeline
description: Use when adding, changing, running, or validating the attrition runtime-certification lane under attrition/, including attrition/attritions.json, invariants.json, attrition/run.py, native C attrition cases, runtime/native attrition telemetry hooks, replay/minimization artifacts, sabotage modes, and teardown-closure audits.
---

# Kain Attrition Pipeline

## Contract

- `attrition/attritions.json` is the source of truth for case ids, titles, lane kind, determinism tier, op scales, expected-fail sabotages, runtime profile defaults, and runtime manifest selection.
- `attrition/invariants.json` is the explicit invariant catalog. Keep every invariant data-driven:
  - `id`
  - `owner_subsystem`
  - `formula`
  - `units`
  - `idle_floor`
  - `allowed_permanent_floor_entries`
  - `sabotage_knobs_expected_to_trip_it`
  - `isolate_lane`
  - `mixed_lane_membership`
- `attrition/run.py` owns compilation, execution, telemetry capture, replay command emission, deterministic failure minimization, and report generation.
- `runtime/native_attrition_runtime.toml` is the default lean native runtime bundle for attrition lanes.
- Internal runtime counters are the primary truth. RSS is secondary and not yet the gating signal.

## Determinism

- Tier 1: exact replay and exact checksum expectation.
- Tier 2: exact failure class / replay-token family.
- Tier 3: advisory chaos only; structural outcome matters more than exact interleaving.
- The current implemented lanes are all deterministic foundation lanes. Advisory async-chaos is planned, not yet the gate.
- Virtual-time lanes must prove time provenance: no raw `clock()`, no raw OS sleep, no timeout path bypassing the attrition clock hooks.

## Profiles

- `release-instrumented`
  - optimized attrition build with telemetry enabled
- `debug-assert`
  - low-optimization replay/diagnostic build
- `poison-allocator`
  - release build with poison-on-free, quarantine, and fragmentation noise
- `virtual-time`
  - release build with virtual-time execution enabled
- `mixed-poison-virtual-time`
  - combined poison plus virtual-time profile

## Current Lanes

- `saturated_rc_hot_object`
  - hot shared-object retain/release storm plus string/decimal churn
- `virtual_time_async_timer`
  - async sleep/task cancellation churn under virtual time
- `actor_reply_port_recycle`
  - actor reply-port generation invalidation and immediate slot reuse
- `process_slot_recycle`
  - process handle reuse and stale-handle rejection
- `mixed_runtime_boss`
  - RC + async/timer + actor reply-port + process slot reuse in one deterministic mixed lane

Each new invariant should map to:
- one isolate lane
- one sabotage proof
- one mixed-lane membership

## Running

- Full default run:
  - `python attrition/run.py`
- Small deterministic gate:
  - `python attrition/run.py --scale small --profile release-instrumented --timeout 300`
- One case:
  - `python attrition/run.py --case virtual_time_async_timer --profile virtual-time`
- One sabotage proof:
  - `python attrition/run.py --case virtual_time_async_timer --scale small --profile release-instrumented --sabotage skip_task_dispose --timeout 240`
- Mixed lane with poison plus virtual time:
  - `python attrition/run.py --case mixed_runtime_boss --profile mixed-poison-virtual-time`

## Artifacts

- Root snapshot:
  - `attrition/latest.md`
- Latest suite reports:
  - `attrition/out/reports/latest.llm.md`
  - `attrition/out/reports/latest.json`
- Timestamped suite reports:
  - `attrition/out/reports/<stamp>.llm.md`
  - `attrition/out/reports/<stamp>.json`
- Per-case build/output:
  - `attrition/out/build/<case>/<profile>/`
- Raw last case result:
  - `attrition/out/build/<case>/<profile>/last_result.json`

Case artifacts should include:
- replay command
- runtime manifest provenance
- determinism tier
- sabotage mode
- expected-fail match state when sabotage is active
- minimized deterministic repro op count unless `--no-minimize` is used

## Sabotage And Expected-Fail

- Implement sabotage in a manifest-declared way; do not hide it in case-local tribal knowledge.
- Every expected-fail sabotage should produce a red lane that the runner recognizes as an expected fail, not a silent pass.
- Use sabotage to test the tester:
  - skip one final release
  - skip one destroy/close/dispose path
  - keep stale-handle rejection proofs live
- The current strong proof point is `virtual_time_async_timer` with `skip_task_dispose`, which should leak async-task occupancy and match an expected-fail report.

## Runtime Truths

- `abi_process_wait(...)` contract:
  - `1` means the child exited
  - `0` means timeout
  - `< 0` means error
- Actor occupancy floor:
  - `actor_occupancy_low_word == 1` is healthy idle state because bit 0 is the reserved invalid actor slot
- `runtime/native/src/core/async.c` task disposal and timer cancellation are one lifecycle seam. If async/timer closure regresses, inspect timer disarm under the task/global locks before blaming the lane logic.

## Proofs

- The attrition flight-recorder ring-copy math is solver-backed at:
  - `runtime/native/src/core/z3/proofs-experimental/attrition-event-ring-copy-window-bounds.smt2`
- The saved proof report is:
  - `z3/reports/20260517T054056Z-attrition-event-ring-copy-window-bounds.json`
- Keep exploratory attrition SMT under `runtime/native/src/core/z3/proofs-experimental` until the invariant deserves promotion into the main runtime proof pack.

## Validation

- `python -m py_compile attrition/run.py`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/async.c`
- `python attrition/run.py --scale small --profile release-instrumented --timeout 300`
- `python attrition/run.py --case virtual_time_async_timer --scale small --profile release-instrumented --sabotage skip_task_dispose --timeout 240`
- Inspect `attrition/out/reports/latest.llm.md` and `attrition/latest.md` before summarizing lane health.
