# Kain Parallel Runtime Lane

This folder is the non-C companion lane for the native runtime completion work.

It exists so we can build Rust and Zig runtime-side systems in parallel with the active C runtime spec work without touching `runtime/native` until that spec run is finished.

## Goals

- keep runtime pairing work data-driven
- consume existing native runtime metadata instead of duplicating it
- define Rust/Zig-side runtime components that will pair with the canonical ABI and service model
- make planned runtime modules visible before they are implemented in the C lane

## Layout

- `config/runtime_pairing_manifest.json`
  Shared source of truth for parallel runtime components, their target phases, service dependencies, and owned outputs.
- `config/toolchains.json`
  Toolchain and report-output configuration for the Rust and Zig lanes.
- `rust/kain-runtime-parallel`
  Rust CLI for validating and summarizing the parallel runtime plan against current native metadata.
- `zig`
  Zig-side scaffold that consumes the same manifest for host/runtime pairing work.
- `scripts/run_parallel_runtime.ps1`
  Orchestrates Rust and Zig report generation into one combined pipeline report.

## Current Rule

This lane must not modify `runtime/native` C sources while the long-haul spec executor is still running.

## Validation

Rust:

```powershell
cargo run -p kain-runtime-parallel -- summary
cargo run -p kain-runtime-parallel -- check
cargo run -p kain-runtime-parallel -- report
```

Zig:

```powershell
zig build --summary all
zig build run -- summary
zig build run -- check
zig build run -- json
```

Pipeline:

```powershell
.\runtime\parallel\scripts\run_parallel_runtime.ps1
```

This pipeline is intentionally config-driven so Rust and Zig stay aligned while the C runtime spec run continues independently.
