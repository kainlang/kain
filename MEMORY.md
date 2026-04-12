# MEMORY

## 2026-04-11 - graphics vendor incorporation plan added for bgfx, bx, bimg, filament-core, and diligentcore

The runtime now has a concrete graphics-vendor doctrine instead of a generic “maybe wire all the renderers in” impulse. The new vendors are useful, but they need different roles.

What changed:

- Added `runtime/KAIN_GRAPHICS_VENDOR_INTEGRATION_PLAN_2026-04-11.md`
  - Positioned `bgfx` as the first practical renderer/backend lane for Kain.
  - Positioned `bx` and `bimg` as support infrastructure that should travel with the `bgfx` lane instead of pretending to be standalone runtimes.
  - Positioned `filament-core` as the higher-level premium scene/material/lighting experiment.
  - Positioned `diligentcore` as the future explicit Kain-native render-graph / compute / device-control lane.
  - Defined the wiring order:
    - build-path preparation for graphics vendors
    - `bgfx` + `bx` + `bimg`
    - viewport/backend split
    - `filament-core`
    - `diligentcore`
- Updated `ARCHITECTURE.md`
  - Recorded the renderer-vendor doctrine as a guardrail so future agents do not try to wire `bgfx`, Filament, and Diligent into one undifferentiated viewport path.

Design decisions:

- Chose `bgfx` first because it solves the fastest practical problem: a real cross-platform renderer/backend lane.
- Chose not to start with Filament even though it offers better immediate visuals, because it solves “look expensive” before it solves “make the runtime host/render sanely across platforms”.
- Chose not to start with Diligent even though it is strategically valuable, because it is the deeper architecture lane rather than the fastest usable runtime rendering substrate.
- Treated `gfx.viewport` as a host-facing presentation service, not as the permanent renderer contract.

Current risks:

- The current native runtime compile path is still effectively C-oriented, while `bgfx`, `filament-core`, and `diligentcore` are C++-heavy implementation trees.
- `bgfx` is easier to approach first because it has a C99 surface, but Filament and Diligent likely need deliberate bridge-library treatment before they should enter the runtime manifest path.
- The runtime still lacks a Kain-owned renderer backend interface, so the first implementation slice should create that seam before trying to make the viewport host cross-renderer-aware.

Recommended next step:

- Build the first renderer-backend seam with `bgfx` + `bx` + `bimg`, then split `gfx.viewport` into host-facing and backend-facing halves before attempting Filament or Diligent integration.

## 2026-04-11 - native runtime metadata and platform truth were resynchronized after the vendor slice

The first vendor-backed runtime slice landed cleanly on Linux, but the manifest and tooling metadata drifted immediately afterward. This follow-up pass brought the native runtime companion metadata back into sync and widened the new vendor lane so Windows development is represented explicitly instead of being treated like a Linux-only accident.

What changed:

- Re-synced the native runtime manifest, CLI manifest loader, compile script, and metadata JSON
  - Updated `crates/cli/src/main.rs` so the manifest parser understands platform-specific define lists through `windows_defines`, `linux_defines`, and `macos_defines`.
  - Updated `runtime/compile_native_runtime.sh` to honor those platform-specific define lists instead of treating POSIX-oriented defines as globally applicable.
  - Rebuilt `runtime/native_runtime_metadata.json` so it reflects the current vendor-backed service families, source groupings, link dependencies, and per-service platform scope from `runtime/native_runtime.toml`.
- Tightened vendor/platform truth for the new service families
  - Updated `runtime/native_runtime.toml` so the vendor-backed `io.*`, `script.quickjs`, `audio.*`, `wasm.*`, and `allocator.*` families declare `platforms = ["windows", "linux"]` instead of looking globally available.
  - Moved libuv-specific compile flags out of shared `defines` and into `windows_defines` / `linux_defines`.
  - Added the Windows libuv source graph plus the required extra Windows link libraries so the manifest can describe a real Windows vendor lane rather than only the Linux build.
  - Updated `runtime/native/include/kain_runtime_vendor_lane.h` and `runtime/native/src/vendor/kain_runtime_vendor_lane.c` so libuv-backed services are allowed on Windows as well as Linux.
- Strengthened docs around metadata durability
  - Updated `runtime/README.md`, `runtime/changelogs/NATIVE_RUNTIME_METADATA.md`, and `ARCHITECTURE.md` to make the TOML/JSON companion rule explicit and to document the platform-specific define model.

Validation completed:

- `cargo test -q -p cli runtime_manifest_resolves_relative_paths -- --nocapture`
- `cargo run -q -p kain-runtime-parallel -- json`
- `./runtime/compile_native_runtime.sh`
- `./runtime/validate_native_runtime.sh`
  - full suite passed again after the manifest/metadata sync: CLI build, native runtime compilation, LLVM/raw-native fixtures, and native runtime conformance

Design decisions:

- Treated `native_runtime.toml` as the primary build/runtime truth and `native_runtime_metadata.json` as a required tooling mirror, not as an eventually-consistent doc artifact.
- Chose explicit per-service platform declaration instead of overloading `status = "available"` to mean "maybe, depending on hidden macros".
- Extended the libuv-backed lane to Windows in the manifest/build model now, even though Linux remains the only deeply validated host at the moment.

Current risks:

- Windows support is now modeled directly in the manifest and vendor lane, but it has not yet been proven by the same end-to-end runtime validation depth as Linux on this host.
- The tooling metadata is synchronized again, but there is still no automated guard that diff-checks TOML service declarations against the JSON companion file.
- `wasm.runtime.full` and `wasm.wasi` remain staged/planned surfaces; the metadata now says so more honestly, but the runtime still needs the real WAMR source selection and compile proof.

Recommended next step:

- Add an automated metadata parity check between `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json`, then run a Windows-native compile/validation pass so the newly declared Windows vendor lane is proven instead of only modeled.

## 2026-04-11 - first native vendor incorporation slice is now live in the manifest-driven C runtime

The native runtime is no longer only planning around third-party vendors; the first real vendor-backed service slice is wired into `runtime/native`, compiled through the manifest-driven bundle, and validated end to end on Linux.

What changed:

