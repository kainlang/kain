---
name: lang-authoring
description: Use when authoring or reshaping Kain `.kn` source, choosing idiomatic Kain constructs, picking exemplar blades or benchmark cases, or turning a requested idea into real Kain code without changing compiler, build, or runtime internals.
---

# Lang Authoring

## Overview

Use this skill for writing in Kain. Stay on the authored side of the boundary: `.kn` structure, module shape, examples, stdlib usage, and semantic style. If the real blocker is parser, codegen, build plumbing, or native substrate behavior, switch to the matching `bootstrap-*`, `runtime-*`, or `tool-build-system` skill instead of hiding the bug inside a tame rewrite.

## Start Here

1. Read `ARCHITECTURE.md` and search `MEMORY.md` for the subsystem, blade, or error string.
2. Read the closest real examples before writing source:
   - `blades/kain-example/src/main.kn`
   - `benchmark/cases/semantic_singularity_crucible/main.kn`
   - `benchmark/cases/quantumerlang/main.kn`
   - `blades/stdlib-domains/src/main.kn`
3. Check `stdlib/STDLIB_MAP.llm.md` before inventing helpers or assuming a `std.*` symbol does not exist.

## Routing

- Use `lang-blades` for runnable blade workspaces, build scripts, and root executable proof loops.
- Use `lang-semantics` when the authored shape is mainly `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `pulse`, `teleport`, or `shatter`.
- Use `lang-actors`, `lang-ui`, `lang-gpu`, `lang-c-abi-ffi`, `lang-ownership`, or `lang-translation` when that topic is dominant.
- Use `bootstrap-core` if you must change parser, typechecker, formatter, import, selfhost, or compiler-owned semantic behavior.
- Use `runtime-core` or `runtime-stdlib` if the missing capability lives in native runtime or stdlib wrapper layers.

## Authoring Rules

- Write Kain like Kain. Do not transliterate Rust or C++ module ceremony into `.kn`.
- Prefer named modules, top-level constants, and real language constructs over `fn` and `let` soup.
- Keep authored behavior in Kain and isolate OS, driver, or ABI work behind stdlib, package, or C bridge boundaries.
- If a benchmark, blade, or proof is part of the claim, keep the validation surface alive instead of deleting the weird part that exposed the bug.
