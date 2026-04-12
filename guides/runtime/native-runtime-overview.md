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

The canonical entrypoint is `runtime/native_runtime.toml`, not the legacy
umbrella C file.

## What The Native Runtime Owns

The runtime is the ABI floor for:

- contract validation and runtime version checks
- service discovery and capability reporting
- reflection payload loading
- actor and async runtime primitives
- platform boundary reporting
- graphics, viewport, material, shader, and compute services
- UI bundle and component runtime
- asset ingestion and realtime bundle consumption
- compatibility and hot-reload policies

## Service Families

The runtime service registry is data-driven. It groups services into families
such as:

- base memory and diagnostics
- contract and reflection
- actor and async
- platform host/input/window
- graphics and rendering
- UI bundle/component
- asset and realtime
- host bridge
- compatibility

## Platform Reality

The active native lane is Windows-first. Linux and macOS are represented in the
service and platform model, but not all of the runtime surface is equally mature
across every host.

## Companion Lane

`runtime/parallel/` is the non-C companion lane. It is useful for parallel
runtime planning, but it is not the canonical definition of native runtime
truth.