- Expanded `runtime/native_runtime.toml`
  - Added `native/src/vendor/kain_runtime_vendor_lane.c`.
  - Added real vendor source sets for `QuickJS`, `miniaudio`, `wasm3`, `mimalloc`, `rpmalloc`, and the Linux `libuv` source graph.
  - Added vendor include roots and manifest defines for the Linux/libuv-oriented build.
  - Added new service declarations for:
    - `io.loop`, `io.fs`, `io.net`, `io.process`, `io.timers`
    - `script.quickjs`
    - `audio.backend`, `audio.graph`, `audio.device`, `audio.assets`
    - `wasm.runtime.light`, `wasm.runtime.full`, `wasm.module`, `wasm.wasi`
    - `allocator.mimalloc`, `allocator.rpmalloc`
- Added `runtime/native/include/kain_runtime_vendor_lane.h` and `runtime/native/src/vendor/kain_runtime_vendor_lane.c`
  - Introduced Kain-owned vendor function tables and a small vendor descriptor catalog.
  - Wired real probe/start/allocate/eval hooks for `libuv`, `QuickJS`, `miniaudio`, `wasm3`, `mimalloc`, and `rpmalloc`.
  - Left `WAMR` intentionally staged/degraded instead of pretending the curated tree is production-ready.
  - Added `KAIN_RUNTIME_VENDOR_STUBS_ONLY` so conformance harnesses can compile the service catalog without dragging the full vendor source graph into every isolated test binary.
- Expanded runtime service and contract surfaces
  - Updated `runtime/native/include/kain_runtime_services.h` and `runtime/native/src/core/kain_runtime_services.c` with the new vendor-backed service families and function-table pointers.
  - Updated `runtime/native/include/kain_runtime_contract.h` and `runtime/native/src/core/kain_runtime_contract.c` with new service-mask bits and `has_*` fields for the vendor families.
  - Preserved the raw-native core assumptions: `KAIN_RUNTIME_SERVICE_CORE_MASK`, `missing_core_service_count`, and `valid_for_raw_native` still only care about the existing three Win32 host services.
- Hardened build and vendor trees
  - Updated `runtime/compile_native_runtime.sh` to consume manifest `defines`, not just sources and include dirs.
  - Patched `runtime/thirdparty/quickjs/quickjs.c` with a `CONFIG_VERSION` fallback so Kain can compile the engine cleanly from the vendor tree.
  - Added `runtime/thirdparty/wamr/core/version.h` as a Kain shim because the curated WAMR tree omits the generated upstream header that `wasm_runtime_common.c` expects.
- Updated conformance harnesses
  - Patched `runtime/conformance/diagnostics/compile_tests.sh` and `runtime/conformance/host_bridge/run_tests.sh` so they compile the vendor lane in stub-only mode when they only need the registry/catalog surface.

Validation completed:

- `./runtime/compile_native_runtime.sh`
  - now compiles a 92-object native runtime bundle with the vendor sources included
- `./runtime/conformance/run_all.sh --verbose`
- `./runtime/validate_native_runtime.sh`
  - full suite passed: CLI build, native runtime compilation, LLVM/raw-native fixtures, and native runtime conformance

Design decisions:

- Treated vendor incorporation as Kain-owned service expansion, not as raw API exposure.
- Chose a real first slice now: `libuv`, `QuickJS`, `miniaudio`, `wasm3`, `mimalloc`, and `rpmalloc` are compile-backed and surfaced through the runtime.
- Kept `WAMR` staged because the curated tree still needs more than a thin manifest add; the missing generated `version.h` was only the first blocker, not proof of readiness.
- Added a stub-only vendor mode for narrow conformance binaries instead of forcing every conformance harness to link the full vendor graph.

Current risks:

- The vendor-heavy manifest materially increases compile time and warning volume; warning cleanup is now a real runtime maintenance lane.
- The new vendor service families are surfaced and probeable, but they are not yet deeply integrated into scheduler policy, host bridge semantics, or the runtime value ABI.
- `WAMR` is still only staged. The shimmed `version.h` unblocks future work, but the full/runtime+WASI lane still needs a deliberately reduced source selection and another compile pass.
- Windows support for the new vendor lane is not proven by the same depth as Linux yet, even though the wrapper source is guarded to avoid forcing Linux-only `libuv` symbols into non-Linux builds.

Recommended next step:

- Start replacing ad hoc runtime subsystems with the new vendor-backed families one seam at a time:
  - move async timer/wake infrastructure toward `io.loop` / `io.timers`
  - add first real `script.quickjs` module-loading and host-bridge hooks
  - add a Kain-owned `audio.*` API above the miniaudio lane
  - decide whether allocator selection becomes runtime-configurable before wiring mimalloc/rpmalloc into shared allocation helpers

## 2026-04-11 - vendor-edit harvest plan added for third-party runtimes under runtime/thirdparty

The repo now has an explicit plan for how imported third-party runtimes should strengthen Kain without becoming the runtime's source of semantic truth.

What changed:

- Added `runtime/KAIN_RUNTIME_HARVEST_PLAN_2026-04-11.md`
  - Defined the operating model for `runtime/thirdparty/` as `vendor, complete, patch, wrap, validate`.
  - Made the intended workflow explicit: edit vendored runtime files in place for Kain rather than embedding them unchanged or rewriting them from scratch.
  - Classified the current third-party imports by practical usefulness:
    - `quickjs`: strongest real integration candidate and the first recommended active service family
    - `wren`: promising embed API, but current import is incomplete
    - `mruby`: useful architecture source, but current import is incomplete
    - `lua`: valuable VM/data-structure reference, but current import is incomplete
    - `cpython/dictobject.c`: idea source for maps/dicts, not a runtime integration path by itself
  - Defined Kain-owned target families around those imports such as `script.quickjs`, `script.wren`, `script.mruby`, `script.lua`, and `data.dict.experimental`.
  - Recorded the first execution order: provenance/completeness audit, QuickJS-first integration, data-structure harvest from Lua and CPython dict behavior, then explicit completion-or-prune decisions for the half-imported runtimes.

Design decisions:

