---
name: kain-release-readiness-gate
description: Use when adding, changing, running, or debugging Kain's repo-level release-readiness matrix in `release/readiness_policy.json`, `scripts/python/release_readiness_gate.py`, the targeted attrition probes such as `kain_float_array_literal_indexing`, or the benchmark/attrition/conformance evidence that decides whether the native LLVM recipe is honestly ready.
---

# Kain Release Readiness Gate

## Contract

- `release/readiness_policy.json` is the source of truth for release-blocking hooks, checks, import rules, and feature coverage.
- `scripts/python/release_readiness_gate.py` is the runner. Keep policy data in JSON instead of hardcoding release-case ids or conformance categories in Python.
- The gate exists to stop surprise classes, not to crown one megatest. Treat `semantic_singularity_crucible` as one lane in the matrix, never the whole matrix.
- If a new blocker class appears, add a durable lane for it:
  - benchmark row if the issue is fairness or performance-shape specific
  - attrition case if the issue is compile/runtime/teardown correctness
  - conformance hook if the issue belongs to the native runtime substrate
  - source import rule if the issue is root-stdlib visibility or import drift

## Core Commands

- Quick focused release gate:
  - `python scripts/python/release_readiness_gate.py --profile quick --run`
- Full release gate:
  - `python scripts/python/release_readiness_gate.py --profile full --run`
- Inspect available profiles:
  - `python scripts/python/release_readiness_gate.py --list-profiles`
- Fast script proof:
  - `python -m py_compile scripts/python/release_readiness_gate.py scripts/python/test_release_readiness_gate.py`
  - `python scripts/python/test_release_readiness_gate.py`

## What Quick Covers

- Honest Kain-only release benchmark subset from `benchmark/run.py`
- Targeted attrition subset from `attrition/run.py`
- Runtime conformance for:
  - `graphics_runtime`
  - `ui_runtime`
  - `input_runtime`
  - `net_runtime`
  - `process_runtime`
- Root-stdlib import-shape rules in benchmark and attrition Kain source
- Feature coverage entries such as:
  - float-array literal indexing
  - graphics stdlib surface
  - filesystem/process/net/input/ui stdlib surfaces
  - actor roundtrip
  - semantic crucible

## What Full Adds

- Runtime conformance for:
  - `actor_runtime`
  - `async_runtime`
  - `diagnostics`
  - `abi_parity`
  - `reflection`
  - `host_bridge`
  - `hot_reload`
  - `platform_parity`

## Editing Rules

- Prefer changing `release/readiness_policy.json` before touching Python when the workflow change is data-shaped.
- Keep evidence ids stable. Coverage entries depend on ids such as:
  - `benchmark.case.<case_id>`
  - `attrition.case.<case_id>`
  - import rule ids like `imports.rule.benchmark.gpu_graphics_submit.graphics`
  - hook ids like `conformance.graphics_runtime`
- If you add a new attrition or benchmark blocker lane, wire it into:
  - the manifest (`attrition/attritions.json` or `benchmark/benchmarks.json`)
  - the release policy hook/check set
  - the coverage matrix entry that explains why the lane exists

## Common Failure Reading

- `benchmark case '<id>' contains forbidden fragment 'not yet parity-safe'`
  - The row still carries a caveat and is not honest enough for release.
- `source import rule '<id>' is missing imports [...]`
  - A root stdlib benchmark/attrition file drifted away from explicit `use std::<domain>` imports.
- `attrition case '<id>' failed: live_rc_objects drifted from baseline`
  - The Kain runtime lane is not closing cleanly for that case profile.
- `feature '<id>' is missing passing evidence '<evidence_id>'`
  - The coverage ledger is telling you exactly which proof lane is still red.

## When Extending The Matrix

- Add the smallest blocker that isolates the new class.
- Name the feature after the actual missing ingredient, not the symptom.
- Keep quick focused on common pre-release truth and full on deeper substrate proof.
