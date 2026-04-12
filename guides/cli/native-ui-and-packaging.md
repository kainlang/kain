# Native UI And Packaging

This page covers the packaging-oriented command families.

## `gpu-artifacts`

`kain gpu-artifacts <input.kn> [--output DIR]`

This emits the shader artifact bundle plus host-side wrappers and reflection
metadata when the GPU/sys lanes are enabled.

## `inject`

`kain inject <file.kn> [more files...] [--plugin-dir DIR] [--plugin NAME]`

Flags:

- `--force`
- `--dry-run`
- `--ue5`

This is the surgical UE plugin injection path. It adds Kain files without
overwriting unless you ask it to.

## UE5 Build Path

`kain build --ue5` builds the UE5 plugin path from `KAIN.toml`.

`kain build --rust` builds the Rust-oriented package lane.

`kain build --embed` keeps source markers in generated C++ for round-tripping
and debugging.

## Native UI Build Path

`kain build native-ui <input.kn>` materializes a native desktop app project.

It resolves:

- the root component
- the app and window names
- the runtime crate and dependency shape
- the output project directory
- the packaged artifact directory

It can build the executable or stop at bundle materialization.

## Artifact Rule

This layer produces real app or plugin artifacts, not just compiler text
outputs.
