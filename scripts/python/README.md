# Kain Python Folder Guide

This folder holds Python-side helpers, scripts, and bridge utilities that support Kain's runtime and tooling.

## What Lives Here

- Python scripts that assist import, packaging, or validation
- bridge helpers that connect Kain to Python-hosted tools
- `bazel_tray.py` watches the Bazel server state from the Windows tray and can shut it down from the context menu
- `validate_dcc_parity_matrix.py` validates the flagship KSculpt/KPainter
  parity inventory under
  `apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json`
- `release_readiness_gate.py` runs the data-driven repo release gate from
  `release/readiness_policy.json` and is the operator path for quick/full
  release-readiness checks across benchmark, attrition, import-shape, and
  runtime-conformance evidence

## Output Hygiene

- keep virtual environments and caches out of this directory
- use `generated/` or `.venv/` outside the repo for build outputs

## Contract Inputs

- Prefer loading canonical validation profiles from Rust-owned or repo-generated JSON rather than baking semantic rules directly into Python.
- `validation_rules.py` will read `KAIN_UE5_RULE_PROFILE` when present and fall back to local defaults only when no contract file is available.
- `release_readiness_gate.py` should keep its blocker matrix in
  `release/readiness_policy.json` instead of hardcoding release-case ids,
  import requirements, or conformance categories in Python.
