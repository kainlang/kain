# Build, Run, And Init

Snapshot: May 12, 2026.

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
- `--lane bootstrap|dev|release|dist|selfhost` for lane-scoped output and cache identity
- `--ue5` for the UE5 plugin packaging path
- `--rust` for Rust-oriented output materialization
- `--embed` to embed Kain source markers in generated C++

`build` has two distinct modes:

1. file mode, where `[input]` is a single `.kn` source file
2. manifest mode, where the command reads `KAIN.toml` from the project root

If `input` is omitted, the build uses `KAIN.toml` and emits the configured
targets or packages. Normal file, project, Rust-output, and native-ui builds
route through the `kain-build` planner/executor and write canonical artifacts
under `.kain/out/<host>/<lane>/<target>/<unit>/<task>/...`; explicit `-o`
copies are materialization conveniences, not the source of artifact identity.

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

`kain run [input]` resolves an entry file, blade, manifest, or workspace through
the `kain-run` crate and executes it through the right adapter.

Current adapters:

- `.kn` / `.god`: Kain interpreter lane through `kain-driver`
- `.c`: hidden cached Clang compile, then execute
- `Cargo.toml` or a Rust crate folder: `cargo run` with a run-cache target dir
- `KAIN.fabric.toml`: Fabric manifest run
- `.js` / `.mjs` / `.cjs`: Node
- `.ts`: Bun

Useful flags:

- `--target auto|kain|c|cargo|fabric|node|bun`
- `--json`
- `--trace`
- `--keep-artifacts`
- `--dry-run`
- trailing runtime args after `--`

`kain run dev [input]` and `kain watch [input]` run the same plan in watcher
mode and re-run when planned inputs change. Use `--dry-run` on either command
to print the resolved plan without entering the resident loop.

`kain run plan [input]` prints the resolved plan without executing it. This is
the quickest way to debug target inference, blade selection, and manifest run
metadata.

Run artifacts are intentionally isolated from build artifacts:

- `.kain/cache/run` stores cached executables and Cargo run target dirs
- `.kain/reports/run` stores JSON session reports plus JSONL event streams

The top-level `-r/--run` flag on the root parser is not the same thing as this
subcommand. One is compile-then-run behavior, the other is the explicit
multi-adapter run pipeline.

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
- optional `run`

The `[run]` section is consumed by `kain-run` and may include:

- `entry`
- `blade`
- `target`
- `args`
- `env`
- `cwd`
- `watch`

## Practical Rule

Use `build` when you want durable artifacts, `run` when you want immediate
execution, `watch` when you want a live local loop, and `init` when you want a
canonical project skeleton. If the question is "which target should I choose?",
start with `cli/targets-and-codegen.md` and `reference/target-matrix.md`.
