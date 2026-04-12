# Native UI And Packaging

Snapshot: April 12, 2026.

This page covers the packaging-oriented command families.

## `gpu-artifacts`

`kain gpu-artifacts <input.kn> [--output DIR]`

This emits the shader artifact bundle plus host-side wrappers and reflection
metadata when the GPU/sys lanes are enabled.

The output family can include:

- SPIR-V
- Rust host wrappers
- reflection JSON
- shader bundle JSON
- optional derived HLSL

## `inject`

`kain inject <file.kn> [more files...] [--plugin-dir DIR] [--plugin NAME]`

Flags:

- `--force`
- `--dry-run`
- `--ue5`

This is the surgical UE plugin injection path. It adds Kain files into an
existing plugin layout without rewriting the whole package unless you ask it to.

## UE5 Build Path

`kain build --ue5`

This packages a UE5 plugin from `KAIN.toml` and the UE5 config surface. It is
the plugin-orchestration lane, not the same thing as single-file UE5 codegen.

`kain build <file.kn> -t ue5`

This is the single-file UE5 codegen path. It emits code and shader artifacts,
not a complete plugin package.

`kain build --rust`

This runs the Rust-oriented package lane.

`kain build --embed`

This keeps source markers in generated C++ for round-tripping and debugging.

## Native UI Build Path

`kain build native-ui <input.kn>` materializes a native desktop app project.

It resolves:

- the root component
- the app and window names
- the runtime crate and dependency shape
- the output project directory
- the packaged artifact directory

It can stop at bundle materialization or continue into the generated executable
build, depending on the selected flags.

## Artifact Sidecars

Native UI and packaging lanes produce real app or plugin artifacts, not just
compiler text outputs. Common sidecars include:

- runtime contracts
- compatibility snapshots
- realtime app bundles
- shader bundle metadata
- reflection payloads
- bridge manifests
- app manifest files

## Practical Rule

If you are debugging what was emitted, do not start in `build` or `inject`
first. Start with the target page in `cli/targets-and-codegen.md`, then return
here for the actual packaging workflow details.
