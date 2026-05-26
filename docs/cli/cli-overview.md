# CLI Overview

Kain ships a modern subcommand CLI plus the older launcher-style entrypoint.
The command surface is live and source-driven; do not rely on stale README
snippets when the binary help output disagrees.

## Launchers

- `kain` is the explicit compiler-oriented launcher.
- `kn` is the run-first launcher. It defaults to interpret mode and shows a
  quick-start menu when invoked without input.
- `kn` also accepts the same subcommands as `kain`; the difference is launch
  bias, not language semantics.

## Top-Level Commands

- `init`
- `lsp`
- `doctor`
- `add`
- `install`
- `publish`
- `check`
- `test`
- `selfhost`
- `omni`
- `fabric`
- `commands`
- `amalgamate`
- `build`
- `runtime`
- `run`
- `watch`
- `gpu-artifacts`
- `inject`
- `import`
- `import-asm`
- `import-c`
- `import-rust`
- `import-crate`
- `import-ts`
- `help`

## Command Registry

Use `kain commands list --bin kain|kn|blade` to inspect the command registry and
`kain commands export --bin kain|kn|blade` for JSON. Add `--runtime` to include
`[[commands]]` contributions discovered from the current blade workspace.
Use `kain commands packs` to inspect the top-level manifest packs and
`kain commands help --bin kain|kn|blade` to render the registry-backed dynamic
Clap view.

## Global Flags

The root command supports:

- source input as a positional file
- inline source via `-c/--code`
- `-o/--output`
- `-t/--target`
- `-r/--run`
- `-w/--watch`
- `--emit-ast`
- `--emit-typed`
- `-v/--verbose`
- `--plugin`
- `--plugins-dir`
- `--dry-run`
- `--strict`
- `--analyze`
- `-h/--help`
- `-V/--version`

## Command Selection Rule

If you want a one-file compile or run, use the root launcher. If you want a
packaging, import, orchestration, or repair workflow, use the corresponding
subcommand. Use `reference/command-matrix.md` when you need the exact flag
table and `reference/target-matrix.md` when you need the target aliases.

Use `kain run`, `kain run dev`, `kain run plan`, or `kain watch` for the
unified runtime loop. That path is owned by `crates/run` and supports Kain
source, native LLVM Kain source via `--target llvm` or `[run] target = "llvm"`,
C files, Cargo crates, Fabric manifests, Node, Bun, blades, and workspace
`[run]` metadata. It also folds `build.kn` / `platform.kn` platform package
requirements and transitive blade `[c_ffi]` bridge inputs into the run plan so
desktop/GPU blades can be launched from their package root without hand-staging
every native input.

Use `kain amalgamate` when you want to pack a file, blade, or workspace into a
portable single-file Kain capsule. `kain amalgamate inspect` and
`kain amalgamate unpack` are the operator-facing inspection and extraction
paths, while `kain run`, `kain build`, and `kain check` can auto-detect capsule
`.kn` inputs and materialize them transparently. The default capsule is an
editable source-first capsule; optional `assets`, `artifacts`, and `evidence`
companions can travel alongside it through a shared capsule-set. `--archive`
switches any of those profiles to the sealed compressed transport form.

Use `kain publish`, `kain install`, and `kain add` when you want the same
capsule format to act as the Kain package transport and install lane.
`publish` emits a source capsule from a package or project root, `install`
stages that capsule into the Kain-owned global package store under
`~/.kain/packages`, and `add` records the dependency into `KAIN.toml` plus
`KAIN.lock`. Package imports still use `use ...`; the package manager only
changes how those modules enter the workspace graph. See
`cli/package-capsules.md` for the install layout and resolver order.

Use `kain import crates [path]` when you want to import a Rust workspace-scale
crate tree. It auto-detects `crates/`, `rust/`, or `src/rust/` under the chosen
root and can either emit one bundled `.kn` or, with `--blades`, mirror the
crate/file layout into a blades-style `.kn` tree.

Use `kain runtime build` and `kain runtime validate` when you want to prove the
owned native C/C++ runtime bundle itself instead of a single authored program.
Those commands are the preferred operator entrypoints and forward to the
existing platform wrapper scripts under `runtime/`.

## Source Of Truth

The CLI surface is defined in:

- `crates/commands/commands/index.toml`
- `crates/commands/commands/*.toml`
- `crates/commands/src/kain.rs`
- `crates/commands/src/blade.rs`
- `crates/commands/src/dynamic_clap.rs`
- `crates/commands/src/registry.rs`
- `crates/commands/src/runtime.rs`
- `crates/run/src/lib.rs`
- `crates/cli/src/main.rs` for host dispatch and handler execution
- `crates/cli/src/run.rs` for CLI printing and exit-code handling around
  `kain-run`
- `crates/check/src/lib.rs`
- `crates/test/src/lib.rs`
- `crates/cli/src/packager/`
- `crates/cli/src/import_*.rs`