- Chose a vendor-edit strategy instead of a clean-room rewrite strategy because the imported runtimes already contain useful mature machinery that Kain can adapt faster than recreating from zero.
- Preserved the rule that Kain still owns semantics, the kernel, diagnostics, tracing, permissions, and public extension contracts.
- Positioned QuickJS as the first active dynamic scripting/service-runtime candidate because it is the only current third-party tree that looks close to a usable embeddable drop.

Current risks:

- `runtime/thirdparty/` is still in an ambiguous repo state: several trees look partial, no shared inventory or patch ledger exists yet, and provenance/build completeness is not normalized.
- Editing vendor code in place will pay off only if patch provenance and conformance are kept disciplined; otherwise the trees will decay into untraceable forks.
- The new plan still depends on a future unified Kain runtime value ABI and Kain-owned script-runtime contract so foreign runtimes do not become ad hoc boundary APIs.

Recommended next step:

- Add `runtime/thirdparty/INVENTORY.md` and a first QuickJS integration spec so the repo distinguishes clearly between buildable vendor lanes, reference-only imports, and the first real harvested runtime service.

## 2026-04-11 - phase2 selfhost advanced through parser, borrow, variant, and builtin-method blockers but is still not green

Phase2 selfhost is materially further forward than the earlier parser-wall state, but it is not fully green yet. The work in this pass pushed the generated `kain-core.kn` through several real language and selfhost-repair seams and left the current front blocker at a smaller builtin-surface gap instead of the earlier structural failures.

What changed:

- Updated `crates/kain-import/src/rust/transformer.rs`
  - Resolved impl `Self` more aggressively across receivers, return types, value/type paths, and pattern variant heads.
  - Added enum-constructor-aware lowering so explicit enum variant values and constructor references lower correctly, including lambda synthesis for higher-order constructor values.
  - Preserved borrowed `for` iterables and added explicit borrowed-field-base deref insertion plus borrow-adapter lowering (`as_ref`, `as_mut`, `as_deref`, `as_deref_mut`).
  - Normalized const/static borrowed slices into owned array-ish shapes for selfhost import.
- Updated `crates/kain-core/src/types.rs`
  - Added bidirectional lambda inference for higher-order call/method sites.
  - Fixed borrowed pattern matching so borrowed enum payloads stay borrowed.
  - Allowed field access through borrowed struct/pointer wrappers.
  - Preserved named-field identity for struct-style enum variants in both pattern binding and enum construction checks instead of positionally zipping fields.
  - Added builtin method typing for `Option.map`, `Option.unwrap_or`, `Result.map`, `Result.map_err`, `Result.unwrap_or`, and `Array.contains`.
  - Normalized empty tuple literals `()` to `Unit`.
  - Added focused regressions for borrowed field access, borrowed struct-variant binding by field name, higher-order `Result.map`, empty tuple unit inference, and array `contains`.
- Updated `crates/cli/src/selfhost.rs`
  - Added a named-function selfhost repair for `extract_compute_metadata_from_comptime_block(...)` so the generated Kain stays statement-shaped instead of leaking `none`-typed expression arms into a unit-only control-flow position.

Validation completed:

- `cargo test -q -p kain-import preserves_borrowed_for_loop_iterables -- --nocapture`
- `cargo test -q -p kain-import lowers_builtin_variant_constructor_values_to_lambdas -- --nocapture`
- `cargo test -q -p kain-import lowers_explicit_tuple_variant_constructor_values_to_lambdas -- --nocapture`
- `cargo test -q -p kain-import lowers_explicit_enum_variant_values_in_expression_position -- --nocapture`
- `cargo test -q -p kain-core typecheck_for_loops_over_borrowed_arrays_as_borrowed_items -- --nocapture`
- `cargo test -q -p kain-core typecheck_infers_lambda_parameters_from_expected_function_type -- --nocapture`
- `cargo test -q -p kain-core typecheck_infers_lambda_arguments_for_higher_order_methods -- --nocapture`
- `cargo test -q -p kain-core typecheck_borrowed_variant_patterns_bind_borrowed_payloads -- --nocapture`
- `cargo test -q -p kain-core typecheck_allows_field_access_through_borrowed_structs -- --nocapture`
- `cargo test -q -p kain-core typecheck_binds_struct_variant_fields_by_name_under_borrow -- --nocapture`
- `cargo test -q -p kain-core typecheck_infers_result_map_callback_types -- --nocapture`
- `cargo test -q -p kain-core typecheck_treats_empty_tuple_literal_as_unit -- --nocapture`
- `cargo test -q -p kain-core typecheck_allows_array_contains -- --nocapture`
- repeated `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir /tmp/kain_phase2_probe_after_patch*`

Design decisions:

- Fixed the real type/import semantics where the language surface was clearly missing something broadly useful, instead of papering over every phase2 blocker in the repair lane.
- Used the selfhost named-function repair only where the generated shape itself was clearly the wrong control-flow form for Kain (`extract_compute_metadata_from_comptime_block`).
- Kept generic selfhost strictness intact; this pass widened well-scoped language support rather than weakening type safety globally.

Current risks:

- Phase2 still is not green. After the latest pass, the current front blocker is in generated `kain-core.kn` around `COMPUTE_PLAN_BINDING_NAMES.contains(&name)`: the checker now accepts `Array.contains`, but the call site is still producing a deeper borrowed argument shape (`&&String`) and likely needs either another small builtin-method compatibility expansion or importer-side borrow cleanup.
- The selfhost lane is still surfacing method-surface gaps one family at a time, so more builtin collection/option/result/string methods may still need to be modeled before the stage2 workspace build becomes the active blocker.
- The worktree also contained unrelated user-side changes in `runtime/conformance/platform_parity/*`; those were left untouched.

Recommended next step:

- Continue phase2 from the current `Array.contains(&name)` seam, then keep rerunning `kain selfhost phase2` until the next front blocker moves from semantic typing to stage2 workspace assembly/build failures.

## 2026-04-11 - dated runtime expansion roadmap added for self-hosting, MCP, web, graphics, audio, and foreign runtime growth

A new dated runtime planning document now captures the desired long-range direction for the native/runtime stack under the repo's current private, AI-developed assumptions.

What changed:

