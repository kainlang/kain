# Ouroboros V2 Selfhost Pipeline

## Purpose

This document defines the working Ouroboros V2 selfhost pipeline as it exists today, the gates required to promote each stage, and the parallel work that can proceed without destabilizing the active bootstrap lane.

The goal is to keep the pipeline **data-driven** and **lane-based** rather than encoded as scattered assumptions in CLI logic.

## Current pipeline entry points

The selfhost control plane now has two explicit lanes under the same CLI entrypoint:

- `kain selfhost phase1`
- `kain selfhost phase2`
- `kain selfhost bootstrap`

Implementation center of gravity:

- `crates/cli/src/selfhost.rs`
- `crates/cli/src/selfhost_report.rs`
- `crates/kain-import/src/rust/selfhost.rs`
- `src/KAIN.toml`
- `runtime/native_runtime.toml`

## Current pipeline data flow

### Rust mirror lane high-level flow

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

### Owned bootstrap lane high-level flow

1. Resolve repo root
2. Load `src/KAIN.toml`
3. Resolve the ordered `src/core` source set
4. Assemble the temporary aggregate bootstrap source under `src/.selfhost/`
5. Compile the aggregate source to LLVM through the hand-written lane
6. Stage native sidecars such as runtime contract and realtime app metadata
7. Resolve and build or reuse the native runtime from `runtime/native_runtime.toml`
8. Link a native `kainc` against the real C runtime artifacts
9. Optionally re-run the produced native compiler for ouroboros verification
10. Emit machine-readable and markdown reports under `src/.selfhost/reports/`

## Existing pipeline inputs

These inputs already exist and should remain the authoritative source of selfhost policy for the Rust mirror lane:

- `macro_inventory.json`
- `module_map.json`
- `selfhost_allowlist.json`
- `trait_inventory.json`

These are currently loaded by `load_inventories(...)` in `crates/cli/src/selfhost.rs`.

The owned bootstrap lane has a separate manifest-driven contract:

- `src/KAIN.toml` is the canonical hand-written selfhost contract
- `runtime/native_runtime.toml` is the canonical native runtime contract
- `src/.selfhost/` is the canonical artifact and report root for the owned lane

## Existing pipeline artifacts

For each Rust mirror run, the pipeline can currently emit:

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

For each owned bootstrap run, the pipeline should emit:

- a manifest-resolved aggregate source under `src/.selfhost/phase0/combined/`
- LLVM IR under `src/.selfhost/phase0/out/`
- runtime contract, realtime app, compute residency, and shader sidecars beside the LLVM/native outputs
- a native `kainc` binary linked against the real C runtime under `src/.selfhost/phase0/out/`
- ouroboros recompile artifacts under `src/.selfhost/ouroboros/`
- JSON and markdown reports under `src/.selfhost/reports/`

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

### Gate F: owned manifest/runtime gate

The owned lane passes the manifest/runtime gate when:

- `src/KAIN.toml` resolves from repo root
- every ordered `src/core` source file exists
- `runtime/native_runtime.toml` resolves
- the command emits JSON and markdown reports even on failure

### Gate G: owned compiler gate

The owned lane passes the compiler gate when:

- the hand-written compiler path emits LLVM for the owned source set
- no Rust parser, typechecker, lowering, or codegen passes are on the main compile path
- expected LLVM/native sidecars are materialized

### Gate H: native self-build gate

The owned lane passes the native self-build gate when:

- the C runtime bundle is resolved from `runtime/native_runtime.toml`
- runtime objects and archives are built or reused successfully
- the produced native `kainc` executable exists and is runnable

### Gate I: ouroboros gate

The owned lane passes the ouroboros gate when:

- the produced native compiler recompiles the same manifest-driven source set
- the verification step emits a deterministic comparison result
- artifact drift is recorded explicitly rather than hidden behind a false green

## Current blocker classes

These are the categories the pipeline should keep tracking explicitly across both lanes:

- importer rejection / unsupported Rust surface
- parser-unsafe emitted KAIN syntax
- invalid type lowering in Rust round-trip emission
- recursive storage/type cycles in generated Rust
- test/dev leakage into the selfhost lane
- symbol collisions caused by flattening
- stage-2 manifest/path rewriting errors
- owned-manifest resolution or source-order drift
- native runtime manifest drift or missing runtime artifacts
- native link failure or missing sidecars after a nominally successful compile
- ouroboros artifact drift between bootstrap and native recompilation

## Recommended pipeline shape

The pipeline should be treated as five lanes, each with its own promotion criteria.

### Lane 1: rust mirror integrity

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

### Lane 4: owned bootstrap/native

Purpose:

- get the hand-written `src/core` lane to emit LLVM, stage native sidecars, and link against the C runtime without routing semantic ownership back through Rust

Success criteria:

- `src/KAIN.toml` is the canonical contract for the owned lane
- the aggregate bootstrap source and LLVM outputs are emitted under `src/.selfhost/`
- the native executable links against the real runtime bundle from `runtime/native_runtime.toml`

### Lane 5: ouroboros parity

Purpose:

- get to a bootable selfhosted `kainc` that recompiles itself deterministically

Success criteria:

- the native `kainc` artifact builds
- the native compiler recompiles the same manifest-driven source set
- parity/drift is recorded as explicit report output

## Promotion policy

Promotion from one wave to the next should require explicit evidence:

- current slice passes import gate
- parser-safe forms are documented for known sensitive constructs
- round-trip blocker categories are classified
- stage-2 workspace assembles deterministically
- stage-2 build is either passing or narrowed to a small known blocker set
- the owned manifest resolves deterministically from repo root
- the owned lane emits reports and sidecars even when it fails
- the native runtime is resolved from manifest truth rather than guessed link flags
- ouroboros verification records exact parity or explicit drift

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
- `src/KAIN.toml` schema widening for true multi-file module graphs
- runtime-contract / sidecar parity checks for the owned bootstrap lane

## What should not be mixed into the hot lane

Avoid coupling these into active bootstrap fixes until the hand-written lane is steadier:

- UE5/editor parity expansion
- deep runtime platform work
- broad parser redesign
- major importer redesign
- large-scale backend rewrites unrelated to current blocker classes
- shrinking or replacing the C runtime before the owned lane is green

## Immediate next execution priorities

### Priority 1

Stabilize the owned bootstrap contract:

- `src/KAIN.toml`
- `src/core`
- `runtime/native_runtime.toml`
- `kain selfhost bootstrap`

### Priority 2

Keep the Rust mirror lane as the oracle/reference slice:

- `kain-core`
- `kain-import`
- `cli`
- `kain-sys-codegen`

### Priority 3

Formalize the data that should drive both lanes:

- slice composition
- lane definitions
- artifact expectations
- promotion gates
- blocker categories
- validation commands
- owned-manifest/runtime artifact expectations

### Priority 4

Promote the first full ouroboros loop:

- native `kainc` artifact
- deterministic recompile comparison
- blocker taxonomy for parity drift

## Bottom line

The repo now has two real selfhost lanes:

- the Rust mirror/reference lane
- the hand-written bootstrap/native lane

The control-plane rule is:

- keep the Rust mirror lane as reference and oracle infrastructure
- promote the hand-written manifest-first lane as the real selfhost target
- keep the C runtime as the canonical native runtime substrate
- treat aggregate bootstrap source as a temporary compatibility bridge until the true multi-file frontend lands
