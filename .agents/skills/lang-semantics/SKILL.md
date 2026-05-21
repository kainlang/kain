---
name: lang-semantics
description: Use when authoring or refactoring Kain source around first-class language semantics such as `world`, `entangle`, `patch`, `law`, `orchestrate`, `converge`, `pulse`, `teleport`, `shatter`, or semantic pressure demos, while staying on the authored-language side rather than changing parser, AST, lowering, or runtime internals.
---

# Lang Semantics

## Overview

This skill owns writing Kain that leans into Kain-only semantics. Use it when the request is about how to express behavior in `.kn` files, how to structure a proof blade or benchmark lane, or how to combine semantic features idiomatically without changing compiler or runtime internals.

## Start Here

- Read the nearest authored proof surface first: `blades/kain-example/src/main.kn`, `benchmark/cases/semantic_singularity_crucible/main.kn`, `benchmark/cases/quantumerlang/main.kn`, `blades/pong/src/main.kn`, or the closest `semantic_singularity*` case.
- Prefer the strongest native Kain construct that matches the request. Do not flatten a semantic task into plain functions and local variables if `world`, `entangle`, `collapse`, `observe`, `patch`, or `actor` carries the meaning directly.
- When the ask is partly authored and partly infrastructural, keep the authored work here and route the engine-side work to the right sibling skill instead of mixing both scopes together.

## Routing

- Stay in `lang-semantics` when the work is primarily inside `.kn` files.
- Switch to `bootstrap-core` when the task changes parser behavior, AST generation, type rules, keyword wiring, interpreter semantics, or LLVM/native lowering hooks.
- Co-trigger `lang-actors` when the semantic design is actor-heavy.
- Co-trigger `lang-ownership` when `collapse`, `observe`, `decay`, or ownership-state semantics are central.
- Co-trigger `lang-gpu` when shader or compute lanes are authored in the same feature.

## Authoring Rules

- Prove semantics with a memorable blade, benchmark case, or smoketest rather than a tiny forgettable snippet.
- Keep the authored lane honest: if the code only works because the compiler/runtime is broken, surface the blocker and route that blocker to `bootstrap-core`, `bootstrap-actors`, `bootstrap-ownership`, `bootstrap-fs`, `bootstrap-gpu`, `runtime-core`, or `runtime-gpu` as appropriate.
- Use existing semantic pressure patterns as the style compass. `semantic_singularity*`, `quantumerlang`, `machine_stones_shatter_loop`, and `pong` are better references than tame CRUD-style examples.
- If the semantic change needs proof or performance evidence, graduate it into `test-harness`, `test-bench`, or `test-attrition` instead of leaving it as hand-wavy authoring guidance.
