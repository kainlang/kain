---
name: test-attrition
description: Use when running, extending, debugging, validating, or reviewing Kain's attrition runtime-certification lane under `attrition/`, including `attrition/attritions.json`, `attrition/invariants.json`, `attrition/run.py`, sabotage modes, replay and minimization artifacts, telemetry interpretation, and LLVM-emitted Kain attrition cases. Use this to certify teardown, closure, and invariant health, not to implement the runtime subsystems themselves.
---

# Test Attrition

Use this skill for Kain's abuse-and-certification lane. It owns attrition manifests, invariants, sabotage proofs, replay artifacts, minimization, and the interpretation of runtime telemetry as a certification surface.

## Trigger Surface

- A task asks for attrition runs, deterministic abuse coverage, sabotage proofs, or teardown-closure certification.
- `attrition/run.py`, `attrition/attritions.json`, or `attrition/invariants.json` needs changes.
- A new certification lane, profile, invariant, or minimizer behavior needs to be added.
- A runtime leak, occupancy drift, or teardown failure needs a deterministic attrition repro rather than ad hoc manual testing.

## Ownership Boundary

- This skill owns the attrition runner, lane manifests, invariant catalog, sabotage declarations, replay commands, minimization, and certification reports.
- If attrition exposes a runtime bug, preserve the failing lane here and hand the implementation fix to `runtime-core` or the relevant runtime domain skill.
- If attrition exposes compiler, lowering, or semantic bugs in real LLVM-emitted Kain lanes, keep the certification case here and hand the fix to `bootstrap-*`.
- If a lane needs authored Kain source changes, co-trigger the relevant `lang-*` skill. This skill still owns the certification harness around that lane.
- Do not use this skill as a substitute for runtime implementation ownership.

## Source Of Truth

- `attrition/attritions.json`: lane ids, titles, determinism tiers, profiles, scales, expected-fail sabotages, manifest selection.
- `attrition/invariants.json`: explicit invariant catalog and ownership metadata.
- `attrition/run.py`: compile, execute, capture telemetry, emit replay commands, minimize deterministic failures, write reports.
- `runtime/native_attrition_runtime.toml`: default lean native runtime bundle for attrition.
- `attrition/latest.md` and `attrition/out/reports/latest.llm.md`: live certification truth.
- `attrition/out/build/<case>/<profile>/last_result.json`: raw case result and replay metadata.

## Working Rules

1. Keep every invariant data-driven in `attrition/invariants.json`; do not bury invariant policy in Python or case-local tribal knowledge.
2. Internal runtime counters are primary truth. RSS is secondary telemetry, not the gate.
3. Respect determinism tiers. Exact replay and exact checksum lanes should stay exact.
4. Every new invariant should have:
   - one isolate lane
   - one sabotage proof
   - one mixed-lane membership
5. Expected-fail sabotage must produce an explicitly recognized red lane, not a silent pass.
6. Real LLVM-emitted Kain attrition cases are for certification. If they expose parser or runtime defects, keep the lane here and move the fix to the owning implementation skill.

## Validation

```powershell
python -m py_compile attrition/run.py
python attrition/run.py --scale small --profile release-instrumented --timeout 300
python attrition/run.py --case virtual_time_async_timer --scale small --profile release-instrumented --sabotage skip_task_dispose --timeout 240
python attrition/run.py --case kain_semantic_singularity_crucible_attrition --scale small --profile release-instrumented --timeout 900
python attrition/run.py --case kain_std_reload_contract --scale small --profile release-instrumented --sabotage skip_final_commit --timeout 900
```

Inspect `attrition/latest.md` and `attrition/out/reports/latest.llm.md` before summarizing lane health. If a sabotage proof no longer trips the intended invariant, fix the certification lane first, then hand off any underlying runtime repair.
