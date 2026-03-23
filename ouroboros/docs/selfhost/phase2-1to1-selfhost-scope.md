# Phase 2 — 1:1 Self-Hosted Bootstrap Scope

## Purpose

This document turns the current self-host progress into a concrete plan for reaching **1:1 parity with the current Rust bootstrap**.

The goal is not merely “a self-hosted compiler exists.”

The goal is:

- **the self-hosted KAIN pipeline reproduces the current Rust bootstrap behavior**
- **the current `kain.exe` command surface is preserved**
- **the current crate graph and feature graph are represented explicitly**
- **the migration is data-driven rather than based on hardcoded assumptions**

## Executive Summary

The project has crossed the hard prerequisite for staged self-hosting.

What is already true:

- **phase-1 self-host passes**
- **`kain-core.kn` round-trips back to Rust**
- **`kain-import.kn` round-trips back to Rust**
- **the phase-1 report now represents a real pass/fail lane instead of bootstrap guesswork**

What this means:

- **the self-host corridor is real**
- **Rust -> KAIN -> Rust works for core compiler code**
- **the next problem is expansion and parity, not importer chaos**

What it does **not** mean yet:

- **we do not yet have a fully self-hosted `kain.exe`**
- **the current default CLI feature set is not yet reproduced in KAIN-owned form**
- **the backend/domain crates are not yet fully inside the self-host slice**

## What 1:1 Actually Means

For this project, “1:1” should mean four things at once.

### 1. Source parity

The current Rust implementation of the KAIN toolchain can be represented in KAIN with enough fidelity to round-trip and compile.

### 2. Executable parity

The self-hosted `kain.exe` exposes the same practical command surface as the current Rust-built `kain.exe`.

### 3. Feature parity

The current feature-gated surfaces remain functional:

- **default CLI surface**
- **`sys` backend surface**
- **`gpu` surface**
- **`web` surface**
- **UE5 surface**

### 4. Dependency parity

The current dependency behavior is preserved explicitly, including:

- **internal KAIN workspace crates**
- **external Rust crates**
- **vendored Unreal ecosystem crates**
- **backend-only dependencies**

This does **not** require zero external Rust dependencies on day one.
It requires equivalent behavior and explicit dependency policy.

## Current Workspace Scope

The current workspace members establish the real 1:1 target surface.

### Core/compiler crates

- **`kain-core`**
- **`kain-asm`**
- **`kain-import`**
- **`kain-omni`**
- **`kain-sys-codegen`**
- **`cli`**

### Backend/target crates

- **`gpu`**
- **`web`**

### UE5 family

- **`ue5`**
- **`ue5-shaders`**
- **`ue5-materials`**
- **`ue5-graphs`**
- **`ue5-editor`**
- **`ue5-gas`**
- **`ue5-config`**
- **`ue5-asset-utils`**
- **`ue5-blueprints`**

### Vendored Unreal asset stack

- **`unreal_asset`**
- **`unreal_asset_base`**
- **`unreal_asset_kismet`**
- **`unreal_asset_exports`**
- **`unreal_asset_properties`**
- **`unreal_asset_registry`**
- **`unreal_asset_proc_macro`**
- **`unreal_helpers`**

## Why `cli` Is the Real Center of Gravity

`kain.exe` is the `cli` binary.

That matters because `cli` already fans out into most of the toolchain.

### Direct internal dependencies of `cli`

- **`kain-core`**
- **`kain-asm`**
- **`kain-import`**
- **`kain-omni`**
- **`kain-sys-codegen`** (feature-gated)
- **`gpu`** (feature-gated)
- **`web`** (feature-gated)
- **UE5 family** (feature-gated)

### Current default features

`cli` currently defaults to:

- **`ue5`**
- **`gpu`**
- **`web`**
- **`sys`**

That means a true 1:1 self-hosted `kain.exe` is broader than a minimal compiler binary.

## External Dependency Families

The current Rust bootstrap also relies heavily on external Rust crates.

These should be treated as dependency families rather than a single blob.

### Compiler/front-end ecosystem

- **`logos`**
- **`chumsky`**
- **`ariadne`**
- **`thiserror`**
- **`indexmap`**
- **`petgraph`**
- **`winnow`**
- **`smallvec`**
- **`rayon`**
- **`tracing`**
- **`tracing-subscriber`**
- **`schemars`**

### Runtime / async / config / tooling

- **`tokio`**
- **`flume`**
- **`once_cell`**
- **`serde`**
- **`serde_json`**
- **`toml`**
- **`jsonschema`**
- **`clap`**
- **`tower-lsp`**
- **`notify`**
- **`ctrlc`**
- **`reqwest`**
- **`flate2`**
- **`tar`**
- **`heck`**
- **`minijinja`**
- **`chrono`**

### Backend dependencies

- **`inkwell`**
- **`walrus`**
- **`rspirv`**
- **`pyo3`**

### Unreal/vendored dependencies

- **`bitflags`**
- **`bitvec`**
- **`byteorder`**
- **`enum_dispatch`**
- **`lazy_static`**
- **`log`**
- **`num_enum`**
- **`ordered-float`**
- **`regex`**
- plus transitive compression/serialization/storage crates in the vendored Unreal asset stack

