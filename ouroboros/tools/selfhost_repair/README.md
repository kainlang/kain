# Selfhost Repair Engine

Standalone, data-driven repair tooling for Ouroboros V2 selfhost phase2.

## Purpose

This tool exists to accelerate iteration on generated selfhost outputs without mutating the original phase2 artifacts.

It focuses on:

- build log classification
- recurring failure taxonomy
- rule-driven repair of copied outputs
- optional validation on repaired outputs
- machine-readable and markdown reporting
- large probe corpus generation

## Layout

- `repair_runner.py`
  - entrypoint for analyze / repair / generate-probes / run-all
- `repair_rules.py`
  - JSON loaders and rule/taxonomy helpers
- `reporting.py`
  - report writers
- `probes.py`
  - data-driven probe materialization

## External control files

- `M:\Code\OuroborosV2\docs\selfhost\repairs\error_taxonomy.json`
- `M:\Code\OuroborosV2\docs\selfhost\repairs\repair_rules.json`
- `M:\Code\OuroborosV2\docs\selfhost\repairs\probe_targets.json`
- `M:\Code\OuroborosV2\docs\selfhost\repairs\repair_workflow.md`

## Default input/output roots

- Input phase2 root:
  - `M:\Code\OuroborosV2\out\selfhost\phase2`
- Repaired root:
  - `M:\Code\OuroborosV2\out\selfhost\phase2_repaired`
- Probe root:
  - `M:\Code\OuroborosV2\probes`

## Commands

From this directory:

```powershell
python repair_runner.py analyze
python repair_runner.py repair --validation check
python repair_runner.py generate-probes
python repair_runner.py run-all --validation check
```

## Outputs

Primary report outputs:

- `M:\Code\OuroborosV2\out\selfhost\phase2_repaired\phase2_repair_report.json`
- `M:\Code\OuroborosV2\out\selfhost\phase2_repaired\phase2_repair_report.md`

Repaired copied artifacts:

- `M:\Code\OuroborosV2\out\selfhost\phase2_repaired\...`

Probe corpus:

- `M:\Code\OuroborosV2\probes\selfhost_core\...`
- `M:\Code\OuroborosV2\probes\selfhost_ui\...`
- `M:\Code\OuroborosV2\probes\selfhost_memory\...`
- `M:\Code\OuroborosV2\probes\selfhost_traits\...`
- `M:\Code\OuroborosV2\probes\selfhost_paths\...`

## Design notes

- Original phase2 outputs remain untouched.
- Repairs are applied only to copied outputs.
- Rules stay in JSON so they can be reviewed and evolved without changing engine code.
- Validation is optional because repaired-output success is an iteration signal, not the final source of truth.

## Recommended usage

Use `run-all --validation check` for the default fast loop:

1. classify current failures
2. copy phase2 outputs into repaired root
3. apply enabled rules
4. run `cargo check` in repaired stage2 workspace
5. generate before/after report
6. generate or refresh the probe corpus
