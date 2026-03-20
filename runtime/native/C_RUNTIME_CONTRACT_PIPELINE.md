# Kain Native C Runtime Contract Pipeline

## Purpose

This document explains how the native C runtime contract is organized today and how the contract source, headers, and service mapping fit together.

The goal is to keep the contract path data-driven and easy to validate as the runtime surface grows.

## Canonical Flow

1. `runtime/native_runtime.toml` defines the active native runtime build surface.
2. `runtime/native/src/core/kain_runtime_contract.c` implements the native contract logic.
3. `runtime/native/include/kain_runtime_contract.h` exposes the public contract ABI and compatibility masks.
4. `runtime/native/include/kain_runtime_services.h` defines the service registry surface used by the newer table-driven model.
5. `runtime/native/include/kain_runtime_version.h` and `runtime/native/include/kain_runtime_win32.h` provide versioning and platform support for the contract layer.
6. `runtime/native/SERVICE_TABLE_MAPPING.md` tracks how legacy service keys map to the canonical service table.

## What This Layer Owns

The C runtime contract layer is responsible for:

- ABI and runtime version checks
- legacy service mask compatibility
- contract startup validation
- service availability and capability reporting
- bridging the compiled runtime surface to the header-defined ABI

## Current Truth Model

- `kain_runtime_contract.c` is the source of implementation truth for the C runtime contract path.
- `kain_runtime_contract.h` remains the compatibility-facing contract header.
- `kain_runtime_services.h` is the direction of travel for the data-driven service registry.
- `kain_runtime.c` is legacy and should not be treated as the active runtime definition.

## Maintenance Rules

- Update the manifest first if the compiled runtime surface changes.
- Prefer adding or widening service-table data over hardcoding new runtime behavior.
- Keep compatibility masks only where they are needed for transition support.
- Add or update a conformance check whenever the contract behavior changes.
- If documentation and code disagree, trust the manifest, the implementation in `runtime/native/src/core/`, and the service mapping docs.

## Related Files

- [runtime/native/SERVICE_TABLE_MAPPING.md](./SERVICE_TABLE_MAPPING.md)
- [runtime/native/include/HELPER_ABI_SUMMARY.md](./include/HELPER_ABI_SUMMARY.md)
- [runtime/README.md](../README.md)
