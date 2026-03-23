# Legacy Self-Host Audit — V1 / Project Ouroboros

## Purpose

This document captures the high-value findings from the legacy self-hosted KAIN pipeline located at `m:\Code\Other\Misc\kainselfhosting`.

The goal is **not** to revive that codebase wholesale. The goal is to identify which pieces are still structurally valuable for the new `OuroborosV2` self-host effort, why they matter, and what should be avoided because it is tightly coupled to older language/runtime assumptions.

## Executive Summary

The legacy self-host tree contains real architectural value.

The strongest reusable assets are:

- **Build orchestration patterns** from `build.ps1`
- **Compiler driver structure** from `src/korec.kn`
- **Module/import resolution architecture** from `src/resolver.kn` and `stdlib/import_resolver.kn`
- **Testing infrastructure direction** from `stdlib/test_runner.kn`
- **Forward-looking self-host product modules** such as `formatter.kn`, `lsp.kn`, `monomorphize.kn`, `comptime.kn`, and `packager.kn`

The weakest assets are direct implementation details tied to the older compiler/runtime model, especially:

- **NaN-boxing-specific runtime/codegen assumptions**
- **Regex-based LLVM IR repair steps**
- **Older generics/type-system workarounds**
- **Single-file combined compiler output as the default architecture**

The correct posture for V2 is:

- **Reuse architecture and workflow ideas**
- **Preserve legacy files for reference**
- **Rewrite implementation against current KAIN semantics**

## Why This Matters

Self-hosting is now strategically different than it was in the original pass.

At that earlier stage, self-hosting happened before the language had been deeply dogfooded with real systems. Today the language is much more mature, the backend surface is broader, and the self-host path is being approached in a more data-driven way.

This audit matters because it gives V2 three advantages:

- **Avoid repeating solved workflow problems**
- **Recover proven orchestration patterns without inheriting old architectural debt**
- **Turn the old project into a design quarry rather than a rewrite trap**

## High-Value Findings

## 1. `build.ps1` is the strongest artifact in the legacy tree

### Source

- `build.ps1`

### Why it matters

The script already models the right concerns for a real self-hosting workflow:

- timestamped build artifact folders
- stable `latest` symlink/junction handling
- separate build modes for bootstrap/native/self/test/runtime/combine
- runtime freshness checks
- logs and verification gating
- cleanup and build history retention
- fallback artifact handling when the compiler writes to an unexpected location

These are exactly the operational concerns a self-host build pipeline needs.

### What should be reused

Reuse the **build state machine** and **artifact model**, not necessarily the exact script implementation.

Patterns worth carrying forward:

- `build/artifacts/<timestamp>` output structure
- `build/logs` capture
- `latest` pointer to newest successful artifact set
- runtime rebuild on source freshness changes
- explicit bootstrap/self/test phases
- verification-aware compile gates
- build history retention and cleanup

### What should not be reused directly

- regex-based postprocessing of LLVM IR as a normal build step
- hardcoded compiler source lists as a long-term module/dependency strategy

## 2. `src/korec.kn` contains a reusable compiler driver shape

### Source

- `src/korec.kn`

### Why it matters

The file models a clean high-level compiler pipeline:

- parse CLI/config
- resolve modules
- build full program
- typecheck
- target-specific codegen
- write artifact
- emit stats

This shape is still valid for V2 even though the implementation details are legacy.

### What should be reused

The structural API shape:

- `CompilerConfig`
- `CompilerStats`
- `Compiler::compile()`
- phase-driven compilation flow

This is a good fit for both human-driven and LLM-driven compiler workflows.

### What should be rewritten

- legacy option/variant handling workarounds
- debug-heavy implementation details
- older target enum assumptions
- anything tied to legacy AST/type constraints

## 3. The module/import resolver is highly reusable

### Sources

- `src/resolver.kn`
- `stdlib/import_resolver.kn`

### Why it matters

A self-host compiler cannot scale without a real module graph.

The legacy resolver work already captures the right concerns:

- typed import path modeling
- module caching
- cycle detection
- dependency scanning
- topological ordering
- configurable search paths

### Best reusable ideas

- `ModulePath` as a typed enum (`Relative`, `Absolute`, `Package`)
- resolver cache keyed by normalized path
- loading set for circular dependency detection
- separate file resolution and import scanning phases
- dependency order materialization

### Recommendation

Port the architecture and rewrite the implementation to align with the current KAIN module model, current package layout, and current stdlib conventions.

## 4. The legacy stdlib contains strong self-host infrastructure ideas

### Sources worth preserving

- `stdlib/import_resolver.kn`
- `stdlib/test_runner.kn`
- `stdlib/formatter.kn`
- `stdlib/lsp.kn`
- `stdlib/packager.kn`
- `stdlib/monomorphize.kn`
- `stdlib/comptime.kn`
- `stdlib/runtime.kn`
- `stdlib/README.md`

### Why they matter

