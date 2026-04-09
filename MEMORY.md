# MEMORY

## 2026-04-09 - Linux LLVM and raw-native runtime lanes now validate end-to-end

The native runtime's Linux surface is no longer blocked at the public-header and validation-harness level.

What changed:

- Updated `runtime/native/include/*` and `runtime/native/src/core/*`
  - Removed Win32-only outer gating from the shared runtime-contract, realtime, UI, asset, graphics, and UI-runtime headers so Linux builds can see the same ABI contract types.
  - Kept the Win32-specific host structs and platform host APIs gated, but moved generic helpers and shared ABI types into the cross-platform surface.
  - Replaced the Unix `usleep(...)` path in `kain_runtime_core.c` with `nanosleep(...)` so the runtime compiles cleanly under modern POSIX feature levels.
- Added Linux runtime support sources
  - `runtime/native/src/platform/linux/kain_runtime_linux_shared.c` now owns Linux env/path/vector helpers plus `_putenv_s`/`Sleep`-adjacent compatibility through the shared base shims.
  - `runtime/native/src/platform/linux/kain_runtime_linux_graphics.c` provides the Linux implementation of `kain_win32_gl_surface_supports_graphics_bundle(...)` so the graphics validation lane can stay source-compatible while the host-specific OpenGL path remains Windows-only.
- Hardened runtime validation and conformance on Linux
  - Normalized runtime shell scripts to LF and taught the fixture runner to prefer the repo-local `target/debug/kain` or `target/release/kain` before falling back to PATH.
  - Switched the native smoke fixtures to current frontend-valid `fn main() -> Int: return 0` programs and made the LLVM fixture path require the final executable, not just the emitted `.ll`.
  - Updated reflection, diagnostics, UI, graphics, and actor conformance harnesses so they compile and run against Linux sources instead of hard-coded Win32 helper objects.
  - Reworked `runtime/validate_native_runtime.sh` so it validates the actual Linux runtime loop: CLI build, native runtime compile, LLVM/raw-native fixtures, and full conformance.
- Updated user-facing/runtime metadata
  - `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json` now advertise Linux in the core raw-native lane and include the Linux-specific source set / thread dependency.
  - `crates/cli/src/main.rs` now prints Linux/macOS install-refresh guidance instead of a PowerShell-only message when the active binary comes from `target/`.

Validation completed on this Linux host:

- `cargo build -p cli`
- `./target/debug/kain doctor`
- minimal `kain build -t llvm` producing and running a native executable
- `./runtime/compile_native_runtime.sh`
- `./runtime/fixtures/validate_all.sh`
- `./runtime/conformance/run_all.sh`
- `./runtime/validate_native_runtime.sh`

Current risks:

- The core raw-native/LLVM/runtime-contract lanes are validated on Linux, but the platform-host services in `native_runtime.toml` (`platform.app-host`, `platform.input`, `gfx.viewport`) are still explicitly Windows-only. Linux support is real for the shared runtime substrate, not yet for the Win32 desktop host layer.
- Several runtime/conformance C files still emit warnings under clang on Linux, but the suite passes.

Recommended next step:

- Add a non-Win32 native host provider for app-host/input/viewport services so the higher-level packaged native desktop lane can advertise Linux parity without relying on Win32-only service entries.

## 2026-04-09 - root universal installer now bundles clang into the repo toolchain

The repo now has a root cross-platform bootstrap entrypoint at `install_kain.py`.

What changed:

- Added `install_kain.py`
  - Detects Linux, macOS, or Windows at runtime.
  - Resolves `clang` from the repo toolchain, `KAIN_CLANG_PATH`, PATH, or common platform install locations.
  - Falls back to platform package managers when `clang` is missing:
    - Linux: `apt-get`, `dnf`, `yum`, `pacman`, `zypper`, `apk`
    - macOS: `brew`
    - Windows: `winget`, `choco`, `scoop`
  - Bundles clang back into the repo under `toolchain/llvm/bin`:
    - Unix-like systems symlink the discovered LLVM tools into the repo-local toolchain bin dir.
    - Windows mirrors the relevant `clang` / `llvm` / `lld` executables and DLLs into the repo-local toolchain bin dir.
  - Writes `toolchain/llvm/kain_bundle_manifest.json` so future agents can see where the current bundled toolchain came from.
  - Builds `cargo build --release -p cli`, installs `kain` and `kn` into the cargo bin dir, and emits activation scripts under `generated/kain-env.sh` and `generated/kain-env.ps1`.
