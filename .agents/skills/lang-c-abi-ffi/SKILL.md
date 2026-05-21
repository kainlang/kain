---
name: lang-c-abi-ffi
description: Use when authoring Kain source that calls through the C ABI, including `use c::...`, blade-local `[c_ffi]` metadata, native bridge usage in `.kn` files, and application-level FFI design, without taking ownership of shared ABI modeling or runtime bridge internals.
---

# Lang C ABI FFI

## Overview

This skill owns the authored side of Kain's C ABI bridge. Use it when the request is about how Kain code should import or call native surfaces, how a blade should declare `[c_ffi]`, or how to shape an application-facing FFI boundary in `.kn`.

## Start Here

- Read the closest working lane first: `blades/vulkain`, `blades/pong`, `blades/kaintana`, `benchmark/cases/gpu_graphics_submit/main.kn`, or any blade already using `use c::...`.
- Confirm the blade-local `KAIN.toml` has the `[c_ffi]` metadata needed by the authored import.
- Keep the authored bridge thin unless the task explicitly needs a larger facade.

## Routing

- Stay here for `use c::...`, authored wrapper functions, blade-local FFI metadata, and Kain-side call shape.
- Switch to `bootstrap-core` when the task changes shared foreign ABI modeling, type mining, import resolution, pointer semantics, or compiler-owned FFI lowering.
- Switch to `runtime-core` when the native bridge implementation, runtime headers, ownership helpers, or host ABI glue is wrong.
- Co-trigger `runtime-gpu` and `package-vulkain` when the C ABI boundary is GPU-facing.
- Co-trigger `lang-gpu` when the FFI lane exists to support authored GPU work.

## Authoring Rules

- Keep Kain-facing FFI APIs idiomatic on the Kain side even when the native library is ugly.
- Do not hide core compiler/runtime ABI defects by bloating authored wrappers. Preserve the intended call shape and route the defect to the owning sibling skill.
- If the bridge exists only for a package surface, prefer the owning package skill as the home for the higher-level semantics.
