# Attrition Pipeline

`attrition/` is the benchmark-shaped native stress and closure pipeline for Kain runtime attrition work. It is separate from `benchmark/`: benchmark asks "how fast is this row?", attrition asks "does this runtime surface stay structurally clean under compressed abuse, deterministic replay, sabotage, teardown audits, and LLVM-emitted real-program pressure?"

## Layout

- `attritions.json`: manifest-driven lane catalog, scales, sabotage modes, and profile defaults.
- `invariants.json`: explicit invariant catalog with subsystem ownership, formulas, floors, sabotage mapping, isolate lanes, and mixed-lane membership.
- `run.py`: runner that compiles native C and real Kain LLVM attrition lanes, executes them, captures machine-readable artifacts, emits replay commands, and shrinks deterministic failures by binary-searching the op count while preserving the same failure family when possible.
- `cases/`: native C attrition lanes, copied Kain LLVM `.kn` lanes, and a shared harness.
- `schema/`: versioned JSON report contracts.
- `out/build/`: case binaries and last raw case-result artifacts.
- `out/reports/`: timestamped suite JSON and LLM-readable markdown reports.
- `latest.md`: compact root snapshot for the last suite run.

## Current Lanes

- `saturated_rc_hot_object`
- `virtual_time_async_timer`
- `actor_reply_port_recycle`
- `process_slot_recycle`
- `mixed_runtime_boss`
- `kain_actor_ask_roundtrip`
- `kain_stdlib_domains`
- `kain_quantumerlang_attrition`
- `kain_semantic_singularity_crucible_attrition`

The first five are the native C foundation lanes. The `kain_*` lanes are real LLVM-emitted Kain programs copied from benchmark/blade surfaces so attrition can pressure parser, lowering, runtime-contract, and native ABI seams too. The broader plan still includes additional isolate lanes such as timer-cancel race stress, string/map churn, entangle/patch/shatter cleanup churn, idle/active alternation, advisory async-chaos, and a longer wall-clock companion soak.

## Commands

- `python attrition/run.py`
- `python attrition/run.py --scale medium`
- `python attrition/run.py --case virtual_time_async_timer --profile virtual-time`
- `python attrition/run.py --case saturated_rc_hot_object --sabotage skip_final_release`
- `python attrition/run.py --case mixed_runtime_boss --profile mixed-poison-virtual-time`

## Report Contract

Every lane run emits:

- a compiled native executable under `out/build/<case>/<profile>/`
- a raw `last_result.json` case artifact beside the executable
- a suite JSON report under `out/reports/<timestamp>.json`
- a markdown report under `out/reports/<timestamp>.llm.md`
- a replay command embedded in the suite report
- a minimized failing op-count for deterministic failures unless `--no-minimize` is used

The current telemetry contract is intentionally much richer than a single leak bit:

- raw snapshots now carry schema `2` with RC/allocator, actor/scheduler, process/handle, async/timer, quarantine/fragmentation, checkpoint, and time-provenance counters
- case telemetry derives:
  - throughput
  - peak metrics
  - activity metrics
  - balance gaps
  - end-state resource closure
  - nonzero end-state field lists
  - raw-time cleanliness
  - event-ring tail/total/dropped counts
- suite telemetry derives:
  - aggregate throughput
  - failed-case count
  - cases with closure drift
  - max peak/end-state offenders by case

Important reading rule: `event_ring_kind_histogram` is tail-only over the copied ring window; pair it with `event_count_total` / `event_ring_dropped_count` before treating it as the whole lifetime trace.

## Design Rules

- Deterministic lanes are the current gate. Tier-3 advisory chaos is planned, not yet implemented.
- Internal runtime counters are the source of truth; RSS is secondary and not yet wired into the case artifacts.
- Every implemented sabotage mode is manifest-declared and should produce an expected-fail report rather than a silent false green.
- The actor occupancy floor intentionally preserves the reserved invalid-slot bit at `1`; that floor is catalogued in `invariants.json` instead of being hand-waved in case code.
