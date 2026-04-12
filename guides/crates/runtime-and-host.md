# Runtime And Host Crates

These crates turn Kain into an embeddable runtime and cross-language host stack.

## Core Host Stack

- `kain-host` embeds Kain in Rust hosts
- `kain-host-derive` supports host-side derive tooling
- `kain-reflect` carries reflection and type-identity support
- `kain-sdk` is the high-level embedding facade
- `kain-interop` defines shared payload contracts

## Foreign Runtime Bridges

- `kain-c-ffi`
- `kain-crate-ffi`
- `kain-python`
- `kain-node`

These crates are how Kain reaches into C, Rust crates, Python, and Node
ecosystems without collapsing authored logic back into host code.

## Runtime Execution Lanes

- `kain-sys-codegen` lowers to native/system targets
- `kain-gpu-runtime` is the Vulkan-side GPU runtime executor
- `kain-fast3d-runtime` supports the fast 3D runtime lane

## Rule

Use the host crates when you need to embed or bridge Kain. Use `kain-core`
when you need to know what the language itself means.
