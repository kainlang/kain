# Native Runtime Overview

The native runtime is the manifest-driven C runtime used by the LLVM/native
execution lane.

## Canonical Truth

Use these files first:

- `runtime/native/C_RUNTIME_CONTRACT_PIPELINE.md`
- `runtime/native/SERVICE_TABLE_MAPPING.md`
- `runtime/native/NATIVE_RUNTIME_ERROR_CODES.md`
- `runtime/native/include/`
- `runtime/README.md`

The canonical entrypoint is `runtime/native_core_runtime.toml`. The sibling
`runtime/native_runtime.toml` now exists only as a lean compatibility mirror
for older tooling and discovery paths.

## What The Native Runtime Owns

The runtime is the ABI floor for:

- contract validation and runtime version checks
- service discovery and capability reporting
- reflection payload loading
- actor and async runtime primitives
- raw platform app-host/input/window boundary reporting
- raw graphics kernel, backend target descriptors, shader registration, and compute services
- UI bundle and component runtime over a passive native UI ABI
- asset ingestion and realtime bundle consumption
- compatibility and hot-reload policies

## Service Families

The runtime service registry is data-driven. It groups services into families
such as:

- base memory and diagnostics
- contract and reflection
- actor, async, network, and process
- platform host/input/window seams
- raw graphics, scene, and backend-target seams
- UI bundle/component
- asset and realtime
- host bridge
- compatibility

## Startup Flow

The native runtime starts by stitching the manifest, contract layer, and
service table together:

1. load `runtime/native_core_runtime.toml`
   Older tooling may still discover `runtime/native_runtime.toml`, but that
   file should resolve to the same lean runtime surface.
2. validate the contract against the native headers and version metadata
3. resolve the service families that the current lane needs
4. publish the reflection and capability payloads
5. start the host/runtime boundary for the selected platform

That flow is what turns the native runtime from a pile of headers into a
repeatable startup contract.

## Platform Reality

The active native lane is Windows-first. Linux and macOS are represented in the
service and platform model, but not all of the runtime surface is equally mature
across every host.

## Companion Lane

`runtime/parallel/` is the non-C companion lane. It is useful for parallel
runtime planning, but it is not the canonical definition of native runtime
truth.
