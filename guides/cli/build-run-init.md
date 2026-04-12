# Build, Run, And Init

Snapshot: April 12, 2026.

This page covers the everyday compiler-facing workflow.

## `init`

`kain init [path] [--name NAME]`

Creates a new project with:

- `KAIN.toml`
- `src/main.kn`
- a minimal `.gitignore`

Use `init` when you want the canonical project skeleton that the rest of the
guide tree assumes.

## `build`

`kain build [input]`

Useful flags:

- `-o/--output`
- `-t/--target`
- `--targets` for comma-delimited multi-target builds
- `--ue5` for the UE5 plugin packaging path
- `--rust` for Rust-oriented output materialization
- `--embed` to embed Kain source markers in generated C++

`build` has two distinct modes:

1. file mode, where `[input]` is a single `.kn` source file
2. manifest mode, where the command reads `KAIN.toml` from the project root

If `input` is omitted, the build uses `KAIN.toml` and emits the configured
targets or packages.

### `build --ue5`

`kain build --ue5`

This packages a UE5 plugin from the manifest and the UE5 config surface.
It is the plugin-oriented lane, not the same thing as single-file UE5 codegen.

### `build -t ue5`

`kain build <file.kn> -t ue5`

This is the single-file UE5 codegen path. It emits code and shader artifacts
rather than packaging a complete plugin project.

### `build native-ui`

`kain build native-ui <input.kn>`

Native UI options:

- `--root`
- `--app-name`
- `--window-title`
- `-o/--out`
- `--artifact-dir`
- `--bundle-only`
- `--release`
- `--runtime-crate`
- `--runtime-path`
- `--runtime-version`

That path materializes a desktop app project and can also compile the generated
app. It is bundle-driven and writes the project skeleton plus sidecar metadata
that the native launcher consumes.

## `run`

`kain run <input.kn>` executes the source in the interpreter/runtime lane.

The top-level `-r/--run` flag on the root parser is not the same thing as this
subcommand. One is compile-then-run behavior, the other is explicit interpret
mode.

## Legacy Root Invocation

The launcher also accepts positional input and can still behave like the older
single-file compiler when no subcommand is supplied.

## Manifest Fields To Expect

The build path reads a `KAIN.toml` that typically includes:

- `package`
- `build`
- `dependencies`
- optional `ue5`
- optional `rust`
- optional `rust_ffi`

## Practical Rule

Use `build` when you want artifacts, `run` when you want immediate execution,
and `init` when you want a canonical project skeleton. If the question is
"which target should I choose?", start with `cli/targets-and-codegen.md` and
`reference/target-matrix.md`.
