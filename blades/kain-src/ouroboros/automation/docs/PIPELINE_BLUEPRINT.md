# Hourly Pipeline Blueprint

This is the operating blueprint for the hourly self-host loop.

It is intentionally turn-based so agents do not thrash between importer work, repair work, and documentation every run.

## Strategic Principle

The shortest path to a real self-hosted KAIN is not random edits across every compiler subsystem.

It is disciplined progress through five pressures:

1. importer fidelity
2. pipeline reproducibility
3. repair-rule quality
4. validation evidence
5. documentation continuity

That is why the loop is lane-based.

## Rotation

Current six-turn cycle:

1. `importer`
2. `importer`
3. `pipeline`
4. `repair`
5. `validation`
6. `docs`

Then repeat.

## Finish Line

This loop retires on executable reality, not vibes.

The retirement target is:

- a real self-hosted `kain.exe`
- a bootstrapped/self-hosted compiler path that can import and compile the intended Rust crate surface from `M:/Code/Kain/crates`
- a practical near-1:1 import corridor, including the hard codegen-heavy and UE5-facing surfaces where the language/runtime support allows it

If the loop reaches a stable phase-2 milestone before total retirement, the next required output is a phase-3 execution plan.

## Why Importer Gets Two Turns

The importer is upstream of almost every other self-host milestone:

- if Rust -> KAIN import is noisy, phase-1 reports are noisy
- if phase-1 is noisy, round-trip and stage-2 failures become mixed and harder to classify
- if the importer is cleaner, repair lanes and validation lanes become much more productive

This is also the right place to harden the Rust importer until it is battle-tested.

## Lane Definitions

### Importer

Target:

- `M:/Code/Kain/crates/kain-import/src/rust/*`
- related importer support files in `common/*`

Acceptable work:

- module discovery fixes
- strict self-host option changes
- diagnostic classification improvements
- tests and corpora
- tighter importer-facing docs

Avoid:

- speculative bootstrap surgery
- broad unrelated parser or backend rewrites

### Pipeline

Target:

- `M:/Code/Kain/crates/cli/src/selfhost.rs`
- `M:/Code/Kain/crates/cli/src/selfhost_report.rs`
- `M:/Code/OuroborosV2/docs/selfhost/pipeline_manifest.json`
- `M:/Code/OuroborosV2/tools/selfhost_pipeline/*`

Acceptable work:

- better artifact/report wiring
- cleaner phase sequencing
- stage-2 assembly improvements
- manifest/schema alignment

Avoid:

- hiding failures behind vague success states

### Repair

Target:

- `M:/Code/OuroborosV2/tools/selfhost_repair/*`
- `M:/Code/OuroborosV2/docs/selfhost/repairs/*`

Acceptable work:

- blocker-family repair rules
- taxonomy improvements
- rule promotion cleanup
- repair runner reliability

Avoid:

- brittle one-off patches unless they unblock a known family and are documented as such

### Validation

Target:

- phase outputs
- repaired workspace status
- executable command reliability
- report evidence quality

Acceptable work:

- run narrow validating commands
- sharpen status surfaces
- convert ambiguous failures into exact next actions

Avoid:

- huge refactors under the banner of validation

### Docs

Target:

- automation docs
- self-host logic maps
- backlog and handoff quality
- scattered-doc consolidation

Acceptable work:

- update maps, commands, and decisions
- refresh drifted docs
- record evidence for next turn

Avoid:

- aspiration-only prose that does not point at live files or commands

## Protected Bootstrap Policy

Bootstrap is still in use, so the automation loop must treat it as protected.

Protected paths:

- `M:/Code/Kain/bootstrap`
- `M:/Code/Kain/kn_library/utilities/bootstrap.kn`
- `M:/Code/Kain/kn_library/utilities/compile_bootstrap.kn`
- `M:/Code/Kain/kn_library/utilities/full_bootstrap.kn`
- `M:/Code/OuroborosV2/legacy`

Default policy:

- do not edit these during normal hourly turns
- only touch them when evidence shows a narrowly scoped bootstrap-safe blocker fix is required
- document exact reason, commands, and risk in the turn report

## Standard Turn Procedure

1. Read config, backlog, and logic map.
2. Determine the current turn.
3. Generate the turn brief.
4. Execute one lane-appropriate improvement.
5. Validate with lane commands or documented fallbacks.
6. Write a report with exact files touched and what the next agent should inspect.

## Promotion Criteria

Promotion should be evidence-based, not mood-based.

Examples:

- importer promotion
  - a previously failing Rust construct now imports cleanly and is covered by a test or corpus fixture
- pipeline promotion
  - phase output artifacts and reports now emit deterministically
- repair promotion
  - a recurring blocker family is reduced by a real rule and reflected in outputs
- validation promotion
  - a lane status moves from ambiguous to explicit pass/fail with artifact proof

## Deliverable Standard

Every hourly run must leave:

- one concrete improvement
- exact validation notes
- one report in `automation/reports`
- one unified changelog entry in `automation/CHANGELOG.md`
- a clear next handoff

If a run is blocked, the deliverable becomes a sharper blocker with exact files and commands, not a vague summary.