- Added `runtime/KAIN_RUNTIME_EXPANSION_ROADMAP_2026-04-11.md`
  - Reframed the runtime target away from a narrow native substrate and toward a capability-driven runtime kernel intended to support self-hosting, web/runtime services, MCP, large 3D/graphics systems, audio/DAW execution, comptime growth, and a foreign-runtime mesh.
  - Defined a phased roadmap covering runtime constitution, kernel/scheduler work, unified runtime values and memory, platform and web services, MCP runtime families, graphics/scene/compute, audio/media, UI/workspace hosting, foreign language integration, self-hosting/comptime, and hardening.
  - Added a foreign-language viability study anchored to official Go, Rust, and Zig documentation.
  - Recorded the strategic language split:
    - Rust and Zig are the strongest Tier 1 in-process runtime-extension candidates.
    - Go is useful but should primarily be treated as a sidecar/service/WASI language rather than the default deep in-process kernel language.
    - WASM should become a first-class sandboxed extension lane once the runtime value ABI is mature.

Design decisions:

- Treated the repo's private/unreleased status as explicit permission for aggressive runtime and ABI refactors before any deliberate v1 freeze.
- Positioned the next big architectural jump as a kernel/value/module-system problem, not just a feature backlog problem.
- Kept the foreign-runtime recommendations contract-first: generated schemas and canonical service families should own the language boundaries rather than bespoke bridge logic.

Current risks:

- The roadmap is intentionally aggressive and assumes the project will tolerate large refactors across compiler, runtime, driver, and bridge layers.
- The current runtime still has real architectural gaps relative to the new target, especially around a unified value model, a stronger scheduler/kernel, cross-platform host services, deep graphics execution, and an audio/DAW subsystem.
- Zig references currently point at the official `master` language reference; production adoption should pin to stable Zig releases when concrete implementation work starts.

Recommended next step:

- Turn the roadmap's first two phases into concrete implementation specs for the runtime constitution, the unified runtime value ABI, and the scheduler/kernel refactor, because those three decisions will constrain every later web/MCP/graphics/audio/foreign-runtime lane.

## 2026-04-11 - native interpreter control-flow and builtin surface are more consistent, and the current Brainfuck lab is now failing on bad expectations rather than the original language bug

The Brainfuck investigation produced one real interpreter bug, one real builtin-surface mismatch, and one misleading test-harness problem.

What changed:

- Updated `crates/kain-core/src/runtime.rs`
  - Fixed native interpreter `elif` execution by teaching `Expr::If` to evaluate `ElseBranch::ElseIf` chains instead of silently dropping them.
  - Added native method dispatch for `String.len()` so the runtime matches the existing typechecker surface for string length.
  - Aligned interpreter `char_at` with the existing native runtime/string contract by returning an empty string for negative or out-of-range access instead of interpreter-only `none`.
- Updated `crates/kain-core/src/stdlib.rs`
  - Added missing stdlib registry entries for `ord` and `chr` so the registry/documented builtin surface matches the runtime/type globals more closely.
- Added `crates/kain-core/src/runtime_tests.rs` and wired it from `crates/kain-core/src/lib.rs`
  - Added focused regressions covering `elif` execution, `String.len()` runtime support, `char_at` out-of-range behavior, and stdlib exposure of `ord`/`chr`.

Validation completed:

- `cargo test -p kain-core runtime_tests -- --nocapture`
- inline interpreter repro now returns `2` for a direct `if/elif` chain on `opcode == "+"`
- inline interpreter repro now returns `3` for `"abc".len()`
- inline interpreter repro now returns an empty string for `char_at("abc", 99)`
- `cargo run -q -p cli -- run labs/brainfuck/main.kn`
- independent local reference Brainfuck execution confirmed that the current fixture programs themselves produce:
  - `hello_world.bf` -> `A[bbe\x1dPehbZ\x1e\t`
  - `echo_input.bf` -> `K`
  - `alphabet_pair.bf` -> `9:`

Design decisions:

- Did not force a repo-wide type-surface migration for `read_file` or `char_at` yet. Existing Kain sources use both direct-string and result-style assumptions, so the safe first step was to fix the interpreter/runtime mismatches that were clearly on the critical path without breaking broad call-site surface area.
- Treated the Brainfuck lab as a semantics probe, not as infallible truth. After the interpreter fixes, the remaining failures came from incorrect expected outputs in the harness, not from disagreement with an independent Brainfuck interpreter.

Current risks:

- `read_file` is still a deeper contract split across the repo: some surfaces assume `String`, others declare `Result<String, String>`, and the runtime behavior is not consistently modeled end-to-end yet.
- The LLVM lane remains a separate blocker for any clean “Brainfuck proves Kain” claim; this pass only addressed the native interpreter/runtime semantics.
- The current Brainfuck fixture names/expected outputs are misleading for proof purposes until they are corrected or the fixture programs are replaced with canonical ones.

Recommended next step:

- Decide the canonical Kain contract for file/string builtins, especially `read_file`, then align runtime, typechecker, stdlib declarations, and any existing Kain wrappers together in one deliberate pass. After that, either fix the Brainfuck harness expectations or replace the fixtures with canonical programs before using it as a Turing-completeness proof artifact.

## 2026-04-11 - Ouroboros selfhost tooling now resolves repo roots from the current checkout and can run its main control-plane paths on Linux without `M:\Code\...` defaults

The Ouroboros/selfhost control plane had drifted into a Windows-only operator loop: absolute `M:\Code\...` roots were baked into the manifest runner, repair tooling, inventory extraction, status scripts, and operator docs, and the phase runner still launched steps through `cmd /c` and PowerShell-specific lanes.

What changed:

- Added `ouroboros/tools/ouroboros_pathing.py`
  - Introduced shared workspace discovery for `repo_root` and `ouroboros_root`.
  - Supports explicit `OUROBOROS_ROOT` / `KAIN_REPO_ROOT` overrides but otherwise resolves from the current checkout layout.
  - Added platform-aware executable-name handling for `kain` vs `kain.exe`.
