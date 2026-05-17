# Attrition Pipeline

`attrition/` is the benchmark-shaped native stress and closure pipeline for Kain runtime attrition work. It is separate from `benchmark/`: benchmark asks "how fast is this row?", attrition asks "does this runtime surface stay structurally clean under compressed abuse, deterministic replay, sabotage, and teardown audits?"

## Layout

- `attritions.json`: manifest-driven lane catalog, scales, sabotage modes, and profile defaults.
- `invariants.json`: explicit invariant catalog with subsystem ownership, formulas, floors, sabotage mapping, isolate lanes, and mixed-lane membership.
- `run.py`: runner that compiles native attrition lanes, executes them, captures machine-readable artifacts, emits replay commands, and shrinks deterministic failures by binary-searching the op count.
- `cases/`: native C attrition lanes plus a shared harness.
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

These are the implemented deterministic foundation lanes. The broader plan still includes additional isolate lanes such as timer-cancel race stress, string/map churn, entangle/patch/shatter cleanup churn, idle/active alternation, advisory async-chaos, and a longer wall-clock companion soak.

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

## Design Rules

- Deterministic lanes are the current gate. Tier-3 advisory chaos is planned, not yet implemented.
- Internal runtime counters are the source of truth; RSS is secondary and not yet wired into the case artifacts.
- Every implemented sabotage mode is manifest-declared and should produce an expected-fail report rather than a silent false green.
- The actor occupancy floor intentionally preserves the reserved invalid-slot bit at `1`; that floor is catalogued in `invariants.json` instead of being hand-waved in case code.
