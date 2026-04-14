# Kain Python Folder Guide

This folder holds Python-side helpers, scripts, and bridge utilities that support Kain's runtime and tooling.

## What Lives Here

- Python scripts that assist import, packaging, or validation
- bridge helpers that connect Kain to Python-hosted tools
- `validate_dcc_parity_matrix.py` validates the flagship KSculpt/KPainter
  parity inventory under
  `apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json`

## Output Hygiene

- keep virtual environments and caches out of this directory
- use `generated/` or `.venv/` outside the repo for build outputs

## Contract Inputs

- Prefer loading canonical validation profiles from Rust-owned or repo-generated JSON rather than baking semantic rules directly into Python.
- `validation_rules.py` will read `KAIN_UE5_RULE_PROFILE` when present and fall back to local defaults only when no contract file is available.