- Updated `ouroboros/tools/selfhost_pipeline/run_pipeline.py`
  - Replaced hardcoded default manifest/output roots with checkout-relative resolution.
  - Added manifest-default template expansion seeded from discovered roots plus `sys.executable`.
  - Reworked step execution around structured `argv` lists, optional working directories, and artifact-log emission instead of `cmd /c` / PowerShell launch assumptions.
  - Made stage2 binary detection check both Linux and Windows binary names.
- Updated `ouroboros/docs/selfhost/pipeline_manifest.json`
  - Replaced absolute drive-letter defaults with `{ouroboros_root}` templates.
  - Converted `analyze`, `phase2-core`, and `phase2-full` steps to data-driven `argv` execution with explicit `cwd` and `artifact_log` fields.
  - Removed the PowerShell-script dependency from the core/full lane definitions by letting the runner execute Cargo checks directly.
- Updated `ouroboros/tools/selfhost_repair/repair_runner.py` and `ouroboros/scripts/extract_selfhost_inventory.py`
  - Switched default roots to the shared resolver so repair and inventory extraction follow the active checkout instead of a fixed Windows path.
- Added `ouroboros/scripts/selfhost_workspace_status.py`
  - Ported the machine-readable workspace status surface to Python so Linux operators no longer depend on `selfhost_workspace_status.ps1`.
- Updated `ouroboros/automation/config/pipeline.config.json`, `ouroboros/tools/selfhost_pipeline/README.md`, `ouroboros/tools/selfhost_repair/README.md`, `ouroboros/docs/selfhost/phase2-current-status.md`, and `ouroboros/docs/selfhost/repairs/repair_workflow.md`
  - Repointed operator commands and path references to repo-relative, Linux-safe entrypoints.
- Updated the legacy PowerShell helpers under `ouroboros/scripts/*.ps1`
  - They still exist for Windows users, but now derive their defaults from `PSScriptRoot` instead of fixed `M:\Code\OuroborosV2\...` paths and recognize both `kain` and `kain.exe`.

Validation completed:

- `python -m py_compile ouroboros/tools/ouroboros_pathing.py ouroboros/tools/selfhost_pipeline/run_pipeline.py ouroboros/tools/selfhost_repair/repair_runner.py ouroboros/scripts/extract_selfhost_inventory.py ouroboros/scripts/selfhost_workspace_status.py`
- `python ouroboros/tools/selfhost_pipeline/run_pipeline.py list`
- `python ouroboros/scripts/selfhost_workspace_status.py`
- `python ouroboros/tools/selfhost_repair/repair_runner.py analyze --validation skip --input-root ouroboros/out/selfhost/phase2 --repaired-root /tmp/kain_phase2_repaired_test --repair-docs ouroboros/docs/selfhost/repairs --report-json /tmp/kain_phase2_repair_report.json --report-md /tmp/kain_phase2_repair_report.md`
- `python -m json.tool ouroboros/docs/selfhost/pipeline_manifest.json`
- `python -m json.tool ouroboros/automation/config/pipeline.config.json`
- `python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane analyze --out-dir /tmp/kain_pipeline_out`
  - this now resolves roots correctly on Linux and fails honestly on missing `phase2` artifacts instead of on bad Windows path assumptions

Design decisions:

- Treated the manifest as the long-term source of truth for lane execution and moved more behavior into structured `argv` / `cwd` / `artifact_log` fields instead of scattering platform-specific wrapper scripts.
- Kept the PowerShell helpers for Windows continuity, but removed them from the main manifest-driven Linux operator path.
- Used checkout-relative discovery first and env overrides second so local development works with no extra setup while CI or split-repo operators can still pin roots explicitly.

Current risks:

- Some older Ouroboros reports and secondary docs still mention `M:/Code/...`; the live tooling path is fixed, but not every archival note has been normalized.
- The manifest runner now owns more of the Cargo check flow directly, so future step kinds should stay structured instead of slipping back toward shell-string wrappers.
- There are still unrelated absolute-path references elsewhere in the broader repo outside the main Ouroboros selfhost control plane.

Recommended next step:

- If Linux is now the primary dev lane, continue normalizing the remaining live non-Ouroboros absolute-path assumptions in runtime tests and helper tooling, but keep that as a separate pass from the now-working selfhost control plane.

## 2026-04-11 - phase1 selfhost is no longer blocked by the opaque `dyn Any + Send + Sync` host-object carrier in `kain-core`

Phase1 strict selfhost had collapsed to a single remaining blocker in `kain-core`: the Rust importer treated the runtime's opaque host-object carrier (`Arc<dyn Any + Send + Sync>`) as a fatal trait-object erasure even though this seam is intentionally just an opaque payload boundary, not a dynamic-dispatch trait API that phase1 needs to preserve exactly.

What changed:

- Updated `crates/kain-import/src/rust/transformer.rs`
  - Added `trait_object_is_opaque_any_carrier(...)` to detect the narrow `dyn Any` carrier family, including wrapped forms like `Arc<dyn Any + Send + Sync>`.
  - Stopped emitting `trait_object_lowering` diagnostics for that narrow family while leaving the fatal diagnostic in place for real dynamic-dispatch trait-object lowering such as `dyn std::fmt::Write`.
  - Added a regression proving the importer stays strict for normal dyn traits but does not flag the opaque Any carrier.

Validation completed:

- `cargo test -p kain-import records_dyn_trait_lowering_diagnostics -- --nocapture`
- `cargo test -p kain-import does_not_flag_opaque_any_trait_object_carriers -- --nocapture`
- `cargo run -q -p cli --bin kain -- selfhost phase1 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir /tmp/kain_phase1_probe`

Design decisions:

- Did not weaken the selfhost allowlist or globally permit trait-object lowering.
- Treated only the `Any` carrier seam as acceptable because phase1 uses it as opaque host-state transport, not as trait-object behavior that must survive into Kain semantics.

Current risks:

- This makes phase1 honest for the current `kain-core` host-object seam, but it does not mean Kain now has general `dyn trait` semantics.
- If more trait-object families appear in the selfhost slice later, they should be handled case-by-case instead of by broadening this exception.

Recommended next step:

- Push phase2 again after phase1 goes green; the next blockers should now be real downstream import/codegen/workspace issues rather than the last phase1 strict-import policy tripwire.

## 2026-04-11 - compiler-owned intent suite is now coherent across parser, typechecker, runtime, bundles, driver, and LSP

