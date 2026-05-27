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

Capsule `.kn` inputs created by `kain amalgamate` also work here. The CLI
materializes them under `.kain/cache/amalgamate/<digest>/workspace` first, then
reuses the normal file or manifest build path. Single-file capsules behave like
file builds; blade/workspace capsules behave like manifest/project builds.

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

`kain run [input]` resolves an entry file, project root, package root, manifest, or workspace through
the `kain-run` crate and executes it through the right adapter.

Current adapters:

- `.kn`: Kain interpreter lane through `kain-driver`
- `.kn` with `--target llvm` or `[run] target = "llvm"`: hidden cached
  native LLVM compile, then execute
- `.c`: hidden cached Clang compile, then execute
- `Cargo.toml` or a Rust crate folder: `cargo run` with a run-cache target dir
- `KAIN.fabric.toml`: Fabric manifest run
- `.js` / `.mjs` / `.cjs`: Node
- `.ts`: Bun

Useful flags:

- `--target auto|kain|llvm|c|cargo|fabric|node|bun`
- `--json`
- `--trace`
- `--keep-artifacts`
- `--dry-run`
- trailing runtime args after `--`

`kain run dev [input]` and `kain watch [input]` run the same plan in watcher
mode and re-run when planned inputs change. Use `--dry-run` on either command
to print the resolved plan without entering the resident loop. Watch inputs now
include manifest defaults, `build.kn` / `platform.kn`, generated platform locks,
binding reports, generated modules, and transitive package-local C/FFI bridge inputs when
they are part of the resolved run graph.

`kain run plan [input]` prints the resolved plan without executing it. This is
the quickest way to debug target inference, project selection, manifest run
metadata, platform lock provenance, and inherited foreign requirements.

When a workspace declares platform packages through `build.kn`, `platform.kn`,
or `[[platform.packages]]`, `kain run` resolves the deterministic package lock
lane before executing. Plans and reports include the build graph source,
platform lock paths, generated package modules, binding reports, and status
(`planned` for `run plan` / dry-run, `locked` for real execution).

Capsule `.kn` inputs are auto-detected before target inference. The CLI
materializes the capsule set to `.kain/cache/amalgamate/<state-hash>/workspace`
and passes the extracted entry, blade root, or manifest root back into
`kain-run`. If sibling companion capsules with the same capsule-set are present
next to the primary capsule, they are merged into the same materialized
workspace automatically.

Run artifacts are intentionally isolated from build artifacts:

- `.kain/cache/run` stores cached executables and Cargo run target dirs
- `.kain/reports/run` stores JSON session reports plus JSONL event streams

## `amalgamate`

`kain amalgamate <path> -o <artifact>.kn`

Use this command when you want a portable single-file source capsule that still
preserves the full workspace tree instead of flattening it into one module.

Useful flags:

- `--name`
- `--version`
- `--author`
- `--note`
- `--tag`
- `--meta key=value`
- `--archive`
- `--contents source|snapshot|assets|artifacts|evidence`
- `--capsule-set <name>`
- `--header minimal|rich|off`
- `--preview-symbols <n>`
- `--compression zstd|none`
- `--api-index auto|off`
- `--module-index auto|off`

Related subcommands:

- `kain amalgamate inspect <artifact>.kn`
- `kain amalgamate unpack <artifact>.kn [-o <dir>]`

By default, `kain amalgamate` writes an editable source capsule: a comment-safe
`.kn` artifact with a generated header, a structured `//!kain-capsule`
metadata block, and one `//!kain-file` section per preserved file in the
source closure. The default `source` profile follows `build.kn`, blade, and
manifest authority instead of snapshotting the whole directory. Use
`--contents snapshot` when you intentionally want the raw folder-dump behavior.
Text files stay inline and searchable inside the capsule; binary files are
still base64-wrapped per file.

Companion layers are first-class:

- `--contents assets` for authored binary payloads such as images or fonts
- `--contents artifacts` for generated build outputs such as `.exe`, `.obj`,
  `.ll`, runtime contracts, or sidecar binaries
- `--contents evidence` for telemetry, benchmark, and attrition outputs

Use `--capsule-set <name>` to bind related capsules together. `kain inspect`
shows the contents profile and capsule-set, while `kain unpack`, `kain run`,
`kain build`, and `kain check` automatically discover sibling companions with
the same capsule-set and materialize them into one workspace.

Use `--archive` when you want the sealed transport form instead. Archive
capsules keep the same header and metadata block but store the workspace as one
compressed `//!kain-capsule-payload` blob. `--compression` only matters on this
archive path.

Editable capsules intentionally refresh their content-derived digest and file
inventory from the inline file blocks when read, so hand-editing the capsule
does not invalidate `kain inspect`, `kain run`, `kain build`, or `kain check`.
Archive capsules remain strict and immutable. `inspect` is the authoritative
operator view; the rich header is only a generated preview.

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