## Required Dependency Policy for 1:1

For 1:1 parity, every dependency should be classified into one of these categories.

### A. Self-host target

These are KAIN-owned workspace crates that should eventually live inside the self-hosted slice.

Examples:

- **`kain-core`**
- **`kain-import`**
- **`kain-asm`**
- **`kain-omni`**
- **`kain-sys-codegen`**
- **`cli`**

### B. Host-Rust preserved

These remain Rust dependencies for a while even while the compiler logic becomes KAIN-owned.

Best early examples:

- **`serde`**
- **`serde_json`**
- **`thiserror`**
- **`clap`**
- **`tokio`**
- **`reqwest`**
- **`toml`**
- **`tracing`**
- **`tracing-subscriber`**

### C. Backend-bound preserved

These remain external because they are tied to host-specific backend generation.

Examples:

- **`inkwell`**
- **`walrus`**
- **`rspirv`**
- **`pyo3`**

### D. Import candidates

These are external crates that may later be imported or replaced if strategically useful.

Examples:

- **`indexmap`**
- **`petgraph`**
- **`smallvec`**
- **`winnow`**
- **`schemars`**

### E. Ecosystem mountain

These belong to a separate compatibility effort and should be treated as their own subgraph.

Examples:

- **vendored Unreal asset crates**
- **deep UE5/editor asset serialization crates**

## Ordered Migration Waves

The correct approach is not to move the entire workspace at once.

The right approach is to expand in topological waves.

### Wave 0 — already proven

- **`kain-core`**
- **`kain-import`**

Deliverable:

- **phase-1 pass with round-trip artifacts**

### Wave 1 — compiler surface expansion

- **`kain-asm`**

Why this wave matters:

- widens the compiler-owned self-host slice without immediately dragging in the full CLI fanout

### Wave 2 — orchestration + executable boundary

- **`kain-omni`**
- **`cli`**

Why this wave matters:

- begins reproducing the actual command behavior of `kain.exe`
- validates orchestration and cross-subsystem glue

### Wave 3 — backend parity core

- **`kain-sys-codegen`**
- **`web`**
- **`gpu`**

Why this wave matters:

- moves from “compiler exists” to “compiler emits the same broad classes of artifacts”

### Wave 4 — UE5 runtime-facing surface

- **`ue5-shaders`**
- **`ue5`**
- **`ue5-config`**
- **`ue5-gas`**

Why this wave matters:

- covers runtime/plugin/shader generation before the deepest asset/editor complexity

### Wave 5 — UE5 asset/editor stack

- **`ue5-asset-utils`**
- **`ue5-materials`**
- **`ue5-blueprints`**
- **`ue5-graphs`**
- **`ue5-editor`**

Why this wave matters:

- brings the heavy domain-specific generation surfaces into the self-host path

### Wave 6 — full vendored parity

- **Unreal asset stack as required by the UE5 lanes**

Why this wave matters:

- this is the final “full parity” mountain, not the first step

## Validation Matrix

Each wave must be validated in the same disciplined way.

### Per-crate validation

For each self-host target crate:

- **Rust -> KAIN import**
- **emit `.kn` bundle**
- **KAIN -> Rust round-trip**
- **compile regenerated Rust**
- **run crate tests if present**

### Executable validation

For `cli` / `kain.exe` specifically:

- **build stage2 executable from the self-host slice**
- **verify key commands behave correctly**

Minimum command set:

- **`kain --version`**
- **`kain build`**
- **`kain selfhost phase1`**
- **import commands**
- **selected backend target commands**

### Artifact validation

For codegen-heavy crates:

- **validate emitted artifacts compile / load / parse**
- **compare semantic outputs, not just text**

### Feature-lane validation

Validate at least these lanes separately:

- **minimal compiler lane**
- **default CLI lane**
- **`sys` lane**
- **`gpu` lane**
- **`web` lane**
- **UE5 lane**

## Data That Should Be Formalized Next

The project should define a dedicated self-host profile manifest.

Suggested contents:

- **crate inclusion list**
- **feature inclusion list**
- **dependency policy per external crate**
- **validation requirements per lane**
- **round-trip requirements per crate**
- **promotion criteria from one wave to the next**

This should be driven from data rather than from hardcoded path rules spread through the CLI.

## Recommended Immediate Next Step

If the goal is 1:1 parity as fast as possible, the next target should be:

- **`kain-asm`**

After that:

- **`kain-omni`**
- **`cli`**
- **`kain-sys-codegen`**

That path gets the project to a meaningful self-hosted executable faster than jumping directly into the full UE5/editor/vendored stack.

## Bottom Line

The 1:1 target is now a concrete execution problem.

The key facts are:

- **phase-1 proved the self-host corridor**
- **`cli` is the real center of gravity for executable parity**
- **the current workspace is broad and feature-gated**
- **external crates must be handled by explicit policy, not wishful thinking**
- **the migration should happen in topological waves with lane-based validation**

The next milestone is not “bootstrap theory.”

The next milestone is:

- **expand the proven self-host slice through `kain-asm`, `kain-omni`, `cli`, and `kain-sys-codegen` while preserving the current Rust bootstrap behavior in a data-driven way**
