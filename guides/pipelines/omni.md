# Omni Pipeline

Snapshot: April 12, 2026.

`omni` is the mixed-language orchestration lane for Kain. It stages foreign
sources into Kain-compatible inputs, resolves them through a manifest-driven
pipeline, and then writes target outputs from the resolved graph.

The CLI entrypoints are `kain omni init [path]` and `kain omni build
--manifest <path>`.

## Manifest Shape

The canonical manifest is `KAIN.omni.toml`.

The manifest model in `src/rust-import/kain-omni/lib.kn` is:

- `project`
  - `entry`
  - `build_dir`
- `imports`
- `targets`
- `import_resolution`
  - `search_roots`
  - `inline_kain_imports`

The default manifest points at `src/main.kn` and writes into `omni_out/`.

## Import Staging

Each import source records:

- `path`
- `language`
- `output`
- `flat`
- `recursive`
- include and exclude filters
- fail-fast behavior
- C include paths and defines when relevant
- assembly format selection when relevant

Omni stages imported inputs into a build-root directory before target
materialization. The staged import list is part of the build result, so the
pipeline can show exactly which foreign source became which generated `.kn`
file.

## Current Source Languages

The manifest currently understands these source lanes:

- `Kain`
- `Rust`
- `TypeScript`
- `C`
- `Asm`

That makes Omni a true mixed-language intake layer rather than a Kain-only
helper.

## Target Families

The target model includes:

- `Rust`
- `Js`
- `Ts`
- `Ks`
- `Cpp`
- `Hlsl`
- `Usf`
- `Spirv`
- `GpuArtifacts`
- `RustBundle`
- `Ue5`
- `Ue5Editor`

The important distinction is that Omni is not a compiler target itself. It is a
pipeline that can emit multiple target families from one manifest.

## Rust Bundles

Rust bundle targets can request a set of emitted artifacts:

- source
- shader host code
- shader reflection
- SPIR-V

That is how Omni bridges Kain source into Rust-hosted execution or packaging
workflows.

## TypeScript Gating

The current Omni implementation still treats TypeScript support as build-gated.
If the compiled lane does not include TypeScript import support, the manifest
can still parse, but the build path will reject the import when it tries to
resolve it.

That should be documented as a current implementation constraint, not as a
permanent language limitation.

## Build Flow

`omni build` resolves the manifest, stages imports, resolves the entry source,
and then writes every requested target output.

The build result reports:

- the manifest path
- the staged imports
- the resolved entry path
- the written outputs

That makes Omni a good fit when readers need to know how a mixed-language tree
became a concrete set of generated files.

## Source Files To Read Next

- `src/rust-import/kain-omni/lib.kn`
- `src/rust-import/cli/omni.kn`
- `smoketest/allinone/fixtures/omni/KAIN.omni.toml`
- `smoketest/allinone/README.md`

## Practical Rule

Use Omni when the question is “how do these foreign sources become staged Kain
and target outputs together?” Use the CLI importer pages when the question is
“how do I convert one source family at a time?”
