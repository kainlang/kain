---
name: lang-stdlib
description: Use when authoring Kain code against the public `std.*` surface, choosing the right stdlib domain imports, composing stdlib capabilities idiomatically in `.kn` code, or shaping stdlib-facing examples and blades without changing the underlying runtime or stdlib implementation.
---

# Lang Stdlib

## Overview

This skill owns consuming the public Kain stdlib from authored code. Use it when the question is "how should Kain code use `std.*`?" rather than "how should the stdlib or native wrapper be implemented under the hood?"

## Start Here

- Check `stdlib/STDLIB_MAP.llm.md` for the canonical public surface.
- Read the closest authored example first: `blades/stdlib-domains/src/main.kn`, `blades/network-domains/src/main.kn`, `blades/math-domains/src/main.kn`, `blades/hash-domains/src/main.kn`, or `blades/kain-example/src/main.kn`.
- Prefer root `std.*` imports. Do not grow a shadow `std.native.*` authoring pattern.

## Routing

- Stay here for authored use of `std.actor`, `std.fs`, `std.net`, `std.http`, `std.process`, `std.graphics`, `std.ui`, `std.crypto`, and related public domains.
- Switch to `runtime-stdlib` when the underlying runtime-backed stdlib behavior or native wrapper implementation needs to change.
- Switch to `bootstrap-core` when import resolution, stdlib loading, parser/type behavior, or compiler-owned stdlib semantics are wrong.
- Co-trigger `lang-gpu`, `lang-ui`, `lang-actors`, `lang-c-abi-ffi`, or `lang-ownership` when those surfaces are central to the authored code.

## Authoring Rules

- Use the public surface the way users should actually see it. Avoid teaching unstable internal imports unless the task is explicitly about bootstrap ownership.
- When a public stdlib API feels awkward because the implementation is incomplete, keep the authored usage honest and route the missing capability to `runtime-stdlib` or `bootstrap-core`.
- If the task adds a new canonical authored stdlib pattern, strengthen a blade or benchmark example so future agents can copy the right shape.
