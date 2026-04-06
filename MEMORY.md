# MEMORY

## 2026-04-06 - Windows bootstrap now falls back to installed LLVM and Python 3.12

Fresh-clone Windows setup now has a more durable path when the repo-local LLVM drop is missing and the machine default Python is newer than the pinned PyO3 lane supports.

What changed:

- Updated `scripts/sync-kain-source-of-truth.ps1`
  - Added `Resolve-ClangPath` so the install/sync flow no longer assumes `toolchain\llvm\bin\clang.exe` exists in every clone.
  - The script now prefers an already-set `KAIN_CLANG_PATH`, then the repo-local toolchain path, then `clang` on PATH, then `C:\Program Files\LLVM\bin\clang.exe`.
  - Added `Resolve-Python312Path` so the sync flow can discover a compatible Python 3.12 interpreter for the current `pyo3 0.20.x` dependency line.
  - The script now seeds both session PATH and persisted user PATH with the installed `kain` binary directory plus the resolved LLVM/Python directories when available, and it persists `PYO3_PYTHON` alongside the existing KAIN resource roots.
- Updated `ARCHITECTURE.md`
  - Added durable `Common Errors` notes covering fresh clones without vendored LLVM binaries and the Python 3.14 versus PyO3 0.20 mismatch / `python312.dll` runtime requirement.

Setup notes validated on this machine:

- Installed LLVM via `winget` and resolved `clang.exe` at `C:\Program Files\LLVM\bin\clang.exe`.
- Installed Python 3.12 alongside an existing Python 3.14 and pointed `PYO3_PYTHON` at the 3.12 interpreter.
- `cargo build -p cli` now succeeds on Windows when `KAIN_CLANG_PATH` and `PYO3_PYTHON` are set to those resolved installs.
- `target\debug\kain.exe --help` and `target\debug\kain.exe doctor` both run once the Python 3.12 directory is on PATH.

Current risks:

- The repo still documents the vendored LLVM drop as expected, so other scripts or docs may still assume `toolchain\llvm\bin\clang.exe` exists until they are similarly refreshed.
- The workspace still pins `pyo3 0.20.x`; future machines with only Python 3.13+ or 3.14 will keep hitting the same build/runtime mismatch unless they install Python 3.12 or the dependency line is upgraded.

Recommended next step:

- Upgrade the workspace's PyO3 dependency when practical, then simplify the Windows bootstrap once Python 3.13+ support is officially available in the pinned dependency line.

## 2026-04-02 - official UE5 authoring docs pipeline added under unreal_plugins/OfficialDocs

The repo now has a dedicated UE5-facing docs set aimed at teaching plugin authoring with Kain as a UE5 DSL and codegen pipeline.

What changed:

- Added `unreal_plugins/OfficialDocs/README.md`
  - Introduces the UE5-only documentation lane, the current crate ownership split, and the recommended reading order.
- Added `unreal_plugins/OfficialDocs/01-Getting-Started.md`
  - Establishes the DSL mental model, minimal `KAIN.toml` shape, first build flow, and the role of The Oracle.
- Added `unreal_plugins/OfficialDocs/02-KAIN-TOML-And-Project-Layout.md`
  - Documents the UE5 manifest shape, module layout patterns, and recommended source organization.
- Added `unreal_plugins/OfficialDocs/03-Language-To-UE5-Mapping.md`
  - Explains how Kain constructs map to UE5 runtime output: actors, components, subsystems, structs, enums, replication, RPCs, async tasks, and state machines.
- Added `unreal_plugins/OfficialDocs/04-Editor-UI-And-Tools.md`
  - Covers Slate, Details, viewports, toolbars, asset editors, editor modules, and reactive editor bindings.
- Added `unreal_plugins/OfficialDocs/05-Shaders-Materials-And-Graphs.md`
  - Covers shader authoring, material graph generation, graph editor/runtime systems, and current shader-manifest caveats.
- Added `unreal_plugins/OfficialDocs/06-Blueprints-GAS-And-Config.md`
  - Covers Blueprint generation, the staged maturity of GAS support, and developer settings/config generation.
- Added `unreal_plugins/OfficialDocs/07-Imports-Injection-And-Migration.md`
  - Frames `kain inject` plus Rust/TS/C imports as UE5 adoption accelerators instead of the main product headline.
