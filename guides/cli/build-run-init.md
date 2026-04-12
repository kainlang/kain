# Build, Run, And Init

This page covers the everyday compiler-facing workflow.

## `init`

`kain init [path] [--name NAME]`

Creates a new project with:

- `KAIN.toml`
- `src/main.kn`
- a minimal `.gitignore`

## `build`

`kain build [input]`

Useful flags:

- `-o/--output`
- `-t/--target`
- `--targets` for comma-delimited multi-target builds
- `--ue5` for the UE5 plugin build path
- `--rust` for Rust-oriented output materialization
- `--embed` to embed Kain source markers in generated C++

If `input` is omitted, the build uses `KAIN.toml` and emits the configured
targets.

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
app.

## `run`

`kain run <input.kn>` executes the source in the interpreter/runtime lane.

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
and `init` when you want a canonical project skeleton.
