# ABI Contract

This page is the user-facing summary of the native C runtime contract layer.

## Canonical Flow

The runtime contract pipeline is:

1. `runtime/native_runtime.toml`
2. `runtime/native/src/core/kain_runtime_contract.c`
3. `runtime/native/include/kain_runtime_contract.h`
4. `runtime/native/include/kain_runtime_services.h`
5. `runtime/native/include/kain_runtime_version.h`
6. `runtime/native/include/kain_runtime_win32.h`
7. `runtime/native/SERVICE_TABLE_MAPPING.md`

## What The Contract Layer Owns

- ABI and runtime version checks
- compatibility masks and service gating
- startup validation
- service availability reporting
- capability reporting
- bridge between manifest truth and header-defined ABI truth

## How To Think About It

The contract layer is not the whole runtime. It is the gate that makes sure the
runtime and the emitted bundle agree before execution starts.

## Practical Rule

If the manifest, headers, and runtime implementation disagree, trust the
manifest and the implementation in `runtime/native/src/core/`.
