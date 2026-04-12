# Native UI And Packaging

Snapshot: April 12, 2026.

This page covers the packaging-oriented command families.
For the conceptual target split, start with
[guides/cli/targets-and-codegen.md](/home/ephemara/Dev/Kain/guides/cli/targets-and-codegen.md).
For the UE5 conceptual model, see
[guides/ue5/overview.md](/home/ephemara/Dev/Kain/guides/ue5/overview.md).

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
Use this when you want a full plugin package with `.uplugin`, `Build.cs`,
module inference, and validation.

`kain build <file.kn> -t ue5`

This is the single-file UE5 codegen path. It emits code and shader artifacts,
not a complete plugin package.
It is the lane that feeds the UE5 code generator directly.

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
The project layout and runtime dependency shape are described in the CLI help
and in the native UI packaging lane, not in the language core.

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
