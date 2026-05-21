---
name: lang-gpu
description: Use when authoring Kain GPU-facing code such as shaders, compute kernels, graphics submission logic, GPU-backed blades, or idiomatic `.kn` usage of `std::gpu` and graphics surfaces, while keeping backend, codegen, runtime, and package internals in their own skills.
---

# Lang GPU

## Overview

This skill owns authored GPU work in Kain. Use it when the task is to write or reshape `.kn` shader lanes, compute flows, graphics submission code, or GPU-heavy demos, and the work should stay on the language/application side.

## Start Here

- Read the nearest GPU proof surface first: `benchmark/cases/gpu_graphics_submit/main.kn`, `benchmark/cases/semantic_singularity_crucible/main.kn`, `blades/vulkain/src/vulkain.kn`, `blades/mesh-scene`, and GPU-facing parts of `blades/kain-example/src/main.kn`.
- Prefer authored shader/compute semantics in Kain where possible instead of offloading everything to opaque native helpers.
- Keep GPU demos attached to a real blade or benchmark lane so they are reusable proof surfaces.

## Routing

- Stay here for authored shader code, compute kernels, graphics session logic, and Kain-side GPU orchestration.
- Switch to `bootstrap-gpu` when the task changes SPIR-V/PTX/CUDA/backend lowering, codegen layout math, or compiler-owned GPU semantics.
- Switch to `runtime-gpu` when the native executor, driver-facing runtime path, submission substrate, or GPU host integration needs to change.
- Co-trigger `package-vulkain` when the authored work uses or expands the Vulkain package surface.
- Co-trigger `lang-c-abi-ffi` when the GPU lane crosses the C ABI boundary directly.

## Authoring Rules

- Author the strongest Kain-facing GPU surface first, then route missing engine capability to `bootstrap-gpu`, `runtime-gpu`, or `package-vulkain` instead of collapsing into generic host code.
- Keep backend-specific hacks out of the authored lane unless the user explicitly wants a target-shaped demo.
- If performance is the point, attach the authored GPU work to `test-bench` or a benchmark case so the claim stays measurable.