- Added `unreal_plugins/OfficialDocs/08-Examples-Feature-Matrix-And-Limits.md`
  - Summarizes the strongest example plugins, a high-level feature matrix, and current known limits worth preserving in future docs and marketing.

Design decisions:

- The docs intentionally position Kain as a UE5 DSL and codegen system rather than trying to explain the entire compiler at once.
- The docs keep strong separation between:
  - production-ready core UE5 codegen
  - advanced but real adjacent lanes
  - partially wired or staged features such as broader GAS phases
- The docs are example-driven and lean on `unreal_plugins/*` as proof instead of only crate-internal claims.

Current risks:

- The new docs are broad and product-facing, but they are still a first-pass foundation rather than a complete reference for every single attribute or crate submodule.
- Future doc passes should expand exact syntax coverage for:
  - editor attributes
  - graph schemas and runtime graphs
  - shader and material authoring details
  - config attribute variants

Recommended next step:

- Add a second-pass UE5 docs expansion with deeper syntax reference pages and a dedicated "cookbook" section built from the strongest example plugins such as `Example_Comprehensive`, `Example_Graph`, `Example_Shader`, `FluidFlow`, and `MetaFitter`.

## 2026-03-29 - self constructor/type normalization now covers Self_ migration artifacts

The repair engine picked up a narrower normalization pass for `Self_` forms that show up in migration drafts and still trip the parser.

What changed:

- `crates/kain-repair/src/engine.rs`
  - Expanded `normalize_self_constructor_syntax` from a bare `Self:`/`Self :` rewrite into a line-aware pass that also handles `Self_` artifacts in constructor and type positions.
  - The pass now normalizes low-risk punctuation-adjacent forms such as `Self_:` / `Self_ :` / `Self_::`, `-> Self_`, `: Self_`, `(Self_`, ` Self_)`, and comma-adjacent variants.
- `crates/kain-repair/tests/fixtures/kain_repair_reserved_self.kn`
  - Added `Self_`-shaped constructor and return-type examples alongside the existing reserved-identifier drift case.
- `crates/kain-repair/tests/repair_fixtures.rs`
  - Updated assertions to prove `Self_` is normalized back into parser-safe `Self` / `Self::` forms.

Behavior now covered:

- `fn Self_(value: Int) -> Self_` -> `fn Self(value: Int) -> Self`
- `Self_:build(type)` -> `Self::build(type)`
- `Result<Self_, Self_>` -> `Result<Self, Self>`
- `Self_(left, right)` -> `Self(left, right)`

Notes:

- No tests or `cargo check` were run.
- This is intentionally conservative: it only rewrites obvious migration artifacts in places where `Self_` is acting like a bogus constructor/type token, not arbitrary identifiers.

## 2026-03-29 - nested declaration placement now flattens parser-hostile blocks

The repair engine now has a deterministic pass for nested declaration blocks that migration drafts tuck inside other declarations and the parser rejects outright.

What changed:

- `crates/kain-repair/src/engine.rs`
  - Added `flatten_nested_declaration_placement`, a line-oriented pass that detects nested `enum` / `struct` / `trait` / `impl` headers and lifts the whole declaration block back to top-level placement by stripping the surrounding indentation.
- `crates/kain-repair/src/registry.rs`
  - Registered the new rule as a safe class pass and placed it after declaration-header normalization.
- `crates/kain-repair/src/lib.rs`
  - Added `FixKind::FlattenNestedDeclarationPlacement` and a `flatten_nested_declarations` profile toggle.
- `crates/kain-repair/tests/fixtures/kain_repair_nested_declarations.kn`
- `crates/kain-repair/tests/repair_fixtures.rs`
  - Added fixture coverage for nested `struct`, `impl`, and `enum` declarations under an outer `enum`.

Behavior now covered:

- Nested `struct ...:` blocks inside an `enum ...:` are flattened to top-level `struct ...:` blocks.
- Nested `impl ...:` blocks inside an `enum ...:` are flattened to top-level `impl ...:` blocks.
- Nested `enum ...:` blocks inside an `enum ...:` are flattened to top-level sibling declarations.

This should eliminate parser failures where proof-tree output shows declarations embedded in declaration bodies, and it should move any remaining failure to the next seam: actual semantic restructuring, invalid block contents, or other non-declaration syntax errors.

Notes:

- No tests or `cargo check` were run.
- The pass is deliberately mechanical. It does not try to rebuild module semantics; it only gets obviously hostile nested declaration placement out of the parser's way.
