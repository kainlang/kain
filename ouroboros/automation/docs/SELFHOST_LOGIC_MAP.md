# Selfhost Logic Map

This document answers the practical question: where does the self-host logic actually live right now?

The short answer is that it is split across two repos:

- `M:/Code/OuroborosV2`
  - control plane, manifests, repair rules, probes, outputs, and self-host research docs
- `M:/Code/Kain`
  - live Rust implementation for CLI self-host commands, strict Rust self-host import, typed lane/report contracts, and codegen

If an agent treats only one repo as the source of truth, it will miss half the system.

## 1. Live Selfhost Entry Points in Kain

### `M:/Code/Kain/crates/cli/src/selfhost.rs`

This is the current center of gravity for the executable self-host pipeline.

Key responsibilities:

- defines the `selfhost` CLI subcommand surface
- runs `phase1` and `phase2`
- resolves repo root, inventory dir, and output dir
- loads inventories from `module_map.json`, `selfhost_allowlist.json`, `macro_inventory.json`, and `trait_inventory.json`
- calls `kain_import::import_rust_selfhost_dir_detailed(...)`
- emits `.kn` bundles
- optionally round-trips KAIN back to Rust
- assembles the stage-2 Cargo workspace
- optionally builds the stage-2 workspace
- emits JSON and markdown reports

Important function anchors:

- `run_phase(...)`
- `load_inventories(...)`
- `compile_kn_source_to_rust(...)`
- `assemble_stage2_workspace(...)`
- `build_stage2_workspace(...)`
- `find_repo_root(...)`

## 2. Phase Reports in Kain

### `M:/Code/Kain/crates/cli/src/selfhost_report.rs`

This file defines the structured report payloads and the markdown rendering path for self-host phases.

Owns:

- phase status types
- per-crate results
- macro findings
- dyn/trait summary
- stage-2 artifact/build status fields
- markdown report rendering

If the automation loop needs better machine-readable or human-readable handoff output, this is one of the first files to inspect.

## 3. Strict Rust Selfhost Importer

### `M:/Code/Kain/crates/kain-import/src/rust/selfhost.rs`

This is the live strict Rust importer path used by the self-host pipeline.

Owns:

- `RustSelfHostOptions`
- inventory loading from `selfhost_allowlist.json` and `module_map.json`
- crate graph discovery
- module filtering and test exclusion
- strict self-host import with diagnostics treated as hard failures unless allow-listed

Key structures:

- `RustSelfHostOptions`
- `RustCrateGraph`
- `RustSelfHostImportResult`
- `SelfHostAllowlist`
- `SelfHostModuleMap`

Important behavior:

- diagnostics are filtered through `is_allowed_diagnostic(...)`
- `module_map.json` can drive exact module discovery
- `include_tests` is off for self-host runs

This is the main lane for making the Rust importer battle-tested.

## 4. Broader Rust Importer Surface

### `M:/Code/Kain/crates/kain-import/src/rust/transformer.rs`
### `M:/Code/Kain/crates/kain-import/src/rust/types.rs`
### `M:/Code/Kain/crates/kain-import/src/rust/parser.rs`
### `M:/Code/Kain/crates/kain-import/src/common/*`

These files are not only self-host code, but they directly determine whether self-host import succeeds.

Most important shared dependencies:

- `common/identifier_registry.rs`
- `common/type_mapper.rs`
- `common/language_schema.rs`

### `M:/Code/Kain/crates/kain-import/README.md`
### `M:/Code/Kain/crates/kain-import/CRATE_REFERENCE.md`

These docs confirm current reality:

- C importer is the production path today
- Rust importer is active development
- reflexive import for self-hosting is a primary strategic purpose

## 5. Typed Selfhost Schema Crate

### `M:/Code/Kain/crates/kain-selfhost/src/*`

This crate is small but strategically important. It encodes the typed control-plane contracts that should eventually replace ad hoc JSON shapes.

Current modules:

- `lane.rs`
  - `SelfHostLane`, `SelfHostStep`, `StepKind`
- `artifacts.rs`
  - `ArtifactExpectation`, `ArtifactContract`, `FrontErrorRecord`
- `report.rs`
  - `SelfHostLaneSummary`, `StepExecutionSummary`
- `pathing.rs`
  - typed path bundle for repo/pipeline roots
- `preflight.rs`
  - structural preflight failure schema
- `rules.rs`
  - typed repair rule schema
- `taxonomy.rs`
  - typed blocker taxonomy schema

This crate is the right place to keep growing data-driven lane/rule/report contracts.

## 6. Rust Round-Trip and Backend Dependency

### `M:/Code/Kain/crates/kain-sys-codegen/src/lib.rs`

The self-host pipeline depends on this crate because `phase2` emits Rust via:

- `generate_rust(...)`

This crate is not the orchestration layer, but it is a critical dependency of:

- `.kn` -> Rust round-trip validity
- stage-2 workspace compilation

If generated Rust is malformed, the issue can be codegen even when importer output is fine.

## 7. OuroborosV2 Pipeline Control Plane

### `M:/Code/OuroborosV2/docs/selfhost/pipeline_manifest.json`

This is the current manifest-driven lane definition for the Ouroboros repair pipeline.

It already encodes:

- repo roots
- output roots
- repair runner paths
- script paths
- crate slices
- lanes:
  - `analyze`
  - `phase2-core`
  - `phase2-full`

### `M:/Code/OuroborosV2/tools/selfhost_pipeline/run_pipeline.py`
### `M:/Code/OuroborosV2/tools/selfhost_pipeline/README.md`

