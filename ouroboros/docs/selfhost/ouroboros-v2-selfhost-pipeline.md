# Ouroboros V2 Selfhost Pipeline

## Purpose

This document defines the working Ouroboros V2 selfhost pipeline as it exists today, the gates required to promote each stage, and the parallel work that can proceed without destabilizing the active bootstrap lane.

The goal is to keep the pipeline **data-driven** and **lane-based** rather than encoded as scattered assumptions in CLI logic.

## Current pipeline entry points

The active entry point is the CLI selfhost command:

- `kain selfhost phase1`
- `kain selfhost phase2`

Implementation center of gravity:

- `crates/cli/src/selfhost.rs`
- `crates/cli/src/selfhost_report.rs`
- `crates/kain-import/src/rust/selfhost.rs`

## Current pipeline data flow

### Phase 1 / Phase 2 high-level flow

1. Resolve repo root
2. Resolve inventory directory
3. Resolve output directory
4. Load selfhost inventories
5. Determine crate slice from `module_map.json`
6. Import Rust crate graph into KAIN using strict selfhost importer
7. Emit `.kn` bundles
8. Optionally round-trip KAIN back to Rust
9. Optionally assemble stage-2 Cargo workspace
10. Optionally build the stage-2 workspace
11. Emit machine-readable and markdown reports

## Existing pipeline inputs

These inputs already exist and should remain the authoritative source of selfhost policy:

- `macro_inventory.json`
- `module_map.json`
- `selfhost_allowlist.json`
- `trait_inventory.json`

These are currently loaded by `load_inventories(...)` in `crates/cli/src/selfhost.rs`.

## Existing pipeline artifacts

For each run, the pipeline can currently emit:

- per-crate `.kn` bundle outputs
- per-crate `.roundtrip.rs` outputs
- stage-2 Cargo workspace
- stage-2 build log
- JSON report
- markdown report

Typical artifact families:

- `out/selfhost/*.kn`
- `out/selfhost/*.roundtrip.rs`
- `out/selfhost/phase2/stage2_workspace/...`
- `out/selfhost/phase2/stage2_workspace/stage2_build.log`
- `out/selfhost/*_report.json`
- `out/selfhost/*_report.md`

## Current gates

### Gate A: import gate

A crate passes the import gate when:

- strict selfhost import succeeds
- no hard diagnostics remain
- no required direct-lower macros remain preserved

### Gate B: bundle emission gate

A crate passes the bundle gate when:

- `.kn` bundle emits successfully
- the bundle is parser-safe KAIN source

### Gate C: round-trip Rust emission gate

A crate passes the round-trip gate when:

- `frontend_to_typed_program(..., Rust)` succeeds
- `kain_sys_codegen::generate_rust(...)` succeeds
- `.roundtrip.rs` is emitted

### Gate D: stage-2 assembly gate

A slice passes stage-2 assembly when:

- workspace Cargo manifest is rewritten successfully
- per-crate manifests are rewritten successfully
- path dependencies are rewritten correctly
- generated `lib.rs` files are written into the stage-2 workspace

### Gate E: stage-2 build gate

A slice passes stage-2 build when:

- `cargo build -p cli --bin kain` succeeds inside the assembled workspace
- target artifact exists
- build log does not contain stage-2 blockers

## Current blocker classes

These are the categories the pipeline should keep tracking explicitly:

- importer rejection / unsupported Rust surface
- parser-unsafe emitted KAIN syntax
- invalid type lowering in Rust round-trip emission
- recursive storage/type cycles in generated Rust
- test/dev leakage into the selfhost lane
- symbol collisions caused by flattening
- stage-2 manifest/path rewriting errors

## Recommended pipeline shape

The pipeline should be treated as four lanes, each with its own promotion criteria.

### Lane 1: import integrity

Purpose:

- prove Rust -> KAIN is faithful enough for the current slice

Success criteria:

- no import rejection
- diagnostics categorized and trending downward

### Lane 2: parser-safe KAIN emission

Purpose:

- guarantee emitted `.kn` is accepted by the current parser

Success criteria:

- bundle parses again
- known parser-sensitive forms are documented
- selfhost emitter uses parser-safe spellings

### Lane 3: Rust round-trip validity

Purpose:

- guarantee `.kn` -> Rust round-trip is structurally valid Rust

Success criteria:

- round-trip Rust compiles crate-by-crate or as a slice
- blocker classes are localized to codegen, not mixed with importer noise

### Lane 4: executable parity

Purpose:

- get to a bootable selfhosted `kain`

Success criteria:

- stage-2 `kain` artifact builds
- selected commands behave correctly

## Promotion policy

Promotion from one wave to the next should require explicit evidence:

- current slice passes import gate
- parser-safe forms are documented for known sensitive constructs
- round-trip blocker categories are classified
- stage-2 workspace assembles deterministically
- stage-2 build is either passing or narrowed to a small known blocker set

## Non-destructive parallel workstreams

The following work can proceed safely in parallel with active lane fixes:

- selfhost profile schema and inventories
- pipeline architecture docs
- blocker taxonomy docs
- validation matrix/checklists
- crate wave planning
- native-KAIN library/backlog design
- parser-safe emission rules documentation
- golden corpus planning for selfhost samples

## What should not be mixed into the hot lane

Avoid coupling these into active stage-2 fixes until the bootstrap path is steadier:

- UE5/editor parity expansion
- deep runtime platform work
- broad parser redesign
- major importer redesign
- large-scale backend rewrites unrelated to current blocker classes

## Immediate next execution priorities

### Priority 1

Stabilize the current active slice:

- `kain-core`
- `kain-import`
- `cli`
- `kain-sys-codegen`

### Priority 2

Formalize the data that should drive the pipeline:

- slice composition
- lane definitions
- artifact expectations
- promotion gates
- blocker categories
- validation commands

### Priority 3

Prepare the next slice expansion:

- `kain-asm`
- `kain-omni`

## Bottom line

The selfhost pipeline is already real.

What it needs now is not speculative redesign. It needs:

- stable lane definitions
- explicit data-driven policy
- repeatable artifact expectations
- disciplined promotion gates
- a clean parallel backlog for native-KAIN work that does not interfere with stage-2 bootstrap