- Updated docs
  - `README.md` now points at the root installer as the first bootstrap step.
  - `toolchain/README.md` documents that the installer repopulates `toolchain/llvm/bin`.
  - `ARCHITECTURE.md` adds the new installer to common commands and fresh-clone guidance.

Design decisions:

- Kept the installer as a single root Python script so Linux, macOS, and Windows all share one bootstrap path.
- Chose repo-local clang bundling over env-only discovery because too much of the repo still assumes `toolchain/llvm/bin/clang*` exists.
- Emitted activation scripts instead of directly mutating user shell profiles in v1. That keeps the installer deterministic and avoids hidden shell-specific side effects.

Current risks:

- The package-manager install paths are best-effort. Some machines will still need manual LLVM setup, especially when `sudo`, `winget`, or `brew` is unavailable.
- Windows bundling currently mirrors the relevant LLVM bin files rather than managing a full versioned LLVM drop under `toolchain/llvm`.
- The older `scripts/sync-kain-source-of-truth.ps1` path still exists, so the repo now has both a Windows-specific sync script and the new universal installer until that consolidation happens.

Recommended next step:

- Make the Windows PowerShell sync path delegate to `install_kain.py` or share one manifest-driven bootstrap core so the repo only has one real installer contract.

## 2026-04-08 - compiler-owned intent quartet landed across parser, runtime, bundles, and driver root selection

Kain picked up the first full pass of the compiler-owned intent quartet: `patch`, `converge`, `world`, and `orchestrate`.

What changed:

- Updated `crates/kain-core/src/ast.rs`, `parser.rs`, and `types.rs`
  - Added new top-level item forms for `patch`, `converge`, `world`, and `orchestrate`.
  - Added `Expr::StageCall` for typed stage-runtime syntax such as `rust fn_name(...)`.
  - Added typed-item support, world surface validation, required-v1 world surfaces, patch mutation-path collection, patch undo-mode classification, converge signature checking, and orchestration stage descriptors.
- Updated `crates/kain-core/src/runtime.rs`
  - Registered and executed `patch`, `converge`, and `orchestrate` as real runtime values.
  - Added patch transaction recording with mutation paths and undo mode.
  - Added converge lane dispatch plus test-lane verification against `spec`.
  - Preserved concrete test failure messages in `run_tests` so converge mismatch diagnostics survive the harness boundary.
- Updated `crates/kain-core/src/runtime_contract.rs` and `realtime_app_bundle.rs`
  - Added explicit `patches[]`, `converges[]`, `worlds[]`, and `orchestrations[]` sections.
  - Added capability / requirement keys for `patch.transactions`, `converge.dispatch`, `world.native-ui`, `world.viewport3d`, `world.web`, `world.ue5`, and `orchestrate.pipeline`.
- Updated downstream consumers
  - `crates/kain-driver/src/lib.rs` and `crates/kain-driver/src/native_app.rs` now resolve native-ui roots from a single `world`'s `native_ui` surface and reject ambiguous multi-world inputs without an explicit selection.
  - `crates/web`, `crates/gpu`, `crates/kain-sys-codegen`, and `crates/ue5` were patched for the new `ResolvedType::Future` / `Expr::StageCall` / `TypedItem` exhaustiveness fallout so the feature compiles through the wider toolchain.
- Added focused validation
  - `crates/kain-core/tests/compiler_owned_intent_test.rs`
  - new driver/native-app unit coverage for single-world auto-root and multi-world rejection
  - `smoketest/compiler_owned_intent` plus an `allinone` manifest entry
  - `docs/kainplan/08_COMPILER_OWNED_INTENT_QUARTET.md`