The original quartet is no longer the right mental model. Kain now ships a five-part compiler-owned intent suite: `law`, `patch`, `converge`, `world`, and `orchestrate`.

What changed:

- Updated `crates/kain-core/src/ast.rs`, `parser.rs`, `types.rs`, `runtime.rs`, `runtime_contract.rs`, and `realtime_app_bundle.rs`
  - Added first-class `law` declarations and runtime-callable `Value::Law`.
  - Tightened `law` semantics to Bool-returning invariant declarations emitted into `laws[]` in both bundle families.
  - Removed world-state name leakage from the global type environment; world state is now accessed only through the world value.
  - Relaxed `world` validation from “all four surfaces required” to “at least one surface required,” while keeping duplicate-surface rejection.
  - Made `converge` selector-less `fast` lanes valid declaration-ordered defaults.
  - Made `verify random(n)` executable for real call args plus deterministic synthesized samples, with a hard typecheck fence around the supported scalar/tuple/array/option subset.
  - Tightened `orchestrate` into a strict top-level typed pipeline and removed silent Rust/Python/Node fallback into normal Kain function dispatch.
- Updated `crates/kain-core/tests/compiler_owned_intent_test.rs`
  - Added coverage for `law`, world-state leakage, sparse worlds, selector-less fast lanes, executable converge verification, strict orchestrate rejection cases, and runtime-label enforcement.
- Updated `crates/kain-driver/src/lib.rs` and `crates/kain-driver/src/native_app.rs`
  - Replaced native-ui-only world selection heuristics with target-aware world selection.
  - Native desktop targets resolve against `native_ui`, web targets against `web`, and UE5 targets against `ue5`.
  - Explicit `--root` world selection now fails if the selected world does not declare the required surface for the active adapter target.
- Updated `crates/cli/src/lsp.rs`
  - Added `law` to keyword/symbol surfacing and kept the full intent-suite declaration set visible to editor tooling.
- Updated `docs/kainplan/08_COMPILER_OWNED_INTENT_QUARTET.md` and `ARCHITECTURE.md`
  - Reframed the old quartet doc into the current intent-suite contract.
  - Recorded the new five-declaration doctrine and the target-aware world-selection behavior.

Validation completed:

- `cargo test -p kain-core --test compiler_owned_intent_test`
- `cargo test -p kain-driver --lib world`
- `cargo check -p cli`

Design decisions:

- Kept the declarations contextual rather than globally reserving `law`, `patch`, `converge`, `world`, and `orchestrate`.
- Chose to make `verify random(n)` honest and executable now instead of leaving it as metadata-only aspiration.
- Chose to make `orchestrate` stricter instead of trying to infer bundle metadata from arbitrary expression shapes.

Current risks:

- The parser still requires at least one `fast` lane in every `converge`, even when a `spec`-only form could be meaningful later.
- `verify random(n)` is intentionally bounded to a scalar-centric synthesis subset; richer structural generation is still future work.
- The full `cargo test -p kain-driver --lib` suite still contains unrelated package/network-heavy failures outside this feature lane, so the validated surface here is the focused world-selection/native-app path plus the full core intent suite.

Recommended next step:

- Decide whether to relax the “at least one fast lane” parser rule for `converge`, and if so, align that parser choice with bundle emission, runtime dispatch, and compiled/codegen lanes together.

## 2026-04-11 - LLVM now lowers world, patch, converge, and orchestrate directly enough for Omega to run without the wrapper path

The previous Omega note is no longer current. The LLVM backend in `crates/kain-sys-codegen` now materializes compiler-owned intent items directly instead of requiring `main()` to route around them.

What changed:

- Updated `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Registered `world` items as real LLVM struct types plus singleton globals with once-only init functions.
  - Added function registration and body emission for `patch`, `converge`, and `orchestrate` items so they lower as callable symbols.
  - Added identifier lowering for world handles so `let omega = Omega` and `let studio = Studio` materialize the singleton pointer instead of failing as undefined locals.
  - Added `Expr::StageCall` lowering and direct-call reuse so `kain/rust/python/node fn(...)` stage syntax compiles through the LLVM lane when the function resolves locally.
- Updated `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added a focused regression covering `world` singleton init, `patch` mutation, `converge` lowering, `orchestrate` lowering, and direct `main()` execution through that path.
- Updated `labs/omega/omega.kn`
  - Removed the fallback `omega_pipeline_runtime(...)` route from `main()`.
  - `main()` now directly touches `Omega`, runs patch calls, and returns `omega_pipeline(...)`.
- Updated `crates/ue5/src/codegen_ue5.rs` and `crates/kain-host/src/lib.rs`
  - Fixed adjacent exhaustiveness fallout from the newer `Law` surface so local validation builds could proceed farther instead of failing on unrelated missing match arms.

Validation completed:

- `cargo test -p kain-sys-codegen llvm_generates_world_patch_converge_and_orchestrate_paths -- --nocapture`
- `./runtime/compile_native_runtime.sh`
- generated current LLVM for `/home/ephemara/Dev/Kain/labs/omega/omega.kn` through a local Rust harness using `kain-core` + `kain-sys-codegen`
- linked `/home/ephemara/Dev/Kain/labs/omega/generated/omega.ll` against `generated/native_runtime/debug/*.o`
- `/home/ephemara/Dev/Kain/labs/omega/generated/omega` exits with code `145`
- generated current LLVM for `/home/ephemara/Dev/Kain/smoketest/compiler_owned_intent/compiler_owned_intent.kn` and confirmed direct `Studio` world init lowering is present in `generated/compiler_owned_intent.ll`

Design decisions:

- Kept LLVM stage-call lowering simple for now: local `StageCall` lowers as a direct function call in the native lane. That is enough to unblock authored `orchestrate` flows like Omega while preserving the existing typed stage syntax in source.
- Treated `converge` as a direct callable lowering of its spec lane in LLVM. This keeps compiled behavior deterministic and aligned with the semantic contract until the backend grows capability-aware lane selection or verification instrumentation.
- Validated the backend through current-library harnesses and direct LLVM/native linking because the repo's `cli` package currently has separate feature-gating issues unrelated to this LLVM change.

