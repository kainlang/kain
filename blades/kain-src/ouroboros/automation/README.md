# Ouroboros V2 Selfhost Automation

This folder is the repo-local control plane for the hourly self-host automation loop.

The important architectural reality is:

- `M:/Code/OuroborosV2` is the self-host research, manifest, repair-rule, and artifact-control repo.
- `M:/Code/Kain` contains the live Rust implementation for the self-host CLI, Rust importer, codegen, and self-host schema crate.

This automation layer exists to stop agents from rediscovering that split every hour.

## Goals

- Drive KAIN to a real self-hosted stage-2 compiler.
- Harden the Rust importer until it is reliable enough for repeated reflexive import runs.
- Preserve the currently used bootstrap corridor instead of destabilizing it.
- Keep phase-1, phase-2, repair, and validation work evidence-driven.
- Leave a clean handoff every turn.

## Non-Negotiable Rule

Do not break the currently used bootstrap path while chasing self-host progress.

Protected surfaces:

- `M:/Code/Kain/bootstrap`
- `M:/Code/Kain/kn_library/utilities/bootstrap.kn`
- `M:/Code/Kain/kn_library/utilities/compile_bootstrap.kn`
- `M:/Code/Kain/kn_library/utilities/full_bootstrap.kn`
- `M:/Code/OuroborosV2/legacy`

These are reference or active bootstrap safety corridors. Touch them only when a turn explicitly targets bootstrap-safe fixes and the report records exact evidence.

## Retirement Criteria

This automation pipeline is not done when phase-2 merely looks better.

It retires when all of the following are true:

- the self-host corridor produces a real `kain.exe`
- the bootstrapped/self-hosted compiler is the practical source of truth for continued KAIN work
- the pipeline can import the full intended Rust surface from `M:/Code/Kain/crates` into KAIN as close to 1:1 as the language permits
- that import-and-compile path includes the hard slices, including `kain-import`, `kain-sys-codegen`, `cli`, and the UE5/codegen-facing crate surfaces
- the generated self-host outputs are good enough that future work can move from "can it self-host?" to "can we build major software like a 3D engine on it?"

If phase 2 reaches a meaningful finish line early, the next required deliverable is a concrete phase-3 plan rather than idle victory prose.

## Rotation

The hourly loop is data-driven by `automation/config/pipeline.config.json`.

Current cycle:

1. `importer`
2. `importer`
3. `pipeline`
4. `repair`
5. `validation`
6. `docs`

Why this order:

- The Rust importer is still the highest leverage bottleneck.
- Pipeline and repair work only matter if importer output is getting cleaner.
- Validation gets a dedicated turn instead of being an afterthought.
- Docs happen once per cycle so the next agents compound instead of re-auditing.

## Files

- `config/pipeline.config.json`
  - Single source of truth for cadence, lanes, references, protected areas, and validation commands.
- `docs/SELFHOST_LOGIC_MAP.md`
  - Inventory of where self-host logic actually lives.
- `docs/PIPELINE_BLUEPRINT.md`
  - Turn-based operating model for the hourly loop.
- `BACKLOG.md`
  - Ranked work queue for future turns.
- `prompts/hourly-loop.prompt.md`
  - Paste-ready automation prompt.
- `scripts/next-turn.mjs`
  - Computes the active lane and brief.
- `scripts/write-report.mjs`
  - Creates the next report stub.
- `scripts/update-changelog.mjs`
  - Creates a seeded unified changelog entry for a turn.
- `templates/session-report.md`
  - Standard report format.
- `CHANGELOG.md`
  - Unified cross-turn changelog for the whole automation loop.

## Usage

Generate the next turn brief:

```bash
node automation/scripts/next-turn.mjs
```

Generate a specific turn:

```bash
node automation/scripts/next-turn.mjs --turn 4
```

Emit JSON:

```bash
node automation/scripts/next-turn.mjs --turn 4 --json
```

Create the next report stub:

```bash
node automation/scripts/write-report.mjs --lane importer
```

Create a specific report:

```bash
node automation/scripts/write-report.mjs --turn 4 --lane repair
```

Seed the unified changelog entry for a turn:

```bash
node automation/scripts/update-changelog.mjs --turn 4 --lane repair --summary "Reduced a real phase-2 blocker family"
```

## Operating Model

Every turn should:

1. Read `automation/config/pipeline.config.json`, `automation/README.md`, `automation/BACKLOG.md`, and `automation/docs/SELFHOST_LOGIC_MAP.md`.
2. Generate the active brief with `node automation/scripts/next-turn.mjs`.
3. Execute one lane-appropriate improvement only.
4. Prefer live implementation work in `M:/Code/Kain` when the change concerns importer, CLI, stage-2 assembly, repair taxonomy, or codegen.
5. Use `M:/Code/OuroborosV2` for manifests, repair rules, artifact inspection, probes, and pipeline orchestration.
6. Validate with the commands in the lane config.
7. Write a handoff report into `automation/reports`.
8. Update `automation/CHANGELOG.md` so the repo has one accumulated history instead of only per-turn reports.

## Current Read

At the time this control plane was created:

- `OuroborosV2` already contains a manifest-driven phase-2 repair pipeline under `docs/selfhost`, `tools/selfhost_pipeline`, `tools/selfhost_repair`, `scripts`, `probes`, and `out/selfhost`.
- `Kain` already contains the real self-host entry points in `crates/cli/src/selfhost.rs`, `crates/cli/src/selfhost_report.rs`, `crates/kain-import/src/rust/selfhost.rs`, `crates/kain-selfhost/src/*`, and `crates/kain-sys-codegen/src/*`.
- The repo split is powerful, but it creates drift if agents treat one repo as the whole story.

This folder is meant to keep that drift under control.