Design decisions:

- Kept the new starters contextual at legal item boundaries instead of reserving them globally.
- Treated the quartet as bounded semantic declarations, not expression-wide grammar rewrites.
- Required all four `world` surfaces in v1 to keep projection coverage explicit instead of leaving partial adapter truth ambiguous.
- Kept `orchestrate` stage-runtime labels semantic in v1; the runtime still dispatches through existing function execution rather than invoking external bridges directly.

Current risks:

- The new feature lane is covered by focused tests, but full `cargo test -p kain-driver --lib` still includes unrelated long-running / networked / pre-existing failures outside this implementation slice.
- `smoketest/compiler_owned_intent/run_smoke.ps1` was added but not executed in this Linux session.
- `world` root selection is currently wired through native-ui/realtime root discovery; deeper per-adapter activation logic for viewport/web/UE5 remains future work.

Recommended next step:

- Make `world` an explicit first-class selection target across more CLI/package flows and teach `orchestrate` stage runtimes to hand off into the real Rust/Python/Node bridge crates instead of stopping at semantic labels.

## 2026-04-08 - kain-core now performs real executable-body semantic checks

The language core picked up the first meaningful semantic-trust pass instead of only walking bodies for syntax-shape validation.

What changed:

- Updated `crates/kain-core/src/types.rs`
  - Expanded the type environment to track global symbols, method signatures, and enum variant payloads.
  - Added real semantic checking for executable bodies: `let` bindings, assignments, returns, calls, method calls, conditionals, loops, `match`, `await`, async blocks, and core low-level memory expressions.
  - Added `ResolvedType::Future(Box<ResolvedType>)` and taught the checker to understand `impl Future<T>`, `async ...`, and `await ...`.
  - Added compatibility-aware builtins for shader/runtime semantics that the stricter checker now depends on, including `Void`, `Vec4`, `vec2` / `vec3` / `vec4`, `dispatch_thread_id`, tuple swizzles like `.x`, and `StorageBuffer<T>` indexing.
  - Added early semantic errors for return-type mismatches, incompatible `match` arm result types, and duplicate boolean match arms.
- Added `crates/kain-core/tests/semantic_typecheck_test.rs`
  - Locks in the new behavior with focused tests for return checking, `match` arm validation, duplicate boolean-arm rejection, and typed async/await acceptance.
- Updated `ARCHITECTURE.md`
  - Documented that `kain-core` now performs executable-body semantic validation before downstream bundle/codegen lanes consume the typed program.

Design decisions:

- Kept the public `TypedProgram` / `TypedItem` surface stable for downstream crates in this phase instead of forcing a broad typed-IR migration immediately.
- Chose a permissive semantic checker that errors on clear known mismatches but still falls back to `Unknown` for unsupported or backend-specific language corners, so the wider repo does not break all at once.
- Treated shader/runtime builtins as compiler-known semantic symbols rather than leaving them as implicit runtime-only behavior.

Current risks:

- The checker is materially stronger, but it is still not a full “typed IR everywhere” system yet; many paths still degrade to `Unknown` instead of proving precise types.
- Full `cargo test -p kain-core --lib --tests` on this machine still shows unrelated/pre-existing failures outside this patch:
  - `language_features::tests::default_profile_keeps_struct_literals_disabled`
  - `stdlib::tests::test_find_stdlib_from_env_var` when `KAIN_STDLIB_PATH` is already exported in the shell
  - two `realtime_app_bundle` tests around viewport parsing / duplicate scene emission
- Warning-capable diagnostics and hardening of non-exhaustive `match` are still future work; this patch validates arm agreement and obvious duplicate bool arms, but it does not yet introduce a formal warnings channel.

Recommended next step:

- Add a typed-body IR layer on top of this semantic pass and thread its results into monomorphization, runtime-contract emission, and downstream codegen so fewer language paths need to fall back to `Unknown`.

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
