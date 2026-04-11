# Selfhost Repair Workflow

## Purpose

This workflow describes how to use the Ouroboros V2 selfhost repair subsystem to accelerate phase2 iteration without mutating the original generated artifacts.

## Inputs

Default phase2 input root:

- `ouroboros/out/selfhost/phase2`

Default repair docs/control root:

- `ouroboros/docs/selfhost/repairs`

Default repaired output root:

- `ouroboros/out/selfhost/phase2_repaired`

## Control files

The repair engine is driven by these files:

- `error_taxonomy.json`
- `repair_rules.json`
- `probe_targets.json`
- `bootstrap_feature_policy.json`

## Typical workflow

### 1. Analyze the current phase2 lane

Run the repair runner in analysis mode to classify the current stage2 failures and produce a report.

Expected outputs:

- `phase2_repair_report.json`
- `phase2_repair_report.md`

## 2. Apply repair rules into repaired outputs

The repair engine copies the current phase2 outputs into a separate repaired root and applies enabled rules there.

The original phase2 outputs remain unchanged.

Typical repaired outputs include:

- copied `.kn` bundles
- copied `.roundtrip.rs`
- copied `.probe.rs`
- copied `stage2_workspace`
- patched repaired files
- repair diff summaries and hit counts

## 3. Re-run validation

Validation is optional and can be enabled after repair application.

Supported validation modes:

- per-bundle `kain build <bundle>.kn -t rust`
- repaired stage2 workspace `cargo check`
- repaired stage2 workspace `cargo build`

Use `cargo check` first unless a full artifact build is needed.

## 4. Compare before and after

The report should be used to compare:

- bucket counts before repair
- bucket counts after repair validation
- rule hit counts
- files most improved
- files still failing hardest
- unknown/unclassified failures

## 5. Extend rules deliberately

When adding new rules:

- prefer JSON rule additions over code edits
- scope rules narrowly at first
- add notes and confidence
- keep repairs non-destructive
- validate against repaired copies only

## Probe workflow

### 1. Generate or regenerate the probe corpus

Use the probe generator to materialize the large selfhost probe corpus under:

- `probes/selfhost_core`
- `probes/selfhost_ui`
- `probes/selfhost_memory`
- `probes/selfhost_traits`
- `probes/selfhost_paths`

### 2. Use probes for discovery and regression

The probes are intended to stress:

- span ownership and propagation
- placeholder lowering
- typed branch fallbacks
- path flattening and sanitization
- trait/impl fidelity
- UI/runtime helper surfaces
- memory lowering helpers

The probes do not all need to compile today.

Their purpose is to expose recurring repairable patterns.

## Recommended operating mode

For active phase2 work:

- keep the live selfhost lane focused on real emitter/parser/codegen fixes
- use the repair engine to accelerate classification and temporary repaired-output iteration
- treat repaired-output success as signal, not as a substitute for upstream correctness
- run the manifest lanes instead of ad hoc command chains:
  - `python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane analyze`
  - `python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core`
  - `python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane phase2-full`

## Extension points

The repair subsystem is designed to support future additions such as:

- confidence-weighted rule ordering
- function-level hotspot ranking
- suggested next manual fix output
- artifact diff snapshots
- probe clustering and minimization

## Bottom line

The repair engine is an iteration accelerator.

Its job is to:

- classify recurring generated-output failures
- apply explicit rule-driven repairs to copied artifacts
- shorten the loop from failure to insight
- build a reusable stress corpus for selfhost evolution