Current risks:

- LLVM `StageCall` is now useful for local lowering, but it is not yet a true native bridge implementation of Python/Node/Rust host execution semantics.
- `converge` lowering still takes the spec lane directly; it does not yet encode runtime lane selection or verification sampling in native codegen.
- The `cli` crate still has unrelated package/feature issues around optional UE5 surfaces, so `cargo build -p cli` is not the clean validation path for this work today.

Recommended next step:

- Decide whether native LLVM should keep `StageCall` as a direct local-call lowering or grow explicit bridge shims for Python/Node/Rust runtime dispatch, then implement the same decision for `converge` lane selection so the compiled lane matches interpreter semantics more closely.

## 2026-04-11 - omega lab now compiles and runs in both interpret and LLVM lanes, with a runtime-safe entrypoint around current compiler-owned-intent LLVM gaps

The modernized `labs/omega/omega.kn` now works as an actual runnable lab file instead of only an interpreter-only proof.

What changed:

- Updated `labs/omega/omega.kn`
  - Kept the modern Omega thesis in current Kain syntax: `world`, `component`, `patch`, `converge`, `orchestrate`, actor syntax, and regular structs/functions.
  - Added a plain `omega_pipeline_runtime(...)` path used by `main()` so the file can compile all the way through the current LLVM/native executable lane.
  - Kept the compiler-owned declarations in the file for runtime-contract / realtime-bundle emission and authored-surface proof, while avoiding the exact call paths that the LLVM backend does not lower yet.

Validation completed:

- `./target/debug/kain run /home/ephemara/Dev/Kain/labs/omega/omega.kn` -> `145`
- `./runtime/compile_native_runtime.sh`
- `./target/debug/kain build /home/ephemara/Dev/Kain/labs/omega/omega.kn --target rust --output /home/ephemara/Dev/Kain/labs/omega/generated/omega.rs`
- `./target/debug/kain build /home/ephemara/Dev/Kain/labs/omega/omega.kn --target llvm --output /home/ephemara/Dev/Kain/labs/omega/generated/omega.ll`
- `/home/ephemara/Dev/Kain/labs/omega/generated/omega` exits with code `145`

Design decisions:

- Treated the old file as a thesis to port, not as syntax to transliterate.
- Chose to preserve `world` / `patch` / `converge` / `orchestrate` in the authored file, but routed `main()` through plain functions for the compiled lane because the LLVM backend currently fails on direct `world` handle materialization and direct `orchestrate` call lowering.

Current risks:

- This is a real current backend gap, not an Omega-specific bug: the existing compiler-owned-intent smoke also fails LLVM codegen when `main()` does `let studio = Studio`, and direct `orchestrate` calls in the compiled lane can lower to undefined symbols during link.
- Omega therefore proves the mixed semantic surface and the native executable lane together, but not yet direct end-to-end execution of compiler-owned intent through LLVM.

Recommended next step:

- Fix LLVM/codegen support for world handle materialization and orchestrate entrypoint emission so authored `world` / `patch` / `converge` / `orchestrate` can execute directly in native compiled binaries without the runtime-safe wrapper path.

## 2026-04-11 - architecture doc now states plainly that Kain is an executable language, not only a manifest/orchestration layer

The durable repo overview had drifted toward the packaging/materialization story and was underselling what `crates/kain-core` already does directly.

What changed:

- Updated `ARCHITECTURE.md`
  - Reframed Kain as a compiled multi-target language toolchain, executable semantic runtime, and embeddable host stack.
  - Added an explicit semantic execution flow section describing `kain-core` as a real execution lane for functions, blocks, closures, control flow, async/await, actors, UI expression evaluation, and runtime execution of `patch` / `converge` / `orchestrate`.
  - Tightened the language around host bridges so future agents do not mistake external adapters for proof that Kain itself is only config glue.
  - Added guardrails stating that Kain-expressible logic should remain in Kain unless the capability is truly platform- or ecosystem-owned.

Design decisions:

- Anchored the architecture wording to actual `kain-core` behavior rather than repo-wide packaging ambitions, because future agents were at risk of reasoning from the wrong center of gravity.
- Kept the bundle/materialization story intact, but repositioned it as downstream consumption of compiler/runtime truth instead of the whole identity of the language.

Current risks:

- The repo still contains a large amount of packaging, adapter, and target-specific work, so future docs can drift back toward an adapter-first framing if they are written from `kain-driver` outward instead of from `kain-core` semantics outward.
- Some ambitious domains are still partly bridge-driven, so agents need to distinguish "Kain can execute real logic" from "every subsystem is already first-class native syntax."

Recommended next step:

- When future major language/runtime features land, record both the in-language execution surface and the emitted bundle/adapter surface together so the repo overview stays balanced.

## 2026-04-11 - kain-3d now has a first-class authored primitive pipeline with stable resource ids

The 3D runtime no longer treats primitives as a few isolated helper meshes spread across scene setup and host wrappers.

What changed:

- Added `crates/kain-3D/src/primitive.rs`
  - Introduced `PrimitiveShape`, `PrimitiveDefinition`, and `PrimitiveLibrary` as the crate-owned authored primitive seam.
  - Added high-fidelity procedural builders for plane, box, UV sphere, quad sphere, cylinder, cone, capsule, and torus.
  - Aligned the library with the existing DCC mesh contract using stable `mesh://primitives/authored/*` resource URIs and `mesh://primitives/authored/definitions` as the document root.
- Updated `crates/kain-3D/src/authoring.rs`
  - Routed `Geometry::plane`, `Geometry::box_mesh`, and `Geometry::uv_sphere` through the shared primitive builder path instead of keeping duplicate local mesh logic.
  - Added `Geometry::cylinder`, `Geometry::cone`, `Geometry::capsule`, `Geometry::torus`, and `Geometry::quad_sphere`.
  - Added `Scene::add_primitive_definition(...)` and `Scene::add_primitive_library(...)` so authoring scenes can register primitive libraries with durable metadata.
- Updated `crates/kain-3D/src/host.rs` and `crates/kain-3D/src/prelude.rs`
  - Exposed the richer primitive set to authored Kain source through new `zen3d` runtime bindings for `quad_sphere`, `cylinder`, `cone`, `capsule`, and `torus`.
