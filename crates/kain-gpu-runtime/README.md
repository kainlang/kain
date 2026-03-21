# kain-gpu-runtime

Runtime-facing Vulkan compute executor for KAIN GPU payloads.

This crate sits on the execution side of the GPU pipeline:

- it loads prepared shader bundles and compute residency data
- it builds Vulkan compute dispatches from the current interop model
- it bridges host-facing C FFI request/response structs
- it keeps runtime execution separate from compiler-owned compute-plan authoring

## Current shape

- `src/lib.rs` re-exports the public runtime surface
- `src/bindings.rs` defines the request/result and binding types
- `src/executor.rs` owns the Vulkan executor and FFI entry points

## Design guardrails

- Treat authored compute metadata as compiler truth, not runtime guesswork
- Keep shared-buffer handling aligned with `crates/kain-interop`
- Preserve narrow FFI boundaries so host callers can detect failures cleanly
- Prefer explicit payload contracts over implicit binding conventions

## Why this exists

Kain already has compiler-side GPU artifact generation. This crate is the runtime executor that consumes those artifacts and drives dispatch in a host-facing lane.
