---
name: lang-ownership
description: Use when authoring Kain code around ownership-state features such as `collapse`, `observe`, `decay`, shatter-aware movement, and ownership-heavy memory flows in `.kn` files, without changing the ownership model implementation, proof packs, or runtime helpers underneath.
---

# Lang Ownership

## Overview

This skill owns authored ownership semantics. Use it when a task is about how Kain code should express ownership transitions or memory-sensitive flows, or when a blade/benchmark needs to dogfood ownership-state behavior from the language side.

## Start Here

- Read ownership-heavy examples first: `benchmark/cases/ownership_memory/main.kn`, `benchmark/cases/quantumerlang/main.kn`, `blades/kain-example/src/main.kn`, and the nearest `semantic_singularity*` case using ownership features.
- Prefer explicit ownership-state constructs over hand-rolled approximations.
- Keep the authored proof surface strong enough that engine regressions are obvious.

## Routing

- Stay here for `collapse`, `observe`, `decay`, ownership-oriented Kain data flow, and authored memory pressure examples.
- Switch to `bootstrap-ownership` when parser/type/lowering/semantic ownership behavior is changing.
- Switch to `runtime-core` when the native runtime helpers or heap-facing behavior under authored ownership code is wrong.
- Co-trigger `lang-semantics` when ownership is fused with `world`, `entangle`, `teleport`, or `patch`.
- Co-trigger `test-harness` or `test-attrition` when the ownership claim needs proof or runtime abuse evidence.

## Authoring Rules

- Make ownership examples prove real movement, observation, or decay behavior instead of acting like decorative syntax.
- If the authored lane exposes a compiler/runtime hole, preserve the intended ownership code and route the engine fix to `bootstrap-ownership` or `runtime-core`.
- Use benchmark or attrition pressure when performance or lifetime cleanliness is part of the claim.