This is the executable manifest runner for the Ouroboros-side pipeline.

It turns lane data into:

- per-step execution
- summary JSON
- log capture
- artifact status
- blocker bucket summaries

## 8. Repair Engine

### `M:/Code/OuroborosV2/tools/selfhost_repair/repair_runner.py`
### `M:/Code/OuroborosV2/tools/selfhost_repair/repair_rules.py`
### `M:/Code/OuroborosV2/tools/selfhost_repair/reporting.py`

These files own the phase-2 repair path.

They consume:

- `docs/selfhost/repairs/repair_rules.json`
- `docs/selfhost/repairs/error_taxonomy.json`
- `docs/selfhost/bootstrap_exceptions.json`
- `docs/selfhost/rule_promotion_ledger.json`

This is where compile-safe repair transforms and blocker classification live today.

## 9. Supporting Scripts in OuroborosV2

### `M:/Code/OuroborosV2/scripts/selfhost_workspace_status.ps1`

Reads latest pipeline outputs and reports:

- core summary
- full summary
- repair report
- front errors
- stage-2 binary existence

### `M:/Code/OuroborosV2/scripts/selfhost_repair_loop.ps1`

Runs the repair engine and then either:

- core-only stage-2 check
- full workspace check

### `M:/Code/OuroborosV2/scripts/selfhost_stage2_core_check.ps1`

Fast narrower stage-2 compile check path.

### `M:/Code/OuroborosV2/scripts/extract_selfhost_inventory.py`

Generates the self-host inventories and allowlists that the live CLI importer path consumes.

This script is a major bridge between the two repos.

## 10. Inventories and Policy Docs

These files are the data-driven self-host policy surface:

- `M:/Code/OuroborosV2/docs/selfhost/inventories/module_map.json`
- `M:/Code/OuroborosV2/docs/selfhost/inventories/selfhost_allowlist.json`
- `M:/Code/OuroborosV2/docs/selfhost/inventories/macro_inventory.json`
- `M:/Code/OuroborosV2/docs/selfhost/inventories/trait_inventory.json`
- `M:/Code/OuroborosV2/docs/selfhost/metadata/selfhost-profile-v2.json`
- `M:/Code/OuroborosV2/docs/selfhost/repairs/bootstrap_feature_policy.json`
- `M:/Code/OuroborosV2/docs/selfhost/repairs/error_taxonomy.json`
- `M:/Code/OuroborosV2/docs/selfhost/repairs/probe_targets.json`
- `M:/Code/OuroborosV2/docs/selfhost/repairs/repair_rules.json`

These are not side docs. They actively drive the importer and repair lanes.

## 11. Probe and Artifact Surfaces

### Probes

- `M:/Code/OuroborosV2/probes/index.json`
- `M:/Code/OuroborosV2/probes/selfhost_core/*`
- `M:/Code/OuroborosV2/probes/selfhost_ui/*`
- `M:/Code/OuroborosV2/probes/selfhost_memory/*`
- `M:/Code/OuroborosV2/probes/selfhost_traits/*`
- `M:/Code/OuroborosV2/probes/selfhost_paths/*`
- `M:/Code/OuroborosV2/probes/selfhost_*_god.kn`

### Outputs

- `M:/Code/OuroborosV2/out/selfhost/*.kn`
- `M:/Code/OuroborosV2/out/selfhost/*.roundtrip.rs`
- `M:/Code/OuroborosV2/out/selfhost/phase2/*`
- `M:/Code/OuroborosV2/out/selfhost/phase2/stage2_workspace/*`

These give concrete evidence about current self-host output quality and stage-2 failures.

## 12. Bootstrap and Legacy Corridors

These are relevant but should be treated cautiously:

- `M:/Code/Kain/bootstrap/*`
- `M:/Code/Kain/kn_library/utilities/bootstrap.kn`
- `M:/Code/Kain/kn_library/utilities/compile_bootstrap.kn`
- `M:/Code/Kain/kn_library/utilities/full_bootstrap.kn`
- `M:/Code/OuroborosV2/legacy/build.ps1`
- `M:/Code/OuroborosV2/legacy/src/*`
- `M:/Code/OuroborosV2/legacy/stdlib/*`

Important nuance:

- these files are part of the bootstrap story
- they are not the preferred hot lane for routine self-host progress
- they should be protected unless a turn is explicitly about bootstrap-safe intervention

## 13. Scattered Selfhost Docs Worth Reading

Primary docs in `OuroborosV2`:

- `docs/selfhost/ouroboros-v2-selfhost-pipeline.md`
- `docs/selfhost/phase2-current-status.md`
- `docs/selfhost/phase2-1to1-selfhost-scope.md`
- `docs/selfhost/parser-safe-variant-forms.md`
- `docs/selfhost/native-kain-parallel-workstreams.md`
- `docs/selfhost/native-kain-software-and-engine-roadmap.md`
- `docs/ouroV1research/legacy-selfhost-audit.md`

Primary docs in `Kain`:

- `crates/kain-import/README.md`
- `crates/kain-import/CRATE_REFERENCE.md`
- `crates/cli/CRATE_REFERENCE.md`

## Bottom Line

If you need to improve self-hosting:

- change `Kain` when the issue is importer, CLI orchestration, typed lane contracts, or codegen
- change `OuroborosV2` when the issue is manifests, inventories, repair rules, probes, outputs, or loop orchestration

Do not confuse the control plane with the implementation plane.
