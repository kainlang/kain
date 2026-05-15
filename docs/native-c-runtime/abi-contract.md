# ABI Contract

This page is the user-facing summary of the native C runtime contract layer.

## Canonical Flow

The runtime contract pipeline is:

1. `runtime/native_core_runtime.toml`
2. `runtime/native_runtime.toml` as the lean compatibility mirror for older discovery paths
3. `runtime/native/src/core/kain_runtime_contract.c`
4. `runtime/native/include/kain_runtime_contract.h`
5. `runtime/native/include/kain_runtime_services.h`
6. `runtime/native/include/kain_runtime_version.h`
7. `runtime/native/include/kain_runtime_win32.h`
8. `runtime/native/SERVICE_TABLE_MAPPING.md`

## What The Contract Layer Owns

- ABI and runtime version checks
- compatibility masks and service gating
- startup validation
- service availability reporting
- capability reporting
- bridge between manifest truth and header-defined ABI truth

## How Startup Uses The Contract Layer

At startup, the native lane uses the contract layer to:

1. load the manifest-driven runtime configuration
   The canonical production source is `runtime/native_core_runtime.toml`.
2. compare the manifest against the generated headers and runtime version
3. reject incompatible combinations before execution begins
4. select the service families and capability masks that are available on the
   current platform
5. hand the validated contract to the service-table layer and the runtime host

That makes the contract layer the gate between declarative configuration and
actual execution.

## How To Think About It

The contract layer is not the whole runtime. It is the gate that makes sure the
runtime and the emitted bundle agree before execution starts.

## Practical Rule

If the manifest, headers, and runtime implementation disagree, trust the
canonical core manifest and the implementation in `runtime/native/src/core/`.