These modules show that the legacy self-host effort was already heading toward a full language toolchain rather than a toy bootstrap.

The most important value here is not just implementation. It is **scope definition** for what a real self-hosted KAIN ecosystem should contain:

- test discovery and execution
- formatting
- LSP/service tooling
- compile-time evaluation
- package/artifact workflows
- monomorphization strategy

### Most important module immediately

#### `stdlib/test_runner.kn`

This is especially valuable because it models:

- test discovery
- filterable runs
- timeout handling
- fail-fast behavior
- summaries and outcomes
- benchmark hooks

That makes it a strong design source for a KAIN-native self-host regression harness.

## 5. `src/all_in_one.kn` is useful as a bootstrap artifact, not as the end state

### Source

- `src/all_in_one.kn`

### Why it matters

A combined-file compiler build can be useful for:

- bootstrap snapshots
- debugging import/module problems
- creating a single frozen compiler artifact for validation

### Why it should not become the primary architecture

It is poor as a permanent structure because it fights:

- modularity
- incremental development
- dependency tracking
- maintainability
- LLM-first editing workflows

Use it as a bootstrap/debugging technique, not the final compiler structure.

## Major Cautions

## 1. NaN-boxing-specific codegen/runtime logic should be treated as legacy-only

### Source

- `src/codegen.kn`

### Why it is risky

This code is tightly coupled to an older runtime representation and value model.

Directly porting it would risk importing outdated assumptions into V2, especially around:

- runtime value encoding
- ABI assumptions
- constant lowering
- low-level type behavior

### Recommendation

Preserve for reference only. Do not directly transplant the NaN-boxing design into the V2 self-host pipeline unless a deliberate modern design decision revalidates it.

## 2. Older generic/type-system internals contain ideas, but not necessarily portable implementations

### Source

- `src/types.kn`

### Why it matters

There are real ideas here, such as scoped generic tracking. But the implementation is shaped by older compiler limitations and should not be assumed to fit the current language model.

### Recommendation

Mine concepts such as:

- generic scope stacks
- scoped type lookup patterns
- environment layering

But re-express them in the current compiler architecture.

## 3. Regex repair of LLVM IR is a warning sign, not a target architecture

### Source

- `build.ps1` `Fix-LLVMIR`

### Why it matters

It documents real historical problems, but it should not become normal pipeline behavior.

The right V2 direction is to fix lowering at the compiler layer rather than patch artifacts after emission.

## Reuse / Adapt / Avoid Matrix

## Reuse almost directly

- build artifact strategy from `build.ps1`
- build staging concepts from `build.ps1`
- module resolver architecture
- compiler driver phase layout
- test harness structure
- logs, verification gates, cleanup policy

## Reuse conceptually, rewrite implementation

- compiler config/stats shapes
- dependency graph handling
- package/import search path modeling
- formatter/LSP/packager architecture
- monomorphization design direction
- comptime structure
- generic scope modeling

## Avoid direct porting

- NaN-boxed runtime assumptions
- legacy option/variant representation hacks
- regex LLVM IR repair as normal workflow
- monolithic combined source as the primary architecture

## Suggested V2 Extraction Order

## Phase 1 — Preserve and document

Preserve the high-value artifacts in `OuroborosV2/legacy` and keep this audit in `docs/ouroV1research`.

## Phase 2 — Rebuild the core self-host infrastructure

Start with:

- build orchestration model
- module graph / import resolver
- compiler driver shell
- test harness

## Phase 3 — Pull forward advanced toolchain pieces

Then evaluate and re-architect:

- formatter
- LSP
- packager
- comptime
- monomorphization

## Recommended Carry-Forward Files

### Build / pipeline

- `build.ps1`
- `src/korec.kn`
- `src/resolver.kn`
- `src/all_in_one.kn`
- `src/codegen.kn`

### Legacy stdlib and tooling

- `stdlib/README.md`
- `stdlib/import_resolver.kn`
- `stdlib/test_runner.kn`
- `stdlib/formatter.kn`
- `stdlib/lsp.kn`
- `stdlib/packager.kn`
- `stdlib/monomorphize.kn`
- `stdlib/comptime.kn`
- `stdlib/runtime.kn`

## Practical Conclusion

The legacy self-host codebase is **not** the implementation baseline for V2.

It **is** a high-value reference archive.

The best path is to:

- preserve the strongest artifacts
- extract the workflow and compiler-service patterns
- rebuild against the current language/runtime/backend reality
- keep the old code available as design reference during the Rust-import-driven self-host transition

## Immediate Relevance To V2

This matters right now because the new self-host effort is trying to avoid the original trap: bootstrapping too early without enough real language experience.

Today the language has enough maturity that self-hosting is no longer a vanity milestone. It is becoming the correct move for:

- reducing Rust/cargo iteration pain
- eliminating toolchain domain hopping
- improving LLM-first development loops
- validating the language against its own compiler/tooling workload

That makes the V1 legacy codebase useful as a **research base**, not as a source of unquestioned truth.