- Updated `crates/kain-3D/src/scene.rs`
  - Switched builtin cube, plane, and UV sphere scene meshes to consume the shared primitive pipeline instead of hand-maintained duplicate mesh builders.
- Updated `ARCHITECTURE.md`
  - Recorded that the authored primitive seam now belongs to `kain-3D` under the viewport/runtime contract lane.

Validation completed:

- `cargo test -p kain-3d --lib -- --nocapture`
- `cargo test -p kain-ui-native viewport_ -- --nocapture`

Design decisions:

- Chose a crate-owned primitive definition layer instead of only adding more `Geometry::*` helpers so future DCC authoring, scene contracts, and app-level mesh documents can point at stable primitive ids and URIs.
- Kept the runtime-facing host API function-based for now because it gives authored `.kn` code immediate access to the new shapes without first widening Kain reflection with a variant-heavy primitive descriptor schema.
- Reused the shared primitive builders inside `scene.rs` so cube/plane/sphere quality and winding rules only live in one place.

Current risks:

- The primitive pipeline is still CPU-authored geometry; there is not yet a GPU tessellation / displacement / remesh lane behind these definitions.
- The Kain-side prelude exposes constructors, but it does not yet expose the full `PrimitiveDefinition` / `PrimitiveLibrary` metadata model directly to `.kn` code.
- Subdivision-ready means topology intent and support-loop density today, not Catmull-Clark or ZBrush-class remeshing guarantees by itself.

Recommended next step:

- Make primitive definitions first-class bundle/runtime resources so Zen and app-level DCC lanes can select, instantiate, and edit authored primitive ids directly instead of flattening them immediately into anonymous meshes.

## 2026-04-10 - manipulator math moved into kain-3d and the host prelude now uses real extern 3D bindings

The 3D lane no longer leaves core viewport edit math stranded inside `kain-ui-native`.

What changed:

- Updated `crates/kain-3D/src/interaction.rs` and `crates/kain-3D/src/lib.rs`
  - Added `ManipulatorSnapSettings` plus `apply_manipulator_drag(...)` as the reusable 3D interaction contract.
  - Centralized screen-drag translation/rotation/scale math in `kain-3D` instead of keeping it host-local.
  - Added constrained drag handling for screen, axis, and plane manipulator modes, plus local-vs-world basis resolution and snap application.
  - Added focused tests for screen drag movement, snap behavior, axis scale positivity, and local-axis translation under object rotation.
- Updated `crates/kain-ui-native/src/lib.rs`
  - Switched the viewport manipulation path to call `kain-3D`’s shared drag helper instead of its own private transform math.
  - Kept `kain-ui-native` in the host-forwarding role: it still owns input capture and viewport chrome, but the transform result is now computed by the 3D crate.
- Updated `crates/kain-3D/src/prelude.rs`
  - Replaced fake Kain-bodied `__zen3d_*` helper implementations with `@extern fn` declarations that match the native bindings installed by `Kain3dSession`.
  - This fixed the existing `kain-3d` host test failure where the generated prelude was constructing placeholder 3D values instead of binding to the Rust-native helpers cleanly.
- Updated `ARCHITECTURE.md`
  - Recorded that manipulator drag math is now a `kain-3D` responsibility under the viewport contract lane.

Validation completed:

- `cargo test -p kain-3d --lib -- --nocapture`
- `cargo test -p kain-ui-native viewport_ -- --nocapture`

Design decisions:

- Chose a shared drag API first instead of adding more ad hoc viewport behaviors in `kain-ui-native`, because the current weakness was ownership drift more than missing host chrome.
- Kept the first integration using `ManipulatorAxis::Screen` in the native UI lane so the host change stays low-risk while the 3D crate can already support axis/plane-constrained drags for the next pass.
- Used `@extern fn` in the 3D prelude because the helper functions are truly native host bindings; fake Kain return bodies were hiding that seam and were fragile under typechecking.

Current risks:

- `kain-ui-native` still defaults to screen-space drag activation; it is not yet choosing gizmo axes from handle hits, so the richer axis/plane drag support in `kain-3D` is only partially exercised by the current host.
- Rotation still lands in Euler component updates, which is acceptable for the current transform model but not a final professional-grade manipulator representation.

Recommended next step:

- Use the GPU/CPU picking paths to identify active gizmo handles and feed real `ManipulatorAxis` selections into `apply_manipulator_drag(...)`, so the native viewport graduates from screen-drag transforms to actual axis/plane gizmo interaction.

## 2026-04-10 - intent_forge_quartet adds a real native 3D executable smoke for the compiler-owned intent system

The repo now has a richer visual proof for the compiler-owned intent quartet under `smoketest/3D/intent_forge_quartet`.

What changed:

- Added `smoketest/3D/intent_forge_quartet/smoke.kn`
  - Authors a studio-style native shell with a central `viewport3d`, left tool rack, right inspector rail, and bottom activity strip.
  - Uses `world IntentForge`, two `patch` declarations, one `converge`, and one `orchestrate` in the same executable app.
  - Keeps a deterministic `main()` result of `105` so `kain run` and the packaged native-ui path can be checked together.
- Added `smoketest/3D/intent_forge_quartet/run_smoke.py`
  - Runs `kain run` on the authored file.
  - Builds the packaged native-ui executable.
  - Verifies generated runtime contract, realtime bundle, and native app bundle artifacts.
- Added `smoketest/3D/intent_forge_quartet/launch_native_app.sh`
  - Linux-friendly launcher for rebuilding and opening the packaged executable.

Design decisions:

- The smoke intentionally uses a richer editor-like shell rather than the earlier minimal quartet proof so the feature is exercised inside a UI shape that reads like an actual DCC/tool app.
- The smoke stays inside currently proven native-ui and viewport primitives instead of inventing a separate rendering lane just for the quartet demo.

Current risks:

- The scene still uses the current generic viewport scene lane, so this is primarily a packaging-and-shell proof rather than a custom geometry-runtime proof.
- `smoketest/3D/` is globally ignored in the repo, so these source files need explicit force-add handling until the broader ignore policy is cleaned up.

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
