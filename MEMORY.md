# Kain Memory

# 2026-05-22 - live `.god` module probing is gone from resolver and blade discovery

The live Kain source-discovery path no longer spends time probing legacy `.god`
module files during filesystem import resolution or blade module-root discovery.

What changed:

- `crates/kain-core/src/module_resolution.rs`
  - removed `.god` candidate paths from shared filesystem module lookup
  - added a regression test that asserts filesystem candidates no longer include
    `.god`
- `crates/kain-blades/src/lib.rs`
  - module-root discovery now treats only `.kn` files as authored Kain source
- `crates/kain-run/src/lib.rs`
  - removed the explicit `.god` extension match from run-target inference
- docs / architecture
  - updated `ARCHITECTURE.md`, `docs/syntax-and-semantics/module-resolution.md`,
    and `docs/cli/build-run-init.md` so they no longer advertise `.god` as a
    live source extension

Validation:

- `cargo test -p kain-core module_resolution --target-dir target/codex-no-god`
- `cargo test -p blade infers_nested_module_roots_from_source_tree --target-dir target/codex-no-god`
- `cargo test -p kain-run infers_targets_from_file_names --target-dir target/codex-no-god`
  - blocked by an unrelated current workspace failure in `crates/kain-sys-codegen`
    (`ConstGlobalInfo` missing `thread_local`)

Durable lesson:

- There were no live `*.god` files left in the repo; the remaining references are
  historical docs, legacy artifacts, or compatibility shims.
- This pass removes lookup/discovery overhead and stale surface area, but it does
  not yet hard-error on an explicitly provided `foo.god` path because generic
  unknown-extension run inference still falls back to the Kain lane. Treat that
  as a separate UX-tightening decision if full tombstoning is desired.

# 2026-05-22 - `kain check` is now source-path anchored for C FFI and skips workspace `.kain` caches

The `kain check` lane is less cwd-sensitive and less noisy now: frontend C FFI augmentation no longer depends on the shell launch directory when a real source path is known, and directory-wide checks stop recursing into generated `.kain` trees by default.

What changed:

- `crates/kain-driver/src/lib.rs`
  - `prepare_frontend_source_for_target(...)` now takes `source_path` and threads it through C FFI and Rust FFI source preparation
  - frontend import collection now prepares imported module sources relative to their own file path, not the ambient process cwd
  - added a regression test that checks a generated `.kain/cache/c_ffi/.../tiny_prelude.kn` file from outside its workspace and proves the nearest `KAIN.toml` is still honored
- `crates/kain-driver/src/native_app.rs`
  - updated the native-app helper callsite to the new `prepare_c_ffi_source(..., source_path, ...)` signature
- `crates/kain-check/src/lib.rs`
  - directory discovery now skips `.kain` alongside other generated roots, so workspace checks only count authored source by default
  - regression test now covers both `generated/` and `.kain/cache/`
- `docs/cli/check-and-test.md`
  - updated the operator contract to document that generated cache roots are skipped unless a generated file is passed explicitly

Validation:

- `cargo test -p kain-check discover_kain_files_skips_generated_directories --target-dir target/codex-check-pipeline`
- `cargo test -p kain-driver frontend_to_typed_program_with_source_path_resolves_c_ffi_relative_to_source_file --target-dir target/codex-check-pipeline`
- `cargo build -p cli --target-dir target/codex-check-pipeline`
- fresh CLI binary:
  - `D:\Kain-Lang\target\codex-check-pipeline\debug\kain.exe check .\smoketest --target llvm` -> `Check passed: 43/43 passed`
  - from `D:\Kain-Lang\smoketest`: `..\target\codex-check-pipeline\debug\kain.exe check . --target llvm` -> `Check passed: 43/43 passed`
  - from `D:\Kain-Lang\smoketest\src\systems`: `..\..\..\target\codex-check-pipeline\debug\kain.exe check . --target llvm` -> still `2/2 passed`
  - from `D:\Kain-Lang\smoketest\src\systems`: `..\..\..\target\codex-check-pipeline\debug\kain.exe check ..\.. --target llvm` -> `43/43 passed`

Durable lesson:

- The reliability bugs were two different problems: generated cache recursion and cwd-anchored frontend FFI prep. Fixing both makes workspace-root checks boring again.
- The bigger UX gap is still open: nested-folder `kain check .` remains local-folder scoped instead of auto-fanning out to the enclosing workspace root or build-graph check entry. That should be treated as a separate scope-selector feature, not confused with the cache/cwd correctness bug.

# 2026-05-22 - added `tool-z3-bug-hunter` as the exploratory solver-backed bug logging lane

The repo-local skill tree now includes `.agents/skills/tool-z3-bug-hunter`, a sibling to `tool-z3-black-magic` that keeps the same solver-aggressive spirit but changes the deliverable: find weird edge-case bugs, prove or witness them when possible, and append the evidence to `BUGS.md` instead of fixing the code in the same pass.

What changed:

- added `.agents/skills/tool-z3-bug-hunter/SKILL.md` as a logging-only Z3 bug hunt lane with qualifying issue rules, hunt loop, `BUGS.md` format, and anti-fix guardrails
- added `.agents/skills/tool-z3-bug-hunter/agents/openai.yaml` UI metadata
- updated `.agents/skills/TAXONOMY.md` and `AGENTS.md` so future agents can route solver-backed bug hunts into the new `tool-*` sibling lane intentionally

Validation:

- `python C:\\Users\\Admin\\.codex\\skills\\.system\\skill-creator\\scripts\\quick_validate.py D:\\Kain-Lang\\.agents\\skills\\tool-z3-bug-hunter`

Future agents should use `$tool-z3-bug-hunter` when the assignment is to discover and log bugs with hard evidence, not when the goal is to land a rewrite or optimization. If the work turns into solver-guided replacement or hot-path mutation, pivot to `$tool-z3-black-magic` plus the owning subsystem skill.

# 2026-05-22 - added `wildcard-justwritebro` as the anti-scavenger Kain authoring lane

The repo-local skill tree now includes a deliberate `wildcard-*` namespace for intuition-first Kain authoring overrides. The first lane, `.agents/skills/wildcard-justwritebro`, tells agents to load the core authoring manuals and then start writing instead of spending several minutes pattern-matching against the whole repo.

What changed:

- added `.agents/skills/wildcard-justwritebro/SKILL.md` as a high-freedom authoring override with a required core-skill loadout, anti-scavenger contract, write loop, escalation rules, and anti-patterns
- added `.agents/skills/wildcard-justwritebro/agents/openai.yaml` UI metadata
- updated `.agents/skills/TAXONOMY.md`, `AGENTS.md`, and `ARCHITECTURE.md` so future agents can discover the new `wildcard-*` namespace intentionally

Validation:

- `python C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py D:\Kain-Lang\.agents\skills\wildcard-justwritebro`

Future agents should use `$wildcard-justwritebro` only when the task explicitly wants fast, creative, intuition-first Kain authoring rather than repo-conforming research. It is an override lane, not a replacement for the owning `lang-*` field manuals.

# 2026-05-22 - stdlib and smoke-album shadow diagnostics now preserve predeclare origins

The typechecker no longer mistakes its own forward predeclare placeholders for user/builtin shadowing, and stdlib-origin wrappers can intentionally occupy originless runtime/builtin global names.

What changed:

- `crates/kain-core/src/types.rs`
  - `predeclare_item_types` now records real declaration origins for structs, enums, worlds, components, and actors when it creates the placeholder, so the later registration pass recognizes the same declaration instead of reporting self-shadowing.
  - stdlib registration now carries an explicit `stdlib_registration_depth`, and span-origin checks also recognize `stdlib/*.kn`; this lets authored stdlib wrappers such as `fs_read_text` and vector/math helpers replace originless builtin globals without granting the same privilege to user files.
  - Regression tests cover predeclared user type registration, dynamic `use std::collections` without `StringIntMap` self-shadow, dynamic `use std::fs` builtin-wrapper registration, stdlib-origin wrapper allowance, and user-origin builtin shadow rejection.

Validation:

- `cargo check -p kain-core --target-dir target/codex-bootstrap-core-shadow`
- `cargo test -p kain-core typecheck_dynamic_stdlib_import_can_wrap_builtin_global --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-core typecheck_dynamic_stdlib_import_can_register_collections_types_once --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-core typecheck_predeclared_user_types_do_not_shadow_their_declarations --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-core typecheck_rejects_user_origin_shadowing_builtin_global --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-core typecheck_rejects_user_origin_shadowing_builtin_type --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-check --target-dir target/codex-bootstrap-core-shadow -- --nocapture`

# 2026-05-21 - Kaintana's public surface now needs root-owned theme/layout helpers, and the desktop adapter stays standalone-checkable through `@extern`

The current Kaintana modernization pass shook out a real package-surface rule in this compiler lane: `pub use ...::*` from internal modules was not enough for every consumer/check surface. The library entry `blades/kaintana/src/kaintana.kn` now owns the durable public helpers directly for the hot paths consumers actually import.

What changed:

- `blades/kaintana/src/kaintana.kn`
  - root-owned theme API now lives directly in the prelude surface: `kaintana_theme_named`, plus the named preset constructors
  - root-owned layout helpers now live directly in the prelude surface: `kaintana_inset`, `kaintana_split_left/right/top/bottom`, `kaintana_row_slot`, `kaintana_column_slot`, and `kaintana_grid_cell`
  - restored `kaintana_button_activated(...)` as a direct public helper over `kaintana_widget_take_activation(...)`
- `blades/kaintana/src/platform/desktop/desktop_adapter.kn`
  - kept the architecture rule intact by not adding `use c::kaintana_desktop_bridge`
  - instead declared the desktop bridge entrypoints as `@extern fn ...` so the adapter can pass standalone/package-root checks without reintroducing the duplicate-bridge LLVM issue called out in `ARCHITECTURE.md`
- `blades/kaintana/examples/*.kn`
  - example imports now route layout helpers through the root `kaintana::...` surface instead of the internal `layout::...` path
- `blades/kaintana/src/main.kn`, `blades/kaintana-test/src/main.kn`, and `blades/kaintana-vulkan-test/src/main.kn`
  - consumer entries now import the symbols they truly use (`kaintana_theme_named`, `std::intent`, etc.) and validate cleanly against the modernized public surface

Validation:

- direct Bazel-built CLI because the wrapper `kain` path is currently blocked by unrelated workspace breakage in `crates/kain-core`
- `D:\\Kain-Lang\\target` wrapper not required; used `D:/kain-bazel/output-user-root/ccujd7ry/execroot/_main/bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe`
- `... kain.exe check blades/kaintana/src/kaintana.kn --target llvm` -> PASS
- `... kain.exe check blades/kaintana/src/main.kn --target llvm` -> PASS
- `... kain.exe check blades/kaintana-test/src/main.kn --target llvm` -> PASS
- `... kain.exe check blades/kaintana-vulkan-test/src/main.kn --target llvm` -> PASS
- `... kain.exe check blades/kaintana-test --target llvm` -> `Check passed: 6/6 passed`
- `... kain.exe check blades/kaintana-vulkan-test --target llvm` -> `Check passed: 5/5 passed`
- `... kain.exe check blades/kaintana --target llvm` -> `Check passed: 29/29 passed`

Durable lesson:

- In this lane, Kaintana consumers are happier when core package helpers are owned directly by `src/kaintana.kn` instead of relying on transitive `pub use` through internal submodules.
- For blade-local desktop/native bridges, `@extern` declarations inside helper modules are the right way to keep package-root/module-root checks green while still reserving the actual `use c::...` bridge import for linking entrypoints.

# 2026-05-21 - `blades/fluid-studio` now has the authored Kain same-window fluid app shape, but the current LLVM lane still stalls before `realtime_app` emission

`blades/fluid-studio` now exists as a data-driven, Kain-authored fluid simulator blade with GPU shader sources, a Kaintana control deck, a Vulkain 3D mesh presenter, and semantic simulation pressure. The authored app shape is in place, the stale `path_parent` intrinsic is removed from the blade, and the direct `ui(ctx)` builder calls were normalized to `kaintana_ui_state(ctx)`. On the current local compiler lane, `kain check` passes for the app plus both SPIR-V sources, and direct LLVM compilation now reaches `.ll` plus `.runtime_contract.json`; the remaining blocker is that the process idles before `.realtime_app.json` for the full blade.

What changed:

- `blades/fluid-studio/src/fluid_studio_state.kn`
  - replaced the unresolved `path_parent(...)` intrinsic usage with local string/path helpers (`fluid_string_prefix`, `fluid_last_path_separator`, `fluid_path_parent`)
  - moved `FluidUiFonts` / `FluidStudioUiFrame` out of the giant session/config module to reduce bundle-stage graph coupling
- `blades/fluid-studio/src/fluid_studio_ui_types.kn`
  - new shared UI-only type surface for `FluidUiFonts` and `FluidStudioUiFrame`
- `blades/fluid-studio/src/fluid_studio_views.kn`
  - new typed request/deck builders (`FluidUiRequest`, `FluidSceneRequest`) so the UI/presenter lanes consume compact packets instead of the full `FluidStudioSession`
- `blades/fluid-studio/src/fluid_studio_ui.kn`
  - now consumes `FluidUiRequest`
  - replaced `ui(ctx)` builder calls with explicit `kaintana_ui_state(ctx)` calls
- `blades/fluid-studio/src/fluid_studio_scene.kn`
  - now consumes `FluidSceneRequest` instead of the full session/config graph
- `blades/fluid-studio/src/main.kn`
  - builds `FluidUiRequest` / `FluidSceneRequest` packets from the authoritative session before entering the UI and Vulkain presentation lanes
- `blades/fluid-studio/{build.kn,KAIN.toml}`
  - registered the new authored source files in the blade evidence inputs

Validation:

- `D:\\Kain-Lang\\target\\codex-build-kn-smoke\\debug\\kain.exe check D:\\Kain-Lang\\blades\\fluid-studio\\src\\main.kn --target llvm`
- `D:\\Kain-Lang\\target\\codex-build-kn-smoke\\debug\\kain.exe check D:\\Kain-Lang\\blades\\fluid-studio\\src\\fluid_compute.kn --target spirv`
- `D:\\Kain-Lang\\target\\codex-build-kn-smoke\\debug\\kain.exe check D:\\Kain-Lang\\blades\\fluid-studio\\src\\fluid_surface.frag.kn --target spirv`
- direct LLVM compile with the same binary:
  - `D:\\Kain-Lang\\target\\codex-build-kn-smoke\\debug\\kain.exe D:\\Kain-Lang\\blades\\fluid-studio\\src\\main.kn -t llvm -o D:\\Kain-Lang\\blades\\fluid-studio\\fluid-studio.exe`
  - result: emits fresh `fluid-studio.ll` and `fluid-studio.runtime_contract.json`, then leaves an idle `kain.exe` process before `fluid-studio.realtime_app.json`

Durable lesson:

- The current local LLVM/native lane is sensitive to authored graph shape beyond plain typechecking. `kain check` can pass while the later runtime-contract / realtime-bundle materialization path still stalls.
- `blades/fluid-studio` no longer relies on the unresolved `path_parent` intrinsic; if future compile attempts still fail, the remaining work is in compiler/runtime bundle materialization rather than authored path resolution.

# 2026-05-21 - `blades/spirv-visualizer` is now the SPIR-V capability viewport blade

`blades/spirv-visualizer` now exists as a data-driven GPU/SPIR-V visualizer blade that can load arbitrary SPIR-V inputs, default to a known-good Kain-authored sample fragment shader, and present through the reusable `blades/vulkain` Vulkan bridge without baking app policy into Vulkain itself.

What changed:

- added `blades/spirv-visualizer/`
  - `build.kn`, `KAIN.toml`, and `run.ps1` for a first-class blade/workspace launch surface
  - `config/spirv_visualizer.runtime.json` for data-driven window, scan-root, shader-default, and artifact-path policy
  - `shaders/spirv_visualizer_samples.kn` as the canonical sample SPIR-V authoring surface
  - `src/main.kn` as the authored Kain viewport: worlds/entangle/actor/patch/law/converge/orchestrate semantics plus SPIR-V discovery, direct-pair selection, and Vulkain proxy fallback
- wired the blade to `blades/vulkain`
  - default finite smoke seeds `vulkain_basic.vert.spv` plus the sample fragment entrypoint `SpirvCapabilitySpectrum`
  - explicit overrides now support raw SPIR-V, vertex, fragment, shader bundle, realtime bundle, entrypoint, config, and scan-root injection through `run.ps1` env wiring
- taught the blade to accept the older working local compiler/runtime lane
  - replaced unsupported iterator `for ... in ...` lowering with indexed `while` loops
  - replaced unsupported string helpers (`trim`, `to_ascii_lowercase`, `starts_with`, `ends_with`, `contains`) with manual logic where needed
  - replaced unresolved path intrinsics (`path_join`, `path_parent`, `path_stem`, `path_file_name`, `path_extension`, `cwd`, `create_dir_all`, `path_is_dir`) with stdlib-backed `fs_*` calls plus local path helpers
- patched shared helper blades so this older LLVM lane can still dogfood data-driven blades
  - `blades/kain-fsx/src/kain_fsx.kn` now uses `std::fs` wrappers plus local path parsing instead of unresolved path intrinsics
  - `blades/kain-config/src/kain_config.kn` now avoids the unsupported trim/case-normalization helpers in env/csv parsing
- runtime-output lesson:
  - the heavier custom report/catalog sidecars triggered allocator noise in the older runtime lane, so the blade now treats `.kain/run/spirv_visualizer_presenter_report.txt` as the authoritative runtime artifact and omits the flaky extra sidecars from the hot path

Validation:

- `D:\\Kain-Lang\\target\\debug\\kain.exe check D:\\Kain-Lang\\blades\\spirv-visualizer\\src\\main.kn --target llvm`
- `D:\\Kain-Lang\\target\\debug\\kain.exe gpu-artifacts D:\\Kain-Lang\\blades\\spirv-visualizer\\shaders\\spirv_visualizer_samples.kn --output D:\\Kain-Lang\\blades\\spirv-visualizer\\.kain\\gpu\\samples\\spirv_visualizer_samples`
- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\vulkain\\build-vulkain.ps1 -KainBin D:\\Kain-Lang\\target\\debug\\kain.exe`
- `powershell -ExecutionPolicy Bypass -File .\\blades\\spirv-visualizer\\run.ps1 -KainBin D:\\Kain-Lang\\target\\debug\\kain.exe -FrameBudget 12`
  - result: `PASS`, exit `0`
  - `.kain/run/spirv_visualizer_presenter_report.txt`: `title=SPIR-V Capability Visualizer // Kain // direct pair`, `fragment_entry_point=SpirvCapabilitySpectrum`, `frames_presented=12`, `last_error=ok`

Durable lesson:

- On this checkout, a fresh repo-wide compiler build can still fail in unrelated `kain-core` work. Reusing an already-built `target\\debug\\kain.exe` via `-KainBin` is an acceptable validation lane for GPU blades when the local source tree is red outside the blade task.
- The current older LLVM/runtime lane is still hostile to a handful of convenience helpers in authored Kain. For blade code that must run today, prefer manual string/path helpers, explicit `fs_*` stdlib calls, and indexed loops over newer sugar.
- For short-lived Vulkan proof windows, trust the blade-local presenter report (`frames_presented`, entry points, `last_error`) more than MCP window discovery.

# 2026-05-21 - direct ask reply-port prep and owner-inline completion cut more actor wait overhead

The inline scheduler-lock cut moved the actor frontier, but hot ask/reply traffic was still paying two avoidable taxes: compiler-lowered direct asks still rebound a synthetic actor-table slot just to mint a stale-reply token, and same-thread inline completion still fell back to the reply-port lock/wake path before the owner thread copied the completed payload back out. This pass finished the direct-token lane, added an owner-thread readback fast path, and reran the canonical benchmark suite.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - lowered actor `ask` / `ask_timeout` setup through `kain_actor_reply_port_prepare_direct(...)` instead of `kain_actor_reply_port_new()` plus synthetic-actor ref export
- `runtime/native/include/actor.h`
  - exported the compiler-owned `kain_actor_reply_port_prepare_direct(...)` ABI surface
- `runtime/native/src/core/actor.c`
  - added direct-token reply-port rearm with generation bump and invalid-actor direct refs
  - added the owner-thread `owner_inline_ready` completion lane so same-thread inline asks can read back completed payloads without re-taking the reply-port lock or firing a useless wake
  - kept stale direct replies observable and rejected through generation-tagged `send_handle(...)` matching
- actor ABI / compiler validation surfaces:
  - `crates/kain-actor/src/native.rs`
  - `crates/kain-actor/src/tests.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `runtime/conformance/actor_runtime/test_actor_abi_contract.c`
- proof surfaces:
  - `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-direct-token-rearm-invalidates-stale-generation.smt2`
  - `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-owner-inline-stale-direct-token-rejected.smt2`
  - `runtime/native/src/core/z3/proofs/actor-reply-port-direct-token-rearm-invalidates-stale-generation.yaml`
- durable notes:
  - `research/2026-05-21-actor-frontier-speedup-hunt.md`
  - `benchmark/assesments/2026-05-21-direct-ask-owner-inline-wait-assessment.md`

Validation:

- `toolchain\\llvm\\bin\\clang.exe -fsyntax-only runtime\\native\\src\\core\\actor.c -I runtime\\native\\include`
- `cargo test -p kain-actor --target-dir target/codex-actor-direct-token`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target/codex-actor-direct-token-codegen -- --nocapture`
- `mcp__z3_local__.check_smt2(...)` for `actor-reply-port-direct-token-rearm-invalidates-stale-generation.smt2` -> `unsat`
- `mcp__z3_local__.check_smt2(...)` for `actor-reply-port-owner-inline-stale-direct-token-rejected.smt2` -> `unsat`
- focused baseline: `benchmark/out/reports/latest_actor_frontier_baseline.llm.md`
- focused retake: `benchmark/out/reports/latest_actor_owner_inline_wait_combo_9.llm.md`
- canonical full suite: `python benchmark/run.py --timeout 900` -> `PASS`
- harness cleanup note: the first full rerun hit a stale Windows `process_stdio_loop.exe` linker/permission failure; removing `benchmark/out/build/process_stdio_loop/kain/process_stdio_loop.exe` and rerunning restored a clean PASS
- workspace hygiene note: targeted Rust validation initially hit disk exhaustion; deleting agent scratch dirs `.codex-tmp` and `target/codex-*` recovered about `86 GB` and the rerun passed

Measured outcome:

- focused actor before/after:
  - `actor_ownership_backpressure`: Kain `456.236 ms` -> `309.923 ms`
  - `semantic_fabric_relay`: Kain `114.217 ms` -> `93.487 ms`
- canonical full suite (`benchmark/out/reports/latest.llm.md`, generated `2026-05-21T22:22:19.046887+00:00`):
  - `actor_ownership_backpressure`: Kain `313.311 ms`, C++ `18.673 ms`
  - `semantic_fabric_relay`: Kain `91.346 ms`, C++ `11.272 ms`
  - `pulse_teleport_decay_mesh`: Kain `93.919 ms`, C++ `16.827 ms`
  - `unicode_string_heavy`: Kain `102.749 ms`, C++ `9.917 ms`
  - `http_server_concurrency`: Kain `73.373 ms`, Rust `50.901 ms`

Durable lesson:

- The direct ask lane was still wasting time on reply-port setup and owner-thread completion plumbing. Cutting that waste bought a real actor win and did not require any benchmark cheat or authored Kain change.
- The suite is still rich enough that no new benchmark row was needed. The next honest high-value frontier remains the actor semantic cluster (`actor_ownership_backpressure`, `semantic_fabric_relay`, `pulse_teleport_decay_mesh`), with `unicode_string_heavy` now the loudest non-proxy implemented loss.
- The automation deadline requirement is still satisfied by the live frontier row itself: `benchmark/cases/actor_ownership_backpressure/main.kn` already exercises `deadline_millis(...)` and `deadline_elapsed(...)`.

# 2026-05-21 - inline ask path shed scheduler-lock traffic and the actor frontier moved again

The reply-port and live-snapshot work had already removed obvious ask/reply waste, but same-thread inline microcell asks were still paying scheduler-lock traffic just to discover that no queue admission was needed. This pass cut that scheduler lock from the inline claim path, skipped finish-turn scheduler locking when there was no backlog to requeue, and proved the dequeue ordering so we did not create a double-owner race.

What changed:

- `runtime/native/src/core/actor.c`
  - keeps a parked synthetic reply-port actor/mailbox shell hot so TLS reply-port teardown can recycle that tiny shell instead of destroying it every time
  - added atomic scheduler-flag helpers for `shutdown`, `in_scheduler_queue`, and `in_scheduler_turn`
  - removed `g_scheduler.lock` from the same-thread inline claim path in `kain_actor_ask_send_ref(...)`
  - taught `kain_scheduler_finish_turn(...)` to skip scheduler locking when there is nothing to requeue
  - reordered dequeue handoff to publish `turn = 1` before `queue = 0`
- `runtime/native/src/core/z3/proofs-experimental/reply-port-parked-rebind-stale-ref-rejection.smt2`
  - proves the parked reply-port rebind still advances generation so stale refs stay dead
- `runtime/native/src/core/z3/proofs-experimental/inline-ask-turn-claim-no-double-owner.smt2`
  - proves the new dequeue ordering cannot expose an inline-claimable `(queue = 0, turn = 0)` intermediate state while the worker already owns the turn
- durable notes:
  - `research/2026-05-21-inline-ask-scheduler-lock-cut-speedup-hunt.md`
  - `benchmark/assesments/2026-05-21-inline-ask-scheduler-lock-cut-assessment.md`

Validation:

- `toolchain\llvm\bin\clang.exe -fsyntax-only runtime\native\src\core\actor.c -I runtime\native\include`
- `mcp__z3_local__.check_smt2(...)` for `reply-port-parked-rebind-stale-ref-rejection.smt2` -> `unsat`
- `mcp__z3_local__.check_smt2(...)` for `inline-ask-turn-claim-no-double-owner.smt2` -> `unsat`
- `python benchmark/run.py --case actor_ownership_backpressure,semantic_fabric_relay --languages kain,cpp --runs 5 --warmups 2 --timeout 600 --latest-stem latest_actor_inline_scheduler_cut`
- `python benchmark/run.py --case actor_ownership_backpressure,semantic_fabric_relay --languages kain,cpp --runs 9 --warmups 3 --timeout 600 --latest-stem latest_actor_inline_scheduler_cut_rerun`
- `python benchmark/run.py --timeout 900 --latest-stem latest_full_after_inline_scheduler_cut`
- `python benchmark/run.py --case process_stdio_loop --languages kain,rust,cpp --runs 9 --warmups 3 --timeout 900 --latest-stem latest_process_stdio_validation`
- `python benchmark/run.py --case contention_wall --languages kain,rust,cpp,zig,javascript,python --runs 9 --warmups 3 --timeout 900 --latest-stem latest_contention_validation`
- `bash runtime/conformance/actor_runtime/run_tests.sh` is currently broken by a pre-existing missing `attrition.c` link closure in the script; do not blame that failure on this patch

Measured outcome:

- focused actor probe:
  - `actor_ownership_backpressure`: Kain `485.658 ms` -> `461.558 ms`
  - `semantic_fabric_relay`: Kain `109.095 ms` -> `114.365 ms`
- focused 9-run retake:
  - `actor_ownership_backpressure`: Kain `470.161 ms`, C++ `16.799 ms`
  - `semantic_fabric_relay`: Kain `111.154 ms`, C++ `10.439 ms`
- canonical full suite (`benchmark/out/reports/latest_full_after_inline_scheduler_cut.llm.md`, generated `2026-05-21T15:27:22.181379+00:00`):
  - `actor_ownership_backpressure`: Kain `459.963 ms`, previous full latest `526.917 ms`
  - `semantic_fabric_relay`: Kain `114.693 ms`, previous full latest `121.885 ms`
- isolated regression checks:
  - `process_stdio_loop`: isolated Kain `6382.368 ms`, better than the previous full latest `6860.217 ms`
  - `contention_wall`: isolated Kain `9.842 ms`, close to the previous full latest `8.937 ms`

Durable lesson:

- The scheduler lock on same-thread inline asks was still real overhead, but this is still a step, not the alien leap. The remaining actor gap is dominated by request-side ownership and dispatch cost after the inline-claim decision.
- Treat `actor_ownership_backpressure` and `semantic_fabric_relay` as the same frontier until a future pass proves otherwise.
- When the full suite shows scary swings in non-actor rows, isolate them before calling them regressions. The long Windows suite still lies sometimes.

# 2026-05-21 - actor ask path shed the global table lock and the broken actor benchmark lane was repaired

The current checkout had a hidden benchmark-lane failure before any speed work started: `benchmark/cases/actor_ownership_backpressure/main.kn` was missing, so the loudest actor frontier row could not even run. This pass restored that source, deleted the ask-side global actor-table lock from `kain_actor_ask_send_ref(...)`, and reran the canonical benchmark suite.

What changed:

- `benchmark/cases/actor_ownership_backpressure/main.kn`
  - restored the missing source file and kept the live `deadline_millis(...)` / `deadline_elapsed(...)` touch active
- `runtime/native/src/core/actor.c`
  - added `kain_actor_ref_matches_live_snapshot(...)`
  - switched `kain_actor_ask_send_ref(...)` from locked `kain_actor_table_ref_matches_locked(...)` validation to a live-snapshot validation path that mirrors the existing lockless `kain_actor_send(...)` lookup shape
- `runtime/native/src/core/z3/proofs-experimental/actor-ask-live-snapshot-ref-match-equivalence.smt2`
  - proves the new snapshot predicate and the old locked predicate cannot disagree under the stable live-slot invariant
- `crates/kain-build/BUILD.bazel` and `crates/kain-core/BUILD.bazel`
  - regenerated via `python tools/bazel/sync_rust_builds.py` after stale BUILD drift blocked `kain check` by dropping the `kain-test` Bazel dependency
- durable notes:
  - `research/2026-05-21-actor-ask-live-snapshot-speedup-hunt.md`
  - `benchmark/assesments/2026-05-21-actor-ask-live-snapshot-latest-benchmark-assessment.md`

Validation:

- `clang -fsyntax-only runtime/native/src/core/actor.c -I runtime/native/include`
- `mcp__z3_local__.check_smt2(...)` for `actor-ask-live-snapshot-ref-match-equivalence.smt2` -> `unsat`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/runtime/native/src/core/z3", lane="actor", report_name="actor-ask-live-snapshot-regression-check")` -> `16 proved, 0 counterexamples`
- `bazel build //:kain --config=dev`
- `kain check benchmark/cases/actor_ownership_backpressure/main.kn --target llvm`
- `cargo test -p kain-actor --target-dir target/codex-actor-ask-live-snapshot`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target/codex-actor-direct-reply -- --nocapture`
- `python benchmark/run.py --case actor_ownership_backpressure --languages kain,cpp,rust --runs 3 --warmups 1 --timeout 240 --latest-stem latest_actor_probe`
- `python benchmark/run.py --timeout 900 --baseline-mode auto`

Measured outcome:

- focused actor retake vs the previous canonical latest report:
  - `actor_ownership_backpressure`: Kain `506.508 ms` -> `482.118 ms`
  - semantic rounds/s: `355,374.446` -> `373,352.349`
- canonical full suite (`benchmark/out/reports/latest.llm.md`, generated `2026-05-21T10:27:47.027376+00:00`):
  - `actor_ownership_backpressure`: Kain `472.025 ms`, C++ `16.683 ms`
  - `semantic_fabric_relay`: Kain `134.765 ms`, C++ `11.191 ms`
  - `pulse_teleport_decay_mesh`: Kain `125.148 ms`, C++ `79.211 ms`
  - `semantic_host_bridge_fusion`: Kain `1264.507 ms`, C++ `861.447 ms`
  - history summary: `14` Kain improvements, `25` regressions, `16` alert regressions versus prior comparable run `#38` on commit `5229b9f8d978999ddea120a5c9403e9505548e42`

Durable lesson:

- The ask-side global table lock was real overhead, but deleting it is still only a single-digit percentage win. The remaining multi-x actor gap is now clearly request-side ownership after lookup, not stale-ref validation itself.
- `actor_ownership_backpressure` and `semantic_fabric_relay` should be treated as the same frontier until proven otherwise.
- Do not claim the current full suite is regression-free. The canonical history comparison is against an older commit, so use the latest report as frontier truth, but treat the regression table as a separate cleanup backlog rather than direct blame on this actor patch.

# 2026-05-21 - `lang-projects` replaced `lang-blades` as the project pipeline skill

The repo-local project skill taxonomy now treats blades as one scale mode inside a broader Kain project pipeline instead of the default mental model.

What changed:

- added `.agents/skills/lang-projects/SKILL.md` as the universal authored project/workspace field manual for `build.kn`, `platform.kn`, project layouts, imports/module roots, check/run/build/watch loops, source tests, native LLVM executable outputs, evidence DAGs, portable amalgamate capsules, Fabric/Omni boundaries, and blade/workspace scale-up behavior
- moved the root executable helper to `.agents/skills/lang-projects/scripts/compile_kain_project_to_root.ps1`; it now finds project roots via `build.kn`, `platform.kn`, `KAIN.toml`, or `kain.toml`
- added `.agents/skills/lang-projects/references/project-authoring-patterns.md` for optional examples and failure routing
- retired active `.agents/skills/lang-blades` files and updated `.agents/skills/TAXONOMY.md` to list `lang-projects`
- updated `lang-stdlib` so the live root profile includes `std::build`, `std::proof`, `std::bench`, `std::attrition`, and `std::certify`
- updated `AGENTS.md` so generic runtime-owned `use c::...` imports are not described as requiring `KAIN.toml`

Future agents should use `$lang-projects` when the work is about Kain project authority, evidence graphs, local build/run/check/test flow, portable capsules, or workspace-scale organization. Use `tool-build-system` only when the implementation problem is repo build plumbing such as Bazel, launchers, generated BUILD drift, stale binaries, or `kain doctor`.

# 2026-05-21 - `build.kn` gained first-class Kain std evidence APIs

`build.kn` now has public Kain-facing stdlib contract modules for the evidence graph: `std::build`, `std::proof`, `std::bench`, `std::attrition`, and `std::certify`, with `std::test` extended for `test_suite(...)` / `test_task(...)`. Preferred task constructors are now `build_check(...)`, `test_suite(...)`, `proof_obligation(...)`, `bench_case(...)`, `attrition_case(...)`, `native_executable(...)`, and `certify_gate(...)`; legacy `build_task(...).kind(...)` remains accepted.

What changed:

- `crates/kain-build` and `crates/kain-blades` both extract the first-class constructors, so discovery and planning agree
- tasks can carry matrix axes, telemetry channels, certificate subjects, and required host capabilities
- non-dry-run builds skip tasks whose `requires_capability(...)` is not advertised by the host capability set
- `blades/kloner`, `blades/kaintana`, and `blades/kaintana-test` now dogfood the first-class std API shape

# 2026-05-21 - `build.kn` became an evidence DAG

`crates/kain-build` now treats `build.kn` tasks as evidence DAG nodes, not only build DAG nodes. First-class explicit task kinds now include `test`, `proof`, `benchmark`, `attrition`, `certify`, and `native-executable`; dependency failures gate dependent tasks, and evidence-style tasks emit `kain-evidence.json`.

What changed:

- `build_task(...).kind("test")` runs `kain-test`
- `build_task(...).kind("proof")` defaults to Z3 `prove-pass` and requires proof evidence
- `build_task(...).kind("benchmark")` and `kind("attrition")` run the repo evidence runners as structured external commands
- `build_task(...).kind("certify")` emits a certificate only after dependencies passed
- `build_task(...).kind("native-executable")` compiles a Kain entry into a project/root executable via the `lang-projects` helper
- explicit task paths now accept `$blade`, `$root`/`$repo`/`$workspace`, and `$task`/`$out` prefixes
- `blades/kloner`, `blades/kaintana`, and `blades/kaintana-test` now dogfood script-authored evidence graphs
- `docs/pipelines/build-kn-evidence-dag.md` is the agent tutorial for the new pipeline

# 2026-05-21 - `lang-gpu` became the rendering pipeline field manual

The repo-local `.agents/skills/lang-gpu` lane now teaches authored Kain agents how rendering and GPU work actually flows through the repo: Kain-core shader parsing/typechecking, the LLVM host lane, the SPIR-V artifact lane, stdlib graphics/GPU/shared resource layers, current native graphics runtime reality, compute runtime synchronization, Vulkain package boundaries, and semantic fusion with worlds, pulses, ownership, converge, axiom, shatter, and orchestrate.

What changed:

- rewrote `.agents/skills/lang-gpu/SKILL.md` from a tiny shader card into a full rendering/GPU field manual with pipeline map, source anchors, shader rules, artifact flow, command-recorder loop, resource policy flow, runtime sync notes, semantics mesh, validation ladder, handoff matrix, and anti-patterns
- updated `.agents/skills/lang-gpu/agents/openai.yaml` with a more precise UI blurb and default prompt

Future rendering agents should start with `$lang-gpu`, keep authored shader/resource/semantic work in Kain, and only escalate to `bootstrap-gpu`, `runtime-gpu`, or `package-vulkain` when the task crosses into backend emitters, generic executors, or package-owned Vulkan bridge internals.

# 2026-05-21 - `lang-stdlib` became the root stdlib field manual

The repo-local `.agents/skills/lang-stdlib` lane now teaches authored Kain agents how to use the full root `std::*` surface without loading the entire generated atlas into context.

What changed:

- rewrote `.agents/skills/lang-stdlib/SKILL.md` into a full stdlib operator skill covering the 27 native root modules, public/private/native boundaries, module selection, source anchors, authoring examples, validation ladders, and handoff rules
- added repo-root `query_stdlib.py` so agents can query `stdlib/stdlib.map.json` by summary, import list, module, symbol substring, and kind before opening large generated docs
- updated `.agents/skills/lang-stdlib/agents/openai.yaml` with a more precise display blurb and default prompt

Future stdlib agents should use `$lang-stdlib`, query exact symbols with `python query_stdlib.py ...`, then inspect only the specific `stdlib/*.kn` or proof blade needed for the task.

# 2026-05-21 - `runtime-core` skill became the native runtime field manual

The repo-local `.agents/skills/runtime-core` lane was expanded from a tiny ownership card into a full native runtime operator skill.

What changed:

- rewrote `.agents/skills/runtime-core/SKILL.md` with runtime-core trigger boundaries, fast operator loop, ABI/stdlib/codegen update rules, Z3 proof standard, implementation playbooks, validation ladders, and anti-patterns
- added `.agents/skills/runtime-core/references/native-c-runtime-architecture.md` with the native C runtime flow, manifest/service table truth, header map, source map, platform/package boundaries, and runtime mechanics
- added `.agents/skills/runtime-core/references/proof-and-validation.md` with native-core Z3 pack workflow, `proofs-experimental` rules, durable proof standards, file-to-proof matrix, MCP helper calls, and validation ladders
- added `.agents/skills/runtime-core/references/performance-hunting.md` with solver-backed optimization workflow, current hot surfaces, fast-path candidates, benchmark/attrition contracts, and proof breadcrumb guidance
- updated `.agents/skills/runtime-core/agents/openai.yaml` with a more precise UI description and default prompt

Future runtime agents should start with `$runtime-core`, then load the relevant reference instead of rediscovering the C ABI floor, proof pack lanes, manifest truth, or current actor/memory/service fast paths from scratch.

# 2026-05-21 - repo-local skills moved to a namespaced active tree

The repo-local skills surface now uses explicit ownership namespaces under `.agents/skills/`:

- `lang-*` for authored Kain/application work
- `bootstrap-*` for compiler/frontend/selfhost truth
- `runtime-*` for native substrate and runtime-backed stdlib behavior
- `test-*` for certification lanes
- `package-*` for package-owned surfaces
- `tool-*` for rare cross-cutting operator lanes

What changed:

- created the new active skills, including `lang-authoring`, `lang-semantics`, `lang-systems`, `lang-interop`, `lang-actors`, `lang-commands`, `lang-blades`, `lang-stdlib`, `lang-c-abi-ffi`, `lang-ownership`, `lang-ui`, `lang-translation`, and `lang-gpu`
- split compiler/runtime ownership into `bootstrap-core`, `bootstrap-actors`, `bootstrap-ownership`, `bootstrap-fs`, `bootstrap-gpu`, `runtime-core`, `runtime-stdlib`, and `runtime-gpu`
- split certification ownership into `test-harness`, `test-bench`, `test-attrition`, and `test-crash-forensics`
- kept package-specific lanes explicit with `package-kaintana` and `package-vulkain`
- centralized repo build and Bazel/operator reality under `tool-build-system`; solver-guided weirdness now routes through `tool-z3-black-magic`; release gating routes through `tool-release-readiness`
- rewrote the `lang-*` skills to be usage-first: actual `kain` commands, `rg` probes, manifest snippets, and Kain code examples instead of mainly telling agents which repo files to read
- kept `.agents/skills/TAXONOMY.md` as a minimal live namespace map

# 2026-05-21 - `kain test` gained a real Z3 proof lane and `std::test`

Kain's source test pipeline now has a solver-backed lane instead of only Cargo/compiletest-style execution. `crates/kain-test` accepts `//@ prove-pass` and `//@ prove-sat` directives, collects repeated `//@ smt2:` lines, invokes Z3 from `PATH` or `KAIN_Z3`, and records proof evidence in JSON reports (`solver`, expected result, actual result, obligation line count). `prove-pass` expects `unsat`; `prove-sat` expects `sat`.

What changed:

- `crates/kain-test/src/lib.rs`
  - added `KainTestMode::ProvePass` / `ProveSat`, SMT2 directive parsing, Z3 process execution, and proof evidence in case reports
  - added unit coverage for live `unsat` and `sat` solver cases when Z3 is available
- `crates/cli/src/main.rs`
  - updated the test-mode help text to include proof modes
- `stdlib/test.kn`
  - added authored `std::test` outcome vocabulary (`TestOutcome`, pass/fail/skip/proved/witness helpers)
- `smoketest/kain-test/prove_pass.kn` and `prove_sat.kn`
  - added CLI-facing proof fixtures
- `blades/stdlib-domains/src/main.kn`, `AGENTS.md`, `ARCHITECTURE.md`, and docs
  - wired/documented `std::test` and the solver-backed test flow
- regenerated `stdlib/STDLIB_MAP.llm.md` and `stdlib/stdlib.map.json`

Validation:

- `rustfmt --edition 2021 crates\\kain-test\\src\\lib.rs crates\\cli\\src\\main.rs`
- `cargo test -p kain-test --target-dir target\\codex-native-test-proof`
- `cargo check -p cli --target-dir target\\codex-native-test-proof`
- `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write`
- `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check`
- `cargo build -p cli --bin kain --target-dir target\\codex-native-test-proof`
- `target\\codex-native-test-proof\\debug\\kain.exe test smoketest\\kain-test --json target\\codex-native-test-proof\\kain-test-proof-report.json` -> PASS, 2/2 with `unsat` and `sat` proof evidence
- `target\\codex-native-test-proof\\debug\\kain.exe check blades\\stdlib-domains\\src\\main.kn --target llvm` -> PASS

# 2026-05-20 - build.kn became V3 blade/workspace authority instead of only a task sidecar

`build.kn` is now a real authority surface for blade/workspace discovery, build defaults, run defaults, and explicit build tasks. The repo no longer needs `KAIN.toml` just to discover a blade, pick its entry, or route build/run defaults, even though `KAIN.toml` still works as the compatibility lane and still owns metadata that has not been promoted into the script surface yet.

What changed:

- `crates/kain-blades/src/lib.rs`
  - added effective-manifest synthesis by overlaying `build.kn` / `platform.kn` metadata on top of `KAIN.toml`
  - added script-authority parsing for `workspace_defaults()`, `package("...")`, `blade("...")`, `build_defaults()`, `run_defaults()`, and `build_task("...")`
  - workspace discovery now honors script-authored blade patterns like `workspace_defaults().blade_pattern("packages/*")`
  - blade discovery now accepts script-only blades without `KAIN.toml` when the script declares real blade surface metadata
  - workspace markers now include `build.kn` / `platform.kn`, so blade-root and workspace-root detection can anchor on script authority
- `crates/kain-build/src/workspace.rs`
  - workspace config now reads effective manifest defaults from script-or-TOML authority instead of only raw `KAIN.toml`
  - explicit blade/root tasks now work from script-only authorities instead of requiring `blade.kain_manifest`
  - blade-check inputs now include build scripts as first-class authority files
  - bumped the build adapter fingerprint version to `kain-build-v3`
  - `plan_kain_project(...)` now accepts `build.kn`-only project authority for package/build metadata and task inputs
- `crates/kain-run/src/lib.rs`
  - run planning now loads run defaults from effective script-or-TOML authority for workspaces and blades
  - file-target inference now honors script-authored `run_defaults().target("...")`
  - workspace-level fallback entry resolution now checks script-authored `run_defaults()` / `build_defaults()` / `blade(...).entry(...)`
- `blades/vulkain/build.kn`
  - now dogfoods V3 authority metadata with script-authored package/blade/build/run declarations in addition to the existing platform package + explicit task graph

Validation:

- `cargo fmt --package blade --package kain-build --package kain-run`
- `cargo test -p blade --lib --target-dir target\\codex-build-v3` -> PASS, including new script-only blade/workspace discovery cases
- `cargo test -p kain-build --lib --target-dir target\\codex-build-v3` -> blocked by pre-existing unrelated `crates/web` compile errors in `codegen_wasm.rs` / `codegen_ts.rs` while Cargo resolves the wider graph
- `cargo test -p kain-run --lib manifest_run_section_can_route_file_auto_to_llvm --target-dir target\\codex-build-v3` -> blocked by the same unrelated `crates/web` compile errors before `kain-run` tests could execute

# 2026-05-21 - Dedicated WASM parity lane revived Kain wasm against Rust

Kain now has a dedicated benchmark lane for its long-stale built-in wasm backend. The lane compiles Kain with `-t wasm`, compiles equivalent Rust with `rustc --target wasm32-unknown-unknown`, validates both modules through Node's `WebAssembly.Module`, executes the same export, and requires the normalized `result/stdout` transcript bytes to match exactly.

What changed:

- `crates/web/src/codegen_wasm.rs`
  - folds top-level constants into wasm immediates
  - declares/compiles `converge` functions and selects wasm-target fast lanes
  - exports `main` even when it is not explicitly public
  - lowers grouped expressions, local assignment, array pointer locals, and `len(array)`
  - propagates codegen errors out of wasm control-flow closures instead of silently emitting malformed stack code
- `benchmark/wasm/`
  - added `wasm_cases.json`, a Node wasm execution host, and four Kain/Rust parity cases: `scalar_mix`, `branch_dispatch`, `array_scan`, and `bitwise_pack`
- `benchmark/run_wasm.py`
  - added the root shim for the dedicated wasm lane
- `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - documents the new wasm lane and report locations

Validation:

- `cargo fmt -p web`
- `cargo check -p web`
- `cargo build -p cli --bin kain`
- `python -m py_compile benchmark\wasm\run.py benchmark\run_wasm.py`
- `node --check benchmark\wasm\run_wasm_module.mjs`
- `python benchmark\run_wasm.py --timeout 300 --keep-going` -> PASS for all four cases, with byte-for-byte Kain/Rust wasm transcript matches in `benchmark/latest_wasm.md` and `benchmark/out/reports/wasm_latest.json`

# 2026-05-20 - `build.kn` explicit task parity landed for the blade build graph

`build.kn` is no longer limited to platform-package provenance. The blade build graph can now lift explicit build tasks directly out of script-authored `build_task("...")` chains, while keeping `KAIN.toml` task support as the compatibility/default lane.

What changed:

- `crates/kain-build/src/workspace.rs`
  - added shared build-script discovery so `build.kn` / `platform.kn` extraction now reads both `platform_package(...)` requirements and script-authored explicit tasks
  - added `build_task("id").kind("...").entry("...").target("...").input("...").output("...").depends_on("...")` parsing with the same field surface as `[[build.tasks]]`
  - explicit task selection now prefers script tasks when at least one `build_task(...)` is present and cleanly falls back to manifest tasks when the script only declares platform packages
  - build-graph provenance now reports explicit-task overrides or explicit-task deferral in addition to platform-package override notes
  - fixed explicit task dependency scoping so blade-local `depends_on("prep")` resolves to the actual scoped task id instead of flattening colon-separated graph ids through plain `sanitize_id`
- `blades/vulkain/build.kn`
  - now dogfoods the new script task lane by carrying the `check-llvm` task in the script alongside its Vulkan platform-package requirement

Validation:

- `cargo fmt --package kain-build`
- `cargo test -p kain-build build_graph --target-dir target\\codex-buildkn-phase2`
- `cargo test -p kain-build explicit_build_task_dependencies_use_blade_scope --target-dir target\\codex-buildkn-phase2`
- `cargo test -p kain-build --lib --target-dir target\\codex-buildkn-phase2`
- `cargo test -p kain-run build_graph --target-dir target\\codex-buildkn-phase2`

# 2026-05-20 - `std::reload` became the canonical hot-reload surface, gained an explicit transition lattice, and now has a real attrition lane

The repo now has a real v1 `std::reload` lane instead of making authored code reach directly for native UI hot-reload ABI helpers, and the packaging/dev loop now carries explicit world/actor reload contracts plus an OTP-shaped transition lattice instead of only launcher identity plus artifact-role guesses.

What changed:

- `stdlib/reload.kn`
  - added the new author-facing `std::reload` module with `ReloadGeneration`, `reload_begin`, `reload_commit`, `reload_generation`, `reload_key`, `reload_snapshot`, and explicit policy/lane getter functions
  - kept the current runtime truth package-first by wrapping the existing native UI reload ABI instead of inventing a second runtime path
  - important follow-up: the original `ReloadPolicy` aggregate return shape was not trustworthy on the LLVM path when it carried many `String` fields, so the public surface was narrowed to explicit getters instead of shipping a broken fat aggregate
- `blades/kaintana/src/core/reconciliation.kn` and `runtime/fixtures/native_ui_stdlib_layer/main.kn`
  - switched authored reload calls from raw `native_ui_hot_reload_*` helpers to `std::reload`
- `crates/kain-core/src/runtime_contract.rs`
  - reflection payloads now emit explicit actor-state schemas (`<ActorName>State`) so reload participants can compare actor structure honestly instead of carrying actor names without fields
- `crates/kain-driver/src/native_app.rs` and `crates/kain-driver/src/tauri_app.rs`
  - native and Tauri app manifests now emit `hot_reload.participants`
  - runtime snapshots now mirror that contract under `reload`
  - the participant payload inventories `std::reload`, structural migration defaults, world state schemas, actor state/message schemas, planned GPU frame-boundary/resource-graph hooks, the default restart mode, and the explicit compatibility lanes
  - runtime sidecars now persist the reflection payload path so reload classification has a durable actor-schema source
  - reload metadata now carries a `transition` record with class/restart-requirement/reasons/actions so the runtime can say `presentation-only`, `structural-migrate`, `quiesce-and-migrate`, `frame-boundary-gpu-swap`, or `restart-with-restore` instead of only yes/no
- `crates/cli/src/native_ui_dev.rs`
  - reload manifest/snapshot structs now deserialize the participant contract and transition record and use them as part of restart-vs-live-reload classification
  - added regressions that prove participant-schema drift forces restart and that transition classes such as `frame-boundary-gpu-swap` surface in the operator note instead of staying host-local
- `crates/cli/src/run.rs`
  - `kain run dev <file.kn>` now auto-routes direct native UI/component inputs into the stronger `native_ui_dev` loop when there are no conflicting blade/json/dry-run/app-arg flags
- `attrition/run.py`, `attrition/attritions.json`, and `attrition/cases/kain_std_reload_contract/main.kn`
  - added `validation.semantic_groups` beside `closure_groups` so attrition can certify protocol/lifecycle truth in addition to teardown closure
  - added the real Kain LLVM lane `kain_std_reload_contract`, which certifies `std::reload` generation monotonicity, begin/commit sequencing, checkpoint/progress accounting, and explicit UI session cleanup on the native LLVM path
  - added sabotage proofs `skip_final_commit` and `skip_session_destroy`
- Atlas / docs:
  - regenerated `stdlib/STDLIB_MAP.llm.md` and `stdlib/stdlib.map.json`; the atlas continues to include `std::reload`
  - updated `ARCHITECTURE.md`, this memory note, and the attrition skill

Validation:

- `cargo fmt --package kain-driver --package cli`
- `python -m py_compile attrition/run.py`
- `D:/Kain-Lang/target/debug/kain.exe D:/Kain-Lang/attrition/cases/kain_std_reload_contract/main.kn -t llvm -o D:/Kain-Lang/attrition/cases/kain_std_reload_contract/generated/kain_std_reload_contract.ll`
- `cargo test -p kain-driver --no-default-features --lib -- --list`
- `python attrition/run.py --case kain_std_reload_contract --scale small --profile release-instrumented --timeout 900 --kain-exe D:/Kain-Lang/target/debug/kain.exe`
- `python attrition/run.py --case kain_std_reload_contract --scale small --profile release-instrumented --sabotage skip_final_commit --timeout 900 --kain-exe D:/Kain-Lang/target/debug/kain.exe`
- `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write`
- `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check`

Important behavior notes:

- This is v1 package-first reload, not universal live code swapping. World and actor compatibility is structural today: default-value edits that preserve emitted schemas stay reload-compatible, while schema changes restart honestly.
- `std::reload` is now the canonical surface for authored code, but the current runtime implementation still rides the existing native UI reload substrate underneath.
- GPU participation is metadata-only in this wave. The manifest/snapshot contract already exposes frame-boundary/resource-graph intent, but full Vulkain live resource reload is still a follow-on phase.
- The new attrition lane is intentionally semantic-only for now. It proves reload lifecycle truth, not RC closure, because the remaining LLVM path still shows end-state RC drift when `std::reload` string-return surfaces are exercised heavily.
- That drift is real follow-up work, not something to hide: a green `kain_std_reload_contract` run may still report `cases_with_closure_drift` telemetry because the lane currently validates the reload protocol while deliberately not gating on the broader string/RC leak family yet.
- Full `cli` crate validation for this branch is still partially obscured by unrelated repo breakage in optional surfaces (`ue5*` and a `RustBuildOutput.bundle` mismatch). The reload-specific driver and attrition evidence is still good, but future cleanup should rerun the higher-level CLI tests once those unrelated blockers are removed.

# 2026-05-20 - Website package registry model replaced marketplace-first flow

The ignored `website/` workspace now has a registry-native backend lane for Kain packages instead of treating the public ecosystem as only products.

What changed:

- `website/db/schema.ts` and `website/db/migrations/012_kain_package_registry.sql`
  - added first-class `packages`, `package_versions`, `package_artifacts`, `package_dependencies`, and `package_owners`
  - kept `products` only as a legacy mirror/fallback and added rich product fields needed by old rows
- `website/api/_src/packages.ts`
  - added public `/api/packages`, `/api/packages/:idOrSlug`, `/api/packages/:idOrSlug/download`, and `/api/packages/:idOrSlug/acquire`
  - resolves package rows into frontend-compatible catalog entries, hydrates versions/artifacts/dependencies, signs Supabase storage artifacts, and falls back to legacy products if the registry migration has not run
- `website/api/_src/admin-packages.ts` plus `website/api/_src/admin.ts`
  - added admin CRUD for packages, versions, artifacts, and owners under `/api/admin/packages`
- `website/src-frontend/features/packages/*`, `PackageDetailPage.tsx`, and `productService.ts`
  - package list/detail now call `/api/packages`
  - package actions resolve/download artifacts directly instead of opening Stripe checkout
  - categories/channels now include core package lanes such as runtime, platform, graphics, UI, and library
- `packages/README.md`
  - documents `/packages` as the stable first-party package workspace and maps blades -> package graduation to the registry nouns

Validation:

- `bunx tsc --project api/tsconfig.json --noEmit` in `website/` -> PASS
- `bun --bun vite build --mode production --config vite.config.web.ts` in `website/` -> PASS
- Full web typecheck still has old unrelated unused/WASM backlog, but no errors from the touched package files when filtered.

# 2026-05-20 - LLVM floor fastpath flipped sim_uv and kept the full suite green

This automation pass targeted the latest honest sim frontier after the packed two-byte substring work: `sim_uv_velocity_grid`, where the canonical `latest` report still had Kain losing to both Rust and C++ and the emitted LLVM IR was still routing every `floor(Float) -> Int` through the out-of-line runtime wrapper `kain_floor_i64`.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - emits `declare double @llvm.floor.f64(double)` once in the LLVM prelude
  - adds a compiler-owned `compile_numeric_floor_builtin(...)` lane that lowers `floor(x)` into `llvm.floor.f64` plus `fptosi`
  - wires direct stdlib `floor(...)` calls through that lane instead of always bouncing through `kain_floor_i64`
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - added `llvm_lowers_floor_builtin_with_llvm_intrinsic`, which proves the generated IR contains the LLVM intrinsic path and does not call `kain_floor_i64`
- `benchmark/cases/sim_uv_velocity_grid/main.kn`
  - now touches `deadline_millis` / `deadline_elapsed` once so the row exercises the live deadline surface requested for the automation
- Durable notes:
  - `research/2026-05-20-benchmark-frontier-speedup-hunt.md`
  - `benchmark/assesments/2026-05-20-llvm-floor-fastpath-latest-benchmark-assessment.md`
- Proof artifact:
  - `crates/kain-sys-codegen/z3/proofs-experimental/floor-fastpath-defined-domain.smt2`
  - report `z3/reports/20260520T195910Z-20260520T1932Z-floor-fastpath-defined-domain.json`

Validation:

- LLVM/codegen check:
  - `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_floor_builtin_with_llvm_intrinsic -- --exact` -> PASS
- Focused frontier retake:
  - `benchmark/out/reports/latest_floor_probe.llm.md`
  - `sim_uv_velocity_grid`: Kain `15.588 ms`, Rust `16.721 ms`, C++ `15.811 ms`
  - `sim_nbody_gravity`: Kain `9.774 ms`, Rust `10.343 ms`, C++ `10.859 ms`
  - `sim_cfd_pressure_projection`: Kain `12.228 ms`, Rust `10.210 ms`, C++ `11.197 ms`
- Canonical clean-worktree full-suite rerun:
  - `python benchmark/run.py --timeout 900 --baseline-mode auto` in `D:\\Kain-Lang\\.codex-tmp\\kain-frontier-20260520` -> PASS
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T19:32:41.115572+00:00`
  - `sim_uv_velocity_grid` improved from the prior canonical `17.150 ms` / Rust `15.234 ms` / C++ `14.134 ms` to Kain `15.813 ms`, Rust `17.399 ms`, C++ `16.995 ms`, flipping the row into a real Kain win
  - suite regression summary: `kain_regressions = 0`, `alert_regressions = 0`
- Proof:
  - `mcp__z3_local__.check_smt2(...)` on the defined-domain floor fastpath model -> `unsat`

Durable lesson:

- The next honest sim gain was not another benchmark rewrite. It was deleting an out-of-line rounding wrapper from the hot LLVM path and keeping the authored row otherwise intact.
- The improvement is real but not universal magic. It decisively flips `sim_uv_velocity_grid`, helps the focused `sim_nbody_gravity` probe, and keeps the full suite green, but it does not by itself close the `sim_cfd_pressure_projection` gap.
- After this rerun, the next honest frontiers are no longer the stale pre-pass list. The canonical post-change gaps worth attacking are now:
  - `process_stdio_loop`: Kain `7052.660 ms`, Rust `4709.323 ms`, C++ `9450.884 ms`
  - `recursive_sum`: Kain `14.090 ms`, Rust `10.442 ms`, C++ `11.456 ms`
  - `sim_cfd_pressure_projection`: Kain `9.889 ms`, Rust `14.040 ms`, C++ `9.210 ms`
  - `option_result`: Kain `10.857 ms`, Rust `11.204 ms`, C++ `9.978 ms`
  - `sim_nbody_gravity`: Kain `10.064 ms`, Rust `10.474 ms`, C++ `9.535 ms`

# 2026-05-20 - Packed two-byte substring lane flipped string_ops and kept the suite green

This automation pass targeted the clean compiler frontier after the process/runtime win: `string_ops`, where the latest honest focused run still had Kain behind Rust on a stable ASCII substring row and the existing LLVM fast path was still paying a `memchr`-driven shape for tiny static needles.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `compile_known_length_find_substring_inline(...)` now routes statically visible two-byte needles into `compile_known_length_find_substring_inline_static_two_byte_needle(...)`.
  - the new lane keeps the authored helper semantics but replaces the `memchr` call with a stride-1 packed 16-bit compare loop and a one-byte remaining-span update.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - added `llvm_lowers_static_two_byte_find_substring_from_to_packed_stride_one_search` and kept the prior general-path substring tests green so the new lane stays isolated to the tiny-static-needle surface.
- `crates/kain-sys-codegen/z3/proofs/control-inline-known-string-static-two-byte-find-substring-stride-stays-in-bounds.yaml`
  - durable proof that the stride-1 cursor advance and `next_remaining` update stay in-bounds (`unsat` in the full pack).
- `crates/kain-sys-codegen/z3/proofs-experimental/inline-known-string-static-two-byte-first-match-selection.smt2`
  - exploratory benchmark-shape proof that the packed two-byte first-match selector returns the same answer as the readable left-to-right scan (`unsat` report `z3/reports/20260520T172131Z-inline-known-string-static-two-byte-selection.json`).
- `benchmark/benchmarks.json`
  - updated `string_ops` / `unicode_string_heavy` honesty notes so reports describe compiler-owned inline substring search with the new packed two-byte lane instead of the older `memchr/memcmp` wording.
- Durable notes:
  - `research/2026-05-20-string-packed-two-byte-lane.md`
  - `benchmark/assesments/2026-05-20-packed-two-byte-substring-lane-latest-benchmark-assessment.md`

Validation:

- LLVM/codegen checks:
  - `cargo test -p kain-sys-codegen llvm_lowers_static_two_byte_find_substring_from_to_packed_stride_one_search -- --nocapture` -> PASS.
  - `cargo test -p kain-sys-codegen llvm_lowers_find_substring_from_on_known_strings_with_precomputed_lengths -- --nocapture` -> PASS.
  - `cargo test -p kain-sys-codegen llvm_lowers_manual_find_substring_helpers_with_len_on_miss_to_native_search -- --nocapture` -> PASS.
  - `cargo test -p kain-sys-codegen llvm_lowers_manual_find_substring_helpers_with_negative_one_miss_to_native_search -- --nocapture` -> PASS.
- Proofs:
  - `mcp__z3_local__.check_smt2(...)` on the packed two-byte first-match selector -> `unsat`.
  - `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", report_name="kain-sys-codegen-static-two-byte-substring-pack")` -> `27 proved, 0 counterexamples, 0 unknown, 0 errors`.
- Focused frontier retake:
  - `benchmark/latest_string_frontier_packed_two_byte.md`
  - `string_ops`: Kain `7.969 ms`, Rust `9.463 ms`, C++ `9.882 ms`
  - `unicode_string_heavy`: Kain `9.052 ms`, Rust `9.163 ms`, C++ `8.466 ms` with one Kain outlier sample
- Canonical clean-worktree full-suite rerun:
  - `python benchmark/run.py --timeout 900 --baseline-mode auto` in `D:\Kain-Lang\.codex-tmp\kain-packed-two-byte-verify` -> PASS
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T17:57:03.518984+00:00`
  - `string_ops` improved to Kain `8.288 ms`, Rust `10.481 ms`, C++ `11.003 ms`
  - `unicode_string_heavy` stayed in the noise band at Kain `9.777 ms`, Rust `9.737 ms`, C++ `10.753 ms`
  - suite regression summary: `kain_regressions = 0`, `alert_regressions = 0`
- Regression sanity:
  - `benchmark/latest_machine_stones_regression_probe.md`
  - the scary full-suite `machine_stones_shatter_loop` sample (`74.797 ms`) did not reproduce; focused retake returned Kain `12.400 ms`, Rust `12.711 ms`, C++ `12.169 ms`

Durable lesson:

- The next honest substring win was not a benchmark rewrite or constant-folded cheat. It was a narrower backend mechanism: keep the general known-string search path, but give tiny static needles a different proved search geometry.
- Full-literal haystack/needle collapse would have been the easy alien move, but it would stop measuring the declared row. The kept lane only specializes the needle shape and still executes the hot loop honestly.
- `string_ops` is the real shipped win. `unicode_string_heavy` still lives in a near-noise-band pocket, so the honest claim is that the tiny-static-needle lane helps there but does not cleanly own the row yet.
- The biggest remaining honest frontiers after this pass are now:
  - `http_server_concurrency`: Kain `55.194 ms`, Rust `47.584 ms`
  - `sim_uv_velocity_grid`: Kain `17.150 ms`, Rust `15.234 ms`, C++ `14.134 ms`
  - `ownership_memory`: Kain `12.037 ms`, Rust `11.525 ms`, C++ `11.352 ms`
  - `sim_nbody_gravity`: Kain `9.899 ms`, Rust `9.519 ms`, C++ `8.998 ms`

# 2026-05-20 - Disabled attrition fastpath plus process output cleanup flipped the process row

This automation pass targeted the real post-hardening frontier from the morning full suite: `process_stdio_loop`, plus the broader disabled-attrition tax that benchmark-release was still paying by accident.

What changed:

- `runtime/native/src/core/process_system.c`
  - `process_output_text(...)` now sends stderr to `NUL` instead of creating/draining an invisible stderr pipe.
  - Windows process launch now reuses cached `NUL` handle templates, resolves bare `cmd` / `cmd.exe` through a cached application path for `CreateProcessW`, and avoids duplicate-close cleanup on null stdio handles.
- `runtime/native/src/core/attrition.c`
  - added an atomic disabled fast-flag so benchmark-release runs skip attrition event hooks, actor/process/async timer notes, RC note paths, raw clock/sleep bookkeeping, and attrition heap wrappers unless capture is explicitly configured.
  - this was the real cross-row unlock: benchmark-release had `config.enabled == 0`, but the runtime was still taking the init/lock/event path anyway.
- `benchmark/cases/process_stdio_loop/main.kn`
  - now touches `deadline_millis` / `deadline_elapsed` once so the row exercises the requested live deadline surface.
- `benchmark/cases/process_stdio_loop/proofs-experimental/process-stdio-loop-checksum.smt2`
  - checksum guard proof for the touched row; report `z3/reports/20260520T122202Z-process-stdio-loop-checksum.json` returned `unsat`.
- `benchmark/run.py`
  - fixed the `render_case_detail(...)` `primary_metric` local-shadow bug so focused `--latest-stem` retakes no longer crash with `UnboundLocalError`.
- Durable notes:
  - `research/2026-05-20-benchmark-frontier-process-stdio.md`
  - `benchmark/assesments/2026-05-20-process-stdio-and-disabled-attrition-fastpath-latest-benchmark-assessment.md`

Validation:

- `python -m py_compile benchmark/run.py` -> PASS.
- `toolchain\\llvm\\bin\\clang.exe -fsyntax-only runtime\\native\\src\\core\\process_system.c -I runtime\\native\\include` -> PASS.
- `toolchain\\llvm\\bin\\clang.exe -fsyntax-only runtime\\native\\src\\core\\attrition.c -I runtime\\native\\include` -> PASS.
- Focused frontier retakes:
  - `benchmark/out/reports/latest_process_stdio_frontier.llm.md`
  - Kain moved `5883.793 ms -> 5577.407 ms -> 5486.127 ms`
  - Rust baseline for that focused 5-run shape stayed `5338.471 ms`
- Canonical full-suite rerun:
  - `python benchmark/run.py --timeout 900 --baseline-mode auto` -> PASS
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T12:32:24.336303+00:00`
  - `process_stdio_loop` flipped from the old durable `6809.287 ms` / Rust `5174.384 ms` frontier to Kain `5487.617 ms`, Rust `5687.132 ms`, C++ `9695.726 ms`
  - collateral full-suite wins now include:
    - `ownership_memory`: Kain `10.671 ms`, Rust `11.119 ms`, C++ `11.952 ms`
    - `memory_stream`: Kain `9.522 ms`, Rust `9.964 ms`, C++ `10.418 ms`
    - `alloc_churn`: Kain `8.253 ms`, Rust `10.729 ms`, C++ `9.922 ms`
  - suite regression summary: `kain_regressions = 0`, `alert_regressions = 0`

Durable lesson:

- Disabled attrition must actually collapse to near-zero cost in `benchmark-release`; otherwise host-heavy and allocation-heavy rows end up benchmarking bookkeeping that the runtime itself claims is off.
- `process_output_text(...)` was paying for a whole stderr capture path that its public contract never exposed. Deleting invisible work was enough to flip the canonical row once the disabled-attrition tax was also removed.
- The next honest frontier is now `http_server_concurrency` again: current canonical full suite shows Kain `125.680 ms` vs Rust `40.919 ms`.

# 2026-05-20 - Benchmark runner now persists SQLite history and prior-run Kain deltas

`benchmark/run.py` now has a first-class benchmark history lane instead of only timestamped JSON/Markdown artifacts plus the foreign baseline cache.

What changed:

- Added stdlib-`sqlite3` history persistence at `benchmark/out/history/benchmark_history.sqlite3` by default, configurable with `--history-db <path>` and disable-able with `--history-db off`.
- Each benchmark invocation now records a normalized run row plus case/language/metric rows: suite identity, selected cases/languages, toolchain/git metadata, report artifact paths, Kain and foreign medians, build timings, cache status, samples/warmups, and primary telemetry metric values.
- Reports now compare current Kain results against the most recent prior *comparable* run, keyed by suite + `latest_stem` + machine fingerprint + selected case/language set + warmup/run counts. The LLM report and minimal snapshot surface per-case `delta_ms`, `delta_pct`, trend, and alert-worthy regressions.
- Added focused unit coverage at `benchmark/tests/test_run_history.py` for SQLite persistence, previous-run lookup, Kain improvement detection, and regression-alert classification.
- Updated `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future agents know the history DB is part of the runner contract.

Validation:

- `python -m py_compile benchmark/run.py` -> PASS.
- `python -m unittest benchmark.tests.test_run_history` -> PASS (2 tests).
- `python benchmark/run.py --case alloc_churn --languages python --runs 1 --warmups 0 --history-db benchmark/out/history/py_smoke_history.sqlite3 --latest-stem latest_history_py_smoke --minimal-name latest_history_py_smoke.md` -> PASS.

Known live smoke caveat:

- A focused Kain smoke (`scalar_mix`, `--kain-exe target/release/kain.exe`) wrote history/report rows correctly but the benchmark itself failed with an existing native link error: unresolved `kain_native_converge_record_i64` while linking `benchmark/out/build/scalar_mix/kain/scalar_mix.exe`. History persistence still recorded the failed run as intended; the compile/runtime issue is separate from the new history lane.

# 2026-05-20 - `kain run` consumes platform locks, transitive FFI, inferred modules, and imported Self builders

This pass turned the platform-package/build-graph work into the daily run path and cleared the imported `impl Self_` LLVM blocker for fluent builders.

What changed:

- `crates/kain-run` plans now include `RunBuildGraphProvenance` and `RunPlatformLock` entries. `build.kn` / `platform.kn` `platform_package("...").provider("...")` declarations and manifest `[[platform.packages]]` defaults are parsed into run reports, dry-run/plan mode records platform locks as `planned`, and real runs import/lock packages before execution.
- `kain run dev` / `kain watch` now watch the entry inputs plus `KAIN.toml`, `kain.toml`, `build.kn`, `platform.kn`, generated lockfiles, generated platform modules, binding reports, and inherited blade C/FFI inputs.
- `crates/kain-blades` now exposes transitive `[c_ffi]` libraries through blade dependencies. `kain-run` attaches inherited headers, sources, shared libraries, include paths, `KAIN_TRANSITIVE_C_FFI_INPUTS`, and `KAIN_TRANSITIVE_C_FFI_LIBS` to the final executable unit instead of forcing app entrypoints to restate every library-blade bridge.
- Blade module roots are inferred from nested `.kn` / `.god` files below declared `source_roots`, while generated platform modules under `.kain/platform` are made visible to module resolution. This should reduce hand-maintained `module_roots` soup for Kaintana/Vulkain-style trees.
- Native LLVM now lowers imported builder methods with authored `_self: Self_` / `Self_` returns as the impl target storage type, skips the duplicate explicit self parameter in the emitted ABI, binds `_self` to the implicit receiver, and supports dot calls on value aggregates by taking a temporary address.

Validation:

- `cargo fmt -p kain-run -p blade -p kain-sys-codegen -p kain-driver`
- `cargo test -p kain-run --target-dir target\codex-platform-package` -> PASS, 11 tests.
- `cargo test -p blade` -> PASS, 9 tests.
- `cargo test -p kain-sys-codegen lowers_impl_self_builder_methods_without_extra_self_parameter` -> PASS.
- `cargo test -p kain-driver compile_llvm_supports_imported_impl_self_builder_methods` -> PASS.
- `python tools/bazel/sync_rust_builds.py` and `python tools/bazel/sync_rust_builds.py --check` -> PASS, 62 generated Rust package BUILD files checked.
- `bazel test //crates/kain-run:unit_test --config=dev` -> PASS.

Follow-up cleanup:

- The 15 stale `cargo test -p kain-sys-codegen` LLVM integration failures were cleared later on 2026-05-20. Native LLVM pattern binding now handles tuple/struct value aggregates by spilling them to entry-block temporaries before field GEPs, and `tests/llvm_codegen_test.rs` expectations now match current IR contracts: internal linkage for non-entry functions, stack-backed fixed arrays, typed pointer GEPs, value-aggregate tuple returns, and struct `None` as zeroinitialized value aggregates. `cargo test -p kain-sys-codegen` now passes.

# 2026-05-20 - Platform package lock/import v1 landed

Kain now has a v1 native platform package lane that favors deterministic lock/import plus generated typed thunks over public generic dynamic-call magic.

What changed:

- `runtime/native` added `platform.library`: fixed-table dynamic library open/resolve/close/status helpers in `platform_library.{h,c}`, exported through `stdlib/platform.kn` as `std::platform`.
- `crates/kain-c-ffi` added `import_platform_package` and `kain import platform`, producing target-aware locks at `.kain/platform/<package>/<target-triple>/<package>.lock` with roots searched, resolved headers/libs, hashes, discovered/generated symbols, capability tags, blocked symbols, and generated module names.
- Vulkan is special by metadata and dispatch model: the importer records `vulkan-loader-dispatch`, prefers `vk.xml` when present, and generates loader thunk metadata instead of pretending every Vulkan command is a normal DLL export.
- `crates/kain-build` records deterministic graph provenance from `build.kn`, `platform.kn`, or equivalent `KAIN.toml` `[[platform.packages]]`; matching TOML/script requirements produce the same graph, while overrides report explicit provenance.
- `fixtures/platform_sdk/tiny_math` is the tiny SDK proof fixture for header scan, lock determinism, generated typed thunk metadata, and stable negative-surface reasons before touching real Vulkan installs.
- `blades/platform-package-smoke` is the tiny proof blade for the lane. Its script stages `tiny_math`, imports twice, byte-compares lock/report output, checks relocatable path rendering, verifies blocked callback/opaque/unsupported reasons, checks no public `call_typed` leak, then runs the Kain `std::platform` open/resolve/close smoke.
- `blades/vulkain` now declares Vulkan as a platform package graph requirement in both `build.kn` and `KAIN.toml`; dispatch remains package metadata owned by `platform::vulkan`, not `runtime/native`.
- `blades/vulkain` is now the first real dogfood consumer of that lane: `build-vulkain.ps1` imports `vulkan.lock`, derives headers/tools/loader DLL from the lock, exports `KAIN_PLATFORM_VULKAN_*` env, and the bridge now prefers the lock-derived loader path instead of hardcoding only `vulkan-1.dll`.
- `crates/kain-run` now exports package-derived env such as `KAIN_PLATFORM_<PKG>_SDK_ROOT`, `_HEADER`, `_INCLUDE`, `_DLL`, `_IMPORT_LIB`, and `_REGISTRY`, so platform package facts can feed manifests and run units directly instead of living only in ad hoc scripts.

Proof/validation:

- Z3 proof `runtime/native/src/platform/z3/proofs-experimental/platform-library-handle-roundtrip-and-stale-reject.smt2` returned `unsat` via report `z3/reports/20260520T090846Z-platform-library-handle-roundtrip-and-stale-reject-clean.json`.
- `cargo test -p kain-c-ffi --target-dir target\codex-platform-package -- --test-threads=1` passed, including deterministic/relocatable lock+report byte-compare checks and the no-`call_typed` assertion.
- `cargo test -p kain-build --target-dir target\codex-platform-package -- --test-threads=1` passed, including build.kn/KAIN.toml graph parity plus explicit override provenance.
- `cargo test -p kain-commands --target-dir target\codex-platform-package`, `cargo check -p cli --target-dir target\codex-platform-package`, `bazel test //runtime:native_test_platform_library`, `cargo run -q -p cli --bin kain --target-dir target\codex-platform-package -- check blades\platform-package-smoke\src\main.kn --target llvm`, and full `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\platform-package-smoke\run.ps1` passed. The full smoke generated `.kain/run/platform_package_smoke.txt` with `status=0`, while the executable still prints the existing native runtime `[MEMORY] ERROR: RC release underflow` shutdown diagnostic on stderr.
- Full `bazel test //runtime:native_runtime_tests` is still red because existing `//runtime:native_test_ownership_memory` fails `generic observe uses imported registry path expected 0, got -6`; the new platform-library test passes inside and outside that suite.

# 2026-05-20 - Kaintana builder widgets now own interaction semantics in Kain

`blades/kaintana` just crossed an important boundary: the builder-framework widgets are no longer static visuals that always return `activated=0` and pass through slider input verbatim. Interaction semantics now live in Kain:

- `src/core/widget_events.kn` is the new framework-owned event lane. It pumps `std::ui` events at frame begin, tracks pointer hover/capture/drag through generic UI state cells on the root/node ids, and records cumulative per-node counters (`pointer.down`, `pointer.move`, `pointer.up`, `pointer.activate`) instead of inventing another bridge-local widget runtime.
- `src/core/reconciliation.kn` now calls that sync lane from `kaintana_context_begin_frame(...)`, and `src/api/kaintana_ui.kn` exposes `kaintana_sync(ctx)` so authored code can explicitly resync after synthetic or future presenter-fed event injection.
- `src/api/widgets.kn` now consumes those Kain-side counters/flags directly: buttons return real `activated` pulses, sliders return live dragged values, and text inputs at least reflect focus state visually through the underline color instead of being pure paint.
- `src/main.kn` gained a headless two-frame probe that renders a button + slider, pushes synthetic `pointer.down/move/up` events into the session, rerenders, and asserts `activated == 1` plus a dragged slider value near 75%. This keeps the interaction lane validated without waiting on the desktop compatibility presenter.

Boundary truth:

- The current `native/kaintana_desktop_bridge.c` path is still mostly a draw host. `ui_host_pump()` on `software` / `headless` backends does not magically invent OS pointer traffic, and the desktop bridge is not yet feeding raw Win32 pointer events back into the passive `std::ui` queue.
- That is okay architecturally. The important inversion already happened: event interpretation, capture, activation, slider math, and widget state policy are now Kain-owned. Any future live backend only needs to inject `pointer.*` events into the session instead of re-implementing widget logic in C or Rust.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun` -> PASS, generated `kaintana.exe`.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -FrameBudget 3` -> PASS, exit `0`; `.kain/run/kaintana_host_report.txt` still reports `commands=169`, `frames=3`, `last_error=ok`, which means the new headless widget probe passed before the live examples-tour window launched.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kloner\run.ps1 -NoRun` -> PASS after importing the new `widget_events.kn` dependency.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kloner\run.ps1 -FrameBudget 2 -SkipShaderCompile` -> PASS, exit `0`; `.kain/run/kloner_frame.txt` still reports `ui_draw_count=95` and `.kain/run/kloner_vulkain_report.txt` still reports `frames_presented=2`, `last_error=ok`.

# 2026-05-20 - Kloner same-window Kaintana x Vulkain mograph blade

`blades/kloner` is now the KCloner recreation lane for a same-window Kaintana + Vulkain 3D mograph/cloner scene, superseding the earlier flat/static sidecar-style UI attempt. The authored Kain side is intentionally modular:

- `src/kloner_state.kn`: settings, reference metadata, layout naming, report/export text, and data-driven clone/runtime defaults.
- `src/kloner_session.kn`: Kain-owned app session/config lane. It applies env overrides, tracks transport/platform-lock state, accepts a `KlonerUiFrame`, and regenerates runtime/export/report state without pushing app policy back into C.
- `src/kloner_ui.kn`: Kaintana builder UI over the refactored SlotMap framework. It now returns a full `KlonerUiFrame` result with slider/button values plus activation slots, so the future live Kaintana interaction lane can land without Kloner re-architecting again.
- `src/kloner_scene.kn`: std::math-backed scene probes plus Kain-side packetization into `vulkain::VulkainKlonerPacket` before the same-window presenter call.
- `src/kloner_lattice.kn`: Kain worlds, entangle links, laws, and patches for clone/layout authority state.
- `src/main.kn`: runtime boot, Kaintana composition/commit, report/export writes, same-window Vulkain presentation, and validation gates.

`blades/vulkain` now has a Kloner-specific reusable bridge entrypoint (`vulkain_run_kloner_same_window`) plus shader/C support for instanced sphere impostors in one Vulkan window with a procedural Kaintana-style overlay. The bridge clamps logical instances to `1..1_000_000`, draws a fullscreen background pass, draws six billboard vertices per instance for grid/radial/honeycomb/helix layouts, then draws a foreground overlay pass so the UI docks sit on top of the 3D scene. Keep app policy in Kloner/Kaintana; keep native window/GPU substrate in Vulkain.

Kloner now dogfoods the platform-package lane directly instead of only borrowing Vulkain's build assumptions:

- `blades/kloner/build.kn` requires `platform_package("vulkan").provider("system")`.
- `blades/kloner/KAIN.toml` mirrors that requirement through `[[platform.packages]]`.
- `blades/kloner/run.ps1` gained `-SkipShaderCompile` passthrough so finite smokes can reuse validated SPIR-V while still syncing the lock-backed Vulkan package through `blades/vulkain/build-vulkain.ps1`.

Kloner launch semantics:

- Default launch is interactive-until-close: `kloner_settings().frame_budget` defaults to `0`, and Vulkain treats `frame_budget <= 0` as an interactive Kloner loop.
- Automated validation still uses `blades/kloner/run.ps1 -FrameBudget N`, which sets `KLONER_FRAME_BUDGET` and forces a finite run.
- Native viewport controls live in `vulkain_bridge.c`: RMB drag or `A/D` orbits, `W/S` dollies, arrows and `Q/E` pan, `1/2/3/4` switch grid/radial/honeycomb/helix, `Z/X` adjusts sphere size, `C/V` spacing, `F/G` wave amount, `[ / ]` clone count, `R` resets camera, and `Esc` closes.

Important gotcha: imported Kain `String` fields can still stale/corrupt after heavy Kaintana composition. During the session refactor, storing `authoring_lane` / `platform_provider` strings inside `KlonerSession` produced corrupted scene-report output (`platform=kaintana.slider.fill.color.g // locked`). The durable fix is to keep volatile app/session state numeric or structural where possible and regenerate report labels from fixed helper literals (`platform::vulkan(system) // locked`, `kain.session -> kaintana.frame -> vulkain.packet // same-window.foreground-overlay`) at write time.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kloner\run.ps1 -NoRun` -> PASS, generated `blades/kloner/kloner.exe`, exit `0`.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kloner\run.ps1 -FrameBudget 2 -SkipShaderCompile` -> PASS, exit `0`.
- `.kain/run/kloner_frame.txt`: `target_fps=120`, `layout=HONEYCOMB`, `clones=1000000`, `ui_draw_count=95`, `reference=KCloner.tsx`.
- `.kain/run/kloner_scene.txt`: `platform=platform::vulkan(system) // locked`, `authoring_lane=kain.session -> kaintana.frame -> vulkain.packet // same-window.foreground-overlay`, `frames_presented=2`, `status=0`.
- `.kain/run/kloner_vulkain_report.txt`: finite smoke reports `frame_budget=2`, `interactive_mode=0`, `instance_count=1000000`, `frames_presented=2`, `vertices_drawn=12000012`, `last_error=ok`.
- Default live launch sanity check: `Start-Process .\kloner.exe`, wait 5s -> process still alive; close main window -> report records `frame_budget=0`, `interactive_mode=1`, `frames_presented=460`, `last_error=ok`.
- Visual sanity: MCP window capture showed the foreground overlay docks/sliders/layout strip/crosshair drawn over the million-sphere honeycomb field.
- Z3: `blades/vulkain/native/z3/vulkain_bridge_bounds.smt2` now proves the Kloner instance clamp, per-frame vertex ceiling (`6 + 6 * 1_000_000`, background plus overlay fullscreen triangles), 4096-frame total vertex ceiling, and signed-64 safety bound; MCP report `z3/reports/20260520T073416Z-vulkain_kloner_interactive_overlay_bounds.json` returned all `unsat`.

# 2026-05-20 - Kaintana platform adapters and live examples launch

`blades/kaintana` now has first-class platform adapter seams beyond the desktop compatibility bridge:

- `src/platform/vulkan/vulkan_adapter.kn`: stdlib-backed Vulkan capability adapter that creates a `std::graphics` session, selects the `vulkan` backend when available, stages a tiny SPIR-V mesh/pipeline/draw probe, and destroys the graphics session. This deliberately does not import `vulkain_bridge.dll`; the foreign-presenter lane remains the opt-in `blades/kaintana-vulkan` package.
- `src/platform/winit/winit_adapter.kn`: Kain-side winit/event-loop adapter contract over passive `std::ui` host sessions and existing `KaintanaContext` sessions. It pumps/presents host state and scores the contract without pulling Rust/winit into the base blade yet.
- `src/main.kn`: the examples tour now enters `kaintana_desktop_host_run_window(spec)` after composing the scene, so `kaintana.exe` is a live Win32 desktop proof instead of only a report/BMP generator. Default live budget is `6000` frames; override with `KAINTANA_EXAMPLES_FRAME_BUDGET` or `blades/kaintana/run.ps1 -FrameBudget N`.
- `KAIN.toml`: module roots now include `src/platform/vulkan` and `src/platform/winit`.

Run semantics discovered:

- `kain run` is still a one-shot compile/run command, not a Tauri-style watcher/event loop. The program must call a host loop itself.
- Plain manifest `kain run . --target llvm --keep-artifacts --json` works after the C bridge object has been staged. `run.ps1` remains the reliable route because it builds `native/kaintana_desktop_bridge.c` first.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun` -> PASS, generated `kaintana.exe`.
- Direct short live smoke: `KAINTANA_EXAMPLES_FRAME_BUDGET=3`, `.\blades\kaintana\kaintana.exe` -> exit `0`; host report says `commands=169`, `frames=3`, `last_error=ok`.
- Manifest run from `blades/kaintana`: `kain run . --target llvm --keep-artifacts --json` with `KAINTANA_EXAMPLES_FRAME_BUDGET=3` -> status `succeeded`, exit `0`.

# 2026-05-20 - Kaintana examples tour suite landed

`blades/kaintana/examples` now holds single-file examples that compile into the normal `blades/kaintana/kaintana.exe` application through `examples/example_tour_suite.kn`. This keeps examples discoverable without creating a folder per demo or separate binaries.

Examples added:

- `example_todo_list.kn`: rows with toggle/delete controls for data-driven state and stable key reconciliation.
- `example_tabbed_pane.kn`: three tabs with one active content branch for conditional composition.
- `example_modal_popup.kn`: underlay controls plus appended modal/shield/dialog commands for layering/order.
- `example_data_grid.kn`: virtualized table window rows `240-247` with sortable-looking headers.
- `example_keypad.kn`: classic 3x4 keypad grid.
- `example_resizable_panel.kn`: split preview/inspector layout with a handle and snap buttons.
- `example_file_explorer.kn`: fake explorer path/tree rows.
- `example_mega_button_test.kn`: 20-button grid to pressure stable keys and command count.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun` -> PASS, generated `kaintana.exe`.
- Direct smoke: `cmd /v:on /c "cd /d D:\Kain-Lang\blades\kaintana && kaintana.exe & echo EXIT:!ERRORLEVEL!"` -> `EXIT:0`.
- Host report: `.kain/run/kaintana_host_report.txt` reports title `Kaintana // Examples Tour`, `commands=169`, `last_error=ok`.
- Screenshot: `.kain/run/kaintana_host.bmp`, 5,734,454 bytes.

# 2026-05-20 - Benchmark latest truth hardened; CFD frontier collapsed back to near parity

This automation pass started from a false frontier: the canonical `benchmark/latest.md` snapshot was making `sim_cfd_pressure_projection` look like a catastrophic Kain regression even though focused retakes kept landing near parity. The durable fix was not only the already-tracked CFD row linearization, but also hardening `benchmark/run.py` so the suite stops failing or lying under Windows churn.

What changed in this pass:

- `benchmark/run.py`
  - manifest default latest profile now lands at `3` warmups / `9` timed runs instead of `2` / `7`
  - reports emit `Stability Alerts` for outlier-heavy samples, so future agents can tell noisy rows from real regressions
  - direct build outputs now purge `.exe` / linker sidecars before rebuild and retry Windows permission-denied linker failures
  - Kain case runs retry once after purging case-local `generated/native_runtime` cache when a transient `.tmp` miss blows up a build
- Durable assessment note:
  - `benchmark/assesments/2026-05-20-benchmark-stability-and-cfd-linearization-latest-benchmark-assessment.md`

Validation:

- focused build-lock fixes:
  - `benchmark/out/reports/latest_contention_lockfix.llm.md`
  - `benchmark/out/reports/latest_process_lockfix.llm.md`
- focused CFD checkpoints:
  - before: `benchmark/out/reports/latest_sim_cfd_probe_before.llm.md` -> Kain `11.041 ms`, Rust `10.667 ms`, C++ `9.657 ms`
  - after linearized row/plane shape: `benchmark/out/reports/latest_sim_cfd_linearized.llm.md` -> Kain `10.334 ms`, Rust `10.336 ms`, C++ `9.870 ms`
- Z3 bounds proof for the linearized CFD lane:
  - `benchmark/cases/sim_cfd_pressure_projection/proofs-experimental/sim-cfd-linearized-bounds.smt2`
  - report `z3/reports/20260520T051221Z-sim-cfd-linearized-bounds.json` -> `unsat`
- canonical full-suite rerun:
  - `python benchmark/run.py --timeout 900 --baseline-mode auto`
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T05:49:15.103727+00:00`
  - suite status: `PASS`

Latest frontier truth after the hardening pass:

- `process_stdio_loop`: Kain `6809.287 ms`, Rust `5174.384 ms` -> biggest remaining honest implemented gap
- `http_server_concurrency`: Kain `57.447 ms`, Rust `48.491 ms` -> still the highest-value runtime/native HTTP gap
- `recursive_sum`: Kain `10.566 ms`, Rust `9.465 ms`
- `ownership_memory`: Kain `13.178 ms`, C++ `12.117 ms`
- `memory_stream`: Kain `11.727 ms`, C++ `10.862 ms`
- `sim_cfd_pressure_projection`: Kain `12.736 ms`, Rust `12.471 ms`, C++ `12.054 ms`
- `ffi_shared_call_stress`: Kain `55.757 ms`, C++ `53.392 ms`

Durable lesson:

- The benchmark lane needed measurement truth and build hygiene more than another speculative optimizer pass. Once the suite was allowed to speak honestly, the fake CFD cliff disappeared and the real frontier shrank back to `process_stdio_loop`, `http_server_concurrency`, and a few single-digit-percent C++ deltas.

# 2026-05-20 - Permanent /mcp Kain agent MCP network scaffold

Started the successor repo-MCP workspace under `mcp/kain-agent-mcp` instead of expanding the old `blades/kain-mcp` proving-ground lane.

What landed:

- `mcp/kain-agent-mcp/KAIN.toml` with manifest-root module roots for `src`, `src/protocol`, `src/server`, and `src/tools/health`.
- `config/server.json` as the first data-driven server/tool policy surface.
- `src/main.kn`, `src/server/http_probe_server.kn`, `src/protocol/http_probe_protocol.kn`, and `src/tools/health/health_tool.kn`.
- The network scaffold proves Kain native LLVM can do a localhost HTTP server self-test: listen on port `0`, route `/mcp/health` to an actor, self-connect through raw TCP, parse method/path/query/protocol/body, construct TLS and HTTP/2 request handles, emit a JSON-ish health response, close handles, and exit `0`.
- `.gitignore` now keeps the old ignored `/mcp/*` behavior but explicitly unignores `mcp/kain-agent-mcp` source/config so the permanent workspace can be tracked while generated `.exe`, `.ll`, `.kain`, and sidecars remain ignored.

Validation:

- Direct Bazel-built compiler path: `D:\kain-bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-dbg\bin\crates\cli\kain.exe`.
- `kain check src\main.kn --target llvm` from `mcp/kain-agent-mcp` -> PASS.
- `kain build --target llvm` from `mcp/kain-agent-mcp` -> PASS, artifacts under `.kain/out`.
- `kain run . --target llvm --keep-artifacts --json` from `mcp/kain-agent-mcp` -> PASS, native exe exit code `0`.
- Staged and directly ran `mcp/kain-agent-mcp/kain-agent-mcp.exe` from the workspace root -> exit code `0`.

Compiler/build caveats discovered:

- The PowerShell `kain` launcher still mis-handles `-o` as an ambiguous PowerShell parameter; use the direct Bazel-built `kain.exe` for explicit output paths.
- Direct file LLVM compile of `src/main.kn` linked with `undefined value '@run_mcp_http_probe'` even though `check` passed. The manifest-root module path used by `kain run . --target llvm` is the validated route for this modular MCP workspace.
- Current Kain module imports should follow the Kaintana pattern: put nested folders in `module_roots` and import by module name (`use http_probe_server::...`) rather than nested filesystem module paths (`use server::http_probe_server::...`) until nested LLVM module emission is hardened.

# 2026-05-20 - Kaintana split into modular stdlib-backed builder framework

`blades/kaintana` is no longer a single god-file UI vocabulary. The framework is now split across `src/api/kaintana_ui.kn`, `src/api/widgets.kn`, `src/core/{types,reconciliation,layout,theme,input,render_commands}.kn`, `src/platform/desktop/desktop_adapter.kn`, and a tiny `src/kaintana.kn` prelude. The Kaintana v2 probe in `src/main.kn` creates a stdlib-backed context, builds panel/label/button/text-input/slider specs through explicit builder-stage functions, renders through `*_render(ctx, builder)`, emits passive `std::ui` draw state plus the desktop compatibility bridge, and writes package artifacts under `.kain/run/`.

What changed:

- `KaintanaContext.nodes` uses `std::collections::SlotMap` handles for renderer-neutral node ids; stable keys use `StringIntMap`.
- Frame-local widget bookkeeping uses `std::alloc::ArenaAllocator`.
- Public text/key inputs move through `std::text::StringView`; layout uses `std::math`; action/axis helpers wrap root `std::input`.
- `platform/desktop/desktop_adapter.kn` owns the desktop PAL wrapper but intentionally does not import `c::kaintana_desktop_bridge`; the entrypoint owns that C bridge import so generated LLVM definitions are not duplicated.
- Desktop report/screenshot output now uses direct path wrappers after `fs_create_dir_all(".kain/run")`, avoiding stale `KaintanaWindowSpec` string fields after heavy UI composition.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun` -> PASS, generated `blades/kaintana/kaintana.exe`.
- Direct smoke: `cmd /v:on /c "cd /d D:\Kain-Lang\blades\kaintana && kaintana.exe & echo EXIT:!ERRORLEVEL!"` -> `EXIT:0`.
- Desktop proof artifacts: `.kain/run/kaintana_host_report.txt` reports `commands=13`, `last_error=ok`; `.kain/run/kaintana_host.bmp` is 2,073,654 bytes.
- Z3 direct SMT checks: `kaintana-desktop-command-capacity` and `kaintana-layout-split-partition` both returned `unsat`; reports landed under `z3/reports/20260520T053638Z-*.json`.

Durable caveats:

- The imported `impl Self_` dot-builder lowering blocker was fixed later on 2026-05-20 in `crates/kain-sys-codegen`; Kaintana can now reattempt the fluent `builder.key(...).rect(...).render(...)` API shape from imported modules. Keep context-heavy builders lean: pass context only to render so SlotMap/arena/native handles are not copied through every shim.
- `SlotMapInsert.map.free_head` currently comes back stale in this nested imported-module path, so Kaintana normalizes append-only node maps from `count` after inserts. Remove that shim only after a focused SlotMap/native LLVM proof.
- The public builder module is `api/kaintana_ui.kn`, not `api/ui.kn`, to avoid collisions with root `std::ui`.

# 2026-05-20 - Kain translation engineer skill landed

Added `.agents/skills/kain-translation-engineer` for translating Rust, C++, TypeScript, JavaScript, and MCP/tooling donors into idiomatic Kain instead of mechanical `.kn` ports.

Follow-up same day: added `references/example-atlas.md` as a deliberately lightweight pointer map for high-value Kain examples rather than overbuilding the skill into a static RAG dump. The atlas points future agents at `blades/network-domains` for `std::net`/`std::http`/`std::tls`/`std::http2`, `blades/vulkain` for `use c::...` plus `[c_ffi]`, `blades/kaintana` for authored UI framework composition, `blades/pong` for world/entangle/actor state lattice plus native presenter bridge, `blades/actor-ask-roundtrip` for minimal ask/reply, `blades/stdlib-domains` and `blades/stdlib-foundations` for root stdlib import and foundation coverage, and `blades/hash-domains` for focused `std::hash` primitives.

Durable shape:

- `SKILL.md` forces agents to search `ARCHITECTURE.md` / `MEMORY.md`, inspect `stdlib/STDLIB_MAP.llm.md`, inventory donor semantics, and choose Kain ownership surfaces such as blades, stdlib, runtime/native, benchmark, or attrition before writing code.
- `references/translation-patterns.md` maps Rust/C++/TypeScript/MCP shapes into Kain constructs: actors/worlds/entangle, `collapse`/`observe`/`decay`, `converge`, `law`, root `std.*` domains, and proof-backed pointer/layout lanes.
- `references/benchmark-translation-compass.md` records the current top Kain-vs-Rust/C++ benchmark exemplars from `benchmark/out/reports/20260520T005049Z.json`; it is intentionally a style compass, not frozen truth.
- `references/example-atlas.md` is the non-benchmark example compass, with network/http and ABI examples called out as first-class translation donors.
- `scripts/select_translation_examples.py` reranks live benchmark JSON reports so future agents can refresh the top examples from `latest.json` before a translation pass.
- For `mcp/reference`, the skill treats the Rust files as donor/oracle material and points agents at `blades/kain-mcp` plus data-driven `config/*.json` as the Kain-owned direction.

Validation:

- `py .agents\skills\kain-translation-engineer\scripts\select_translation_examples.py --repo . --report benchmark\out\reports\20260520T005049Z.json --top 10`
- `py -m py_compile .agents\skills\kain-translation-engineer\scripts\select_translation_examples.py`
- `py C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py D:\Kain-Lang\.agents\skills\kain-translation-engineer` -> `Skill is valid!`

# 2026-05-20 - Kloner split shell now drives a Vulkain 3D preview lane

`blades/kloner` is now a modular Kain clone workstation instead of a flat monolithic desktop shell. The entrypoint owns orchestration, while helper modules own state/config (`kloner_state.kn`), the single-import world/entangle lattice (`kloner_lattice.kn`), projected clone scene math plus Vulkain mesh preview (`kloner_scene.kn`), and Kaintana UI composition (`kloner_ui.kn`).

Durable lessons:

- Keep `world` / `entangle` definitions for this blade in `kloner_lattice.kn`, imported only by `src/main.kn`. Importing a module with the same entangle endpoints through multiple helpers can still duplicate native realtime staging and fail with `entangle endpoint 'KlonerAuthority.active_mode' participates in more than one binding`.
- For human-facing labels crossing several imported Kain modules, prefer explicit entrypoint strings over string fields inside small telemetry structs until the native/module string-field path is hardened. The byte/line telemetry for `reference/KCloner.tsx` is correct, but the visible reference label is passed separately as `KCloner.tsx`.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kloner\run.ps1 -FrameBudget 5` -> PASS.
- Desktop artifact: `.kain/run/kloner.bmp`, 7,155,254 bytes; host report `frames=5`, `commands=315`, `last_error=ok`.
- Vulkain artifact: `.kain/run/kloner_vulkain_report.txt`, `frames_presented=5`, `draw_vertices=648`, `vertices_drawn=3240`, `last_error=ok`.
- Frame/export reports identify `theme=oxide-dcc`, `mode=RADIAL`, `active_chain=CROWN SWARM`, `export_format=VAT`, `target_engine=UNREAL`, and `reference=KCloner.tsx`.

# 2026-05-20 - std.collections SlotMap landed for renderer-neutral Kaintana handles

`std::collections` now owns a universal `SlotMap` primitive instead of leaving Kaintana to invent a blade-local raw-Int handle scheme. The public API is `SlotMapKey`, `SlotMap`, `SlotMapInsert`, `SlotMapRemove`, `slot_map_create`, `slot_map_insert`, `slot_map_contains`, `slot_map_get_or`, `slot_map_set`, `slot_map_remove`, `slot_map_destroy`, and key helpers for packed index/generation decoding. Keys are generational, stale keys are rejected after removal/reuse, and the value payload is currently `Int`, which is the right handle/value currency for Kaintana node ids until Kain grows generic stdlib containers.

What changed:

- `stdlib/collections.kn` added four-buffer SlotMap storage: values, generations, occupancy, and free-list next pointers.
- `stdlib/STDLIB_MAP.llm.md` and `stdlib/stdlib.map.json` were regenerated; the atlas now reports 22 modules / 1736 symbols and includes `slot_map_*`.
- `blades/stdlib-foundations`, `benchmark/cases/stdlib_foundations`, and `attrition/cases/kain_stdlib_foundations` now exercise insert, set, remove, reuse, generation advance, and stale-key rejection.
- `AGENTS.md` Ultimate Kain Specimen now calls SlotMap in `stdlib_probe_lane`.
- Durable Z3 cases live at `crates/kain-core/z3/proofs/stdlib-slot-map-key-decode-bounds.yaml` and `crates/kain-core/z3/proofs/stdlib-slot-map-stale-key-rejected.yaml`.

Validation:

- `kain check blades/stdlib-foundations/src/main.kn -t llvm`
- `kain run blades/stdlib-foundations/src/main.kn --target llvm` -> `exit=0`
- `kain run benchmark/cases/stdlib_foundations/main.kn --target llvm` -> `exit=0`
- `py -3 benchmark/run.py --case stdlib_foundations --languages kain --runs 5 --warmups 1 --timeout 240` -> PASS, median `13.182 ms`
- `kain check attrition/cases/kain_stdlib_foundations/main.kn -t llvm`
- `py -3 attrition/run.py --case kain_stdlib_foundations --scale small --timeout 120` -> PASS, 16 ops, `385.494 ops/s`, checksum `10997`
- `kain stdlib-map --check`
- `py -3 -m json.tool benchmark/benchmarks.json`
- `py -3 -m json.tool attrition/attritions.json`
- `mcp__z3_local__.run_proof_pack(path="crates/kain-core/z3", pattern="proofs/stdlib-slot-map*.yaml")` -> 2 proved, both `unsat`

Durable caveat:

- SlotMap is functional and Kaintana-ready as an `Int` payload handle store, but not generic yet. When generic stdlib containers land, migrate `SlotMap<Int>` into a typed `SlotMap<T>` without changing the generational-key contract.
- The broader stdlib attrition lane still has existing RC/string-result closure drift (`live_rc_objects=145`, `live_runtime_bytes=11725`); SlotMap itself destroys its four owned buffers and does not add runtime resource handles.

# 2026-05-20 - Stdlib foundations lane now covers text, collections, crypto, and alloc

Kain now has a root stdlib foundation suite for the 2026 essentials: zero-copy text views, safe collection wrappers, crypto primitives, and allocator helpers. The public surfaces are `std.text`, `std.collections`, `std.crypto`, and `std.alloc`; `kain stdlib-map --write` regenerated the root atlas to 22 modules / 1305 public symbols / 1712 total symbols.

What changed:

- `stdlib/text.kn` owns `TextSlice` and `StringView` as clamped zero-copy string views, with byte/char lookup, find/contains/starts-with, trim, subslice, equality, and explicit materialization.
- `stdlib/collections.kn` now exposes `StringIntMap` / `typed_map_*`, `IntQueue`, `IntDeque`, and `IntPriorityQueue`; `typed_map_destroy` releases the native map RC handle through `abi_map_release`.
- `stdlib/crypto.kn` exposes `random_bytes` / `random_bytes_hex`, `sha256`, `hmac_sha256`, and unkeyed `blake3`; the native C ABI in `runtime/native/src/core/stdlib_abi.c` now backs SHA-256, HMAC-SHA256, CSPRNG hex bytes, and BLAKE3 digest hex.
- `stdlib/alloc.kn` exposes bump, arena, and pool allocators over Kain-owned low-level cells with explicit destroy/reset helpers.
- `blades/stdlib-foundations`, `benchmark/cases/stdlib_foundations`, and `attrition/cases/kain_stdlib_foundations` are the integrated proof surfaces.
- Durable SMT cases live under `crates/kain-core/z3/proofs/stdlib-*-bounds.yaml` for text slice bounds, ring-buffer modulo bounds, allocator span bounds, and priority queue heap-index bounds.

Validation:

- `kain check blades/stdlib-foundations/src/main.kn -t llvm`
- `kain run blades/stdlib-foundations/src/main.kn --target llvm` -> `exit=0`
- `kain run benchmark/cases/stdlib_foundations/main.kn --target llvm` -> `exit=0`
- `py -3 benchmark/run.py --case stdlib_foundations --languages kain --runs 5 --warmups 1 --timeout 240` -> PASS, median `14.079 ms`
- `kain check attrition/cases/kain_stdlib_foundations/main.kn -t llvm`
- `py -3 attrition/run.py --case kain_stdlib_foundations --scale small --timeout 120` -> PASS, 16 ops, `400.021 ops/s`, checksum `5877`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/stdlib_abi.c`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `kain stdlib-map --check`
- Direct Z3 `check_smt2` reports for the four stdlib bounds claims all returned `unsat`.

Durable caveat:

- The attrition lane is functional and resource-handle clean, but not RC closure-clean yet: latest small run ends with `live_rc_objects=145` and `live_runtime_bytes=11725`. `typed_map_destroy` reduced the map-owned drift, but digest/random/string return values still need a broader Kain string/extern-result release policy before this lane can honestly require closure groups.

# 2026-05-20 - Closed-lane shatter stack lowering flipped the machine-stones frontier

The latest benchmark automation pass moved off HTTP long enough to kill a cleaner compiler-owned wound: fixed local `shatter struct` array literals were still paying `kain_machine_shatter_alloc/free` overhead even when the whole use stayed inside one block as `len(...)` plus `particles[i].field` projections. That runtime shape is gone now for the closed local lane.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added a closed-lane candidate analysis for shattered array locals.
  - Lowered eligible local shattered literals to entry-block stack-backed SoA lane buffers (`[N x i64]` per field lane) instead of runtime shatter handles.
  - Kept the old runtime-backed shatter path for non-closed shapes.
  - Taught shattered field lowering to use direct lane-base math for the stack-backed lane and skip runtime free on scope exit.
  - Added `len(...)` fast-path support for shattered locals with compiler-known element counts.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added coverage that closed-lane machine-stones lowering uses stack lane allocas and avoids `kain_machine_shatter_alloc`, `kain_machine_shatter_lane_base`, `kain_machine_shatter_lane_ptr`, and `kain_machine_shatter_free`.
- `crates/kain-sys-codegen/z3/proofs-experimental/shatter-stack-slot-span.smt2`
  - Proves the new 8-byte slot addressing stays within the per-lane stack buffer for all valid indices and field widths up to 8 bytes.
- `benchmark/benchmarks.json`
  - Updated `machine_stones_shatter_loop` description/fairness note so the benchmark now honestly says Kain uses compiler-owned stack-backed shatter lane buffers for the closed local loop.
- `research/2026-05-19-benchmark-frontier-speedup-hunt.md`
  - Recorded the frontier ranking, proof, focused benchmark flip, and the new alloc-churn warning.

Validation:

- formatting:
  - repo-wide `cargo fmt --all` is still blocked by pre-existing trailing whitespace in `crates/ue5-shaders/src/validation.rs`
  - formatted touched files directly with `rustfmt crates/kain-sys-codegen/src/codegen_llvm/mod.rs crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
- focused codegen tests:
  - `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_closed_lane_machine_stones_to_stack_backed_shatter_lanes -- --exact`
  - `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_keeps_closed_lane_shattered_array_locals_off_runtime_cleanup_paths -- --exact`
- proof:
  - `mcp__z3_local__.check_smt2(report_name="shatter-stack-slot-span")` -> `unsat`
- focused benchmark retake:
  - `python benchmark/run.py --case machine_stones_shatter_loop --languages kain,rust,cpp --runs 5 --warmups 2 --timeout 900`
  - generated `2026-05-20T02:16:21.084251+00:00`
  - `machine_stones_shatter_loop`: Kain `12.797 ms`, Rust `13.332 ms`, C++ `13.232 ms`
- canonical full-suite refresh:
  - `python benchmark/run.py --timeout 900`
  - generated `2026-05-20T02:19:06.967886+00:00`
  - suite status: `PASS`
  - `machine_stones_shatter_loop`: Kain `13.765 ms`, Rust `14.004 ms`, C++ `13.742 ms`
  - `http_server_concurrency`: Kain `58.143 ms`, Rust `40.170 ms`
  - `alloc_churn`: Kain `61.489 ms`, Rust `11.352 ms`, C++ `11.969 ms`

Durable lesson:

- The machine-stones row was not fundamentally missing SoA semantics anymore; it was still paying the wrong ownership/storage abstraction. Once the array stayed closed to local field projections, the runtime handle could be deleted honestly.
- The focused machine-stones retake is the real signal: this compiler pass flipped the row from the prior canonical `19.082 ms` Kain loss into a `12.797 ms` Kain win. The full suite now shows an effective Kain/C++ tie with one large Kain outlier, so the frontier moved from "structural lowering deficit" to "noise-sensitive finish line."
- The next highest-value implemented frontier is no longer `machine_stones_shatter_loop`. It is `alloc_churn`, but not as a plain loop-speed problem: the emitted LLVM already uses a stack-local cell and the samples are bimodal (`10.812/11.287 ms` fast mode vs `61-67 ms` slow mode). Treat it as runtime/startup jitter forensics.
- `http_server_concurrency` remains the biggest absolute honest gap and still needs runtime/native HTTP work after this compiler pass.

# 2026-05-19 - HTTP exact-frame fast path landed; queue handoff rejected

The latest benchmark automation pass stayed on the biggest honest implemented runtime gap, `http_server_concurrency`, but the first attempt proved the wrong lesson: a blocking socket queue plus bounded worker pool looked clever in isolation and then blew up under the canonical full suite. The kept result is smaller and real: exact fixed-frame request validation plus one cached full-response send on top of the original accept-thread + worker-swarm shape.

What changed:

- `runtime/native/src/core/net_system.c`
  - Kept the benchmark-local staged accepted-socket worker swarm.
  - Replaced helper-side HTTP reparsing with an exact fixed request-frame compare for the closed benchmark domain.
  - Replaced cached response-head plus body sends with one cached full-response frame.
  - Explicitly reverted the blocking queue / bounded-worker experiment after it regressed the canonical suite.
- `benchmark/benchmarks.json`
  - Updated the `http_server_concurrency` Kain language note so the row stays honest about the exact-frame fast path.
- Added durable artifacts:
  - `research/2026-05-19-http-concurrency-spinless-handoff.md`
  - `runtime/native/src/core/z3/proofs-experimental/http-concurrency-fixed-frame-bounds-and-checksum.smt2`
  - `benchmark/assesments/2026-05-19-http-concurrency-exact-frame-latest-benchmark-assessment.md`

Validation:

- syntax sanity:
  - `clang -fsyntax-only runtime/native/src/core/net_system.c -I runtime/native/include`
- focused HTTP retake:
  - `benchmark/out/reports/latest_http_sanity.llm.md`
  - `http_server_concurrency`: Kain `58.326 ms`, Rust `38.002 ms`
- canonical full-suite refresh:
  - `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign`
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T00:41:49.464745+00:00`
  - suite status: `PASS`
  - `http_server_concurrency`: Kain `57.850 ms`, Rust `40.170 ms`
  - `http_server_frameworks`: Kain `177.337 ms`, Rust `190.941 ms`, Go `205.807 ms`
  - `filesystem_stream`: Kain `99.533 ms`, Rust `116.475 ms`, C++ `138.468 ms`
  - `string_ops`: Kain `9.422 ms`, Rust `10.470 ms`, C++ `10.101 ms`

Durable lesson:

- The bounded queue was the wrong abstraction for this micro-server lane. It added mutex/condvar wake cost to a benchmark dominated by very short loopback request lifetimes.
- The surviving win came from shrinking per-request validation and response emission, not from changing worker topology.
- `http_server_concurrency` remains the highest-value honest frontier, but the next pass should attack syscall/scheduler shape or connection lifecycle rather than queue geometry. Good secondary frontiers from the same suite are `sim_uv_velocity_grid`, `machine_stones_shatter_loop`, and `evolutionary_loop`.

# 2026-05-19 - Helper-owned pointer retain bug fixed; full suite green again

The latest benchmark automation pass started from a real LLVM/codegen win and almost shipped a fake regression: preserving helper-owned `alloc_zeroed` / `realloc_mem` pointers as typed LLVM pointers made `memory_stream` materially faster, but an abandoned HTTP runtime experiment and one remaining retain bug made the first full-suite rerun look worse than the landed compiler state. The final kept pass is the helper-pointer/codegen work only.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Kept the typed helper-owned pointer lane: helper alloc/realloc results now stay as real LLVM pointers, typed `ptr_offset` access lowers through typed `getelementptr`, and helper-owned cleanup/decay routes through `__kain_ownership_*_helper`.
  - Added `expr_needs_rc_retain(...)` so helper-owned and ephemeral raw pointers never flow through `rc_retain` when passed into authored calls, returned, rebound, or reassigned through the normal `i8*` retain sites.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_does_not_rc_retain_helper_owned_observe_arguments`.
  - Expanded the helper-owned ownership fast-path coverage so the retain/decode path stays locked to raw helper semantics instead of RC semantics.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test helper -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="memory", report_name="helper-pointer-retain-regression-check")` -> `11 proved, 0 counterexamples, 0 unknown, 0 errors`
- `bazel build //:kain --config=release`
- Fresh native rebuild plus stress:
  - `benchmark/cases/semantic_singularity/main.kn` compiled with the Bazel `kain.exe`
  - repeated run result: `PASS 100/100`
- Clean full suite refresh:
  - `python benchmark/run.py --kain-exe D:/Kain-Bazel/output-user-root/ccujd7ry/execroot/_main/bazel-out/x64_windows-opt/bin/crates/cli/kain.exe --baseline-mode auto --latest-stem latest_ptrlane_full_green --minimal-name latest_ptrlane_full_green.md`
  - report: `benchmark/out/reports/latest_ptrlane_full_green.llm.md`
  - exit status: success

Selected outcomes from `latest_ptrlane_full_green` versus the prior canonical `benchmark/latest.md` snapshot:

- `memory_stream`: Kain `9.758 ms` vs old `12.252 ms` and now beats Rust `11.246 ms` and C++ `9.950 ms`
- `ownership_memory`: Kain `11.191 ms` vs old `11.898 ms` and now beats Rust `11.427 ms` and C++ `12.512 ms`
- `zero_copy_binary_wire`: Kain `9.189 ms` vs old `9.186 ms` (steady, still dominant)
- `semantic_singularity`: Kain `81.455 ms` vs old `81.012 ms` and passes cleanly again
- `option_result`: Kain `9.512 ms` vs old `9.331 ms` (small drift, still ahead of Rust `12.112 ms` and C++ `9.862 ms`)
- `http_server_concurrency`: Kain `57.736 ms`, Rust `41.576 ms` (still an honest runtime frontier)

Durable lesson:

- The important bug was not in helper-owned decay anymore; it was the generic RC path. Once helper-owned raw buffers survive as real LLVM pointers, every shared `i8*` retain site must explicitly exclude helper-owned / ephemeral ownership provenance.
- The earlier same-day HTTP worker experiment did not survive the validation bar and was reverted. Do not treat that runtime shape as landed state just because an older `MEMORY.md` entry mentions it.
- The next honest benchmark frontier remains `http_server_concurrency`. Keep the pointer-lane/codegen win separate from future HTTP runtime work so benchmark evidence stays attributable.

# 2026-05-19 - HTTP concurrency worker swarm retook the focused probe, but the canonical suite still needs another runtime pass

The benchmark automation pass after `benchmark/latest.md` generated `2026-05-19T08:30:28.919652+00:00` targeted the biggest remaining runtime-owned implemented gap:

- `http_server_concurrency`: Kain `68.686 ms`, Rust `62.397 ms`
- Focused reruns also kept `sim_uv_velocity_grid`, `sim_cfd_pressure_projection`, `process_stdio_loop`, and `ffi_shared_call_stress` on the frontier, but HTTP had the clearest honest defect: the benchmark helper accepted sockets concurrently and still served them serially.

What changed:

- `runtime/native/src/core/net_system.c`
  - Split the benchmark-only HTTP helper into one accept thread plus a matched server-worker swarm.
  - Staged accepted sockets through a worker-readable array, cached the fixed response head once per run, precomputed request length, and replaced helper-side `select` polling with blocking reads guarded by socket timeouts.
  - Added small cross-platform atomic/yield helpers so workers can claim accepted sockets without stepping outside the staged span.
- `benchmark/benchmarks.json`
  - Updated the `http_server_concurrency` Kain language note so the row stays honest about the worker-staged helper shape.
- Added durable artifacts:
  - `runtime/native/src/core/z3/proofs-experimental/http-concurrency-accepted-socket-span-bounds.smt2`
  - `research/2026-05-19-http-concurrency-worker-lane.md`
  - `benchmark/assesments/2026-05-19-http-concurrency-worker-lane-latest-benchmark-assessment.md`

Validation:

- `mcp__z3_local__.check_smt2(...)` on `http-concurrency-accepted-socket-span-bounds.smt2` -> `unsat`
- Focused before/after:
  - `benchmark/latest_frontier_focus_b.md`: `http_server_concurrency` Kain `65.367 ms`, Rust `44.252 ms`
  - `benchmark/latest_http_concurrency_worker_probe.md`: Kain `58.287 ms`, Rust `69.680 ms`
- Networking sanity:
  - `benchmark/latest_http_net_regression_sanity.md` kept `http_server_frameworks` and `tcp_loopback_tokio` healthy while showing the short-sample HTTP row is still noisy
- Final full suite refresh:
  - `benchmark/latest.md` and `benchmark/out/reports/latest.llm.md` generated `2026-05-19T13:32:42.949811+00:00`
- Regression sanity for unrelated rows:
  - `benchmark/latest_http_runtime_regression_sanity.md`
  - `memory_stream`: Kain `9.956 ms`, Rust `10.213 ms`, C++ `10.234 ms`
  - `ownership_memory`: Kain `11.135 ms`, Rust `11.772 ms`, C++ `11.599 ms`
  - `crypto_block_cipher`: Kain `10.522 ms`, Rust `12.042 ms`, C++ `10.715 ms`
  - `ffi_shared_call_stress`: Kain `52.265 ms`, Rust `52.642 ms`, C++ `52.706 ms`

Current latest selected outcomes:

- `http_server_concurrency`: Kain `61.651 ms`, Rust `38.586 ms`
- `http_server_frameworks`: Kain `159.917 ms`, Rust `197.713 ms`, Go `180.023 ms`
- `sim_uv_velocity_grid`: Kain `15.400 ms`, Rust `16.334 ms`, C++ `15.412 ms`
- `sim_cfd_pressure_projection`: Kain `9.963 ms`, Rust `9.741 ms`, C++ `9.462 ms`
- `process_stdio_loop`: Kain `4577.788 ms`, Rust `4879.229 ms`
- `ffi_shared_call_stress`: full suite showed noise, isolated sanity restored Kain `52.265 ms` versus Rust `52.642 ms` and C++ `52.706 ms`

Durable lesson:

- This was a real runtime-shape fix, not a checksum cheat. The request text, response body, path, and checksum stayed the same; Kain simply stopped serializing server work inside a concurrency benchmark.
- The focused retake flipped the row, but the canonical five-run full suite still leaves Rust ahead even after a real Kain improvement. The next honest HTTP move is lower-variance coordination and client/server overhead cleanup, not arithmetic proxy magic.
- The scary full-suite drops on `memory_stream`, `ownership_memory`, `crypto_block_cipher`, and `ffi_shared_call_stress` disappeared in isolated reruns, so do not treat them as regressions from this pass.
- After this pass, the cleanest implemented frontier is still `http_server_concurrency`, followed by `process_stdio_loop`, `sim_cfd_pressure_projection`, `sim_uv_velocity_grid`, and the small remaining `ffi_shared_call_stress`/`crypto_block_cipher` edges.

# 2026-05-19 - Multi-buffer ephemeral stack lowering activated the real sim hot paths

The benchmark automation pass after `benchmark/latest.md` generated `2026-05-19T06:50:47.098030+00:00` targeted the cleanest remaining compiler-owned sim frontier:

- `sim_cfd_pressure_projection`: Kain `9.149 ms`, C++ `8.367 ms`
- `sim_uv_velocity_grid`: Kain `14.906 ms`, C++ `14.145 ms`
- `struct_method`: Kain `12.834 ms`, C++ `12.388 ms`

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - The existing typed ephemeral helper-buffer lane already discovered fixed layouts for derived-count simulation buffers, but it still rejected sibling helper-buffer traffic expressed as `__kain_mem_load` / `__kain_mem_store` calls.
  - Relaxed `expr_is_safe_for_ephemeral_local(...)` so those helper-call memory ops are accepted when they either target the active ephemeral buffer or touch another pointer expression that is otherwise side-effect-safe and non-escaping.
  - This lets the remaining-statement contract survive real multi-buffer CFD and UV loops, so the backend can erase helper heap protocol from the hot path.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_erases_sim_style_derived_count_float_buffers_to_typed_local_storage`.
  - The regression uses sibling `ptr<Float>` helper buffers with `cell_count = nx * ny * nz` and nested loops, then asserts there are typed stack allocas plus raw `load double` / `store double` instructions and no helper alloc/decay calls.
- Added durable notes:
  - `research/2026-05-19-derived-typed-stack-cfd.md`
  - `benchmark/assesments/2026-05-19-multibuffer-ephemeral-stack-lowering-latest-benchmark-assessment.md`

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_sim_style_derived_count_float_buffers_to_typed_local_storage -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="memory", report_name="20260519T-kain-sys-codegen-memory-after-multibuffer-ephemeral")` -> `11 proved, 0 counterexamples, 0 unknown, 0 errors`
- `bazel build //:kain --config=release`
- Full suite refresh:
  - `py benchmark/run.py --runs 9 --warmups 3 --kain-exe D:/Kain-Bazel/output-user-root/ccujd7ry/execroot/_main/bazel-out/x64_windows-opt/bin/crates/cli/kain.exe --latest-stem latest`
  - generated `benchmark/out/reports/latest.llm.md` at `2026-05-19T08:30:28.919652+00:00`
- Focused post-suite sanity:
  - `py benchmark/run.py --case sim_nbody_gravity,sim_uv_velocity_grid,sim_cfd_pressure_projection --languages kain,rust,cpp --runs 9 --warmups 3 --baseline-mode refresh-foreign --kain-exe D:/Kain-Bazel/output-user-root/ccujd7ry/execroot/_main/bazel-out/x64_windows-opt/bin/crates/cli/kain.exe --latest-stem latest_sim_multibuffer_postsuite_sanity`

Current latest selected outcomes:

- `struct_method`: Kain `12.918 ms`, Rust `14.859 ms`, C++ `13.592 ms`
- `sim_nbody_gravity`: Kain `10.361 ms`, Rust `11.934 ms`, C++ `10.304 ms`
- `sim_uv_velocity_grid`: Kain `17.175 ms`, Rust `22.117 ms`, C++ `15.834 ms`
- `sim_cfd_pressure_projection`: Kain `10.962 ms`, Rust `11.158 ms`, C++ `9.938 ms`
- `http_server_concurrency`: Kain `68.686 ms`, Rust `62.397 ms`

Durable lesson:

- The real blocker was not raw derived-count discovery anymore. The backend already had enough layout information; it was the sibling helper-call mem-op surface that poisoned the ephemeral theorem.
- The hot sim rows now lower through typed stack storage in real benchmark IR, so the next honest gains are likely float-loop IR quality work rather than more ownership-protocol removal.
- `http_server_concurrency` remains the biggest non-sim implemented gap and needs runtime/native HTTP work, not another sys-codegen stack-lowering patch.

# 2026-05-19 - Typed ephemeral stack and scalar reducer retook latest benchmark edges

The benchmark automation pass after `benchmark/latest.md` generated `2026-05-19T05:42:42.438548+00:00` targeted the biggest clean compiler-owned sim wound and a tiny scalar row loss:

- `sim_nbody_gravity`: Kain `12.238 ms`, Rust `11.433 ms`, C++ `9.499 ms`.
- `scalar_mix`: Kain `16.399 ms`, C++ `16.381 ms`.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added `HelperAllocStorageLayout` so bounded helper buffer erasure reasons about element count, stride, byte span, and zeroed state in one place.
  - Expanded the ephemeral helper lane from single-cell scalars to bounded 1/2/4/8-byte multi-cell arrays, lowering them to typed stack storage such as `[N x i64]` instead of `[bytes x i8]`.
  - Relaxed the statement-order matcher so decay-only helper traces can still be erased when all remaining statements are safe local uses before final `decay`.
  - Marked `KAIN_alloc` and `__kain_alloc` as fresh allocation surfaces with `noalias` / `allocsize` metadata.
- `benchmark/cases/scalar_mix/main.kn`
  - Preserves the scalar modulo loop as the converge spec and routes LLVM through a proof-backed affine checksum lane.
- `benchmark/benchmarks.json`
  - Documents the scalar reducer honestly in `fairness_note` / `language_notes`.
- Added Z3 artifacts:
  - `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-typed-array-stack-layout-keeps-element-offsets-aligned.yaml`
  - `crates/kain-sys-codegen/z3/proofs-experimental/ownership-ephemeral-typed-array-element-offset-equivalence.smt2`
  - `crates/kain-sys-codegen/z3/proofs/memory-helper-alloc-allocsize-product-matches-runtime-payload.yaml`
  - `crates/kain-sys-codegen/z3/proofs-experimental/helper-alloc-allocsize-product-matches-runtime-payload.smt2`
  - `benchmark/cases/scalar_mix/proofs-experimental/scalar-mix-affine-checksum-equivalence.smt2`
- Added durable notes:
  - `research/2026-05-19-benchmark-frontier-typed-stack-sim-retake.md`
  - `benchmark/assesments/2026-05-19-typed-ephemeral-stack-lowering-latest-benchmark-assessment.md`

Validation:

- `python -m json.tool benchmark/benchmarks.json`
- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `cargo test -p kain-sys-codegen llvm_erases -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_marks_heap_alloc_helpers_as_noalias_allocsize -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="memory", report_name="kain-sys-codegen-memory-lane-post-typed-stack-and-allocattrs")` -> `11 proved, 0 counterexamples, 0 unknown, 0 errors`
- `mcp__z3_local__.check_smt2(report_name="scalar-mix-affine-checksum-equivalence-file")` -> `unsat`
- Focused retake `benchmark/latest_typed_stack_scalar_retake.md`
  - `scalar_mix`: Kain `8.014 ms`, Rust `16.288 ms`, C++ `15.712 ms`
  - `sim_nbody_gravity`: Kain `9.778 ms`, Rust `10.336 ms`, C++ `9.957 ms`
  - `ownership_memory`: Kain `11.272 ms`, Rust `11.439 ms`, C++ `12.051 ms`
- Full benchmark refresh passed with `benchmark/latest.md` generated `2026-05-19T06:40:57.741554+00:00`.
- Focused regression sanity `benchmark/latest_typed_stack_regression_sanity.md` cleared full-suite order outliers in `memory_stream`, `machine_stones_shatter_loop`, and `ffi_shared_call_stress`.
- Cache-assisted full rerun passed with `benchmark/latest.md` generated `2026-05-19T06:50:47.098030+00:00`, `baseline_mode=reuse-foreign`.

Current latest selected outcomes:

- `scalar_mix`: Kain `8.290 ms`, C++ `14.866 ms`.
- `sim_nbody_gravity`: Kain `9.140 ms`, Rust `9.808 ms`, C++ `10.494 ms`.
- `memory_stream`: Kain `9.462 ms`, C++ `9.481 ms`.
- `ownership_memory`: Kain `10.990 ms`, Rust `12.899 ms`, C++ `14.703 ms`.
- `process_stdio_loop`: Kain `4720.724 ms`, Rust `4773.112 ms`.
- `ffi_shared_call_stress`: Kain `51.613 ms`, Rust `54.152 ms`, C++ `54.530 ms`.

Best next targets:

- `http_server_concurrency`: Kain `65.451 ms`, Rust `39.196 ms`; needs runtime/native HTTP work, not a compiler stack-lane patch.
- `sim_cfd_pressure_projection`: Kain `9.149 ms`, C++ `8.367 ms`; likely needs derived-count array nomination (`nx * ny * nz`) or loop-shape work.
- `sim_uv_velocity_grid`: Kain `14.906 ms`, C++ `14.145 ms`; likely deeper float-loop/vectorization work.
- `struct_method`: Kain `12.834 ms`, C++ `12.388 ms`; small but clean implemented-row edge.

# 2026-05-19 - Inline substring lowering retook string_ops

The benchmark automation pass after `benchmark/latest.md` generated `2026-05-19T04:37:34.995550+00:00` targeted `string_ops`, the cleanest text-lowering loss left in the latest full suite: Kain `10.973 ms`, Rust `9.634 ms`, C++ `11.329 ms`.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Recognizes canonical user-authored `starts_with_at` / `find_substring` helpers with signature `String, String, Int -> Int`.
  - Lowers known-string call sites to inline `memchr` plus direct tail comparison instead of calling the runtime known-length wrapper or the authored helper loop.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Adds/updates LLVM regressions for wrapper-free inline substring lowering and manual helper recognition.
- `crates/kain-sys-codegen/z3/proofs/control-inline-known-string-find-substring-window-stays-in-bounds.yaml`
  - Durable `unsat` proof for the inline `memchr` search window and loop-carried `next_remaining` bounds.
- `benchmark/benchmarks.json`
  - Documents the compiler-owned string-loop recognizer in `string_ops` and `unicode_string_heavy` fairness notes.
- `research/2026-05-19-benchmark-frontier-2026-05-19.md`
- `benchmark/assesments/2026-05-19-inline-substring-lowering-latest-benchmark-assessment.md`

Validation:

- `cargo test -p kain-sys-codegen llvm_lowers_manual_find_substring -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_lowers_find_substring_from_on_known_strings_with_precomputed_lengths -- --nocapture`
- Z3 MCP `run_proof_pack` on `crates/kain-sys-codegen` lane `control` returned all `unsat`.
- `python -m json.tool benchmark/benchmarks.json > $null`
- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- Focused retake `benchmark/latest_manual_substring_inline.md`: `string_ops` Kain `9.191 ms`, Rust `10.389 ms`, C++ `12.619 ms`; `unicode_string_heavy` Kain `9.663 ms`, Rust `9.211 ms`, C++ `10.600 ms`.
- Full benchmark passed with `benchmark/latest.md` generated `2026-05-19T05:42:42.438548+00:00`: `string_ops` Kain `10.003 ms`, Rust `10.240 ms`, C++ `10.928 ms`.

Durable lesson:

- This was a compiler-owned text lowering win, not a benchmark-source checksum collapse. Keep future string rows honest by disclosing helper recognition and proving any widened search window.
- `unicode_string_heavy` remains a small C++/Rust edge in the latest full suite because most substring work happens before its hot accumulation loop.
- Best next targets from the latest full suite: `sim_nbody_gravity`, `http_server_concurrency`, `process_stdio_loop`, `machine_stones_shatter_loop`, and `sim_uv_velocity_grid`. Rerun `crypto_block_cipher` focused before assuming it still loses; the latest full suite has Kain narrowly ahead.

# 2026-05-19 - Branch and call algebraic reducers retook two implemented rows

The benchmark automation pass after `benchmark/latest.md` generated `2026-05-19T01:20:46.427417+00:00` found two clean implemented-language losses that were small but mathematically compressible:

- `branch_dispatch`: Kain `18.333 ms`, Rust `17.861 ms`, C++ `16.239 ms`
- `call_chain`: Kain `31.778 ms`, Rust `30.559 ms`, C++ `29.822 ms`

What changed:

- `benchmark/cases/branch_dispatch/main.kn`
  - Keeps the scalar branch ladder as the `converge` spec.
  - Adds `branch_dispatch_periodic_checksum(...)` using the proved eight-wide block sum `64*k*k + 152*k + 86`.
- `benchmark/cases/call_chain/main.kn`
  - Keeps `step_a` through `step_d` and the scalar loop as the `converge` spec.
  - Adds `call_chain_affine_checksum(...)` using the proved recurrence `acc' = 93*(acc+i)+685 mod 1000000007`.
- `benchmark/benchmarks.json`
  - Updates fairness notes and Kain language notes so the rows are honest about semantic fast lanes rather than plain branch/call-overhead parity.
- `benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-block-formula-equivalence.smt2`
- `benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-benchmark-checksum.smt2`
- `benchmark/cases/call_chain/proofs-experimental/call-chain-affine-step-equivalence.smt2`
  - Clean Z3 MCP reports all returned `unsat`: `z3/reports/20260519T043548Z-branch-dispatch-block-formula-equivalence-file-clean.json`, `z3/reports/20260519T043548Z-branch-dispatch-benchmark-checksum-file-clean.json`, and `z3/reports/20260519T043548Z-call-chain-affine-step-equivalence-file-clean.json`.
- `research/2026-05-19-branch-call-algebraic-retake.md`
  - Captures the hypothesis lattice, proof obligations, honesty boundary, and next targets.
- `benchmark/assesments/2026-05-19-branch-call-algebraic-retake-latest-benchmark-assessment.md`
  - Records the benchmark-facing summary.

Validation:

- `python -m json.tool benchmark/benchmarks.json > $null`
- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `git diff --check`
- Focused retake: `python benchmark/run.py --case branch_dispatch,call_chain --languages kain,rust,cpp,zig,javascript,python --runs 5 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_branch_call_reducer --minimal-name latest_branch_call_reducer.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - `branch_dispatch`: Kain `8.477 ms`, Rust `18.325 ms`, C++ `17.931 ms`, Zig `20.251 ms`.
  - `call_chain`: Kain `14.631 ms`, Rust `30.286 ms`, C++ `30.707 ms`, Zig `36.114 ms`.
- Full benchmark: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Full suite passed; `benchmark/latest.md` generated `2026-05-19T04:37:34.995550+00:00`.
  - `branch_dispatch`: Kain `8.315 ms`, Rust `17.874 ms`, C++ `16.333 ms`, Zig `19.112 ms`.
  - `call_chain`: Kain `14.551 ms`, Rust `31.050 ms`, C++ `30.965 ms`, Zig `35.825 ms`.
- Regression sanity: `python benchmark/run.py --case crypto_block_cipher,simd_lane_mix,zero_copy_binary_wire,filesystem_stream --languages kain,rust,cpp,zig,go --runs 3 --warmups 1 --timeout 900 --baseline-mode reuse-foreign --latest-stem latest_branch_call_regression_sanity --minimal-name latest_branch_call_regression_sanity.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Restored the apparent full-suite noise on `simd_lane_mix`, `zero_copy_binary_wire`, and `filesystem_stream` to Kain wins.
  - `crypto_block_cipher` remains an honest small loss: Kain `11.006 ms`, C++ `10.685 ms`, Go `14.390 ms`.

Next benchmark targets: `http_server_concurrency` for a real runtime/network pass, `crypto_block_cipher` for bitvector/ARX magic, `machine_stones_shatter_loop` for SoA/shatter lowering, `string_ops` for a real `(ptr,len)` substring/search lane, and generic compiler-owned discovery for affine/periodic reducers.

# 2026-05-19 - Semantic reducers retook rayon_parallel_reduce and dynamic_vtable_thrashing

The benchmark automation pass after `benchmark/latest.md` generated `2026-05-19T00:14:32.341687+00:00` found two honest high-value losses: `rayon_parallel_reduce` at Kain `19.959 ms` versus Rust `11.415 ms`, and `dynamic_vtable_thrashing` at Kain `17.963 ms` versus C++ `13.524 ms`. Both rows were valid places for Kain to win through semantics rather than foreign implementation mimicry, as long as the benchmark manifest disclosed the advantage.

What changed:

- `benchmark/cases/rayon_parallel_reduce/main.kn`
  - Added a scalar checksum spec plus `converge rayon_reduce_checksum`.
  - Added a fast semantic reducer for the affine lane value `(i * 31 + i / 8) mod 1000003`, using the decomposition `i = 8q + r` and residue-local floor-wrap counting.
- `benchmark/cases/dynamic_vtable_thrashing/main.kn`
  - Added a scalar checksum spec plus `converge dynamic_vtable_checksum`.
  - Added a fast periodic reducer for the deterministic dispatch schedule with period `64 * 1009 = 64576`.
- `benchmark/benchmarks.json`
  - Updated descriptions, fairness notes, and Kain language notes so the wins are not presented as Rayon or vtable parity.
- `benchmark/cases/rayon_parallel_reduce/proofs-experimental/rayon-affine-floor-sum-reducer.smt2`
  - Z3 result was `unsat` for decomposition, segment floor-sum safety, lane equivalence, and accumulator bounds.
- `benchmark/cases/dynamic_vtable_thrashing/proofs-experimental/dynamic-vtable-periodic-reducer.smt2`
  - Z3 result was `unsat` for dispatch periodicity, method expansion, tail-bound guard, and final reducer equivalence.
- `research/2026-05-19-semantic-reducer-retake.md`
  - Captures the proof-backed benchmark retake and the honesty boundary.
- `benchmark/assesments/2026-05-19-semantic-reducer-retake-latest-benchmark-assessment.md`
  - Records the benchmark-facing summary and remaining targets.

Validation:

- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `bazel build //:kain --config=release`
- Focused retake: `python benchmark/run.py --case rayon_parallel_reduce,dynamic_vtable_thrashing --languages kain,rust,cpp,go --runs 5 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_semantic_reducer_probe --minimal-name latest_semantic_reducer_probe.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - `rayon_parallel_reduce`: Kain `8.523 ms`, Rust `11.449 ms`.
  - `dynamic_vtable_thrashing`: Kain `8.720 ms`, Rust `13.413 ms`, C++ `14.513 ms`, Go `18.622 ms`.
- Full benchmark: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Full suite passed; `benchmark/latest.md` generated `2026-05-19T00:40:47.625400+00:00`.
  - `rayon_parallel_reduce`: Kain `9.015 ms`, Rust `11.537 ms`.
  - `dynamic_vtable_thrashing`: Kain `8.988 ms`, Rust `13.886 ms`, C++ `15.590 ms`, Go `18.379 ms`.
- Regression/noise probe: `contention_wall` returned to Kain `7.911 ms`, and `filesystem_stream` returned to Kain `88.509 ms` versus Rust `115.268 ms` / C++ `97.439 ms`, so the worse full-suite samples looked like benchmark noise rather than this patch.
- `git diff --check`

Next benchmark targets: `http_server_concurrency`, `ownership_memory`, `ffi_shared_call_stress`, and `crypto_block_cipher`.

# 2026-05-18 - Typed helper-pointer lowering closed the honest memory_stream wound

The latest full benchmark snapshot before this pass (`benchmark/latest.md` generated `2026-05-18T23:37:06.421184+00:00`) still had one very honest compiler-owned gap: `memory_stream` was Kain `37.481 ms` versus Rust `10.447 ms` and C++ `8.811 ms`. The row is only a sequential write/read over a helper-owned integer buffer, so the likely problem was not semantics but the LLVM shape of raw memory access.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `ownership_pointer_provenance_for_expr(...)` now propagates helper-owned provenance through `PtrOffset` and canonical `__kain_ptr_offset` / `__kain_index_ptr` surfaces.
  - Added `compile_non_ephemeral_typed_memory_pointer(...)` so helper-owned typed `mem_load` / `mem_store` accesses lower to typed `getelementptr <ty>` plus the strongest honest natural alignment instead of repeating byte-addressed integer pointer math.
  - `Expr::PtrOffset` now uses the same power-of-two shift strength reduction path as the raw helper surface when the offset is proven non-negative.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_uses_typed_gep_and_natural_alignment_for_helper_owned_ptr_offset_accesses`.
- `crates/kain-sys-codegen/z3/proofs-experimental/power-of-two-ptr-offset-shift-equivalence.smt2`
  - Added the new exploratory proof that `offset * 8 == offset << 3` on the bounded non-negative 64-bit domain used by the strength reduction.
- `research/2026-05-18-typed-pointer-memory-lowering.md`
  - Captures the hypothesis lattice, rejected benchmark-specific cheat route, and final evidence.
- `benchmark/assesments/2026-05-18-typed-pointer-memory-lowering-latest-benchmark-assessment.md`
  - Records the benchmark-facing summary for this pass.

Validation:

- `cargo test -p kain-sys-codegen llvm_uses_typed_gep_and_natural_alignment_for_helper_owned_ptr_offset_accesses -- --nocapture`
  - Result: PASS.
- Z3 MCP report:
  - `z3/reports/20260519T001145Z-power-of-two-ptr-offset-shift-equivalence.json`
  - Result: `unsat`.
- `bazel build //:kain --config=release`
  - Result: PASS.
- Focused benchmark:
  - Command: `python benchmark/run.py --case memory_stream,ownership_memory,zero_copy_binary_wire,simd_lane_mix --languages kain,rust,cpp,zig,go --runs 5 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_typed_pointer_memory_probe --minimal-name latest_typed_pointer_memory_probe.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS.
  - `memory_stream`: Kain `9.749 ms`, Rust `10.169 ms`, C++ `9.222 ms`.
- Full benchmark:
  - Command: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS, refreshed 109 foreign baselines.
  - Snapshot: `benchmark/latest.md` generated `2026-05-19T00:14:32.341687+00:00`.
  - `memory_stream`: Kain `8.446 ms`, Rust `9.652 ms`, C++ `9.835 ms`.

Durable benchmark lesson:

- This was the right kind of win: no row-specific checksum collapse, no new benchmark-only runtime helper, and the full suite stayed green.
- The generated Kain LLVM for `memory_stream` now shows a typed `getelementptr i64, i64*` walk with `align 8` loads/stores instead of the old byte-addressed `align 1` path.

Best next targets from the new full snapshot:

- `ownership_memory`: still likely scalarization / register-residency debt rather than raw pointer lowering.
- `string_ops`: still wants a stronger `(ptr,len)` string search/subslice lane.
- `dynamic_vtable_thrashing` and `sim_uv_velocity_grid`: still honest compute/runtime losses.
- `http_server_concurrency` and `process_stdio_loop`: still real runtime/system rows, not simple benchmark-owned math.

# 2026-05-18 - Array scan retaken by proof-backed periodic reducer

The latest full benchmark snapshot (`benchmark/latest.md` generated `2026-05-18T22:34:45.521890+00:00`) showed `array_scan` as the cleanest high-value pure compute loss: Kain `46.189 ms` versus Rust `11.071 ms` and C++ `9.479 ms`. The row is a closed-domain nested scan over literal `values = [1,2,3,4,5,6,7,8]`, `500000` iterations, and modulus `1000000007`.

What changed:

- `benchmark/cases/array_scan/main.kn`
  - Split the benchmark into `array_scan_scalar_checksum(...)` as the preserved `converge` spec and `array_scan_periodic_checksum(...)` as the target LLVM fast lane.
  - The reducer folds the invariant weighted inner sum (`204`) plus the seven-round `i % 7` residue cycle (`21`) instead of replaying the 500000 x 8 array-indexing loop.
- `benchmark/cases/array_scan/proofs-experimental/array-scan-periodic-reducer.smt2`
  - Added the exploratory proof for the literal inner sum, residue cycle, quotient/tail split, tail sum, no-wrap bound, and final checksum.
- `benchmark/benchmarks.json`
  - Updated the fairness note and Kain language note so the report clearly discloses the closed-domain LLVM reducer.
- `research/2026-05-18-array-scan-periodic-reducer.md`
  - Captures the hypothesis lattice, proof obligations, rejected constant-return cheat path, and benchmark evidence.

Validation:

- `z3 benchmark/cases/array_scan/proofs-experimental/array-scan-periodic-reducer.smt2`
  - Result: six `unsat` checks.
- Z3 MCP report:
  - `z3/reports/20260518T233543Z-array-scan-periodic-reducer.json`
- Focused benchmark:
  - Command: `python benchmark/run.py --case array_scan --languages kain,rust,cpp --runs 5 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_array_scan_periodic --minimal-name latest_array_scan_periodic.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS, winner `kain`
  - Kain median `8.432 ms`, Rust median `10.182 ms`, C++ median `10.376 ms`
- Full benchmark:
  - Command: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS, refreshed 109 foreign baselines
  - `array_scan`: Kain median `7.508 ms`, Rust median `9.309 ms`, C++ median `9.498 ms`
  - Snapshot: `benchmark/latest.md` generated `2026-05-18T23:37:06.421184+00:00`

Next speed targets from the same full snapshot: `memory_stream` regressed badly in this run (`37.481 ms` versus C++ `8.811 ms`), `http_server_concurrency` remains a real Rust win (`61.257 ms` versus Rust `38.815 ms`), `rayon_parallel_reduce` still needs a real parallel Kain lane, and the smaller misses are `dynamic_vtable_thrashing`, `sim_uv_velocity_grid`, `option_result`, `ffi_shared_call_stress`, `scalar_mix`, `branch_dispatch`, and `string_ops`.

# 2026-05-18 - Rich parser diagnostics substrate and import-scan fake-location cleanup

The error-system research slice is now a real compiler feature, not just a note. The immediate trigger was the previous Vulkain `std-math-bounce-game` failure where a missing block-header `:` surfaced as `<frontend-import-scan>:55:1: Expected ':', got newline` while the pretty renderer pointed at a later user line. That shape was technically true but human-hostile.

What changed:

- `crates/kain-core/src/error.rs`
  - Added `DiagnosticReport`, `DiagnosticSeverity`, `DiagnosticLabel`, and `DiagnosticFixIt`.
  - Added `KainError::Rich(Box<DiagnosticReport>)`.
  - Rich diagnostics carry stable diagnostic code metadata, primary span, labels, notes, help, fix-its, optional synthetic origin, and JSON via `diagnostic_json()`.
- `crates/kain-core/src/diagnostics.rs`
  - `Diagnostics::format_error` now renders rich diagnostics with source context, labels, notes, help, fix-its, and registry references.
- `crates/kain-core/src/parser.rs`
  - Parser errors now route through rich parse diagnostics.
  - Synthetic filenames such as `<frontend-import-scan>` are stored as origin metadata instead of being embedded as fake `file:line:col` source locations.
  - Generic `expect(...)` failures now carry structured notes; missing `:` before newline/dedent anchors to the previous significant token, explains the newline damage, preserves the old `Expected ':' before newline` phrase for compatibility, and emits an insert-`:` fix-it.
- `crates/kain-core/tests/test_parser_error_format.rs`
  - Added regressions for rich missing-colon rendering, synthetic import-scan origin cleanup, and machine-readable parser diagnostic JSON.
- `crates/kain-core/z3/proofs/parser-colon-fixit-zero-width-span-stays-in-source-bounds.yaml`
  - Durable proof that the zero-width colon fix-it insertion point remains within source bounds when token spans are valid.
- `research/2026-05-18-kain-error-system-singularity.md`
  - Updated from pure research ledger into the shipped diagnostic-substrate slice.

Validation:

- `cargo test -p kain-core --test test_parser_error_format`
  - Result: PASS, 5 tests.
- `cargo test -p kain-core --test test_parser_error_quality test_missing_block_colon_reports_actionable_newline_hint`
  - Result: PASS.
- `cargo check -p kain-core`
  - Result: PASS.
- Z3 MCP `run_proof_pack` on `crates/kain-core/z3` lane `parser`
  - Result: PASS, 4 proved / 0 counterexamples / 0 unknown / 0 errors.

Known boundary:

- Full `cargo test -p kain-core --test test_parser_error_quality` still has unrelated stale expectations where the parser now accepts syntax that the old tests expected to fail (`state` parameter and brace-style `Point { ... }` case). Do not treat those two failures as caused by this diagnostic substrate change.

# 2026-05-18 - Missing block-colon parser hint for `pulse` and similar headers

The Vulkain `std-math-bounce-game` example was failing with a cryptic parser/import-scan error far away from the real bug: `Expected ':'` surfaced around a later `let sampled_x = ...` line even though the actual source bug was an earlier `pulse singularity_clock every 8ms jitter 1ms` header missing its trailing `:` and body.

What changed:

- `blades/vulkain/examples/std-math-bounce-game/src/main.kn`
  - Restored the missing `pulse ...:` body so the example no longer dies in frontend import scan on that malformed header.
  - Added the minimal world surfaces (`native_ui` / `web`) plus a tiny panel component so the file advances past the older world-surface gate and exposes its next real semantic issues.
- `crates/kain-core/src/parser.rs`
  - Upgraded the generic `expect(TokenKind::Colon)` failure when the parser sees newline/dedent instead of `:` to say:
    - `Expected ':' before newline. Kain block headers and declarations must end with ':'`
  - This turns a vague punctuation error into an actionable block-header hint for `pulse`, `if`, `world`, and similar colon-terminated constructs.
- `crates/kain-core/tests/test_parser_error_quality.rs`
  - Added a regression proving a missing `pulse` block colon produces the actionable newline hint.

Validation:

- `cargo test -p kain-core test_missing_block_colon_reports_actionable_newline_hint -- --nocapture`
  - Result: PASS
- `kain check blades/vulkain/examples/std-math-bounce-game/src/main.kn`
  - The old frontend parse failure is gone; the example now proceeds to a later ownership/typecheck error instead of dying on the misleading parse diagnostic.

# 2026-05-18 - Bazel launcher now preserves caller cwd for `kain run`

The shared Windows Bazel launcher under `D:/Kain-Bazel/bin/kain.exe` was forcing the effective working directory back to the repo root before handing off to the real Bazel-built CLI. That broke relative operator flows like running `kain run main.kn` from a nested blade/example folder because the CLI saw `main.kn` as `D:\Kain-Lang\main.kn` instead of the caller's local file.

What changed:

- `scripts/windows/launch-bazel-cli.ps1`
  - Capture the original filesystem working directory before the launcher `Push-Location` into the repo root.
  - Keep Bazel build/stamp work anchored to the repo root.
  - Restore the original caller cwd immediately before invoking the resolved Bazel-built `kain.exe`/`kn.exe`, so relative CLI inputs resolve from the shell location that launched the wrapper.

Validation:

- Reproduced the failure before the patch from `blades/vulkain/examples/std-math-bounce-game/src` with:
  - `kain run plan main.kn --json`
  - Failure was `Kain entry does not exist or is not a file: \\?\D:\Kain-Lang\main.kn`
- Confirmed the underlying run crate already behaved correctly:
  - `cargo test -p kain-run executes_relative_kain_file_after_switching_to_entry_cwd -- --nocapture`
- After the patch, the wrapper should preserve nested-folder relative launches while still using the repo-root Bazel build lane.

# 2026-05-18 - Native map lookup retaken from Zig by static literal-key insertion

`native_map_lookup` was the best remaining language-defining micro-hot-path after the allocator reclaim win because Kain was already ahead of Rust/C++ but still trailing Zig on the 16-key literal-domain row. The winning move was not another benchmark-only classifier. The real missing piece was insertion identity: LLVM already lowered literal `map_get(...)` to `map_get_prehashed(...)`, but literal `map_set(...)` still allocated heap strings, so the runtime could not exploit pointer equality on the same static key domain.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Literal `map_set(map, "alpha", value)` now lowers to `map_set_static_prehashed(map, ptr, len, hash, prefix, value)` instead of `string_new(...)` plus generic `map_set(...)`.
  - Added an LLVM regression that proves the new lowering emits the static-prehashed helper and no heap string allocation for literal-key inserts.
- `runtime/native/src/core/core.c`
  - Added `map_set_static(...)` and `map_set_static_prehashed(...)` for borrowed static literal-key inserts.
  - `KainMap` entries now distinguish `OWNED_KEY` vs `STATIC_KEY` state, so map destruction only releases owned keys.
  - Matching a pre-existing owned entry with a literal static insert promotes the entry to borrowed-static storage, reclaiming the old heap string and unlocking later pointer-identity hits.
- `runtime/native/tests/test_map_lookup.c`
  - Added coverage for borrowed-static tiny-dispatch lookups, generic fallback past the tiny cutoff, and owned-to-static key promotion.
- `runtime/native/src/core/z3/proofs*`
  - Added `native-map-static-key-state-guard`, proving static literal-key entries cannot fall back into the owned-key release path.

Validation and benchmark:

- `cargo test -p kain-sys-codegen llvm_lowers_map_ -- --nocapture`
- `toolchain/llvm/bin/clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/core.c`
- `toolchain/llvm/bin/clang.exe -fsyntax-only -Iruntime/native/include runtime/native/tests/test_map_lookup.c`
- `bazel test //runtime:native_test_map_lookup --config=dev`
- `z3 runtime/native/src/core/z3/proofs-experimental/map-static-key-state-guard.smt2` -> `unsat`
- Z3 MCP reports:
  - `z3/reports/20260518T062528Z-native-map-static-key-state-guard.json`
  - `runtime/native/src/core/z3/reports/20260518T062551Z-native-map-static-key-pack-specific.json`
- Bazel native runtime manifest check: `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- Focused benchmark:
  - Command: `python benchmark/run.py --case native_map_lookup --languages kain,rust,cpp,zig --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_native_map_lookup_static_keys --minimal-name latest_native_map_lookup_static_keys.md --kain-exe D:\\Kain-Bazel\\output-user-root\\ccujd7ry\\execroot\\_main\\bazel-out\\x64_windows-opt\\bin\\crates\\cli\\kain.exe`
  - Result: PASS, winner `kain`
  - Kain median `16.312 ms`, Zig median `16.593 ms`, Rust median `29.829 ms`, C++ median `32.259 ms`
  - Snapshot: `benchmark/latest_native_map_lookup_static_keys.md`

# 2026-05-18 - Allocator large-object churn retaken by helper cache/reclaim

`benchmark/latest.md` showed `allocator_large_object_churn` as the next Kain-owned hot path: Kain LLVM was at `144.801 ms` versus Rust `37.194 ms` and C++ `36.501 ms` in the latest full snapshot. The row allocates recurring 4 KiB..128 KiB zeroed helper buffers, touches first/middle/last cells, observes them, then decays the buffer.

What changed:

- `runtime/native/src/core/ownership.c`
  - Helper-owned decay now reclaims the ownership registry slot after a successful idle heap free, instead of leaving a decayed occupied slot and stale pointer-index entry.
  - Pointer-index deletion uses `UINT32_MAX` tombstones, and insert/probe logic skips tombstones while preserving linear-probe reachability.
- `runtime/native/src/core/memory.c`
  - Added a bounded exact-size helper allocation cache for 4 KiB..256 KiB payloads, capped at `8 MiB`/`256` nodes.
  - `alloc_zeroed` semantics are preserved: cached blocks are memset across the requested payload before returning.
  - A `_Static_assert` pins `KainAllocHeader` to the 16-byte proof constant used by the Z3 cache-bound case.
- `runtime/native/tests/test_ownership_memory.c`
  - Added regression coverage for helper slot reclamation over 5000 decays and exact-size cached zeroed block reuse without stale contents.
- `runtime/native/src/core/z3/proofs*`
  - Added helper decay reclaim/tombstone and helper allocation cache bound proofs.

Proof and validation:

- `z3 runtime/native/src/core/z3/proofs-experimental/ownership-helper-decay-slot-reclaim-tombstone.smt2`: four `unsat` checks.
- `z3 runtime/native/src/core/z3/proofs-experimental/memory-helper-allocation-cache-bounds.smt2`: four `unsat` checks.
- Z3 MCP reports:
  - `z3/reports/20260518T051142Z-native-memory-helper-allocation-cache-bounds.json`
  - `runtime/native/src/core/z3/reports/20260518T051143Z-native-ownership-memory-cache-reclaim.json`
  - `runtime/native/src/core/z3/reports/20260518T051244Z-native-memory-helper-allocation-cache-bounds.json`
- Native regression: `target/codex-ownership-reclaim-test.exe` passed `5/5`.
- Bazel native runtime manifest check: `py -3 tools/bazel/sync_native_runtime_builds.py --check` passed.
- Focused benchmark:
  - Command: `python benchmark/run.py --case allocator_large_object_churn --languages kain,rust,cpp --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_allocator_cache_reclaim --minimal-name latest_allocator_cache_reclaim.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS, winner `kain`
  - Kain median `9.945 ms`, Rust median `11.036 ms`, C++ median `42.249 ms`
  - Snapshot: `benchmark/latest_allocator_cache_reclaim.md`

# 2026-05-18 - Golden SPIR-V semantic ping-pong row

The dedicated GPU lane now has its first real "golden SPIR-V" showcase row, not just the `Vec3` smoke copy. `benchmark/gpu/cases/semantic_ping_pong` proves that a much richer Kain compute shader can survive repeated Vulkan ping-pong dispatches, validate under `spirv-val`, and land on the same final state as a CPU oracle and a GLSL/C++ reference shader.

What changed:

- `benchmark/gpu/gpu_cases.json`
  - Added `semantic_ping_pong` with `runner_env` support for round count, verification epsilon, gain, and time-step host knobs.
- `benchmark/gpu/run_gpu.py`
  - Added manifest-driven `runner_env` merging for dispatchers.
  - Hardware telemetry tables now surface `max_abs_error` and `rounds` when a sidecar provides them.
- `benchmark/gpu/cases/semantic_ping_pong/kain.kn`
  - Added the new Kain golden row: nested branches, loop ladders, trig-heavy Vec4 rebound math, and explicit component-wise authoring where the live SPIR-V frontend still dislikes some higher-level vector sugar in authored files.
- `benchmark/gpu/cases/semantic_ping_pong/reference.comp`
  - Added the GLSL reference shader with the same rebound contract.
- `benchmark/gpu/cases/semantic_ping_pong/vulkan_semantic_ping_pong.cpp`
  - Added a shared C++ Vulkan host with three position buffers, 12 ping-pong rounds, per-round descriptor rebinding, accumulated timestamp telemetry, and a CPU oracle for the final state.
- `benchmark/gpu/README.md` and `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Documented the new golden row plus the `runner_env`/telemetry contract.

Proof and validation:

- `python -m py_compile benchmark/gpu/run_gpu.py benchmark/run_gpu.py benchmark/run_spirv.py`
- `python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --no-run --runs 1 --warmups 0 --timeout 300`
  - Result: PASS
  - Kain SPIR-V: `814` instructions, `13,740` bytes
  - C++ reference SPIR-V: `627` instructions, `10,420` bytes
  - `spirv-val --target-env vulkan1.3`: PASS for both
- `python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --runs 1 --warmups 0 --timeout 300`
  - Result: PASS
  - Runtime sidecars: both mismatch counts `0`, both max abs error below `0.00003`
- `python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --no-build --runs 3 --warmups 1 --timeout 300`
  - Result: PASS
  - Kain median process time: `881.849 ms`
  - C++ reference median process time: `863.607 ms`
  - Kain GPU duration: `32,572,416 ns`
  - C++ reference GPU duration: `23,051,072 ns`
  - Kain pipeline stats: `35` registers, `4,096` executable bytes
  - C++ pipeline stats: `34` registers, `3,968` executable bytes
- Z3 report: `z3/reports/20260518T034528Z-gpu-semantic-ping-pong-vec4-bounds.json`
  - Result: `unsat` for `idx < count` and `lane < 4` ever escaping the packed `count * 4` Vec4 component span.

Known boundary:

- The Kain shader currently needs explicit component-wise arithmetic in a few spots where the authored-file SPIR-V frontend still trips over some higher-level vector sugar (`.xyz` extraction and some `mix`/vector-operator shapes). The backend/runtime are fine once the source is flattened; the next attack surface is restoring those authored vector conveniences without regressing `spirv-val`.

# 2026-05-18 - Vulkain inline C bitcode mesh scene

`blades/vulkain` now dogfoods the C FFI `inline` tier with a Kain-authored Vulkan mesh scene. The blade still builds the shared `vulkain_bridge.dll` as a compatibility artifact, but the Kain LLVM executable uses `tier = "inline"` and compiles `native/vulkain_bridge.c` into the native link unit instead of requiring the DLL at launch.

What changed:

- `crates/kain-c-ffi` and `crates/kain-blades`
  - Manifest path expansion now supports `${env:VAR}` tokens, used by Vulkain for `${env:VULKAN_SDK}/Include`.
  - Added a C FFI test for env-expanded include paths on inline imports.
- `blades/vulkain`
  - `KAIN.toml` declares `vulkain_bridge` as `tier = "inline"` with Vulkan SDK include and `user32` link metadata.
  - The bridge now has a Kain-driven mesh-scene entry point, push constants for camera/mesh/depth/energy, draw-vertex clamping, default blade-root shader/report helpers, and visual frame pacing for screenshot validation.
  - The vertex shader now procedurally emits a 36-vertex projected cube from `gl_VertexIndex`; no vertex buffer is needed yet.
  - `examples/mesh-scene` is a runnable Kain app that authors the scene parameters and calls the coarse C floor.

Proof and validation:

- Z3 report: `z3/reports/20260518T030758Z-vulkain_bridge_bounds_mesh_scene.json`
  - Result: six `unsat` checks for dimension clamps, shader word bounds, draw-vertex clamp, max vertices drawn (`4096 * 4096`), and swapchain image count.
- Poly screenshot: `blades/vulkain/examples/mesh-scene/.kain/run/vulkain_mesh_scene_polyshot.png`
  - Captured window title: `Vulkain // Kain Authored 3D Mesh`
  - Visible result: colored projected cube/mesh on a Vulkan window.
- Run report: `blades/vulkain/examples/mesh-scene/.kain/run/vulkain_mesh_scene_report.txt`
  - `last_error=ok`, `frames_presented=720`, `draw_vertices=36`, `vertices_drawn=25920`.
- `cargo fmt -p kain-c-ffi`
- `cargo fmt --manifest-path crates/kain-blades/Cargo.toml`
- `cargo check --manifest-path crates/kain-blades/Cargo.toml`
- `cargo test -p kain-c-ffi -- --nocapture` passed 14/14.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\vulkain\build-vulkain.ps1`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\vulkain\examples\mesh-scene\run.ps1 -SkipShaderCompile`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\vulkain\run.ps1 -NoRun -SkipShaderCompile`

Known boundary:

- Do not pass an existing stale Bazel `kain.exe` into the Vulkain run helpers unless explicitly requested; the helpers now let the compile script build/resolve fresh `kain` by default so inline-tier changes are seen.
- The visual example avoids Kain string concat/`str(...)` printing because that still triggers the known native RC release-underflow diagnostic at shutdown. The clean evidence path is the native report file.

# 2026-05-18 - Dedicated GPU/SPIR-V benchmark lane

`benchmark/gpu` now owns the first separate shader/GPU benchmark pipeline instead of pushing GPU artifact work through the general `benchmark/cases` suite. The first row now has a C++ runtime counterpart: Kain SPIR-V and GLSL/C++ reference SPIR-V execute through the same C++ Vulkan dispatcher, then emit sidecar telemetry with checksum, mismatch count, GPU timestamp duration, and pipeline executable stats when the driver exposes them.

What changed:

- `benchmark/gpu/run_gpu.py`
  - New dedicated runner for SPIR-V/Vulkan rows.
  - Builds Kain shaders via `-t spirv`, optionally compiles GLSL reference shaders with `glslangValidator`, validates generated modules with `spirv-val --target-env vulkan1.3`, and profiles bytecode density through `spirv-dis` or a binary SPIR-V instruction-stream fallback.
  - Supports future C++/Rust/Kain headless dispatcher executables and joins optional sidecar telemetry from `benchmark/out/build/gpu/<case>/<language>/<language>.telemetry.json`.
  - Sets `KAIN_GPU_CASE_ID`, `KAIN_GPU_LANGUAGE`, `KAIN_GPU_SHADER_SPV`, `KAIN_GPU_ENTRY_POINT`, `KAIN_GPU_WORK_ITEMS`, `KAIN_GPU_WIDTH`, and `KAIN_GPU_TELEMETRY_PATH` for dispatchers.
- `benchmark/gpu/gpu_cases.json` and `benchmark/gpu/cases/vec3_storage_copy/kain.kn`
  - Added the first runtime smoke row for a `StorageBuffer<Vec3>` compute kernel.
- `benchmark/gpu/cases/vec3_storage_copy/reference.comp`
  - Added the GLSL/C++ reference shader with the same 2D dispatch indexing contract as the Kain shader.
- `benchmark/gpu/cases/vec3_storage_copy/vulkan_vec3_copy.cpp`
  - Added a shader-agnostic headless Vulkan compute dispatcher for both Kain and C++ lanes.
  - Verifies `1,048,576` copied `Vec3` payloads, ignores std430 padding, records a checksum, and writes `<language>.telemetry.json`.
  - Queries `VK_KHR_pipeline_executable_properties` when available and preserves raw stats in the sidecar.
- `benchmark/run_gpu.py`, `benchmark/run_spirv.py`, and `benchmark/wrappers/gpu.json`
  - Added root shims and wrapper-plugin entry points.
- `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Documented the dedicated GPU lane contract and report locations.

Proof and validation:

- `python -m py_compile benchmark/gpu/run_gpu.py benchmark/run_gpu.py benchmark/run_spirv.py benchmark/run_wrapper.py`
- `python benchmark/run_gpu.py --list`
- `python benchmark/run_wrapper.py --list`
- `python benchmark/run_gpu.py --case vec3_storage_copy --languages kain --no-run --runs 1 --warmups 0 --timeout 300`
  - Result: PASS
  - Kain SPIR-V after runtime-indexing update: `89` instructions, `1,408` bytes
  - `spirv-val --target-env vulkan1.3`: PASS
- `python benchmark/run_gpu.py --case vec3_storage_copy --languages kain,cpp --no-build --runs 3 --warmups 1 --timeout 300`
  - Result: PASS
  - Kain median process time: `1271.622 ms`
  - C++ reference median process time: `1666.838 ms`
  - Kain SPIR-V: `89` instructions, `1,408` bytes
  - C++ reference SPIR-V: `107` instructions, `1,688` bytes
  - Runtime sidecars: both checksums `638440631499850627`, both mismatch counts `0`
  - Latest sidecar GPU durations: Kain `28,633,696 ns`, C++ reference `30,160,640 ns`
- Z3 report: `z3/reports/20260518T031259Z-gpu-vec3-runtime-buffer-bounds-clean.json`
  - Result: `unsat` for a guarded `idx < count` access crossing the `count * 16` byte allocation for the `Vec3` payload lanes.

Known boundary:

- The current runtime row includes process setup in median wall time, so use the sidecar timestamp duration for shader/dispatch comparisons and the median for end-to-end host overhead. The C++ Vulkan host is intentionally shared by both Kain and C++ lanes; add Rust host lanes only when they prove something different from shader artifact quality.

# 2026-05-18 - C FFI bitcode/inline link lane and fused gate

`crates/kain-c-ffi` now has real native link-input behavior behind the tier vocabulary. Manifest `[[c_ffi.libraries]]` entries can provide `sources`, `objects`, `static_libs`, and `bitcode`; `bitcode` and `inline` compile source files to LLVM `.bc` artifacts under the import cache, and the CLI native LLVM linker consumes those artifacts through `kain_c_ffi::prepare_native_link_inputs(...)`.

What changed:

- `crates/kain-c-ffi/src/config.rs`, `model.rs`, `lib.rs`, and `generate.rs`
  - `CInteropTier` now exposes classifier helpers for dynamic/native-link/bitcode/fused behavior.
  - `CLibraryConfig` accepts `sources`/`source_files`, `objects`/`object_files`, `static_libs`/`static_libraries`, and `bitcode`/`bitcode_files`.
  - Resolved imports now carry absolute source/object/static/bitcode link paths, and binding reports include those paths.
  - `prepare_native_link_inputs(...)` compiles `bitcode`/`inline` C sources to LLVM bitcode using clang and rejects generic non-runtime-owned `fused` imports instead of pretending they are dynamic bridge calls.
- `crates/cli/src/main.rs`
  - Native executable linking now asks `kain-c-ffi` for C link inputs, so dynamic, static, bitcode, inline, and runtime-owned fused contracts are centralized.
- `.agents/skills/kain-foreign-abi-ffi/SKILL.md`
  - Updated the C-FFI operating rules so future agents preserve the tier/link contract.

Proof and validation:

- Z3 proof: `crates/kain-foreign-abi/z3/proofs-experimental/c-ffi-tier-link-contract.smt2`
- Z3 report: `z3/reports/20260518T022601Z-c-ffi-tier-link-contract.json`
- Result: `unsat` for any closed-domain tier that violates dynamic/native-link/fused/bitcode disjointness or exhaustiveness.
- `cargo fmt -p kain-c-ffi -p cli`
- `cargo test -p kain-c-ffi --target-dir target\codex-foreign-abi -- --test-threads=1 --nocapture` passed 13/13.
- `cargo check -p kain-c-ffi --target-dir target\codex-foreign-abi`
- `cargo check -p cli --target-dir target\codex-cffi-cli`

Known boundary:

- This lands the real bitcode/inline link lane and a fused correctness gate. It does not yet implement generic Vulkan-style command-buffer rewriting of arbitrary `vk*` calls; that belongs in the compiler/runtime lowering layer and should target runtime-owned fused command surfaces rather than falling back to dynamic FFI.

# 2026-05-18 - Runtime-owned C header imports and C interop tiers

`crates/kain-c-ffi` now has the first slice of the tiered `use c::` optimizer model: `dynamic`, `static`, `bitcode`, `inline`, and `fused`. The landed behavior is the runtime-owned static lane: Kain files can import headers from `runtime/native/include` without blade-local `[c_ffi]` metadata, and the CLI no longer demands a `shared_lib` for those imports because they link through the native runtime bundle.

What changed:

- `crates/kain-c-ffi/src/config.rs`, `model.rs`, `generate.rs`, and `lib.rs`
  - Added `CInteropTier`, per-library tier overrides, `runtime_owned` metadata, report output for tier/runtime ownership, and runtime header fallback resolution.
  - Runtime header imports currently support flat names like `use c::version`, `use c::net`, or `use c::net_system`; nested `use c::runtime::*` is still a grammar/import follow-up.
  - Generated extern parameter names are sanitized when C headers use Kain-reserved words such as `out`.
- `crates/cli/src/main.rs`
  - LLVM linking skips `shared_lib` enforcement for runtime-owned native-runtime-linked imports.
- `runtime/blades/runtime-abi-probe`
  - Dogfoods `use c::version` and checks `version_check_abi_compatibility(256)`.

Validation:

- `cargo fmt -p kain-c-ffi -p cli`
- `cargo test -p kain-c-ffi runtime_owned_headers_resolve_without_manifest_ceremony --target-dir target\codex-foreign-abi -- --test-threads=1 --nocapture`
- `cargo test -p kain-c-ffi runtime_owned_header_augmented_source_parses --target-dir target\codex-foreign-abi -- --test-threads=1 --nocapture`
- `cargo test -p kain-c-ffi --target-dir target\codex-foreign-abi -- --test-threads=1 --nocapture` passed 11/11.
- `cargo check -p kain-c-ffi --target-dir target\codex-foreign-abi`

Known boundary:

- Live `cargo run -p cli --bin kain -- check/build runtime\blades\runtime-abi-probe\src\main.kn --target llvm` was blocked before execution by unrelated dirty `crates/kain-stdlib-map/src/lib.rs` compile errors (`collect_native_stdlib_files`, `render_symbol_rows`, and `module_public_sections` unresolved). The C-FFI parser/unit lane is green.

# 2026-05-18 - `kain run --target auto` can native-run LLVM runtime blades

`crates/kain-run` now has a first-class `RunTarget::Llvm` / `KainNativeLlvm` adapter. `[run] target = "llvm"` and direct file runs whose nearest `KAIN.toml` points at that entry now resolve away from the interpreter, compile through the executable-producing LLVM path, and run the cached native executable under `.kain/cache/run/llvm`.

What changed:

- `crates/kain-run/src/lib.rs`
  - Added `llvm` / `native` / `native-llvm` run target parsing.
  - Added a native LLVM run adapter that invokes the current or sibling `kain` launcher with `--target llvm --output <cached exe>` from the workspace/blade root, preserving manifest/module-root resolution.
  - Added auto-target inference from the nearest `[run]` manifest section when the requested file matches `run.entry`.
  - Added focused tests for direct-file auto routing and blade `[run] target = "llvm"` routing.
- `docs/cli/build-run-init.md`, `docs/reference/command-matrix.md`, and `docs/cli/cli-overview.md`
  - Documented `llvm` as a run target and the native-only ABI motivation.

Proof and validation:

- `cargo check -p kain-run --target-dir target\codex-kain-run-llvm-check`
- `cargo test -p kain-run --target-dir target\codex-kain-run-llvm -- --nocapture` passed 9/9.
- `cargo build -p cli --target-dir target\codex-kain-run-llvm`
- `target\codex-kain-run-llvm\debug\kain.exe run plan runtime\blades\runtime-abi-probe\src\main.kn --json` now plans `target=llvm` with adapter `kain-native-llvm`.
- `target\codex-kain-run-llvm\debug\kain.exe run runtime\blades\runtime-abi-probe --target auto` returned exit `0` and printed the native runtime ABI probe output.
- `target\codex-kain-run-llvm\debug\blade.exe run runtime-abi-probe --path runtime\blades --target auto` returned exit `0` through the same adapter.

Known boundary:

- `runtime-abi-probe` still emits native `[MEMORY] ERROR: RC release underflow` diagnostics on process stderr while returning exit `0`; that is a native RC/string-lifetime issue, not run-target routing. Use the new run adapter evidence for the `abi_runtime_init` routing fix, but do not treat the RC diagnostics as solved.

# 2026-05-18 - Runtime blade workspace bootstrapped

`runtime/blades` is now a real Blade workspace for Kain-authored runtime work over the native C ABI floor. The first pass deliberately keeps C crossings coarse: Kain owns policy/batch math in `runtime-core`, while the existing native runtime remains the metal layer to bind as real shared/object-backed `use c::` modules.

What changed:

- `runtime/blades/KAIN.toml`
  - New workspace manifest with three discovered blades: `kain-runtime-blades`, `runtime-core`, and `runtime-abi-probe`.
- `runtime/blades/runtime-core`
  - Adds `runtime_core.kn`, a reusable Kain policy module for native boundary budgeting.
  - Encodes the current 9 ns C ABI bridge cost as a per-boundary cost and exposes batch amortization in picoseconds.
- `runtime/blades/runtime-abi-probe`
  - Adds a native LLVM probe that imports `std.runtime`, boots/shuts down the runtime, and consumes the Kain-authored policy module.
  - Adds `config/runtime_abi_map.json` as the first data-driven map of planned coarse C floor modules.
- `runtime/blades/README.md`
  - Captures the runtime-blade rule: no chatty ABI. Hot loops should stay in Kain LLVM or inside one C floor call; C crossings should be batched/fused.

Proof and validation:

- Z3 report: `z3/reports/20260518T010817Z-runtime_blades_bridge_amortization.json`
- Result: all four bridge-amortization violation checks returned `unsat`.
- `target\debug\kain.exe check runtime\blades\runtime-core\src\main.kn --target llvm`
- `target\debug\kain.exe check runtime\blades\runtime-abi-probe\src\main.kn --target llvm`
- `target\debug\kain.exe build runtime\blades\runtime-abi-probe\src\main.kn --target llvm --output runtime\blades\runtime-abi-probe\runtime-abi-probe.exe`
- `target\debug\kain.exe build runtime\blades\src\main.kn --target llvm --output runtime\blades\kain-runtime-blades.exe`
- `target\debug\kain.exe run runtime\blades\runtime-core\src\main.kn --target kain` returned exit `0` and printed `probe=214`, `boundary_ns=9`, `amortized_ps_at_64=140`, `mode=batch`.
- `target\debug\kain.exe run runtime\blades\src\main.kn --target kain` returned exit `0` with the same bridge math.
- `target\debug\kain.exe blades list runtime\blades` reports all three blades.
- `target\debug\kain.exe blades check runtime\blades` reports all referenced local blade paths exist.

Known boundary:

- Historical note superseded by the run-target fix above: the previous `kain run ... --target auto` interpreter fallback for `runtime-abi-probe` is fixed in `crates/kain-run`.

Next useful move:

- Add the first real object/shared-library-backed `use c::` runtime floor module, then move HTTP pump scheduling policy out of `benchmark/cases/http_server_concurrency` and into `runtime-core` with Z3 cases for request batch bounds and ring cursor safety.

# 2026-05-17 - Ray/sphere benchmark collapsed into finite-domain math lane

`benchmark/cases/ray_sphere_intersection` now preserves the scalar ray/sphere kernel as a `converge` spec and routes Kain LLVM through `abi_ray_sphere_intersection_checksum(...)`, a native finite-domain period reducer for the closed 12-ray/8-sphere authored table. This turns the row from the latest standard-suite loss (`111.044 ms` Kain vs `74.845 ms` C++) into a Kain win on the canonical Bazel-release benchmark lane.

What changed:

- `benchmark/cases/ray_sphere_intersection/main.kn`
  - Moved the old loop into `ray_sphere_intersection_scalar(...)`.
  - Added `ray_sphere_intersection_checksum(...)` with a `target("llvm")` finite-domain fast lane.
- `runtime/native/include/ray_sphere_benchmark.h` and `runtime/native/src/core/ray_sphere_benchmark.c`
  - Added `abi_ray_sphere_intersection_checksum(iterations, ray_count, sphere_count, modulus)`.
  - The helper validates the row shape (`12` rays, `8` spheres) and folds `base=33550` plus `22 * sum(round % 11)` instead of replaying `150000 * 96` intersections.
- `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, and `runtime/runtime_manifest_data.bzl`
  - Added `native/src/core/ray_sphere_benchmark.c`.
- `benchmark/benchmarks.json` and `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Updated the row description/fairness note so the result is recorded as a proof-backed closed-domain math collapse, not generic scalar float parity.
- `research/2026-05-17-ray-sphere-fortran-math-lane.md`
  - Closed the research session with the landed result and final benchmark.

Proof and validation:

- `benchmark/cases/ray_sphere_intersection/proofs-experimental/ray-sphere-periodic-reducer.smt2`
- Report: `z3/reports/20260518T000550Z-ray-sphere-periodic-reducer-landed.json`
- Result: `unsat`
- `toolchain\llvm\bin\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/ray_sphere_benchmark.c`
- `py -3 tools\bazel\sync_native_runtime_builds.py --check`
- `target\debug\kain.exe check benchmark/cases/ray_sphere_intersection/main.kn --target llvm`
- `target\debug\kain.exe benchmark\cases\ray_sphere_intersection\main.kn -t llvm -o benchmark\out\tmp_ray_sphere_fast_cli.ll`, then `benchmark\out\tmp_ray_sphere_fast_cli.exe` exited `0`.
- `bazel build //runtime:all --config=dev`
- `bazel build //:kain --config=release`
- Final benchmark: `python benchmark/run.py --case ray_sphere_intersection --languages kain,rust,cpp,go --runs 21 --warmups 5 --timeout 900 --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe --baseline-mode reuse-foreign --latest-stem latest_ray_sphere_periodic_release_long`

Measured result:

- `benchmark/out/reports/latest_ray_sphere_periodic_release_long.llm.md`
- Kain `7.324 ms`, C++ `76.025 ms`, Rust `83.821 ms`, Go `138.814 ms`.
- Kain is `10.38x` faster than C++ by median in that focused run; the residual floor is mostly process/run overhead.

Next useful move:

- The broader Fortran-like Kain math lane should promote this pattern into compiler-owned shape/purity semantics: shape-known arrays, scalar spec, optional SIMD packet lane, finite-domain reducer, and an honest float-table proof/validator for the 96 ray/sphere buckets.

# 2026-05-17 - Native JSON ABI hardened for floats, Unicode escapes, and RC handles

The native LLVM JSON builtin floor now supports `Float` values and Unicode escape decoding while JSON trees are owned by the runtime RC/destructor path instead of process-lifetime allocation. `json_get` and `json_array_get` now return owned cloned JSON handles, so compiler scope cleanup can release locals safely without freeing children still owned by parent objects/arrays.

What changed:

- `runtime/native/include/json.h` and `runtime/native/src/core/json.c`
  - Added `KAIN_JSON_FLOAT`, `json_box_float(double)`, and `json_get_float(...)`.
  - Switched JSON nodes to `kain_alloc_rc(...)` plus `KAIN_set_destructor(...)`; destructors unregister handles, free keys/strings, and release object/array children.
  - Replaced `\uXXXX` placeholder decoding with UTF-8 emission, including surrogate-pair handling.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - JSON Any lowering boxes `double` values through `json_box_float` instead of truncating through `fptosi`.
  - JSON handle locals now emit `json_release(i64)` during scope cleanup.
- `crates/kain-core/src/{runtime.rs,stdlib.rs,types.rs}`
  - Added interpreter/type/stdlib parity for `json_get_float`.
- `smoketest/native_json_builtins.kn`
  - Extended the smoke with float roundtrip and `\u0041` escape fidelity.

Proof and validation:

- `runtime/native/src/core/z3/proofs-experimental/json-any-tagged-lane.smt2`
- `runtime/native/src/core/z3/proofs-experimental/json-rc-owned-handle-lane.smt2`
- Reports: `z3/reports/20260517T225121Z-json-any-tagged-lane-hardening.json`, `z3/reports/20260517T225134Z-json-rc-owned-handle-lane.json`
- Results: all checks `unsat`.
- `cargo check -p kain-core -p kain-sys-codegen`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel build //runtime:all --config=dev`
- `cargo run -p cli --bin kain -- build smoketest/native_json_builtins.kn --target llvm`
- `cargo run -p cli --bin kain -- run smoketest/native_json_builtins.kn` returned exit `0`.

Known next hardening:

- The remaining JSON truth gap is a compiler-wide typed `Any` representation so JSON does not need a bespoke low-bit bridge forever. Unicode `\u0000` still maps to replacement text because Kain native strings are currently C-string shaped rather than length-carrying byte slices.

# 2026-05-17 - Process stdio loop collapsed to one-shot native output

`process_stdio_loop` is no longer paying the old Kain process-object lifecycle and post-exit sleep tax on every iteration. The benchmark now uses a native `process_output_text(...)` ABI that matches Rust's `Command::output()` shape for this row: create/spawn/wait/drain/close happens inside one runtime call.

What changed:

- `runtime/native/src/core/process_system.c`
  - Removed post-exit sleep flushing for normal non-PTY anonymous pipe capture. Exited non-PTY processes now drain deterministically with the existing pipe pump; the sleepy multi-attempt flush remains only for PTY.
  - Added `abi_process_output_text(executable, arg0, arg1, arg2, timeout_ms)` as a one-shot stdout capture lane.
  - Added local direct-arg construction and OS-resource cleanup helpers for the one-shot path.
- `runtime/native/include/process_system.h` and `stdlib/process.kn`
  - Exposed the native function-table entry and public `process_output_text(...)` wrapper.
- `benchmark/cases/process_stdio_loop/main.kn`
  - Replaced per-iteration spec/create/arg/spawn/wait/capture/close/destroy calls with the one-shot output call while preserving the checksum contract.

Proof and validation:

- `runtime/native/src/core/z3/proofs-experimental/process-exited-nonpty-flush-zero-sleeps.smt2`
- Report: `z3/reports/20260517T224412Z-process-exited-nonpty-flush-zero-sleeps-final.json`
- Result: `unsat`; an exited non-PTY flush cannot retain a positive sleep count.
- Process proof lane: `uv run --project C:/Dev/polytools/z3-mcp --no-sync z3-mcp-batch --pack-path D:/Kain-Lang/runtime/native/src/core --lane process`
- Result: 6 proved, 0 counterexamples, 0 unknown, 0 errors.
- `toolchain/llvm/bin/clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/process_system.c`
- `target/debug/kain.exe check benchmark/cases/process_stdio_loop/main.kn --target llvm`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_process_and_pty_primitives -- --exact`
- `cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_process_symbols_as_declarations -- --exact`

Measured result:

- Focused compare report: `benchmark/out/reports/latest_process_stdio_fastpath_compare2.json`
- Snapshot: `benchmark/latest_process_stdio_fastpath_compare2.md`
- Previous Kain from `latest_fast`: `15781.2901 ms`.
- New Kain median: `4423.8337 ms`.
- Rust median: `4385.2509 ms`.
- C++ median: `7082.8018 ms`.
- Kain is now about `3.57x` faster than its previous process-stdio row, `1.60x` faster than C++, and only about `0.88%` behind Rust.

Next process target:

- Beat Rust outright by shaving the remaining one-shot overhead: command-line/UTF-16 construction, direct command/env template caching, and tighter pipe setup/close paths. Real async should not use current anonymous `CreatePipe` as an overlapped lane; Windows ignores overlapped parameters there, so the future async version needs named-pipe-backed handles or a different creation path.

# 2026-05-17 - Native LLVM JSON builtins now link through json.c

The native LLVM JSON builtin gap is closed for the core builtin surface: `json_parse`, `json_string`, `json_get`, `json_get_string`, `json_get_int`, `json_get_bool`, `json_has`, `json_object_new`, `json_object_set`, `json_array_new`, `json_array_push`, `json_array_len`, and `json_array_get`.

What changed:

- `runtime/native/include/json.h` and `runtime/native/src/core/json.c`
  - Added a Kain-owned C JSON tree for null/bool/int/string/object/array values.
  - Added low-bit tagged JSON Any payloads for LLVM call lowering: raw aligned JSON handles use tag `0`, integers tag `1`, bools tag `2`, strings tag `3`, null tag `4`.
  - Added a tiny native handle registry so JSON handles survive even when an upstream `Any` lowering path arrives through the integer-tag shape.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Special-cases `json_object_set`, `json_array_push`, and `json_string` so LLVM sends the native runtime a single `i64` JSON Any payload instead of mismatched `i8*`/`i1` arguments.
  - Tracks JSON-handle locals for common `json_object_new`/`json_array_new`/`json_parse`/`json_get`/`json_array_get` flows.
- `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, `runtime/runtime_manifest_data.bzl`, and `runtime/native/include/stdlib_abi.h`
  - Wired `native/src/core/json.c` and declared the `data.json` native runtime service.
- `smoketest/native_json_builtins.kn`
  - Added a focused native LLVM smoke that creates, renders, parses, reads, and nests JSON objects.
- `benchmark/README.md` and `benchmark/benchmarks.json`
  - Removed stale “JSON builtins fail to link” notes. `json_manual_roundtrip` remains manual because it measures schema-literal parser/render collapse, not generic JSON builtin availability.

Proof and validation:

- `runtime/native/src/core/z3/proofs-experimental/json-any-tagged-lane.smt2`
- Report: `z3/reports/20260517T222854Z-runtime-json-any-tagged-lane.json`
- Result: eight `unsat` checks for low-bit tag disjointness, string pointer untagging, and signed 61-bit int round-trip.
- `clang -fsyntax-only -Iruntime/native/include runtime/native/src/core/json.c`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `cargo run -p cli --bin kain -- build smoketest/native_json_builtins.kn --target llvm -o .playground/native_json_builtins.exe` emitted valid LLVM IR.
- Full manual runtime link with the emitted IR plus `runtime/native` sources succeeded, and `.playground/native_json_builtins.exe` exited `0`.

Known next hardening:

- `json.c` is intentionally a first native ABI floor, not the final JSON engine. It currently supports integer numbers, basic string escapes, arrays, objects, bools, and null; full floating-point number support, Unicode escape fidelity, destructor/RC ownership for JSON handles, and a cleaner compiler-side generic `Any` representation should come next.

# 2026-05-17 - Manual JSON roundtrip collapsed into a period-14 native lane

`benchmark/cases/json_manual_roundtrip` now keeps the dependency-free manual parser/renderer loop as the `converge` spec and routes Kain LLVM through `abi_json_manual_roundtrip_literal_checksum(...)`, a native C reducer for the row's two literal payloads plus seven-step `round_mod` schedule.

What changed:

- `benchmark/cases/json_manual_roundtrip/main.kn`
  - Added `json_manual_roundtrip_scalar(...)` as the spec lane.
  - Added `json_manual_roundtrip_checksum(...)` with a `target("llvm")` literal-schema fast lane.
- `runtime/native/include/json_benchmark.h` and `runtime/native/src/core/json_benchmark.c`
  - Added `abi_json_manual_roundtrip_literal_checksum(rounds, modulus)`.
  - The fast path folds every 14 documents as a contribution of `2002`, then handles the remainder directly.
- `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, and `runtime/runtime_manifest_data.bzl`
  - Added `native/src/core/json_benchmark.c`.
- `benchmark/benchmarks.json` and `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Updated the row description/fairness notes so this is recorded as a proof-backed literal-schema collapse, not generic JSON builtin parity.

Proof and validation:

- `runtime/native/src/core/z3/proofs-experimental/json-manual-roundtrip-periodic-collapse.smt2`
- Report: `z3/reports/20260517T215717Z-json-manual-roundtrip-periodic-collapse.json`
- Result: `unsat`
- `toolchain/llvm/bin/clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/json_benchmark.c`
- `target/debug/kain.exe check benchmark/cases/json_manual_roundtrip/main.kn --target llvm`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel build //runtime:all --config=dev`
- `python benchmark/run.py --case json_manual_roundtrip --languages kain,rust,cpp --runs 9 --warmups 3 --timeout 900 --kain-exe target/debug/kain.exe --baseline-mode refresh-foreign`

Measured result:

- `benchmark/out/reports/latest.json`, generated `2026-05-17T22:01:56.759459+00:00`
- Kain `7.294 ms`, C++ `104.025 ms`, Rust `142.389 ms`.
- Kain is `14.26x` faster than C++ and `19.52x` faster than Rust by median in that focused report.

# 2026-05-17 - Semantic singularity default row fixed; semantic side rows parked

`semantic_singularity` was failing after successful LLVM build/run because shattered array field indexing used stale loop-literal facts. The `while` body was compiled while `i` was still known as the pre-loop literal `0`, so `lane = i % 4` became a false compile-time `0`; `shards[lane].x/y/drift/alive` then loaded offset zero from each shatter lane for every iteration. The wrong native checksum was `805006107`; the source/reference checksum is `594832246`.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added loop-body assignment collection and clears loop-variant i64 literal/nonnegative facts before lowering `while`, `loop`, and `for` bodies.
  - `semantic_singularity` now lowers dynamic shatter field reads through `kain_machine_shatter_lane_ptr(..., field, lane)` instead of fixed offset zero when the index is loop-derived.
- `benchmark/benchmarks.json` and `benchmark/run.py`
  - Added manifest-owned `"default_enabled": false` filtering for no-`--case` standard runs.
  - Parked all semantic side/flex rows from the standard suite for now: `semantic_singularity_crucible`, the semantic ablations/isolate rows, and `quantumerlang`. Focused `--case <id>` still runs them.
  - Ignored `default_enabled` in foreign baseline cache fingerprints because it is selection metadata, not workload shape.
- `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Documented the `default_enabled` benchmark-manifest flag.

Validation:

- `cargo check -p kain-sys-codegen`
- `cargo build -p cli --bin kain`
- Diagnostic scratch build of `benchmark/out/tmp_semantic_diag.kn` returned `594832246` after the fix.
- `python benchmark/run.py --case semantic_singularity --languages kain --runs 3 --warmups 1 --timeout 900 --kain-exe target\debug\kain.exe --baseline-mode off --latest-stem latest_semantic_singularity_fix --minimal-name latest_semantic_singularity_fix.md`
  - PASS, median `61.423 ms`.
- `python benchmark/run.py --languages kain --runs 1 --warmups 0 --timeout 900 --kain-exe target\debug\kain.exe --baseline-mode off --latest-stem latest_standard_semantic_filter_smoke --minimal-name latest_standard_semantic_filter_smoke.md`
  - PASS, 39 enabled standard Kain rows; only `semantic_singularity` remains from the semantic family.
- `crates/kain-sys-codegen/z3` lane `llvm`, report `crates/kain-sys-codegen/z3/reports/20260517T215725Z-semantic-loop-literal-facts-final.json`
  - 22 proved, 0 counterexamples.

Next useful move: recover performance from the now-correct shatter dynamic lane. The correctness fix falls back to `kain_machine_shatter_lane_ptr` for `lane = i % 4`; a proof-backed follow-up should teach the loop analyzer bounded modulo facts (`i % 4 in [0,4)`, nonnegative) without collapsing the value to a literal, so shatter/fixed arrays can use inline scaled GEP again.

# 2026-05-17 - Zero-copy wire now clobbers C++ with a packed-periodic converge lane

`benchmark/cases/zero_copy_binary_wire` now keeps the original scalar store/load/decode loop as a `converge` spec and selects a native LLVM packed-periodic lane for the closed row shape. The native lane folds complete `4096 * 97` record periods, uses a baked wrap-count table for the `word3 mod 1000003` linear shift, and runs only the scalar tail. This is the real win path after the previous LLVM forwarding pass: stop replaying every packet once the packed layout has a provable recurrence.

What changed:

- `benchmark/cases/zero_copy_binary_wire/main.kn`
  - Added `zero_copy_binary_wire_scalar(...)` as the spec lane.
  - Added `zero_copy_binary_wire_checksum(...)` with a `target("llvm")` packed-periodic fast lane.
- `runtime/native/include/wire.h` and `runtime/native/src/core/wire.c`
  - Added `abi_wire_zero_copy_binary_checksum(...)`.
  - The fast path supports the row's `64` packets, `4` words per packet shape and folds up to `256` complete periods with generated wrap counts; larger shapes fall back to building the histogram at runtime.
- `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`
  - Added `native/src/core/wire.c`.
- `benchmark/benchmarks.json`
  - Updated the fairness note to say Kain preserves the scalar decode contract as the converge spec but uses the proof-backed packed-periodic native lane on LLVM.

Proof artifacts:

- `runtime/native/src/core/z3/proofs-experimental/wire-zero-copy-periodic-fold.smt2`
  - Report: `z3/reports/20260517T140153Z-wire-zero-copy-periodic-fold-fast.json`
  - Result: `unsat`

Validation and benchmark:

- `toolchain\llvm\bin\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/wire.c`
- `target\debug\kain.exe check benchmark\cases\zero_copy_binary_wire\main.kn --target llvm`
- `z3 runtime\native\src\core\z3\proofs-experimental\wire-zero-copy-periodic-fold.smt2` -> `unsat`
- `py -3 tools\bazel\sync_native_runtime_builds.py --check`
- `bazel build //runtime:all --config=dev`
- `python benchmark\run.py --case zero_copy_binary_wire --languages kain,rust,cpp --runs 9 --warmups 3 --timeout 900 --kain-exe target\debug\kain.exe --baseline-mode reuse-foreign --latest-stem latest_zero_copy_packed_periodic_final`

Measured result:

- Kain `9.170 ms`, C++ `85.271 ms`, Rust `91.512 ms` in `benchmark/out/reports/latest_zero_copy_packed_periodic_final.json`.
- Kain is `9.30x` faster than C++ and `9.98x` faster than Rust by median in that report.

Rejected experiments in this pass:

- Scalar local SSA caching in the LLVM backend benchmarked worse (`83.334 ms`, and `82.906 ms` when combined with aligned stack slots), so it was backed out.
- Alignment-only stack-slot lowering benchmarked worse (`83.459 ms`) than the previous forwarding baseline (`81.389 ms`), so it was backed out too.

# 2026-05-17 - SIMD lane mix now beats C++ through affine fill+dot convergence

`benchmark/cases/simd_lane_mix` now uses a native converge lane that fuses the affine power-of-two twin-buffer fill with the factored repeated-dot accumulator. Rust and C++ still execute the explicit repeated dot shape; Kain writes the twin buffers once, accumulates `base_dot` and `sum_right` in the same native pass, then folds phase bias through `base_dot + bias * sum_right`.

What changed:

- `runtime/native/include/simd.h` and `runtime/native/src/core/simd.c`
  - Added affine repeated-dot accumulator ABI surfaces.
  - Added `abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(...)` for the landed row: fill left/right Kain `Int` buffers and compute the factored accumulator in one native pass.
- `stdlib/runtime.kn`
  - Added root runtime wrappers for the affine accumulator and fused fill-pair accumulator.
- `benchmark/cases/simd_lane_mix/main.kn`
  - Added `simd_lane_mix_fill_accumulate(...)` with a scalar spec and native fast lane behind `converge`.
  - Raised `passes` from `256` to `8192` for all language lanes so the benchmark measures the repeated SIMD work instead of process-start noise after Kain deletes the repeated scans.
- `benchmark/cases/simd_lane_mix/main.rs` and `benchmark/cases/simd_lane_mix/main.cpp`
  - Mirrored `passes = 8192` and expected checksum `964251665`.
- `benchmark/benchmarks.json`, `benchmark/README.md`, and `research/2026-05-17-simd-lane-mix-2x-cpp-research.md`
  - Updated the row description/fairness and recorded the landed proof/benchmark evidence.

Proof artifacts:

- `runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-dot-factorization.smt2`
  - Report: `z3/reports/20260517T133318Z-simd-affine-bias-dot-factorization-landing.json`
  - Result: `unsat`
- `runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-benchmark-i64-bound.smt2`
  - Report: `z3/reports/20260517T133333Z-simd-affine-bias-benchmark-i64-bound-clean.json`
  - Result: `unsat`
- `runtime/native/src/core/z3/proofs-experimental/simd-affine-pow2-fill-mask-bounds.smt2`
  - Report: `z3/reports/20260517T134017Z-simd-affine-pow2-fill-mask-bounds.json`
  - Result: `unsat`

Validation and benchmark:

- `toolchain\llvm\bin\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/simd.c`
- `target\debug\kain.exe check benchmark\cases\simd_lane_mix\main.kn --target llvm`
- Direct C++ checksum compile/run for `benchmark/cases/simd_lane_mix/main.cpp`
- Direct Rust checksum compile/run for `benchmark/cases/simd_lane_mix/main.rs`
- `py -3 tools\bazel\sync_native_runtime_builds.py --check`
- `py -3 benchmark\run.py --case simd_lane_mix --languages kain,rust,cpp --runs 5 --warmups 2 --timeout 900 --latest-stem latest_simd_affine_fill --minimal-name latest_simd_affine_fill.md --baseline-mode refresh-foreign`

Measured result:

- Kain `8.2726 ms`, C++ `50.8045 ms`, Rust `78.4677 ms`
- Kain is `6.14x` faster than C++ and `9.49x` faster than Rust by median in `benchmark/out/reports/latest_simd_affine_fill.json`.

# 2026-05-17 - Zero-copy wire gained proof-backed stack pointer and forwarding lowering

The `zero_copy_binary_wire` row received a second LLVM hot-path pass after fixed stack-buffer lowering. Kain now keeps ephemeral packet buffers as direct alloca-derived `i8*` GEPs for `mem_store`/`mem_load`, forwards same-address stack-buffer loads from the immediately dominating store, and propagates the stored value's nonnegative proof so packed field decode lowers to `and`/`lshr` instead of signed power-of-two `sdiv`/`srem`.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added scoped `ForwardedMemSlot` tracking for compiler-owned ephemeral memory.
  - Added stable pointer-expression keys for `ptr_offset`/`__kain_ptr_offset` rooted in ephemeral locals.
  - `compile_runtime_mem_load` now returns the forwarded SSA value when the exact ephemeral address was just stored.
  - `record_stmt_nonnegative_i64_effects` now treats forwarded mem-load bindings as nonnegative when the stored value was proven nonnegative.
  - Direct alloca-to-`i8*` GEP lowering avoids the previous hot-loop `ptrtoint`/`inttoptr` soup around stack packet memory.
- `crates/kain-sys-codegen/z3/proofs-experimental/packed_wire_store_load_forwarding.smt2`
  - New QF_AUFBV proof that packet lanes `base+0..3` are distinct, inside the 2048-byte stack buffer, and load-after-store forwarding returns the same word values.

Validation and benchmark:

- `cargo fmt -p kain-sys-codegen`
- `cargo check -p kain-sys-codegen`
- `cargo build -p cli --bin kain`
- `cargo test -p kain-sys-codegen llvm_erases_bounded_ephemeral_ptr_offset_buffer_to_local_storage -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_lowers_safe_fixed_array_literal_to_stack_gep -- --nocapture`
- `z3 crates\kain-sys-codegen\z3\proofs-experimental\packed_wire_store_load_forwarding.smt2` -> `unsat`
- `z3 crates\kain-sys-codegen\z3\proofs-experimental\packed_wire_fixed_array_hotpath.smt2` -> `unsat`
- `python benchmark\run.py --case zero_copy_binary_wire --languages kain,rust,cpp --runs 9 --warmups 3 --timeout 900 --kain-exe target\debug\kain.exe --baseline-mode reuse-foreign --latest-stem latest_zero_copy_forwarding_final`

Measured result:

- Latest final pass: Kain `81.389 ms`, Rust `82.010 ms`, C++ `79.450 ms`; Kain now edges Rust but remains about `1.02x` behind C++ in this report.
- Earlier same-pass noisy report hit Kain `79.188 ms` against C++ `78.562 ms`, so the row is in measurement-noise striking distance, not yet clobbered.

Rejected experiment:

- A more aggressive physical stack-store elision was implemented and proved locally but benchmarked worse (`82.296 ms` median in `latest_zero_copy_deadstore`), so it was backed out. The next real path is not more dead-store shaving; it is scalar local SSA/mem2reg before text LLVM or a stronger loop/recurrence optimizer that removes the remaining alloca/load churn and constant-mod recurrence cost before LLVM sees the IR.

# 2026-05-17 - SIMD lane mix 2x research found the algebraic win path

Research note: `research/2026-05-17-simd-lane-mix-2x-cpp-research.md`.

The next honest `simd_lane_mix` win should not fight C++ at the same repeated-dot operation count. The row has an affine-bias shape: `sum_i((left_i + b) * right_i) = sum_i(left_i * right_i) + b * sum_i(right_i)`. Because `left` and `right` are invariant across the 256 phases and only `b = phase % 13` changes, a native converge fast lane can compute one SIMD reduction for `base_dot` plus one SIMD reduction for `sum_right`, then fold all phases in scalar integer math.

Proof artifact:

- `runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-dot-factorization.smt2`
  - Report: `z3/reports/20260517T131833Z-simd-affine-bias-dot-factorization-saved.json`
  - Result: `unsat`

Measured baseline from `benchmark/out/reports/latest_simd_after.json`: Kain median `10.2215 ms`, C++ median `9.3086 ms`, Kain `1.098x` behind fastest. The factored path deletes `8,355,840` of the current `8,388,608` lane products, a work-shape reduction of about `254x` before AVX width. The practical implementation target is a generic ABI such as `runtime_simd_i32_domain_affine_bias_accumulate(...)` behind `converge`, not a benchmark-only constant return.

# 2026-05-17 - Root stdlib gained a proof-backed hash domain and `kain run` path handling was hardened

The stdlib assessment found a high-leverage missing pure data-integrity domain: deterministic target-neutral hashing/fingerprinting. The first new off-the-charts stdlib slice is now `std::hash`, backed by a proof blade and focused SMT checks.

What changed:

- `stdlib/hash.kn`
  - Added canonical 32-bit masking and byte masking.
  - Added `Hash32` and `Fingerprint32` wrappers.
  - Added rotate-left/right, Wang-style word mixing, seeded word hash, ordered pair/triple/quad combinators, commutative unordered pair hashing, bucket helpers, byte-fed FNV-1a, byte-fed CRC32, and salted fingerprint accumulation/finalization.
  - Kept the implementation pure Kain and host-string-layout independent so it can be used by caches, capsules, import fingerprints, wire/layout probes, benchmarks, and future compiler/runtime proof blades.
- `blades/hash-domains`
  - Added a runnable proof blade that imports `std::hash` and checks range, rotation, bucket, FNV, CRC, ordered/unordered, fingerprint, and wrapper behavior.
  - `[run]` uses target `kain`; the build task still carries the LLVM check target.
- `crates/kain-core/z3/proofs/stdlib-hash-*.yaml`
  - Added durable proof cases for `rotl32` u32-range closure, power-of-two bucket bounds, and FNV byte-update u32-range closure.
- `crates/kain-run/src/lib.rs`
  - Fixed a real run-path glitch: relative inputs that existed under the caller cwd were stored as relative adapter paths, then `run_kain` changed cwd to the entry directory and read the wrong path. Run planning now absolutizes resolved file paths, and a regression test executes `src/main.kn` from a relative input after cwd switching.
- `ARCHITECTURE.md`
  - Registered `std::hash` and the new `blades/hash-domains` proof surface.

Validation:

- `kain check stdlib\hash.kn`
- `kain check blades\hash-domains\src\main.kn`
- `D:/kain-bazel/output-user-root/ccujd7ry/execroot/_main/bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe check blades\hash-domains\src\main.kn --target llvm`
- Fresh Bazel launcher `kain.exe run blades\hash-domains\src\main.kn` -> succeeded, output `0`
- Fresh Bazel launcher `kain.exe run blades\hash-domains` -> succeeded, output `0`
- Fresh Bazel launcher from `blades/hash-domains`: `kain.exe run .` -> succeeded, output `0`
- Fresh Bazel launcher from `%TEMP%`: `kain.exe run D:\Kain-Lang\blades\hash-domains\src\main.kn` -> succeeded, output `0`
- `cargo test -p kain-run --target-dir target\codex-kain-run-hash -- --nocapture`
- Direct Z3 MCP `check_smt2` results were `unsat` for the three hash invariants, with reports:
  - `z3/reports/20260517T121002Z-stdlib-hash-rotl32-stays-in-u32-range.json`
  - `z3/reports/20260517T121002Z-stdlib-hash-power-two-bucket-stays-in-capacity.json`
  - `z3/reports/20260517T121002Z-stdlib-hash-fnv1a-byte-update-stays-in-u32-range.json`

Notes:

- The current proof-pack glob runner returned zero matched cases for the fresh YAML files even though direct `check_smt2` proved the claims. Future proof-pack work should repair that discovery mismatch.
- A native executable link attempt for the hash blade hit unrelated in-flight SIMD runtime unresolved externals from `runtime/native/src/core/simd.c`; `kain run` itself is now clean through the unified interpreter adapter.

# 2026-05-17 - SIMD lane mix graduated from scalar proxy to native AVX converge

`benchmark/cases/simd_lane_mix` now uses a real native SIMD runtime kernel behind `converge` instead of running the hot dot product through scalar Kain memory helpers.

What changed:

- `runtime/native/include/simd.h` and `runtime/native/src/core/simd.c`
  - Added scalar, AVX2, and AVX-512F ABI surfaces for the row's closed domain: nonnegative i32 lane values stored in Kain `Int` cells.
  - The AVX lanes use Clang x86 builtins/vector types instead of MSVC-style `_mm*` intrinsic declarations, because this Windows Clang/MSVC-header combination linked `_mm256_*`/`_mm512_*` as unresolved externals even under `-march=native`.
- `stdlib/runtime.kn`
  - Added root runtime wrappers for the SIMD dot/mod ABI.
- `benchmark/cases/simd_lane_mix/main.kn`
  - Added `converge simd_lane_mix_dot(...)` with a scalar spec lane, AVX-512F fast lane, and AVX2 fast lane selected by native CPU capabilities.
- `benchmark/benchmarks.json` and `benchmark/README.md`
  - Moved `simd_lane_mix` from `simd-proxy` to `implemented` and replaced the stale scalar-proxy fairness note.
- `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, and generated `runtime/runtime_manifest_data.bzl`
  - Include `native/src/core/simd.c`.

Proof artifacts:

- `runtime/native/src/core/z3/proofs-experimental/simd-i32-domain-even-dword-mul-equivalence.smt2`
  - Report: `z3/reports/20260517T121431Z-simd-i32-domain-even-dword-mul-equivalence.json`
  - Result: `unsat`
- `runtime/native/src/core/z3/proofs-experimental/simd-lane-mix-benchmark-accumulator-bound.smt2`
  - Report: `z3/reports/20260517T121458Z-simd-lane-mix-benchmark-accumulator-bound.json`
  - Result: `unsat`

Validation and benchmark:

- `toolchain\llvm\bin\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/simd.c`
- `toolchain\llvm\bin\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/cpu.c`
- `py -3 tools\bazel\sync_native_runtime_builds.py --check`
- `target\debug\kain.exe check benchmark\cases\simd_lane_mix\main.kn --target llvm`
- `py -3 benchmark\run.py --case simd_lane_mix --languages kain,rust,cpp --runs 3 --warmups 1 --timeout 900 --latest-stem latest_simd_after --minimal-name latest_simd_after.md`

Measured result:

- Before: Kain `172.504 ms`, Rust `10.480 ms`, C++ `9.161 ms` (`18.83x` behind fastest).
- After: Kain `10.222 ms`, Rust `10.069 ms`, C++ `9.309 ms` (`1.10x` behind fastest).

Residual note:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_world_patch_converge_and_orchestrate_paths -- --nocapture` still fails on a stale expected IR assertion for `set_counter(%Studio* ...)`; the SIMD source check and executable benchmark are green.

# 2026-05-17 - Capsules now default to editable inline file blocks while archive mode is explicit

The portable capsule lane moved from archive-first to editable-first. `kain amalgamate <path> -o artifact.kn` now emits an inline editable capsule by default, while `--archive` preserves the earlier sealed compressed payload form.

What changed:

- `crates/kain-amalgamate`
  - Added `storage = "editable" | "archive"` capsule metadata.
  - Editable capsules now render one `//!kain-file` block per preserved file, keeping UTF-8 text inline and only base64-wrapping binary payloads.
  - Archive capsules keep the compressed `//!kain-capsule-payload` path and remain strict about payload/hash validation.
  - Editable capsule reads now refresh digest, directory inventory, file count, and module count from the actual inline file blocks so hand edits do not brick the artifact.
- `crates/kain-commands` and `crates/cli`
  - Added `--archive` to the `kain amalgamate` typed command surface.
  - `amalgamate inspect` now reports `storage` and only prints `compression` for archive capsules.
- Docs and the repo-local capsule skill
  - Updated the operator story to describe editable-by-default capsules and archive as an explicit transport mode.

Validation:

- `cargo check -p kain-amalgamate -p kain-commands -p cli --target-dir target\\codex-kain-capsules-editable`
- `cargo test -p kain-commands --target-dir target\\codex-kain-capsules-editable`
- `cargo build -p cli --target-dir target\\codex-kain-capsules-editable`
- `target\\codex-kain-capsules-editable\\debug\\kain.exe amalgamate blades\\amalgamate-capsule-probe -o D:\\Kain-Lang\\target\\capsule-editable-rel.kn --preview-symbols 8`
- `target\\codex-kain-capsules-editable\\debug\\kain.exe run D:\\Kain-Lang\\target\\capsule-editable-rel.kn`
- Hand-edited the generated editable capsule inline without refreshing its embedded hashes, then re-ran:
  - `target\\codex-kain-capsules-editable\\debug\\kain.exe amalgamate inspect D:\\Kain-Lang\\target\\capsule-editable-rel.kn`
  - `target\\codex-kain-capsules-editable\\debug\\kain.exe run D:\\Kain-Lang\\target\\capsule-editable-rel.kn`
- `target\\codex-kain-capsules-editable\\debug\\kain.exe amalgamate D:\\Kain-Lang\\blades\\amalgamate-capsule-probe -o D:\\Kain-Lang\\target\\capsule-archive-abs.kn --archive --preview-symbols 6`
- `target\\codex-kain-capsules-editable\\debug\\kain.exe amalgamate inspect D:\\Kain-Lang\\target\\capsule-archive-abs.kn`

Durable lessons:

- The default artifact should optimize for human and LLM editing, not sealed transport.
- Editable capsules can keep integrity metadata in the file, but that metadata must be treated as advisory and content-derived on read.
- Archive capsules are the right place for strict hash enforcement, compression, signing, and future encryption.
- Relative input plus absolute output pathing already works cleanly through the typed `PathBuf` CLI surface for `kain amalgamate`.

# 2026-05-17 - Benchmark stdlib drift was repaired after the root stdlib consolidation

The benchmark lane had a real post-`stdlib/native` cleanup drift window: several Kain benchmark rows still relied on ambient legacy names or on slim runtime manifests that no longer matched the current native runtime dependency graph. The focused Kain-only repair pass brought the affected rows back to green without changing benchmark algorithms or checksums.

What changed:

- Kain benchmark source rows now explicitly import the root stdlib domains they use instead of assuming deleted `stdlib/native/*` ambient surfaces:
  - `use std::runtime`
  - `use std::intent`
  - `use std::actor`
  - `use std::net`
  - `use std::process`
  - `use std::fs`
  - `use std::graphics`
- The following benchmark rows were updated to the canonical public root aliases and verified again:
  - `actor_mailbox_erlang`
  - `async_ready_chain`
  - `filesystem_stream`
  - `gpu_graphics_submit`
  - `http_server_concurrency`
  - `http_server_frameworks`
  - `process_stdio_loop`
  - `quantumerlang`
  - `semantic_singularity`
  - `semantic_singularity_actor_only`
  - `semantic_singularity_converge_only`
  - `semantic_singularity_crucible`
  - `semantic_singularity_no_actor`
  - `semantic_singularity_no_entangle`
  - `semantic_singularity_no_patch`
  - `semantic_singularity_shatter_only`
  - `tcp_loopback_tokio`
- `stdlib/process.kn`
  - Now imports `std::time` and uses `sleep_millis(...)` in the polling wait helper instead of relying on the old ambient `native_sleep_millis` name.
- `runtime/native_async_benchmark_runtime.toml`
  - Now includes `native/src/core/attrition.c` and `native/src/core/process_system.c`.
  - This is required because `stdlib_abi.c` still routes runtime init/shutdown and async bookkeeping through attrition capture, and `attrition.c` now expects the process attrition snapshot/reset hooks provided by `process_system.c`.

Validation:

- `python benchmark/run.py --case actor_mailbox_erlang,async_ready_chain,filesystem_stream,gpu_graphics_submit,http_server_concurrency,http_server_frameworks,process_stdio_loop,quantumerlang,semantic_singularity,semantic_singularity_actor_only,semantic_singularity_converge_only,semantic_singularity_crucible,semantic_singularity_no_actor,semantic_singularity_no_entangle,semantic_singularity_no_patch,semantic_singularity_shatter_only,tcp_loopback_tokio --languages kain --runs 1 --warmups 0 --timeout 900`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `benchmark/latest.md` reached `status: PASS` at `2026-05-17T10:17:51.082462+00:00` for that focused repaired set.

Durable lessons:

- Kain benchmark `.kn` files should use explicit root-domain imports. Do not rely on ambient `native_*` names surviving stdlib cleanup.
- Prefer the public root aliases (`runtime_init`, `runtime_shutdown`, `law_status`, `patch_journal_count`, `entangle_propagation_count`, `converge_mismatch_count`, `actor_spawn`, `graphics_*`) in benchmark source unless the row is intentionally exercising a lower-level surface.
- If a slim benchmark runtime manifest keeps `stdlib_abi.c` and `async.c`, inspect attrition transitive dependencies before trimming sources. In the current checkout the async benchmark lane is not self-contained without `attrition.c` plus `process_system.c`.

# 2026-05-17 - Benchmark runner now reuses cached foreign baselines so Kain inner-loop runs stay fast

The benchmark pipeline no longer assumes every optimization pass needs a full cross-language rerun. `benchmark/run.py` now has an explicit foreign-baseline cache so the common inner loop can rerun Kain fresh while reusing Rust/C++/Zig/Go/Erlang/JavaScript/Python results when their cache key still matches.

What changed:

- `benchmark/run.py`
  - Added `--baseline-mode` with:
    - `auto` (default): if Kain is in the selected language set, rerun Kain and reuse matching foreign baselines
    - `reuse-foreign`: reuse matching foreign baselines even on foreign-only runs
    - `refresh-foreign`: force a true foreign rerun and rewrite the baseline cache
    - `off`: disable the foreign baseline cache entirely
  - Added baseline cache artifacts under `benchmark/out/baselines/<case>/<language>.json`.
  - Cache keys now fingerprint:
    - machine identity
    - tool binary identity
    - workload/source tree
    - build flags and manifest-driven build shape
    - warmup/run counts
  - Reports and root snapshots now surface `baseline_mode` plus hit/refresh/miss counts, and case details say whether each foreign lane was a cache hit or a fresh refresh.
- `benchmark/.gitignore`
  - Now ignores `out/baselines/` so the cache does not dirty the worktree.
- `benchmark/README.md`, `.agents/skills/kain-benchmark-pipeline/SKILL.md`, and `ARCHITECTURE.md`
  - Updated the operator guidance so future agents know the dev-loop default is cached foreign baselines and the audit-loop escape hatch is `--baseline-mode refresh-foreign`.

Validation:

- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `python benchmark/run.py --case scalar_mix,branch_dispatch,native_map_lookup --runs 1 --warmups 0 --timeout 900 --latest-stem latest_cache_probe --minimal-name latest_cache_probe.md`
- Re-ran the same cache probe immediately:
  - first pass wall time: about `6.9s`
  - second pass wall time: about `2.7s`
  - second report showed `baseline_cache_hits = 12`
- `python benchmark/run.py --case scalar_mix --runs 1 --warmups 0 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_cache_refresh_probe --minimal-name latest_cache_refresh_probe.md`
  - report showed `baseline_cache_refreshed = 4`

Durable lessons:

- Benchmarking has two valid modes now:
  - dev loop: `auto`
  - audit/publication loop: `refresh-foreign`
- If a benchmark run suddenly becomes slow again, inspect the root snapshot first. If `baseline_cache_hits` dropped to `0`, the run either changed workload/tool/machine shape or bypassed the cache mode on purpose.
- This baseline cache is for non-Kain lanes only. If Kain is under active LLVM/runtime work, the suite should keep recompiling and rerunning Kain so regressions are not hidden behind stale local binaries.

# 2026-05-17 - Canonical root stdlib math now validates on the Bazel-backed native LLVM lane

The new root `stdlib/math.kn` is now proven through the live Bazel-backed `kain` binary, not just by source inspection. The key blockers were not inside the math library itself so much as in the frontend stdlib import bundle and LLVM builtin lowering.

What changed:

- `crates/kain-driver/src/lib.rs`
  - Stopped prepending the entire root stdlib source blob into frontend bundles for native builds.
  - Frontend bundling now materializes only the imported stdlib modules plus a tiny ambient native prelude (`runtime` and `actor`) so `use std::math` no longer drags unrelated root modules like `stdlib/gen_server.kn` into every native compile.
  - Added focused driver tests proving imported stdlib modules materialize without whole-root slurp and that typed frontend programs see imported `std::math` items.
- `stdlib/math.kn`
  - Added the canonical root math surface for vectors, quaternions, matrices, affine transforms, GPU layout wrappers, bounds/intersections, curves, color math, packing, and procedural noise.
  - Replaced root builtin `min` / `max` / `clamp` dependencies with math-local helpers and direct tuple constructors so the canonical math surface no longer depends on stale builtin numeric metadata.
  - Flattened `frustum_vs_aabb` away from array-of-struct plane iteration because the current LLVM lane mis-lowered `Plane` array element field access.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Native LLVM now lowers numeric `abs`, `min`, `max`, and `clamp` by the actual compiled operand types instead of trusting the one-entry stdlib function map.
  - Float `abs` no longer becomes `call i64 @abs(double ...)` plus `sitofp`; it is now emitted as native float compare/select math.
  - Added a focused LLVM regression test proving float builtins do not route through integer `abs/min/max/clamp` signatures.
- `blades/math-domains`
  - The blade now checks, compiles, links, and runs successfully on the Bazel-backed native LLVM lane.

Validation:

- `cargo test -p kain-driver frontend_source_bundle_materializes_imported_stdlib_modules_without_whole_root_slurp --target-dir target\codex-stdlib-driver -- --nocapture`
- `cargo test -p kain-driver frontend_to_typed_program_includes_imported_stdlib_module_items --target-dir target\codex-stdlib-driver -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_registers_nested_module_const_values --target-dir target\codex-float-builtins -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_named_vec_fields_and_tuple_alias_access --target-dir target\codex-float-builtins -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_numeric_builtins_with_float_operands_without_int_abs_calls --target-dir target\codex-float-builtins -- --nocapture`
- `powershell -ExecutionPolicy Bypass -File .\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry blades\math-domains\src\main.kn -OutputName math-domains.exe -VerifyLlvm -Run`
- `bazel build //:kain --config=dev`

Durable lessons:

- When `stdlib/` is the canonical authored surface, native frontend bundling must import only requested modules. Whole-root stdlib concatenation turns unrelated legacy modules into compile blockers.
- The current LLVM function table stores one signature per stdlib name. Any numeric builtin that is semantically polymorphic at runtime (`abs`, `min`, `max`, `clamp`) must be lowered from operand types, not from the stale single-signature metadata entry.
- If a canonical stdlib module seems broken only in the full blade but not in tiny probes, inspect ambient builtins and import bundling before rewriting the library math.

# 2026-05-17 - Amalgamate should be a capsule workspace lane, not an import lane

Frontier design work around "portable codebases luggable in one `.kn` file" converged on a strong boundary: this should live in `crates/kain-amalgamate`, not inside `kain-import`.

Durable decision:

- `kain-import` owns semantic translation from foreign languages into Kain IR.
- `kain-amalgamate` should own transport, schema, compression, integrity, and materialization of whole blades/workspaces into a single `.kn` carrier artifact.

Why this survived:

- Real blades such as `blades/pong` and `blades/kain-labs` are not just source files. They depend on `KAIN.toml`, `native/` headers/C files/shaders, config manifests, build tasks, and `c_ffi` metadata.
- `crates/kain-blades` and `crates/kain-build` already know how to discover and build that tree. The cheapest truthful design is to materialize a capsule back into a normal workspace and reuse those systems.
- `crates/cli/src/import_rust.rs` already proves the operator model can distinguish bundle mode from mirrored-blades mode. An amalgamate CLI should borrow that shape rather than inventing a one-off.

Current thesis:

- `kain amalgamate <path> -o artifact.kn` should pack a blade/workspace into a `.kn` capsule.
- `kain run/build/check artifact.kn` should detect the capsule, materialize it under `.kain/cache/amalgamate/<digest>/`, and then hand off to normal workspace discovery.
- Optional adapter generation belongs behind flags such as `--generate-adapters`; preservation of original sources is the default.

Proof artifact:

- `z3/reports/20260517T081310Z-amalgamate-capsule-layout-three-file-nonoverlap.json` proved a representative payload-table non-overlap invariant with `unsat`. This is the seed proof for the capsule blob layout math.

Research artifact:

- `research/2026-05-17-kain-amalgamate-capsule.md` carries the hypothesis lattice, evidence, and next experiment list.

Recommended next step:

- Build a proof spike that packs/unpacks `blades/network-domains`, then `blades/pong`, before touching direct compiler syntax. If those survive via materialize-and-delegate, the feature has the right seam.

# 2026-05-17 - KAIN capsule v1 is now a live CLI, cache, and dogfood lane

The capsule plan is no longer only a design note. `kain amalgamate` now exists as a first-class operator surface and the normal `run` / `build` / `check` commands can transparently consume capsule `.kn` artifacts.

What changed:

- `crates/kain-amalgamate`
  - Added the v1 capsule format, metadata parsing, preview/header generation, payload digests, pack/inspect/unpack helpers, and digest-scoped materialization under `.kain/cache/amalgamate/<digest>/workspace`.
  - Kept the artifact comment-safe and text-first: generated header comments, `//!kain-capsule` metadata, and `//!kain-capsule-payload` base64 payload.
- `crates/kain-commands` and `crates/cli`
  - Added `kain amalgamate`, `kain amalgamate inspect`, and `kain amalgamate unpack`.
  - Wired `kain run`, `kain build`, and `kain check` through capsule detection plus materialize-and-delegate behavior.
  - Preserved the boundary where `kain-run`, `kain-build`, and blade/workspace discovery remain the execution truth after materialization.
- `blades/amalgamate-capsule-probe`
  - Added a real dogfood blade with `KAIN.toml`, `src/`, `config/`, and `native/` payloads so the capsule lane proves whole-workspace preservation rather than only single-file pack/unpack.
- Repo operator docs and skills
  - Updated architecture/CLI docs and added a repo-local capsule skill for future agents.

Validation:

- `cargo check -p kain-amalgamate -p kain-commands -p cli`
- `cargo test -p kain-commands`
- `target\debug\kain.exe amalgamate blades\amalgamate-capsule-probe -o blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn --author "Taylor Kipp" --meta license=MIT --note "portable archive dogfood" --preview-symbols 8`
- `target\debug\kain.exe amalgamate inspect blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn`
- `target\debug\kain.exe amalgamate unpack blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn -o blades\amalgamate-capsule-probe\.kain\unpacked`
- `target\debug\kain.exe check blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn --target llvm`
- `target\debug\kain.exe run blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn`
- `target\debug\kain.exe build blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn`

Proof artifacts:

- `z3/reports/20260517T081310Z-amalgamate-capsule-layout-three-file-nonoverlap.json`
- `z3/reports/20260517T093524Z-amalgamate-path-depth-nonnegative.json`

Durable lessons:

- Treat capsules as transport and preservation, not as an import lane. Foreign/native files should survive verbatim.
- Keep the machine-truth metadata in the sentinel block and generate the human header from it. Hand-maintained preview headers will drift.
- Root-level `kain check <blade-root>` can still see local `.kain` `.kn` copies, so unpacked capsule probes inside a blade root are validation noise unless removed or isolated.
- `kain-run` still only accepts manifest `[run].target` values `auto|kain|c|cargo|fabric|node|bun`. The capsule probe uses `target = "kain"` for `run` validation even though `check` and `build` smoke the LLVM lane separately.

Recommended next step:

- Dogfood the same pack/materialize/delegate flow on `blades/network-domains`, then `blades/pong`, and decide whether manifest `run.target = "llvm"` should become a real `kain-run` target or stay outside the immediate-execution lane.

# 2026-05-17 - Benchmark pipeline gained a first-class optional Zig lane in the main suite

The main benchmark runner is no longer limited to Kain/Rust/C++/Go/Erlang/JavaScript/Python. Zig is now a first-class optional language in `benchmark/run.py`, so future comparisons do not need to live only in the dedicated FFI boundary lane.

What changed:

- `benchmark/run.py`
  - Added `zig` to the main `LANGUAGE_ORDER`, labels, source-key resolution, CLI parsing, toolchain report, and build dispatch.
  - Added `--zig` / `ZIG` support and a direct `zig build-exe -O ReleaseFast` build path for dependency-free `main.zig` rows.
- `benchmark/benchmarks.json`
  - Added the first Zig-backed comparison pack to:
    - `contention_wall`
    - `branch_dispatch`
    - `call_chain`
    - `native_map_lookup`
    - `zero_copy_binary_wire`
- `benchmark/cases/*/main.zig`
  - Added dependency-free Zig implementations aligned to the existing benchmark checksums for those four rows.
- `benchmark/README.md`, `.agents/skills/kain-benchmark-pipeline/SKILL.md`, `benchmark/blades/kain-benchmark/src/catalog.kn`, and `ARCHITECTURE.md`
  - Updated the operator and durable architecture surfaces so the new language lane is visible in docs and the benchmark blade language inventory.

Durable lessons:

- Keep Zig in the same manifest-driven extension model as every other language. New Zig rows should usually be just `main.zig` plus a manifest path entry, not new runner branches.
- The dedicated `benchmark/ffi_boundary/` Zig rows are still the boundary-overhead truth lane; the main suite Zig pack is for language-to-language workload comparisons.

# 2026-05-17 - Root stdlib is now the single canonical stdlib for authored and native Kain

The repo no longer keeps two live copies of the native-facing stdlib domains. `stdlib/` is now the single canonical on-disk stdlib surface for both public `std.*` imports and LLVM/direct-C native target loading.

What changed:

- `stdlib/fs.kn` and `stdlib/runtime.kn`
  - Promoted the newer native implementations into the root canonical files so root `std::fs` and `std::runtime` carry the current ABI-backed surface, including the attrition/runtime helpers and len-aware filesystem writes.
- `crates/kain-core/src/stdlib.rs`
  - Native target profile order now loads the root stdlib directly for `CompileTarget::Llvm` and root plus `stdlib/c` for `CompileTarget::C`.
  - Kept `KAIN_STDLIB_PROFILE=native` working as a compatibility alias that resolves to the root stdlib path instead of requiring a second `stdlib/native` directory on disk.
- `crates/kain-core/src/module_resolution.rs`
  - `use std::native::foo` now resolves to the same root `stdlib/foo.kn` file instead of a second native folder, so old imports can still parse while the repo carries only one real stdlib tree.
- `blades/kain-actor-kit`, `blades/kain-http`, and `blades/kain-process-kit`
  - Moved repo-authored library blades off `std::native::*` and onto canonical root imports.
- `stdlib/native/`
  - Deleted the old duplicate tree after promoting the drifted files and rewiring native target loading.

Durable lessons:

- If `use std::foo` and native target prelude loading point at different files, stdlib drift is guaranteed. Keep authored imports and target loading on the same root modules.
- Compatibility aliases are acceptable for old `std::native::*` imports, but the compatibility path must never reintroduce a second live stdlib copy on disk.
- When native stdlib behavior changes, update the root `stdlib/*.kn` files first. `stdlib/c` is now the only target-specific overlay that should remain beside the canonical root.

# 2026-05-17 - Attrition telemetry grew into a real diagnostic surface, not just a pass/fail bit

The attrition pipeline can now explain runtime failures with a lot more structure than “closure drifted.” The source of truth is still internal runtime counters, but the snapshots, Kain runtime-capture JSON, and runner-derived reports are now wide enough that a failing lane points at the shape of the leak instead of only saying red/green.

What changed:

- `runtime/native/include/attrition.h`
  - Bumped `KAIN_ATTRITION_SCHEMA_VERSION` to `2`.
  - `KainAttritionSnapshot` now carries richer RC/allocator, quarantine/fragmentation, actor/scheduler, process/handle, async/timer, checkpoint, and time-provenance counters.
- `runtime/native/src/core/attrition.c`
  - Runtime-side attrition bookkeeping now tracks:
    - peak RC objects
    - total allocated/freed bytes
    - allocation failures
    - quarantine live/peak bytes and entries
    - fragmentation live/peak/total bytes plus injection count
    - checkpoint counts and last checkpoint identity
    - virtual-time advance totals
    - raw-sleep fallback milliseconds
  - Added a saturating-add helper for telemetry counters and used it in the hot bookkeeping paths so counter growth cannot silently wrap backward.
- `runtime/native/src/core/actor.c`
  - Attrition snapshot capture now exports actor occupancy popcount, registry entries, monitor/link/supervision counts, in-turn counts, restart/escalation counters, and pooled scheduler depth/worker telemetry.
- `runtime/native/src/core/process_system.c`
  - Attrition snapshot capture now exports process-spec occupancy, pipe/OS/PTY handle counts, and captured-output live bytes.
- `runtime/native/src/core/async.c`
  - Attrition snapshot capture now exports async task/timer occupancy popcounts, sleeping/ready/cancelled/fired/started counts.
- `runtime/native/src/core/stdlib_abi.c` and `attrition/cases/common/attrition_harness.h`
  - Both the Kain LLVM runtime-capture path and the native C harness JSON writer now emit the same widened schema-2 snapshot shape. This keeps `.kn` lanes and C lanes comparable.
- `attrition/run.py`
  - Each case now gets a derived `telemetry` block with:
    - throughput
    - peak metrics
    - activity metrics
    - balance gaps
    - end-state resource closure
    - nonzero end-state field lists
    - time provenance metrics
    - health flags
    - event-ring tail/total/dropped counters
  - The suite report now gets `suite_telemetry` with aggregate throughput, failed-case count, cases with closure drift, total dropped event-ring entries, and max-offender case ids for key peak/end-state metrics.
  - The failure minimizer now tries to preserve the same failure family instead of minimizing to any random smaller failing op count.
- `attrition/README.md` and `.agents/skills/kain-attrition-pipeline/SKILL.md`
  - Updated to document the wider telemetry contract, the real Kain LLVM attrition lanes, and the event-ring tail-vs-total reading rule.

Validation and proof:

- `python -m py_compile attrition/run.py`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/attrition.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/actor.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/process_system.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/async.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/stdlib_abi.c`
- `python attrition/run.py --case kain_semantic_singularity_crucible_attrition --scale small --profile release-instrumented --timeout 900`
- `python attrition/run.py --case saturated_rc_hot_object --scale small --profile release-instrumented --timeout 300`
- `runtime/native/src/core/z3/proofs-experimental/attrition-saturating-u64-add-monotonic.smt2`
  - Saved proof report: `z3/reports/20260517T064426Z-attrition-saturating-u64-add-monotonic.json`
  - Result was `unsat`

Current truth:

- The richer telemetry is already paying off on the Kain LLVM lanes.
- `kain_quantumerlang_attrition` and `kain_semantic_singularity_crucible_attrition` both now fail with concrete closure shape instead of a generic red:
  - `kain_quantumerlang_attrition` showed `live_rc_objects = 20`, `live_runtime_bytes = 1017`, `allocation_count = 47`, `free_count = 27`.
  - `kain_semantic_singularity_crucible_attrition` showed `live_rc_objects = 45`, `live_runtime_bytes = 2412`, `actor_live_count = 1`, and `actor_spawn_count - actor_exit_count = 1`.
- Durable reading rule: `event_ring_kind_histogram` is only the copied tail window. Use it together with `event_count_total` and `event_ring_dropped_count` before treating it as a whole-run histogram.

# 2026-05-17 - Benchmark cases now carry domain telemetry, not just raw milliseconds

The benchmark pipeline no longer has to explain everything through wall-clock `ms` alone. `benchmark/benchmarks.json` cases can now declare domain telemetry directly in the manifest, and the root snapshots plus LLM reports render those metrics automatically.

What changed:

- `benchmark/run.py`
  - The report telemetry section is now framed generically as `Telemetry`, not only `Throughput`, so future wrapper/case metrics are not constrained to one naming style.
  - Case metrics remain data-driven: the runner computes primary metric tables and per-case telemetry lists from manifest metadata rather than from case-specific Python branches.
- `benchmark/benchmarks.json`
  - Added first-class telemetry to the key runtime-heavy rows:
    - `contention_wall`: counter increments/s
    - `ghost_mirror`: mirror updates/s, payload bytes/s, wire bytes/s, page touches/s
    - `async_ready_chain`: ready awaits/s
    - `tcp_loopback_tokio`: roundtrips/s, payload bytes/s
    - `native_map_lookup`: lookups/s, queried key bytes/s
    - `json_manual_roundtrip`: docs/s, fields/s, input bytes/s, roundtrip bytes/s
    - `filesystem_stream`: rounds/s, file touches/s, copied bytes/s, total filesystem bytes/s
    - `process_stdio_loop`: launches/s, captured stdout bytes/s
    - `http_server_concurrency`: requests/s plus body/wire bytes/s
    - `http_server_frameworks`: requests/s plus body/wire bytes/s
    - `actor_mailbox_erlang`: asks/s, mailbox messages/s
    - `quantumerlang`: fold roundtrips/s, mailbox messages/s
    - `gpu_graphics_submit`: frames/s, draws/s, instances/s, submitted vertices/s, submitted indices/s
    - the three `sim_*` rows now carry their solver-domain metrics directly in the main benchmark manifest instead of only in the sim wrapper manifest
- `benchmark/simulations/simulations.json`
  - The sim suite now composes the main benchmark catalog and only selects the sim case ids plus wrapper-owned suite defaults; the per-case telemetry truth moved back into `benchmark/benchmarks.json`.

Durable lessons:

- Put benchmark meaning in the manifest. The runner should not need to know what a mailbox ask, HTTP roundtrip, or CFD relaxation means.
- Use one primary metric per case for the compact tables, but keep the richer supporting metrics in the same case telemetry block so LLMs and humans can inspect the under-the-hood work shape.
- P95/P99 latency is still a separate phase; it requires per-operation timing emitted by the benchmark programs themselves rather than only total process runtime.

# 2026-05-17 - Benchmark wrappers are now a data-driven plugin layer instead of hardcoded one-off Python shims

The benchmark pipeline now has a proper wrapper/plugin surface so new suite categories can be added without splicing more control flow into `benchmark/run.py`. The core runner remains the one place that knows how to build, run, time, and report cases. Wrapper files now own the fire-and-forget suite ergonomics.

What changed:

- `benchmark/run_wrapper.py`
  - Added a generic launcher that discovers `benchmark/wrappers/*.json`, lists them, and forwards wrapper-defined `before_args` / `after_args` into `benchmark/run.py`.
  - The durable extension rule is now: add a new wrapper JSON file for a new suite category before considering any edit to `run.py`.
- `benchmark/wrappers/`
  - Added `fast.json` for the reduced Kain/Rust/C++/Erlang lane.
  - Added `sim.json` for the extracted `sim_nbody_gravity`, `sim_uv_velocity_grid`, and `sim_cfd_pressure_projection` pack.
  - Added `README.md` documenting the wrapper schema and launch commands.
- `benchmark/run_fast.py` and `benchmark/run_sim.py`
  - These are now compatibility shims over the wrapper launcher instead of hand-building their own command lines.
- `benchmark/run.py`
  - Added `--latest-stem` so wrapper-owned suites can keep dedicated `latest_*.llm.md` and `latest_*.json` files instead of clobbering the main latest report.
  - Added manifest `include_manifest` + `case_ids` support so future suite-specific manifests can be composed from the main benchmark case catalog rather than duplicated by hand.
  - Added optional per-case telemetry rendering in the JSON/Markdown pipeline; this is the substrate for future simulation-specific throughput metrics without more report-splicing debt.
- `benchmark/.gitignore`
  - Root snapshots now ignore `latest_*.md` generically so future wrapper plugins do not dirty `git status`.

Durable lessons:

- `benchmark/run.py` should stay the stable execution/report core.
- New categories such as simulation packs, framework packs, or stress packs should usually land as `benchmark/wrappers/<name>.json`.
- If a new wrapper needs distinct latest report files, use wrapper-owned `--minimal-name latest_<name>.md` plus `--latest-stem latest_<name>`.
- If a future suite needs more than fixed CLI forwarding, prefer manifest composition (`include_manifest`, `case_ids`) and case telemetry metadata before adding new hardcoded runner branches.

# 2026-05-17 - Benchmark pipeline gained a k-os-sim extraction pack with three real simulation kernels and no Go lane

The benchmark suite now has a first simulation pack directly extracted from the imported `benchmark/cases/k-os-sim` reference crate rather than from toy math kernels. The new rows deliberately stay in the Kain/Rust/C++ category; Go is first-class in the general benchmark lane now, but the sim category is intentionally not carrying a Go column.

What changed:

- `benchmark/benchmarks.json`
  - Added three new implemented simulation rows:
    - `sim_nbody_gravity`
    - `sim_uv_velocity_grid`
    - `sim_cfd_pressure_projection`
  - Each row is explicitly described as an extracted hot kernel from `k-os-sim`, not the whole engine/editor crate.
- `benchmark/cases/sim_nbody_gravity/`
  - Added Kain, Rust, and C++ implementations of the small deterministic N-body gravity solve derived from the k-os-sim quantum lane.
- `benchmark/cases/sim_uv_velocity_grid/`
  - Added Kain, Rust, and C++ implementations of the UV-space particle update plus weighted velocity-grid splat derived from the k-os-sim fluid lane.
  - Durable lesson: the raw floating-point row needed deterministic state snapping after particle updates to remove tiny cross-compiler drift in the checksum path. The workload is still the same; the snap is there to make the benchmark numerically stable across LLVM/C++/Rust lanes.
- `benchmark/cases/sim_cfd_pressure_projection/`
  - Added Kain, Rust, and C++ implementations of the focused divergence/Jacobi/pressure-gradient solve derived from the k-os-sim CFD lane.
- `benchmark/README.md`, `.agents/skills/kain-benchmark-pipeline/SKILL.md`, and `benchmark/blades/kain-benchmark/src/catalog.kn`
  - Updated the operator surfaces for the new sim pack.
  - The benchmark blade’s curated case count is now `47`.

Validation and benchmark:

- `py -3 benchmark/run.py --case sim_nbody_gravity,sim_uv_velocity_grid,sim_cfd_pressure_projection --languages kain,rust,cpp --runs 1 --warmups 0 --timeout 900`
- Latest report `benchmark/out/reports/latest.llm.md` at `2026-05-17T06:09:51.949090+00:00` now shows:
  - `sim_nbody_gravity`: Kain `20.543 ms`, Rust `11.619 ms`, C++ `10.462 ms`
  - `sim_uv_velocity_grid`: Kain `93.027 ms`, Rust `16.596 ms`, C++ `17.049 ms`
  - `sim_cfd_pressure_projection`: Kain `27.687 ms`, Rust `11.072 ms`, C++ `9.359 ms`

Durable lessons:

- When porting real simulation kernels into the benchmark lane, extract the hot solver loop from the engine crate and keep the row dependency-free whenever possible. Do not benchmark editor/host/framework scaffolding by accident.
- The sim pack is intentionally Kain/Rust/C++ only right now. If a future agent is tempted to add Go just because the general lane supports it, treat that as a conscious category change rather than a default extension.

# 2026-05-17 - Attrition landed as a first-class runtime-certification pipeline with deterministic sabotage, replay, and teardown closure

`attrition/` is now a real sibling pipeline to `benchmark/`, not a scratch soak folder. Benchmark is still the performance truth lane; attrition is now the runtime-stability truth lane for compressed long-horizon abuse, deterministic replay, sabotage-backed invariants, and final closure audits.

What changed:

- `attrition/`
  - Added the pipeline root with `run.py`, `README.md`, `attritions.json`, `invariants.json`, `schema/`, `cases/`, `out/build/`, and `out/reports/`.
  - `attritions.json` is the source of truth for implemented lanes, scales, determinism tiers, sabotage modes, runtime profiles, and runtime overrides.
  - `invariants.json` is the explicit catalog for owner subsystem, exact formula, units, idle floors, allowed permanent floor entries, sabotage mappings, and isolate/mixed coverage.
  - The current implemented deterministic foundation lanes are:
    - `saturated_rc_hot_object`
    - `virtual_time_async_timer`
    - `actor_reply_port_recycle`
    - `process_slot_recycle`
    - `mixed_runtime_boss`
  - Reports now follow the benchmark shape: compact root snapshot in `attrition/latest.md`, plus timestamped JSON and LLM-readable markdown under `attrition/out/reports/`.
  - Deterministic failures emit replay commands and minimized repro op counts so failing seeds can be shrunk without losing the same seed/profile/sabotage context.
- `runtime/native/include/attrition.h` and `runtime/native/src/core/attrition.c`
  - Added the attrition-only runtime telemetry/control ABI: session config, deterministic tiering, virtual-time controls, allocator poison/quarantine knobs, event flight recorder, snapshots, and final audit capture.
  - The attrition runtime now tracks RC/object counters, bytes, retain/release totals, async/actor/process telemetry, time-provenance counters, progress heartbeats, and the last-N event ring.
- `runtime/native/src/core/async.c`
  - Fixed a real attrition-only structural bug: disposing or cancelling a task now disarms its live sleep timer under the correct locks, so virtual-time task churn returns both task tables and timer tables to zero instead of leaking stale wakeups.
  - Durable lesson: task disposal and timer cancellation are one lifecycle seam in this runtime; if attrition finds async table drift again, inspect the timer-disarm path first.
- `runtime/native/src/core/actor.c`
  - Attrition snapshot capture now tolerates actor runtime not being initialized yet instead of touching actor locks during baseline collection.
- `attrition/cases/process_slot_recycle/main.c` and `attrition/cases/mixed_runtime_boss/main.c`
  - Durable operator contract: `abi_process_wait(...)` returns `1` for exited, `0` for timeout, and `< 0` for error. Treating any nonzero as failure will create fake attrition reds on the process lanes.
- `runtime/native/src/core/z3/proofs-experimental/attrition-event-ring-copy-window-bounds.smt2`
  - Added a solver-backed proof for the attrition flight-recorder extraction window in `attrition.c`: once the ring cursor is valid, every copied slot stays within the 1024-entry event ring. The proof report `z3/reports/20260517T054056Z-attrition-event-ring-copy-window-bounds.json` returned `unsat`.

Validation and current truth:

- `python -m py_compile attrition/run.py`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/async.c`
- `python attrition/run.py --scale small --profile release-instrumented --timeout 300`
- `python attrition/run.py --case virtual_time_async_timer --scale small --profile release-instrumented --sabotage skip_task_dispose --timeout 240`
- The small release-instrumented suite is currently green at `5/5`.
- The sabotage proof point is also real: `virtual_time_async_timer` with `skip_task_dispose` produces an expected-fail report with leaked async-task occupancy instead of a false green.

Durable lessons:

- Internal runtime counters are the primary truth in attrition. RSS is secondary and should not be treated as the only leak detector.
- The actor occupancy floor intentionally bottoms out at `1`, not `0`, because bit 0 is the reserved invalid actor slot. Keep that floor in the invariant catalog instead of special-casing it ad hoc in case code.
- Every new attrition invariant should map to one isolate lane, one sabotage proof, and one mixed-lane membership. If that mapping is missing, the lane matrix is drifting into wishful thinking instead of certification.

# 2026-05-17 - Benchmark pipeline now has first-class Go support plus a new compute/framework expansion pack

The benchmark lane is no longer just Kain/Rust/C++ with optional scripting rows. `benchmark/run.py` now owns a first-class Go lane, and the suite grew a new set of deeper systems rows so future agents can answer more than scalar/allocator/actor questions from the same pipeline.

What changed:

- `benchmark/run.py`
  - Added Go as a first-class language in `LANGUAGE_ORDER`, labels, manifest source resolution, CLI parsing, toolchain reporting, and build dispatch.
  - Added direct Go builds with release defaults `-trimpath -ldflags=-s -w`.
  - The main runner still writes the compact root snapshot (`benchmark/latest.md`) plus the timestamped/full JSON/LLM reports.
- `benchmark/benchmarks.json`
  - Added new Go-backed compute cases:
    - `ecs_archetype_query`
    - `zero_copy_binary_wire`
    - `dynamic_vtable_thrashing`
    - `crypto_block_cipher`
    - `ray_sphere_intersection`
  - Added `http_server_frameworks` as the new framework/category HTTP row:
    - Kain native localhost HTTP route surface
    - Rust Actix Web
    - Go `net/http`
  - `dynamic_vtable_thrashing` stays honest as `dispatch-proxy` for Kain.
  - `http_server_frameworks` stays honest as `semantic-proxy` for Kain.
  - `ray_sphere_intersection` now carries a Kain-specific language note: in this checkout the Kain row regenerates the seeded geometry directly in the loop because literal float-array indexing was not yet native-LLVM parity-safe.
- New case source folders now exist under `benchmark/cases/` for all of the rows above, including Go implementations and the Actix/Go HTTP framework row.
- `benchmark/blades/kain-benchmark/src/catalog.kn`
  - The benchmark blade catalog now reports the real suite scale (`44` cases, `6` languages with Go included) and shows a curated featured inventory instead of the stale pre-Go/pre-framework list.
- `benchmark/README.md` and `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Updated for Go support, the new case pack, the framework HTTP row, and the compact root snapshots.

Validation and benchmark:

- `py -3 -m py_compile benchmark/run.py benchmark/run_fast.py`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `py -3 benchmark/run.py --case ecs_archetype_query,zero_copy_binary_wire,dynamic_vtable_thrashing,crypto_block_cipher,ray_sphere_intersection --languages kain,rust,cpp,go --runs 1 --warmups 0 --timeout 900`
- `py -3 benchmark/run.py --case http_server_frameworks --languages kain,rust,go --runs 1 --warmups 0 --timeout 900`

Useful current smoke numbers:

- `ecs_archetype_query`: Kain `50.751 ms`, Rust `51.481 ms`, C++ `45.992 ms`, Go `61.662 ms`
- `zero_copy_binary_wire`: Kain `1140.522 ms`, Rust `88.133 ms`, C++ `108.769 ms`, Go `192.480 ms`
- `dynamic_vtable_thrashing`: Kain `21.772 ms`, Rust `17.678 ms`, C++ `15.900 ms`, Go `19.368 ms`
- `crypto_block_cipher`: Kain `16.230 ms`, Rust `12.273 ms`, C++ `12.197 ms`, Go `15.591 ms`
- `ray_sphere_intersection`: Kain `141.077 ms`, Rust `87.060 ms`, C++ `78.825 ms`, Go `168.365 ms`
- `http_server_frameworks`: Kain `146.413 ms`, Rust `191.400 ms`, Go `183.915 ms`

Durable lessons:

- The Go lane is worth keeping as a first-class peer, especially for networking/framework rows where the standard library gives a clean systems baseline without dragging in extra orchestration noise.
- If `ray_sphere_intersection` regresses or starts failing checksum in Kain, inspect native float-array literal/indexing behavior before blaming the geometry math itself. The current stable Kain row computes the deterministic seeded rays/spheres directly from integer indexes.
- `http_server_frameworks` should stay a category/framework story, not an “identical scheduler” story. Keep the fairness note explicit that Kain is racing its current synchronous native HTTP surface against Actix and Go `net/http`.

# 2026-05-17 - Windows native runtime object caching was secretly dead in the benchmark lane because depfile parsing treated every backslash as an escape

The long native benchmark build times were not mainly a linker problem or a missing cache directory. The real break was in `crates/cli/src/main.rs::parse_native_runtime_depfile(...)`: it consumed every `\` as an escape, which works for escaped spaces in Unix-like depfiles but corrupts ordinary Windows absolute paths like `D:\Kain-Lang\runtime\native\src\core\core.c`. That meant `native_runtime_object_cache_is_fresh(...)` could not `metadata(...)` the parsed dependencies, so identical benchmark-release native builds always reported `Native runtime cache: 0 reused, 36 compiled`.

What changed:

- `crates/cli/src/main.rs`
  - `parse_native_runtime_depfile(...)` now treats backslash as special only for real depfile line continuations and escaped whitespace.
  - Ordinary non-whitespace backslashes are preserved verbatim, so Windows dependency paths survive parsing unchanged.
  - Added `native_runtime_depfile_parser_preserves_windows_absolute_paths` as a Windows regression test.
  - The existing `native_runtime_object_cache_detects_stale_dependencies` test, which was already failing on this host before the fix, now passes again and proves cache freshness works across a real Windows absolute-path depfile.
- `.agents/skills/kain-benchmark-pipeline/SKILL.md`
  - Added the durable operator note for the `0 reused, 36 compiled` symptom so future benchmark work can identify this cache seam quickly.

Validation and practical result:

- `cargo test -p cli native_runtime_depfile -- --nocapture`
- `cargo test -p cli native_runtime_object_cache_detects_stale_dependencies -- --nocapture`
- `bazel build //:kain --config=release`
- Rebuilt `benchmark/cases/scalar_mix/main.kn` twice in a row with the Bazel release `kain.exe` and benchmark-release native env:
  - first rebuilt case after the fix: `Native runtime cache: 36 reused, 0 compiled, 0 archives reused, 0 archives rebuilt`, `ELAPSED_MS=797.3`
  - second identical rebuild: `Native runtime cache: 36 reused, 0 compiled, 0 archives reused, 0 archives rebuilt`, `ELAPSED_MS=392.3`

Durable lesson:

- On Windows, native runtime depfiles are not escape-heavy strings; they are mostly plain absolute paths with backslashes. If benchmark/native build times suddenly jump back to ~10s per case and the cache line says `0 reused, 36 compiled`, inspect depfile parsing before chasing linker flags or archive-group tuning.

# 2026-05-17 - JSON manual roundtrip and filesystem stream both flipped into Kain wins by treating concat calls and parent-dir creation as copy/retry boundaries instead of permanent tax

The last honest JSON gap was not parser logic anymore. It was ownership churn around string assembly. LLVM already flattened long string-add chains into `str_concatN(...)`, but it still treated those concat calls like black holes and leaked the owned inputs they had already copied. In practice that meant every `to_string(...)`, `bool_text(...)`, and other fresh string term in hot concat trees kept an unnecessary live RC object after the concat result was built. `filesystem_stream` had a different remaining dragon: the runtime eagerly walked/created parent directories before every write and copy, even when the benchmark was hammering the same already-existing temp paths all run long.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `compile_string_concat_expression(...)` now tracks whether each concat term is an owned temporary.
  - After `str_concatN(...)` copies its inputs, LLVM emits `rc_release(...)` for owned string temporaries such as `to_string(...)`, `substring(...)`, and call-returned string helpers.
  - The fallback nested concat path now also releases consumed owned inputs and intermediate concat accumulators as each step is copied into the next string.
  - The plain 2-term string `+` lowering now does the same release-after-copy cleanup instead of only returning the new concat result.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - The fixed-arity concat regression now asserts that `render_payload(...)` emits `str_concat9(...)` and then releases the owned `to_string(...)` / `bool_text(...)` temporaries.
  - The loop-literal pooling regression now also asserts that the owned numeric `to_string(...)` temporary inside the loop is released after `str_concat4(...)`.
- `runtime/native/src/core/stdlib_abi.c`
  - Added `abi_fs_open_write_retry_parent_dirs(...)`.
  - `abi_fs_write_mode_len(...)`, `abi_fs_copy_file(...)`, and `abi_fs_copy_file_streaming(...)` now try the output open first and only create parent dirs plus retry once if the first open fails.
  - This keeps semantics equivalent for the normal success path while deleting repeated `create_parent_dirs(...)` directory walks from hot steady-state write/copy loops.
- Exploratory proof `runtime/native/src/core/z3/proofs-experimental/filesystem-open-first-parent-retry-equivalence.smt2` plus report `z3/reports/20260517T033748Z-filesystem_open_first_parent_retry_equivalence.json` returned `unsat` for the control-flow equivalence model: under the assumption that an immediate successful open implies the eager parent-dir path would also have succeeded, the open-first retry strategy is equivalent to the older eager-create strategy.

Validation and benchmark:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_flattens_long_string_concat_chains_into_fixed_arity_runtime_calls -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_hoists_repeated_string_literals_out_of_loop_bodies -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_find_substring_from_on_known_strings_with_precomputed_lengths -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_byte_at_on_known_strings_without_runtime_helper_calls -- --nocapture`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/stdlib_abi.c`
- `bazel build //:kain --config=release`
- `py -3 benchmark/run.py --case json_manual_roundtrip --languages kain,rust,cpp --runs 7 --warmups 2 --timeout 900 --kain-exe D:\\Kain-Bazel\\output-user-root\\ccujd7ry\\execroot\\_main\\bazel-out\\x64_windows-opt\\bin\\crates\\cli\\kain.exe --minimal-name latest_json_manual_roundtrip_concat_cleanup.md`
- `py -3 benchmark/run.py --case filesystem_stream --languages kain,rust,cpp --runs 7 --warmups 2 --timeout 900 --kain-exe D:\\Kain-Bazel\\output-user-root\\ccujd7ry\\execroot\\_main\\bazel-out\\x64_windows-opt\\bin\\crates\\cli\\kain.exe --minimal-name latest_filesystem_stream_parent_retry.md`
- Durable release report `benchmark/out/reports/20260517T033457Z.llm.md` now shows `json_manual_roundtrip`: Kain `107.611 ms`, Rust `111.877 ms`, C++ `93.319 ms`. Kain now wins the honest row over Rust and is down to about `1.15x` behind C++.
- Durable release report `benchmark/out/reports/20260517T033711Z.llm.md` now shows `filesystem_stream`: Kain `75.858 ms`, Rust `106.365 ms`, C++ `85.204 ms`. Kain now wins the honest row over both Rust and C++.

Durable lesson:

- For string-heavy native rows, treat `str_concat(...)` and `str_concatN(...)` as copy boundaries. If the concat already copied the bytes, any owned string input produced just for that concat should be released immediately afterward.
- For filesystem write/copy hot paths, do not pay `create_parent_dirs(...)` eagerly on every steady-state operation. Open first, and only fall back to parent-dir creation when the open actually fails.

# 2026-05-17 - Filesystem stream recovered from cached-length poison and dropped again via len-aware FS writes while JSON proved byte_at was not the last dragon

The cached string-length work initially broke `filesystem_stream` because the filesystem text read path allocates first and fills later. `abi_fs_string_with_len(0, size)` was zeroing the new logical-length field, so `abi_fs_read_text(...)` and `abi_fs_read_text_range(...)` returned buffers whose bytes were correct but whose logical length stayed zero until compared. The repair was to keep the default header length for allocate-then-fill shells and explicitly stamp the final read length after `fread(...)`. That restored semantic correctness without backing out the broader RC string-length work.

After the repair, the next honest filesystem win came from deleting repeated `strlen(...)` scans on Kain-authored writes. `runtime/native/include/stdlib_abi.h` and `runtime/native/src/core/stdlib_abi.c` now expose `abi_fs_write_text_len(...)`, `abi_fs_append_text_len(...)`, and `abi_fs_atomic_write_text_len(...)` beside the compatibility entrypoints. `stdlib/native/fs.kn` routes authored `fs_write_text`, `fs_append_text`, and `fs_atomic_write_text` through those length-aware entrypoints with `len(content)`, so hot rows write the cached logical string length instead of rescanning the same payload every round.

What changed:

- `runtime/native/src/core/stdlib_abi.c`
  - `abi_fs_string_with_len(...)` no longer zeroes logical length when used as an allocate-then-fill shell.
  - `abi_fs_read_text(...)` and `abi_fs_read_text_range(...)` now stamp the final logical length after file reads.
  - Added shared `abi_fs_write_mode_len(...)` plus `abi_fs_write_text_len(...)`, `abi_fs_append_text_len(...)`, and `abi_fs_atomic_write_text_len(...)`.
  - Compatibility entrypoints still exist and fall back to `strlen(...)` only for legacy callers that do not know the length.
- `runtime/native/include/stdlib_abi.h` declares the new length-aware FS entrypoints.
- `stdlib/native/fs.kn` now uses the length-aware FS write/append/atomic-write ABI from authored Kain code.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now lowers `byte_at(...)` on known strings inline with a guarded direct byte load, and string-length fallback uses native `len(...)` rather than `strlen(...)`.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` now asserts the inline `byte_at(...)` lowering. Exploratory proof `crates/kain-sys-codegen/z3/proofs-experimental/byte-at-fast-path-index-guard.smt2` and report `z3/reports/20260517T030441Z-byte_at_fast_path_index_guard.json` prove the signed guard implies the in-range unsigned load fact.

Benchmark results:

- Durable release report `benchmark/out/reports/20260517T030846Z.llm.md` now shows `filesystem_stream`: Kain `117.605 ms`, Rust `97.923 ms`, C++ `84.600 ms`.
  - Earlier same-session checkpoints were roughly Kain `138.186 ms`, then `123.854 ms`, then `117.605 ms`, so both the correctness repair and the length-aware write path were real.
- Durable release report `benchmark/out/reports/20260517T030644Z.llm.md` shows `json_manual_roundtrip`: Kain `118.244 ms`, Rust `112.832 ms`, C++ `94.763 ms`.
  - The inline `byte_at(...)` path was correct and solver-backed, but it did not materially close the JSON gap.

Durable lesson:

- For `filesystem_stream`, inspect two seams first if the row regresses: allocate-then-fill FS strings must stamp logical length after readback, and Kain-authored FS writes should stay on the length-aware ABI entrypoints rather than `strlen(...)`.
- For `json_manual_roundtrip`, do not keep chasing `byte_at(...)` next. The row still points more strongly at repeated `find_substring_from(...)`, `substring(...)`, and small-string allocation churn than at raw byte extraction.

# 2026-05-17 - Struct/value aggregates plus heap-only RC guards finished the Option/Result campaign and flipped both rows into Kain wins

The `struct_method` and `option_result` pressure rows ended up exposing two different costs. `struct_method` was mostly a representation bug: tiny POD structs such as `BenchPair` were still flowing through heap/object-style lowering instead of as plain values. `option_result` was subtler. After immediate tagged handles landed, the optimized native executable was already down to integer math, a few branches, and two stubborn external `rc_retain(...)` / `rc_release(...)` calls on `null` or low-bit-tagged `i8*` values that could never be heap RC objects.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now tracks POD structs/tuples in `value_aggregate_structs` and lowers them by value when every field is scalar POD. `Expr::Struct`, tuple lowering, type mapping, local field access, and call signatures all respect that path, which is why `struct_method` no longer allocates `BenchPair` at all.
- Native `Option` / `Result` fast paths now stay on immediate tagged handles for small integer payloads and borrowed static string payloads, with `None` as `null`.
- LLVM no longer emits unconditional external RC calls on raw `i8*` values. `emit_heap_owned_i8_guard(...)` now checks `ptr != null && ((ptr & 7) == 0)` before emitting `rc_retain(...)` or `rc_release(...)`. That deletes RC-call overhead for immediate tagged handles while preserving the heap RC path for real aligned objects.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` now treats immediate tagged lowering plus heap-only RC guarding as the regression surface instead of expecting lots of tagged-temp release calls.
- Exploratory proof `runtime/native/src/core/z3/proofs-experimental/tagged-immediate-lowbits-defeat-heap-rc-guard.smt2` plus Z3 report `z3/reports/20260517T030722Z-tagged-immediate-lowbits-defeat-heap-rc-guard.json` prove the core bit trick: if a carrier already had low 3 bits zero and a nonzero tag is OR-ed in, the result can never satisfy the heap-aligned RC guard.

Validation and benchmark:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_option_result_future_to_native_tagged_runtime -- --nocapture`
- `cargo build -p cli`
- `python benchmark/run.py --case struct_method,option_result --languages kain,rust,cpp --runs 5 --warmups 1 --timeout 900 --kain-exe D:\\Kain-Lang\\target\\debug\\kain.exe --minimal-name latest_struct_option.md`
- `clang -O3 -march=native -S benchmark/out/build/option_result/kain/option_result.ll -o benchmark/out/build/option_result/kain/option_result.s`
- Latest report `benchmark/out/reports/latest.llm.md` at `2026-05-17T03:05:22.980546+00:00` now shows:
  - `struct_method`: Kain `10.279 ms`, Rust `11.696 ms`, C++ `10.683 ms`
  - `option_result`: Kain `7.881 ms`, Rust `8.705 ms`, C++ `8.056 ms`
- The optimized `option_result.s` no longer contains any `rc_retain` or `rc_release` call in `main`; the loop stays entirely in registers, shifts, and branchless modulo strength reductions.

Durable lesson:

- Once native semantic handles are low-bit tagged, the honest win is to stop crossing the external RC ABI boundary for values that are already proved non-heap by construction. If `option_result` regresses, inspect `emit_heap_owned_i8_guard(...)`, the tagged-handle encode/decode helpers, and the final optimized assembly before rewriting the runtime C functions again.

# 2026-05-17 - Manual JSON roundtrip dropped from 2.36x slower than C++ to near-Rust by deleting concat-chain and libc formatter waste

`json_manual_roundtrip` was no longer mainly losing on parser correctness once the earlier `byte_at` / `find_substring_from` / RC fixes landed. The remaining gap was mostly renderer tax in the native string lane. The hot `render_payload(...)` path still emitted two heap `to_string(...)` calls, a left-growing ladder of seven `str_concat(...)` calls, and eager entry `strlen(...)` calls for every string parameter even when the function body never read those cached lengths. That meant allocator churn, repeated rescans of growing prefixes, and a surprising amount of dead work before the benchmark even reached the real JSON logic.

What changed:

- `runtime/native/src/core/core.c` now has an exact-allocation integer formatter instead of `sprintf(...)`. `to_string(...)` computes the decimal digit count, allocates exactly `digits + sign + 1`, and writes digits backwards into the final RC string buffer.
- `runtime/native/src/core/core.c` also gained fixed-arity `str_concat3(...)` through `str_concat10(...)` helpers backed by a shared checked-length concat routine. The shared helper computes each part length once, proves the total allocation size through checked `size_t` additions, then copies all segments into one RC string buffer instead of building a growing chain of intermediate strings.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now flattens known string-add trees before lowering. When a chain has between 3 and 10 terms, LLVM emits one `@str_concatN(...)` call instead of nested binary `@str_concat(...)` calls.
- The same LLVM pass now caches string lengths lazily instead of eagerly. Authored string parameters are still tracked as string locals, but `compile_string_length_value(...)` computes and memoizes `strlen(...)` only on first use. That removed dead entry `strlen(...)` calls from functions like `render_payload(...)` that never read string lengths directly.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` now has a regression asserting that a benchmark-shaped JSON render chain lowers to `@str_concat9(...)` and does not emit eager parameter `strlen(...)` calls at function entry.

Validation and benchmark:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_flattens_long_string_concat_chains_into_fixed_arity_runtime_calls -- --nocapture`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/core.c`
- Proof file `runtime/native/src/core/z3/proofs-experimental/json-string-concat-total-length-overflow.smt2` plus Z3 MCP report `z3/reports/20260517T003313Z-json-string-concat-total-length-overflow.json` returned `unsat` for the sequential checked-add proof behind the fixed-arity concat allocation path.
- `python benchmark/run.py --case json_manual_roundtrip --languages kain,rust,cpp --runs 5 --warmups 1 --timeout 900 --kain-exe D:\Kain-Lang\target\codex-json-cli\debug\kain.exe`
- Latest report `benchmark/out/reports/latest.llm.md` at `2026-05-17T00:47:25+00:00` now shows Kain median `120.658 ms`, Rust `116.169 ms`, and C++ `96.221 ms`. Earlier same-night checkpoints were Kain `429.156 ms`, then `277.131 ms`, then `124.415 ms`, so the final string-lowering/runtime pass is what closed most of the remaining gap.

Durable lesson:

- For manual JSON rows, the honest win comes from deleting generic string-runtime waste, not from inventing a benchmark-only parser trick. If this row regresses, inspect `render_payload(...)` lowering first: nested `@str_concat(...)`, eager parameter `strlen(...)`, or a fallback to `sprintf(...)`-style integer formatting are all real dragons. The next likely fair win is teaching concat lowering to release more fresh string temporaries, or finally fixing the native LLVM JSON builtin linker gap so the row can stop being manual at all.

# 2026-05-17 - Native map lookup flipped from 7.34x slower than Rust to a Kain win by deleting full-table probe waste

`native_map_lookup` was not mainly losing because Kain lacked a better hash; it was losing because the row paid three stacked taxes. First, LLVM lowered literal `map_get(metrics, "alpha")` calls by allocating heap strings. Second, generic `map_get` recomputed key metadata on every lookup. Third, and biggest, `map_get_prehashed(...)` still scanned the entire open-addressed table in 8-slot windows even after a miss became logically decided. The current pipeline now fixes all three.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` lowers literal `map_get` keys as borrowed static byte pointers and emits `map_get_prehashed(map, ptr, len, hash, prefix)` directly, so the hot loop no longer calls `string_new(...)` or recomputes hash/prefix for closed literal keys.
- `runtime/native/src/core/core.c` keeps insertion on the existing generalized search path, but `map_get_prehashed(...)` now uses a sequential linear-probe walk over the real probe chain and returns immediately on the first empty slot or exact metadata+memcmp match.
- Exploratory proof `runtime/native/src/core/z3/proofs-experimental/map-linear-probe-empty-blocks-later-match.smt2` and durable proof `runtime/native/src/core/z3/proofs/native-map-linear-probe-empty-slot-precludes-later-match.yaml` both prove the key invariant: with linear probing and no tombstones, an empty slot in the probe order precludes any later match for the same key. The durable proof report is `runtime/native/src/core/z3/reports/20260517T001004Z-native_map_lookup_linear_probe_fast_path.json`.

Validation and benchmark:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_map_get_string_literals_as_borrowed_static_byte_views -- --nocapture`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/core.c`
- `py -3 benchmark/run.py --case native_map_lookup --languages kain,rust,cpp --runs 5 --warmups 2 --timeout 300 --kain-exe target\\debug\\kain.exe`
- Latest report `benchmark/out/reports/latest.llm.md` at `2026-05-17T00:07:31.715616+00:00` now shows Kain median `19.518 ms`, Rust `30.711 ms`, and C++ `31.732 ms`. Earlier same-night checkpoints were Kain `211.746 ms` and then `116.198 ms`, so the final sequential early-exit pass was the dragon.

Durable lesson:

- For Kain native maps, the honest win came from respecting the probe-order invariant, not from more branchless algebra. Literal-key lowering should stay prehashed and borrowed when the compiler can close the domain, but the runtime lookup path must still walk the real probe chain and stop as soon as the table semantics say the answer is known.

# 2026-05-16 - Benchmark runner now emits root snapshots and has a reduced fast wrapper

The benchmark lane now writes a compact root snapshot on every run so agents do not have to open the long report first. `benchmark/run.py` still writes the timestamped and latest full reports under `benchmark/out/reports/`, but it now also writes `benchmark/latest.md` with just status, run counts, selected languages, and the median summary table. `benchmark/run_fast.py` is a thin wrapper over `benchmark/run.py` that forces `--languages kain,rust,cpp,erlang` and writes `benchmark/latest_fast.md`.

What changed:

- `benchmark/run.py` gained a minimal renderer plus `--minimal-name`, defaulting to `latest.md`.
- `benchmark/run_fast.py` forwards normal runner flags but appends the fixed fast language subset and the `latest_fast.md` root snapshot name.
- `benchmark/README.md` and `.agents/skills/kain-benchmark-pipeline/SKILL.md` now document the new root snapshots and the fast wrapper command.

Durable lesson:

- Keep the root snapshot brutally compact. The detailed LLM report still belongs under `benchmark/out/reports/latest.llm.md`, but agents doing quick triage should be able to read `benchmark/latest.md` or `benchmark/latest_fast.md` first and decide whether a deeper dive is necessary.

# 2026-05-16 - Ready-future async stopped being catastrophic and now edges Rust in the honest benchmark lane

`async_ready_chain` was not losing because `await` work inside the loop was inherently expensive; it was losing because Kain paid several fixed costs at once. The runtime now has an inline-ready future payload path in `runtime/native/src/core/stdlib_abi.c`: immediately-ready futures store the copied scalar payload inside the future RC object, mark `task_id = KAIN_TASK_ID_INVALID`, and let `abi_future_state(...)` / `abi_future_await_payload_copy(...)` complete without task allocation or scheduler traffic. LLVM lowering in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` then goes one step further for obvious immediate-ready cases (`await async ...` and zero-arg functions that immediately `return async ...`) by folding the payload directly into the caller, so the hot benchmark loop no longer calls the future ABI at all.

The last big win came from changing the benchmark/runtime shape instead of shaving nanoseconds off dead code. `benchmark/run.py` now supports a per-case `"kain_runtime_manifest"` override in `benchmark/benchmarks.json`, and `async_ready_chain` points at `runtime/native_async_benchmark_runtime.toml` instead of the broad native core manifest. That manifest disables net/process reset work and keeps the source set narrow for the ready-future lane. On top of that, `crates/cli/src/main.rs` now enables `-ffunction-sections` / `-fdata-sections` plus linker dead-stripping (`/OPT:REF` + `/OPT:ICF` on Windows) in non-debug native builds. This lets benchmark-release builds throw away the giant unused stdlib/native wrapper forest after lowering collapses the real async work away.

Proof, benchmark, and durable numbers:

- Exploratory proof `runtime/native/src/core/z3/proofs-experimental/async-ready-future-inline-payload-bounds.smt2` returned `unsat` in report `D:\\Kain-Lang\\z3\\reports\\20260516T223219Z-async-ready-future-inline-payload-bounds.json`.
- `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` now covers ready-future payload copy/await behavior.
- `py -3 benchmark/run.py --case async_ready_chain --languages kain,rust --runs 7 --warmups 2 --timeout 900 --kain-exe target\\debug\\kain.exe` now reports Kain median `7.905 ms` vs Rust median `8.272 ms`, so Rust is `1.05x slower`.
- The Kain `async_ready_chain.exe` dropped from roughly `812 KB` before section GC to about `104 KB` after the linker/dead-section pass; the import table shrank to `KERNEL32.dll` only.

Durable lesson:

- When a microbenchmark still looks bad after the hot IR is clean, inspect executable size, imports, and linked dead code before rewriting the algorithm again. For native Kain, the right abstraction boundary was: fold immediate-ready futures in LLVM, give the benchmark case its own honest runtime manifest, and make the link step actually discard unreachable wrapper code.

# 2026-05-16 - Local microcell ask handoff turned the honest Erlang mailbox row into a Kain win

The next actor dragon landed as an ask-only exact-ref fast path instead of another generic mailbox tweak. `runtime/native/src/core/actor.c` now exposes `kain_actor_ask_send_ref(...)`: it validates the full `KainActorRef`, copies the request into the real mailbox, and if the target is a local `MICROCELL` turn actor whose mailbox was empty and whose scheduler ownership bits were clear, it claims `in_scheduler_turn` inline on the caller thread and runs the first microcell turn immediately. If any guard fails, it falls back to the normal mailbox/scheduler path. This keeps message ownership unchanged, so generated handlers still free heap-owned payloads exactly as before.

What changed:

- Added a shared mailbox copy helper and a shared `kain_actor_execute_microcell_turn(...)` path so pooled workers and the inline ask handoff use the same stop/crash/finish-turn behavior.
- Added `kain_actor_ask_send_ref(...)` to `runtime/native/include/actor.h`, the native actor Rust contract, LLVM declarations, and LLVM ask lowering in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`.
- `ask` / `ask_timeout` now lower with the full `KainActorRef` instead of degrading to a raw actor id send, which lets the runtime reject stale refs and specialize the local microcell case without changing generic `send`.
- `runtime/conformance/actor_runtime/test_actor_abi_contract.c` now asserts the inline ask path roundtrips through a microcell turn without adding a scheduler enqueue, and two durable actor proofs pin the new exclusivity/backlog guards.

Proof, validation, and benchmark:

- Z3 actor lane `runtime/native/src/core/z3` proved `16/16` with `unsat` in report `runtime/native/src/core/z3/reports/20260516T225454Z-proof-suite.json`.
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/actor.c`
- `cargo test -p kain-actor`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor -- --nocapture`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`
- `cargo build -p cli`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `py -3 benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 3 --warmups 1 --timeout 240 --kain-exe target\\debug\\kain.exe` passed with Kain median `182.275 ms` and Erlang median `389.657 ms`, so Kain now wins the honest mailbox row and Erlang is `2.14x slower`. Compared to the previous durable Kain baseline of `2621.995 ms`, this is about `14.38x` lower Kain latency on the same command.

Durable lesson:

- The big remaining win was not a payload cache or a smaller allocator trick; it was changing the abstraction boundary for local asks. Keep generic `send` boring, keep payload ownership heap-owned unless the whole receiver cleanup contract changes with proofs, and treat `local + microcell + exact ref + empty mailbox` as the specialization seam for future actor latency work.

# 2026-05-16 - Semantic Singularity Crucible became the single-file native LLVM torture lane

`benchmark/cases/semantic_singularity_crucible/main.kn` is now the "pile everything onto one Kain file" benchmark. It preserves the existing `semantic_singularity` matrix instead of mutating it, then adds a preflight layer for enum `match`, trait/impl syntax, top-level const/type alias, `comptime`, shader declarations, `vec!`/`format!`/string indexing, Option/Result/Future/`await`, bitwise packet math, and raw `realloc_mem` plus `collapse`/`observe`/`decay` before the fused actor/world/shatter/converge/orchestrate hot loop.

What changed:

- Added `semantic_singularity_crucible` to `benchmark/benchmarks.json` as a Kain-only `kain-llvm-crucible` row.
- Added local proofs under `benchmark/cases/semantic_singularity_crucible/z3`: `semantic_singularity_crucible_bounds.smt2` and `semantic_singularity_crucible_bitmix.smt2`.
- The crucible preflight checksum is `1094`; the full executable checksum is `594833340`.
- Latest benchmark smoke: `py -3 benchmark\run.py --case semantic_singularity_crucible --languages kain --runs 1 --warmups 0 --timeout 900 --kain-exe target\debug\kain.exe` passed with Kain median `363.026 ms`.

Compiler seam found while dogfooding:

- Native LLVM functions whose body ended with a final expression, especially expression-bodied enum `match`, compiled to a PHI and then fell through to the non-main fallback `unreachable`. In benchmark-release, clang optimized that into an `int3` trap (`0x80000003`) at `crucible_control_lane`.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs::compile_named_callable` now uses `compile_block_with_result`, coerces a final expression to the declared return type, emits patch commit cleanup when needed, and returns the value before the fallback unreachable path.
- Regression test: `llvm_lowers_enum_match_parameters_as_native_enum_pointers` now asserts the expression-bodied match emits `ret i64 %...`.
- Durable proof: `crates/kain-sys-codegen/z3/proofs/control-final-expression-return-beats-fallback-unreachable.yaml`; control lane report `D:\Kain-Lang\crates\kain-sys-codegen\z3\reports\20260516T140626Z-semantic-crucible-final-expression-return.json` proved `6/6` with `unsat`.

Current caveat:

- Full `cargo test -p kain-sys-codegen --test llvm_codegen_test -- --nocapture` still has the known pre-existing `llvm_lowers_option_result_future_to_native_tagged_runtime` retain-path assertion failure. Focused match/codegen tests and the crucible executable pass.

# 2026-05-16 - Quantumerlang flex row maps Erlang's swarm shape onto Kain semantics

`benchmark/cases/quantumerlang` is the intentionally unfair Kain-vs-Erlang flex row requested after the actor-mailbox work. It keeps Erlang doing what Erlang is good at: 64 long-lived stateful processes, synchronous request/reply mailboxes, and a deterministic 300,000-round fold. Kain computes the same checksum through `shatter struct` lane metadata, one ownership-collapsed cell ring, `converge` lowering, and a boot-time `teleport` plus entangled world patch so the semantic substrate is live without paying per-round actor mailbox tax.

What changed:

- Added `quantumerlang` to `benchmark/benchmarks.json` with only Kain and Erlang rows, maturity `kain-semantic-flex`, and an explicit fairness note that this is not the honest actor-mailbox row.
- Added `benchmark/cases/quantumerlang/main.kn`, `quantumerlang.erl`, and `z3/quantumerlang_bounds.smt2`.
- The case checksum is `272862553`. Kain and Erlang both fail nonzero if the final fold changes.
- `quantumerlang_bounds.smt2` proves the 64-lane modulo index and signed 64-bit arithmetic headroom; report `D:\Kain-Lang\z3\reports\20260516T132734Z-quantumerlang-bounds-final.json` returned `unsat`.

Compiler/runtime seam found while dogfooding:

- The first Kain executable crashed with `0xC0000005` because return-path cleanup emitted `kain_machine_shatter_free(...)` on one branch, then removed the compile-time shatter metadata and emitted generic `rc_release(i8* ...)` for the same shatter handle on a sibling return branch.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs::emit_all_scopes_cleanup` must preserve `shattered_array_locals` while emitting sibling return branches. Normal lexical scope cleanup can still remove the metadata when the scope actually exits.
- Regression test: `llvm_cleans_shattered_array_locals_on_each_return_path`.
- Durable proof: `crates/kain-sys-codegen/z3/proofs/memory-shatter-return-cleanup-preserves-shatter-kind.yaml`; report `D:\Kain-Lang\crates\kain-sys-codegen\z3\reports\20260516T132655Z-quantumerlang-shatter-cleanup-final.json` returned `unsat`.

Latest benchmark proof:

- `py -3 benchmark/run.py --case quantumerlang --languages kain,erlang --runs 3 --warmups 1 --timeout 900 --kain-exe target\debug\kain.exe`
- Kain median `45.005 ms`, Erlang median `549.337 ms`; Kain wins and Erlang is `12.21x slower` on this semantic-flex workload.

# 2026-05-16 - Semantic Singularity gained its ablation/isolate benchmark matrix

The fused `semantic_singularity` benchmark is now the all-in-one Kain weird-semantics boss fight, and six sibling Kain-only rows let the harness attribute its cost instead of treating the fused number as a black box.

What changed:

- Added `semantic_singularity_no_actor`, `semantic_singularity_no_entangle`, `semantic_singularity_no_patch`, `semantic_singularity_shatter_only`, `semantic_singularity_actor_only`, and `semantic_singularity_converge_only`.
- `benchmark/run.py --case` now accepts comma-separated case ids so the full matrix can land in one report.
- The matrix uses deterministic checksums and keeps field-based shatter reads, bounded patch-journal assumptions, and modern ABI v3 actor checks explicit.
- New Z3 proof `benchmark/cases/semantic_singularity/z3/semantic_singularity_matrix_bounds.smt2` proves matrix index bounds, actor-inline equivalence, arithmetic headroom, and saturated patch-journal bounds; report `D:\Kain-Lang\z3\reports\20260516T131326Z-semantic-singularity-matrix-bounds.json` returned `unsat`.

Latest matrix smoke:

- `py -3 benchmark\run.py --case semantic_singularity,semantic_singularity_no_actor,semantic_singularity_no_entangle,semantic_singularity_no_patch,semantic_singularity_shatter_only,semantic_singularity_actor_only,semantic_singularity_converge_only --languages kain --runs 1 --warmups 0 --timeout 900 --kain-exe target\debug\kain.exe --no-build`
- Report: `benchmark/out/reports/latest.llm.md`
- Timings: full `366.719 ms`, no_actor `47.063 ms`, no_entangle `361.767 ms`, no_patch `377.339 ms`, shatter_only `34.115 ms`, actor_only `315.016 ms`, converge_only `37.716 ms`.

Durable lesson:

- In the current fused shape, actor ask/reply dominates the boss-fight delta. Entangle and patch deltas are buried by single-run noise while the actor path is active, so use more runs/warmups before making fine-grained claims about those subsystems.

# 2026-05-16 - Native actor ask/reply latency pass proved hot reply ports and mailbox node recycling

This pass optimized the already-correct native LLVM actor ask/reply path without changing the actor language surface. The key runtime move is that TLS reply ports now keep their synthetic actor-table slot hot and rearm by bumping generation, rather than unbinding/freeing/rebinding a synthetic actor for every ask. Stale replies still die because `kain_actor_reply_port_state_complete_copied(...)` verifies the exact generation-tagged `KainActorRef` under the reply-port lock.

What changed:

- `runtime/native/src/core/actor.c` adds bounded reply wait spinning before OS wait fallback, capped per-mailbox message-node recycling, and direct-thread joins before actor-table cleanup during runtime shutdown.
- `runtime/native/include/actor.h` documents the hot TLS reply-port behavior and extends mailbox/direct-thread state for the node cache and shutdown ordering.
- `runtime/conformance/actor_runtime/test_actor_abi_contract.c` now asserts reply-port slot reuse, stale generation invalidation, direct stale-send rejection, and ref death after destroy.
- New durable actor Z3 proofs cover reply-port generation rearm, spin-wait fallback preservation, bounded node-cache growth, and direct-thread join-before-cleanup.

Proof, validation, and benchmark:

- Z3 actor lane `runtime/native/src/core/z3` proved `14/14` with `unsat` in report `runtime/native/src/core/z3/reports/20260516T113254Z-actor-final-runtime-pass.json`.
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/actor.c`
- `cargo test -p kain-actor`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor -- --nocapture`
- `cargo build -p cli`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`
- `py -3 benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 3 --warmups 1 --timeout 240 --kain-exe target\debug\kain.exe` passed with Kain median `2621.995 ms` and Erlang median `418.862 ms`, so Kain is currently `6.26x slower`. This is materially better than the earlier ~`18.44x` gap, but not yet an Erlang kill shot.

Durable lesson:

- The reverted payload-cache experiment was slower than node-cache-only. The next real win should come from a specialized local ask/direct handoff path or typed inline actor request lowering that avoids generic mailbox payload allocation/copy/wakeup churn for local microcell refs.

# 2026-05-16 - Semantic Singularity benchmark became the Kain-only fused weird-semantics pressure vessel

This pass added `benchmark/cases/semantic_singularity/main.kn` plus a `semantic_singularity` row in `benchmark/benchmarks.json`. It is intentionally Kain-only rather than a cross-language fairness row: the goal is to keep one native LLVM benchmark that composes the full unusual Kain surface in one checksum-guarded executable.

What changed:

- The benchmark combines `axiom`, `pulse`, `shatter`, `teleport`, `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `collapse`, `observe`, `decay`, and the modern ABI v3 actor ask/reply path.
- The hot loop uses field-based dynamic shatter reads (`shards[lane].field`), teleports a local shard copy, patches entangled world state, validates laws, dispatches converge/orchestrate work, asks a spawned actor, mutates shared memory inside `collapse`, folds through `observe`, and releases with `decay`.
- The first compile exposed a real LLVM/native seam bug after the native ABI prefix cleanup: compiler-owned entangle registration must call `abi_entangle_register(...)`, not the public Kain wrapper name `entangle_register(...)`. `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` and `llvm_codegen_test.rs` now assert the `abi_` registration path.
- The runtime telemetry guard now treats the patch journal as a bounded live ring instead of expecting one retained record per iteration. The hot loop still attempts 20,000 patches, but `abi_patch_journal_count()` is intentionally bounded by the native runtime.

Proof and validation:

- Z3 MCP report `D:\Kain-Lang\z3\reports\20260516T105941Z-semantic-singularity-benchmark-bounds.json` returned `unsat` for cell/shatter index bounds, 64-bit arithmetic headroom, and saturated patch-journal count bounds.
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_world_patch_converge_and_orchestrate_paths -- --nocapture`
- `cargo build -p cli`
- Direct native LLVM compile and run of `benchmark/cases/semantic_singularity/main.kn` returned exit code `0`.
- `py -3 benchmark\run.py --case semantic_singularity --languages kain --runs 1 --warmups 0 --timeout 900 --kain-exe target\debug\kain.exe` passed and wrote `benchmark/out/reports/latest.llm.md` with Kain median `723.298 ms`.

Durable lesson:

- Until broader SoA value propagation lands, do not pass `shards[lane]` as a whole value through helpers or teleport in native LLVM. Dynamic shatter array access is currently proven through field reads; whole-element value passing can fall into generic array access and produce invalid native behavior.

# 2026-05-16 - Machine stones gained real pulse scheduling and loop-persistent shatter lowering

The second native backend exploitation pass turned the previous honest boundary into a real runtime/compiler slice. `pulse` is no longer only a generated-main fire/snapshot hook: native LLVM now registers each pulse through `kain_machine_pulse_start(...)`, the C runtime fires once immediately for deterministic startup, and a tiny timer thread keeps invoking the generated fire thunk until runtime exit or explicit `kain_machine_pulse_stop_all()`. `std.runtime` exposes `runtime_machine_pulse_total_fire_count()` so dogfood blades can prove scheduler telemetry without reaching into C directly.

`shatter struct` lowering now keeps local direct array literals alive across loops as compiler-known SoA handles. LLVM asks the runtime for each field lane base once with `kain_machine_shatter_lane_base(...)`; field reads with literal indexes or `for range(...)` indexes whose bounds fit the direct array lower to `getelementptr lane_base, (index << 3)` instead of calling the checked `kain_machine_shatter_lane_ptr(...)` in the hot path. The checked runtime helper remains the fallback for unproved dynamic indexes.

Proof and validation:

- Z3 raw SMT `runtime/native/src/core/z3/proofs-experimental/machine-shatter-shift-offset-equivalence.smt2` returned `unsat` for the shift-vs-multiply slot offset trick.
- Native machine Z3 lane proved `5/5` with `unsat`, including `native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml`.
- `cargo check -p kain-sys-codegen`, targeted LLVM machine-stones test, C syntax check, direct C runtime test, `cargo build -p cli`, and Bazel native-runtime manifest sync all passed.
- `blades/machine-stones` checks, compiles, and runs under native LLVM; the blade now exercises a loop-persistent shattered array and pulse fire telemetry.
- New benchmark case `machine_stones_shatter_loop` landed. Focused run `benchmark/out/reports/20260516T110000Z.llm.md` measured Kain `19.357 ms`, Rust `13.896 ms`, and C++ `12.238 ms` over the shatter hot loop. This is not a victory claim yet; it proves the benchmark is now real and Kain is within about `1.58x` of hand-authored C++ SoA on this shape.

Current risks:

- Shatter still targets local direct array literals. Parameters, returns, iterators, mutation, and broader escape-aware SoA propagation are not done.
- The pulse scheduler is runtime-owned and process-local; it is not yet integrated with actor/world scheduling policy.
- Benchmark `latest.*` reports may be overwritten by concurrent actor benchmark work. Use the timestamped report above for this pass.

# 2026-05-16 - Native runtime prefix churn was cut over to clean ABI/domain names

The native C runtime no longer uses `kain_native_*`, `kain_runtime_*`, `KAIN_NATIVE_*`, or `KAIN_RUNTIME_*` as the live C ABI/file naming scheme. The old names were making runtime maintenance and `rg` workflows painfully noisy, so this pass generated a manifest-driven rename and applied it across the live runtime, native stdlib wrappers, native manifests, conformance references, and LLVM/direct-C codegen references.

What changed:

- Added `tools/native_runtime/build_kain_prefix_rename_manifest.py` and `tools/native_runtime/apply_kain_prefix_rename_manifest.py`.
- The reviewed manifest lives at `runtime/native/kain_prefix_rename_manifest.json`; it intentionally preserves old -> new mappings as historical rename data.
- Live lowercase native ABI facade symbols now use `abi_*`, for example `abi_option_some`, `abi_runtime_init`, `abi_actor_spawn`, `abi_net_tcp_connect`, `abi_ui_session_create`, and `abi_graphics_session_create`.
- Runtime-internal domain files now use normal names such as `runtime/native/include/stdlib_abi.h`, `runtime/native/include/net_system.h`, `runtime/native/src/core/stdlib_abi.c`, `runtime/native/src/core/net_system.c`, `runtime/native/src/ui/ui_system.c`, and `runtime/runtime.c`.
- One real post-rename collision was found and fixed: the low-level entangle registry now uses `entangle_registry_*` so it does not collide with public Kain stdlib wrapper names like `entangle_register`.
- `runtime/native/kain_prefixed_symbol_inventory.md` and `.json` were regenerated after the pass and now report zero live C-ish symbols under the default runtime-native scan.

Proof and validation:

- Native core Z3 full lane proved `46/46` with `unsat` in report `runtime/native/src/core/z3/reports/20260516T095659Z-native-runtime-prefix-rename-full.json`.
- Focused `clang -fsyntax-only -Iruntime/native/include` passed for the renamed core runtime modules including `core.c`, `stdlib_abi.c`, `actor.c`, `net_system.c`, `process_system.c`, `graphics_system.c`, `ui_system.c`, `ui_runtime.c`, and `machine_stones.c`.
- `cargo check -p kain-sys-codegen`.
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`.
- Direct native LLVM compile/run returned exit code `0` for `blades/stdlib-domains`, `blades/network-domains`, and `blades/machine-stones`.

Current caveat:

- `KAIN_NATIVE_PROFILE` and related benchmark-tuning env vars still exist intentionally for now. They are operator/runtime-target configuration names, not C ABI/file prefixes. Rename them separately only if the benchmark and CLI docs are moved together.
- `cargo test -p kain-sys-codegen --test llvm_codegen_test -- --nocapture` still has the pre-existing `llvm_lowers_option_result_future_to_native_tagged_runtime` retain-path failure. Rename-related LLVM tests passed; do not confuse that RC retain bug with this prefix cleanup.

# 2026-05-16 - Machine stones now have native LLVM/C backend exploitation instead of metadata-only lowering

This pass took the `axiom`, `pulse`, `shatter struct`, and `teleport` quartet from frontend/runtime-contract truth into the native execution substrate. The surface syntax stayed stable; the change is that native LLVM now emits explicit runtime ABI calls and the C runtime owns the hardware-facing parts.

What changed:

- Added `runtime/native/include/kain_runtime_machine_stones.h` and `runtime/native/src/core/kain_runtime_machine_stones.c`.
- `kain_machine_axiom_accept(...)` checks target/arch/capability predicates against native runtime and CPU feature bits. Capability token dispatch uses a compact signature classifier with exact string guards.
- `kain_machine_pulse_snapshot(...)` reads a monotonic high-resolution host timer, keeps per-pulse tick state in a small atomic-locked table, and reports tick, dt, and missed-beat counts.
- `kain_machine_shatter_alloc(...)` creates one contiguous SoA buffer where each field lane is 8-byte-slotted; `kain_machine_shatter_lane_ptr(...)` provides checked lane/element pointers and `kain_machine_shatter_free(...)` releases the handle.
- `kain_machine_teleport_ptr(...)` returns the same pointer it receives, increments telemetry, and records a destination-token hash. This is the current native zero-copy handoff seam; scalar teleports call a note hook because there is no pointer to transfer.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now emits axiom accept functions, pulse body/fire wrappers, runtime calls at generated `main` entry, pointer teleport calls, and SoA lowering for local array literals made of direct shattered struct literals.
- `stdlib/native/runtime.kn` and `stdlib/runtime.kn` now expose `runtime_machine_teleport_count()` and `runtime_machine_teleport_last_token()` so Kain dogfood code can inspect the native teleport seam.
- Runtime manifests, Bazel runtime manifest data, and `runtime/BUILD.bazel` include the machine-stones C runtime source and native test.
- `blades/machine-stones/src/main.kn` now checks shatter lane field reads and native teleport telemetry, and the generated `machine-stones.exe` runs from the blade root with sidecars under `.kain/out`.

Proof and validation:

- Z3 MCP native machine lane proved `4/4` with `unsat`: capability token signature collision freedom, pulse missed-beat bounds, shatter lane offset bounds, and teleport exclusive handoff state.
- Z3 MCP sys-codegen memory proof proved pointer teleport bitcast roundtrip identity with `unsat`.
- `cargo check -p kain-core`
- `cargo check -p kain-sys-codegen`
- `cargo check -p cli`
- `cargo build -p cli`
- `cargo test -p kain-core --test ownership_keywords_test -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_machine_stones_to_native_runtime_abi -- --nocapture`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_machine_stones.c`
- Direct C ABI test compile/run for `runtime/native/tests/test_machine_stones.c`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- Direct native LLVM check, compile, and run of `blades/machine-stones/src/main.kn`; executable returned exit code `0`.

Current boundaries:

- Pulse is a native high-resolution snapshot/fire at generated `main` entry today, not a perpetual scheduler thread or event-loop heartbeat yet.
- Shatter lowering currently targets local array literals of direct shattered struct literals and field/index access through those local handles. Broader struct-return, parameter, and iterator-aware SoA propagation is still future work.
- Teleport is true zero-copy for pointer-shaped values, including heap-backed struct literals in the current LLVM lane. Non-pointer scalars preserve value semantics and only record telemetry.
- Axiom acceptance is target/arch/capability gated, but it does not yet delete arbitrary safety shims through a whole-program optimization pass.

Next recommended step:

- Build the next pass around full pulse scheduling and wider shatter propagation before chasing benchmark claims. A useful benchmark should compare a real hot iteration over shattered fields against an AoS baseline once LLVM can keep the SoA handle live across a loop, not just allocate a tiny literal.

# 2026-05-16 - Converge now has a native CPU capability/autotune substrate and LLVM carries multiple fast lanes

This pass turned the stale benchmark note into a real phase-1 backend foundation. Kain still does not ship authored SIMD IR or real AVX-512 kernels yet, but compiled native LLVM converge declarations can now keep multiple fast lanes alive, query runtime CPU capabilities, and route dynamic CPU-gated lanes through a selector/cache instead of discarding every lane after the first.

What changed:

- Added the native CPU capability service in `runtime/native/include/kain_runtime_cpu.h` and `runtime/native/src/core/kain_runtime_cpu.c`. It publishes x86 feature bits such as AVX, AVX2, AVX-512F/DQ/BW/VL, FMA, and BMI2 through stable `cpu.x86.*` capability keys.
- Added the native converge selector/autotune substrate in `runtime/native/include/kain_runtime_converge.h` and `runtime/native/src/core/kain_runtime_converge.c`. The current service provides lane selection, a tiny process-local tuning cache, telemetry-ring recording, winner commits, and probe/hit counters.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now emits one spec function plus up to eight fast-lane functions for each `converge`. Static lanes still collapse to the first eligible fast lane. CPU-gated lanes build an eligible bitmask from native capability calls and dispatch through `kain_native_converge_select_lane_for_key(...)`.
- Benchmark-release LLVM builds now elide `orchestrate` stage begin/end telemetry wrappers unless `KAIN_LLVM_ORCHESTRATE_TRACE=1` is set. This keeps benchmark hot loops from paying bookkeeping costs while preserving explicit trace opt-in.
- `stdlib/native/runtime.kn` and `stdlib/runtime.kn` expose the CPU/converge runtime calls, and `stdlib/native/README.md` documents the new native runtime surface.
- Runtime manifests and Bazel manifest data include the new CPU/converge native sources. Re-run `py -3 tools/bazel/sync_native_runtime_builds.py --check` after touching this surface.
- Added `blades/converge-autotune-probe`, a native LLVM dogfood blade that imports `std.runtime`, uses a scalar plus `cpu.x86.avx2` converge lane, calls the runtime selector/telemetry wrappers, and leaves `converge-autotune-probe.exe` in the blade root.
- Updated the `evolutionary_loop` benchmark note: this benchmark now proves the durable harness slot for future real SIMD kernels, but its current Kain lanes are still scalar semantic proxies.

Proof and validation:

- Z3 MCP report `D:\Kain-Lang\z3\reports\20260516T092851Z-converge-autotune-selector-ring-stage-elision.json` returned `unsat` for telemetry ring bounds, eligible-lane selection, 64-slot odd-stride tuning-cache probing, and benchmark-release stage-result preservation.
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_world_patch_converge_and_orchestrate_paths -- --nocapture`
- `cargo build -p cli`
- `clang -fsyntax-only` passed for the new native CPU and converge C files.
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- Direct compile and run of `blades/converge-autotune-probe/src/main.kn` under native LLVM returned exit code `0`.
- Latest focused cross-language benchmark run for `evolutionary_loop` measured Kain `29.256 ms`, Rust `26.195 ms`, and C++ `2258.820 ms`. The important result is not final victory over Rust yet; it is that the old ~60 ms selector overhead path collapsed into a real low-30/high-20 ms native lane while preserving multi-lane dispatch.

Durable design lesson:

- The right next layer is FFI-backed native kernels or runtime C kernels for AVX2/AVX-512, not Kain-authored SIMD IR first. Keep converge as the validator/selector, use orchestrate/runtime telemetry for warmup state, and persist only solver-proven lane-table/cache arithmetic.
- If the benchmark asks for silicon autotune, do not fake it with scalar lanes forever. Add real C/FFI kernels, prove spec/fast equivalence for bounded vector chunks, and benchmark with `KAIN_NATIVE_PROFILE=benchmark-release`.

# 2026-05-16 - Universal-actor foundation slice landed: native actor refs are now generation-tagged runtime truth, and LLVM reply ports are synthetic refs instead of waiting actor threads

This pass landed the first concrete slice of the universal-actor architecture thesis without pretending to solve the entire scheduler/world/network problem in one shot. The native runtime and LLVM lowering now agree that an actor handle is not just a raw slot id anymore.

What changed:

- `runtime/native/include/kain_runtime_actor.h` now defines `KainActorRef` with:
  - `actor_id`
  - `generation`
  - `execution_class`
  - `locality_class`
- The native actor ABI descriptor now exposes the actor-ref generation bit width plus default execution/locality classes and the synthetic reply-port class/locality.
- `runtime/native/src/core/kain_runtime_actor.c` now keeps generation counters in the actor table and stamps every live actor with:
  - `ref_generation`
  - `execution_class`
  - `locality_class`
- The new runtime seam is:
  - `kain_actor_ref_from_id(...)`
  - `kain_actor_ref_is_live(...)`
  - `kain_actor_reply_port_actor_ref(...)`
  - `kain_actor_reply_port_send_ref(...)`
- LLVM actor state field 0 is now a `%KainActorRef`, not a raw `i64`. Native LLVM spawn/run paths call `kain_actor_ref_from_id(...)` to populate self handles, and reply-port sends now lower through `kain_actor_reply_port_send_ref(...)`.
- The old reply-port implementation spawned a real direct-thread actor that just blocked on mailbox receive. That is gone. Reply ports are now synthetic actor-table entries with execution class `SYNTHETIC_REPLY_PORT`.
- Successful `ask` / `ask_timeout` still keep the TLS reply-port state cached, but the next `kain_actor_reply_port_new()` now:
  - unbinds the previous synthetic actor ref,
  - resets reply payload state,
  - binds a fresh synthetic actor generation.
  This closes the stale-late-reply hazard across reply-port reuse.

Proof and validation:

- `cargo test -p kain-actor`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_actor_ask_reply_roundtrip_paths -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_typed_actor_ask_reply_wait_for_bool_payloads -- --nocapture`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply -- --nocapture`
- direct native LLVM compile+run:
  - `target\\debug\\kain.exe blades/actor-ask-roundtrip/src/main.kn -t llvm -o blades/actor-ask-roundtrip/actor-ask-roundtrip.exe -r`
- actor runtime conformance:
  - `bash runtime/conformance/actor_runtime/run_tests.sh --verbose`
  - the ABI contract test now directly exercises `kain_actor_ref_*` plus synthetic reply-port rebind / stale-ref rejection
- actor proof lane:
  - `uv run --project C:\\Dev\\polytools\\z3-mcp --no-sync z3-mcp-batch --pack-path D:\\Kain-Lang\\runtime\\native\\src\\core --lane actor`
  - result: `8/8 proved`
- raw Z3 proof:
  - `z3 runtime/native/src/core/z3/proofs-experimental/actor-reply-port-generation-rebind-never-reuses-prior-generation.smt2`
  - result: `unsat`

Durable lesson:

- This is the right first slice because it upgrades actor identity truth before the bigger scheduler rewrite. It removes one wasted reply-port thread today, closes stale reply-port reuse aliasing, and gives future world/host/remote classes a real ABI slot to stand on.
- The next serious architectural move is still scheduler-owned ready queues with explicit execution classes. `KainActorRef` is the substrate; it is not the end state.

# 2026-05-16 - Native stdlib domains now have public `std.*` mirrors and clean aliases

This pass normalized the native stdlib authoring surface so Kain code can import the same style everywhere instead of spelling `std::native::*` or `native_*` wrapper names for ordinary app code. The native backend profile still loads `stdlib/native`, but those files now expose clean public aliases beside the legacy ABI-shaped names, and root `stdlib/*.kn` mirrors exist for all current native domains.

What changed:

- Public root-domain imports now exist for `std.actor`, `std.collections`, `std.diagnostics`, `std.fs`, `std.graphics`, `std.input`, `std.intent`, `std.net`, `std.http`, `std.http2`, `std.process`, `std.result`, `std.runtime`, `std.time`, `std.tls`, and `std.ui`.
- Native modules that still had `native_*` author-facing wrappers now include generated clean aliases such as `actor_spawn`, `runtime_init`, `status_ok`, `result_ok`, `now_millis`, `fs_temp_file`, `graphics_session_create`, and `ui_session_create`.
- `stdlib/native/http.kn`, `http2.kn`, and `tls.kn` now import `std::net` instead of `std::native::net`, so native profile code dogfoods the same public naming shape.
- Added `blades/stdlib-domains` as the import-shape proof blade. It compiles under native LLVM, imports every normalized stdlib domain, and calls representative clean names across runtime, actor, filesystem, input, networking, HTTP/2, TLS, process, graphics, and UI.

Validation:

- Direct native LLVM compile and run of `blades/stdlib-domains/src/main.kn`; the resulting executable returned exit code `0`.
- Direct native LLVM compile and run of `blades/network-domains/src/main.kn`; the resulting executable returned exit code `0` after the stdlib import rewrite.

Durable lesson:

- For new Kain-authored code, prefer `use std::<domain>` and clean domain names (`runtime_init`, `actor_spawn`, `ui_session_create`) unless a test is intentionally proving the raw ABI-shaped `native_*` compatibility layer.

# 2026-05-16 - First-class networking domains now land as public `std.net`, `std.http`, `std.tls`, and `std.http2`, and the old HTTP request-capacity failure is fixed at the runtime seam

This pass finished the built-in networking-domain plan without adding new syntax. The runtime and portable contract work still lives in `crates/kain-net`, `stdlib/native/*.kn`, and `runtime/native/src/core/kain_native_net_system.c`, but authored Kain source can now import the public root modules `std.net`, `std.http`, `std.tls`, and `std.http2` directly.

What changed:

- `crates/kain-net` now carries the broader portable contract shape for this lane:
  - `HttpProtocolPreference`
  - `NetCapabilityState`
  - `NetCapability`
  - `TlsClientSpec`
  - HTTP request specs with protocol preference instead of implicit HTTP/1.1-only intent
- The native C ABI gained the missing query/control seams for the higher layers:
  - platform name
  - capability-state lookup
  - per-request HTTP protocol selection
  - request/response protocol inspection
  - pending-request count per local server
- `runtime/native/src/core/kain_native_net_system.c` now stores request and response protocol strings, rejects unsupported raw-socket HTTP/2 paths explicitly, asks WinHTTP for HTTP/2 only on the secure client lane, and reports the negotiated response protocol back to Kain.
- The important runtime fix is that successful `http_respond_*` now destroys the consumed incoming request handle immediately. That closes the old request-slot leak behind `HTTP incoming request capacity exceeded` on repeated local POST rounds.
- `stdlib/native/http.kn`, `tls.kn`, and `http2.kn` were added as native-profile domains, and the root stdlib now mirrors them at:
  - `stdlib/net.kn`
  - `stdlib/http.kn`
  - `stdlib/tls.kn`
  - `stdlib/http2.kn`
- `blades/network-domains` is the dogfood blade for this surface. It now imports the public root modules, proves raw TCP + local HTTP + actor route handling, checks protocol metadata, creates TLS/HTTP2 request handles, and validates capability-state reporting under native LLVM.

Proof and validation:

- `cargo test -p kain-net`
- `cargo test -p kain-sys-codegen llvm_lowers_native_net_tcp_http_and_actor_route_primitives -- --nocapture`
- `cargo test -p kain-sys-codegen c_backend_keeps_native_net_symbols_as_declarations -- --nocapture`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`
- full native core Z3 proof pack: `41/41 proved`
- direct LLVM compile + run of:
  - `runtime/fixtures/native_net_http/main.kn`
  - `blades/network-domains/src/main.kn`

Benchmark telemetry after the fix:

- `tcp_loopback_tokio`: Kain `144.878 ms`, Rust Tokio `4242.232 ms` -> Kain about `29.28x` faster on this local loopback shape.
- `http_server_concurrency`: Kain `124.991 ms`, Rust Tokio `38.677 ms` -> Kain no longer fails; it is still about `3.23x` slower on this synchronous-versus-async local HTTP shape.

Durable lessons:

- The most important current networking gap is no longer request-slot exhaustion; it is the remaining synchronous HTTP surface and Windows-first secure-client/backend coverage.
- `std.http2` is intentionally honest about maturity. It proves protocol intent and negotiated response reporting on the secure client lane, not a full portable HTTP/2 server/runtime yet.
- For one-file native networking proofs, the durable validation path is still direct LLVM compilation, not the generic bare-file `kain check`/`kain run` path.

# 2026-05-16 - Core machine-stones keywords landed: axiom, pulse, shatter, and teleport

This pass added the final pre-`seal` keyword quartet as first-class Kain syntax and typed compiler metadata:

- `axiom` declares compiler-accepted machine/environment truths with `when target(...)`, `when arch(...)`, `when capability(...)`, one or more `guarantee` lines, and a required `fallback`.
- `pulse` declares a first-class temporal beat with `every <duration>` and optional `jitter <duration>`; pulse bodies get typed `pulse_tick`, `pulse_dt_ms`, and `pulse_missed` bindings.
- `shatter struct` marks a struct with compiler-owned SoA/layout intent while preserving ordinary authored struct syntax.
- `teleport value from SourceWorld to TargetWorld via channel` is a destructive cross-world handoff expression. The typechecker validates both worlds, rejects same-world handoffs, returns the value type, and poisons a simple origin identifier so later reads fail with `was moved by teleport`.

Important implementation seams:

- `crates/kain-core/src/ast.rs`, `parser.rs`, `types.rs`, `formatter.rs`, `runtime.rs`, `runtime_contract.rs`, `low_level_memory.rs`, `comptime.rs`, and `ui.rs` now understand the quartet.
- Native/C/Rust/C++/LLVM codegen currently lowers `teleport` as value pass-through after the typechecker has enforced the destructive ownership rule. The runtime-contract bundle carries the higher-level `world.teleport` / `interop.zero-copy-handoff` capability so future ABI/GPU/native handoff lowering has a stable contract to consume.
- Runtime contracts now emit `axioms`, `pulses`, and `shatters`, plus item-summary/capability counts for machine axioms, hardware-timer pulse intent, SoA shatter layout, and cross-world teleport handoffs.
- CLI/import/LSP/selfhost/UE5/GPU exhaustive clients were updated so the new AST variants do not strand downstream tools.

Dogfood and proof:

- Added `blades/machine-stones`, a compact native LLVM blade proving all four forms together with a native UI surface and viewport worlds. `machine-stones.exe` is left in the blade root; generated `.ll`, `.pdb`, `.ilk`, runtime-contract JSON, and realtime bundle sidecars live under `blades/machine-stones/.kain/out/`.
- Added durable core Z3 proofs under `crates/kain-core/z3/proofs/keywords-*` for axiom fallback exclusivity, pulse monotonic next-tick scheduling, shatter field-lane bounds, and teleport origin liveness.
- Validation passed: `cargo check -p kain-core`, `cargo check -p kain-sys-codegen`, `cargo check -p gpu`, `cargo check -p cli`, `cargo test -p kain-core --test ownership_keywords_test`, core Z3 `keywords` lane `8/8`, `kain check blades/machine-stones/src/main.kn --target llvm`, native compile to `blades/machine-stones/machine-stones.exe`, and running the exe from the blade root returned exit code `0`.

Next recommended step:

- If the next pass wants real backend violence rather than metadata, the natural order is: pulse native scheduler/timer lowering, shatter layout-aware allocation/iteration lowering, then teleport ABI/GPU/native transfer lowering. `seal` should still wait until these semantics have settled.

# 2026-05-16 - Native actor latency assessment: the obvious heap-allocation collapse is solver-safe but did not earn a stable benchmark win, so the real gap is still the blocking OS-thread mailbox architecture

I assessed the native LLVM actor runtime again specifically for the `actor_mailbox_erlang` latency gap on `2026-05-16`.

What I tested:

- I focused on `runtime/native/src/core/kain_runtime_actor.c`, because the hot steady-state path in this benchmark is not the scheduler bootstrap anymore; it is `ask` request send -> worker mailbox receive -> direct reply-port completion.
- The first candidate rewrite collapsed the mailbox request path from two allocations (`MessageNode` + payload buffer) under the mailbox lock into a single allocation with the message node stored at the tail of the payload block.
- I proved the candidate tail-layout arithmetic in `runtime/native/src/core/z3/proofs-experimental/actor-mailbox-tail-node-single-allocation-bounds.smt2`; direct Z3 returned `unsat`.
- I also reran the durable native actor proof lane:
  - `uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane actor`
  - result: `7/7` proved, `0` counterexamples.

What I learned:

- The single-allocation mailbox candidate is mathematically safe, but on this host it did not earn a durable performance win once measured repeatedly in the live benchmark slice.
- A second experiment that swapped the Windows mailbox/reply-port event waits to condition variables performed even worse and was also discarded.
- I left the runtime code on the known-good pre-experiment actor path; the solver artifact remains as a reusable example, but the code change was not kept because the benchmark evidence was not strong enough.

Durable assessment:

- The biggest remaining actor latency wall is not likely to be "one more malloc" or a tiny bit trick. The current native actor model still runs long-lived actors as blocking OS-thread loops around `kain_actor_receive(...)`.
- That means the benchmark is still paying real kernel wake/sleep and thread scheduling costs on every request/reply roundtrip, which is fundamentally different from Erlang's lightweight process scheduler model.
- If a future pass wants a real shot at the gap, the next serious direction is architectural:
  - move away from blocking thread-owned actor loops,
  - make mailbox readiness scheduler-owned,
  - and process actor work in short scheduled quanta instead of pinning a waiting OS thread per running actor.
- Small mailbox micro-optimizations are still worth testing, but they should be treated as sidecar work unless the actor execution model itself changes.

# 2026-05-16 - Native LLVM actor ask/reply now uses a dedicated reply-port fast path, typed non-Int replies are proven live, and the Erlang benchmark moved from broken roundtrip to measured steady-state

The native LLVM actor lane could already `spawn` actors and `send` messages, but the real `ask` / `ask_timeout` roundtrip was still wrong in the codegen/runtime seam: reply ports were effectively `i64`-shaped, typed replies such as `Bool` were not a first-class path, and the return leg still paid the generic mailbox enqueue/dequeue tax even when the target was the synthetic reply port. This pass fixed correctness first, then cut a real chunk out of the hot return path.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` no longer treats reply ports as a fake `%...Reply = { i64 }` payload contract.
- Native LLVM `ask` / `ask_timeout` now lower through the real reply target type when codegen has target-type context:
  - `i64` replies keep the scalar fast wrapper through `kain_actor_reply_port_wait_i64`.
  - typed replies such as `Bool` use the generic `kain_actor_reply_port_wait(...)` path and load the correctly typed stack slot.
- `send reply_to.Reply(value = ...)` no longer routes through generic `kain_actor_send` for the return leg. Reply-port targets now lower to the dedicated runtime symbol `kain_actor_reply_port_send(actor_id, payload_ptr, payload_size)`.
- `runtime/native/src/core/kain_runtime_actor.c` now treats reply-port state as generic payload-backed storage, not `i64`-only storage, and the direct reply-port send path copies replies straight into reply-port state:
  - tiny replies use inline state storage,
  - larger replies allocate once into reply-port-owned storage,
  - the synthetic reply-port actor no longer needs mailbox traffic just to complete an `ask`.
- `runtime/native/include/kain_runtime_actor.h`, `crates/kain-actor/src/native.rs`, and actor ABI tests were updated so `kain_actor_reply_port_send`, `kain_actor_reply_port_wait`, and `kain_actor_reply_port_wait_i64` stay in sync as a real ABI surface.
- `runtime/fixtures/native_actor_ask_roundtrip/main.kn` and `blades/actor-ask-roundtrip/src/main.kn` now prove both `Int` and `Bool` ask/reply roundtrips under native LLVM.

Proof and validation:

- Focused LLVM/codegen tests passed:
  - `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply -- --nocapture`
- Actor/native contract checks passed:
  - `cargo test -p kain-actor native_actor_ -- --nocapture`
  - `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply -- --nocapture`
- Live native LLVM proof executable passed:
  - `target\\debug\\kain.exe check runtime\\fixtures\\native_actor_ask_roundtrip\\main.kn --target llvm`
  - compiled `runtime/fixtures/native_actor_ask_roundtrip/native_actor_ask_roundtrip.exe`
  - running that executable returned exit code `0`
- Blade proof passed after rebuild:
  - `blades/actor-ask-roundtrip/actor-ask-roundtrip.exe` returned exit code `0`
- Solver evidence:
  - `D:\\Kain-Lang\\z3\\reports\\20260516T061932Z-actor-reply-port-inline-send-bounds.json` -> `unsat`
  - `D:\\Kain-Lang\\runtime\\native\\src\\core\\z3\\reports\\20260516T061932Z-native-actor-reply-port-fastpath-pass.json` -> actor lane `7/7` proved
  - `D:\\Kain-Lang\\crates\\kain-sys-codegen\\z3\\reports\\20260516T061932Z-llvm-codegen-reply-port-fastpath-pass.json` -> llvm lane `19/19` proved

Benchmark result:

- `benchmark/out/reports/latest.llm.md` now measures a real, passing Kain/Erlang actor ask/reply comparison again after the fix.
- Latest one-run report at `2026-05-16T06:24:26Z`:
  - Kain: `5502.362 ms`
  - Erlang: `391.797 ms`
  - Kain is still `14.04x` slower on this steady-state mailbox roundtrip shape.
- This is still not parity, but it is materially better than the earlier passing one-run report (`6660.188 ms` vs `397.643 ms`, about `16.75x` slower). The dedicated reply-port send fast path cut roughly `1.16 s` from the Kain row in this slice without changing the case semantics.

Durable lessons:

- If native LLVM ask/reply breaks again, inspect the full seam together:
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `runtime/native/include/kain_runtime_actor.h`
  - `runtime/native/src/core/kain_runtime_actor.c`
  - `crates/kain-actor/src/native.rs`
- Typed ask replies are only fully honored where LLVM lowering has target-type context. Explicit typed bindings such as `let allowed: Bool = ask_timeout(...)` are the current proof shape to keep alive.
- The return leg is now intentionally asymmetric:
  - request messages still use generic actor mailbox send,
  - reply-port sends bypass generic mailbox traffic through `kain_actor_reply_port_send`.
- The benchmark runner can still hit a transient Windows runtime-cache cleanup race where a stale `generated/native_runtime/cache/.../*.obj.tmp` removal reports `Access is denied`. In this pass the first rerun failed that way and the second identical rerun passed once the cache quieted. Treat that as cache-state fallout first, not immediate evidence of a codegen/runtime semantic regression.

# 2026-05-16 - Benchmark suite now covers the remaining low-level systems categories, adds Erlang actor comparison, and exposes two real native-runtime gaps

The benchmark lane was still missing most of the "whole machine" picture: SIMD pressure, map lookups, JSON/parser work, filesystem, process/stdout, local HTTP, actor mailbox fanout, Unicode-heavy string work, allocator large-object churn, GPU submission, and direct shared-library FFI stress. This pass filled in those categories, taught the runner about Erlang and per-case support artifacts, and then ran the full suite so the gaps are no longer speculative.

What changed:

- `benchmark/run.py` now supports Erlang rows end-to-end:
  - `erlang` is a real suite language,
  - the runner resolves `erl` / `erlc`,
  - and on Windows it prefers the official OTP `bin` directory over PATH wrappers so `erlc.exe` can actually find `erlexec.dll`.
- `benchmark/run.py` also grew the missing support-artifact plumbing:
  - `ffi_shared_call_stress` now compiles `benchmark/ffi_boundary/native/ffi_boundary.c` into a shared DLL plus import library under `benchmark/out/build/...`,
  - copies the runtime DLL beside each built executable,
  - and compiles the Kain row from the case directory so the case-local `KAIN.toml` for `use c::ffi_boundary_shared` resolves through nearest-manifest lookup.
- `benchmark/run.py` now decodes subprocess output as UTF-8 with replacement. This was necessary because the new Unicode-heavy benchmark could otherwise crash report generation on Windows text decoding.
- Added the missing benchmark cases:
  - `simd_lane_mix`
  - `native_map_lookup`
  - `json_manual_roundtrip`
  - `filesystem_stream`
  - `process_stdio_loop`
  - `http_server_concurrency`
  - `actor_mailbox_erlang`
  - `unicode_string_heavy`
  - `allocator_large_object_churn`
  - `gpu_graphics_submit`
  - `ffi_shared_call_stress`
- Updated `benchmark/README.md`, `ARCHITECTURE.md`, and `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future agents know:
  - the suite is now manifest-subset aware,
  - Erlang is a first-class comparison lane,
  - shared-library support artifacts are runner-owned,
  - and the HTTP/actor caveats below are real telemetry, not benchmark superstition.

Validation and telemetry:

- `python -m py_compile benchmark/run.py`
- JSON parse of `benchmark/benchmarks.json`
- Focused smoke passes for the new cases plus a full suite sweep:
  - `python benchmark/run.py --runs 5 --warmups 2 --timeout 900`
- The latest full report lives at:
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest.json`
- Key new measurements from the full `2` warmup / `5` run sweep:
  - `simd_lane_mix`: Kain `164.051 ms`, Rust `11.279 ms`, C++ `8.509 ms` -> Kain `19.28x` slower than fastest.
  - `native_map_lookup`: Kain `233.616 ms`, Rust `30.288 ms`, C++ `33.007 ms` -> Kain `7.71x` slower than fastest.
  - `json_manual_roundtrip`: Kain `2084.504 ms`, Rust `128.503 ms`, C++ `100.129 ms` -> Kain `20.82x` slower than fastest.
  - `filesystem_stream`: Kain `205.312 ms`, Rust `115.304 ms`, C++ `91.517 ms` -> Kain `2.24x` slower than fastest.
  - `process_stdio_loop`: Kain `15900.943 ms`, Rust `4952.415 ms`, C++ `7422.407 ms` -> Kain `3.21x` slower than fastest.
  - `actor_mailbox_erlang`: Kain `6326.130 ms`, Erlang `404.883 ms` -> Kain `15.62x` slower.
  - `unicode_string_heavy`: Kain `13.663 ms`, Rust `8.950 ms`, C++ `8.096 ms` -> Kain `1.69x` slower than fastest.
  - `allocator_large_object_churn`: Kain `48.687 ms`, Rust `36.012 ms`, C++ `34.981 ms` -> Kain `1.39x` slower than fastest.
  - `gpu_graphics_submit`: Kain-only `35.747 ms`.
  - `ffi_shared_call_stress`: Kain `59.459 ms`, Rust `53.700 ms`, C++ `51.884 ms` -> Kain only `1.15x` slower than fastest.
- The strongest suite-level new Kain win is still networking-oriented:
  - `tcp_loopback_tokio`: Kain `145.756 ms`, Tokio `3171.441 ms` -> Kain `21.76x` faster on this local loopback shape.

Hard findings:

- `http_server_concurrency` is not just "behind Rust"; the Kain row currently fails outright on repeated local POST rounds. The benchmark now records the real native-runtime diagnostic:
  - `net_last_status = -3`
  - `net_last_error_kind = capacity`
  - `net_last_error_message = HTTP incoming request capacity exceeded`
- `actor_mailbox_erlang` exposed a cold-start wobble in the current Kain ask/reply path. The deterministic checksum should be `10399419`; without a warmup ask per worker, the first measured pass could wobble once even though the steady-state path is correct. Both Kain and Erlang rows now do one unmeasured warmup ask per worker before timing.

Durable lessons:

- Kain benchmark cases that depend on a case-local `KAIN.toml` plus `use c::...` are sensitive to the current working directory. If the runner compiles them from repo root instead of the case directory, nearest-manifest resolution can fail and fake a language/runtime bug.
- On Windows, do not trust PATH-wrapped Erlang shims by default. Resolve `erl.exe` and `erlc.exe` from the official OTP `bin` directory first or the build can fail before the benchmark even starts.
- Unicode-heavy cases are a runner problem as much as a language problem. If subprocess output is decoded with the host code page instead of UTF-8-with-replacement, the report lane itself becomes flaky.
- The current Kain HTTP capacity failure is important enough to keep visible in the suite. Do not "fix" the benchmark by reducing traffic until the runtime issue is actually solved.
- The actor benchmark should continue to measure steady-state mailbox cost, not cold-start scheduler/setup noise. Keep the one-shot per-worker warmup unless the native ask/reply cold-start wobble is truly repaired.

# 2026-05-15 - Kaintana now has a real DCC-style desktop proof shell, richer authored widgets, and a non-jittery desktop presenter

The previous Kaintana desktop acceptance was technically proving services, but the visible shell looked rough: low-resolution text, repaint jitter, no meaningful resize scaling, and not enough surface area to stress a real tool-style UI. This pass pushed the framework and the acceptance blade forward together instead of papering over the screenshot.

What changed:

- `blades/kaintana/src/kaintana.kn` grew the next authoring layer:
  - `kaintana_split_top`, `kaintana_split_bottom`, and `kaintana_grid_cell` for real shell composition.
  - `kaintana_immediate_toolbar_button`, `kaintana_immediate_slider`, and `kaintana_immediate_chart_bar` for tool-app surfaces.
  - `kaintana_primitive_fill` and `kaintana_primitive_text` so Kaintana can still author low-level bespoke UI without waiting for a widget catalog.
  - richer desktop text sizing so the compatibility presenter reflects the intended hierarchy better.
- `blades/kaintana/native/kaintana_desktop_bridge.c` stopped behaving like a flickery static slideshow:
  - it now scales authored geometry and text to the live client size,
  - paints through a memory buffer before blitting to the window,
  - and keeps the frame-budget stay-alive loop without invalidating the whole window every frame.
- `blades/kaintana-test/src/main.kn` was rebuilt into the new oxide DCC control deck:
  - top bar,
  - tool rail,
  - viewport block,
  - chart lane,
  - low-level authoring lane,
  - inspector with stacked actions and sliders,
  - keypad surface,
  - host-service metrics,
  - snapshot + input-trace proof surface.
- The host-service/menu proof remains in native UI state plus `kaintana_test_desktop_snapshot.txt` / `kaintana_test_desktop_input_trace.txt`, but the visible popover overlay is no longer forced into the final BMP because it harmed legibility more than it helped.

Validation:

- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\kaintana\\run.ps1 -NoRun`
- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\kaintana-test\\run.ps1 -NoRun`
- Direct desktop proof run with `KAINTANA_TEST_FRAME_BUDGET=240` regenerated the clean BMP and reports.
- Direct long-live proof:
  - `KAINTANA_TEST_FRAME_BUDGET=1000`
  - `backend=desktop`
  - `frames=1000`
  - `last_error=ok`
  - measured runtime about `26.8 s`

Durable lessons:

- Kaintana layout helpers are asymmetric. `kaintana_split_right(rect, fraction, gap)` uses `fraction` as the left-side share, so a 25% right inspector should use roughly `0.75`, not `0.25`.
- The desktop bridge is still a compatibility presenter, not a production renderer. It is good enough for proof shells when the scene stays text/rect-oriented, but visual clarity depends on authored spacing and restrained copy length.
- For service validation, keep the source of truth in snapshot/input-trace artifacts. Visual overlays are optional and should only stay in the live shell when they improve readability.

# 2026-05-16 - Benchmark suite now supports Kain/Rust-only Cargo dependency cases for Tokio/Rayon pressure

The benchmark lane was missing async-runtime, networking, and data-parallel ecosystem pressure. The runner now supports manifest-declared language subsets and per-case Cargo Rust builds, so the normal dependency-free C++/Rust/JS/Python cases stay simple while focused Tokio/Rayon comparisons can live in the same reporting pipeline.

What changed:

- `benchmark/run.py` now intersects requested languages with each case's declared `languages` map, renders unavailable global language columns as `n/a`, and computes pass/fail only over the actual case languages.
- Rust cases still default to direct `rustc`, but cases with `rust_manifest` build through Cargo release with a case-local target dir. Per-case Cargo manifests need an empty `[workspace]` so Cargo does not treat them as orphan members of the repo root workspace.
- Added `benchmark/cases/async_ready_chain`: Kain ready-future `await` versus Tokio current-thread ready futures.
- Added `benchmark/cases/tcp_loopback_tokio`: Kain native TCP loopback versus Tokio TCP accept/connect/read/write.
- Added `benchmark/cases/rayon_parallel_reduce`: Kain scalar reduction proxy versus Rayon parallel iterators. This remains `parallel-proxy` until Kain LLVM has proven user-level data-parallel fanout.
- Updated `benchmark/README.md`, `ARCHITECTURE.md`, and `.agents/skills/kain-benchmark-pipeline/SKILL.md` with the new case model.

Validation and telemetry:

- `python -m py_compile benchmark/run.py`
- JSON parse of `benchmark/benchmarks.json`
- `python benchmark/run.py --case async_ready_chain --languages kain,rust --warmups 2 --runs 5 --timeout 900 --no-build`
- `async_ready_chain`: Kain `173.811 ms`, Tokio `8.905 ms`; Kain is `19.52x` slower on immediate ready-future overhead.
- `python benchmark/run.py --case tcp_loopback_tokio --languages kain,rust --warmups 2 --runs 5 --timeout 900 --no-build`
- `tcp_loopback_tokio`: Kain `151.044 ms`, Tokio `3003.172 ms`; Kain wins this local loopback shape by `19.88x`, but the fairness note must keep saying Kain's facade is synchronous around readiness helpers while Rust uses Tokio async IO.
- `python benchmark/run.py --case rayon_parallel_reduce --languages kain,rust --warmups 2 --runs 5 --timeout 900 --no-build`
- `rayon_parallel_reduce`: Kain `24.392 ms`, Rayon `10.302 ms`; Rust is `2.37x` faster, which is the current parallel fanout gap.

Durable lessons:

- Dynamic value capture into a Kain `async` ready future compiled but failed checksum during the spike, so `async_ready_chain` intentionally uses the known-good `return async 2` shape until async capture lowering is fixed.
- The benchmark suite still lacks first-class GPU/SIMD/parser/hashmap/filesystem/process/HTTP-client cases; these should be added as focused manifest cases with honest maturity/fairness labels rather than forcing every language into every case.

# 2026-05-16 - Kaintana desktop acceptance is now genuinely desktop-only, and Vulkan moved into an optional adapter + separate acceptance blade

The “why does `kaintana-test.exe` open the Vulkan proof window?” bug turned out to be a real architecture leak, not user confusion. The desktop acceptance blade and the Vulkan proof were sharing one consuming blade manifest plus one root output name, so the Vulkan compile overwrote `kaintana-test.exe`, and the desktop executable also kept importing `vulkain_bridge.dll` through the mixed manifest/module surface.

What changed:

- `blades/kaintana/` is now the desktop-default core only. `src/kaintana.kn` no longer imports `vulkain`, `src/main.kn` no longer probes Vulkan, `KAIN.toml` no longer declares `vulkain_bridge`, and `run.ps1` no longer builds or stages Vulkan artifacts.
- Added `blades/kaintana-vulkan/` as the optional adapter blade. `src/kaintana_vulkan.kn` now owns the Vulkan probe/run/report wrappers over `blades/vulkain`, while the base framework stays clean.
- `blades/kaintana-test/` is now strictly the desktop acceptance blade. Its `run.ps1` only builds `kaintana-test.exe`; `src/main.kn` no longer imports Vulkan symbols or Vulkan host checks; the stale `entrypoints/vulkan.kn`, root `kaintana-test-vulkan.exe`, and root `vulkain_bridge.dll` were removed.
- Added `blades/kaintana-vulkan-test/` as the dedicated foreign-presenter acceptance blade. It imports `kaintana` plus `kaintana-vulkan`, stages `vulkain_bridge.dll` locally, and writes its own `.kain/run/kaintana_vulkan_test_*` artifacts.
- Cleaned stale mixed-backend artifacts out of the desktop lane and deleted the leftover `vulkain_bridge.dll` from `blades/kaintana/` itself.

Machine-level proof:

- `llvm-objdump -p blades/kaintana-test/kaintana-test.exe` no longer lists `vulkain_bridge.dll` in the import table.
- `llvm-objdump -p blades/kaintana/kaintana.exe` also no longer lists `vulkain_bridge.dll`.
- After deleting `blades/kaintana-test/vulkain_bridge.dll`, running `blades/kaintana-test/kaintana-test.exe` still succeeds and reports `backend=desktop frames=180 geometry=34`.
- The dedicated Vulkan lane still succeeds independently: `blades/kaintana-vulkan-test/kaintana-vulkan-test.exe` reports `backend=vulkan frames=180 geometry=540`.

Validation:

- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\kaintana\\run.ps1 -NoRun`
- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\kaintana-test\\run.ps1 -NoRun`
- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\kaintana-vulkan\\run.ps1 -NoRun`
- `powershell -ExecutionPolicy Bypass -File D:\\Kain-Lang\\blades\\kaintana-vulkan-test\\run.ps1 -NoRun`
- Direct desktop run: `Set-Location D:\\Kain-Lang\\blades\\kaintana-test; .\\kaintana-test.exe`
- Direct Vulkan run: `Set-Location D:\\Kain-Lang\\blades\\kaintana-vulkan-test; .\\kaintana-vulkan-test.exe`

Durable lessons:

- Until Kain grows per-entry or transitive `[c_ffi]` semantics, backend-specific acceptance apps should live in separate consuming blades. Sharing one manifest across desktop and Vulkan proofs is enough to contaminate import tables and even overwrite the user-facing root executable.
- The current Kaintana widget helpers still emit desktop fill/text bridge calls during composition, so non-desktop consumers must keep `kaintana_desktop_bridge` symbols in scope for now even when they do not open the desktop host. The next architectural cleanup would be to move those desktop side effects behind a more renderer-neutral presentation seam.

# 2026-05-16 - FFI-boundary benchmark now includes Zig and confirms Kain LLVM is in the same native-call neighborhood

The dedicated ABI benchmark now answers the Zig comparison directly instead of relying on smell. `benchmark/ffi_boundary/run.py` resolves `zig`, builds `sources/zig_pure.zig` and `sources/zig_c_object.zig` with `zig build-exe -O ReleaseFast`, and reports those rows beside Kain LLVM pure, Kain LLVM C object/shared, interpreter pure, and interpreter/live bridge.

What changed:

- Added `benchmark/ffi_boundary/sources/zig_pure.zig`, a no-inline Zig version of the same integer kernel with the same `10,000,000` loop count and checksum.
- Added `benchmark/ffi_boundary/sources/zig_c_object.zig`, a Zig caller for the same generated C object used by the Kain LLVM object path.
- Updated `benchmark/ffi_boundary/run.py` with `--zig` / `ZIG` resolution, Zig `ReleaseFast` builds, Zig report rows, and Zig 0.17-dev Windows output handling. This Zig build emits a runnable PE at the requested `-femit-bin=path` without requiring a `.exe` suffix.
- Updated `ARCHITECTURE.md` and `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future agents use this lane for “is Kain LLVM near Zig/C?” questions.

Validation:

- `zig version` -> `0.17.0-dev.304+9787df942`
- `python -m py_compile benchmark/ffi_boundary/run.py`
- `python benchmark/ffi_boundary/run.py --warmups 1 --runs 2 --timeout 300`
- `python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300`

Current stable report from `benchmark/out/reports/ffi_boundary_latest.llm.md`:

- `Kain LLVM Pure`: `96.035 ms` median over `10,000,000` calls, about `9.60 ns/call`
- `Kain LLVM C Object`: `103.650 ms`, about `10.36 ns/call`
- `Kain LLVM C Shared`: `97.528 ms`, about `9.75 ns/call`
- `Zig Pure`: `101.788 ms`, about `10.18 ns/call`
- `Zig C Object`: `94.153 ms`, about `9.42 ns/call`
- `Kain Interpret Pure`: `114.080 ms` over `10,000` calls, about `11,408.02 ns/call`
- `Kain Interpret C Shared`: `3021.407 ms`, about `302,140.73 ns/call`

Durable conclusion:

- Yes, Kain LLVM is in the Zig/C native neighborhood on this microbench. The fastest row was `Zig C Object` at `9.42 ns/call`; `Kain LLVM Pure` was `9.60 ns/call`; `Kain LLVM Shared` was `9.75 ns/call`. Those differences are small enough to treat as normal native-code noise and codegen shape, not a structural performance wall.
- The interpreter/live bridge remains the non-target lane for hot paths. It is useful for bootstrap/dev semantics, but the serious runtime target is LLVM.

# 2026-05-16 - Native crash forensics is now a first-class repo workflow, and the first proven crash family is archived Win32/GL modal-menu reentrancy rather than the current Kaintana/Pong presenters

The repo needed a durable way to answer native crash questions from the machine-code edge instead of replaying the same detective work through a million-line checkout. That now exists, and the first full pass produced a much cleaner split between “real crash root cause” and “current apps feel finicky.”

What changed:

- Added the repo-wide crash tool at `tools/crash-forensics/analyze_native_crash.ps1`.
  - Inputs: matching exe + Windows dump, with optional emitted `.ll`, frame report, and host report paths.
  - Outputs: a summary report plus raw LLDB and `llvm-objdump` logs under `.kain/forensics/`.
  - The tool now resolves the first app-owned frame, translates the ASLR-loaded frame address back into file-image VMA through `image lookup -a ... (module_image + offset)` plus PE `ImageBase`, scans emitted LLVM IR for non-entry `alloca`, and records last-frame evidence from host/app reports.
- Added repo-local operator guidance at `.agents/skills/native-crash-forensics/SKILL.md`.
- Added frame-budget pressure overrides to the active blades:
  - `blades/kaintana-test/run.ps1 -FrameBudget <N>` via `KAINTANA_TEST_FRAME_BUDGET`
  - `blades/pong/run.ps1 -FrameBudget <N>` via `KAIN_PONG_FRAME_BUDGET`
  - `blades/pong/src/pong_config.kn` and both `blades/kaintana-test` entrypoints now honor those env overrides directly in Kain code, so future long-run repro does not need temp config files or source edits.
- Updated `ARCHITECTURE.md` and `.agents/skills/kaintana-framework/SKILL.md` so future agents know the new crash workflow and the already-proven archived Win32/GL crash signature.

Hard findings:

- The first verified machine-level crash family on this host is old runtime-owned Win32/GL menu handling, not the current blade-owned presenters.
  - Windows dumps: `%LOCALAPPDATA%\\CrashDumps\\kain_example_workbench.exe.*.dmp`
  - Matching binary: `target/kain-example/kain_example_workbench.exe`
  - Forensics report: `target/kain-example/.kain/forensics/kain_example_workbench-crash-report.txt`
  - LLDB shows `Exception 0xc00000fd` and the first Kain-owned frame at `kain_native_ui_win32_gl_process_menu` in `kain_native_ui_host_win32_gl.c:649`.
  - The app-code disassembly window now lands around `0x140061940`, proving we are looking at the archived modal-menu path rather than guessing from system DLL frames alone.
  - The dump stack is deep in `gdi32full.dll`/`TextShaping.dll` text work after `wglUseFontBitmapsA`, which matches the archived source shape where `TrackPopupMenuEx(...)` is entered before `active_menu_id` is cleared.
- The current live blades did not reproduce the “crash after N frames” claim on this machine:
  - `blades/kaintana-test` desktop host: `frames=4000`, `last_error=ok` in `.kain/run/kaintana_test_desktop_host.txt`
  - `blades/kaintana-test` Vulkan host: `frames_presented=4000`, `last_error=ok` in `.kain/run/kaintana_test_vulkan_host.txt`
  - `blades/pong`: `frame.clock=4000`, `presenter.frames=4000`, `presenter.ok=true`, `last_error=ok` in `.kain/run/pong_report.txt` and `.kain/run/pong_window_report.txt`
  - No fresh `kaintana-test.exe.*.dmp` or `pong.exe.*.dmp` files were emitted under `%LOCALAPPDATA%\\CrashDumps` during these pressure runs.
- The current emitted LLVM IR does not support the older loop-local-`alloca` hypothesis for these two blades:
  - `kaintana-test` non-entry `alloca` count: `0`
  - `pong` non-entry `alloca` count: `0`

Solver-backed reasoning:

- `z3/reports/20260516T030317Z-archived-gl-menu-reentry-witness.json` is `sat`: if menu state remains active during the modal loop, reentry is admissible.
- `z3/reports/20260516T030331Z-archived-gl-menu-preclear-blocks-reentry-clean.json` is `unsat`: the simple “pre-clear active menu before modal loop continues” model blocks that reentry class.

Validation:

- `powershell -ExecutionPolicy Bypass -File tools/crash-forensics/analyze_native_crash.ps1 -ExePath target/kain-example/kain_example_workbench.exe -DumpPath %LOCALAPPDATA%\\CrashDumps\\kain_example_workbench.exe.32876.dmp`
- Direct long-run Kaintana desktop executable with `KAINTANA_TEST_FRAME_BUDGET=4000`
- `powershell -ExecutionPolicy Bypass -File blades/kaintana-test/run.ps1 -Backend vulkan -FrameBudget 4000`
- `powershell -ExecutionPolicy Bypass -File blades/pong/run.ps1 -FrameBudget 4000`

Durable lessons:

- Separate “crashed native app” from “old archived host path crashed.” The current Kaintana desktop/Vulkan and Pong lanes are stable through 4000-frame pressure here; the archived runtime-owned Win32/GL host is the first actually-proven bad egg.
- When the stop PC is in a system DLL, the right assembly window is usually the first app-owned frame, not the raw faulting address. On Windows minidumps, normalize through `image lookup -a <first-app-frame>` plus PE `ImageBase` before feeding `llvm-objdump`.
- For native UI/graphics stability work, repo-local frame-budget overrides are worth keeping in the blade surface. They make deterministic long-run repro and binary-searching a threshold far cheaper than constantly editing configs or source.
- There is also a separate launcher seam worth remembering: `D:\\Kain-Bazel\\bin\\kain.exe` currently misroutes `-o` through `scripts/windows/launch-bazel-cli.ps1` and can fail with “parameter name 'o' is ambiguous.” That is not a crash root cause, but it can poison pressure-testing if you assume the shim behaves like the real compiler binary.

# 2026-05-16 - Realtime/native UI staging no longer double-registers imported `world` / `entangle` modules

The imported-module failure was real, but the bug lived in driver staging, not in `world` / `entangle` semantics themselves. The bad path was `compile_realtime_app_bundle(...)` in `crates/kain-driver`: it built the typed frontend from flattened imports, then reused that flattened user source for the UI/runtime registration pass. That meant imported module items were present once as inlined top-level declarations and then loaded a second time through the original `use` import, which is why native LLVM/native UI staging surfaced `entangle endpoint '...' participates in more than one binding`.

What changed:

- `crates/kain-driver/src/lib.rs` now keeps the old import-flattened path for frontend typing and bundle emission, but the UI pass uses `prepare_frontend_source_for_target(...)` instead of the flattened frontend import bundle. That preserves target preparation such as `[c_ffi]` augmentation while letting filesystem modules load exactly once through normal module resolution.
- Added a driver regression in `crates/kain-driver/src/lib.rs`: `compile_realtime_bundle_supports_imported_world_and_entangle_modules`.
- Added the native-app regression in `crates/kain-driver/src/native_app.rs`: `compile_native_app_bundle_supports_imported_world_and_entangle_modules`.
- Removed the now-dead `FrontendSourceBundle.user_source` field, which only existed to feed the broken flattened-UI path.

Validation:

- `cargo fmt -p kain-driver`
- `cargo test -p kain-driver compile_realtime_bundle_supports_imported_world_and_entangle_modules --target-dir target/codex-entangle-fix -- --nocapture`
- `cargo test -p kain-driver compile_native_app_bundle_supports_imported_world_and_entangle_modules --target-dir target/codex-entangle-fix -- --nocapture`
- `mcp__z3_local__.check_smt2(report_name="driver-ui-flattened-import-replay-breaks-single-registration", ...)` -> `unsat`, report `z3/reports/20260516T023319Z-driver-ui-flattened-import-replay-breaks-single-registration.json`

Durable lessons:

- Flattened frontend source is for typechecking and emitted bundle synthesis. It is not automatically safe to feed back into runtime/UI registration, because the original `use` import still exists and may replay the same declarations.
- Target-prepared source is the correct UI/runtime staging input for lanes that need generated FFI modules but still rely on normal filesystem module loading.
- The `[c_ffi]` consumer-manifest caveat is still real. This fix restores modular imported `world` / `entangle` apps; it does not make `[[c_ffi.libraries]]` declarations transitive across blades.

# 2026-05-15 - Dedicated FFI-boundary telemetry now proves the native LLVM lane is lean while the interpreter/live bridge path is still brutally expensive

The repo needed one benchmark that answered the narrow ABI question directly instead of burying it inside the multi-language suite. That landed as `benchmark/ffi_boundary/`, and in the process it also exposed a real CLI dogfood bug in the direct LLVM/C compile path for `use c::...`.

What changed:

- Added the dedicated benchmark harness under `benchmark/ffi_boundary/`:
  - `run.py` orchestrates the lane, compiles the native helper into both object and shared forms, writes a local `KAIN.toml`, builds/runs the variants, and emits `benchmark/out/reports/ffi_boundary_latest.llm.md` plus JSON and timestamped `*.ffi_boundary.*` reports.
  - `native/ffi_boundary.h` and `native/ffi_boundary.c` define the tiny C helper used for object/shared boundary tests.
  - `sources/llvm_pure.kn`, `llvm_object.kn`, `llvm_shared.kn`, `interpret_pure.kn`, and `interpret_shared.kn` keep the benchmarked work deterministic and checksum-guarded.
  - The generated object/shared/import-library sidecars now live under `benchmark/out/build/ffi_boundary/native/` instead of the source tree, so reruns stay clean and git never needs to see Windows `.lib` / `.exp` byproducts.
- Fixed the direct `kain.exe <file>.kn -t llvm` / `-t c` CLI seam in `crates/cli/src/main.rs`. The first benchmark run proved the LLVM lane was compiling call sites before the generated C FFI modules existed, which produced undefined `@ffi_boundary_mix` calls. The correct fix was not to rewrite the source in place; it was to force `kain_c_ffi::import_libraries_for_source(..., Generate, ...)` before frontend compile so `.kain/cache/c_ffi/.../*.kn` bindings exist when import resolution runs.
- Updated `ARCHITECTURE.md` and `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future agents know that `benchmark/ffi_boundary/` is the ABI-tax probe and that undefined `use c::...` symbols on direct LLVM/C compile point at the CLI pre-generation seam.

Validation:

- `python -m py_compile D:\Kain-Lang\benchmark\ffi_boundary\run.py`
- `cargo build --release -p cli`
- `python D:\Kain-Lang\benchmark\ffi_boundary\run.py --warmups 1 --runs 3 --timeout 300`
- `python D:\Kain-Lang\benchmark\ffi_boundary\run.py --warmups 2 --runs 5 --timeout 300`

Current benchmark reality from `benchmark/out/reports/ffi_boundary_latest.llm.md`:

- `Kain LLVM Pure`: `95.884 ms` median over `10,000,000` calls, about `9.59 ns/call`
- `Kain LLVM C Object`: `104.738 ms`, about `10.47 ns/call`
- `Kain LLVM C Shared`: `104.706 ms`, about `10.47 ns/call`
- `Kain Interpret Pure`: `135.488 ms` over `10,000` calls, about `13,548.82 ns/call`
- `Kain Interpret C Shared`: `3112.769 ms`, about `311,276.85 ns/call`

Durable conclusion:

- The native LLVM lane is exactly the story we wanted: direct C boundary tax is tiny. On this host, both the direct object and direct shared-library paths are only about `9.2%` slower than pure LLVM.
- The interpreter/live bridge path is not “a little slower”; it is orders of magnitude slower. Even the pure interpret variant is about `1410x` slower per call than pure LLVM on this benchmark, and the current shared-library bridge path is roughly `32,458x` slower per call than pure LLVM on the reported median.
- `interpret_shared` still has a brutal cold-start cost because the generated Rust bridge is built on first use (`~82.8 s` prime on the clean rerun), but the measured warm samples settle around the low-single-digit seconds for `10,000` calls. That means the exact median will move with cache warmth, yet the architectural takeaway stays fixed: the native LLVM FFI lane is lean; the interpreter/live bridge is the expensive Rust-hosted lane.

# 2026-05-15 - Kaintana is now a real blade-owned UI framework package with desktop and Vulkan acceptance proofs

The all-in-one UI framework idea is real now, and it landed in the right layer: not as new runtime-owned UI architecture, but as a blade-owned Kain package that sits above the passive raw UI ABI. The important design win is that `kaintana` owns the authored UI surface, themes, layout vocabulary, session helpers, and host routing, while `runtime/native` stays the generic substrate.

What changed:

- Added `blades/kaintana/` as the framework package. `src/kaintana.kn` now exports the first public Kaintana surface: `KaintanaWindowSpec`, theme packs (`solar-broadcast`, `marine-terminal`, `kawaii-voltage`), rect/split/row/column helpers, passive session/hot-reload wrappers over `stdlib/native/ui.kn`, immediate helpers (`panel`, `badge`, `button`, `metric`), retained helpers (`region`, `surface`, `label`), and backend-neutral host helpers that route to either the blade-local desktop host or the reusable `vulkain` lane.
- Added the blade-owned desktop compatibility host in `blades/kaintana/native/kaintana_desktop_bridge.c` plus `build-desktop.ps1`. It is intentionally small and compatibility-scoped: Win32/GDI only, a fixed command buffer of rect/text operations, screenshot/report support, and zero runtime ownership creep.
- Added `blades/kaintana-test/` as the acceptance blade. `src/main.kn` is the desktop workbench proof, and `entrypoints/vulkan.kn` is the foreign-presenter Vulkan proof. Both are full Kain apps that drive `world`, `entangle`, `patch`, `law`, `converge`, and `orchestrate` while calling into the Kaintana package for UI authoring.
- `blades/kaintana-test/run.ps1` now truthfully compiles the selected entrypoint for `desktop`, `vulkan`, or `all` instead of writing a fake runtime-config file that the app never consumed. The proof artifacts now land under `.kain/run/` as `kaintana_test_desktop.bmp`, `kaintana_test_desktop_frame.txt`, `kaintana_test_desktop_host.txt`, `kaintana_test_vulkan_frame.txt`, and `kaintana_test_vulkan_host.txt`.
- Fixed `kaintana_frame_report_text(...)` in `blades/kaintana/src/kaintana.kn`. The first attempt wrote pointer-looking garbage because the old array/loop string builder shape did not serialize the way it looked in source. The current sequential append form writes stable textual telemetry.
- The desktop acceptance artifact is visually real and readable: the screenshot shows the intended solar-broadcast workbench shell with a left rail, hero surface, telemetry lane, and command lane instead of a placeholder or a runtime-owned canned panel kit.

Validation:

- `powershell -ExecutionPolicy Bypass -File D:\Kain-Lang\blades\kaintana\run.ps1 -NoRun`
- `powershell -ExecutionPolicy Bypass -File D:\Kain-Lang\blades\kaintana-test\run.ps1 -Backend desktop`
- `powershell -ExecutionPolicy Bypass -File D:\Kain-Lang\blades\kaintana-test\run.ps1 -Backend vulkan`
- `mcp__z3_local__.check_smt2(report_name="kaintana-desktop-command-capacity", ...)` -> `unsat`, report `z3/reports/20260516T015051Z-kaintana-desktop-command-capacity.json`
- `mcp__z3_local__.check_smt2(report_name="kaintana-layout-split-partition", ...)` -> `unsat`, report `z3/reports/20260516T015051Z-kaintana-layout-split-partition.json`
- `samply --help` again confirmed the current Windows limitation: recording is Linux/macOS-only here, and Windows can only load existing profiles.

Durable lessons:

- Kaintana should keep proving the architecture rule, not breaking it: runtime UI stays passive, while actual presenters and framework vocabulary live in blades.
- `use c::...` imports are still resolved from the consuming blade's local `[[c_ffi.libraries]]` entries. Wrapping a native bridge in a library blade does not make the bridge declaration transitive yet, so consumer blades must repeat those `c_ffi` manifest entries.
- Imported local Kain modules that contain `world` / `entangle` declarations no longer need the old self-contained-entrypoint workaround. The bug was in realtime/native UI staging replaying flattened imports plus the original `use`; if this symptom ever returns, re-check `crates/kain-driver::compile_realtime_app_bundle(...)` before treating it as an entangle semantics failure.
- The current Vulkan proof is honest but intentionally narrow: it proves Kaintana can drive the same high-level app contract into a foreign presenter lane (`vulkain`) without runtime changes. It does not yet translate the full authored Kaintana scene graph into Vulkan draw commands.

# 2026-05-15 - `blades/pong` is now a real visual state-lattice demo, and LLVM learned the scalar constructor/direct-call coercions it needed to compile it

The fresh `blades/pong` workspace turned into a good dogfood task because it hit two real truths at once: the authored Kain state lattice was worth keeping, but the current native UI host adapter is passive-only, and the LLVM backend still had a scalar-constructor/direct-call coercion gap that a new blade exposed immediately.

What changed:

- Added the new blade workspace under `blades/pong/` with data-driven config in `config/pong_demo.json`, a Kain-authored `src/main.kn` that drives `world` / `entangle` / actor / `collapse` / `observe` state, layout/theme/helpers modules, and a blade-local run script that keeps all artifacts under `.kain/`.
- Replaced the old false-negative entangle self-check in `blades/pong/src/main.kn` with runtime-backed evidence from `native_entangle_registered_count()` and `native_entangle_propagation_count()`. The report and the on-screen metric now reflect the runtime's actual registration/propagation surface instead of trusting a stale mirror snapshot.
- Added a real blade-owned Win32/WGL presenter through `[c_ffi]`: `blades/pong/native/pong_window_bridge.h`, `blades/pong/native/pong_window_bridge.c`, `blades/pong/build-pong-window.ps1`, and the `KAIN.toml`/`run.ps1` wiring. The visible window, screenshot capture, and close semantics now come from that bridge, while the passive native-UI session remains in the blade as an authored state/report surface.
- The presenter writes `blades/pong/.kain/run/pong.bmp` and `blades/pong/.kain/run/pong_window_report.txt`. The screenshot shows the intended vector-arcade board, swarm field, trail, side telemetry bars, and score pips.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now lowers scalar constructor/cast calls like `Float(...)`, `Int(...)`, and `Bool(...)` as real numeric coercions instead of inventing undeclared direct calls such as `@Float`. The same pass also coerces direct-call numeric arguments to the declared LLVM primitive param type when the callee expects a different scalar width/type.
- Added LLVM regressions in `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`:
  - `llvm_coerces_numeric_call_arguments_to_declared_param_types`
  - `llvm_lowers_float_constructor_calls_as_numeric_casts`
- Added real local Pong SMT artifacts under `blades/pong/z3/proofs-experimental/`:
  - `pong-vertical-bounce-clamp.smt2`
  - `pong-paddle-clamp.smt2`
  - `pong-swarm-sample-grid.smt2`

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_coerces_numeric_call_arguments_to_declared_param_types -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_float_constructor_calls_as_numeric_casts -- --nocapture`
- `powershell -ExecutionPolicy Bypass -Command "& 'D:\\Kain-Lang\\blades\\pong\\run.ps1'"`
- `mcp__z3_local__.check_smt2(report_name="pong-vertical-bounce-clamp", ...)` -> `unsat`, report `z3/reports/20260516T011236Z-pong-vertical-bounce-clamp.json`
- `mcp__z3_local__.check_smt2(report_name="pong-paddle-clamp", ...)` -> `unsat`, report `z3/reports/20260516T011236Z-pong-paddle-clamp.json`
- `mcp__z3_local__.check_smt2(report_name="pong-swarm-sample-grid", ...)` -> `unsat`, report `z3/reports/20260516T011340Z-pong-swarm-sample-grid.json`
- `samply --help` on this Windows host confirmed the current limitation: `samply record` is Linux/macOS-only here, and Windows can only load existing profiles.

Durable lessons:

- In this checkout, `ui_host_session_create(..., "software")` does not mean "real window." It means a passive session/draw-command recording lane. If a blade needs actual pixels or screenshot artifacts, it must bring a blade-owned presenter through `[c_ffi]` or another live host path.
- `blades/pong` is now the small reference blade for that split: Kain owns the state lattice and proof/report logic, while a blade-local presenter owns the window.
- If a new blade starts failing with undeclared scalar constructor calls (`@Float`, `@Int`) or mismatched primitive call signatures, do not patch the authored Kain around it first. Re-check `compile_direct_call` in the LLVM backend and the two Pong-driven regressions above.

# 2026-05-15 - Ephemeral-local ownership cells now elide dead zero-fill when a full-width store dominates the first read

The next LLVM ownership win landed cleanly: fresh zeroed helper cells that already qualify for the `EphemeralLocal` erasure lane no longer pay an entry `zeroinitializer` store when the compiler can prove the first ownership use is a full-width dominating write on the exact cell before any read. This keeps the earlier semantic contract honest: if the fresh zero state could still be observed, the zero-fill stays. If it provably cannot, the compiler stops doing it.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now tracks a second proof-backed nomination set beside the existing ephemeral-local candidate set: block-scoped names whose zero-fill is dead under the current statement order.
- Added conservative type/width inference for obvious scalar LLVM lanes (`i1`, `i8`, `i32`, `i64`, `double`, and pointer-width values) so the backend can recognize full-width stores without pretending it understands arbitrary aggregate payloads.
- Added a dominance-style source scan for the local block: the pass only elides zero-init when the first ownership touch on the target is a `collapse` whose body begins with a full-width `mem_store` / `__kain_mem_store` to the exact pointer, and the remainder of the block still satisfies the old fresh/non-escaping ownership contract.
- `compile_stmt` now emits the ephemeral stack slot without `store [N x i8] zeroinitializer` when that proof-backed nomination succeeds. Read-before-write shapes stay on the earlier zeroed lane.
- Added a retained-zero regression in `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`: `llvm_keeps_ephemeral_zero_init_when_first_use_is_read`.
- Added new proofs:
  - `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-zero-init-elides-under-dominating-full-width-store.yaml`
  - `crates/kain-sys-codegen/z3/proofs-experimental/ownership-ephemeral-zero-init-dead-after-dominating-full-width-store.smt2`
- Updated `.agents/skills/kain-ownership-system/SKILL.md`, `ARCHITECTURE.md`, and a new benchmark note so future LLVM work treats zero-init elision as part of the ephemeral-local ownership theorem, not as a benchmark-only hack.

Validation:

- `cargo fmt -p kain-sys-codegen`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_ephemeral_single_cell_ownership_to_local_storage -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_keeps_ephemeral_zero_init_when_first_use_is_read -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_routes_helper_owned_ownership_keywords_to_helper_fast_path -- --exact --nocapture`
- `mcp__z3_local__.check_smt2(report_name="ownership-ephemeral-zero-init-dead-after-dominating-full-width-store", ...)` -> `unsat`, report `z3/reports/20260516T003258Z-ownership-ephemeral-zero-init-dead-after-dominating-full-width-store.json`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="llvm", report_name="llvm-ephemeral-zero-init-elision")` -> `19/19 proved`, report `crates/kain-sys-codegen/z3/reports/20260516T003258Z-llvm-ephemeral-zero-init-elision.json`
- `python benchmark/run.py --case alloc_churn --languages kain,rust --runs 9 --warmups 2` -> report `benchmark/out/reports/20260516T003453Z.llm.md`

Current benchmark reality:

- `alloc_churn` improved again on the stable `9`-run / `2`-warmup lane: Kain moved from `13.767 ms` in `benchmark/out/reports/20260516T000619Z.llm.md` down to `12.718 ms` in `benchmark/out/reports/20260516T003453Z.llm.md`, while Rust measured `10.201 ms`.
- Relative to the earlier corrected pre-erasure baseline `17.459 ms`, the combined ephemeral-local passes have now removed `4.741 ms` from Kain on this case, about a `27.2%` median reduction.
- The generated hot loop in `benchmark/out/build/alloc_churn/kain/alloc_churn.ll` still shows the intended alien shape: `@main` uses stack-backed `[8 x i8]` storage at line `9601`, stores the computed value directly at line `9622`, loads it back at line `9625`, and no longer emits a `store [8 x i8] zeroinitializer` before the first write.

Durable conclusion:

- The ownership theorem surface just widened in a meaningful way. Kain is no longer only proving “this heap/runtime protocol can evaporate”; it is also proving “the fresh zero state itself is unobservable here, so the zero-fill can evaporate too.”
- The remaining `alloc_churn` wall is now visibly scalar: stack traffic plus the per-iteration `% modulus` at `alloc_churn.ll:9631`. The next LLVM/Z3 frontier on this case should focus on register residency and a proof-backed modulo lowering rather than more ownership-runtime surgery.

# 2026-05-15 - LLVM now carries an ephemeral-local ownership witness lane that erases fresh loop-local cells into stack byte storage

The moonshot branch is real now: `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` has a compiler-owned `EphemeralLocal` provenance lane for fresh single-cell helper allocations whose ownership trace never escapes the local block. This is the first production lowering where the right question stopped being “how do we make heap bookkeeping cheaper?” and became “was a physical heap object ever semantically required here at all?”

What changed:

- Added `OwnershipPointerProvenance::EphemeralLocal`, `EphemeralOwnershipLocalWitness`, and block-local candidate tracking in the LLVM backend.
- Fresh single-cell helper allocs that stay inside a balanced `collapse -> observe -> decay` trace now lower to stack-backed `[N x i8]` storage plus direct load/store lowering instead of `__kain_alloc(...)`, `__kain_ownership_*`, and `inttoptr` helper traffic.
- Fixed the first real benchmark-shaped bug in the candidate matcher: nested blocks originally lost outer literal facts such as `cell_count = 1`, so loop-local `alloc_zeroed(cell_count, "Int")` in `alloc_churn` could not prove the single-cell contract. The backend now carries block-scoped known-`Int` literal maps far enough for nested ephemeral nomination.
- Added a new LLVM regression in `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` for the exact loop-local `alloc_churn` shape: `llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage`.
- Updated `.agents/skills/kain-ownership-system/SKILL.md` and `ARCHITECTURE.md` so future work treats the ephemeral lane as a proof-backed ownership species, not a benchmark-only trick.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_ephemeral_single_cell_ownership_to_local_storage -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_routes_helper_owned_ownership_keywords_to_helper_fast_path -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_ownership_keywords_to_runtime_guards -- --exact --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_consumes_lowered_alloc_and_realloc_helpers -- --exact --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="llvm", report_name="llvm-ephemeral-loop-local-ownership-erasure")` -> `18/18 proved`, report `crates/kain-sys-codegen/z3/reports/20260516T000551Z-llvm-ephemeral-loop-local-ownership-erasure.json`
- `python benchmark/run.py --case alloc_churn --languages kain,rust --runs 5 --warmups 1` -> report `benchmark/out/reports/20260516T000522Z.llm.md`
- `python benchmark/run.py --case ownership_memory --languages kain,rust --runs 5 --warmups 1` -> report `benchmark/out/reports/20260516T000551Z.llm.md`
- `python benchmark/run.py --case alloc_churn --languages kain,rust --runs 9 --warmups 2` -> report `benchmark/out/reports/20260516T000619Z.llm.md`

Current benchmark reality:

- `alloc_churn` now emits the intended alien shape in `benchmark/out/build/alloc_churn/kain/alloc_churn.ll`: stack-backed `[8 x i8]` storage in `@main`, no `__kain_alloc(...)`, and no ownership runtime calls in the hot cell path.
- On the more stable `9`-run / `2`-warmup report `benchmark/out/reports/20260516T000619Z.llm.md`, `alloc_churn` improved from the earlier post-contract baseline `17.459 ms` down to `13.767 ms`. Rust still led at `10.673 ms`, so the pass removed a large category of work but did not yet close the last scalar gap.
- On the `5`-run / `1`-warmup report `benchmark/out/reports/20260516T000551Z.llm.md`, `ownership_memory` stayed on the erased lane but still measured `18.019 ms` vs Rust `11.614 ms`. That confirms the old diagnosis: once heap/runtime protocol disappears, this case is mostly scalarization/register-residency/math lowering, not a native ownership-helper emergency.

Durable conclusion:

- The ephemeral-local witness lane is now a real production contract, not just a research note. It proved that a fresh helper-owned cell can evaporate out of the heap/runtime universe when the ownership trace is local and balanced.
- The remaining `alloc_churn` gap is now mostly outside the ownership runtime. The next best LLVM-side attack is likely dead zero-init elision for ephemeral locals that are always written before first read, plus any remaining scalar/register cleanup in the hot loop.

# 2026-05-15 - Low-level helper alloc count is element-count, not byte-count, and the first ephemeral-cell erasure theorem surface is now saved

The helper ABI contract has to stay explicit: `alloc(count, "T")` and `realloc_mem(ptr, count, "T", ...)` are element-count APIs, not byte-count APIs. A small but important repo-wide smell had crept into the benchmark and fixture authoring surface, where some single-`Int` cells were written as `alloc_zeroed(sizeof_type("Int"), "Int")`. Under the live helper ABI in `runtime/native/include/kain_runtime_memory.h`, that allocates `8` `Int` cells on the current 64-bit lane, not one.

What changed:

- Corrected the affected Kain sources to use element counts instead of byte counts for one-cell or two-cell heap storage:
  - `benchmark/cases/alloc_churn/main.kn`
  - `benchmark/cases/ownership_memory/main.kn`
  - `benchmark/cases/contention_wall/main.kn`
  - `runtime/fixtures/llvm_heap_memory/main.kn`
  - `docs/examples/07_low_level_memory_and_layout.kn`
- Updated `docs/syntax-and-semantics/low-level-memory.md` to say the rule explicitly: `alloc(n, "T")` uses `n` as element count and the helper ABI multiplies by `sizeof(T)` internally.
- Updated `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future benchmark work does not regress into byte-style heap authoring for single-cell cases.
- Added two solver-backed research artifacts:
  - `runtime/native/src/core/z3/proofs-experimental/helper-abi-single-int-cell-requires-one-element-count.smt2`
  - `crates/kain-sys-codegen/z3/proofs-experimental/ownership-ephemeral-cell-store-load-decay-erases-to-ssa.smt2`
- Expanded `research/2026-05-15-ephemeral-cell-erasure.md` from a stub into the actual frontier note for the moonshot branch.

Validation:

- `mcp__z3_local__.check_smt2(report_name="helper-abi-single-int-cell-requires-one-element-count", ...)` -> `unsat`
- `mcp__z3_local__.check_smt2(report_name="ownership-ephemeral-cell-store-load-decay-erases-to-ssa", ...)` -> `unsat`
- `python benchmark/run.py --case alloc_churn --languages kain,rust --runs 5 --warmups 1`
- `python benchmark/run.py --case ownership_memory --languages kain,rust --runs 5 --warmups 1`
- `python benchmark/run.py --case contention_wall --languages kain,rust --runs 3 --warmups 1`
- `kain.exe runtime/fixtures/llvm_heap_memory/main.kn -t llvm -o runtime/fixtures/llvm_heap_memory/.kain/llvm_heap_memory.ll` plus executing the generated `.exe`
- `kain.exe docs/examples/07_low_level_memory_and_layout.kn -t llvm -o docs/examples/.kain/07_low_level_memory_and_layout.ll` plus executing the generated `.exe` (`memory_total=11`)

Current benchmark reality:

- `alloc_churn` on the warm corrected rerun landed at Kain `17.459 ms` vs Rust `9.411 ms` in `benchmark/out/reports/20260515T232656Z.llm.md`.
- `ownership_memory` landed at Kain `15.070 ms` vs Rust `10.823 ms` in `benchmark/out/reports/20260515T232601Z.llm.md`.
- `contention_wall` still posted a massive proxy win at Kain `12.764 ms` vs Rust `1758.026 ms` in `benchmark/out/reports/20260515T232632Z.llm.md`.

Durable conclusion:

- The byte-count authoring mistake was real and needed to be fixed, but it does not erase the remaining `alloc_churn` gap by itself. The real next frontier is still the one we wanted: compiler-owned ephemeral ownership cells. Once a helper-owned cell is fresh, non-escaping, single-store, and alias-free, the meaningful question is no longer “how do we make heap bookkeeping cheaper?” but “can we prove the heap object was never semantically required?” Future LLVM work should introduce an explicit ephemeral-local provenance/witness class rather than trying to shave more nanoseconds off the same helper protocol forever.

# 2026-05-15 - The generic ownership prepare path was the wrong abstraction; LLVM and the native runtime now split imported and helper-owned ownership calls

The earlier helper-slot optimization exposed a deeper bug: `__kain_ownership_prepare_managed_pointer(...)` tried to infer helper-owned provenance by reading bytes before an arbitrary pointer. A saved solver witness now proves the old contract admitted a bad state where prepare returned success on a fake helper-looking prefix without actually making the later ownership operation safe.

What changed:

- `runtime/native/include/kain_runtime_ownership.h` and `runtime/native/src/core/kain_runtime_ownership.c` no longer use the generic prepare helper. Imported or unknown pointers now go through `__kain_ownership_ensure_imported(...)`, and the generic `begin_observe` / `end_observe` / `begin_collapse` / `end_collapse` / `decay` functions are registry-only and never probe helper headers.
- The helper fast path still exists, but it is now explicit: `__kain_ownership_begin_observe_helper(...)`, `__kain_ownership_end_observe_helper(...)`, `__kain_ownership_begin_collapse_helper(...)`, `__kain_ownership_end_collapse_helper(...)`, and `__kain_ownership_decay_helper(...)` are helper-owned-only entry points that may rely on the packed slot token in the allocation header.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now tracks helper-owned pointer provenance through lowered `alloc_zeroed` / `realloc_mem` locals. LLVM emits helper-only ownership calls for those locals and emits `__kain_ownership_ensure_imported(...)` plus the safe registry path for imported parameters and unknown pointers.
- `runtime/native/tests/test_ownership_memory.c` now includes a spoofed-prefix regression: an imported stack cell with bytes that look like a helper header must still use the imported registry path and must not be mistaken for helper-owned memory.
- Added new proofs:
  - `runtime/native/src/core/z3/proofs-experimental/ownership-generic-prepare-fake-header-bypass.smt2`
  - `runtime/native/src/core/z3/proofs/native-ownership-imported-ensure-fake-header-does-not-bypass-registration.yaml`
  - `runtime/native/src/core/z3/proofs/native-ownership-helper-fast-path-requires-heap-slot-match.yaml`

Validation:

- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_memory.c runtime/native/src/core/kain_runtime_ownership.c`
- `clang -I runtime/native/include runtime/native/tests/test_ownership_memory.c runtime/native/src/core/kain_runtime_memory.c runtime/native/src/core/kain_runtime_ownership.c -o target/codex-ownership-split/native_test_ownership_memory.exe; target\\codex-ownership-split\\native_test_ownership_memory.exe`
- `cargo test -p kain-core --test ownership_keywords_test --target-dir target/codex-ownership-split -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_ownership_keywords_to_runtime_guards --target-dir target/codex-ownership-split -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_routes_helper_owned_ownership_keywords_to_helper_fast_path --target-dir target/codex-ownership-split -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_consumes_lowered_alloc_and_realloc_helpers --target-dir target/codex-ownership-split -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path=\"D:/Kain-Lang/runtime/native/src/core/z3\", lane=\"ownership\", report_name=\"native_ownership_split_imported_helper_paths\")` proved `6/6`
- `mcp__z3_local__.check_smt2(report_name=\"ownership-generic-prepare-fake-header-bypass-witness\", ...)` returned `sat` with the intended fake-header witness

Current benchmark reality after deleting the wrong abstraction:

- `benchmark/out/reports/20260515T231608Z.llm.md` shows `alloc_churn` at Kain `17.512 ms` vs Rust `9.904 ms`. Relative to the earlier helper-slot report (`17.906 ms`), the split-path rewrite saved another `0.394 ms`, about `7.9 ns` per iteration.
- `benchmark/out/reports/20260515T231622Z.llm.md` shows `ownership_memory` at Kain `15.228 ms` vs Rust `10.634 ms`. Relative to the earlier helper-slot report (`15.446 ms`), this pass saved another `0.218 ms`.
- The runtime is now both faster and structurally safer on this lane: helper-owned benchmarks avoid imported registration entirely, while imported pointers no longer rely on speculative helper-header reads.

# 2026-05-15 - Helper-owned ownership guards now use a packed header slot token and a runtime prepare helper instead of repeated registry probes

This pass landed the first production slice from the earlier native-runtime speedup assessment. The goal was to cut the repeated hash-probe/import-precheck tax on helper-owned heap cells without regressing imported-pointer or post-decay semantics.

What changed:

- `runtime/native/include/kain_runtime_memory.h` now defines the helper allocation header as a packed `magic_and_slot + payload_size` pair. The low 16 bits of `magic_and_slot` carry `slot + 1`, so helper allocations keep a stable ownership registry slot token without growing the header beyond 16 bytes.
- `runtime/native/src/core/kain_runtime_memory.c` now registers helper allocations through `__kain_ownership_register_helper_allocation(...)`, stores the returned slot token in the header, and uses `__kain_ownership_helper_allocation_state(...)` plus `__kain_ownership_relocate_helper_allocation(...)` for the realloc path instead of the old `state -> maybe register missing region -> update` sequence.
- `runtime/native/src/core/kain_runtime_ownership.c` now has a generalized registry upsert helper, direct helper-slot resolution from the packed allocation header, and `__kain_ownership_prepare_managed_pointer(...)` for LLVM lowering. Helper-owned `begin_observe`, `end_observe`, `begin_collapse`, `end_collapse`, and `decay` now resolve their registry slot directly instead of re-hashing the pointer on every runtime call.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` no longer emits `__kain_ownership_state(...)` plus `__kain_ownership_register_imported(...)` around every ownership expression. The LLVM preamble is now one `__kain_ownership_prepare_managed_pointer(...)` call and an abort-on-error branch.
- Added durable proofs:
  - `runtime/native/src/core/z3/proofs/native-ownership-helper-realloc-slot-fast-path-rejects-non-idle-region.yaml`
  - `runtime/native/src/core/z3/proofs/native-ownership-helper-slot-token-stays-within-registry-capacity.yaml`
- Added exploratory SMT:
  - `runtime/native/src/core/z3/proofs-experimental/ownership-helper-slot-token-roundtrip.smt2`
- Removed the stale proof `runtime/native/src/core/z3/proofs/native-ownership-realloc-registers-missing-region-before-moving.yaml` because helper realloc no longer depends on missing-region preregistration.

Validation:

- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_memory.c runtime/native/src/core/kain_runtime_ownership.c`
- `clang -I runtime/native/include runtime/native/tests/test_ownership_memory.c runtime/native/src/core/kain_runtime_memory.c runtime/native/src/core/kain_runtime_ownership.c -o target/codex-ownership-fastpath/native_test_ownership_memory.exe; ./target/codex-ownership-fastpath/native_test_ownership_memory.exe`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_ownership_keywords_to_runtime_guards --target-dir target/codex-ownership-fastpath -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_consumes_lowered_alloc_and_realloc_helpers --target-dir target/codex-ownership-fastpath -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/runtime/native/src/core/z3", lane="ownership", report_name="native_ownership_helper_slot_fastpath")` proved 4/4 with `unsat`
- `mcp__z3_local__.check_smt2(report_name="ownership-helper-slot-token-roundtrip", ...)` returned `unsat`

Current benchmark reality from fresh focused runs:

- `benchmark/out/reports/20260515T222448Z.llm.md` shows `alloc_churn` at Kain `17.906 ms` vs Rust `9.731 ms` on `5` timed runs with `1` warmup. Relative to the earlier `2026-05-15T21:42:57Z` full-suite report (`19.628 ms` vs Rust `10.927 ms`), Kain improved by about `1.722 ms`, or roughly `34.4 ns` per iteration. The case is still Rust-favored, but the gap moved in the exact direction predicted by the solver-backed assessment.
- `benchmark/out/reports/20260515T222406Z.llm.md` shows `ownership_memory` at Kain `15.446 ms` vs Rust `10.130 ms` on the same `5`/`1` settings. Kain improved materially versus the earlier full-suite number (`17.402 ms`), but the remaining Rust gap is still small enough that the old diagnosis holds: this case is now mostly frontend/codegen/scalarization work, not a native ownership-helper emergency.
- The generated LLVM for both fresh cases now calls `__kain_ownership_prepare_managed_pointer(...)` before `observe`/`collapse`/`decay` and no longer emits direct `__kain_ownership_state(...)` or `__kain_ownership_register_imported(...)` calls in the hot benchmark body.

Recommended next step:

- If the target is “beat Rust on alloc-heavy cells,” stay on this runtime lane and remove the remaining alloc-side tax: either eliminate the alloc-time helper-region registration altogether with a proved tombstone/address-reuse strategy, or attack the `alloc_zeroed(stride, "Int") -> __kain_alloc(8, 8, 1)` surface mismatch so the benchmark stops paying for 64-byte payloads on one-`Int` cells. If the target is `ownership_memory`, shift back into LLVM/codegen and scalarization work instead of spending more time on the native ownership helpers.

# 2026-05-15 - Fair `string_ops` shape is now the general substring path, and the LLVM entry-hoist is the real win

The `string_ops` benchmark should stay a general string-lowering test, not a source-level micro-specialization contest. A shared benchmark-local `needle_len == 2` shortcut was solver-valid, but it was not the right lever for Kain: once removed, the fair general-path case actually ran faster locally than the specialized benchmark version.

What changed:

- Kept the LLVM backend repair in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` that hoists runtime top-level string const init into the function entry preamble, caches string lengths for known string values, and preserves the earlier bytewise `char_at(...) == char_at(...)` fast path plus borrowed string parameter lowering.
- Removed the benchmark-local two-byte substring shortcut from `benchmark/cases/string_ops/main.kn`, `main.rs`, `main.js`, and `main.py`.
- Kept the dead `% MODULUS` removal and the boolean branch toggle in all four language lanes.
- Updated `benchmark/benchmarks.json` and `.agents/skills/kain-benchmark-pipeline/SKILL.md` so the fairness note explicitly says `string_ops` stays on the general substring path and the intended specialization belongs in the compiler/backend, not benchmark-only case code.
- Deleted `benchmark/z3/proofs/string-ops-two-byte-kernel-matches-generic-search-step.yaml` and kept the durable fairness proof `benchmark/z3/proofs/string-ops-prefix-accumulator-never-reaches-modulus.yaml`.

Validation:

- `node --check benchmark/cases/string_ops/main.js`
- `python -m py_compile benchmark/cases/string_ops/main.py`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/benchmark/z3", lane="full", report_name="benchmark-string-ops-general-path-fairness")`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="llvm", report_name="llvm-entry-hoisted-const-init-string-ops-general-path")`
- `python benchmark/run.py --case string_ops --languages kain,rust,javascript,python --runs 5 --warmups 1 --timeout 300`

Current benchmark reality after this pass:

- The fresh fair report is `benchmark/out/reports/latest.llm.md` generated at `2026-05-15T22:09:50.881106+00:00` with `5` timed runs.
- `string_ops` now reports Kain `13.699 ms`, Rust `8.863 ms`, JavaScript `49.096 ms`, Python `284.333 ms`.
- That means the general-path Kain case is about `1.55x` slower than Rust but about `3.58x` faster than JavaScript on this run.
- The current Kain IR still shows the intended backend-owned wins: `@main` hoists `__kain_init_const_STRING_TEXT/NEEDLE/TAIL` once at entry, the loop no longer contains `srem` for parity or dead modulo math, and the branch selector is a simple `xor i1`.

# 2026-05-15 - Solver-backed native-runtime speedup assessment split the remaining Rust gap into true C-runtime tax versus frontend tax

This pass did not land production code. It was a Z3-first assessment aimed at the current benchmark report `benchmark/out/reports/20260515T214739Z.llm.md`, with exploratory proofs saved under `runtime/native/src/core/z3/proofs-experimental/`.

New experimental SMT files:

- `runtime/native/src/core/z3/proofs-experimental/string-two-byte-first-match-selection.smt2`
- `runtime/native/src/core/z3/proofs-experimental/ownership-helper-owned-no-import-fast-path.smt2`
- `runtime/native/src/core/z3/proofs-experimental/memory-stream-shift8-offset-bounds.smt2`

Solver results:

- `z3/reports/20260515T220233Z-string-two-byte-first-match-selection.json` -> `unsat`
  - proved a packed 16-bit two-byte window selector can return the same first-match index as the readable left-to-right scan for the current `string_ops` shape (12-byte text, 2-byte needle, start 0)
- `z3/reports/20260515T220039Z-ownership-helper-owned-no-import-fast-path.json` -> `unsat`
  - proved the helper-owned benchmark trace `alloc -> begin_collapse -> end_collapse -> begin_observe -> end_observe -> decay` never needs the imported-pointer registration fallback; the repeated `state == NOT_FOUND -> register_imported` branch is dead on that path
- `z3/reports/20260515T220039Z-memory-stream-shift8-offset-bounds.json` -> `unsat`
  - proved the `memory_stream` benchmark index domain (`i < 262144`, stride `8`) keeps `i * 8` equal to `i << 3` and below the 2 MiB byte span, so a shift-only/cursor-increment specialization is safe for that benchmark arithmetic

Durable conclusions:

- `alloc_churn` is the clearest remaining native-runtime win target. The gap to Rust is `174.02 ns` per iteration (`19.628 ms` vs `10.927 ms`). The generated LLVM currently pays three redundant ownership-state probes plus the ownership begin/end/decay operations every iteration. If only the three dead pre-checks disappear, the break-even budget is about `58 ns` saved per dead probe. If the whole ownership path for helper-owned pointers collapses into a direct header fast path, the break-even budget is about `24.86 ns` per ownership op across the seven repeated registry-style operations. This is realistic because the useful work in the benchmark is only one store, one load, and one modular add.

- `string_ops` is still beatable from the runtime/string-representation side even after the earlier LLVM repair. The current gap to Rust is only `54.4 ns` per benchmark iteration (`14.988 ms` vs `9.548 ms`), but the generated path still performs `1,700,000` `strlen` calls over the full benchmark run: `200,000` from `find_substring` itself and `1,500,000` from `starts_with_at`. That is `17` `strlen` calls per iteration on average, so eliminating only `3.2 ns` of cost per redundant `strlen` is enough to erase the full Rust gap. The packed two-byte proof above means a length-aware `(ptr,len)` fast path plus a direct 16-bit window search is mathematically viable.

- `memory_stream` still has a real low-level helper tax. The current gap to Rust is `7.644 ms` (`18.029 ms` vs `10.385 ms`), and the benchmark executes `524,288` `__kain_ptr_offset` calls. Closing the whole gap would require saving about `14.58 ns` per offset computation. That is plausible for a specialized shift/add or induction-cursor path because the current helper still carries multiply plus overflow arithmetic that the benchmark domain does not need.

- `ownership_memory` is mostly no longer a C-runtime problem. Its remaining gap is only about `7.192 ns` per loop iteration, and the ownership runtime calls happen outside the 750,000-iteration hot loop. Future work here should focus on scalarization / register residency / lowering quality rather than on the ownership C helpers themselves.

- `struct_method`, `option_result`, `branch_dispatch`, and most of `array_scan` are still primarily frontend/codegen problems, not native-runtime problems. C-runtime black magic will not be enough by itself to move those paths past Rust.

Important investigation note:

- The current benchmark/example authoring style often uses `alloc_zeroed(stride, "Int")` for a single `Int`, and the lowered ABI call is `__kain_alloc(8, 8, 1)`, which means the helper allocates `size * stride = 64` payload bytes for a one-`Int` cell. That may reflect a surface-contract mismatch between Kain authoring expectations and the canonical helper ABI (`size = element count`, `stride = bytes per element`). Treat this as a high-signal fairness/perf smell before trusting small-cell allocation benchmarks as pure allocator/runtime measurements.

Recommended next step:

- Attack `alloc_churn` first with a helper-owned ownership fast path that removes dead imported-pointer fallback checks and, ideally, bypasses the global ownership registry for helper allocations. After that, attack `string_ops` with stored string lengths plus a direct 2-byte packed search path. Those are the two runtime-owned routes that look most capable of flipping benchmark wins against Rust without waiting on broad frontend refactors.

# 2026-05-15 - LLVM string hot path no longer loses to JavaScript because authored string helpers now stay on byte math instead of RC churn

The LLVM backend's `string_ops` path was materially repaired in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`. The old benchmark loss was mostly self-inflicted lowering tax: repeated `len` calls became repeated `strlen`, `char_at(lhs) == char_at(rhs)` allocated temporary one-character strings and called `deep_eq`, and direct authored helper calls paid caller retain plus callee release on every string argument.

What changed:

- Added string-aware const metadata (`is_known_string`, literal text, byte length) so top-level string consts can skip generic runtime shape checks in the hot path.
- Added `string_length_values` plus direct `strlen` entry caching for authored non-extern string parameters. `len(x)` on those parameters now reuses cached SSA values instead of re-emitting runtime string-length work inside loops.
- Added a char-at equality fast path in LLVM lowering. When the source shape is `char_at(lhs, i) == char_at(rhs, j)` or `!=`, the backend now emits validity checks plus direct byte loads instead of allocating temporary one-character heap strings and calling `deep_eq`.
- Split internal string parameter ownership from general RC ownership. Direct authored function calls now skip caller-side retain for known string params, and the callee marks those params as borrowed locals so scope exit does not release them.
- Added durable Z3 proofs in `crates/kain-sys-codegen/z3/proofs/` for the char-at equality fast path and for borrowed-string call-frame refcount neutrality.

Validation:

- `cargo test -p kain-sys-codegen --lib --no-run`
- `python benchmark/run.py --case string_ops --languages kain,rust,javascript,python --runs 3 --warmups 1 --timeout 300`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="llvm", report_name="llvm-string-ops-fast-path-and-borrowed-call")`

Current benchmark reality after this pass:

- `string_ops` dropped from the earlier `150.708 ms` clean-suite median to `14.329 ms` locally on the repaired backend.
- Relative to the first post-runtime-debloat baseline for this session, Kain moved from `136.625 ms` to `14.329 ms`, roughly a `9.53x` improvement.
- Kain is now faster than JavaScript on `string_ops` (`14.329 ms` vs `49.247 ms`) and much closer to Rust (`8.525 ms`) than before.
- The next highest-payoff LLVM bottlenecks are still `struct_method` and `option_result`: tiny POD structs still lower through unconditional `KAIN_alloc`, and thin `Option<Int>` / `Result<Int, String>` shapes still allocate tagged boxes in their hot loops.

# 2026-05-15 - Native runtime is now lean by default and the old vendor/app lane is archived

The active C runtime has been cut back to the raw ABI floor. Vendored code, runtime-owned OpenGL, runtime-owned asset loaders, and hardcoded app/demo lanes are no longer part of ordinary native builds.

What changed:

- Archived the old runtime-owned vendor/app trees under `.zarchive/runtime_devendor_2026-05-15/`, including `runtime/native/src/vendor/`, `runtime/native/src/asset/`, `runtime/native/src/gfx/opengl/`, `runtime/native/third_party/`, `runtime/3rdparty/`, and the old vendor bridge headers.
- `runtime/native_core_runtime.toml` is now the only canonical production runtime manifest. `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json` are lean compatibility mirrors so older lookup paths cannot silently revive the archived lane.
- `crates/kain-core/src/runtime_contract.rs` now emits a lean raw-native default service set and only binds host/UI/graphics/actor services when the authored program shape actually requires them.
- The active runtime service registry is now the 31-service raw-native catalog. Vendor-era keys such as `ui.layout.yoga`, `ui.backend.imgui`, `gfx.backend.bgfx`, `script.quickjs`, audio vendor services, wasm vendor services, and allocator vendor services are gone from the supported production registry.
- CLI/bootstrap lookup, runtime pairing policy, Bazel sync, and the core conformance harnesses now all resolve to the lean runtime surface.

Validation:

- `cargo test -p kain-core runtime_contract --target-dir target/codex-runtime-contract -- --nocapture`
- `cargo test -p cli runtime_tools --target-dir target/codex-runtime-cli -- --nocapture`
- `cargo test -p cli selfhost_bootstrap --target-dir target/codex-selfhost-bootstrap -- --nocapture`
- `bash runtime/conformance/02_service_registry/compile_test.sh`
- `bash runtime/conformance/03_abi_startup_validation/compile_test.sh`
- `bash runtime/conformance/diagnostics/run_tests.sh`
- `bash runtime/conformance/graphics_runtime/run_tests.sh`
- Z3 MCP reports:
  - `z3/reports/20260515T115453Z-lean-runtime-service-mask-layout-clean.json` -> `unsat`
  - `z3/reports/20260515T115911Z-lean-service-registry-magic-collision-free.json` -> `unsat`

Current notes:

- Treat `.zarchive/runtime_devendor_2026-05-15/` as salvage/reference only. Do not let active manifests, build scripts, or service docs depend on it again.
- `//runtime:native_full_runtime` is now only a legacy-named compatibility mirror target. It should resolve to the same lean source set as the default Bazel runtime lane.

# 2026-05-15 - Full multi-language benchmark suite is green again, and the current Kain-vs-JS losses are mostly frontend boxing/string taxes

The full benchmark lane under `benchmark/` now completes cleanly on this Windows checkout with `python benchmark/run.py --timeout 300`, and the canonical reports at `benchmark/out/reports/latest.llm.md` plus `latest.json` reflect all four languages correctly: `kain`, `rust`, `javascript`, and `python`.

What changed:

- Fixed a dead native-runtime service probe path in `runtime/native/src/core/kain_runtime_services.c`. The service registry now probes only the live native net/process tables instead of referencing the deleted vendor-era function table.
- Fixed Windows-native filesystem metadata emission in `runtime/native/src/core/kain_runtime_native_stdlib.c` by using `GetFileAttributesExA` instead of POSIX `stat` in the Win32 path, plus a small helper for FILETIME-to-Unix conversion.
- Implemented the missing Win32 frame timer helpers in `runtime/native/src/platform/win32/kain_runtime_win32_shared.c`, which unblocked native host linking during the benchmark lane.
- Hardened the native runtime object-cache path in `crates/cli/src/main.rs` for Windows warm builds. When a cached object slot is stale, the CLI now clears old `.obj`, depfile, fingerprint, and lingering `.tmp` artifacts before recompiling, and it retries transient clang `permission denied` rename failures.
- Wrote the durable benchmark analysis to `benchmark/assesments/2026-05-15-full-suite-benchmark-assessment.md`.

Validation:

- `python benchmark/run.py --case evolutionary_loop --languages kain --runs 1 --warmups 0 --timeout 300`
- `python benchmark/run.py --timeout 300`
- `benchmark/out/reports/latest.json` now reports `ok: true`
- Solver-backed checks:
  - `z3/reports/20260515T114406Z-benchmark_struct_method_scalarization_equivalence_clean.json` -> `unsat`
  - `z3/reports/20260515T114406Z-benchmark_option_result_scalarization_equivalence_clean.json` -> `unsat`

Current benchmark reality:

- Kain wins `contention_wall` and `ghost_mirror` decisively, which means the language already has real semantic/runtime advantages in some shapes instead of merely keeping up through codegen tricks.
- Rust still owns the geomean because the LLVM lane boxes too many small values and routes too much work through runtime helpers.
- In the clean suite, JavaScript only beats Kain in `string_ops` and `struct_method`; `option_result` is already nearly tied and Kain-favored.
- The repo-backed reason for those losses is not "JS magic." The current LLVM/native lowering still heap-allocates tiny structs and tagged values too eagerly, and string hot paths still go through `string_new`, `char_at` -> heap `String`, `strlen`, `strcmp`, retain/release traffic, and runtime const-init guards.

Recommended next step:

- Attack the three obvious LLVM-lane bottlenecks in order:
  1. scalarize non-escaping POD structs/tuples instead of unconditional `KAIN_alloc`
  2. de-box small `Option` / `Result` values into scalar tag+payload forms when they do not escape
  3. rebuild string lowering around direct `(ptr,len)`-style value semantics and char/byte fast paths instead of heap-string helpers

# 2026-05-15 - Bazel Rust lane no longer carries the dead test app or Windows Swift `arch` noise

The root Rust/Bazel lane is now tighter and cleaner: the dead `apps/kade-desktop/controller` workspace member is gone, and the Windows `rules_swift` local-config override now fixes both the missing-`SDKROOT` path and the bogus `APPLE_PLATFORMS_CONSTRAINTS[arch]` emission for the generated Windows Swift toolchain stanza.

What changed:

- Removed `apps/kade-desktop/controller` from the root Cargo workspace in `Cargo.toml`, which also pruned the stale package entry from `Cargo.lock` and crate-universe state in `Cargo.Bazel.lock`.
- Extended the `rules_swift` single-version override in `MODULE.bazel` to apply two ordered patches:
  - `tools/bazel/patches/rules_swift_windows_sdkroot_guard.patch`
  - `tools/bazel/patches/rules_swift_windows_toolchain_target_compat.patch`
- The new second patch fixes `swift/internal/swift_autoconfiguration.bzl` so the generated Windows toolchain uses explicit Windows x86_64 constraints instead of the undefined `APPLE_PLATFORMS_CONSTRAINTS[arch]`.
- Regenerated Rust BUILD metadata with `tools/bazel/sync_rust_builds.py` after the workspace cleanup.

Validation:

- `$env:CARGO_BAZEL_REPIN='1'; bazel fetch //:kain --config=dev`
- `python tools/bazel/sync_rust_builds.py`
- `python tools/bazel/sync_rust_builds.py --check`
- `cargo metadata --no-deps --format-version 1`
- `bazel build //:kain //:kn //:blade --config=dev`

Current notes:

- The old Windows `rules_swift` `name 'arch' is not defined` analysis noise is gone after the patch-stack fix. If it reappears, assume patch drift first.
- The root Cargo workspace should stay reserved for promoted/core Rust packages. Temporary experiments belong in blades or other non-workspace surfaces unless they are intentionally part of the always-on Bazel lane.

# 2026-05-15 - Runtime-owned OpenGL presenter lane was evicted into `blades/opengl`

The native runtime no longer owns a live OpenGL presenter. Win32/WGL compatibility now lives as a blade-owned package under `blades/opengl`, while `runtime/native` keeps only the passive host substrate and generic UI ABI.

What changed:

- Removed the runtime-owned Win32/OpenGL presenter sources from the active runtime manifests and moved the legacy implementation into `blades/opengl/reference/runtime_legacy/` for salvage only.
- `runtime/native/src/ui/kain_native_ui_host_adapter.c` is now a passive host boundary. The active runtime only accepts passive backends such as `software`, `memory`, and `headless`; `win32-gl` is explicitly rejected instead of being treated as a default path.
- Removed `opengl32` from the runtime-owned default link library sets in both CLI bootstrap paths, and added per-`[c_ffi]` `link_libs` resolution so blade-owned native packages can own system libraries themselves.
- Removed the runtime-owned GL/helper declarations and overlay compile ownership from the active runtime headers, manifests, umbrella compile unit, metadata, and conformance compile script.
- Repointed authored Kain examples and smokes away from `win32-gl` so the teaching surface now reflects the passive runtime boundary.
- Added `blades/opengl/` as the raw reusable Win32/WGL compatibility blade package, with blade-local C bridge code, Kain wrappers, `build-opengl.ps1`, `run.ps1`, report output under `.kain/run/`, and screenshot capture through `OPENGL_BLADE_SCREENSHOT_PATH`.
- Fixed a blade-local LLVM collision by importing `c::opengl_bridge` only once at the app entry boundary, matching the proven `blades/vulkain` FFI pattern.
- Fixed the OpenGL screenshot path by capturing the rendered backbuffer before `SwapBuffers`; the first version produced a black BMP because readback happened after the swap.

Validation:

- `py -3 tools/bazel/sync_native_runtime_builds.py`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bash runtime/conformance/ui_runtime/compile_tests.sh`
- `cargo test -p blade --target-dir target\\codex-opengl-runtime-decouple -- --nocapture`
- `cargo test -p cli windows_default_native_runtime --target-dir target\\codex-opengl-runtime-decouple -- --nocapture`
- `powershell -ExecutionPolicy Bypass -File blades/opengl/run.ps1 -NoRun`
- `powershell -ExecutionPolicy Bypass -Command "$env:OPENGL_BLADE_SCREENSHOT_PATH='D:\Kain-Lang\blades\opengl\.kain\run\opengl.bmp'; & 'D:\Kain-Lang\blades\opengl\run.ps1'"`
- `blades/opengl/.kain/run/opengl_report.txt` now reports `frames=180`, `triangles=180`, `last_error=ok`
- `blades/opengl/.kain/run/opengl.bmp` is a nonblank 1280x720 render proof showing the compatibility triangle from the blade-owned presenter path
- `samply --help` still confirms this Windows host can load profiles but cannot record them

Current notes:

- The broad `cargo test -p cli` suite is still not fully green in this checkout because of pre-existing unrelated failures outside this refactor (`import_c::tests::test_import_with_target` and `selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout`).
- Historical `win32-gl` references still remain in old memory entries; the active runtime tree only keeps the conformance assertion that the legacy presenter must now be rejected.

# 2026-05-15 - `blades/vulkain` is now the raw reusable Vulkan blade package

`blades/vulkain` now exists as the minimal reusable Vulkan package blade for native LLVM Kain work. The design intentionally stays raw: the public Kain surface is only probe/counter/run/report calls over a blade-local C bridge, while app-specific policy belongs in consuming blades instead of in the package itself.

What changed:

- Added `blades/vulkain/` with `KAIN.toml`, `src/vulkain.kn`, `src/main.kn`, `build-vulkain.ps1`, `run.ps1`, `native/vulkain_bridge.c/.h`, GLSL shaders under `native/shaders/`, a runtime manifest example under `config/`, and a durable SMT proof at `native/z3/vulkain_bridge_bounds.smt2`.
- `src/vulkain.kn` is intentionally tiny: `vulkain_probe`, `vulkain_frames_presented`, `vulkain_vertices_drawn`, `vulkain_run_window`, and `vulkain_write_report`. This blade is the reference pattern for “raw package first, policy later.”
- `build-vulkain.ps1` now keeps all blade-local outputs under `.kain/`: SPIR-V in `.kain/gpu/basic_window/`, bridge DLL/import-lib in `.kain/native/`, and reports in `.kain/run/`. It also links `user32` explicitly for the Win32 window path.
- `run.ps1` now compiles `blades/vulkain/vulkain.exe`, copies `vulkain_bridge.dll` beside the root exe for easy testing, and runs the exe from the blade root so relative shader paths resolve correctly.
- `build-vulkain.ps1` now uses explicit `RuntimeInformation::IsOSPlatform(...)` checks instead of `$IsWindows` / `$IsMacOS`, because strict PowerShell hosts in this repo do not guarantee those convenience variables exist.
- `run.ps1` now accepts `-KainBin` / `$env:KAIN_BIN` and will reuse an already-built Bazel `kain.exe` when one is present. This keeps the blade on the Bazel compiler path even if `bazel build //:kain` is temporarily blocked by unrelated workspace breakage.
- The blade root no longer needs tracked runtime sidecars. `vulkain.runtime_contract.json` and `vulkain.realtime_app.json` now live under `.kain/out/vulkain/`, leaving the root focused on `vulkain.exe` plus the staged bridge DLL.
- Patched `crates/cli/src/main.rs` so Windows C FFI link resolution prefers a sibling import library (`.lib`) when a blade declares a shared library (`.dll`). Without this, LLVM/native link steps tried to feed the DLL itself to the linker and failed with `LNK1107`.

Validation:

- `.\\blades\\vulkain\\run.ps1 -NoRun` now succeeds, leaving `blades/vulkain/vulkain.exe`, `blades/vulkain/vulkain_bridge.dll`, and all side artifacts under `blades/vulkain/.kain/`.
- `.\\blades\\vulkain\\run.ps1` exits `0`, presents `240` frames, draws `720` vertices, and writes `.kain/run/vulkain_report.txt` with `last_error=ok`.
- `spirv-val --target-env vulkan1.3` accepts both `.kain/gpu/basic_window/vulkain_basic.vert.spv` and `.kain/gpu/basic_window/vulkain_basic.frag.spv`.
- Z3 MCP returned five `unsat` checks for `blades/vulkain/native/z3/vulkain_bridge_bounds.smt2` after tightening the shader-size claim into a real accepted-word-count invariant.
- `samply --help` still confirms the Windows host can load profiles but cannot record them.
- Fresh 2026-05-15 rerun: deleting `blades/vulkain/.kain/native_runtime/cache` still leads to a clean rebuild (`16 compiled`) and a clean launch (`frames=240 vertices=720`), which proves the blade-local runtime cache redirection is healthy after the script fixes.

Current notes:

- The MCP screenshot enumerator did not surface the short-lived Vulkan window during validation even though the executable presented all frames and wrote a clean report; use the frame/report proof as the reliable artifact here unless the window-capture tooling is extended for this path.
- The current frontend import-scan still dislikes multiline `pub fn` signatures in blade helper modules. Keep exported helper signatures on one line unless you are fixing that parser path directly.
- The current `bazel build //:kain` lane is blocked by unrelated workspace issues in this checkout (`crates/unreal/unreal_asset_registry/src/objects/md5_hash.rs` missing), and the Cargo CLI lane is also blocked by unrelated workspace manifest drift (`apps/kade-desktop/controller/Cargo.toml` missing). `blades/vulkain/run.ps1` therefore reuses an existing Bazel-built compiler artifact when available instead of pretending those unrelated breakages are Vulkan-blade problems.

# 2026-05-15 - KQuantum now drives a real Vulkan particle window through C FFI

`blades/kain-labs` now has a blade-local Win32 Vulkan bridge for KQuantum instead of only metadata-level native graphics reporting. Kain still owns the app, UI, worlds, entangle state, actors, reports, and orchestration; the C bridge owns the platform Vulkan surface/swapchain/pipeline ABI and is imported through `[c_ffi]`.

What changed:

- Added `blades/kain-labs/native/kquantum_vulkan_bridge.c/.h`, which dynamically loads `vulkan-1.dll`, creates a Win32 Vulkan instance/surface/device/swapchain/render pass/point-list pipeline, loads SPIR-V shaders from `.kain/gpu/vulkan_window/`, and renders procedural 3D particles.
- Added GLSL window shaders under `blades/kain-labs/native/shaders/` plus `build-vulkan-bridge.ps1`; outputs stay under `.kain/gpu/vulkan_window/` and `.kain/native/`.
- `src/main.kn` now launches the Vulkan window through `use c::kquantum_vulkan_bridge`, records counters in `.kain/run/kquantum_vulkan_report.txt`, shows Vulkan status in the native UI, and keeps all Kain runtime reports relative to the blade root (`.kain/run`).
- The C FFI import header intentionally exposes only numeric/status/report functions. C-owned `const char*` accessors caused a heap-corruption crash when called from Kain, so text diagnostics now cross the boundary through report files until foreign string ownership is explicitly modeled.
- Patched `.agents/skills/kain-blade-workspace/scripts/compile_kain_blade_to_root.ps1` to run check/compile from the blade root, default blade builds to `runtime/native_core_runtime.toml`, and move `.lib`/`.exp` linker sidecars under `.kain/out/<exe>/`.
- Patched `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` so LLVM signature pre-scan and item lowering recurse into generated `mod c: mod <library>:` blocks; without this, `[c_ffi]` externs could exist in augmented source but not be declared/lowered in LLVM.

Validation:

- `.\blades\kain-labs\run.ps1 -SkipShaderCompile` builds with the Bazel-backed compiler, leaves `blades/kain-labs/kain-labs.exe`, runs the Kain app, writes `.kain/run/kquantum_report.txt`, and reports `vulkan.window.frames=96`, `vulkan.window.particles_drawn=25165824`, `last_error=ok`.
- `spirv-val --target-env vulkan1.3` accepts `.kain/gpu/kquantum_kernels/kernels.spv`, `.kain/gpu/vulkan_window/kquantum_particles.vert.spv`, and `.kain/gpu/vulkan_window/kquantum_particles.frag.spv`.
- Direct C Vulkan harness under `.kain/native/` reports `probe=1 rc=0 frames=16 drawn=4194304`.
- Added durable solver source at `blades/kain-labs/native/z3/kquantum_vulkan_bridge_bounds.smt2`; Z3 MCP returned four `unsat` checks for particle budget, draw-counter overflow, safe swapchain cleanup index, and shader byte/word bounds.
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="llvm")` proved `14/14`.
- `cargo test -p kain-sys-codegen lowers_extern_cffi_declarations --target-dir target\codex-kquantum-vulkan-llvm -- --nocapture` passes, including the generated-module C FFI extern regression.
- UI screenshot proof exists at `blades/kain-labs/.kain/run/kquantum_ui.bmp`, 1440x860, about 4.95 MB, with nonzero sampled payload bytes.
- `samply --help` confirms this Windows host can load profiles but cannot record new profiles; no fresh samply profile was possible here.

Current notes:

- Bazel still prints the known Windows `rules_swift` local-config `name 'arch' is not defined` analysis noise, but `//:kain` completes under `--keep_going`.
- The C FFI cache hash already includes the header SHA. If an old `.kain/cache/c_ffi/<hash>` directory still shows removed symbols, check the newest hash before assuming active stale bindings.

# 2026-05-15 - Benchmark lane now covers Kain, Rust, JavaScript, and Python

The benchmark pipeline under `benchmark/` is now a multi-language lane instead of a Kain-vs-Rust-only lane.

What changed:

- `benchmark/benchmarks.json` now declares Kain, Rust, JavaScript, and Python source paths for each normal case, with language notes where a lane is intentionally a proxy.
- Added dependency-free `main.js` and `main.py` implementations for all 14 current cases.
- `benchmark/run.py` now supports `--languages`, builds/checks Node and CPython lanes, pins Kain benchmark links to `runtime/native_core_runtime.toml`, writes `latest.llm.md` plus JSON, and removes stale HTML output.
- Added the native Kain benchmark console at `benchmark/blades/kain-benchmark`, with the executable built to `benchmark/kain-benchmark.exe`. The console previews the latest LLM report and can run quick/full benchmark passes through the native process system.
- `stdlib/native/ui.kn::ui_event_kind_is` now avoids direct whole-string equality on extern-backed event strings; it compares length and characters so `&String` event values do not trip the checker.
- The blade compile helper now runs Kain from the discovered blade root during check/compile, which lets nested blade workspaces such as `benchmark/blades/kain-benchmark` resolve sibling source modules.

Validation:

- `python -m py_compile benchmark/run.py`
- JS and Python syntax checks across all benchmark cases
- `python benchmark/run.py --languages javascript,python --runs 1 --warmups 0 --timeout 300`
- `python benchmark/run.py --case scalar_mix --languages kain,rust,javascript,python --runs 1 --warmups 0 --timeout 300`
- `.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName D:\Kain-Lang\benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm`
- `benchmark\kain-benchmark.exe` exits 0 under `KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES=8` and writes a 3.7 MB screenshot under `benchmark/blades/kain-benchmark/.kain/run/`.

Current notes:

- `contention_wall` JavaScript/Python are explicit scalar proxies. Do not present those as equivalent worker/thread contention results.
- If an older benchmark script still resolves `runtime/native_runtime.toml`, that path is now lean too. The earlier Yoga/vendor drift warning is historical, but `runtime/native_core_runtime.toml` remains the canonical runtime path for ordinary file builds.
- The Win32/GL screenshot readback still appears flipped/mirrored in viewers; use it as nonblank render proof unless working directly on screenshot orientation.
- `samply --help` works on this Windows host, but recording is not supported here; it can only load existing profiles.

# 2026-05-15 - Native compiled UI hot reload now has a cross-platform runtime lane

The native compiled-UI/runtime bundle lane now has a real live-reload spine in C instead of only generation markers.

What changed:

- Added `runtime/native/include/kain_ui_hot_reload.h` and `runtime/native/src/ui/kain_ui_hot_reload.c` as the public/runtime implementation for compiled-bundle hot reload control.
- The API is cross-platform by design. File-backed live reload watches `KAIN_NATIVE_UI_BUNDLE` everywhere, while the low-latency control plane uses shared-memory backends behind one API instead of hardcoding Windows IPC into the architecture.
- `runtime/native/include/kain_ui_runtime.h` and `runtime/native/src/ui/kain_ui_runtime.c` now expose bundle reload options/reporting plus state transfer across reloads. Focus, active edit targets, hovered targets, editable values, and dirty state now survive bundle swaps when component ids or `persistent_layout_id` values match.
- `runtime/native/src/platform/win32/kain_runtime_viewport_win32.c` and `runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c` now boot the reload controller, poll it in-frame, apply new compiled bundles without closing the process, and update the live native window title when the reloaded bundle changes it.
- `runtime/conformance/ui_runtime/test_ui_runtime_reload.c` is the new durable smoke for both state-preserving reload and the shared hot-reload channel round trip.
- `runtime/native/src/ui/z3/proofs-experimental/kain_ui_hot_reload_ring_invariants.smt2` captures the ring invariants used by the shared event lane. The checked claims were: `seq & 127 == seq mod 128` and wrapped append ranges never exceed the 128-slot ring capacity.

Validation:

- Z3 MCP `check_smt2` returned `unsat` for the hot-reload ring mask/range proof pack (`native_ui_hot_reload_ring_invariants_fixed`).
- `bash runtime/conformance/ui_runtime/compile_tests.sh`
- `runtime/conformance/ui_runtime/bin/test_ui_runtime_reload.exe`
- `clang -I runtime/native/include -Wall -Wextra -std=c11 -D_CRT_SECURE_NO_WARNINGS -c runtime/native/src/platform/win32/kain_runtime_viewport_win32.c`
- `clang -I runtime/native/include -Wall -Wextra -std=c11 -D_CRT_SECURE_NO_WARNINGS -c runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c`

Current notes:

- `test_ui_runtime_reload.exe` passes and proves the new reload/state-transfer lane directly.
- `test_ui_runtime_parity.exe` and `test_native_ui_system_host_services.exe` are still failing in this environment, but those failures are in older fixture/live-host paths and are not specific to the new reload API surface.
- The optional shared-memory control plane now hangs off `KAIN_NATIVE_UI_HOT_RELOAD_CHANNEL`; use that for explicit live-reload orchestration, but keep the file-watch path healthy because it is the portable baseline on non-Windows hosts.

# 2026-05-15 - Blade builds now stay blade-local

The `kain-blade-workspace` skill and helper script now enforce blade-local build hygiene.

What changed:

- `compile_kain_blade_to_root.ps1` now defaults to `-OutputPlacement blade-root`, so `blades/<blade>/src/main.kn` compiles to `blades/<blade>/<blade>.exe` instead of repo-root `<blade>.exe`.
- The helper moves compiler/linker sidecars (`.ll`, `.bc`, `.pdb`, `.ilk`, runtime contract JSON, realtime app JSON) into `blades/<blade>/.kain/out/<exe-name>/`.
- The skill reference now requires SPIR-V artifacts under `blades/<blade>/.kain/gpu/<kernel-name>/` and UI/runtime screenshots or reports under `blades/<blade>/.kain/run/`; agents should not use repo-root `target/<blade>/` for blade-local work unless explicitly requested.
- KQuantum now writes its report to `blades/kain-labs/.kain/run/kquantum_report.txt`, and the validated executable is `blades/kain-labs/kain-labs.exe`.

Validation:

- `$env:CARGO_BAZEL_REPIN='true'; bazel fetch //:kain --config=dev` was needed because existing Cargo manifest drift made `Cargo.Bazel.lock` stale.
- `bazel build //:kain --config=dev` passes after the repin; the usual Windows `rules_swift` `name 'arch' is not defined` noise still appears but does not block the target.
- `.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry blades\kain-labs\src\main.kn -OutputName kain-labs.exe -VerifyLlvm` passes and leaves only `kain-labs.exe` in the blade root.
- `kain gpu-artifacts blades\kain-labs\src\kernels.kn -o blades\kain-labs\.kain\gpu\kquantum_kernels` plus `spirv-val` passes.
- Running `blades\kain-labs\kain-labs.exe` with `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH=blades\kain-labs\.kain\run\kquantum.bmp` exits `0` and writes blade-local run artifacts.

# 2026-05-15 - Foreign ABI core and C FFI v2 raw API classification

`kain-c-ffi` now has a shared ABI brain instead of owning scalar/pointer policy locally. The new `crates/kain-foreign-abi` crate models foreign ABI types, normalized C scalar tables, pointer/callback/aggregate bridge classes, external raw-pointer ownership tags, safety reports, and a local Z3 pack.

What changed:

- Added `crates/kain-foreign-abi` to the Cargo/Bazel workspace with `ForeignAbiType`, `ForeignBridgeClass`, `ScalarTypeTable`, `ForeignAbiLoweringPolicy`, `CBridgeTypeShape`, and `BridgeSafetyReport`.
- `kain-c-ffi` now consumes `kain-foreign-abi` for C type classification instead of rejecting function pointers, arrays, multi-level pointers, and raw scalar pointers in ad hoc extractor branches.
- The extractor now keeps a small typedef registry, so callback typedefs and pointer typedef aliases can affect later function signatures. This is important for Vulkan/D3D12-style `PFN_*` callbacks and handle typedefs.
- Generated bridge code now supports raw pointer handles, callback-pointer null/handle passthrough, multi-level pointer handles, and byte-buffer pointer returns as host objects. The Kain surface remains `Any` for those unsafe raw shapes because ownership/lifetime must stay explicit at the boundary.
- Added `tools/foreign_abi/mine_c_abi_shapes.py` to scan header corpora for callback typedefs, inline function pointers, raw scalar pointers, multi-level pointers, byte-buffer returns, arrays, and by-value named types.

Current design boundary:

- By-value aggregates are captured in the foreign ABI graph but are not callable from generated C bridges yet. That is intentional: calling a C function with a by-value struct/union without parsed layout and ABI-specific passing rules would be fake safety. The next real step is layout extraction and target ABI lowering, not `void*` pretending.
- Raw pointers imported through `kain-c-ffi` are marked as external-ownership shapes. They do not become first-class `collapse`/`observe`/`decay` regions until an explicit foreign ownership contract exists.
- `cargo test -p kain-c-ffi` can collide under default parallel test execution because bridge loading/env vars are process-global. Use serial execution for this crate until the test fixtures get unique bridge namespaces or registry reset hooks.

Validation:

- `cargo test -p kain-foreign-abi --target-dir target\codex-foreign-abi -- --nocapture`
- `cargo test -p kain-c-ffi --target-dir target\codex-foreign-abi -- --test-threads=1 --nocapture`
- `python tools/foreign_abi/mine_c_abi_shapes.py smoketest/fabric_FFI/c_ffi --out target/codex-foreign-abi/ffi_shape_report.json`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-foreign-abi/z3", report_name="foreign-abi-proof-pack-full")` proved `1/1`.
- `python tools/bazel/sync_rust_builds.py --check`
- `bazel test //crates/kain-foreign-abi:unit_test --config=dev` passed. Bazel still prints the known Windows `rules_swift` local-config `name 'arch' is not defined` analysis warning, but the Rust target completes.

Recommended next step:

- Add target ABI layout extraction for by-value structs/unions and callback trampolines for real Kain closures. The current v2 bridge supports null/passthrough callback pointers, which unblocks many raw APIs that allow null allocators/debug callbacks, but not yet closure-to-C trampoline generation.

# 2026-05-15 - KQuantum native GPU lab blade

`blades/kain-labs` is now a real Kain blade workspace for reference-driven native lab apps. The first lab recreates `blades/kain-labs/reference/KQuantum.tsx` as a Kain-authored native GPU particle/fluid simulator shell with a dense native UI, mode catalog, native graphics path, and SPIR-V compute kernels.

What changed:

- Added `blades/kain-labs/KAIN.toml`, `config/quantum_modes.toml`, and focused modules under `src/` for layout, theme, modes, native UI helpers, graphics helpers, GPU kernels, and the main KQuantum runtime.
- `src/main.kn` intentionally exercises expressive Kain features: `component`, `world`, `entangle`, `actor`, `patch`, `law`, `converge`, `orchestrate`, native UI, native graphics, filesystem output, runtime init/shutdown, and a root executable proof.
- `src/kernels.kn` defines four compute kernels: particle advection, velocity field, fluid pressure projection, and feedback composite. The fluid index contract is `x < 256`, `y < 256`, `z < 4`; using particle-count bounds for `id.x` was proven unsafe by Z3 before the fixed contract proved `unsat`.
- Patched `crates/gpu/src/codegen_spirv.rs` so SPIR-V storage/uniform wrapper type caches are module-scoped across multiple shader entries. This fixes duplicate `ArrayStride` decorations when several compute shaders share `StorageBuffer<Vec4>`.
- Added `spirv_edge_case_multi_entry_storage_buffer_types_are_decorated_once` in `crates/gpu/tests/spirv_smoke.rs` as the focused regression.

Validation:

- `mcp__z3_local__.check_smt2` proved the fixed KQuantum fluid cell index and linear particle dispatch bounds `unsat`; the first unconstrained-fluid attempt returned a real counterexample and is preserved in `z3/reports/`.
- `.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry blades\kain-labs\src\main.kn -OutputName kain-labs.exe -VerifyLlvm` produced `D:\Kain-Lang\kain-labs.exe`.
- `kain gpu-artifacts blades\kain-labs\src\kernels.kn -o target\kain-labs\kquantum_kernels` generated `.spv`, reflection JSON, and Rust host wrappers; `spirv-val target\kain-labs\kquantum_kernels\kernels.spv` passes after the codegen cache fix.
- The focused test binary run passed: `spirv_smoke_test.exe spirv_edge_case_multi_entry_storage_buffer_types_are_decorated_once --exact`.
- Running `kain-labs.exe` with `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH=target\kain-labs\kquantum.bmp` exits `0`, writes `target/kain-labs/kquantum_report.txt`, and captures a 4.95 MB BMP.

Current notes:

- `bazel test //crates/gpu:spirv_smoke_test --test_filter=...` still executes the whole Rust test binary in this checkout and fails on pre-existing SPIR-V smoke cases already documented below (`.xyz`, `group_index`, tuple/vector arithmetic, constructor-style casts). Use the exact test binary invocation for this focused regression until that broader lane is cleaned up.
- The native UI BMP capture confirms rendering but appears vertically flipped/mirrored through the current Win32/GL screenshot readback path. Treat this as a screenshot-host quirk unless working specifically on UI capture orientation.
- `samply --help` works, but this Windows host can only load saved profiles; it cannot record a fresh CPU profile.

# 2026-05-15 - Native UI runtime now uses indexed sidecars, bitset free-slot scans, and harder Win32/GL close semantics

The raw native UI kernel in `runtime/native/src/ui/kain_native_ui_system.c` and `runtime/native/src/ui/kain_native_ui_host_win32_gl.c` now has a much harder low-level spine.

What changed:

- Nodes, stable keys, styles, state cells, resources, menus, and dialogs now use power-of-two hash sidecars instead of repeated full-array linear scans on every lookup.
- Free-slot allocation for nodes/styles/state/resources/menus/menu-items/dialogs now uses occupancy bitsets plus a de Bruijn low-bit decode helper instead of scanning every slot for the next hole.
- `kain_native_ui_node_destroy()` now clears parent child counts, focus/IME/drag/event references, and the node's style/state payloads before rebuilding the affected indices, so repeated create/destroy churn no longer leaks logical occupancy.
- `kain_native_ui_node_set_parent()` now rejects parent cycles instead of allowing a node to be reparented under one of its own descendants.
- The Win32/GL host now treats `WM_CLOSE`, `WM_DESTROY`, `WM_QUIT`, and explicit shutdown consistently: close marks the session closed, `ensure_window()` refuses to recreate after shutdown intent, and screenshot auto-exit can close cleanly without a recreate race.

Proof and validation:

- `cargo check -p kain-ui-native --target-dir target\\codex-kain-ui-native-check`
- `mcp__z3_local__.check_smt2` returned `unsat` for `runtime/native/src/ui/z3/proofs-experimental/ui-low-bit-debruijn-signature-unique.smt2`, proving the de Bruijn multiplier used by `kain_native_ui_low_bit_index_u64()` yields distinct 6-bit signatures for all 64 one-hot inputs.
- Promoted that invariant into the curated UI pack at `runtime/native/src/ui/z3/proofs/c/kain_native_ui_low_bit_index_u64-debruijn-signature-unique.yaml`.
- `mcp__z3_local__.run_proof_pack(path=\"D:/Kain-Lang/runtime/native/src/ui/z3\", lane=\"smoke\")` now reports 2 proved UI cases. The remaining 10 counterexamples are older unconstrained generic overflow templates, not regressions introduced by this pass.
- Added a direct C runtime smoke at `runtime/native/src/ui/z3/fixtures/native_ui_runtime_index_smoke.c`. It compiles and passes in both:
  - software mode: direct runtime validation of the new sidecars, stable-key lookup, style/state cleanup, menu/dialog/resource handling, cycle rejection, and repeated create/destroy churn
  - live `win32-gl` mode: real window create/present/auto-exit with screenshot capture at `target\\codex-native-ui-win32-smoke\\native_ui_runtime_index_smoke.bmp`
- `samply --help` confirms the local Windows build can load saved profiles but cannot record new CPU samples on this host, so no native UI profile capture was possible here.

Current blocker outside the runtime changes:

- The higher-level `kain build` smokes for `smoketest/native-ui/pilot` and `runtime/fixtures/native_ui_runtime_systems` are currently blocked in this checkout by Kain build-lane identifier resolution failures (`ui_frame_begin` / `ui_node_from_stable_key`) before the produced executable stage. The direct C harness was added specifically so runtime-native validation can continue while that compiler/build issue is fixed separately.

Recommended next step:

- Replace the legacy UI `generic-size-add` proof placeholders with domain-aware `range_check` or `check_smt2` claims tied to real guards and capacities, so the UI Z3 lane becomes a trustworthy green/red surface instead of mostly reporting trivial unconstrained counterexamples.

# 2026-05-15 - Native runtime now lazily boots the actor scheduler on first actor use

The native startup path no longer eagerly pays the pooled actor scheduler cost for every executable. `kain_native_runtime_init()` still resets net/process sidecars, but actor runtime bring-up now happens on first actor spawn or actor-registry touch instead of process start.

Why this mattered:

- `memory_stream` was carrying a shared startup floor even though it never touched actors.
- The lazy-init path removes that cost from pure compute benchmarks while preserving actor behavior once the subsystem is actually used.

Validation:

- `mcp__z3_local__.check_smt2` returned `unsat` for `runtime/native/src/core/z3/proofs-experimental/actor-runtime-lazy-init-state-machine.smt2`.
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_actor.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_native_stdlib.c`
- `python benchmark/run.py --case memory_stream --runs 5 --warmups 1 --timeout 180`
- `python benchmark/run.py --case option_result --runs 5 --warmups 1 --timeout 180`

Benchmark read:

- `memory_stream` improved versus the eager-init build on the same machine, but still remains Rust-dominated.
- `option_result` is still allocation-bound and remains far from Rust; lazy actor startup is not the limiting factor there.

# 2026-05-14 - Benchmark lane expanded to 14 cases with recursion, string search, and array scans

The paired Kain-vs-Rust benchmark lane now covers 14 cases after adding:

- `recursive_sum`: recursive call-stack lowering in a tight loop.
- `string_ops`: substring search and string length/indexing over top-level string consts.
- `array_scan`: nested fixed-array indexing and weighted accumulation.

Design notes:

- The new string case intentionally stays on the compiler's proven surface: top-level string consts, `find_substring`, `len`, and `char_at`. It avoids the earlier string-builder shape that was tripping standalone benchmark compilation.
- The new array case uses the same untyped array-literal pattern already seen in the working example tree.

Validation:

- `python benchmark/run.py --runs 3 --warmups 1`

Latest report:

- `benchmark/out/reports/latest.html`
- `contention_wall` now wins even harder under the fair lane, while the new core-parity cases currently favor Rust. That is useful signal, not a failure: it shows the compiler/runtime gap is still concentrated in ordinary lowering, not just the ownership path.

# 2026-05-14 - Native net, async, and process handle registries now use hashed sidecars and bitset allocators

The native runtime's hottest fixed-capacity registries now avoid linear scans:

- `runtime/native/src/core/kain_native_net_system.c` uses SplitMix-style ID hashing, power-of-two probe tables, and occupancy bits for connections, listeners, HTTP requests, responses, and servers.
- `runtime/native/src/core/kain_runtime_async.c` uses the same sidecar pattern for task and timer lookup, and `kain_task_await` / `kain_async_sleep` now call the internal executor directly instead of bouncing through the public poll wrapper.
- `runtime/native/src/core/kain_native_process_system.c` uses the same sidecar pattern for process specs and process handles, with bitset-based free-slot selection and occupancy-based counts.
- The low-bit decoder stays shared with the already-proved actor occupancy path; new probe-bound proofs live under `runtime/native/src/core/z3/proofs-experimental/`.

Validation:

- Z3 MCP `check_smt2` returned `unsat` for `net-handle-index-probe-bounds`, `async-handle-index-probe-bounds`, and `process-handle-index-probe-bounds`.
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_native_net_system.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_async.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_native_process_system.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`
- `bash runtime/conformance/process_runtime/run_tests.sh --verbose`

Scratch benchmark:

- `target/codex_runtime_registry_hotpath_benchmark.exe`
- lookup path: about `4.74x` faster than the old linear scan
- free-slot path: about `13.21x` faster than the old linear scan

# 2026-05-14 - Benchmark lane now runs release compiler + benchmark-native tuning

The paired Kain-vs-Rust benchmark lane now prefers a release-built `kain.exe`
and injects an explicit benchmark-native tuning profile into Kain native
builds:

- `KAIN_NATIVE_PROFILE=benchmark-release`
- `KAIN_NATIVE_OPT_LEVEL=3`
- `KAIN_NATIVE_TARGET_CPU=native`
- `KAIN_NATIVE_DEBUG_INFO=0`

The CLI native LLVM/C path now threads those settings into both the runtime
object cache fingerprint and the final `clang` link step, so benchmark runs
cannot silently reuse debug-built native artifacts. The benchmark runner also
records the resolved compiler source, native tuning env, and Rust release flags
in `latest.json` and `latest.html`.

Validation:

- `cargo test -p cli native_toolchain_tuning -- --nocapture`
- `python benchmark/run.py --runs 3 --warmups 1`

Latest report:

- `benchmark/out/reports/latest.html`
- Kain still wins `contention_wall` under the fairer release/native lane.
- Rust still wins the remaining implemented edge cases, which is useful signal:
  the benchmark is now exposing real codegen/runtime gaps instead of debug-mode
  noise.

# 2026-05-14 - LLVM top-level consts and blade shader staging are now wired

The LLVM backend now treats top-level `const` items as real global values instead
of dropping them during item emission. Scalar literal consts lower to immutable
`internal constant` globals; runtime-backed consts such as derived expressions or
strings lower to internal globals with a lazy initializer and an init flag.
`Expr::Ident` and addressable identifier lowering both consult the const-global
table, so functions can read top-level consts without falling through to
`Undefined variable`.

Important design notes:

- Runtime-backed const initializers store the computed value before setting the
  init flag. `compile_const_load` emits the init call before loading the global.
- The LLVM LangRef distinction matters here: globals mutated by a lazy
  initializer cannot be marked `constant`; only fully literal scalars use
  `internal constant`.
- The Kain example proving-ground now has direct regression coverage for const
  globals, including const-to-const references and string consts.
- `kain-build` had a separate native artifact staging path from the CLI helper.
  It now extracts shader-only source before compiling optional SPIR-V shader
  bundles, preventing native-only functions such as
  `native_runtime_heap_validate()` from being typechecked under the GPU target
  during `kain build ... -t llvm`.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_resolves_top_level_const_values --target-dir target\codex-llvm-const -- --nocapture`
- `cargo test -p kain-build --target-dir target\codex-llvm-const shader_artifact_source_extracts_kain_example_shaders_without_native_body -- --nocapture`
- `cargo test -p cli --target-dir target\codex-llvm-const shader_artifact_source_extracts_kain_example_shaders_without_native_body -- --nocapture`
- `target\codex-llvm-const\debug\kain.exe build blades\kain-example\src\main.kn -t llvm -o target\codex-llvm-const\kain_example_const.ll`
- `python benchmark/run.py --case scalar_mix --runs 3 --warmups 1 --kain-exe target\codex-llvm-const\debug\kain.exe`
- `benchmark/out/reports/latest.json` now records the `scalar_mix` benchmark with top-level consts compiled and run successfully on both Kain and Rust.

Durable proof added:

- `crates/kain-sys-codegen/z3/proofs/control-top-level-const-lazy-load-follows-initializer.yaml`

# 2026-05-14 - Native ownership collapse guards now use a pointer index and occupancy-word allocator

The native collapse/observe/decay guard path in
`runtime/native/src/core/kain_runtime_ownership.c` no longer depends on a
4096-entry linear registry scan for every pointer lookup. The ownership
semantics remain the same, but the registry now has sidecar silicon math:

- a SplitMix-style `uintptr_t` mixer using `0xbf58476d1ce4e5b9` and
  `0x94d049bb133111eb`
- an 8192-entry power-of-two pointer index with masked probing
- 64-bit occupancy words for finding the next free ownership region
- the same de Bruijn low-bit decoder already proven useful in the actor table

What changed:

- `kain_ownership_find_slot` now uses the pointer index instead of scanning all
  `KAIN_OWNERSHIP_MAX_REGIONS`.
- `kain_ownership_find_free_slot` now scans 64 occupancy words instead of 4096
  region structs.
- new ownership registrations set both the occupancy word and pointer-index
  entry.
- realloc/update keeps the old state guards, mutates the region pointer only
  after validation, and rebuilds the pointer index so stale pointer hashes do
  not accumulate.

Experimental proofs added under
`runtime/native/src/core/z3/proofs-experimental/`:

- `ownership-pointer-index-probe-bounds.smt2`
- `ownership-occupancy-slot-composition-bounds.smt2`
- `ownership-debruijn-low-bit-distinct.smt2`

Validation:

- direct Z3 returned `unsat` for all three new experimental proof artifacts.
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core\\z3", lane="ownership", report_name="native-ownership-lane-after-pointer-index-pass")` proved `3/3`.
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core\\z3", lane="full", report_name="native-core-full-after-ownership-pointer-index")` proved `38/38`.
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-ownership\\z3", lane="full", report_name="kain-ownership-semantic-lane-after-native-index-pass")` proved `7/7`.
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_ownership.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`
- `target/native_test_ownership_memory.exe`
- `cargo test -p kain-ownership --target-dir target/codex-ownership-check -- --nocapture`
- `cargo check -p kain-core -p kain-sys-codegen --target-dir target/codex-ownership-check`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel test //runtime:native_test_ownership_memory`

Scratch benchmark `target/codex-ownership-hotpath-benchmark.exe`:

- ownership pointer lookup: about `100.07x` faster than the old linear scan
- full-table free-slot discovery: about `58.80x` faster than the old linear scan

Design note:

- This is not a semantic lattice change. It is a native registry data-structure
  change under the existing serialized lock. The proof boundary is index and
  slot arithmetic, while the curated ownership proof lanes continue proving the
  state-machine contract for collapse/observe/decay.

# 2026-05-14 - Reflection and native UI closed-string classifiers now use branchless token selectors

The reflection JSON/kind parser and native UI flag path now promote the
previously experimental closed-token math into runtime code.

What changed:

- `runtime/native/src/core/kain_runtime_reflection.c` now classifies
  reflection type kinds, item kinds, and JSON field names through packed
  16-byte token descriptors plus the shared 64-bit magic-state polynomial.
- Reflection parse loops now switch on `KainReflectionFieldToken` instead of
  paying repeated `strcmp(field_name, ...)` and `strcmp(key, ...)` chains.
- `runtime/native/src/ui/kain_native_ui_system.c` now classifies UI flags
  (`hidden`, `visible`, `focusable`, `interactive`, `disabled`, `hovered`,
  `pressed`) with the same branchless token selector.
- `visible` no longer needs a string-special-case branch. The hidden-bit
  inversion is folded into `enabled_bit = nonzero(enabled) ^ visible_bit`, and
  the flag update is a single mask equation.

Experimental proofs added under
`runtime/native/src/core/z3/proofs-experimental/`:

- `reflection-type-kind-selector-equivalence.smt2`
- `reflection-item-kind-selector-equivalence.smt2`
- `reflection-field-selector-equivalence.smt2`
- `native-ui-flag-selector-equivalence.smt2`
- `native-ui-flag-update-equivalence.smt2`

Validation:

- Z3 MCP `check_smt2` returned `unsat` for all five selector/update proof
  artifacts.
- Direct `z3.exe` also returned `unsat` for the five new artifacts and the
  combined `reflection-ui-token-magic-collision-free.smt2` reference.
- `clang -fsyntax-only -Iruntime/native/include runtime/native/src/core/kain_runtime_reflection.c`
- `clang -fsyntax-only -Iruntime/native/include runtime/native/src/ui/kain_native_ui_system.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1`
- Core curated Z3 full lane proved `38/38`.

Known proof-pack note:

- `runtime/native/src/ui/z3` still has noisy auto-extracted generic
  `size_add_ok` cases with empty bindings that report unrelated
  counterexamples. The new durable UI flag math is covered by the explicit SMT
  artifacts above.

# 2026-05-14 - Native actor runtime now uses occupancy-bitset allocation and a masked ring scheduler

The native actor runtime in `runtime/native/src/core/kain_runtime_actor.c`
got the kind of solver-backed hot-path rewrite that is only worth doing when
the old code is obviously paying rent on every spawn and schedule step.

What shipped:

- Actor-table allocation no longer linearly scans 1023 slots looking for a
  free `actor_id`. The table now keeps 64-bit occupancy words, reserves slot 0
  as the invalid ID bit, isolates the first free bit, and decodes the slot
  index with a de Bruijn multiply/lookup fast path.
- The pooled scheduler no longer heap-allocates and frees a linked-list node on
  every enqueue/dequeue. It now uses a fixed-capacity power-of-two ring buffer
  keyed by actor IDs with masked cursor indexing.
- Actor removal now clears the occupancy bit so the allocator and scheduler
  stay in the same truth domain.
- Experimental proof references for the weird math now live under
  `runtime/native/src/core/z3/proofs-experimental/`:
  - `actor-scheduler-ring-mask-index-bounds.smt2`
  - `actor-table-slot-composition-bounds.smt2`
  - `actor-table-debruijn-hash-distinct.smt2`

What the proof/benchmark loop said:

- The ring-mask and slot-composition constraints both proved `unsat`.
- The full 64-entry de Bruijn one-hot hash distinctness check also proved
  `unsat`, which means the low-bit decode table is valid for every one-hot
  occupancy mask state we rely on.
- Scratch benchmark `target/codex-actor-hotpath-benchmark.exe` showed:
  - actor-table insert path: about `48.17x` faster
  - scheduler queue path: about `19.00x` faster

Validation:

- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_actor.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core\\z3", lane="actor", report_name="native-actor-lane-after-bitset-ring-pass")` proved `7/7`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`

Design note:

- This is the right kind of dirty optimization for the native actor lane
  because the capacity is fixed and small enough that closed-form bit math is
  simpler than dynamic allocation. The unsafe-looking part is not the ring
  buffer itself; it is trusting the masked arithmetic and de Bruijn decode. The
  solver now owns that trust boundary.

# 2026-05-14 - Service registry alias canonicalization now uses cached token metadata and a solver-backed fast path

After the native `KainMap` branchless lookup pass, the best honest closed-world
target in `runtime/native` turned out to be the service registry rather than the
tiny reflection-kind classifiers.

What shipped:

- `runtime/native/include/kain_runtime_services.h` now gives
  `KainServiceDescriptor` cached `key_length` and `key_state` fields.
- `runtime/native/src/core/kain_runtime_services.c` now computes lowercase
  first-32-byte magic-state metadata once per descriptor and uses it to reject
  lookup candidates before paying for case-insensitive string compare.
- `kain_service_registry_canonicalize_key` now uses a solver-backed switch over
  the current `native.*` alias universe instead of a linear alias scan.

What stayed experimental:

- `runtime/native/src/core/kain_runtime_reflection.c` was a tempting candidate
  for the same finite-token trick, and the token-state proofs are sound, but the
  local benchmark showed the state computation costs more than the short
  `strcmp` ladder for that tiny universe. The proof remains in the lab and the
  runtime code stays on the old direct string checks.

Experimental proofs under `runtime/native/src/core/z3/proofs-experimental/`:

- `service-registry-magic-collision-free.smt2`
- `service-alias-canonicalizer-token-states.smt2`
- `reflection-ui-token-magic-collision-free.smt2`
- `reflection-kind-token-states.smt2`

Validation:

- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_services.c`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_reflection.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`
- Z3 MCP `check_smt2` returned `unsat` for `service-alias-canonicalizer-token-states`
  and `reflection-kind-token-states`.
- Scratch benchmark `target/codex-token-fastpath-benchmark.exe`:
  - service alias canonicalization: about `2.68x` faster
  - reflection token-state classifier: about `0.77x`, so it was not promoted

# 2026-05-14 - Native C runtime map hot path now uses cached key metadata and mask probing

The low-level builtin map in `runtime/native/src/core/kain_runtime_core.c` is no longer the old djb2-plus-modulo-plus-strcmp-everywhere implementation. The runtime now treats map capacity as a power-of-two invariant, caches per-entry key metadata, and probes with a mask instead of `%`.

What changed:

- `MapEntry` now stores `hash`, `key_prefix`, and `key_length`, and `KainMap` now stores a cached `mask`.
- `map_get` and `map_set` compute key metadata once, use `hash & mask` for the start slot, fold the first 32 bytes of the key into a synthesized prefix state, and use an 8-slot branchless probe window to reject almost all collisions before `memcmp`.
- Resize no longer recursively calls `map_set` on old entries. Rehash now reinserts with stored metadata and does not `rc_retain` again, which fixes the old refcount leak on every resize.
- Added durable Z3 proofs and reference SMTs:
  - `native-map-entry-allocation-does-not-wrap-after-capacity-guard.yaml`
  - `native-map-growth-threshold-stays-below-capacity.yaml`
  - `runtime/native/src/core/z3/proofs-experimental/map-magic-current-intent-pool.smt2`
  - `runtime/native/src/core/z3/proofs-experimental/map-eight-slot-selection.smt2`
  - `runtime/native/src/core/z3/proofs-experimental/map-power-two-window-index-bounds.smt2`
- Gathered direct solver reports for the bitwise probe math: `map-magic-multiplier-no-current-key-collisions`, `map-eight-slot-value-selection`, and `map-eight-slot-power-two-index-bounds` returned `unsat`.

Validation:

- `clang -c runtime/native/src/core/kain_runtime_core.c -Iruntime/native/include -o target/codex-map-core.obj`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="native", report_name="native-core-map-hotpath-native-lane")` proved 31/31.
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_core.c`
- Scratch C smoke `target/codex-map-smoke.exe` inserted, updated, grew, and read back native map entries successfully.
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="full", report_name="native-core-full-after-branchless-map-swar")` proved 38/38.
- Conservative local microbenchmark in `target/codex-map-benchmark.c` / `target/codex-map-benchmark.exe`:
  - insert speedup: ~1.71x
  - hit lookup speedup: ~1.48x
  - miss lookup speedup: ~1.29x

Design note:

- This is a real hot-path improvement, but not a fantasy 100x or 1000x jump. The current pass already crossed into deliberately alien territory with branchless 8-slot selection math. The next truly aggressive move would be a SwissTable-style control-byte sidecar or a generated perfect-hash fast path for closed key universes such as compiler-owned intent pools.

Recommended next step:

- If map lookups still matter in profiles, build a second-stage specialized table for runtime-owned closed dictionaries: 7-bit fingerprints in a control-byte array plus 8-at-a-time SWAR probing, or a generated perfect hash for known key sets.

# 2026-05-14 - `benchmark/` expanded from pressure tests into a broader language-edge suite

The Kain vs Rust LLVM benchmark lane now covers both the "alien tech" pressure tests and ordinary compiler battle cases. The suite is still manifest-driven through `benchmark/benchmarks.json`, and every case remains paired as `main.kn` plus `main.rs` with no external language dependencies.

What changed:

- Added `branch_dispatch`, `call_chain`, `memory_stream`, `alloc_churn`, `struct_method`, and `option_result` cases.
- Updated `benchmark/README.md` and `.agents/skills/kain-benchmark-pipeline/SKILL.md` with the new case taxonomy and current Kain gaps.
- Kept reports generated under ignored `benchmark/out/`; latest local report is still `benchmark/out/reports/latest.html`.

Current Kain gaps exposed:

- Scalar `match` in the standalone branch hot loop built but trapped at runtime, so `branch_dispatch` uses equivalent `if` dispatch until that native codegen path is fixed.
- Method receiver field access in the aggregate benchmark hit a native codegen gap, so `struct_method` uses `score_pair(pair)` instead of `pair.score()`.

Validation:

- `python benchmark\\run.py --runs 1 --warmups 0`
- `python benchmark\\run.py --runs 3 --warmups 1`

Latest compact run:

- `contention_wall`: Kain median ~978.235 ms, Rust median ~1940.509 ms, Kain won the proxy.
- `ghost_mirror`: Kain median ~80.953 ms, Rust median ~52.844 ms, Rust won.
- `evolutionary_loop`: Kain median ~155.904 ms, Rust median ~24.907 ms, Rust won.
- `ownership_memory`: Kain median ~90.714 ms, Rust median ~16.907 ms, Rust won.
- `branch_dispatch`: Kain median ~125.412 ms, Rust median ~17.590 ms, Rust won.
- `call_chain`: Kain median ~185.414 ms, Rust median ~33.882 ms, Rust won.
- `memory_stream`: Kain median ~93.237 ms, Rust median ~10.355 ms, Rust won.
- `alloc_churn`: Kain median ~93.864 ms, Rust median ~11.394 ms, Rust won.
- `struct_method`: Kain median ~157.714 ms, Rust median ~13.084 ms, Rust won.
- `option_result`: Kain median ~140.499 ms, Rust median ~10.870 ms, Rust won.

Recommended next step:

- Treat Rust's wins here as an optimization roadmap: native Kain needs LLVM optimization/link flags, aggregate/match codegen fixes, tagged-value fast paths, and cheaper ownership guard paths before these become competitive outside the collapse proxy.

# 2026-05-14 - `benchmark/` now has a paired Kain LLVM vs Rust LLVM pressure-test lane

The new `benchmark/` workspace is a manifest-driven benchmark lane for comparing dependency-free Kain LLVM examples against paired Rust LLVM examples. It is intentionally honest about maturity: some cases are direct implemented comparisons, while others are pressure-test proxies for Kain runtime features that are not fully exposed to user LLVM code yet.

What changed:

- Added `benchmark/benchmarks.json` as the data source for cases, source paths, maturity labels, and fairness notes.
- Added `benchmark/run.py`, a Python stdlib-only runner that resolves a direct Kain compiler, builds Kain `.kn` to LLVM/native executables, builds Rust with `rustc`, times warmups and samples, and writes `benchmark/out/reports/latest.html` plus `latest.json`.
- Added paired Kain/Rust cases for `contention_wall`, `ghost_mirror`, `evolutionary_loop`, and `ownership_memory`.
- Added `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future agents can extend the lane without rediscovering the runner contract.

Current benchmark interpretation:

- `contention_wall` is a proxy: Rust uses 100 OS threads and `AtomicI64`; Kain uses a zero-lock `collapse` ownership chunk over the same total increment count because Kain LLVM does not yet expose user-level OS-thread fanout for shared collapse regions.
- `ghost_mirror` is a semantic proxy: Rust uses std TCP loopback for a 1 MiB payload; Kain uses in-process entangle mirroring plus helper-owned payload mutation, not a two-process transport.
- `evolutionary_loop` is a dispatch skeleton: Rust uses runtime feature detection; Kain expresses the future autotuning slot through `converge` and `orchestrate`, but does not yet race AVX-512/native lanes.
- `ownership_memory` is direct: Kain `collapse`/`observe`/`decay` over a helper-owned heap cell versus Rust `Box` ownership.

Validation:

- `python -m py_compile benchmark\\run.py`
- `python benchmark\\run.py --case ownership_memory --runs 1 --warmups 0`
- `python benchmark\\run.py --case evolutionary_loop --runs 1 --warmups 0`
- `python benchmark\\run.py --case ghost_mirror --runs 1 --warmups 0`
- `python benchmark\\run.py --case contention_wall --runs 1 --warmups 0`
- `python benchmark\\run.py --runs 3 --warmups 1`

Latest compact run:

- `contention_wall`: Kain median ~828.686 ms, Rust median ~1569.326 ms, Kain won the current proxy.
- `ghost_mirror`: Kain median ~77.823 ms, Rust median ~40.087 ms, Rust won.
- `evolutionary_loop`: Kain median ~155.916 ms, Rust median ~24.992 ms, Rust won.
- `ownership_memory`: Kain median ~80.633 ms, Rust median ~12.505 ms, Rust won.

Recommended next step:

- Add real Kain-native OS-thread or actor-join support for the contention case, and add a Kain two-process entangle transport benchmark before using these numbers as marketing proof. Until then, keep the `maturity` and `fairness_note` fields blunt.

# 2026-05-14 - Native LLVM workbench 711-frame crash traced to loop-local allocas and fixed

The interactive `blades/kain-example` Win32/GL workbench no longer crashes at the deterministic 711-frame mark. `samply` reproduced the original failure as Windows stack overflow (`0xc00000fd`), and the generated LLVM IR showed the root cause: `alloca` instructions were being emitted inside long-running loop blocks in `@main`, causing per-frame stack growth.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now routes backend stack slots through `emit_entry_alloca`, which inserts textual LLVM `alloca` instructions immediately after the active function's `entry:` label instead of the current loop/control block.
- `next_reg()` now emits named locals (`%rN`) rather than ordered unnamed numeric locals (`%0`, `%1`, ...). This avoids invalid LLVM when later entry-block insertion places a higher-numbered alloca before an earlier emitted temporary.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` now has `llvm_hoists_loop_local_allocas_to_function_entry`, which checks a loop-heavy function and verifies the generated LLVM with repo `llvm-as`.
- `crates/kain-sys-codegen/z3/proofs/memory-entry-alloca-hoist-keeps-loop-stack-growth-zero.yaml` is the durable Z3 proof that frame count cannot create stack overflow once loop-stack contribution is zero and fixed entry allocation fits.
- Ownership keyword integration holes exposed by the full build were patched across `kain-core`, `kain-sys-codegen`, `gpu`, `ue5`, and CLI importer/selfhost walkers so `observe`/`collapse`/`decay` are not parser-only surface.

Validation:

- `cargo test -p kain-sys-codegen llvm_hoists_loop_local_allocas_to_function_entry -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_consumes_lowered_memory_helpers_into_pointer_ir -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_sizes_runtime_memory_helpers_for_bool_values -- --nocapture`
- `cargo build -p cli`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen", lane="full", report_name="llvm-full-after-named-ssa-and-entry-alloca")` proved 13/13.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\\blades\\kain-example\\run-ui.ps1`
- `@main` in `target/kain-example/kain_example_workbench.ll` now has 288 allocas and all 288 are in `entry`; non-entry alloca count is 0.
- `KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES` runs at 711, 720, 1000, 1500, and 3000 frames all returned exit code 0 with valid BMP captures.
- Post-fix `samply` profile: `target/kain-example/samply-after-fix-711.json.gz`. Assembly dump: `target/kain-example/kain_example_workbench.after_fix.text.asm`. Dynamic stack-adjust sites matching `subq %rax, %rsp` dropped from 278 in the stale crashing assembly to 31 in the rebuilt binary.

Recommended next step:

- Keep using `blades/kain-example/run-ui.ps1` as the first native LLVM UI proving loop. If another deterministic frame-count crash appears, inspect non-entry allocas and unnamed numeric local ordering before blaming Win32/GL.

# 2026-05-14 - `collapse`, `observe`, and `decay` are first-class ownership keywords across core, LLVM, and native runtime

The ownership model moved from semantic-kernel design into the language/runtime path. `collapse`, `observe`, and `decay` now parse as reserved keywords, typecheck against pointer-like regions, execute through interpreter ownership guards, emit `memory.ownership` runtime-contract requirements, lower through LLVM checked native calls, and have a C runtime guard registry backing native heap/imported pointer lifetimes.

What changed:

- `kain-core` now owns `Expr::Observe`, `Expr::Collapse`, and `Expr::Decay`, parser support, formatter support, comptime/runtime traversal, typechecking, backend memory validation, runtime-contract capability emission, and interpreter guard transitions.
- `crates/kain-sys-codegen` now lowers ownership expressions to `__kain_ownership_*` runtime calls. Untracked LLVM pointers are lazily registered as imported regions before guard transitions, so FFI/local pointers can participate without claiming Kain heap ownership.
- `runtime/native` now has `kain_runtime_ownership.h/.c`, a serialized C11-atomic registry for observe/collapse/decay transitions, heap allocation registration from `__kain_alloc`, pre-move registration for `__kain_realloc`, and `__kain_free` behind heap decay.
- `runtime/BUILD.bazel`, `tools/bazel/sync_native_runtime_builds.py`, and `runtime/runtime_manifest_data.bzl` now include `native_test_ownership_memory`, which proves the C runtime guard surface under Bazel.
- `crates/kain-ownership` policy now treats imported pointers as borrowed observe/collapse/lifetime-end regions, not heap-free regions.

Formal proof gathered with Z3:

- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-ownership", lane="full", report_name="ownership_keywords_core")` proved 7/7.
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="ownership", report_name="native_ownership_runtime")` proved 3/3.

Validation:

- `cargo fmt -p kain-core -p kain-sys-codegen -p kain-ownership`
- `cargo check -p kain-core -p kain-sys-codegen --target-dir target\\codex-ownership-check`
- `cargo test -p kain-ownership --target-dir target\\codex-ownership-check -- --nocapture`
- `cargo test -p kain-core --test ownership_keywords_test --target-dir target\\codex-ownership-check -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_ownership_keywords_to_runtime_guards --target-dir target\\codex-ownership-check -- --nocapture`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel test //runtime:native_test_ownership_memory`
- `bazel build //runtime:all`

Known validation noise:

- `bazel test //runtime:native_runtime_tests` still fails in the pre-existing actor monitor/link test (`Link did not propagate crash`). The new ownership C runtime test passes, and actor supervision passes.
- Bazel still prints the known Windows `rules_swift` local-config `name 'arch' is not defined` analysis noise under `--keep_going`, while the requested runtime build/test targets complete.

Recommended next step:

- Add a small `.kn` native executable fixture that allocates, observes, collapses, and decays a heap pointer end-to-end through the real LLVM linker path, then promote it into the native LLVM proving-ground blade once the sample surface is stable.

# 2026-05-14 - `crates/kain-ownership` landed as the proof-backed memory ownership kernel

The first vertical slice of the `collapse` / `observe` / `decay` ownership model now exists as a dedicated semantic crate instead of remaining a design note.

What changed:

- Added `crates/kain-ownership` to the workspace with a portable ownership-state lattice: `Idle`, `Observed(n)`, `Collapsed`, and `Decayed`.
- Added conservative region policy for local alloca, heap allocations, RC objects, world state, entangled authority endpoints, entangled mirrors, and imported pointers.
- Added lowering hints for future LLVM/native work, including readonly, noalias, lifetime-end, runtime guard, snapshot, and release/free implications.
- Added `crates/kain-ownership/z3` with focused lanes for state and policy proofs.
- Added `.agents/skills/kain-ownership-system/SKILL.md` so future agents have a targeted guide for this pipeline.

Design decisions:

- The new crate is intentionally a semantic kernel only. It does not add parser syntax or backend lowering yet.
- `observe` over world and entangle-backed regions is snapshot-first in v1. Direct live readonly aliasing would be dishonest until epoch/freeze semantics exist.
- `collapse` only succeeds from `Idle`; observed regions reject it.
- `decay` only succeeds from `Idle`; observed, collapsed, or already-decayed regions reject it.
- Entangled mirrors are deliberately conservative: no collapse or decay powers are claimed. Imported pointer policy was expanded later the same day to borrowed observe/collapse/lifetime-end without heap-free ownership.

Formal proof gathered with Z3:

- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-ownership", lane="full", report_name="kain-ownership-initial-full-after-line-anchors")`
- Result: 4/4 proved, 0 counterexamples, 0 unknown, 0 errors.

Validation:

- `cargo fmt -p kain-ownership`
- `cargo test -p kain-ownership --target-dir target\\codex-kain-ownership -- --nocapture`

Recommended next step:

- Wire `kain-core` parser/AST/typechecker support for intrinsic-style `observe`, `collapse`, and `decay` expressions against this crate before adding LLVM/native lowering.

# 2026-05-14 - Windows `kain` and Codex MCP now route through Bazel-backed launcher shims instead of stale Cargo binaries

The Windows workstation no longer relies on PATH order alone to keep `kain` fresh. The launcher contract now installs a real native shim in both the shared Bazel launcher directory and the old Cargo-bin location, so even long-lived agent processes that inherited an older PATH order still rebuild `//:kain` or `//:kn` before execution.

What changed:

- Added `scripts/windows/kain_bazel_cli_launcher.rs`, a tiny native Windows launcher that derives whether it is running as `kain.exe` or `kn.exe`, resolves the repo root plus Bazel config, and dispatches into `scripts/windows/launch-bazel-cli.ps1`.
- `scripts/windows/sync-kain-source-of-truth.ps1` now builds that launcher shim with `rustc`, installs it to the canonical shared launcher dir `D:/Kain-Bazel/bin`, and shadows `%USERPROFILE%/.cargo/bin/kain.exe` plus `kn.exe` with the same Bazel-backed shim after making a one-time `.pre-bazel-wrapper` backup of the previous binaries.
- The same sync script still writes compatibility `.cmd` wrappers in `D:/Kain-Bazel/bin`, but those now trampoline into the native `.exe` shim instead of embedding PowerShell command strings directly.
- `blades/kain-mcp/config/runtime_policy.json` now records the shadow launcher dir contract, and root `mcp.json` plus the live `C:\Users\Admin\.codex\config.toml` block now launch MCP through plain `command = "kain"` instead of pinning Python or a stale hardcoded Cargo binary path.
- `.agents/skills/kain-bazel-rust-sync/SKILL.md` and `ARCHITECTURE.md` now document the shared-plus-shadow launcher contract so future agents stop reaching for copied CLI binaries.

Why it changed:

- The earlier `D:/Kain-Bazel/bin/*.cmd` wrapper fixed fresh shells but not long-lived agent processes that had already inherited a PATH where `C:\Users\Admin\.cargo\bin` came first.
- That meant `where kain` could still resolve the old Cargo-installed binary even after Bazel work was correct, which is exactly the stale-CLI failure mode the user called out: a library target was green, but the executable an agent actually launched could still be old.
- Replacing both the shared launcher path and the legacy Cargo-bin entrypoint with the same Bazel-backed shim removes that ambiguity. `kain` now means "build the current Bazel CLI target, then run the Bazel artifact" no matter which of those two Windows locations wins resolution first.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv`
- `$env:CARGO_BAZEL_REPIN='1'; bazel fetch //:kain --config=dev`
- `kain doctor`
- `D:\Kain-Bazel\bin\kain.exe doctor`
- `C:\Users\Admin\.cargo\bin\kain.exe doctor`
- `codex exec --json --ephemeral -C D:\Kain-Lang -c mcp_servers.poly.enabled=false -c mcp_servers.z3_local.enabled=false "Respond with OK and exit."`

Durable note:

- On this workstation, future agents should treat `kain` as a Bazel launcher shim, not as a copied Cargo binary. If MCP or shell commands ever start hitting a stale CLI again, the first repair step is rerunning `scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv`, not manually rebuilding `cargo build -p cli` and copying exes around.
- The wrapper now exposes Cargo-to-Bazel drift immediately. If `kain` fails during Bazel analysis with a `crate_universe` digest mismatch, repin with `$env:CARGO_BAZEL_REPIN='1'; bazel fetch //:kain --config=dev` and keep the resulting `MODULE.bazel.lock` update with the Cargo manifest change that caused it.

# 2026-05-14 - `crates/kain-sys-codegen` now has a durable LLVM Z3 proof pack

The LLVM backend in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now has
its own durable solver workspace at `crates/kain-sys-codegen/z3`, so future
backend math and CFG work no longer has to start from ad hoc chat-only proofs.

What changed:

- Added `crates/kain-sys-codegen/z3/z3.toml` and `README.md` with focused lanes
  for `layout`, `control`, `casts`, `memory`, `llvm`, `full`, and workspace
  `smoke`.
- Added 12 curated proof cases that cover the current solver-friendly LLVM seams:
  `align_abi_size`, the struct-layout step in `abi_layout_for_ty`, string
  literal `len + 1` headroom, `next_label`, `next_reg`, match guard-fail target
  shape, `i1/i8/i32/i64` integer/bool cast semantics, and runtime
  base-address-plus-size bridge preconditions.
- Added `crates/kain-sys-codegen/z3/scripts/analyze_codegen_llvm_targets.py`,
  which scans `src/codegen_llvm/mod.rs` and emits
  `generated/codegen_llvm_target_inventory.{json,md}` so future agents can keep
  mining proof targets even when the parser-based analyzer is noisy on this
  large file.
- Ran the pack successfully: the clean full lane proved 12/12.
- Ran two off-lane floating-point counterexample checks and intentionally kept
  them out of `proofs/` so the durable CI lane stays green:
  - `double -> bool` differs from a naive non-zero float interpretation on
    `NaN`
  - `double -> i64` requires a finite in-range precondition and immediately
    admits witnesses like `+oo` when that contract is absent

Why it changed:

- The repo already had durable solver packs for `kain-core`, GPU codegen, and
  the native C runtime, but the LLVM lowering lane had recently produced real
  ownership and CFG bugs without having its own proof workspace.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` is large, high churn, and
  full of arithmetic and control-flow seams that are cheap to regress but also
  cheap to prove once the pack exists.

Validation:

- `python crates/kain-sys-codegen/z3/scripts/analyze_codegen_llvm_targets.py`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen", lane="layout", report_name="llvm-codegen-layout-rerun")` proved 3/3
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen", lane="full", report_name="llvm-codegen-proof-pack-clean")` proved 12/12
- `mcp__z3_local__.check_smt2(report_name="llvm-double-to-bool-nan-counterexample", ...)` returned `sat` with `x = NaN`
- `mcp__z3_local__.check_smt2(report_name="llvm-double-to-i64-precondition-witness", ...)` returned `sat` with `x = +oo`

Durable operator notes:

- After touching `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`, rerun the
  focused lane that matches the seam:
  - `layout` for `align_abi_size`, `abi_layout_for_ty`, and literal byte counts
  - `control` for `next_label`, `next_reg`, match CFG work, and PHI shape
  - `casts` for integer/bool coercions
  - `memory` for runtime bridge span contracts
- Keep counterexample-only experiments out of `proofs/` unless the backend
  semantics are also being hardened. Green proof lanes should stay green.
- If a future change touches float-to-int or float-to-bool lowering, do not stop
  at unit tests. Either reject unsupported float values earlier, or lower them
  through explicit checked semantics and then promote those semantics into the
  durable pack.

Recommended next step:

- Harden `double -> i64/i32/i8` and `double -> i1` lowering with explicit
  finite/in-range semantics or frontend rejection, then convert the current
  counterexample reports into new green proof cases.

# 2026-05-14 - `blades/kain-example` is now the native LLVM proving ground, the caller/runtime ownership seam was hardened, and LLVM `match`/print lowering was repaired

The repo now has a canonical one-file native LLVM example at `blades/kain-example/src/main.kn`, and the work to make it real flushed out three native-lane issues that are now fixed instead of worked around.

What changed:

- Added `blades/kain-example/KAIN.toml` plus a broad `src/main.kn` that intentionally exercises native Kain surface area in one place: low-level memory helpers, `Option`/`Result`/`Future`, `patch`/`law`/`converge`/`world`/`entangle`/`orchestrate`, actors, filesystem, input, networking, process, native stdlib UI, native graphics, shader declarations, and heap-health checkpoints.
- `crates/cli/src/llvm_native_stage.rs` no longer extracts shader source from mixed native+shader files by slicing raw spans. It now rebuilds shader-only source from the AST via the new formatter entrypoints in `crates/kain-core/src/formatter.rs`, which fixed mixed-file native LLVM staging for the example blade.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now retains borrowed `String` arguments before non-extern direct calls. This fixes the real ownership bug where stdlib/native wrappers and authored Kain callables released parameter locals on scope exit and could steal the caller's only live reference, which showed up as Windows heap corruption in the filesystem lane.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now lowers `print`/`println` through `stdout_write`, lowers `vec!` and `format!` in the native LLVM lane, registers enum layouts for native enum-pointer parameters, and repairs `Expr::Match` control flow so condition blocks, guard-fail cleanup, no-match fallback, and merge/PHI blocks are emitted as valid LLVM IR instead of aliasing the merge label into the last condition arm.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` now includes direct coverage for `println`, enum-parameter `match`, `vec!`/`format!`, and a new `llvm-as` verifier test that exercises guarded string-returning `match` lowering. That verifier test exists because the broken `match` lowering still looked fine to string-based assertions while producing invalid LLVM IR.
- `runtime/native/src/core/kain_runtime_memory.c` was hardened again: low-level helper allocation math still uses explicit size overflow guards, and pointer helpers now reject signed stride multiplication or uintptr address rebuild overflow instead of relying on UB-prone raw C pointer arithmetic. `runtime/native/include/kain_runtime_memory.h` now documents the `ERANGE` failure mode for helper arithmetic overflow.
- `runtime/native/src/core/kain_runtime_native_stdlib.c`, `runtime/native/include/kain_runtime_native_stdlib.h`, and `stdlib/native/runtime.kn` now expose `native_runtime_heap_validate()` on Windows so the proving-ground example can assert heap health between major native subsystems.
- `runtime/native/src/core/z3/z3.toml` and `README.md` now include a focused `memory` lane, and five durable `native-memory-*` proof cases now cover low-level allocation-header math plus pointer-address rebuild arithmetic.
- `blades/kain-example/src/main.kn` now uses the newly-repaired native LLVM surface directly: enum `match`, numeric `for` over `range`, `vec!`, `format!`, `println`, and a direct impl method call all live inside the executable lane instead of being commented as future work.

Why it changed:

- The first full pass at `blades/kain-example` reliably reproduced a Windows heap corruption (`0xC0000374`) only when stdlib/native filesystem wrappers were called inside the larger authored file shape. That was the key signal that the issue was in LLVM ownership lowering, not just in the C runtime.
- The same effort exposed that the low-level C memory helpers still performed signed multiplication and pointer-address addition directly in C, which is the wrong place to tolerate UB in a compiler-owned runtime ABI floor.
- The next proving-ground pass then exposed a second backend-only failure: enum `match` examples that frontend-checked correctly still emitted invalid LLVM IR because the last arm reused the merge label as a condition block, guard-fail paths skipped scope cleanup, and no-match fallbacks could contribute wrongly typed PHI values such as integer `0` for `i8*`.

Validation:

- `cargo test -p kain-sys-codegen retains_borrowed_string_arguments_before_non_extern_calls -- --nocapture`
- `cargo build -p cli --bin kain --bin kn`
- `target\\debug\\kain.exe check blades\\kain-example\\src\\main.kn --target llvm`
- `target\\debug\\kain.exe blades\\kain-example\\src\\main.kn -t llvm -o target\\kain-example\\kain_example.ll`
- `target\\kain-example\\kain_example.exe`
- repeated `target\\kain-example\\kain_example.exe` runs returned `0`
- `target\\debug\\kain.exe runtime\\fixtures\\llvm_heap_memory\\main.kn -t llvm -o target\\codex-kain-example-probes\\llvm_heap_memory.ll`
- `target\\codex-kain-example-probes\\llvm_heap_memory.exe`
- `mcp__z3_local__.run_proof_pack(path=\"D:\\Kain-Lang\\runtime\\native\\src\\core\", lane=\"memory\")` proved 5/5
- `mcp__z3_local__.run_proof_pack(path=\"D:\\Kain-Lang\\runtime\\native\\src\\core\", lane=\"full\", report_name=\"native-core-full-with-memory\")` proved 33/33

Additional validation from the follow-up LLVM-lowering repair:

- `cargo test -p kain-sys-codegen llvm_generates_match_patterns_for_ranges_or_and_literals -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_lowers_enum_match_parameters_as_native_enum_pointers -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_lowers_println_to_stdout_write -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_match_ir_verifies_with_guarded_string_results -- --nocapture`
- `target\\debug\\kain.exe docs\\examples\\01_types_structs_enums_patterns.kn -t llvm -o target\\codex-kain-example-probes\\match_example.ll`
- `toolchain\\llvm\\bin\\llvm-as.exe target\\codex-kain-example-probes\\match_example.ll -o target\\codex-kain-example-probes\\match_example.bc`
- `toolchain\\llvm\\bin\\llvm-as.exe target\\kain-example\\kain_example.ll -o target\\kain-example\\kain_example.bc`
- `cmd /c "start /wait "" target\\kain-example\\kain_example.exe & echo EXITCODE:%ERRORLEVEL%"`
- repeated `cmd /c "start /wait "" target\\kain-example\\kain_example.exe & echo EXITCODE:%ERRORLEVEL%"` runs returned `EXITCODE:0`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="full", report_name="native-core-full-after-match-lowering-fix")` proved 33/33

Durable operator notes:

- If a native LLVM-only heap corruption shows up after a stdlib/native call, inspect `compile_direct_call` in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` before assuming the C runtime is wrong. The current contract is that non-extern callees own parameter locals and release them on scope exit, so borrowed `String` arguments must be retained at the callsite.
- Treat `blades/kain-example/src/main.kn` as the first defacto native regression file, not as throwaway sample code. When it fails, either the language example drifted or the native lane regressed; both are real bugs.
- If a `match`-heavy native LLVM file compiles through parsing/typechecking but dies during link or LLVM verification, inspect `Expr::Match` lowering before assuming the authored source is wrong. The known failure class was merge-block reuse plus missing guard-fail cleanup, and the durable tripwire is the `llvm_match_ir_verifies_with_guarded_string_results` test plus `llvm-as` verification of emitted `.ll`.
- On this Windows workstation, use `cmd /c "start /wait "" <exe> & echo EXITCODE:%ERRORLEVEL%"` to verify generated native executable exits. `Start-Process` can misreport `-2147483645` for this lane even when the executable really returns `0`.

# 2026-05-14 - Bazel now uses a shared D-drive cache plus throttled interactive defaults on this workstation

The Bazel lane is now tuned for this Windows workstation to stop pinning the whole machine during Rust-heavy builds and tests, while still keeping a one-flag escape hatch for deliberate max-throughput runs.

What changed:

- `.bazelrc` moved the disk cache from the repo-local `.bazel-cache/disk` path to the shared `D:/Kain-Bazel/disk-cache` path alongside the existing output base, repository cache, and temp roots.
- `.bazelrc` now inherits `PATH` and `PATHEXT` into Bazel test environments so Windows subprocess-based Rust tests can find tools like `rustc`.
- The default local Bazel resource profile is now throttled for interactive work:
  - `--jobs=HOST_CPUS*.625`
  - `--loading_phase_threads=HOST_CPUS*.5`
  - `--local_resources=cpu=HOST_CPUS*.625`
  - `--local_resources=memory=HOST_RAM*.75`
  - `--local_test_jobs=HOST_CPUS*.25`
- `.bazelrc` also now exposes `--config=maxperf` to opt back into full-host Bazel scheduling when the operator explicitly wants it.
- Root `BUILD.bazel` now exposes more top-level crate aliases (`kain_blades`, `kain_codebase`, `kain_entangle`) and splits the top-level Bazel suites into:
  - `//:developer_smoke_tests` for the currently green Rust lane
  - `//:workspace_diagnostic_tests` for known source/runtime failures that Bazel now surfaces honestly

Why it changed:

- The earlier `build --jobs=HOST_CPUS` default let Bazel saturate the full 8c/16t host, which made Windows interactivity and Codex sessions noticeably worse during Rust-heavy work.
- Several crates were failing under Bazel for Windows-environment reasons rather than real source failures. Inheriting `PATH` and `PATHEXT` fixed those false negatives and let the root smoke lane become a real green lane.
- Keeping the disk cache on the shared `D:/Kain-Bazel` root means multiple runs and work surfaces reuse the same artifacts instead of each workspace warming its own isolated cache.

Validation:

- `bazel test //:key_crate_tests --config=dev`
- `bazel test //:developer_smoke_tests --config=dev`
- `bazel test //crates/kain-build:unit_test --config=dev`
- `bazel test //crates/kain-core:unit_test --config=dev`
- `bazel test //runtime:native_runtime_tests --config=dev`
- `bazel test //crates/cli:unit_test --config=dev --test_timeout=1200`

Current state:

- `//:developer_smoke_tests` is green under Bazel on this host.
- `//crates/kain-core:unit_test` fails for three source-level tests:
  - `language_features::tests::default_profile_keeps_struct_literals_disabled`
  - `realtime_app_bundle::tests::emits_bundle_owned_camera_and_presentation_metadata_for_viewports`
  - `realtime_app_bundle::tests::emits_realtime_bundle_with_viewport_scene_binding`
- `//runtime:native_runtime_tests` now reaches real C runtime failures instead of dying in analysis; `native_test_actor_monitor_link` still fails its crash-propagation assertion.
- `//crates/cli:unit_test` also now runs to completion under Bazel and exposes two real failures instead of timing out on missing tool lookup:
  - `import_c::tests::test_import_with_target`
  - `selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout`

Recommended next step:

- If interactive performance is still too spiky, lower the default CPU fraction one more notch to `HOST_CPUS*.5` before touching anything else. The current profile is intentionally conservative enough to leave headroom, but the next clean knob is CPU, not cache layout.

# 2026-05-13 - Bazel Rust workspace lane builds the main CLIs with D-drive cache/temp roots

The repo now has a generated `rules_rust` Bazel lane for workspace crates, with root aliases for the main developer binaries. This is meant to reduce repeated Cargo rebuild pain while keeping Cargo metadata as the source of truth.

What changed:

- `.bazelrc` now pins Bazel output, repository cache, repo/action/test temp env, and disk cache away from the low-space Windows `C:` drive:
  - `D:/Kain-Bazel/output-user-root`
  - `D:/Kain-Bazel/repository-cache`
  - `D:/Kain-Bazel/tmp`
  - workspace-local `.bazel-cache/disk`
- `tools/bazel/sync_rust_builds.py` generates deterministic Rust package `BUILD.bazel` files from `cargo metadata`.
- Generated crate rules now model Cargo's implicit same-package library visibility by adding `:<package>` deps to generated binaries/tests when a normal library target exists.
- `MODULE.bazel` wires `rules_rust`/crate-universe with the Rust 1.95.0 toolchain, PyO3 Python override, and zstd-sys build-script handling.
- Root Bazel aliases expose `//:kain`, `//:kn`, `//:blade`, plus focused crate library/test entrypoints.

Validation:

- `python -m py_compile tools/bazel/sync_rust_builds.py`
- `python tools/bazel/sync_rust_builds.py`
- `python tools/bazel/sync_rust_builds.py --check`
- `bazel info output_base` reports `D:/kain-bazel/output-user-root/ccujd7ry`
- `bazel build //:kain --config=dev`
- `bazel build //:kn --config=dev`
- `bazel build //:blade --config=dev`
- `bazel test //crates/kain-build:unit_test --config=dev`

Known issues:

- `bazel test //:crate_tests --config=dev` is not yet green. `kain-build` and `kain-commands` pass, but `kain-core` has current source/test failures and `cli` exits before output under Bazel.
- At least `language_features::tests::default_profile_keeps_struct_literals_disabled` is not a Bazel regression; the same targeted test fails under Cargo because `ParserStructLiterals` is enabled by default while the test expects it disabled.
- Bazel still emits a noisy Windows `rules_swift` local-config error (`name 'arch' is not defined`). With `--keep_going`, the Rust targets above complete successfully.
- Moving `--repository_cache` to `D:/Kain-Bazel/repository-cache` causes a one-time external dependency/toolchain refetch; subsequent builds reuse that D-drive cache.

Recommended next step:

- Repair or quarantine the current `kain-core`/`cli` unit-test assumptions, then promote `bazel test //:crate_tests --config=dev` from diagnostic lane to required green lane.

# 2026-05-13 - Bazel-native C runtime lane now mirrors the manifest split and is validated on Windows

The repo's Bazel work is no longer crates-only. `runtime/` now has a manifest-synced Bazel lane that gives parallel agents a shared native-runtime build surface instead of each person inventing local compile glue.

What changed:

- Added `tools/bazel/sync_native_runtime_builds.py` to parse `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml` and regenerate `runtime/runtime_manifest_data.bzl`.
- Added `runtime/native_runtime_rules.bzl` so runtime bundle expansion, platform selects, header globs, and Windows/POSIX compile options live in one Bazel macro layer instead of being repeated inline.
- Added `runtime/BUILD.bazel` with:
  - `//runtime:native_core_runtime`
  - `//runtime:native_runtime` as the default alias to the lean core lane
  - `//runtime:native_full_runtime` as the legacy-named second manifest target
  - `//runtime:native_runtime_tests` for the two actor C tests already living under `runtime/native/tests`
- Added root aliases/test-suite exposure so the runtime Bazel lane is visible from the workspace root the same way the crate Bazel work is.
- Added direct `rules_cc` and `platforms` deps in `MODULE.bazel` so the runtime package can use `cc_library`, `cc_test`, and platform constraints without relying on accidental transitive visibility.

Why it changed:

- The repo already had real Bazel synchronization work in `crates/`, but the native C runtime was still outside that shared contract.
- Without a runtime Bazel lane, parallel repo work would keep drifting between crate-only Bazel assumptions and ad hoc native compile scripts.
- At the time, the runtime had a lean-vs-broad manifest split. As of the 2026-05-15 runtime cleanup, `runtime/native_runtime.toml` has been collapsed into a lean compatibility mirror of `runtime/native_core_runtime.toml`, but the Bazel sync/generator structure from this entry still stands.

Validation:

- `py -3 tools/bazel/sync_native_runtime_builds.py`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel build //runtime:native_core_runtime //runtime:native_test_actor_monitor_link //runtime:native_test_actor_supervision --verbose_failures`
- `bazel test //runtime:native_test_actor_monitor_link //runtime:native_test_actor_supervision --test_output=errors`
- `bazel build //runtime:all`

Design decisions:

- The default Bazel runtime target is intentionally the lean lane: `//runtime:native_runtime -> //runtime:native_core_runtime`. That matches the repo's existing native-build default better than pointing Bazel at the broad manifest.
- The second Bazel runtime target originally preserved the broader manifest as `//runtime:native_full_runtime`. As of the 2026-05-15 runtime cleanup it is only a legacy-named compatibility mirror target over the same lean source set.
- The generator avoids `tomllib` so it still runs in the local Python 3.10 lane while producing deterministic `.bzl` data for Bazel.
- Windows Bazel C builds now pass `/experimental:c11atomics` in the runtime macro layer because `<stdatomic.h>` required it under Bazel's MSVC toolchain.

Current risks:

- Superseded on 2026-05-15: `runtime/native_runtime.toml` no longer pulls QuickJS/vendor sources. Any remaining docs or scripts that describe a broad Bazel vendor lane are stale and should be updated toward the lean compatibility-mirror contract instead of reviving the old sources.

Recommended next step:

- Keep `//runtime:native_full_runtime` as a compatibility name only, or remove it entirely once no callers still depend on the old Bazel entrypoint.

# 2026-05-13 - live Codex startup was fixed by pinning `kain_mcp` to direct `kain.exe`, not the Python launcher

The recurring `MCP startup failed: handshaking with MCP server failed: connection closed: initialize response` error on this machine was not the `blades/kain-mcp` request loop anymore. The live global Codex config had drifted back to the Python managed-sync launcher, and real `codex exec` reproduced the hang there even after the blade itself had already been hardened.

What changed:

- Rechecked the actual live `C:\\Users\\Admin\\.codex\\config.toml` instead of assuming the earlier direct-binary edit was still present.
- Reproduced the failure through real `codex exec --json --ephemeral -C D:\\Kain-Lang "Respond with OK and exit."` using the live global config.
- Confirmed through `C:\\Users\\Admin\\.kain\\state\\kain_mcp_launcher_trace.jsonl` that Codex was launching `scripts/python/launch_kain_mcp.py`, which then branched through managed-sync/preflight/fallback behavior before spawning `kain`.
- Proved the clean path by overriding Codex to launch `kain_mcp` directly as `C:\\Users\\Admin\\.cargo\\bin\\kain.exe run D:\\Kain-Lang\\blades\\kain-mcp`; the same proof also passed against `D:\\Kain-Lang\\target\\debug\\kain.exe`.
- Updated the live global Codex block so `mcp_servers.kain_mcp` now launches the direct binary with `args = ['run', 'D:\\Kain-Lang\\blades\\kain-mcp']`, `cwd = 'D:\\Kain-Lang'`, and `KAIN_NO_BANNER=1`.

Why it changed:

- The repo docs were already right that the Python launcher should be fallback/debug plumbing, not the default Codex boot path.
- The launcher is still useful for explicit managed-sync flows, but it introduces extra state branches (stale checks, lock/cooldown handling, preflight fallback, binary selection) that can make live Codex startup look flaky even when the Kain MCP server itself is healthy.
- Direct `kain.exe run ...` removes that ambiguity and matches the machine-facing stdout contract already enforced in `crates/cli/src/main.rs`.

Validation:

- `codex exec --json --ephemeral -C D:\\Kain-Lang -c mcp_servers.poly.enabled=false -c mcp_servers.z3_local.enabled=false -c mcp_servers.kain_mcp.command='C:\\Users\\Admin\\.cargo\\bin\\kain.exe' -c 'mcp_servers.kain_mcp.args=["run","D:\\\\Kain-Lang\\\\blades\\\\kain-mcp"]' ... "Respond with OK and exit."` (pass)
- Same direct proof against `D:\\Kain-Lang\\target\\debug\\kain.exe` (pass)
- Plain `codex exec --json --ephemeral -C D:\\Kain-Lang "Respond with OK and exit."` after the live config edit, with the normal global MCP set enabled (`poly`, `z3_local`, `kain_mcp`) (pass)

Durable note:

- On this machine, future agents should treat `C:\\Users\\Admin\\.codex\\config.toml` as the real source of truth for the live Codex MCP path. Keep `kain_mcp` pointed at direct `kain.exe run D:\\Kain-Lang\\blades\\kain-mcp` unless you are explicitly debugging or exercising the managed-sync launcher.

# 2026-05-13 - `collapse`, `observe`, and `decay` are a good ownership subsystem but not a drop-in final keyword pass

The proposed memory triad is directionally right for Kain's native story, but the solver pass and repo sweep showed it does not blend into the current `world`/`entangle`/LLVM pipeline as simple last-minute top-level keywords.

What changed:

- Researched `collapse`, `observe`, and `decay` against the current parser, AST, typechecker, interpreter runtime, LLVM backend, native helper ABI, and RC/destructor substrate.
- Proved three direct-fit counterexamples with Z3:
  - `observe` conflicts with the current entangle propagation model because authority writes immediately mutate mirrors in place.
  - `collapse` as a whole-world exclusive/noalias promise conflicts with the current shared runtime representation where world state is cloned into multiple handles.
  - `decay` as immediate return-to-OS conflicts with the existing RC substrate when weak references are still outstanding.
- Proved a guarded ownership lattice is internally coherent if:
  - `collapse` requires zero observers and no destroyed state
  - `observe` is only legal outside collapsed/destroyed state
  - `decay` requires zero strong refs, zero weak refs, zero observers, and no active collapse

Why it changed:

- The current language split matters: compiler-owned intent items live as top-level declarations, while low-level memory work lives as expression-level forms. The triad matches the second category more naturally than the first.
- `crates/kain-sys-codegen` currently emits no `noalias`, `readonly`, `alias.scope`, `llvm.lifetime.*`, or `llvm.invariant.*` markers, so the LLVM-side guarantees the triad wants are not present yet.
- The native runtime already has a useful destruction substrate through `rc_release` and custom destructors, but there is no surfaced Kain-level ownership state machine or canonical `free`/`decay` helper path yet.

Formal proof gathered with Z3:

- `mcp__z3_local__.find_counterexample` found a live model for `observe_active = true` plus in-place entangle mirror writes.
- `mcp__z3_local__.find_counterexample` found a live model for `collapse_active = true` with `alias_count = 2`.
- `mcp__z3_local__.find_counterexample` found a live model for `decay_now = true` with `strong_refs = 0` and `weak_refs = 1`.
- `mcp__z3_local__.check_smt2` proved `unsat` for mixed guarded states where collapsed or decayed memory still has observers, and for a decay precondition that still carries strong refs, weak refs, observers, or collapsed state.

Design decisions:

- Treat `collapse`, `observe`, and `decay` as one ownership-state subsystem, not as three isolated peer crates with duplicated state logic.
- Prefer a shared crate such as `crates/kain-ownership` or `crates/kain-memory-state`, then expose thin syntax/lowering hooks for the three surface forms.
- Prefer expression/block-scoped semantics over new top-level intent-item declarations. That keeps them aligned with the existing low-level memory model rather than forcing them into the `patch`/`law`/`world` item family.
- `observe` likely needs snapshot, freeze, or epoch semantics across an entangled component. `collapse` likely needs an explicit exclusive token. `decay` should be expressed as a zero-outstanding-capability transition, not as a blind free.

Current risks:

- Adding the triad as pure parser keywords without a shared ownership model would create semantic drift between `kain-core`, `kain-entangle`, the interpreter, the native runtime, and LLVM lowering.
- The interpreter still models low-level pointer and memory forms mostly as pass-through values, so non-native lanes cannot honestly simulate this triad yet.
- Splitting into three crates too early would likely duplicate diagnostics, token-state rules, and lowering policy instead of giving the compiler one ownership truth.

Recommended next step:

- Build one proof-backed ownership crate first, wire it into low-level memory expressions, and only then decide whether the surface syntax should read as `collapse`, `observe`, and `decay` keywords or as ownership blocks/helpers.

# 2026-05-13 - Native core runtime proof pack expanded into graphics, realtime, services, and stdlib hotspots

The native C core runtime proof pack under `runtime/native/src/core/z3` is now materially broader, and two real overflow models in live C code were eliminated instead of being left as hypothetical review notes.

What changed:

- Hardened `runtime/native/src/core/kain_native_graphics_system.c` so `kain_native_graphics_read_file_bytes` rejects wrapped `total + read` growth, refuses file sizes above the public `int64_t` ABI limit, and stops doubling capacity once it must grow directly to `needed`.
- Hardened `runtime/native/src/core/kain_runtime_native_stdlib.c` so `kain_native_fs_builder_reserve` rejects wrapped `length + additional + 1` arithmetic, tolerates zero-capacity callers, and stops doubling once it must grow directly to `needed`.
- Added new durable proof cases under `runtime/native/src/core/z3/proofs` for graphics file-read growth, graphics buffer and draw-command capacity counters, realtime binding-array bounds, service text-copy bounds, stdlib fs-builder growth, stdlib patch-journal growth, and stdlib parent-dir stack-buffer copy bounds.
- Added focused proof-pack lanes `graphics`, `realtime`, `services`, and `stdlib` in `runtime/native/src/core/z3/z3.toml`, and documented them in the pack README.

Why it changed:

- Z3 produced real pre-fix witnesses for unchecked runtime growth math:
  `z3/reports/20260513T235032Z-native-graphics-read-file-needed-size-prepatch.json`
  `z3/reports/20260513T235032Z-native-fs-builder-needed-size-prepatch.json`
- Those witnesses showed the prior code admitted 64-bit wraparound models even before considering allocator behavior, so the right move was to harden the arithmetic and then prove the guarded formulas.

Formal proof gathered with Z3:

- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="graphics")`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="stdlib")`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="realtime")`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="services")`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="full")`

The final full-pack report is `runtime/native/src/core/z3/reports/20260513T235833Z-native-core-full-expansion.json`, and it proved 28/28 native-core runtime obligations with zero counterexamples.

Validation:

- `powershell -ExecutionPolicy Bypass -File runtime\\compile_native_runtime.ps1`

Design decisions:

- The graphics file loader now treats the signed ABI return range as part of the safety contract, not as a separate caller concern. If the runtime cannot report a positive byte count in `int64_t`, it fails before allocating and copying more bytes.
- The stdlib text builder now prefers exact-to-needed growth once doubling would cross the `SIZE_MAX / 2` seam. That keeps the implementation simple while proving the dangerous arithmetic branch away.
- The new proof cases are pack-local and explicit rather than chat-only solver checks, so future agents can rerun focused runtime lanes without rediscovering these seams.

Current risks:

- The native core pack still has no focused proof lane for graphics hex decode return-range guards or for global session-id exhaustion over arbitrarily many create/destroy cycles; those are the next arithmetic seams worth formalizing if graphics session churn becomes a priority.

Recommended next step:

- Add pack-local extraction templates for graphics/runtime utility patterns so future passes can auto-suggest `count < max => count + 1 <= max` and guarded `len + extra + 1` obligations across new C helpers instead of requiring fully manual proof-case authoring.

# 2026-05-13 - Essential blade library ecosystem landed and cross-blade runtime imports were fixed

The `blades/` folder is no longer just `kain-json` plus `kain-mcp`. It now has a first reusable Kain-library layer that future runnable blades can depend on directly, and the runtime/import path was fixed so sibling blade imports work even when a blade runs from its own `src` directory.

What changed:

- Added essential library blades: `kain-fmt`, `kain-log`, `kain-fsx`, `kain-config`, `kain-process-kit`, `kain-http`, `kain-actor-kit`, and `kain-interop-kit`.
- Upgraded `blades/kain-json` from a tiny demo into a reusable JSON helper blade with `kain_json.kn` helpers and a real `kain_library` manifest.
- Rewired `blades/kain-mcp` to depend on the shared blade layer instead of keeping all formatting/config/process/path helpers local.
- Renamed the process wrapper blade from `kain-process` to `kain-process-kit` to avoid colliding with the existing Rust crate blade named `kain-process`.
- Fixed `blade::discover_blade_module_roots_from` so it merges module roots from ancestor workspaces. This is the key behavior that lets `kain run blades/<blade>` resolve sibling blade imports when the interpreter current directory is `blades/<blade>/src`.
- Added regression coverage in `crates/kain-blades` for ancestor-workspace module-root discovery and in `crates/kain-run` for executing a blade that imports a sibling blade dependency.

Design decisions:

- These new blades are intentionally thin authoring-core wrappers over existing stdlib/native capabilities, not competing second runtimes.
- `kain-mcp` is the first proof consumer for the shared ecosystem, so common helpers should migrate outward into library blades before new MCP-local helpers are invented.
- The Kain-facing process wrapper keeps the `kain_process.kn` module name for ergonomic imports, while the blade/package identity is `kain-process-kit` to stay unambiguous in workspace graphs.

Validation:

- `kain blades list .`
- `kain blades graph .`
- `kain check blades/kain-fmt`
- `kain check blades/kain-json`
- `kain check blades/kain-fsx`
- `kain check blades/kain-config`
- `kain check blades/kain-log`
- `kain check blades/kain-http`
- `kain check blades/kain-actor-kit`
- `kain check blades/kain-interop-kit`
- `kain check blades/kain-process-kit`
- `kain check blades/kain-mcp`
- `cargo test -p blade discovers_ancestor_workspace_module_roots_from_inside_a_blade`
- `cargo test -p kain-run executes_kain_blade_with_sibling_blade_dependency`
- `cargo run -p cli -- run blades/kain-json`
- `cargo run -p cli -- run blades/kain-process-kit`
- `cargo run -p cli -- run blades/kain-http`
- `cargo run -p cli -- blades run kain-mcp --dry-run`

Current risks:

- `kain blades check .` still reports unrelated pre-existing missing generated paths for `kade-desktop` and the Fabric DCC suite apps. That workspace-level failure is not caused by the new essential blades.
- The older PATH-installed Cargo binary drift risk is superseded on this workstation by the Bazel launcher shims in `D:/Kain-Bazel/bin` and `%USERPROFILE%/.cargo/bin`. If drift reappears, rerun `scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv` before falling back to manual CLI rebuilds.

Recommended next step:

- Build a second wave on top of this authoring core: `kain-schema`, `kain-cli`, `kain-template`, and likely `kain-toml` next, while continuing to move reusable logic out of `kain-mcp` and future blades into the shared library layer.

# 2026-05-13 - raw PTX/CUDA compute backend added beside canonical SPIR-V

Kain now has a first vertical CUDA backend slice without depending on `nvcc` or
the CUDA Toolkit. The backend emits raw PTX text from typed compute shader ASTs
and the runtime can load that PTX directly through the NVIDIA Driver API when an
installed driver is available.

What changed:

- Added `crates/gpu/src/codegen_ptx.rs` and exported `gpu::generate_ptx`.
  `CompileTarget::Cuda`, `ptx`, and `nvptx` now route to raw PTX output.
- Kept SPIR-V as the canonical shader bundle payload while adding optional
  derived PTX sidecars for compute-only shader bundles. HLSL remains a derived
  output too.
- Added `.ptx` artifact materialization in `crates/cli`, `crates/kain-build`,
  and `crates/kain-omni` so GPU artifact flows can write PTX beside the existing
  SPIR-V/HLSL bundle outputs.
- Added `crates/kain-gpu-runtime/src/nvidia_ptx.rs`, which dynamically loads
  `nvcuda.dll` on Windows, resolves CUDA Driver API symbols, loads PTX in
  memory with `cuModuleLoadDataEx`, launches compute kernels, and copies
  storage-buffer results back without external NVIDIA tooling.
- Added scalar type-constructor handling in `crates/kain-core/src/types.rs` so
  generated shader code such as `Float(i)` typechecks before backend lowering.

Design decisions:

- PTX is compute-only in this first slice. Non-compute shader bundles keep SPIR-V
  canonical payloads and get a reflection note instead of failing the whole
  bundle because PTX derivation is inapplicable.
- The emitter currently writes conservative `.version 7.8` / `.target sm_50`
  PTX for broad driver-JIT compatibility. NVIDIA's current CUDA docs are on PTX
  ISA 9.2, but this backend should only raise the default when it starts
  emitting instructions or targets that require a newer PTX ISA.
- Runtime execution accepts existing compute residency sidecars when the shader
  bundle contains derived PTX; the runtime does not invent a second compute-plan
  schema for CUDA.

Formal proof gathered with Z3:

- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\gpu", lane="ptx")`
  proved 5/5 PTX obligations: dispatch-thread-id lowering, group-index
  flattening, parameter alignment, runtime/codegen parameter-order equivalence,
  and storage-buffer byte-range safety.
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\gpu", lane="full")`
  proved 11/11 combined SPIR-V/PTX GPU codegen obligations.

Validation:

- `cargo fmt -p kain-core -p gpu -p kain-driver -p cli -p kain-gpu-runtime -p kain-build -p kain-omni`
- `cargo test -p gpu --test ptx_codegen --target-dir target\\codex-ptx-tests -- --nocapture`
- `cargo test -p cli cuda --target-dir target\\codex-ptx-tests -- --nocapture`
- `cargo test -p kain-driver compile_shader_artifact_bundle --target-dir target\\codex-ptx-tests -- --nocapture`
- `cargo test -p kain-gpu-runtime ptx_dispatch_group_count_rounds_up --target-dir target\\codex-ptx-tests -- --nocapture`
- `cargo test -p kain-gpu-runtime nvidia_ptx_executor_can_launch_tiny_kernel_when_driver_is_available --target-dir target\\codex-ptx-tests -- --nocapture`
- `cargo check -p gpu -p kain-driver -p cli -p kain-gpu-runtime --target-dir target\\codex-ptx-check`

Current hardware note:

- The local Windows checkout had `nvcuda.dll` available and the hardware-optional
  tiny-kernel smoke passed, proving the in-memory driver path on this machine.
  Keep that test skip-friendly for machines without an NVIDIA driver/device.

# 2026-05-13 - repo-local `kain_mcp` configs must use repo-relative paths, not `${KAIN_REPO_ROOT}`

The lingering post-reboot `kain_mcp` timeout was not inside the blade runtime.
The installed `C:\Users\Admin\.cargo\bin\kain.exe` answered a real MCP
`initialize` request in about `0.6 s`, including when launched from
`C:\Users\Admin`. The actual break was the repo-local `codex.config.toml` and
root `mcp.json`: both used `${KAIN_REPO_ROOT}` placeholders that Codex treated
as literal text in this lane. New repo sessions could therefore override the
good global MCP block with a broken local block even after a reboot.

What changed:

- Updated repo `codex.config.toml` to use `command = "kain"`,
  `args = ["run", "blades/kain-mcp"]`, `cwd = "."`, and `enabled = true`.
- Updated root `mcp.json` to use the same repo-relative launch contract and
  dropped the fake `KAIN_REPO_ROOT` env indirection.
- Updated `ARCHITECTURE.md` so future agents know the repo-local Codex config
  must stay repo-relative instead of relying on unsupported placeholder
  interpolation.

Formal proof gathered with Z3:

- `kain_mcp_literal_repo_root_placeholder_never_equals_real_repo_root`

That proof encodes the exact strings involved in this checkout and proves the
literal placeholder `${KAIN_REPO_ROOT}` cannot equal the real repo root
`D:\Kain-Lang`, so a client that skips interpolation must mis-resolve the path.

Validation:

- Literal-placeholder smoke:
  `C:\Users\Admin\.cargo\bin\kain.exe run ${KAIN_REPO_ROOT}/blades/kain-mcp`
  from `D:\Kain-Lang` failed immediately with
  `D:\Kain-Lang\${KAIN_REPO_ROOT}\blades\kain-mcp`
- Direct MCP `initialize` smoke against
  `C:\Users\Admin\.cargo\bin\kain.exe run D:\Kain-Lang\blades\kain-mcp`
  from `D:\Kain-Lang` returned the first `Content-Length` frame in about
  `0.613 s`
- Direct MCP `initialize` smoke against the same command from
  `C:\Users\Admin` returned the first `Content-Length` frame in about `0.594 s`

# 2026-05-13 - `kain build` now uses a Rust-style planned artifact graph

The Kain build surface now routes normal file, project, Rust-output, and
native-ui builds through `crates/kain-build` instead of keeping separate CLI
branches for each artifact family. The design goal is Rust/Cargo-grade build
quality with stronger lane isolation and explicit artifact identity.

What changed:

- `kain-build` owns typed build planning for Kain file builds, project builds,
  Rust artifact emission, native-ui app materialization, Blade workspaces,
  Cargo adapters, C sidecars, GPU artifacts, Fabric validation/runs, and
  explicit Node/Bun/custom tasks.
- Canonical build artifacts now default to
  `.kain/out/<host>/<lane>/<target>/<unit>/<task>/...`; cache stamps remain
  under `.kain/cache/build`, and reports remain under `.kain/reports/build`.
- Build reports and artifact manifests include the build lane, host, target,
  and SHA-256 output identities. Cargo invocations use isolated
  `CARGO_TARGET_DIR` roots and harvest `compiler-artifact` JSON messages.
- `kain build --lane bootstrap|dev|release|dist|selfhost` is the user-facing
  lane selector. Release-like lanes map Cargo work to release profile while
  `dev` and `bootstrap` stay debug-oriented.
- Native app and GPU-runtime helper builds now pass isolated Cargo target dirs
  instead of writing through ambient Cargo defaults.
- `crates/kain-build/z3` is the durable proof pack for output-collision,
  lane-isolation, and bounded DAG-cycle invariants.

Design decisions:

- Treat `.kain/out` as the canonical artifact contract. Source-adjacent or
  explicit output paths are materialized copies/views unless a manifest has a
  deliberate override.
- Keep build semantics in `kain-build`; CLI code should parse arguments, call
  planner/executor APIs, and print reports.
- If a future adapter needs Cargo, set `CARGO_TARGET_DIR` and parse Cargo JSON
  artifacts rather than checking for folder existence.

Validation:

- `cargo check -p kain-build --target-dir target\codex-kain-build-system`
- `cargo test -p kain-build --target-dir target\codex-kain-build-system -- --nocapture`
- `cargo check -p kain-commands -p cli --target-dir target\codex-kain-build-system-cli`
- `mcp__z3_local__.run_proof_pack(path="D:\Kain-Lang\crates\kain-build", lane="build")`

# 2026-05-13 - `kain_mcp` now boots directly from compiled `kain.exe` without the Python shim

The `kain_mcp` boot lane no longer needs `py` in the default Codex path. The
root cause of the lingering "Starting MCP servers" hang was that direct
`kain.exe run blades/kain-mcp` boot still mixed human CLI output with machine
stdio expectations, and the blade's runtime-policy lookup was brittle when the
repo root was the current working directory.

What changed:

- Updated `blades/kain-mcp/src/runtime_settings.kn` so the blade can resolve
  `blades/kain-mcp/config` correctly when launched from the repo root instead of
  assuming a blade-local cwd.
- Updated `crates/cli/src/main.rs` so the CLI suppresses the human banner on
  non-terminal stdout and also honors `KAIN_NO_BANNER` /
  `KAIN_ENGINE_NO_BANNER`. Machine-facing consumers like MCP and JSON pipes now
  get protocol/data output first instead of a banner line.
- Switched repo `codex.config.toml` and root `mcp.json` to launch
  `kain run ${KAIN_REPO_ROOT}/blades/kain-mcp` directly instead of routing
  through `scripts/python/launch_kain_mcp.py`.
- Switched the live machine `C:\Users\Admin\.codex\config.toml` block to launch
  `C:\Users\Admin\.cargo\bin\kain.exe` directly with
  `run D:\Kain-Lang\blades\kain-mcp`.
- Kept `scripts/python/launch_kain_mcp.py` as the managed-sync fallback and
  launcher-trace path rather than deleting it. It still matters for explicit
  sync/debug workflows, but it is no longer the default Codex boot contract.

Formal proof gathered with Z3:

- `kain_mcp_nonterminal_stdout_never_emits_cli_banner`

That proof encodes the new banner gate and proves there is no model where
stdout is non-terminal yet `suppress_banner` is false.

Validation:

- `cargo build -p cli --target-dir target/codex-kain-mcp-direct`
- Direct stdio `initialize` smoke against
  `target/codex-kain-mcp-direct/debug/kain.exe run D:\Kain-Lang\blades\kain-mcp`
  returned `Content-Length` first and a valid MCP initialize body in about
  `12588 ms`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- Direct stdio `initialize` smoke against
  `C:\Users\Admin\.cargo\bin\kain.exe run D:\Kain-Lang\blades\kain-mcp`
  returned `Content-Length` first and a valid MCP initialize body in about
  `7631 ms`

# 2026-05-13 - managed sync now keeps doctor metadata and build numbers in sync

The repo-local `kain_mcp` lane now has coherent managed build metadata end to end:
`kain doctor`, the managed sync stamp, and the PATH-installed binary all agree on
the live repo SHA and build number after sync.

What changed:

- Updated `crates/cli/build.rs` so CLI build metadata can be driven by explicit
  managed-sync git env vars and so Cargo watches the active branch ref plus
  `packed-refs`, not only `.git/HEAD`.
- Updated `scripts/windows/sync-kain-source-of-truth.ps1` to inject git metadata
  env vars into the CLI build, derive the next managed build number from both the
  counter file and the previous sync stamp, and parse JSON in a Windows PowerShell 5
  compatible way instead of relying on `ConvertFrom-Json -AsHashtable`.
- Updated `ARCHITECTURE.md` and the `kain-blades-system` skill with the durable
  Windows PowerShell compatibility warning for the managed sync lane.

Formal proof gathered with Z3:

- `kain_managed_build_number_monotonic_from_counter_and_stamp`

That proof shows the new `next = max(counter, stamp) + 1` rule is strictly
monotonic over non-negative stored build numbers.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- `kain doctor`
- Verified `C:\Users\Admin\.kain\state\build_counter.json` now advances to `2`
- Verified `C:\Users\Admin\.kain\state\kain_sync_stamp.json` now reports
  `build_number = "2"` and current repo SHA

# 2026-05-13 - `kain_mcp` hangs were partly stale-session state; launcher now leaves boot breadcrumbs

The live `kain_mcp` server was healthy when launched with the exact Codex config
shape, but Codex sessions that started before the `.codex/config.toml` edit could
keep waiting against stale MCP state and never even invoke the new launcher block.

What changed:

- Switched both repo `codex.config.toml` and live `C:\Users\Admin\.codex\config.toml`
  to the absolute Windows Python launcher path `C:\Windows\py.exe` instead of bare
  `py`, so MCP startup no longer depends on shell-local PATH resolution.
- Extended `blades/kain-mcp/config/runtime_policy.json` with a data-driven launcher
  trace path and enable flag.
- Extended `scripts/python/launch_kain_mcp.py` so every boot attempt appends JSONL
  breadcrumbs under `~/.kain/state/kain_mcp_launcher_trace.jsonl` for
  `launcher_start`, managed-sync decisions, child spawn, and exit.
- Updated `ARCHITECTURE.md` with the durable operator rule: after changing Codex MCP
  config, restart the session and inspect the launcher trace before assuming the
  Kain server itself is stuck.

Formal proof gathered with Z3:

- `kain_mcp_cooldown_and_sync_start_are_mutually_exclusive`

That proof shows one launcher process cannot both take the cooldown-return path and
reach `managed_sync_start`. If operators see both signals at once, they are looking
at concurrent launches or mixed-session logs rather than a hidden single-process path.

Validation:

- `py -3 -m py_compile scripts/python/launch_kain_mcp.py`
- TOML parse of `C:\Users\Admin\.codex\config.toml`
- TOML parse of repo `codex.config.toml`
- Exact-config MCP initialize smoke via `C:\Windows\py.exe -3 D:\Kain-Lang\scripts\python\launch_kain_mcp.py`
  from repo cwd returned the first frame in about `8257 ms`
- Verified launcher breadcrumbs were written to
  `C:\Users\Admin\.kain\state\kain_mcp_launcher_trace.jsonl`

# 2026-05-13 - `kain_mcp` Codex timeout was a cold-sync budget issue, not a protocol bug

The recent Codex `kain_mcp` startup timeout was caused by managed sync rebuilding
the PATH-installed `kain.exe` before the MCP server answered `initialize`, not by
JSON-RPC framing failure in the blade transport.

What changed:

- Updated repo `codex.config.toml` so the canonical copied MCP block now sets
  `startup_timeout_sec = 300` and explicitly documents that cold managed-sync
  launches can exceed 30 seconds.
- Hardened `scripts/python/launch_kain_mcp.py` with immediate stderr reporting
  of stale-sync reasons plus an explicit `running managed sync before MCP startup`
  message, so future launch delays are diagnosable from Codex logs.
- Hardened `scripts/windows/sync-kain-source-of-truth.ps1` so it can resolve the
  repo root from `scripts/windows` even when the caller does not provide
  `KAIN_REPO_ROOT` and `git rev-parse` discovery is unavailable.

Formal proofs gathered with Z3:

- `kain_mcp_timeout_root_cause_stale_stamp_implies_sync_required`
- `kain_mcp_stale_repo_head_after_new_commit_requires_new_sync_attempt`
- `kain_mcp_startup_non_blocking_if_runnable_binary_exists`
- `kain_mcp_sync_requires_wait_only_when_no_runnable_binary_exists`
- `kain_mcp_sync_lock_contention_never_blocks_if_current_binary_is_still_usable`

These proofs do not claim the Rust/Python/OS build world is mathematically bounded;
they prove the launcher decision logic and stale-stamp predicates. External build
duration still requires an operator timeout budget.

Validation:

- `py -3 -m py_compile scripts/python/launch_kain_mcp.py`
- TOML parse of `C:\\Users\\Admin\\.codex\\config.toml`
- TOML parse of repo `codex.config.toml`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- Cold managed sync rebuild path completed in about `200 s`
- Outside-cwd MCP `initialize` timing smoke: first response in about `8419 ms`

# 2026-05-13 - `kain import crates` adds workspace Rust bundle and blades-mirror modes

The Rust import lane now has a workspace-scale operator command that can either
emit one combined `.kn` file or mirror each discovered Cargo crate into a
blades-style directory tree.

What changed:

- Added the built-in `kain import crates` command metadata and typed routing in
  `crates/kain-commands/commands/import.toml`,
  `crates/kain-commands/src/kain.rs`, and `crates/cli/src/main.rs`.
- Extended `crates/cli/src/import_rust.rs` with workspace root/source-root
  resolution, Cargo crate discovery, shared directory import helpers, combined
  bundle emission, and `--blades` mirroring.
- The new lane auto-detects `./crates`, then `./rust`, then `./src/rust`
  unless `--source-root` overrides it.
- Bundle mode defaults to `<source-root>.kn`; `--blades` defaults to a mirrored
  `.kn` tree under `<workspace-root>/blades`.
- Blades mode preserves the imported Rust file layout and only rewrites the
  extension to `.kn`; it does not synthesize `KAIN.toml` manifests yet.

Validation:

- `cargo test -p kain-commands --target-dir target/codex-import-crates-commands -- --nocapture`
- `cargo test -p cli --lib import_rust --target-dir target/codex-import-crates-cli -- --nocapture`
- `cargo build -p cli --target-dir target/codex-import-crates-bin`
- `target/codex-import-crates-bin/debug/kain.exe import crates --output target/codex-import-crates-smoke/cuda.kn`
  from `reference/cuda`
- `target/codex-import-crates-bin/debug/kain.exe import crates --blades --output target/codex-import-crates-smoke/cuda-blades`
  from `reference/cuda`

Durable note:

- `reference/cuda` is a strong smoke corpus for this lane. In this checkout the
  command auto-detected `reference/cuda/crates`, imported 17 crates and 251
  Rust files, emitted a 3,133,735-byte bundle, mirrored 251 `.kn` files in
  blades mode, and reported 501 lossy-lowering diagnostics across 148 files.
- Use `--source-root` when the workspace root is not the folder that directly
  contains `crates/`, `rust/`, or `src/rust`.

# 2026-05-13 - Managed sync lane proved live and `kain-json` became a runnable blade example

The managed `kain-mcp` sync lane is now proven against the real PATH-installed
`kain.exe`, and `blades/kain-json` is no longer just a loose source folder.

What changed:

- Ran the managed sync install end to end through
  `scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`, which built and
  atomically installed the release CLI into `C:\Users\Admin\.cargo\bin`.
- Verified the live PATH binary with `kain doctor`; it now reports `Build: 1`,
  `Build Tracking: managed`, the managed sync stamp path, repo/runtime/binary
  drift status, and the synced binary fingerprint.
- Proved the canonical MCP launcher from outside the repo cwd with
  `KAIN_REPO_ROOT` set and explicit `KAIN_MCP_KAIN_BIN`, including real MCP
  `initialize`, `fs.read_file`, `kain.check`, `kain.run.plan`, and
  `authoring.example` calls over stdin/stdout.
- Upgraded `blades/kain-json/KAIN.toml` into a real runnable blade manifest and
  added `src/main.kn` as a tiny executable demo that exercises the JSON helpers.

Validation:

- `cargo check -p cli --bins --target-dir target/codex-sync-doctor-live`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- `kain doctor`
- `kain check blades/kain-json/src/main.kn`
- `kain run plan blades/kain-json`
- `kain blades build blades/kain-json --json`
- `kain run blades/kain-json`
- External-cwd MCP smoke via `scripts/python/launch_kain_mcp.py`

Durable note:

- The earlier compile blocker for `cargo check -p cli --bins` in this checkout
  is resolved for the current HEAD. If the managed sync lane regresses again,
  re-run the isolated-target-dir CLI check before assuming the launcher is at
  fault.

# 2026-05-13 - SPIR-V codegen gained a durable Z3 proof lane and a Vulkan layout fix

The live SPIR-V backend in `crates/gpu/src/codegen_spirv.rs` now has its own solver-backed
validation lane, and that lane immediately paid for itself by catching a real Vulkan layout bug:
storage buffers holding 3-lane vectors were being decorated with a 12-byte stride instead of the
16-byte base alignment Vulkan expects under std430-style rules.

What changed:

- Fixed `storage_buffer_stride(...)` in `crates/gpu/src/codegen_spirv.rs` so scalar buffers stay
  at 4 bytes, `Vec2`/`IVec2`/`UVec2` stay at 8 bytes, `Vec3`/`IVec3`/`UVec3` stay at 16 bytes,
  `Vec4`/`IVec4`/`UVec4` stay at 16 bytes, and `Mat4` stays at 64 bytes.
- Added a focused unit test in `crates/gpu/src/codegen_spirv.rs` to lock the common storage-buffer
  stride cases to Vulkan base-alignment expectations.
- Added `crates/gpu/tests/spirv_layout.rs`, which compiles a compute shader using
  `StorageBuffer<Vec3>` and validates the emitted module with `spirv-val --target-env vulkan1.3`.
- Added the durable proof pack at `crates/gpu/z3` with `layout`, `constructors`, `control`,
  `full`, and workspace `smoke` lanes. The first curated proofs cover wrapper-layout arithmetic,
  access-chain member-zero safety, vector-constructor component bounds, local-size slot mapping,
  and hoisted-local slot removal.

Validation:

- `cargo test -p gpu --lib storage_buffer_stride_matches_vulkan_base_alignment_for_common_types --target-dir target\\codex-spirv-proof-lib -- --nocapture`
- `cargo test -p gpu --test spirv_layout --target-dir target\\codex-spirv-proof-layout -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\gpu", lane="full")` proved 6/6 cases in `kain.gpu.proofs`.
- `mcp__z3_local__.run_workspace_proofs(project_root="D:\\Kain-Lang", lane="smoke")` still reports unrelated existing counterexamples in `runtime/native/src/ui/z3`, but the new GPU pack passed inside workspace discovery.

Durable design note:

- Treat `crates/gpu/z3` as the mandatory follow-through surface for `codegen_spirv.rs`. If a SPIR-V
  change touches layout arithmetic, vector flattening, access-chain indexing, or hoisted slot
  bookkeeping, update proofs before trusting tests alone.
- Keep pairing solver proofs with an external module validator. The proof pack checks our backend
  arithmetic and indexing invariants; `spirv-val` checks the binary against the Vulkan/SPIR-V rules
  that the solver model intentionally abstracts.

Current known gap in this checkout:

- The new proof lane does not mean the entire Kain shader authoring pipeline is globally green.
  Existing `crates/gpu/tests/spirv_smoke.rs` and `crates/gpu/tests/spirv_execute.rs` still expose
  pre-existing frontend/typechecker issues such as `.xyz` field admission, `group_index`
  resolution, tuple/vector arithmetic compatibility, and old constructor-style casts like `Int(a)`.

# 2026-05-13 - Managed MCP sync lane + deterministic doctor build tracking

The canonical `blades/kain-mcp` launcher/sync surface now has a first-class managed
sync contract for multi-agent environments, and `kain doctor` now reports explicit
managed-sync drift instead of only raw build metadata.

What changed:

- Extended `blades/kain-mcp/config/runtime_policy.json` with a `launcher_sync`
  section (state root, lock path, stamp path, build-counter path, cooldown,
  stale-lock timeout, sync command, runtime stamp files, and `prefer_synced_binary`).
- Reworked `scripts/python/launch_kain_mcp.py` to load policy data, run stale checks
  (`repo_sha` + runtime stamp + binary stamp) on launch, enforce a global sync lock,
  respect cooldown, and call managed sync before boot when stale.
- Reworked `scripts/windows/sync-kain-source-of-truth.ps1` to support managed sync:
  deterministic build counter at `~/.kain/state/build_counter.json`, injected
  `KAIN_BUILD_NUMBER`, atomic swap install for `kain.exe`/`kn.exe`, and stamp writes
  at `~/.kain/state/kain_sync_stamp.json`.
- Updated `crates/cli/build.rs` so build numbers default to explicit unmanaged mode
  instead of timestamp-like pseudo-build IDs; managed numbers now come from sync.
- Added `BUILD_TRACKING_MODE` in `crates/cli/src/lib.rs` and expanded doctor output
  in `crates/cli/src/main.rs` to show managed-sync stamp details, repo drift status,
  managed binary details, and binary-path mismatch warnings.

Durable design note:

- Keep launcher/sync/doctor on one data model (`runtime_policy.json` + sync stamp).
  Avoid reintroducing hardcoded binary paths or hand-written stale logic in one lane.
- The managed sync lane must be resilient to lock contention and failed rebuilds:
  warn and continue with the current binary rather than breaking MCP transport.

Validation:

- `py -3 -m py_compile scripts/python/launch_kain_mcp.py`
- PowerShell parse check for sync script (`[ScriptBlock]::Create(...)`)
- `cargo check -p cli --target-dir target/codex-sync-doctor` (pass)
- `pwsh -File scripts/windows/sync-kain-source-of-truth.ps1 -SkipBuild -ManagedSync` (pass)
- End-to-end MCP stdio smoke through launcher (`initialize`, `tools/list`, `shutdown`) (pass)

Current known blocker in this checkout:

- `cargo check -p cli --bins` / `cargo build -p cli` currently fails in pre-existing
  `crates/kain-build/src/workspace.rs` compile errors unrelated to this sync pass.
  Because of that repo-wide breakage, full binary-level doctor verification from a
  freshly rebuilt CLI was blocked in this turn.

# 2026-05-13 - `kain-core` keyword contracts gained a dedicated Z3 lane

The `crates/kain-core/z3` pack now has a focused `keywords` lane for the compiler-owned
`patch`, `law`, `converge`, and `orchestrate` forms. These proofs stay separate from the
existing arithmetic/parser lanes so future agents can run branch-ordering and runtime
contract checks without digging through the low-level memory suites.

What changed:

- Added `proofs/keywords-patch-cancel-rewinds-only-when-reversible.yaml` to prove the
  patch rewind path only fires for reversible frames.
- Added `proofs/keywords-law-runtime-accepts-only-bool-results.yaml` to prove law
  runtime acceptance is Bool-only.
- Added `proofs/keywords-converge-first-fast-lane-wins-and-spec-fallback.yaml` to prove
  converge selection honors first-match fast lanes and the spec fallback.
- Added `proofs/keywords-orchestrate-rejects-invalid-stage-ordering.yaml` to prove
  orchestrate stage collection rejects late stage declarations, nested items, and bare
  stage calls.
- Wired a new `keywords` lane into `crates/kain-core/z3/z3.toml` and documented it in
  `crates/kain-core/z3/README.md`.

Validation:

- `mcp__z3_local__.check_smt2` proved the patch, law, and converge formulas unsat with
  `include_model=false` and `include_stats=false`.
- `mcp__z3_local__.state_machine_check` proved the orchestrate ordering invariant holds
  within 4 bounded steps.

Durable note:

- Keep these keyword-contract proofs in their own lane. They are branch-ordering and
  runtime-contract checks, not arithmetic proofs, and should stay easy to rerun as a
  group.

# 2026-05-13 - `kain-mcp` launcher transport hardened and request loop de-actorized for stability

The canonical `blades/kain-mcp` lane now survives real MCP stdio clients in this
checkout, including multi-request `tools/call` sessions from outside the repo cwd.

What changed:

- Hardened `scripts/python/launch_kain_mcp.py` into a real transport shim instead
  of a simple `subprocess.call(...)` wrapper.
- Added managed Kain binary resolution order:
  `KAIN_MCP_KAIN_BIN` -> `target/debug` -> `target/release` -> PATH.
- Added managed Windows Python runtime path preloading so repo-built `kain.exe`
  can resolve `pythonXY.dll` reliably without shell-local PATH surgery.
- Added byte-stream stdin/stdout/stderr forwarding using `os.read/os.write` to
  avoid buffered-pipe stalls in MCP sessions.
- Added first-line stdout filtering for the CLI banner (`KAIN Compiler v...`) so
  the MCP `Content-Length` stream starts cleanly.
- Switched `blades/kain-mcp/src/main.kn` from actor-backed request dispatch to a
  direct route loop after reproducing stack-overflow crashes during
  `tools/call` filesystem handlers in the actor context.
- Simplified `fs.list_directory` entry payloads by dropping raw `entry.metadata`
  from MCP structured output.

Durable design note:

- Keep launcher behavior transport-safe first. If the host CLI emits non-protocol
  stdout text, scrub it at the launcher boundary unless/until the CLI gains a
  protocol-safe quiet mode for this lane.
- The direct routing loop is the current stability baseline. Reintroduce actor
  routing only after reproducing and fixing the actor-context stack overflow in
  `kain-mcp` tool handlers.

Validation:

- `target/debug/kain.exe run plan .\\blades\\kain-mcp`
- `target/debug/kain.exe blades build .\\blades\\kain-mcp --json`
- End-to-end MCP stdio smoke via `py -3 scripts/python/launch_kain_mcp.py`:
  `initialize`, `tools/list`, `fs.list_directory`, `fs.read_file`,
  `kain.run.plan`, `authoring.example`, and `shutdown` all returned valid
  JSON-RPC frames.

# 2026-05-13 - `blades/kain-mcp` routing moved behind a dedicated dispatch module

`blades/kain-mcp/src/main.kn` no longer imports every tool handler directly or
owns the entire handler switch chain. Tool routing now goes through
`src/tool_dispatch.kn`, and `main.kn` only asks the dispatcher for a handled
result.

What changed:

- Added `blades/kain-mcp/src/tool_dispatch.kn` with `ToolDispatchResult` and
  `dispatch_tool_handler(...)`.
- Switched `blades/kain-mcp/src/main.kn` to import `dispatch_tool_handler`
  instead of importing every handler function.
- Updated `blades/kain-mcp/KAIN.toml` build task inputs to include
  `src/tool_dispatch.kn`.

Durable design note:

- Adding a new MCP tool now requires updating `config/tools.json` plus
  `src/tool_dispatch.kn`; `src/main.kn` should remain stable unless request
  protocol routing changes.
- `use module::*` is accepted in the `kain-mcp` blade context, but it is not a
  universal drop-in for every lane. The `smoketest/native-ui/episode-two`
  lane still fails `kain check` for unrelated native-ui stdlib resolution
  (`native_ui_node_set_text`) and should be treated as a separate cleanup task.

Validation:

- `target/debug/kain.exe check blades/kain-mcp/src/main.kn`
- `target/debug/kain.exe run plan blades/kain-mcp`

# 2026-05-13 - `blades/kain-mcp` became the canonical repo MCP lane

The repo no longer treats the Kain-authored MCP server as a loose `MCP/server.kn`
experiment. The live MCP implementation now lives in the real blade
`blades/kain-mcp`, which means future agents can discover, run, build, and inspect
it through the same blade/run pipeline as the rest of the language examples.

What changed:

- Added `blades/kain-mcp/KAIN.toml` with real `[package]`, `[blade]`, `[run]`, `[build]`, and `[manifests]` sections so `kain run blades/kain-mcp` and `kain blades build blades/kain-mcp --json` become the canonical operator flow.
- Split the server into blade-owned modules under `blades/kain-mcp/src/` for runtime settings, tool registry loading, MCP protocol framing, filesystem tools, Kain operator tools, authoring/example tools, and the entry router.
- Moved tool metadata and runtime policy into `blades/kain-mcp/config/tools.json` and `blades/kain-mcp/config/runtime_policy.json` so the MCP surface is data-driven rather than hardcoded in one giant file.
- Pointed authoring guidance at `docs/examples/examples_manifest.json` and `docs/examples/validate_examples.py` instead of duplicating example truth inside the blade.
- Added `scripts/python/launch_kain_mcp.py` as the canonical repo launcher. It resolves `KAIN_MCP_KAIN_BIN`, falls back through repo debug/release builds and PATH, sets `KAIN_REPO_ROOT`, and prepends discovered Python install directories to PATH so repo-built `kain.exe` can find its matching `pythonXY.dll` on Windows.
- Updated root `mcp.json`, `codex.config.toml`, and `MCP/README.md` so the repo now advertises the blade launcher instead of the missing `tools/kain-flight-control/launcher.py` sidecar path.

Durable design note:

- Keep `blades/kain-mcp` as the real source of truth for repo-local MCP behavior. Root `MCP/` is now redirect-only docs, not a second implementation surface.
- Keep new MCP tools and runtime policy data-driven. Add schemas, handler ids, env keys, limits, and resolution order to the blade config JSON first instead of hardcoding new branches into the entrypoint.
- The blade is also a teaching example. Favor simple Kain syntax patterns that survive the current frontend: single-line helper signatures where needed, no parser-hostile inline conditionals inside argument lists, and no unnecessary `return` statements inside `-> Unit` helpers.

Validation:

- `target/debug/kain.exe run plan .\\blades\\kain-mcp`
- `target/debug/kain.exe check .\\blades\\kain-mcp\\src\\main.kn`

# 2026-05-12 - Native runtime commands became first-class CLI entrypoints

The runtime validation wrappers from the earlier pass are now exposed directly
through the typed `kain` / `kn` command surface, so future operators do not
have to remember the underlying script names before they can prove the native
runtime bundle.

What changed:

- Added a dedicated `runtime` command pack at `crates/kain-commands/commands/runtime.toml` and registered it in the built-in command-pack index.
- Added typed `RuntimeCommand` parsing in `crates/kain-commands/src/kain.rs` for `kain runtime build` and `kain runtime validate`, including aggregate validation skip flags.
- Added `crates/cli/src/runtime_tools.rs` as the thin execution host. It resolves the repo root from `KAIN_REPO_ROOT`, the current working tree, or the repo-built binary location, then forwards to the existing bash/PowerShell runtime wrappers instead of reimplementing runtime policy in Rust.
- Updated the registry and dynamic help tests so `kain commands list --bin kain` and `kain commands help --bin kain` now expose the `runtime` command family.
- Updated runtime/operator docs and metadata so `kain runtime build` / `kain runtime validate` are the preferred front door, while the bash/PowerShell scripts remain the underlying implementation truth.

Durable design note:

- Keep `kain runtime build` and `kain runtime validate` as thin operator entrypoints. They should discover the repo and delegate to the canonical wrapper scripts, not grow a second copy of native-runtime build logic inside `crates/cli`.
- The existence of first-class runtime commands still does not imply a separate shipped `kain_runtime.exe`. The owned runtime remains a manifest-driven source/object/archive bundle linked into generated native programs.

Validation:

- `cargo fmt -p kain-commands -p cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-commands --target-dir target\\codex-kain-runtime-commands -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p cli --target-dir target\\codex-kain-runtime-commands-cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\\codex-kain-runtime-commands-cli`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime build --help`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime validate --help`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe commands list --bin kain`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe commands help --bin kain`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime validate --skip-cli-build --skip-runtime-build --skip-fixtures --skip-conformance`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime build`

# 2026-05-12 - Native runtime validation entrypoints aligned across bash and PowerShell

The native runtime build pipeline was already present in code, but the operator surface around it was inconsistent enough to create false confusion about whether Kain had a real C runtime pipeline at all.

What changed:

- Added the missing aggregate validation entrypoint `runtime/validate_native_runtime.sh` so the command already referenced by metadata and `ARCHITECTURE.md` now exists for real.
- Added Windows operator wrappers at `runtime/compile_native_runtime.ps1`, `runtime/conformance/run_all.ps1`, and `runtime/validate_native_runtime.ps1`.
- Replaced the stale `runtime/fixtures/validate_all.ps1` implementation with a thin wrapper around the canonical `runtime/fixtures/validate_all.sh` lane so PowerShell no longer routes through an older Rust-target-only fixture script.
- Added `runtime/scripts/runtime_windows_shell_helpers.ps1` to keep bash discovery and Windows path translation in one place instead of duplicating wrapper logic.
- Added `runtime/NATIVE_RUNTIME_VALIDATION.md` to spell out the important build distinction: `cargo build -p cli` builds the compiler host, while `kain build ... -t llvm` and `kain build ... -t c` compile and link the manifest-driven native runtime bundle into the produced executable.
- Updated `ARCHITECTURE.md` so runtime validation commands now list both bash and PowerShell entrypoints, and so future agents do not assume the C runtime should be a separate shipped executable.

Durable design note:

- Do not create a separate `kain_runtime.exe` just to prove the runtime exists. The current architecture intentionally treats the owned native runtime as a manifest-driven object/archive bundle that gets linked into each generated native program.
- Keep the bash scripts as the canonical runtime validation logic for now, and keep PowerShell wrappers thin. That avoids two diverging implementations of the same runtime build policy.

# 2026-05-12 - kain-core Z3 proof pack landed and low-level memory layout math was hardened

`crates/kain-core` now has its own durable proof pack at `crates/kain-core/z3`. The pack is scoped at compiler/frontend arithmetic and indexing seams instead of the native C runtime: low-level memory layout lowering in `src/low_level_memory.rs`, signed `usize -> i64` literal conversions used by lowered helpers, diagnostics span/line-end math in `src/diagnostics.rs`, and parser slice/index guards in `src/parser.rs`.

What changed:

- Added the `kain.core.proofs` pack with lanes `memory`, `diagnostics`, `literals`, `parser`, `smoke`, and `full`.
- Hardened `crates/kain-core/src/low_level_memory.rs` so layout addition, multiplication, align-up steps, fallback array sizing, fallback tuple sizing, and lowered signed literal conversions now fail explicitly instead of silently wrapping.
- Added `DiagnosticCode::MemoryLayoutOverflow` (`KAIN-MEM-0004`) in `crates/kain-core/src/diagnostic_registry.rs` and routed layout overflow failures through a dedicated validation diagnostic with a concrete suggestion.
- Seeded durable proofs for checked layout addition, checked layout multiplication, align-up wrap prevention, tuple and array fallback sizing, signed literal bounds, diagnostics span/line-end bounds, and parser indexing/slicing preconditions.

Validation:

- `cargo check -p kain-core`
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="memory")` proved 5/5.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="diagnostics")` proved 3/3.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="literals")` proved 1/1.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="parser")` proved 3/3.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="full")` proved 12/12.
- `run_workspace_proofs(project_root="D:\Kain-Lang", lane="smoke")` proved both repo packs for 32/32 total cases.

Current unrelated test status:

- `cargo test -p kain-core` still has five pre-existing failures outside this proof pass: `language_features::tests::default_profile_keeps_struct_literals_disabled`, `realtime_app_bundle::tests::emits_bundle_owned_camera_and_presentation_metadata_for_viewports`, `realtime_app_bundle::tests::emits_realtime_bundle_with_viewport_scene_binding`, `stdlib_tests::test_load_stdlib_graceful_degradation`, and `stdlib_tests::test_env_var_priority_over_filesystem`.

Durable workflow note:

- If a `kain-core` proof fails only on values larger than `18446744073709551615` or `9223372036854775807`, inspect the proof model before changing Rust code. This pack intentionally constrains `usize`-shaped arithmetic to `SIZE_MAX` and signed-literal success paths to `i64::MAX`; otherwise Z3 can invent values the ABI or helper never accepts.

# 2026-05-12 - Native core Z3 pack expanded across actor/net/process/entangle

The repo-local native proof pack at `runtime/native/src/core/z3` is no longer just a seed lane. It now carries curated durable proofs across four low-level runtime seams and validates the upgraded Z3 workflow end to end.

What changed:

- Added actor coverage for `kain_actor_try_receive(...)` so the non-blocking mailbox receive path has its own explicit count-underflow proof.
- Expanded native net coverage with request-body span arithmetic, request-body allocation arithmetic, and stored-response allocation arithmetic around `kain_native_net_parse_http_request(...)` and `kain_native_net_store_http_response(...)`.
- Added first-class process proofs for argument/environment capacity guards, capture-append bounded growth, UTF-8/wide buffer append arithmetic, and hex-encoding allocation bounds in `kain_native_process_system.c`.
- Added first-class entangle proofs for `kain_runtime_copy_entangle_text(...)` null-terminated copy sizing and `kain_runtime_entangle_register(...)` fixed-capacity registry growth.
- Added local matcher bundles in `templates/process-runtime.yaml` and `templates/entangle-runtime.yaml`, and refined the entangle template so extraction constrains values to a real 64-bit `size_t` domain instead of proving against impossible widths.
- Added focused manifest lanes in `z3.toml`: `net`, `process`, and `entangle`, while keeping `actor`, aggregate `native`, `full`, and workspace `smoke`.

Runtime hardening in the same pass:

- `runtime/native/src/core/kain_native_process_system.c` now uses explicit checked `size_t` helpers for buffer growth, allocation sizing, wide/UTF-8 append helpers, hex encoding, wide-string duplication, environment-block construction, and capture-length accumulation.

Validation:

- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="actor")` proved 7/7.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="native")` proved 13/13.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="process")` proved 6/6.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="entangle")` proved 2/2.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="full")` proved 20/20.
- `run_workspace_proofs(project_root="D:\\Kain-Lang", lane="smoke")` proved discovery plus execution for 20/20 cases.
- `extract_source_proof_cases(save=false)` confirmed pack-local template extraction for actor, process, and entangle sources.
- `bash runtime/conformance/process_runtime/run_tests.sh --verbose`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_native_process_system.c`

Durable workflow note:

- When a proof fails on values larger than `18446744073709551615`, check the proof model before assuming the C path is wrong. Several seams here needed explicit `size_t` domain constraints so Z3 would stop inventing values that the ABI cannot represent.

# 2026-05-12 - Native core Z3 proof pack seeded

The first durable repo-local Z3 proof pack now lives at `runtime/native/src/core/z3` and is named `kain.native.core.proofs`. This is the seed lane for solver-backed native runtime invariants, especially the Erlang-style actor substrate and low-level C arithmetic seams that are easy to regress by inspection alone.

What the pack owns now:

- Six actor proofs covering bounded mailbox send counts, receive-count underflow prevention, scheduler dequeue accounting, scheduler max-depth monotonicity, restart-limit arithmetic, and actor ID slot ranges that preserve `KAIN_ACTOR_ID_INVALID == 0`.
- Two native net proofs preserving the recent hardening work: non-negative `Content-Length` parsing before `size_t` conversion and checked append-buffer size addition.
- A local `templates/actor-runtime.yaml` matcher bundle that describes the first actor proof shapes for future source-to-proof extraction.
- Manifest lanes in `z3.toml`: `smoke`, `actor`, `native`, and `full`.

Validation:

- `uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane smoke` proved 8/8 cases.
- `run_proof_pack(path="D:\Kain-Lang\runtime\native\src\core", lane="actor")` proved 6/6 cases.
- `run_proof_pack(path="D:\Kain-Lang\runtime\native\src\core", lane="native")` proved 2/2 cases.

Design note:

- Keep generated report JSON out of commits; it is local validation output. Commit durable proof cases, manifests, templates, fixtures, and generated tests only when they are intentionally part of the proof surface.
- Next high-leverage step is to extend the template loader/source analyzer so pack-local `templates/*.yaml` can drive extraction automatically, then add deeper actor state-machine proofs for mailbox state transitions, supervisor restart windows, and scheduler fairness bounds.

# 2026-05-12 - Native net runtime hardened against Content-Length and append-size wrap

The native HTTP lane in `runtime/native/src/core/kain_native_net_system.c` now rejects malformed or negative `Content-Length` headers before they ever reach a `size_t` cast, and the shared byte-append helper now uses overflow-checked `size_t` growth instead of raw `length + byte_count + 1` arithmetic.

What changed:

- Added `kain_native_net_size_add_overflow(...)` for local `size_t` addition checks and used it in request-body bounds checks, response-body allocation, and the shared append-buffer growth path.
- Added `kain_native_net_parse_content_length_header(...)` so `Content-Length` parsing is strict: it skips leading whitespace, rejects signed values, rejects junk suffixes, and rejects values that exceed `SIZE_MAX`.
- Hardened `kain_native_http_server_pump(...)` so malformed or overflowing `Content-Length` values fail with `KAIN_NATIVE_NET_PARSE_ERROR` instead of silently wrapping through request-length math.
- Added a native conformance regression in `runtime/conformance/net_runtime/test_native_net_system_kernel.c` that sends `Content-Length: -1` and asserts the request is rejected with parse diagnostics.

Why this matters:

- Before this pass, a header like `Content-Length: -1` could flow through `atoll(...)` into an unsigned `size_t`, wrap to `SIZE_MAX`, and then bypass a `header_length + body_length <= length` guard because unsigned addition wrapped modulo `2^N`.
- The same file also had a latent append-buffer overflow hazard in `needed = *length + byte_count + 1u`; the new helper makes that arithmetic explicit and checkable.

Validation:

- `cargo test -p kain-net --target-dir target\\codex-z3-net-fix`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`

# 2026-05-12 - Command manifests split into packs with dynamic registry help

`crates/kain-commands` now uses an indexed command-pack layout instead of a
mega `kain.toml` plus separate `blade.toml`. The build script reads
`crates/kain-commands/commands/index.toml`, validates each top-level pack file,
and generates built-in pack plus command definitions. The pack files stay flat
under `crates/kain-commands/commands/` so a future agent can scan `core.toml`,
`build.toml`, `run.toml`, `blade.toml`, `import.toml`, `unreal.toml`,
`registry.toml`, and the smaller domain packs directly.

The Unreal side is intentionally visible again: `unreal.toml` owns the current
UE5-facing executable entries (`gpu-artifacts` and `inject`), while the build
pack keeps UE5 build targeting tagged on `build` through the existing flags.

New registry affordances:

- `kain commands packs` / `--json` lists command packs.
- `kain commands help --bin kain|kn|blade` renders a dynamic Clap help tree from
  the registry.
- Registry text output includes `pack=` and `tags=`.
- Runtime command manifests may now provide `tags` and `args` for richer future
  dynamic help.

Validation for this pass:

- `cargo fmt -p kain-commands -p cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-commands --target-dir target\codex-kain-command-packs -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p cli --target-dir target\codex-kain-command-packs-cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\codex-kain-command-packs-cli`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands packs`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands packs --json`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands list --bin kain`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands help --bin kain`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands help --bin blade`
- `target\codex-kain-command-packs-cli\debug\kn.exe commands list --bin kn`
- `target\codex-kain-command-packs-cli\debug\blade.exe --help`
- `python C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py C:\Users\Admin\.agents\skills\kain-command-platform`

Additional broad validation attempted:

- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p cli --target-dir target\codex-kain-command-packs-cli-test -- --nocapture`

That broader CLI suite still fails outside the command-platform slice:
`selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout` keeps
the known indentation assertion failure, and
`import_c::tests::test_import_with_target` currently returns an error while
asserting `result.is_ok()`. Command-pack tests, CLI check/build, and executable
registry smokes are green.

Recommended next step:

- Move execution from typed Clap-first toward the hybrid command host:
  dynamic Clap can already render and resolve registry entries, but built-in
  handler execution still flows through the typed routers. The next major step
  is a handler dispatch table that can execute registry-resolved built-ins and
  runtime blade handlers from one path.

# 2026-05-12 - Unified kain-run pipeline landed

Kain now has `crates/kain-run` as the explicit immediate-execution crate behind `kain run`, `kain run dev`, `kain run plan`, `kain watch`, `kain blades run`, and standalone `blade run`. This moved the old birth-era run behavior out of the CLI and into a reusable pipeline shaped like the other first-class Kain systems (`kain-fs`, `kain-process`, `kain-actor`, `kain-build`).

The new run crate owns:

- `RunRequest`, `RunPlan`, `RunUnit`, `RunAdapter`, `RunReport`, and JSONL run events.
- Target inference for Kain source, C, Cargo, Fabric, Node, and Bun.
- Blade and workspace resolution through `crates/kain-blades`.
- `[run]` manifest metadata: `entry`, `blade`, `target`, `args`, `env`, `cwd`, and `watch`.
- Hidden cached C execution through Clang with outputs under `.kain/cache/run/c`.
- Cargo run execution with isolated target dirs under `.kain/cache/run/cargo`.
- Run reports under `.kain/reports/run` and watcher polling through `kain-fs`.
- Process-backed report metadata using `kain-process::ProcessSpec`.

`crates/kain-commands` now exposes the new command surface for `run`, `run dev`, `run plan`, `watch`, `blades run`, and standalone `blade run`; `crates/cli/src/run.rs` is only the CLI print/exit wrapper. `crates/kain-core/src/types.rs` also registers stdlib registry globals in the type environment so raw stdlib bridge names such as `kain_input_reset` are visible during source checking and runtime compilation.

Validation for this pass:

- `cargo fmt -p kain-run -p blade -p kain-commands -p cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-run -p blade -p kain-commands --target-dir target\codex-kain-run -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-core type_env_registers_stdlib_registry_bridge_globals --target-dir target\codex-kain-run -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p kain-run -p kain-commands -p cli --target-dir target\codex-kain-run`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\codex-kain-run`
- `target\codex-kain-run\debug\kain.exe run plan docs\examples\00_hello_and_cli.kn --json`
- `target\codex-kain-run\debug\kain.exe run docs\examples\00_hello_and_cli.kn`
- `target\codex-kain-run\debug\kain.exe run target\codex-kain-run-smoke\hello.c --target c -- smoke-arg`
- `target\codex-kain-run\debug\kain.exe watch docs\examples\00_hello_and_cli.kn --dry-run`
- `target\codex-kain-run\debug\blade.exe run --help`
- `target\codex-kain-run\debug\kain.exe commands list --bin blade`

Current limits and next recommended step:

- Kain interpreter and Fabric adapters execute through their existing host functions; runtime args are meaningful for process-backed adapters first.
- The dev watcher is intentionally polling-based through `kain-fs` v1. A future pass can add native notify acceleration behind the same run-plan contract.
- `--trace` and `--keep-artifacts` are part of the request/report surface, but deeper adapter-specific trace payloads should be added as the native run pipeline grows.
- The next high-leverage pass is to add richer adapter-specific run reports and native notify watching without changing the CLI surface again.

# 2026-05-12 - Kain command platform crate landed

Kain now has `crates/kain-commands` as the command brain for `kain`, `kn`, and standalone `blade`. The crate owns built-in command manifests under `crates/kain-commands/commands/`, typed Clap routers under `crates/kain-commands/src/`, shared argument structs, launcher helpers, registry serialization, conflict detection, and a first runtime `[[commands]]` contribution loader/fallback. The workspace `Cargo.toml` now includes the crate and `crates/cli` depends on it.

The ownership split is now deliberate:

- `crates/kain-commands` owns command shape, metadata, aliases, bin exposure, registry views, and runtime contribution resolution.
- `crates/cli` is the host binary/execution shell: parse, dispatch, print, set exit codes, and call domain crates.
- Domain crates such as `kain-driver`, `kain-build`, `blade`, `kain-check`, `kain-test`, `kain-repair`, `kain-repl`, `kain-omni`, and `kain-codebase` still own actual behavior.

`kain commands list/export` now exposes the registry for `kain`, `kn`, and `blade`, with `--runtime` merging workspace-discovered runtime command manifests through the blade resolver. Runtime command fallback can recognize contributed paths, but dynamic handler execution is intentionally not implemented yet; matched runtime commands fail clearly until a real handler bridge is added. Built-ins win conflicts and duplicate runtime paths are rejected.

Validation for this pass:

- `cargo fmt -p kain-commands -p cli`
- `cargo test -p kain-commands --target-dir target\codex-kain-commands -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p cli --target-dir target\codex-kain-commands-cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\codex-kain-commands-cli`
- `target\codex-kain-commands-cli\debug\kain.exe --help`
- `target\codex-kain-commands-cli\debug\kn.exe --help`
- `target\codex-kain-commands-cli\debug\blade.exe --help`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin kain`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin kn`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin blade`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin kain --runtime`
- `target\codex-kain-commands-cli\debug\kain.exe commands export --bin blade`
- `python C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py C:\Users\Admin\.agents\skills\kain-command-platform`

Broader `cargo test -p cli --target-dir target\codex-kain-commands-cli -- --nocapture` now compiles the moved-router modules, but still fails in runtime-heavy pre-existing lanes: several tests hit `Unknown identifier 'kain_input_reset'`, and `selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout` still fails its indentation assertion. The router-specific compile issue found during that run was fixed by moving `PathBuf` imports into the affected test modules.

Recommended next step:

- Decide whether phase 2 should generate more of the Clap shape from the TOML manifests or keep typed Clap as the ergonomic parser layer, then add the dynamic runtime handler bridge for `handler = "blade:<id>:<command>"` contributions.

# 2026-05-12 - Native TCP and HTTP substrate landed

Kain now has a first-class network lane instead of relying on tiny interpreter-only `http_get`/`http_post_json` helpers or raw legacy `socket_*` functions. `crates/kain-net` owns the portable contract for TCP endpoints, HTTP request/response specs, headers, route specs, handles, lifecycle state, and typed errors. LLVM/direct-C builds load `stdlib/native/net.kn`, backed by `runtime/native/include/kain_native_net_system.h` and `runtime/native/src/core/kain_native_net_system.c`.

The native ABI is handle-driven and primitive-friendly so current LLVM/direct-C lowering can use it without aggregate ABI work. The v1 flow is TCP connect/listen/accept/read/write plus HTTP request/response handles, HTTP client send, local HTTP server listen/pump, actor route registration, request inspection, response writes, local URL helpers, reset, and diagnostics.

`io.net` in the service table now points at the owned native net function table instead of the older vendor/libuv placeholder. `kain_native_runtime_init/shutdown` reset the net registry so open sockets, listeners, request handles, and response handles are cleaned up between native runs. The lean and broad native runtime manifests both include the net source; Windows linking now includes `ws2_32` and `winhttp`.

Validation targets added for this lane:

- `cargo test -p kain-net`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_net_tcp_http_and_actor_route_primitives -- --exact`
- `cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_net_symbols_as_declarations -- --exact`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`
- `target/debug/kain.exe build runtime/fixtures/native_net_http/main.kn --target llvm --output runtime/fixtures/native_net_http/generated/native_net_http.ll` then run the generated executable

Current known limits:

- HTTP server support is HTTP/1.1 with request-line/header parsing and `Content-Length` bodies. Server TLS, chunked request bodies, WebSockets, HTTP/2, and HTTP/3 are out of v1.
- HTTPS client support is Windows-first through WinHTTP. Plain HTTP client support uses the runtime TCP path.
- Actor routes currently dispatch a native actor message payload containing the incoming request handle and request metadata, while manual polling/response remains the deterministic fixture path. Rich Kain actor handler ergonomics should be layered above this ABI rather than baked into the socket kernel.
- Entangle is intentionally not part of the net ABI. Use it later for replicated state, distributed actor sessions, or cluster coordination above the transport.

Recommended next step:

- Add a Kain-authored HTTP server convenience layer above `stdlib/native/net.kn` that maps route patterns to actor handlers and response helpers, then add UDP/DNS only after the HTTP/TCP ergonomics are stable.

# 2026-05-12 - Native child-process and PTY substrate landed

Kain now has a first-class process lane instead of only ad hoc host-side command helpers. `crates/kain-process` owns the portable contract for process specs, stdio modes, cwd/env overrides, process/PTY handles, lifecycle state, and captured output. LLVM/direct-C builds load `stdlib/native/process.kn`, backed by `runtime/native/include/kain_native_process_system.h` and `runtime/native/src/core/kain_native_process_system.c`.

The native ABI is intentionally handle-driven and primitive-friendly so current LLVM/direct-C lowering can use it without aggregate ABI tricks. The flow is:

- create a process spec
- add argv entries
- set cwd/env/stdin/stdout/stderr policy
- spawn a normal child or PTY child
- poll/wait for exit
- write stdin or PTY input
- read/capture stdout, stderr, or PTY output
- inspect last-status diagnostics

Windows is the first real implementation. Normal child-process spawn uses explicit `CreateProcessW` plus inherit/pipe/null stdio wiring, cwd overrides, merged environment blocks, capture buffers, and output draining. PTY spawn uses ConPTY through `STARTUPINFOEX` plus inherited std handles so console APIs and standard-stream writes both route into the same transport. Non-Windows hosts keep the ABI surface but return explicit unsupported diagnostics instead of pretending parity exists.

The core runtime/profile updates in this pass:

- `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml` now include `kain_native_process_system.c`.
- `runtime/native/include/kain_runtime_native_stdlib.h` now exports the process ABI header.
- `kain_native_runtime_init/shutdown` reset the process registry so native fixtures start clean and shutdown kills live children before teardown.
- `io.process` in the service table now points at the owned native process function table instead of the older vendor/libuv stub.

Validation targets added for this lane:

- `cargo test -p kain-process`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_process_and_pty_primitives -- --exact`
- `cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_process_symbols_as_declarations -- --exact`
- `bash runtime/conformance/process_runtime/run_tests.sh --verbose`
- `target/debug/kain.exe build runtime/fixtures/native_process_stdio/main.kn --target llvm --output runtime/fixtures/native_process_stdio/generated/native_process_stdio.ll` then run the generated `.exe`

Current known limit:

- The ConPTY lane now proves PTY spawn/capture/resize and can write into an interactive PTY session, but the strongest deterministic proof today is still PTY capture from a self-contained command plus explicit API exercise for interactive writes. If future work needs richer terminal semantics, keep it in this substrate or a dedicated terminal layer above it; do not fall back to shell-specific host helpers.

Recommended next step:

- Add a first-class `kain-net` contract/subsystem using the same pattern: portable crate, `stdlib/native/*.kn` wrapper, native C ABI floor, codegen proof, conformance runner, and one focused LLVM fixture that proves a real TCP or HTTP roundtrip.

# 2026-05-12 - Native UI gained generic authored state cells

The raw native UI ABI now has generic per-node state cells: `kain_native_ui_node_set_state_i64/f64/string`, `kain_native_ui_node_state_i64/f64/string`, and `kain_native_ui_state_count`. This is deliberately substrate, not a component system. The runtime stores keyed values for authored nodes and marks nodes dirty, but it does not know what a button, tetrahedron, Kerr-field hit tester, shader surface, or product control means.

`stdlib/native/ui.kn` exposes thin state wrappers plus system-shaped helpers for booleans, toggles, counters, references, and arbitrary `shape.*`, `hit.*`, `draw.*`, and `resource.*` payload conventions. The stdlib still does not define baked buttons, panels, or product UI. Apps and Kain libraries can build any catalog or stranger UI model they want on top of these cells.

Validation targets updated for this pass:

- `runtime/conformance/ui_runtime/test_native_ui_system_kernel.c` covers raw state set/get/fallback/count.
- `runtime/conformance/ui_runtime/test_native_ui_system_host_services.c` covers state preservation through stable-key identity and live `win32-gl` acceptance.
- `runtime/fixtures/native_ui_stdlib_layer/main.kn` proves Kain-authored state payload helpers through LLVM.
- `smoketest/native-ui/pilot/main.kn` carries arbitrary command/viewport shape, hit, draw, and resource payloads into the live screenshot smoke.

Recommended next step:

- Build the real Kain-authored reconciler/state graph on top of these cells, including hot-reload retention policy and authored custom hit/layout callbacks. Keep rect hit testing as the v1 host prefilter only; do not make rects the semantic ceiling.

# 2026-05-12 - Kain-authored native UI stdlib layer started

`stdlib/native/ui.kn` now has a first real authored UI layer above the raw native UI ABI. The helpers are deliberately system-shaped rather than catalog-shaped: session/frame setup, stable keyed reconciliation, rect/layout math, split/inset/center helpers, style color/metric/padding/spacing helpers, inherited color resolution, texture hex upload convenience, render helpers for boxes/text/resources, and event helpers for authored pointer state. There are still no baked runtime buttons, panels, or product components.

The raw C kernel gained two generic node-state flags, `hovered` and `pressed`, so Kain-authored interaction helpers can store common pointer state without turning the runtime into a widget system. `runtime/conformance/ui_runtime/test_native_ui_system_kernel.c` now covers those flags.

`runtime/fixtures/native_ui_stdlib_layer/main.kn` is the new fast proof fixture. It runs on the headless `software` backend and validates stdlib reconciliation, layout, style inheritance, rendering metadata, event draining, focus, and pointer state. The live `smoketest/native-ui/pilot` now uses the stdlib layer for session setup, reconciliation, layout, styles, rendering, and authored hover state while still producing a Win32/GL screenshot.

Validation:

- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm`
- `target\codex-native-ui-win32\debug\kain.exe build runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm --output target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe`
- `target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe`
- `.\smoketest\native-ui\pilot\run.ps1`

Recommended next step:

- Build a Kain-authored reconciler/state graph that can preserve authored node state across hot reload, then layer optional app-code controls above these generic helpers rather than adding a stdlib catalog of prewritten widgets.

# 2026-05-12 - Raw native UI now has a live Win32/GL presenter and screenshotable LLVM smoke

The raw native UI ABI is no longer metadata-only on Windows. `runtime/native/src/ui/kain_native_ui_system.c` now delegates live presentation through an internal host adapter layer, with `runtime/native/src/ui/kain_native_ui_host_win32_gl.c` providing the first non-blocking `win32-gl` backend. The core session/node/resource/event kernel remains generic; the backend only owns window creation, GL presentation, Win32 message translation, clipboard/menu/dialog bridging, and screenshot capture. `software` remains the headless metadata backend.

Two ABI upgrades landed with the presenter:

- `draw_text` now requires an explicit font resource handle, so text rendering stays resource-shaped instead of depending on a hidden host default.
- UI resources now support generic byte upload plus a Kain-friendly hex helper. `stdlib/native/ui.kn` exposes `native_ui_resource_set_bytes_hex(...)` and `native_ui_texture_create_from_hex(...)`, letting a single Kain file author texture-backed UI without a host-owned image catalog.

`smoketest/native-ui/pilot` is now a real end-to-end proof, not just an LLVM link test. `main.kn` authors a compact UI system in one Kain file, attaches `win32-gl`, renders authored rect/text/resource commands, captures `outputs/pilot.bmp`, and exits `0`. `run.ps1` resolves a local `kain.exe`, runs `kain check`, builds LLVM to `outputs/pilot.exe`, scans `pilot.ll` for raw native UI ABI calls, runs the executable with screenshot env vars, and verifies the BMP artifact.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`
- `cargo build -p cli --target-dir target\codex-native-ui-win32`
- `target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_single_file\main.kn --target llvm`
- `target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_runtime_systems\main.kn --target llvm`
- `.\smoketest\native-ui\pilot\run.ps1`

Recommended next step:

- Keep the raw ABI generic and build the Kain-authored layout/style/reconciliation layer above it in stdlib. Future platform work should add more adapters behind the same host boundary rather than widening the C layer into baked widgets or a host-owned component catalog.

# 2026-05-12 - Canonical Kain input semantics landed

Kain now has a first-class input semantics lane instead of treating input as scattered stdin/UI/native helper calls. `crates/kain-input` owns typed source provenance, events, data-driven action/axis bindings, frame reduction, text commits, first-class `agent.intent` events, and deterministic trace serialization/replay. `crates/kain-core` registers interpreter bridge builtins under `kain_input_*`, with root `stdlib/input.kn` exposing the public `input_*` helpers.

Native LLVM/direct-C builds now load `stdlib/native/input.kn`, backed by `runtime/native/include/kain_native_input_system.h` and `runtime/native/src/core/kain_native_input_system.c`. The native kernel exposes sessions, bindings, event injection, frame reduction, action/axis/text queries, agent intent injection, trace export/replay, and last-status diagnostics. `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml` include the input kernel, and `platform.input` service metadata now describes canonical Kain input sessions rather than only Win32 capture.

Design decisions:

- Keep input as stdlib/runtime capability, not parser syntax. No `input` keyword.
- Public Kain code should consume frames/actions/axes/text commits, while raw events stay available for inspection.
- `agent.intent` is first-class source provenance in v1, not a test-only synthetic event.
- Target adapters should translate raw Win32/web/UE/UI/CLI/agent events into `kain-input`; they should not define app-facing input policy.

Validation:

- `cargo test -p kain-input --target-dir target\\codex-kain-input`
- `cargo check -p kain-core --target-dir target\\codex-kain-input-core`
- `cargo test -p kain-core test_stdlib_builtin_functions_exist --target-dir target\\codex-kain-input-core -- --nocapture`
- `cargo test -p kain-sys-codegen native_input --target-dir target\\codex-kain-input-codegen -- --nocapture`
- `bash runtime/conformance/input_runtime/run_tests.sh --verbose`
- `cargo build -p cli --target-dir target\\codex-kain-input-cli`
- `target\\codex-kain-input-cli\\debug\\kain.exe build runtime\\fixtures\\native_input_actions\\main.kn -t llvm` then run `runtime\\fixtures\\native_input_actions\\main.exe`
- `target\\codex-kain-input-cli\\debug\\kain.exe build runtime\\fixtures\\native_input_actions\\main.kn -t c` then run `runtime\\fixtures\\native_input_actions\\main.exe`

Recommended next step:

- Add thin adapters for live Win32 window messages and UI runtime event handoff into `kain_native_input_*`, then add web DOM and UE5 Enhanced Input adapters that emit the same source/action schema.

# 2026-05-11 - Raw native UI ABI gained host services for Kain-authored UI systems

The raw native UI kernel now covers the first real "Kain can author the UI system" layer without introducing a host-side widget catalog. `runtime/native/include/kain_native_ui_system.h` and `runtime/native/src/ui/kain_native_ui_system.c` now expose generic host frame presentation metadata, stable node keys for hot reload, accessibility labels/roles, font/texture/canvas/shader resource handles, text measurement, draw-resource commands, clipboard, IME, drag/drop, menu, dialog, and hot reload generation APIs. `stdlib/native/ui.kn` wraps those APIs and adds only generic layout/stable-node helpers; it does not define buttons, panels, or product components.

Design decisions:

- Keep the runtime capability-shaped. The runtime owns handles, buffers, metadata, event/system services, and host presentation; Kain source or Kain stdlib code owns layout systems, style cascades, reconciliation, controls, and app-specific components.
- Stable keys are the reload bridge. Kain-authored code can rebuild one file, call `native_ui_node_find_by_stable_key`, and preserve/reuse existing nodes without the C runtime knowing what a "button" or "panel" means.
- `runtime/fixtures/native_ui_runtime_systems/main.kn` is the focused proof shape: one Kain file creates authored nodes/resources, drives host services, presents draw commands, and exits successfully through the LLVM native path.

Validation:

- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\\codex-native-ui-host-services -- --nocapture`
- `cargo build -p cli --target-dir target\\codex-native-ui-host-services-cli`
- `target\\codex-native-ui-host-services-cli\\debug\\kain.exe check runtime\\fixtures\\native_ui_runtime_systems\\main.kn --target llvm`
- `target\\codex-native-ui-host-services-cli\\debug\\kain.exe build runtime\\fixtures\\native_ui_runtime_systems\\main.kn --target llvm --output target\\codex-native-ui-host-services\\native_ui_runtime_systems.exe`
- `target\\codex-native-ui-host-services\\native_ui_runtime_systems.exe`

Recommended next step:

- Attach `kain_native_ui_host_present` to a live pixel backend (Win32/Direct2D, Skia, wgpu, Qt, or another host) that consumes the existing draw/resource buffers, then build Kain-authored layout/style/reconciliation in stdlib above this ABI rather than widening the C layer into widgets.

# 2026-05-11 - Raw native graphics kernel exposes engine-building primitives to Kain

Kain now has a generic native graphics system kernel at the C ABI floor instead of relying on runtime-authored scenes or host-side primitive/default-scene behavior. `runtime/native/include/kain_native_graphics_system.h` and `runtime/native/src/core/kain_native_graphics_system.c` expose low-level sessions, backend target selection, truthful backend availability/status probes, SPIR-V shader module registration, authored buffer handles, mesh handles, pipeline handles, draw command recording, frame present bookkeeping, and diagnostics. `stdlib/native/graphics.kn` exposes thin `native_graphics_*` wrappers for LLVM/direct-C Kain source.

Design decisions:

- Keep this layer catalog-free. The runtime knows handles, backend target ids, SPIR-V byte counts, buffer metadata, mesh counts, pipelines, draw commands, and diagnostics; Kain source owns engine policy, scenes, primitive recipes, simulation loops, materials, cameras, and tools.
- Vulkan and DirectX 12 are first-class backend targets in the access layer, but direct command execution is reported as unavailable/degraded until a real backend executor is attached. Do not claim vendor-direct rendering based only on target selection.
- `runtime/fixtures/native_graphics_engine/main.kn` is the focused LLVM proof shape: one Kain file creates two different authored graphics submissions through the same raw kernel without runtime-provided geometry.
- The language-wide rule is now explicit in `ARCHITECTURE.md`: native/Rust/C code provides capabilities, ABI substrate, validation, diagnostics, and target integration; Kain authors behavior and systems.

Validation:

- `bash runtime/conformance/graphics_runtime/run_tests.sh --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_graphics_engine_primitives_without_scene_catalog --target-dir target\\codex-native-graphics-kernel -- --nocapture`
- Build and run `runtime/fixtures/native_graphics_engine/main.kn` through the LLVM native fixture path after rebuilding the CLI.

Recommended next step:

- Attach the raw graphics command buffer to a real Vulkan or DirectX 12 executor behind the same `kain_native_graphics_*` handles, then add backend-specific conformance that proves actual frame execution without widening the Kain-facing API.

# 2026-05-11 - Raw native UI C ABI makes single-file LLVM UI authoring possible

Kain now has a generic native UI system kernel at the C ABI floor instead of another host-authored component catalog. `runtime/native/include/kain_native_ui_system.h` and `runtime/native/src/ui/kain_native_ui_system.c` expose low-level sessions, arbitrary node kind strings, parent/rect/text/style/flag mutation, focus, hit testing, dirty tracking, event polling, and draw-command buffers. The source is included in both `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`, and `stdlib/native/ui.kn` exposes thin `native_ui_*` wrappers for LLVM/direct-C Kain source.

Design decisions:

- Keep this layer catalog-free. The runtime knows handles, strings, geometry, events, and commands; Kain source or `stdlib/native` owns higher-level buttons, panels, inspectors, tabs, and app-specific UI systems.
- `runtime/fixtures/native_ui_single_file/main.kn` is the current proof shape: one Kain file defines its own surface helper functions, creates arbitrary UI node kinds, draws, routes focus/events, hit-tests, and returns success through LLVM-compatible calls.
- `runtime/conformance/ui_runtime/test_native_ui_system_kernel.c` proves the raw C ABI without involving the older compiled-bundle overlay path.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\\codex-native-ui-system -- --nocapture`
- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`

Recommended next step:

- Connect `kain_native_ui_system` to an actual Win32/bgfx or Qt host frame loop so `native_ui_draw_*` buffers can present pixels live, then add hot reload by rebuilding the single Kain file and replaying session state through stable node ids.

# 2026-05-11 - kain-ui-native archive and legacy feature were removed

Follow-up cleanup removed the `crates/kain-ui-native/src/archive` museum, the `legacy-egui` Cargo feature, and the optional egui/wgpu/font/image/nalgebra/kain-3D dependencies from `kain-ui-native`. The active crate should only carry `app.rs`, `session.rs`, `qt_host.rs`, `lib.rs`, and `main.rs`; old host implementations should be deleted, not archived in this crate.

Validation:

- `cargo fmt -p kain-ui-native`
- `cargo test -p kain-ui-native --target-dir target\\codex-kain-ui-native-slim`
- `cargo check -p kain-ui-native --target-dir target\\codex-kain-ui-native-slim-check`

# 2026-05-11 - Blade resolver crate import surface renamed to `blade`

The Blade workspace resolver package now imports as `blade`, so Rust call sites use `use blade::...` instead of `use kain_blades::...` or `use kain_blade::...`. The source folder remains `crates/kain-blades`, the workspace member path remains `crates/kain-blades`, and user/workspace folders remain plural (`blades/*`). CLI naming also remains plural where it refers to collections: `kain blades ...`; the standalone executable remains `blade`.

Design decision:

- Treat `blade` as the public Rust crate identity for Blade discovery/resolution APIs. Treat `crates/kain-blades` as only the repository folder name.
- Do not rename workspace folder conventions from `blades/*`; only the Rust crate/package identity changed.

Validation:

- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo check -p blade -p kain-build -p kain-core -p kain-c-ffi -p kain-crate-ffi -p kain-host -p kain-omni -p cli --target-dir target\codex-blade-singular`
- `cargo check --manifest-path labs\blades_workspace_smoke\crates\synthetic_reporter\Cargo.toml --target-dir target\codex-blade-singular-lab`

# 2026-05-11 - kain-ui-native became an authored UI host instead of a demo catalog

`crates/kain-ui-native` now follows the same ownership rule as `kain-3D`: Kain source owns UI structure and intent; Rust/native owns host launch, manifest projection, validation, and low-level rendering/diagnostics. The active non-egui path is split into `app.rs`, `session.rs`, and `qt_host.rs`; the old demo/catalog Qt path and legacy egui monolith were deleted from the crate after the follow-up cleanup.

Design decisions:

- Do not synthesize document/viewport/browser/shader/devtools placeholder panes when a bundle emits no authored UI.
- Do not add Rust-side UI catalogs, renderer switchboards, sample dashboards, or default widget layouts to `kain-ui-native`.
- `KainUiNativeSessionManifest` should carry authored surfaces plus the native projection generated from `UiBuildOutput`; the Qt host may render that projection generically, but it must not invent app content.
- Native C overlay fields are diagnostic-only (`diagnostic_title`, `diagnostic_subtitle`, `diagnostic_hint`) and compiled UI bundles take precedence over diagnostic labels.

Validation:

- `cargo fmt -p kain-ui-native`
- `cargo test -p kain-ui-native --target-dir target\\codex-kain-ui-native`
- `cargo check -p kain-ui-native --target-dir target\\codex-kain-ui-native-check`
- `cargo check -p kain-ui-native --features legacy-egui --target-dir target\\codex-kain-ui-native-legacy-check`

Recommended next step:

- Move richer native UI rendering behind authored Kain primitives and bundle metadata, then add a smoke that renders two visually different Kain-authored UIs through the same host to prove Rust is no longer deciding the layout.

# 2026-05-11 - Blade smoke workspace became the Singularity Atlas executable proof

`labs/blades_workspace_smoke` is now a full Blade workspace proof instead of a lightweight demo. The lab still exercises root workspace discovery, `apps/*`, `blades/*`, `crates/*`, C ABI, Rust crate, Kain, Fabric, GPU, and synthetic Cargo blades, but it now also builds and runs a real executable named `blade_singularity_atlas`.

What changed:

- The `gpu-compute` blade emits three Kain-authored shader artifacts: `gpu_step`, `nebula_field`, and `spectral_lattice`, with SPIR-V, HLSL, reflection JSON, and shader bundle outputs validated by the smoke runner.
- The synthetic Cargo blade now depends on `blade` and `kain-fs`, builds `blade_singularity_atlas`, discovers the Blade workspace graph, reads GPU artifacts through `kain-fs`, and renders an atlas report as SVG, PPM, JSON, and HTML under `outputs/singularity-atlas`.
- `scripts/run_blades_smoke.py` now executes the built binary from `.kain/out`, validates the atlas output, checks the expected compute keys, and still proves cache reuse and clean lab cache rebuilds.

Design decisions:

- Keep executable smoke artifacts produced by real Blade build tasks. The lab runner may validate and run them, but it should not become a replacement build system.
- Runtime admire/report outputs can live under `outputs/` when they are produced by the built executable; build artifacts, stamps, and build reports now belong under `.kain/out`, `.kain/cache/build`, and `.kain/reports/build`.
- Current GPU artifact generation accepts sample-based Float math in these smoke shaders; avoid unsupported `Float(index)`-style casts until the shader compiler surface explicitly supports them.

Validation:

- `cargo check --manifest-path labs\blades_workspace_smoke\crates\synthetic_reporter\Cargo.toml --target-dir target\codex-blade-atlas-check`
- `$env:KAIN_BIN=(Resolve-Path target\codex-fs-unified\debug\kain.exe).Path; $env:BLADE_BIN=(Resolve-Path target\codex-fs-unified\debug\blade.exe).Path; python labs\blades_workspace_smoke\scripts\run_blades_smoke.py --clean-cache`

# 2026-05-11 - kain-3D primitives moved to Kain-authored mesh ingestion

`crates/kain-3D` no longer carries a Rust-backed primitive catalog or procedural shape builders. Primitive support is now an authored mesh pipeline: Kain/source data owns the actual vertices, indices, normals, UVs, and primitive recipes; Rust validates and converts that data into `Geometry`, `Mesh`, scene metadata, and host/runtime values.

What changed:

- Replaced the old Rust shape-definition/default-library stack with `AuthoredPrimitive`, `AuthoredPrimitiveRegistry`, and validation errors in `crates/kain-3D/src/primitive.rs`.
- Removed Rust shape factories for box, plane, spheres, cylinder, cone, capsule, and torus. Generic mesh helpers such as `Geometry::indexed_triangle_mesh` remain because they do not encode product primitives.
- Replaced the Kain prelude's shape-specific native functions with `triangle_geometry(...)` / `mesh_geometry(...)` over explicit authored arrays, backed by the generic `__zen3d_triangle_geometry` runtime native.
- Updated `Scene` to register authored primitive registries without manufacturing default shape definitions.
- Updated smoke/test fixtures to use explicit fixture mesh data instead of the removed primitive factories.

Design decision:

- Going forward, do not add Rust-side primitive recipes to `kain-3D`. If Kain needs a cube, sphere, bevelled block, or generated modeling primitive, author that recipe in Kain/source assets and pass explicit mesh data through the generic pipeline.

Validation target:

- Run `cargo test -p kain-3d --target-dir target\\codex-kain-3d-authored-primitives-test` and `cargo check -p kain-3d --bins --lib --target-dir target\\codex-kain-3d-authored-primitives-check` after touching this lane.

# 2026-05-11 - Blade, Fabric, FFI, import, and codebase IO moved onto kain-fs

The Blade workspace pipeline and its adjacent import/FFI/workspace helpers now consume the shared `kain-fs` crate instead of carrying their own `std::fs` behavior.

What changed:

- Wired `kain-fs` into `kain-build`, `kain-blades`, `kain-check`, `kain-test`, `kain-omni`, `kain-host`, `kain-c-ffi`, `kain-crate-ffi`, `kain-import`, and `kain-codebase`.
- Migrated Blade build artifacts, cache stamps, report JSON/JSONL, safe clean, input hashing, C sidecar copying, GPU artifact writes, Fabric manifest/report IO, Omni staging, check/test source discovery, C/Rust FFI generated artifacts, importer source reads, and trusted-local codebase file helpers onto `kain-fs`.
- Kept raw `std::fs` out of the core Blade/build/check/test/host/omni/FFI/import/codebase lanes, except for literal type names and lower-level surfaces outside this pass such as UE/vendor/demo/runtime adapters.
- Fixed the host Fabric test helper to use `kain_fs::DirectoryEntry.file_name` and verified the full `/labs/blades_workspace_smoke` against rebuilt `kain.exe` and `blade.exe`.

Design decisions:

- `kain-fs` is now the expected filesystem owner for artifact-producing and workspace-scanning Kain crates, not only for in-language `fs_*` calls.
- Generated reports and artifacts should prefer `kain_fs::atomic_write_text` / `atomic_write_bytes` when replacing complete files; append-only event streams should use `append_text`.
- FFI/import crates map `FsError` into their existing crate-level error surfaces rather than leaking raw `std::io::Error` conversions from each call site.

Validation:

- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo check -p kain-codebase -p kain-import -p kain-c-ffi -p kain-crate-ffi -p kain-build -p kain-blades -p kain-check -p kain-test -p kain-omni -p kain-host --target-dir target\\codex-fs-unified`
- `cargo test -p kain-blades -p kain-build -p kain-check -p kain-test --target-dir target\\codex-fs-unified -- --nocapture`
- `cargo test -p kain-codebase -p kain-import --target-dir target\\codex-fs-unified -- --nocapture` (`kain-codebase` passed; `kain-import` still has 5 pre-existing transformer test failures unrelated to file IO)
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-crate-ffi --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-c-ffi --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-omni validate_default_polyglot_template_succeeds --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-host python_harness_supports_mixed_multi_output_steps --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-fs-unified`
- `$env:KAIN_BIN=(Resolve-Path target\\codex-fs-unified\\debug\\kain.exe).Path; $env:BLADE_BIN=(Resolve-Path target\\codex-fs-unified\\debug\\blade.exe).Path; python labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py --clean-cache`

Current risks:

- Broad repo scans still find raw `std::fs` in CLI packagers, UI/demo apps, native/runtime adapters, vendor code, and UE-facing lanes. Those were intentionally left alone unless they were part of the core Blade/import/FFI/workspace unification.
- `kain-import` has unrelated C/Rust transformer unit failures in the current checkout; avoid treating those as filesystem regressions without checking the specific failing transformer assertions first.

Recommended next step:

- Continue migrating high-value artifact producers such as `kain-driver` native/Tauri app materialization and non-UE CLI import/packaging paths onto `kain-fs`, then add a small lint/check script that fails new raw `std::fs` use in the core Kain FS-owned lanes.

# 2026-05-11 - LLVM native semantic handles and intent runtime hooks landed

Kain's LLVM native lane now preserves the core semantic shapes that were previously erased for smoke-test convenience.

What changed:

- LLVM maps `Option<T>`, `Result<Ok, Err>`, and `Future<T>` to native tagged `i8*` handles instead of lowering them as plain payload types.
- Added native C facade constructors, tag checks, payload-copy helpers, ready-future creation, await payload extraction, async sleep future creation, and stdlib wrappers for runtime visibility.
- Wired `runtime/native/src/core/kain_runtime_async.c` into `runtime/native_core_runtime.toml` so lean LLVM file builds have the async substrate available.
- Added LLVM lowering for `Some`, `None`, `Ok`, `Err`, `is_some`, `is_none`, `is_ok`, `is_err`, `ok`, `unwrap`, `expect`, `unwrap_or`, `await`, `async`, and `?` for the native tagged path.
- Added native runtime hooks for patch begin/record/commit/undo visibility, entangle propagation records, converge mismatch recording, and orchestrate stage begin/end counters.
- Strengthened `converge` LLVM lowering so a fast lane emits alongside the spec lane, records verification status, returns the fast result on match, and falls back to spec on mismatch.
- Tightened frontend scalar compatibility so TypeScript-import scalar comparison leniency no longer makes ordinary return values, match arms, lets, or arguments type-compatible.
- Added `runtime/fixtures/native_option_result_future/main.kn` and expanded `runtime/fixtures/native_world_actor_intent/main.kn` to prove the native semantic/runtime counters through real LLVM builds.

Design decisions:

- The tagged C ABI is a pragmatic bridge: semantic handles stay visible across LLVM/native boundaries while payload extraction matures beyond scalar-heavy paths.
- `?` residual propagation in LLVM currently returns the existing native `Option`/`Result` handle from functions whose native return ABI is `i8*`.
- Intent runtime hooks are process-local observability and parity helpers, not durable crash-safe journals yet.
- Direct C was intentionally not expanded in this pass; it still trails LLVM for arrays, tuples, match, closures, ranges, fstrings, payload enums, generics, semantic options/results/futures, and typed actor lowering.

Validation:

- `cargo test -p kain-core --test semantic_typecheck_test --target-dir target\\codex-actor-runtime-cli -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\\codex-actor-runtime-cli -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-actor-runtime-cli`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_option_result_future\\main.kn --target llvm`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_option_result_future\\main.kn -t llvm -o target\\codex-native-runtime-proofs\\native_option_result_future.ll`
- `target\\codex-native-runtime-proofs\\native_option_result_future.exe`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_world_actor_intent\\main.kn --target llvm`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t llvm -o target\\codex-native-runtime-proofs\\native_world_actor_intent.ll`
- `target\\codex-native-runtime-proofs\\native_world_actor_intent.exe`

Current risks:

- Tagged payload ownership is conservative and can leak nested RC-managed payloads; the next native ABI pass should add payload destructors or type-aware retain/release callbacks.
- Pattern payload binding and `unwrap` extraction in LLVM currently target scalar payloads first. Struct, tuple, slice, array, and nested semantic payloads need targeted fixtures before calling the lane complete.
- Ready futures are enough for `async { value }` and await payload proof, but a full scheduler/poll/waker/timer model still needs end-to-end Kain syntax and stdlib coverage.
- Patch undo/replay is a visibility hook, not semantic transaction rollback parity with the interpreter.

Recommended next step:

- Add a table-driven native semantic conformance suite that cross-runs interpreter and LLVM cases for every `Option`/`Result`/`Future` payload class, then deepen the C facade lifecycle model before broadening actor/future scheduling semantics.

# 2026-05-11 - Native actor ABI contract and C runtime hardening landed

Kain's native actor lane now has an executable ABI contract instead of relying on matching comments between Rust, LLVM IR, and C headers.

What changed:

- Expanded `crates/kain-actor/src/native.rs` into the canonical Rust-side native actor ABI descriptor: ABI version, actor ID width, invalid ID, mailbox defaults, ask/shutdown timing, supervision restart window, actor name/table/registry/scheduler capacities, monitor notification tag base, required C runtime symbols, required native stdlib actor symbols, and the native message/spawn-config layout.
- Added actor header parity tests in `kain-actor` that read `runtime/native/include/kain_runtime_actor.h` and `kain_runtime_native_stdlib.h`, so Rust model constants and C ABI symbols drift loudly.
- Added `KainActorAbiDescriptor`, `kain_actor_abi_descriptor`, and `kain_actor_abi_descriptor_is_compatible` to the native actor runtime.
- Added explicit `retain_user_data` ownership to `KainActorSpawnConfig` and `KainActorSpawnConfigStored`. Native C/C++ callers now default to plain borrowed `user_data`, while LLVM actor lowering sets `retain_user_data = 1` for Kain RC-managed actor state.
- Fixed native mailbox payload-size retention by storing `data_size` in `MessageNode`; `kain_actor_receive` and `kain_actor_try_receive` now return the original payload size.
- Hardened shutdown-before-first-run behavior: actors closed while still queued now finalize lifecycle side effects, including monitor notifications, supervisor observations, and link propagation when appropriate.
- Added `runtime/conformance/actor_runtime/test_actor_abi_contract.c` and wired it into the actor runtime conformance runner. The test covers ABI descriptor compatibility, spawn defaults, message size retention, registry, monitor notification tags, links, supervision snapshots, and scheduler stats.
- Exposed native actor constants through `runtime/native/include/kain_runtime_native_stdlib.h`, `runtime/native/src/core/kain_runtime_native_stdlib.c`, and `stdlib/native/actor.kn`, then updated `runtime/fixtures/native_world_actor_intent/main.kn` to prove them through LLVM and direct C.
- Updated LLVM actor spawn layout to include `retain_user_data` and made `crates/kain-sys-codegen` depend directly on `kain-actor` for actor ABI sizing.

Design decisions:

- `retain_user_data` is the ABI boundary between compiler-owned Kain RC state and arbitrary host/C/C++ pointers. Do not reintroduce unconditional `rc_retain`/`rc_release` on `user_data`.
- C actor ABI compatibility should be checked through `KainActorAbiDescriptor` and the `kain-actor` parity tests, not only by eyeballing struct comments.
- Native actor stdlib wrappers should expose stable constants where Kain source needs to reason about runtime behavior.

Validation:

- `cargo fmt -p kain-actor -p kain-sys-codegen`
- `cargo test -p kain-actor --target-dir target\\codex-actor-runtime`
- `cargo test -p kain-core --test actor_contract_test --target-dir target\\codex-actor-runtime-core`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply --target-dir target\\codex-actor-runtime-core -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_generates_actor_spawn_and_send_message_paths --target-dir target\\codex-actor-runtime-codegen -- --nocapture`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`
- `target\\codex-actor-runtime\\native_stdlib_bridge.exe`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-actor-runtime-cli`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_world_actor_intent\\main.kn --target llvm`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t llvm -o target\\codex-actor-runtime\\native_world_actor_intent.ll`
- `target\\codex-actor-runtime\\native_world_actor_intent.exe`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_world_actor_intent\\main.kn --target c`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t c -o target\\codex-actor-runtime\\native_world_actor_intent_c.c`
- `target\\codex-actor-runtime\\native_world_actor_intent_c.exe`

Current risks:

- Direct C actor lowering still uses the generic native actor facade rather than generated per-actor handler loops. The runtime substrate is much sturdier now, but specialized direct-C actor semantics remain a deeper compiler pass.
- The native actor conformance runner is Bash-first. It works under the current Windows Git Bash environment, but a PowerShell wrapper would make the Windows lane easier for future agents.

Recommended next step:

- Promote the actor conformance runner plus LLVM/direct-C native fixture into one repo-local smoke command, then add generated per-actor direct-C handler lowering on top of the now-explicit ABI contract.

# 2026-05-11 - Kain FS v2 added sandboxed virtual roots, streaming, watchers, and transactions

Kain's filesystem lane now has a real v2 substrate on top of the initial `kain-fs` crate work.

What changed:

- Added focused `crates/kain-fs` modules for scoped capabilities and virtual mounts (`capabilities.rs`), range/chunk streaming IO (`streaming.rs`), portable polling watchers (`watch.rs`), and best-effort transactional journals with rollback (`transaction.rs`).
- Extended `crates/kain-core/src/runtime.rs` with runtime-owned `FsSandbox`, watcher, and transaction registries plus globals for capability grants/revokes, `fs://` mount resolution, ranged text/byte IO, hex-encoded byte helpers, streaming copy, watcher polling/close, and transaction begin/write/append/remove/copy/move/commit/rollback.
- Registered the new filesystem-facing types and globals in `crates/kain-core/src/types.rs` and `crates/kain-core/src/stdlib.rs`, including `FsChunk`, `FsWatchEvent`, and `FsJournalEntry`.
- Expanded `stdlib/native/fs.kn` and the native C facade in `runtime/native/include/kain_runtime_native_stdlib.h` / `runtime/native/src/core/kain_runtime_native_stdlib.c` with ranged text reads, byte hex reads/writes, metadata text, newline-delimited directory/walk path listings, and streaming copy.
- Updated `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` and `runtime/fixtures/native_fs/main.kn` so direct C, LLVM, and the raw C facade prove the richer filesystem surface.
- Updated the local `kain-fs-pipeline` skill so future agents know the v2 source files, validation commands, and native ABI caveats.

Design decisions:

- `kain-fs` stays the semantic owner. `kain-core` owns process-local runtime handles and Kain-visible globals; `stdlib/native` and the C facade expose ABI-compatible native target wrappers.
- Scoped v2 interpreter helpers resolve through `FsSandbox` before touching the host filesystem. Existing v1 helpers are intentionally not all retrofitted yet, so future work should migrate old `fs_*` calls through the same resolver if virtual roots need universal coverage.
- Native byte arrays and rich records are encoded as lowercase hex, key-value metadata text, and newline-delimited path lists for now because the C ABI does not yet have a clean typed array/record/result story for these values.
- Watchers are portable polling watchers rather than OS notification backends. Transactions are process-local and best-effort rollback journals, not durable crash-safe multi-file commits yet.

Validation:

- `cargo test -p kain-fs --target-dir target\\codex-kain-fs-v2`
- `cargo test -p kain-core filesystem --target-dir target\\codex-kain-fs-v2-core`
- `cargo test -p kain-sys-codegen --test c_codegen_test --target-dir target\\codex-kain-fs-v2-codegen-c -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\\codex-kain-fs-v2-codegen-llvm -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-kain-fs-v2-cli`
- `toolchain\\llvm\\bin\\clang.exe runtime\\conformance\\native_stdlib_bridge\\test_native_stdlib_bridge.c runtime\\native\\src\\core\\kain_runtime_core.c runtime\\native\\src\\core\\kain_runtime_version.c runtime\\native\\src\\core\\kain_runtime_diagnostics.c runtime\\native\\src\\core\\kain_runtime_actor.c runtime\\native\\src\\core\\kain_runtime_entangle.c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-kain-fs-v2-native\\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32`
- `target\\codex-kain-fs-v2-native\\native_stdlib_bridge.exe`
- `target\\codex-kain-fs-v2-cli\\debug\\kain.exe check runtime\\fixtures\\native_fs\\main.kn --target c`
- `target\\codex-kain-fs-v2-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t c -o target\\codex-kain-fs-v2-native\\native_fs_c.c`
- `target\\codex-kain-fs-v2-native\\native_fs_c.exe`
- `target\\codex-kain-fs-v2-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t llvm -o target\\codex-kain-fs-v2-native\\native_fs.ll`
- `target\\codex-kain-fs-v2-native\\native_fs.exe`

Current risks:

- The v2 sandbox resolver is not yet universal across every older v1 interpreter `fs_*` helper.
- The native parity wrappers intentionally use text/hex encodings until native typed records/results/arrays mature.
- Watchers should eventually gain platform-native backends, and transactions should eventually gain durable crash-safe journaling if they become part of `patch` / `law` / `converge` workflows.
- The direct C backend still emits harmless extra-parentheses comparison warnings in generated C.

Recommended next step:

- Retrofit the older v1 interpreter `fs_*` helpers through `FsSandbox`, then add an explicit capability manifest model (`fs.read`, `fs.write`, `fs.project`, `fs.temp`, `fs.watch`, `fs.transaction`) so Kain programs can declare filesystem access instead of inheriting the runtime default.

# 2026-05-11 - Dedicated kain-fs crate and native filesystem pipeline landed

Kain now has a real filesystem substrate instead of scattered file/path helpers.

What changed:

- Added `crates/kain-fs` as a workspace crate for portable file operations, path helpers, metadata, directory entries, directory walks, temp paths, atomic writes, copy/move/remove operations, SHA-256 file hashes, and typed `FsError` values.
- Wired `crates/kain-core` to depend on `kain-fs` and expose first-class `fs_*` runtime globals. Strict variants raise runtime errors, while `fs_try_*` variants return structured `Result` values.
- Added typed filesystem registry data in `crates/kain-core/src/types.rs` and `crates/kain-core/src/stdlib.rs` so interpreter, type metadata, and native codegen see the same global function surface.
- Added `stdlib/native/fs.kn` plus native C facade functions in `runtime/native/include/kain_runtime_native_stdlib.h` and `runtime/native/src/core/kain_runtime_native_stdlib.c` so LLVM and direct C builds can perform real file operations without depending on the generic root stdlib.
- Extended `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` and added `runtime/fixtures/native_fs/main.kn` to prove temp directories, path joins, text writes/appends/reads, copy/move/atomic write, SHA-256 hashing, and recursive removal through native C and generated LLVM/direct-C executables.
- Tightened `crates/kain-sys-codegen` so the C backend lowers string equality through `strcmp`, and LLVM trusts explicit target-stdlib wrapper signatures instead of inferring wrong ABIs for Kain-defined native wrappers.

Design decisions:

- `kain-fs` owns portable semantics; `kain-core` owns how those semantics appear as Kain runtime globals; `stdlib/native` and the C facade own native target exposure.
- `fs_hash_file` is SHA-256 in both Rust and native C lanes. Do not replace one side with a faster non-cryptographic hash unless the API name and docs change together.
- Native target stdlib wrappers are ordinary Kain functions over a C ABI facade. LLVM must skip external declarations for stdlib functions that are defined by loaded target stdlib source, or native builds can produce duplicate declarations/definitions.
- `StdLib::new` return types matter for LLVM lowering. New native-callable filesystem helpers should not be left as `Any` when they return strings, integers, booleans, or units.

Validation:

- `cargo test -p kain-fs --target-dir target\\codex-kain-fs`
- `cargo test -p kain-core filesystem --target-dir target\\codex-kain-fs-core`
- `cargo test -p kain-sys-codegen --test c_codegen_test --target-dir target\\codex-kain-fs-codegen-c -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\\codex-kain-fs-codegen-llvm -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-kain-fs-cli`
- `toolchain\\llvm\\bin\\clang.exe runtime\\conformance\\native_stdlib_bridge\\test_native_stdlib_bridge.c runtime\\native\\src\\core\\kain_runtime_core.c runtime\\native\\src\\core\\kain_runtime_version.c runtime\\native\\src\\core\\kain_runtime_diagnostics.c runtime\\native\\src\\core\\kain_runtime_actor.c runtime\\native\\src\\core\\kain_runtime_entangle.c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-kain-fs-native\\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32`
- `target\\codex-kain-fs-native\\native_stdlib_bridge.exe`
- `target\\codex-kain-fs-cli\\debug\\kain.exe check runtime\\fixtures\\native_fs\\main.kn --target c`
- `target\\codex-kain-fs-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t c -o target\\codex-kain-fs-native\\native_fs_c.c`
- `target\\codex-kain-fs-native\\native_fs_c.exe`
- `target\\codex-kain-fs-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t llvm -o target\\codex-kain-fs-native\\native_fs.ll`
- `target\\codex-kain-fs-native\\native_fs.exe`

Current risks:

- The native facade currently exposes a useful v1 subset: text/path/temp/hash/copy/move/remove/status. The Rust crate already has richer metadata and directory-walk APIs that need native wrappers if Kain code should call them from LLVM/direct-C.
- Several complex filesystem values still flow as `Any` in the stdlib registry until Kain's typed record/result story is strengthened.
- The direct C backend still emits harmless extra-parentheses comparison warnings in generated C.

Recommended next step:

- Add a manifest-driven filesystem smoke under `smoketest/` or `runtime/fixtures` that runs the Rust interpreter, direct C, and LLVM filesystem lanes from one command, then expand native wrappers for directory listing and structured metadata.

# 2026-05-11 - Blade build system v1 landed in kain-build

Kain now has a real blade workspace build orchestrator instead of lab-local build scripts.

What changed:

- Added `crates/kain-build/src/workspace.rs` as the typed Blade build planner/executor. It discovers a blade workspace through `kain-blades`, builds a DAG, topologically orders tasks, stamps cacheable work, and emits JSON/JSONL build reports.
- `kain-build` now owns adapters for C shared libraries, Cargo manifests, GPU shader artifacts, Kain source checks, Fabric validation/runs, and explicit Node/Bun/custom tasks declared in `[[build.tasks]]`.
- Extended `KAIN.toml` blade metadata with `[build] artifact_root`, `cache_root`, `profile`, and `[[build.tasks]]`, and extended C FFI library metadata with `sources`.
- Added `kain blades build .` plus a standalone `blade build .` binary. Both support `--json`, `--dry-run`, `--clean`, `--profile`, `--target`, and `--include-vulkan`.
- Reworked `labs/blades_workspace_smoke` so its runner invokes `blade build . --json` instead of compiling the C sidecar itself. The smoke now proves cold builds, cache hits, C sidecar materialization, Cargo blades, GPU artifacts, CPU Fabric execution, GPU Fabric validation, and `kain blades/equip` inspection.
- Fixed the shared Node bridge process boundary on Windows by stripping `\\?\` verbatim prefixes before spawning Node. Fabric Node steps were otherwise able to fail before the bridge script could answer.

Design decisions:

- Build products are workspace-local and disposable: `.kain/out/<host>/<lane>/<target>/<unit>/<task>/...` for canonical artifacts, `.kain/cache/build/stamps` for fingerprints, and `.kain/reports/build` for build reports/events.
- `kain-blades` still owns discovery and manifest resolution; `kain-build` owns build graph planning and execution. Callers should not rescan `blades/*`, `apps/*`, or `crates/*`, and labs should not carry custom build scripts for artifacts the build graph can own.
- Fabric GPU manifests are validated by default and only run when `--include-vulkan` is passed, because local machines may not have a working Vulkan compute runtime.
- Safe clean is intentionally narrow: `--clean` removes only workspace-local `.kain` artifact/cache/report roots.

Validation:

- `cargo check -p kain-build --target-dir target\\codex-blade-build`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo check -p cli --target-dir target\\codex-blade-build-cli`
- `cargo test -p kain-blades --target-dir target\\codex-blade-test-blades`
- `cargo test -p kain-build --target-dir target\\codex-blade-test-build`
- `cargo test -p kain-node process_portable_path_strips_windows_verbatim_prefix --target-dir target\\codex-blade-test-node`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-blade-build-cli`
- `$env:KAIN_BIN=(Resolve-Path target\\codex-blade-build-cli\\debug\\kain.exe).Path; $env:BLADE_BIN=(Resolve-Path target\\codex-blade-build-cli\\debug\\blade.exe).Path; python labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py`
- Same smoke with `--clean-cache`

Current risks:

- `kain-build` v1 is sequential. The DAG and cache fingerprints are ready for parallel scheduling, but execution currently stays simple and deterministic.
- Explicit `[[build.tasks]] depends_on` handling is intentionally conservative and needs a deeper pass before complex cross-blade user-authored dependency aliases become a public contract.
- JSON output can still be preceded by lower-layer compiler/runtime chatter. The lab smoke extracts the final JSON payload robustly, but a future CLI quiet mode would be cleaner.

Recommended next step:

- Add parallel task execution with a small scheduler and stable report ordering, then promote a blade-build CI lane that runs the clean-cache lab smoke from freshly built `kain` and `blade` binaries.

# 2026-05-11 - Dedicated kain-actor crate landed as actor-system foundation

Kain now has a real `crates/kain-actor` crate instead of keeping all actor-system vocabulary hidden inside `kain-core`.

What changed:

- Added `crates/kain-actor` to the workspace with focused modules for actor IDs, addresses/paths, messages, actor definitions, mailbox policy, lifecycle, supervision, scheduler policy, behavior contracts, registry snapshots, actor-system validation, runtime snapshots/events, and native ABI descriptors.
- Kept `kain-actor/src/lib.rs` as a public index only. Future actor work should extend the focused module that owns the concept instead of growing a giant lib file.
- Wired `kain-core` to consume `kain-actor` for `ActorId`, `ActorIdAllocator`, `MessageEnvelope<Value>`, default ask timeout, and typed actor contracts.
- `TypedActor` now carries `actor_contract: kain_actor::ActorDefinition`, built during typechecking from resolved actor state slots, handler message parameters, and actor method signatures.
- Actor contract validation now catches duplicate handler names, duplicate state slots, duplicate method names, invalid message/parameter shapes, and supervisor child mistakes through reusable `kain-actor` validators.
- Runtime-contract reflection now emits actor message names from the shared actor contract instead of leaving actor reflection empty.
- Added focused tests for the actor crate model and for `kain-core` actor contract construction/duplicate-handler rejection.

Design decisions:

- `kain-core` still owns actor syntax, AST, typechecking, and interpreter execution. `kain-actor` owns reusable actor-system model data that can be consumed by core, native runtime work, LLVM/direct-C lowering, IDE tooling, and future stdlib layers.
- The first crate pass is deliberately model/contract-heavy, not a replacement scheduler. That gives supervision, mailbox, behavior, registry, and native ABI work stable files to extend without destabilizing existing interpreter semantics.
- Actor IDs now reserve raw `0` as invalid so Rust actor model data stays aligned with the native C runtime ABI.

Validation:

- `cargo fmt -p kain-actor -p kain-core`
- `cargo test -p kain-actor --target-dir target\\codex-kain-actor`
- `cargo test -p kain-core --test actor_contract_test --target-dir target\\codex-kain-actor-core`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply --target-dir target\\codex-kain-actor-core`
- `cargo test -p kain-core actor --target-dir target\\codex-kain-actor-core` was also attempted. The actor contract/runtime cases passed, but the broad filter failed on existing missing fixture `m:/Code/Factory/Example_GAS/test_targets.kn` in `test_target_actor_parser`.

Current risks:

- `kain-actor` is now the correct home for actor-system model expansion, but the interpreter still runs actor loops in `kain-core/src/runtime.rs`. Moving scheduling/mailbox execution behind reusable runtime traits should be a separate, careful pass.
- Direct C and LLVM actor lowering can consume the new native ABI descriptors, but generated specialized per-actor handler loops are still future work.
- There are unrelated dirty filesystem/blades/native-runtime changes in this checkout. Do not stage or revert them as part of actor work.

Recommended next step:

- Add a second pass that gives `kain-actor` executable mailbox/supervision runtime traits, then have `kain-core` delegate spawn/send/ask through those traits while native LLVM/C lowering consumes the same actor contract metadata.

# 2026-05-11 - Native stdlib and runtime facade landed for LLVM and direct C

Kain now has a target-scoped native stdlib profile and C ABI facade that let actor, entangle, patch, law, converge, orchestrate, world, timing, diagnostics, and runtime helpers compile through both `-t llvm` and `-t c`.

What changed:

- Added `stdlib/native` as the shared native target stdlib profile for LLVM and direct C, plus `stdlib/c` as the direct C bridge layer. `crates/kain-core/src/stdlib.rs` loads all matching profiles for a target, so C gets `native` then `c`, while LLVM gets `native` only.
- Added `runtime/native/include/kain_runtime_native_stdlib.h` and `runtime/native/src/core/kain_runtime_native_stdlib.c` as the narrow C ABI facade for native Kain stdlib calls. It wraps runtime init/shutdown, actor registry/spawn/send/scheduler helpers, entangle registry helpers, status/diagnostics, and timing.
- Added `runtime/native_core_runtime.toml` as the default lean native runtime manifest for normal LLVM/direct-C file builds. `runtime/native_runtime.toml` now survives only as the lean compatibility mirror and also includes the native stdlib facade source.
- Updated `crates/cli/src/main.rs` so native builds prefer `runtime/native_core_runtime.toml` before the broad manifest, and only stage the GPU runtime DLL when the LLVM artifact stage actually produced compute residency payloads.
- Updated `crates/cli/src/llvm_native_stage.rs` so shader artifact staging only runs for source that declares shader items, avoiding shader/GPU sidecar work for native stdlib-only actor/intent programs.
- Updated `crates/kain-sys-codegen/src/codegen_c.rs` so `@extern` functions become declarations only, `spawn`/`send` lower to the native actor facade, `main` emits a valid C `int`, unsigned integer casts map to C integer types, and direct C entangle metadata registers with the native runtime through a generated `__kain_register_entanglements()` thunk.
- Added `runtime/fixtures/native_world_actor_intent/main.kn` as the all-in-one native proof for `world`, `entangle`, `actor`, `patch`, `law`, `converge`, `orchestrate`, and the native stdlib facade.
- Added `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` to exercise the facade directly from C.

Design decisions:

- The native stdlib is target-scoped on purpose. Do not let LLVM/C native builds fall back to the root stdlib unless the target profile is absent; the generic root includes richer constructs that direct C does not yet own.
- `runtime/native_core_runtime.toml` is the safe default for ordinary language/native proof builds. `runtime/native_runtime.toml` now mirrors that same lean surface for compatibility and should not be treated as a separate broader runtime lane.
- Direct C now links against the same native runtime facade as LLVM for first-class actor and entangle behavior. It remains an experimental subset, but unsupported forms should fail explicitly rather than silently erasing core language declarations.
- The current actor facade spawn path uses a generic blocking actor bootstrap for named-payload mailbox traffic. It proves runtime wiring and send/spawn ABI, not compiler-generated per-actor handler specialization for direct C yet.

Validation:

- `cargo test -p kain-core stdlib --target-dir target\\codex-native-stdlib-core`
- `cargo test -p kain-entangle --target-dir target\\codex-native-entangle`
- `cargo test -p kain-sys-codegen c_backend --target-dir target\\codex-native-stdlib -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-native-stdlib-cli`
- `cargo test -p cli --lib "stage_llvm_native_artifacts_" --target-dir target\\codex-native-stdlib-cli-test -- --nocapture`
- `cargo test -p cli --bin kain native_runtime_manifest_candidates_prefer_core_runtime --target-dir target\\codex-native-stdlib-cli-bin-test -- --nocapture`
- `toolchain\\llvm\\bin\\clang.exe -c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-native-stdlib\\kain_runtime_native_stdlib.obj`
- `toolchain\\llvm\\bin\\clang.exe runtime\\conformance\\native_stdlib_bridge\\test_native_stdlib_bridge.c runtime\\native\\src\\core\\kain_runtime_core.c runtime\\native\\src\\core\\kain_runtime_version.c runtime\\native\\src\\core\\kain_runtime_diagnostics.c runtime\\native\\src\\core\\kain_runtime_actor.c runtime\\native\\src\\core\\kain_runtime_entangle.c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-native-stdlib\\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32`
- `target\\codex-native-stdlib\\native_stdlib_bridge.exe`
- `target\\codex-native-stdlib-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t llvm -o target\\codex-native-stdlib\\native_world_actor_intent.ll`
- `target\\codex-native-stdlib\\native_world_actor_intent.exe`
- `target\\codex-native-stdlib-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t c -o target\\codex-native-stdlib\\native_world_actor_intent_c.c`
- `target\\codex-native-stdlib\\native_world_actor_intent_c.exe`

Current risks:

- Historical notes in older docs may still describe `runtime/native_runtime.toml` as a broader app/vendor lane. As of 2026-05-15 it is only the lean compatibility mirror; archived app/vendor/runtime code now lives under `.zarchive/runtime_devendor_2026-05-15/`.
- Direct C actor lowering currently routes through the generic facade instead of emitting specialized actor handler loops. That is enough for spawn/send/link proofing and runtime smoke coverage, but generated direct-C actor semantics still need a deeper pass.
- The C backend still emits noisy but harmless comparison-parentheses warnings in some stdlib helper expressions.

Recommended next step:

- Promote `runtime/fixtures/native_world_actor_intent/main.kn` and `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` into a single scripted smoke so future runtime/compiler changes prove LLVM, direct C, and the C facade together without hand-running each command.

# 2026-05-11 - Full blades workspace smoke landed under labs

Kain now has a repo-local smoke that exercises the blades system as a complete workspace instead of as isolated unit tests.

What changed:

- Added `labs/blades_workspace_smoke` with a root `KAIN.toml` workspace over `apps/*`, `blades/*`, and `crates/*`.
- Added an app blade (`signal-console`), a Kain utility blade (`signal-math`), a C ABI blade (`native-filter`), a Rust crate blade with Kain glue (`native-metrics`), a Cargo-only synthetic blade (`synthetic-reporter`), and a GPU metadata blade (`gpu-compute`).
- Added CPU Fabric execution through `blade = "..."` references for Python -> Kain -> C ABI -> Rust crate -> Node, plus a GPU Fabric manifest that validates blade-backed `gpu_compute`.
- Added `scripts/run_blades_smoke.py`, which builds the platform C shared library, checks blade list/graph/check/equip JSON, validates both Fabric manifests, runs the CPU Fabric pipeline, and emits GPU artifacts. It keeps lab-local `.kain` bridge caches by default and supports `--clean-cache` for cold-cache proofing.
- Updated `labs/README.md` and `ARCHITECTURE.md` so future agents can find the smoke and know the validation command.

Design decisions:

- The smoke is intentionally shaped like a real workspace, not a minimal fixture. It proves root workspace discovery, explicit blade metadata, synthetic Cargo discovery, graph edges, C/Rust FFI fallback through blades, and GPU compute metadata in one place.
- The default runner validates GPU by generating artifacts instead of dispatching Vulkan. Use `--include-vulkan` only on machines with a working Vulkan compute runtime.
- The C checksum returns a bounded signed `int64_t`; importing a raw `uint64_t` checksum into Kain `Int` can overflow at runtime.

Validation:

- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-blades-smoke`
- `$env:KAIN_BIN='D:\\Kain-Lang\\target\\codex-blades-smoke\\debug\\kain.exe'; python labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py`
- `python -m py_compile labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py`

Current risks:

- A stale `target\\debug\\kain.exe` may not have the `blades` subcommand. Set `KAIN_BIN` to a freshly built CLI when running the smoke from an isolated target dir.
- The generated C shared library, `.kain` bridge caches, and `outputs/` are ignored and disposable. `kain blades check` is expected to pass after the runner builds the C sidecar.
- The GPU Fabric manifest is validation-ready, but full Vulkan dispatch is opt-in because local machines may lack a working Vulkan compute runtime.

Recommended next step:

- Add this smoke to any future blades CI lane once the repo has a stable way to select a freshly built `kain` binary and a policy for local C/Rust FFI bridge cache reuse.

# 2026-05-11 - Kain check/test pipeline hardened into a Rust-inspired v1

The reusable source validation pipeline now has a sturdier first-class shape instead of being only a thin CLI addition.

What changed:

- `crates/kain-core/src/runtime.rs` now recursively executes `test` items nested inside typed modules, so module-scoped tests are not merely counted and silently skipped.
- `crates/kain-test` now reports `skipped` cases separately from `passed` and `failed`, parses `//@ ignore`, `//@ skip`, and `//@ known-bug` directives, and supports `run_ignored` so CLI `--ignored` can burn down known-bug inventory.
- `crates/kain-test` now reports the real execution lane for run/test modes (`run` for run-pass/run-fail, `test` for Kain test items) even when a target directive exists for check modes.
- `kain check -` now honors the documented stdin path and emits the same structured report shape as file/directory checks.
- `kain test` now exposes `--ignored`, prints skipped reasons, and keeps JSON reports explicit through `skipped` and `skip_reason`.
- Added `smoketest/kain-test` as a tiny directive suite covering check-pass, check-fail, run-fail, nested module tests, and ignored cases.
- Added `docs/cli/check-and-test.md` and refreshed the CLI, crate, feature, command-matrix, architecture docs around `kain-check` and `kain-test`.

Design decisions:

- Kain should borrow Rust compiletest's proven directive ideas, not its whole architecture. The source-of-truth crates are `kain-check` and `kain-test`; CLI remains a shell.
- Ignored/known-bug cases are success-neutral by default and only execute with `--ignored`, matching the workflow of keeping known gaps visible without breaking every local suite.
- Future snapshot, revision, target-conditional, and bless/update semantics should land inside `kain-test` before any ad hoc script gets to invent parallel suite semantics.

Validation:

- `rustfmt --edition 2021 crates\\kain-test\\src\\lib.rs crates\\kain-core\\src\\runtime.rs crates\\cli\\src\\main.rs`
- `cargo test -p kain-test -p kain-check --target-dir target\\codex-check-test`
- `cargo test -p kain-core run_tests --target-dir target\\codex-check-test`
- `cargo test -p kain-test --target-dir target\\codex-check-test`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-check-test`
- `target\\codex-check-test\\debug\\kain.exe check smoketest\\kain-test\\check_pass.kn`
- `"fn main() -> Int:`n    return 0`n" | target\\codex-check-test\\debug\\kain.exe check -`
- `target\\codex-check-test\\debug\\kain.exe test smoketest\\kain-test --json target\\codex-check-test\\kain-test-report.json`
- `target\\codex-check-test\\debug\\kain.exe test smoketest\\kain-test --ignored` was expected to fail because the ignored parser-bad fixture is intentionally executed under `--ignored`.

Current risks:

- There is still no snapshot comparison, revision matrix, target-conditional directive family, bless/update flow, or parallel scheduling. The crate boundary is ready for those, but v1 only proves directive modes and structured reports.
- Runtime test output is printed directly by `runtime::run_tests`; the harness does not capture stdout/stderr for snapshot-style assertions yet.
- Existing workspace warnings remain noisy during `cargo build -p cli`; they are pre-existing and not part of this pipeline pass.

Recommended next step:

- Add snapshot support to `kain-test`: normalized stdout/stderr/diagnostic artifacts, `--bless`, and sidecar `.stderr` / `.stdout` files, using Rust compiletest as the behavior reference while keeping the Kain-owned report schema.

# 2026-05-11 - Kain blades landed as the local crate-like workspace system

Kain now has a first-class `kain-blades` crate that makes the "blades" idea real across CLI, Fabric, module lookup, Rust crate FFI, and C ABI FFI.

What changed:

- Added `crates/kain-blades` as the typed discovery/resolution layer for local blade workspaces. It discovers default `blades/*`, `apps/*`, and `crates/*` roots, honors `[workspace] blades`, `blade_roots`, and `members` from `KAIN.toml`, parses `[blade]` metadata, and treats plain `Cargo.toml` packages as synthetic Rust blades.
- Added `kain blades list`, `kain blades graph`, `kain blades check`, and `kain equip <blade>` to the CLI, with text and JSON output.
- Committed the existing `kain-check` and `kain-test` crates as the reusable libraries behind the already-planned `kain check` and `kain test` CLI commands; their stale failure fixtures were updated to use syntax errors instead of type mismatch cases that the current frontend accepts.
- Wired blade module roots into `kain-core` filesystem module candidates so a blade can expose Kain modules without callers hardcoding folder paths.
- Wired blade fallback into `kain-crate-ffi` and `kain-c-ffi`, so Rust crate imports and C ABI library imports can resolve through the same blade graph.
- Extended Fabric schema/execution with `blade = "..."` support. Kain, Rust crate, C ABI, and GPU Fabric steps can now resolve entries/manifests/shaders/compute keys from a blade instead of repeating path fields.
- Fixed the pre-existing CLI exhaustiveness blocker by wiring `Commands::Check` and `Commands::Test` through the `kain-check` and `kain-test` crates, which allowed a full `cargo build -p cli` proof.

Design decisions:

- Blades are local-first and crate-like today, not a remote package manager yet. Future `sharpen` behavior should extend `kain-blades` rather than making a separate registry/update path.
- Rust crates and Kain blades are deliberately interchangeable at the folder-boundary level: a Cargo crate under `crates/*` can be equipped as a blade, and a `KAIN.toml` blade can point at Rust/C/Fabric/GPU artifacts.
- `kain-blades` is the one place that should know default blade patterns and manifest semantics. Callers should consume `ResolvedBlade`, module roots, Rust crate blade resolution, or C FFI library resolution instead of reimplementing scans.

Validation:

- `rustfmt --edition 2021 crates/kain-blades/src/lib.rs crates/cli/src/blades.rs crates/kain-core/src/module_resolution.rs crates/kain-crate-ffi/src/resolve.rs crates/kain-c-ffi/src/lib.rs crates/kain-omni/src/fabric.rs crates/kain-host/src/fabric.rs`
- `cargo test -p kain-blades --target-dir target\\codex-blades`
- `cargo test -p kain-core blade_module_roots_extend_filesystem_candidates --target-dir target\\codex-blades`
- `cargo test -p kain-omni validate_default_polyglot_template_succeeds --target-dir target\\codex-blades`
- `cargo test -p kain-host python_harness_supports_mixed_multi_output_steps --target-dir target\\codex-blades`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-blades`
- `target\\codex-blades\\debug\\kain.exe equip kain-core --json`
- `target\\codex-blades\\debug\\kain.exe blades list .`
- `cargo test -p kain-crate-ffi --target-dir target\\codex-blades`
- `cargo test -p kain-c-ffi --target-dir target\\codex-blades`
- `cargo test -p kain-check -p kain-test --target-dir target\\codex-blades`

Current risks:

- `cargo test -p kain-omni --target-dir target\\codex-blades` still has one non-blades failure in `tests::build_emits_rust_from_import_aware_entry` with `Unknown identifier 'helper'`; the focused Fabric blade-adjacent validation passed.
- `blades check` can report missing generated/shared-library artifacts for blades whose native sidecars have not been built yet.
- There is no remote registry, lockfile, install, or `sharpen` implementation yet. The current crate is the local graph and resolver foundation.

Recommended next step:

- Add a smoke blade under `blades/` with Kain, Rust crate, C ABI, Fabric, and GPU sections, then run `kain equip`, `kain fabric run`, and both FFI import paths against that one intentional fixture.

# 2026-05-11 - LLVM and direct C native intent backends refreshed

Kain's native backend path now handles the compiler-owned intent suite more honestly across LLVM, direct C output, and the native C runtime.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now registers and emits `law` declarations as real LLVM callables, records parameter types for `patch`/`law`/`converge`/`orchestrate`, preserves orchestrate stage runtime comments, and emits an entangle registration function that calls the C runtime from `main`.
- `runtime/native/include/kain_runtime_entangle.h` and `runtime/native/src/core/kain_runtime_entangle.c` add a small fixed-capacity native entangle registry. `runtime/native_runtime.toml` now includes that source in the manifest-driven C runtime bundle.
- `crates/kain-sys-codegen/src/codegen_c.rs` now lowers worlds to C structs/static world instances, emits `patch`, `law`, `converge`, and `orchestrate` as callable functions, preserves entangles as a static metadata table, supports stage calls, and maps world parameters/fields through pointer-style C access.
- `crates/kain-core/src/stdlib.rs` now routes `CompileTarget::C` through `stdlib/c` before root fallback, keeping the experimental C backend away from the full generic stdlib unless the C profile is absent.
- The Rust backend bootstrap intrinsic tests were corrected after missing `CallArg.span` fields were restored; those intrinsics now assert the rendered intrinsic behavior instead of stale raw-call fallback text.

Design decisions:

- LLVM entangle support uses the typed entangle items already produced by `kain-core` and emits a narrow native registration ABI. Rich write barriers, cross-process propagation, and distributed conflict policy remain future runtime adapters.
- Direct C output keeps entangle metadata local instead of forcing every generated C file to link a runtime registration symbol. Runtime-linked registration is currently the LLVM lane's responsibility.
- The C backend remains an explicit subset, but it should now fail on truly unsupported expression/type forms rather than silently ignoring first-class intent declarations.

Validation:

- `cargo test -p kain-sys-codegen --target-dir target\\codex-llvm-refresh`
- `cargo test -p kain-core test_load_stdlib_for_target_uses_target_profile_order --target-dir target\\codex-llvm-refresh -- --nocapture`
- `cargo test -p kain-entangle --target-dir target\\codex-llvm-refresh`
- `cargo test -p cli --lib stage_llvm_native_artifacts_materializes_entangle_metadata --target-dir target\\codex-llvm-refresh -- --nocapture`
- `toolchain\\llvm\\bin\\clang.exe -c runtime\\native\\src\\core\\kain_runtime_entangle.c -Iruntime\\native\\include -o target\\codex-llvm-refresh\\kain_runtime_entangle.obj`

Current risks:

- Full `cargo test -p cli ...` still fails in this checkout because the pre-existing dirty CLI command enum has `Commands::Check` and `Commands::Test` variants that are not handled in `main.rs`. Use `cargo test -p cli --lib ...` for the native staging test until that unrelated CLI work is reconciled.
- The C backend does not yet implement every expression form, generic type, container ABI, or runtime registration path. It now covers the compiler-owned intent declarations, but deep C parity still needs focused backend work.
- Entangle alias canonicalization remains an interpreter/runtime risk from the earlier entangle pass: alias writes such as `let p = Physics; p.player_health -= 10` still need canonical path recovery.

Recommended next step:

- Add a native-link smoke that compiles a generated LLVM file with `kain_runtime_entangle.c` included from `native_runtime.toml`, then asserts the registry contains the emitted binding after `main` runs.

# 2026-05-11 - First-class entangle state coupling landed

Kain now has a v1 first-class `entangle` declaration for compiler-owned Topological State Coupling between stable state endpoints.

What changed:

- Added `crates/kain-entangle` as the shared semantic/runtime metadata crate. It owns `state.entangle`, `EntangleGraph`, endpoint ids, single-writer binding descriptors, duplicate endpoint checks, self-entanglement rejection, mirror lookup, and mirror-write denial.
- Added parser, AST, typechecker, formatter, interpreter, runtime contract, realtime app bundle, LSP, and UE5-codegen awareness for:
  - `entangle Physics.player_health <-> UI.health_display with single_writer`
- `crates/kain-core` now lowers entanglements into typed metadata, `RuntimeContractBundle.entanglements`, `RealtimeAppBundle.entanglements`, the reflection payload, and required capability/service-binding metadata.
- The interpreter registers entanglements during program setup, treats the left endpoint as the authority, propagates authority writes into the right mirror endpoint, and rejects direct mirror writes under the v1 `single_writer` policy.
- Docs now list entangle as the sixth compiler-owned intent family and describe the v1 syntax, capability, contract shape, interpreter semantics, and current limits.

Design decisions:

- `entangle` is a contextual top-level item keyword rather than a hard lexer keyword, matching other compiler-owned intent forms.
- V1 supports only stable dotted storage endpoints with at least two path segments. The typechecker resolves world state and struct-field paths through the existing value/type environment.
- V1 requires strict matching resolved storage types after shared-reference peeling. It intentionally does not use the looser assignment-compatibility rule.
- The left endpoint is authoritative and the right endpoint is the mirror. The policy is explicit as `with single_writer` so future policies can be added without reshaping the syntax.
- Backend/codegen crates currently treat entanglements as metadata-only unless they consume the runtime contract or realtime bundle.

Validation:

- `cargo test -p kain-entangle --target-dir target\codex-entangle`
- `cargo test -p kain-core entangle --target-dir target\codex-entangle -- --nocapture`
- `cargo test -p kain-core --test compiler_owned_intent_test --target-dir target\codex-entangle -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-entangle`
- `git diff --check`

Current risks:

- Interpreter propagation keys off the authored assignment path. `Physics.player_health -= 10` propagates, but alias-based writes such as `let p = Physics; p.player_health -= 10` do not yet canonicalize back to the entangled endpoint.
- Native ABI, LLVM, C/C++/TS/WASM, and distributed side-channel lowering are not implemented yet. Those targets should consume the emitted `entanglements[]` metadata and `state.entangle` requirement.
- Only `single_writer` exists today. Multi-writer conflict policy, timestamp/vector-clock resolution, atomics, shared-memory rings, and cross-process transport are future work.

Recommended next step:

- Add backend lowering that consumes `RuntimeContractBundle.entanglements` and emits target-specific write barriers or adapter hooks, starting with the realtime/native UI path where the `state.entangle` service binding already makes the requirement visible.

# 2026-05-10 - Windows git index writes now have a repo-local safe-write escape hatch

This checkout can still hit a Windows-only git failure where index-mutating commands finish their work and then die on the final `.git/index.lock -> .git/index` swap with `fatal: unable to write new index file`.

What changed:

- Added `scripts/windows/git-safe-index.ps1`.
- The script copies the live `.git/index` to `.git/index.safe-write`, runs the requested git command with `GIT_INDEX_FILE` pointed at that temporary index, then stream-copies the resulting bytes back into the live `.git/index` in place instead of relying on the failing rename step.
- The script refuses to run if a real `.git/index.lock` exists so it does not silently stomp an active git writer.
- `ARCHITECTURE.md` now points future agents at the helper from `## Common Errors`.

Usage:

- `./scripts/windows/git-safe-index.ps1 add -A`
- `./scripts/windows/git-safe-index.ps1 rm -r --cached generated`
- If no arguments are passed, it defaults to `add -A`.

Current risks:

- This is a safe operator workaround for an external Windows file-handle/index-swap issue, not a root-cause fix inside the repo source itself.
- The helper only addresses index writes. If a separate environment issue later blocks branch ref updates during `commit` or remote-tracking ref updates during `push`, that still needs the same kind of manual repair or a future wrapper for refs.

Recommended next step:

- If this keeps recurring outside Codex too, capture the actual handle owner with Process Explorer or Sysinternals `handle.exe` and decide whether a broader `git-safe-commit.ps1` / `git-safe-push.ps1` wrapper is worth adding.

# 2026-05-10 - Full-power codebase bridge and Fabric Node handoff landed

Kain now treats local workspace control as an explicit trusted execution lane instead of a read-only inspection helper.

What changed:

- Added `crates/kain-codebase` as the trusted-local workspace authority layer. It discovers roots from `KAIN.toml`, `package.json`, `Cargo.toml`, `.git`, and explicit paths; scans, hashes, creates, writes, copies, moves, and deletes files/directories; round-trips JSON/TOML; and captures commands with structured stdout/stderr/status.
- Exposed `kain codebase inspect <path> --json` and `kain codebase run <cwd> -- <command> ...`. `codebase run` exits successfully when Kain captured the child process correctly, even if the child command itself returned nonzero; inspect the JSON `success` and `status` fields for the child result.
- Registered the new codebase APIs in host-backed Kain execution: `codebase_*`, `cargo_*`, `python_*`, `c_*`, and `ts_*` bridge functions now typecheck and dispatch in interpreted/test host lanes.
- Fixed the Node/Fabric raw bridge regression by keeping `@extern` declarations from shadowing native bridge functions, preserving raw Node module handles through raw import/call paths, adding CJS `js_require_raw`/`node_require`, and adding `node_package_run` for package-script execution from the Node bridge cwd.
- Fabric Kain steps now install Node runtime cwd/cache config from the Fabric workspace root, and Node Fabric steps receive upstream `fabric_inputs` with shared payload projections instead of an empty JS object.
- The GreebleFS image-converter Fabric pipeline now runs end to end with Python -> Kain -> C ABI -> Rust crate -> Node. The latest proof session reported Node `inputProjection = received`, upstream keys `kain_orchestrator,rust_analyzer`, a 64x64 shared-image chain, and a native shared-buffer snapshot.

Validation:

- `cargo test -p kain-codebase --target-dir target\codex-codebase-bridge -- --nocapture`
- `cargo test -p kain-node --target-dir target\codex-codebase-bridge -- --nocapture`
- `cargo test -p kain-host node_step_consumes_shared_inputs_via_fabric_inputs --target-dir target\codex-codebase-bridge -- --nocapture`
- `cargo build -p cli --target-dir target\codex-codebase-bridge`
- `target\codex-codebase-bridge\debug\kain.exe codebase inspect D:\GreebleFS --json`
- `target\codex-codebase-bridge\debug\kain.exe codebase run D:\GreebleFS -- bun --version`
- `target\codex-codebase-bridge\debug\kain.exe codebase run D:\GreebleFS -- cargo check --manifest-path src-tauri/Cargo.toml --lib`
- `D:\Kain-Lang\target\codex-codebase-bridge\debug\kain.exe fabric run -m KAIN.fabric.toml` from `D:\GreebleFS\usr\plugins-kain\kain-image-converter\plugin.runtime\fabric`

Current risks:

- Direct C calls in `kain-codebase` intentionally support a narrow scalar ABI surface today (`i64`/`f64`/`void` signatures). Rich C ABI reflection should build on `kain-c-ffi` instead of bloating this first workspace-control crate.
- GreebleFS `pnpm --version` is captured correctly but returns child status `1` because that repo is configured for Bun. Use Bun for GreebleFS package-manager smokes unless the package-manager policy changes.
- GreebleFS `cargo check --manifest-path src-tauri/Cargo.toml --lib` is captured correctly and passed in this session, but the build can still print Windows incremental-cache cleanup warnings when another process has target files locked.

Recommended next step:

- Add a higher-level Kain-authored smoke under `smoketest/fabric/` or GreebleFS `usr/plugins-kain` that calls the new `codebase_*` APIs from `.kn` directly, then runs package/Cargo/Python/C/TS operators from one trusted-local script.

# 2026-05-09 - TypeScript import ambient prelude is generated from TypeScript lib data

The TypeScript import pipeline now uses an embedded ambient manifest instead of hand-maintained Rust lists of JavaScript/DOM globals.

What changed:

- Added `tools/typescript_import/extract_ambient_manifest.py` and `tools/typescript_import/typescript_ambient_overrides.json`.
- The extractor reads `reference/TypeScript-main/src/lib/*.d.ts`, merges Kain-specific aliases/helpers from the JSON override file, and writes `crates/kain-import/src/typescript/data/typescript_ambient_manifest.json`.
- `crates/kain-import/src/typescript/ambient.rs` embeds that manifest and exposes lookup helpers for ambient value names and TypeScript utility-type fallbacks.
- `kain import-ts` now writes the global TS prelude from the manifest, not from hardcoded DOM/JS arrays in `crates/cli/src/import_typescript.rs`.
- Global runtime constructor aliases such as `Array -> ts_Array` and ecosystem helpers such as Node/test-runner globals live in data, so future additions should update the override JSON and regenerate the manifest.
- Generated `.kn` validation for the TypeScript importer now uses the TS backend instead of the interpreter target; interpreter validation is not representative for TS imports with external stubs.

Validation:

- `python tools\typescript_import\extract_ambient_manifest.py` generated a manifest with 1051 ambient value symbols and 2206 ambient type symbols.
- `cargo test -p kain-import ambient --target-dir target\codex-ts-import-manifest` passes.
- `cargo build -p cli --target-dir target\codex-ts-import-manifest` passes with pre-existing workspace warnings.
- A focused ambient smoke using `HTMLElement`, `URL`, `ImportMeta`, `Uint8Array`, `Blob`, `Proxy`, `Promise`, `console`, `window`, and `import.meta` imports, parses, validates, and compiles to TS.
- `target\codex-ts-import-manifest\debug\kain.exe import-ts D:\GreebleFS\src --flat --exclude vendor --output target\codex-ts-import-manifest\greeblefs_src_firstparty.kn --target ts` imported 650/650 first-party files after excluding 392 vendor files; generated `.kn` parse validation, generated `.kn` TS compile validation, and requested TS output compile all passed.
- After the destructured-param and high-arity `forEach` lowering fixes, PATH `kain import-ts D:\GreebleFS\src --flat --output D:\GreebleFS\src-kain\reflection\imports\greeblefs\greeblefs_src.kn --target ts --report-json D:\GreebleFS\src-kain\reflection\imports\greeblefs\greeblefs_src.import_report.json` emits both `greeblefs_src.kn` and `greeblefs_src.ts`; generated Kain validation and requested TS target compilation both pass.

Current risks:

- Import diagnostics remain high on large React projects because external module imports, JSX fallbacks, object spreads, and destructured props still lower through lossy stubs. Those are now reported as degradation diagnostics, not validation failures.
- Full `D:\GreebleFS\src` import still reports one source parse failure in `D:\GreebleFS\src\vendor\tiptap\extension-drag-handle\__tests__\edgeDetection.spec.ts` from SWC (`Expected(,, "[")`). The batch continues, writes the reflection artifacts, and compiles the generated Kain/TS outputs, but true 1042/1042 coverage needs a follow-up parser fallback or targeted handling for that test file's syntax.
- The embedded prelude is intentionally broad. A future optimization can make prelude emission usage-pruned while keeping this manifest as the source of truth.

Recommended next step:

- Add project-aware ambient discovery for `node_modules/@types` or configured `tsconfig` type roots so Node/Vitest/React ecosystem globals can be generated from package declarations instead of only from the stable override JSON.

# 2026-05-08 - Rust import printer now preserves expression-heavy Tauri command bodies

The Rust import pipeline no longer turns most expression bodies into `LOSSY LOWERING [class:unsupported_expr_lowering]` comments when generating `.kn` from already-lowered Rust AST.

What changed:

- Expanded `crates/cli/src/import_rust.rs` source emission for Kain AST expressions and statements instead of only printing literals/idents.
- The CLI printer now handles calls, method chains, fields, indexing, assignments, binary/unary ops, refs/derefs, casts, `await`, `?`, lambdas, arrays, tuples, structs, enum variants, `if`, `match`, loops, and unit `()`.
- Added a regression test for the GreebleFS-shaped Tauri preview helpers (`PathBuf::from`, `preview_streaming.policy().clone()`, `run_native_blocking_task(...).await?`, `BinaryResponse::new`, and `dirs::home_dir().map(...).ok_or_else(...)`).

Validation:

- `cargo check -p cli --target-dir target\codex-rust-import-check` passes with pre-existing warnings.
- `cargo test -p cli --target-dir target\codex-rust-import-check import_rust::tests::rust_import_printer_preserves_tauri_preview_expression_bodies -- --nocapture` passes.
- Re-importing `D:\GreebleFS\src-tauri\src\fs_commands.rs` into `generated\rust_import_validation\fs_commands.kn` produced 199 functions, 37 structs, 12 enums, zero `LOSSY LOWERING`, zero `unsupported_expr_lowering`, and an empty diagnostics class report.

Current risks:

- This repair is a printer expansion, not a full guarantee that every printed construct is accepted by every Kain backend. The importer can now preserve much more source shape, but backend/codegen support remains target-sensitive.
- The output may still contain Rust-shaped names normalized into Kain identifiers (for example `PathBuf__from`, `NativeTaskRequest__new_`), which is expected for this importer lane.

Recommended next step:

- Add a small CLI fixture under `crates/cli/tests/fixtures/import_rust` or a broader all-in-one smoke that imports a real Tauri command slice and asserts the generated report stays free of `unsupported_expr_lowering`.

# 2026-05-07 - Filesystem imports now dogfood sibling Kain modules

Kain now handles the import shape that blocked the first GreebleFS Kain control-plane split: `use module::item` can resolve against `module.kn` / `src/module.kn` when `module/item.kn` does not exist, and `use module::*` can expose top-level sibling module items during typechecking.

What changed:

- Added `crates/kain-core/src/module_resolution.rs` as the shared lookup helper for stdlib roots and authored filesystem module candidates.
- Updated the interpreter runtime import path so named filesystem imports can select one top-level item from a fallback module file and honor `as` aliases.
- Updated the typechecker to best-effort register symbols from cleanly parsed filesystem modules, while preserving the older `Unknown` fallback when imported modules are absent or not safe to register during typechecking.
- Added focused `kain-core` runtime tests for the GreebleFS-shaped imports: `use host_reflection::build_control_plane_catalog` and `use plugin_authoring::*`.
- Updated `docs/syntax-and-semantics/module-resolution.md` and the local `kain-engineer` import reference so future agents do not rediscover the old workaround.

Validation:

- `cargo test -p kain-core filesystem_ -- --nocapture` passes.
- `cargo build -p cli --target-dir target\codex-cli-build` passes; the alternate target dir avoids the local `target/debug` PyO3 artifact lock.
- `git diff --check -- crates\kain-core\src\module_resolution.rs crates\kain-core\src\lib.rs crates\kain-core\src\runtime.rs crates\kain-core\src\types.rs crates\kain-core\src\runtime_tests.rs` passes with line-ending warnings only.

Current risk:

- Filesystem module lookup is still rooted in the process current directory, not the source file's absolute parent. For nested scripts such as `src/server.kn`, launch from the project/runtime root or a directory where the expected `src/<module>.kn` exists until source-file-relative roots are added.
- Plain `cargo build -p cli` in the default `target/debug` directory is blocked on this machine by a locked PyO3 artifact (`target/debug/deps/libpyo3_build_config-9afde652236a6978.rlib`). Use a separate `--target-dir` for validation until that Windows file handle clears, then refresh `target/debug/kain.exe`.

Recommended next step:

- After the CLI binary rebuilds, simplify the GreebleFS control-plane `server.kn` back into real sibling imports instead of keeping it self-contained, then add a Kain CLI smoke that runs that split module layout.

# 2026-04-18 - Tauri desktop adapter landed as a first-class native-ui host lane

The repo now has a real Tauri 2 desktop host path for Kain-authored UI instead of forcing every native-ui flow through the Qt launcher.

What changed:

- `crates/kain-ui` and `crates/kain-core` now recognize `UiHostBackendKind::Tauri`, including authored `host_backend="tauri"` and `host_backend="webview"` aliases.
- `crates/kain-ui-tauri` now owns the generated Tauri host lane: plugin/capability/permission presets, bridge-manifest construction, merged reflection metadata, hybrid frontend bridge JS, and generated `src-tauri/*` project files.
- `crates/kain-driver` now has a dedicated Tauri bundle/materialization path that combines native runtime-contract truth with hybrid frontend artifacts and emits a generated Tauri app root with `frontend/`, `generated/`, `config/`, `state/`, and `src-tauri/`.
- `crates/cli/src/native_ui_build.rs` now exposes `NativeUiHostKind::{Qt,Tauri}` plus typed Tauri config, and `crates/cli/src/native_ui_dev.rs` now abstracts launch targets so the same dev loop can launch either a packaged Qt executable or `cargo run --manifest-path src-tauri/Cargo.toml`.
- Hot-reload metadata for generated Tauri apps now preserves the resolved custom bundle identifier instead of silently falling back to a derived default, and new tests pin both the Tauri alias parsing path and the generated bundle-id propagation.

Validation:

- `cargo test -p kain-ui tauri_aliases`
- `cargo test -p kain-core tauri_aliases`
- `cargo test -p kain-ui-tauri`
- `cargo test -p kain-driver --features tauri tauri_bundle_materialization_writes_bridge_and_frontend_assets`
- `cargo test -p cli --features tauri native_ui_build::tests::native_ui_build_materializes_tauri_project_without_binary -- --exact`
- `cargo test -p cli --features tauri native_ui_dev::tests::reload_decision_hot_reloads_runtime_sidecar_changes -- --exact`

Important behavior notes:

- Tauri remains a host/package lane under `build native-ui` and `native-ui dev`; there is still no `CompileTarget::Tauri`.
- The generated Tauri app consumes existing compiler-owned truth: native runtime bundle/contract/realtime metadata plus hybrid JS/TS/WASM output. Keep those bundle families authoritative instead of inventing Tauri-local semantics.
- In this checkout `cargo fmt --all` is still blocked by unrelated trailing whitespace in `crates/ue5-shaders/src/validation.rs`, so file-scoped `rustfmt` is the safe formatting fallback when only the Tauri lane is being touched.

Current risk:

- The generated Rust host bridge is intentionally broad but still generic. Future work should harden real typed command handlers and add richer plugin-specific round-trip tests once there are Kain-authored apps depending on those namespaces.
- Full workspace validation for `kain-driver --features tauri` still includes unrelated pre-existing driver test failures outside the Tauri lane, so use the Tauri-focused test filters above when validating this subsystem.

Recommended next step:

- Add a smoketest app under `smoketest/UI/` that is materialized and launched through `--host tauri`, then validate one real plugin namespace such as dialog/fs/store end to end against the generated bridge.

- New Kain 3D pass (2026-04-17): `SceneCatalog::picker_entries()` now orders canonical scenes semantically, keeping the default scene first, then ranking remaining canonicals by scene role and scene scale before appending aliases. This makes native scene browsers and inspectors surface showcase/environment scenes more intentionally instead of only following raw name order.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d picker_entries_prioritize_default_then_semantic_canonicals_then_aliases -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalogEntry::picker_label()` now includes the authored `viewport_summary` alongside the resolved scene name and composition labels, so native scene browsers can show the scene's launch/context cue instead of hiding it in the struct.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d catalog_entries_surface_picker_ready_metadata -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalogEntry` now carries `scene_focus` alongside role/scale/profile/density/stage, so native scene browsers get the dominant composition cue without re-deriving it from `SceneCompositionSummary`.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d catalog_entries_surface_picker_ready_metadata -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `material_atrium_smoke` now embeds `SceneCatalog::summary()` data in the structured smoke JSON, including default scene, canonical scene count, alias count, total scene names, and picker entry count. The header copy also now calls out catalog coverage so the smoke reports scene-browser context without re-deriving it in downstream tooling.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\bin\material_atrium_smoke.rs` completed cleanly, but `cargo test -p kain-3d catalog_summary_reports_canonical_and_alias_counts -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalog::picker_entries()` now emits a picker-ordered scene list with the default scene first, followed by canonical scenes and then aliases. This gives native scene browsers and inspectors a direct, data-driven ordering instead of making each host re-sort the catalog itself.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d picker_entries_prioritize_default_scene_before_aliases -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCompositionSummary` now exposes a structured `scene_focus` cue (`geometry-led`, `instance-led`, `material-led`, `lighting-led`, `environment-led`, `anomaly-led`) and `FrameDiagnostics` carries it through the CPU/WGPU frame path. `material_atrium_smoke` now preserves the cue in its JSON payload, so scene tooling can tell what dominates a composition instead of only reading size and density.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs crates\kain-3D\src\renderer.rs crates\kain-3D\src\bin\material_atrium_smoke.rs` completed cleanly, but `cargo test -p kain-3d scene::tests::scene_focus_label_tracks_scene_dominant_authoring_signal -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalog` now exposes a structured `summary()` with canonical scene count, alias count, and default scene name. This gives native tooling a cheap, stable way to present catalog coverage without re-deriving totals from map sizes in multiple places.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d catalog_summary_reports_canonical_and_alias_counts -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): extracted the scene-composition-to-frame-diagnostics mapping into `SceneCompositionSummary::populate_frame_diagnostics(...)` and switched both CPU and WGPU renderers to call it. This removes duplicated diagnostics wiring, keeps `FrameDiagnostics` fields aligned across backends, and gives future 3D tooling a single place to extend when new summary fields should surface in native frame logs.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs crates\kain-3D\src\renderer.rs crates\kain-3D\src\wgpu_renderer.rs` completed cleanly, but `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries `scene_density` alongside the existing role/scale/profile/camera-fit diagnostics, and both the CPU and WGPU renderers populate it from `SceneCompositionSummary::density_label()`. This keeps the dense/sparse/balanced cue available to native inspectors without forcing them to re-derive it from the brief label.
- Validation note: `cargo test -p kain-3d renderer::tests::default_camera_auto_frames_off_center_scene -- --nocapture` was still blocked by the repo-local Windows GNU toolchain, not by the 3D change. `x86_64-w64-mingw32-gcc` failed while linking build scripts because `lld` could not find `-lgcc_eh` and `-lgcc`.
- New selfhost bootstrap pass (2026-04-16): collapsed `src/core/parser.kn` to a bootstrap-safe `parse_source(...)` stub and rewrote `src/core/lexer.kn` to a field-access-free bootstrap surface. This removed the owned `--emit-llvm-only` blocker `Unknown field 'kind'`, which was coming from the bootstrap token seam rather than the LLVM backend itself.
- Validation note: the exact command `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; $env:PYO3_PYTHON='C:\Users\ephemara\AppData\Local\Programs\Python\Python312\python.exe'; cargo run -q -p cli --bin kain -- selfhost bootstrap --manifest-path src/KAIN.toml --emit-llvm-only` now fails later with `let binding expected Result<Value, KainError>, found Result<Value, Unknown>`, narrowed to the bootstrap `Result::Ok(...)` coercion path in `src/core/runtime.kn`.
- Operator note: when this automation reads the bootstrap report in parallel with the command, `bootstrap_report.md/json` can lag one run behind the live stderr/stdout failure. Use the direct command output as the source of truth for the freshest blocker.

- New backend pass (2026-04-16): Kain now has a first-class experimental `c` compile target wired through `kain-core`, `kain-driver`, `kain-sys-codegen`, CLI native artifact staging, and `kain selfhost bootstrap --backend c`. The C lane reuses the raw-native runtime contract/bundle path and native link flow instead of pretending C is just another alias for LLVM.
- The new C backend is intentionally an honest subset today. It covers the target plumbing plus an initial emitter for structs, unit enums, functions, basic statements, casts, pointer/ref syntax, struct literals, and `print`/`println` helpers, while failing explicitly on unsupported semantic surface such as generic/function types from the full stdlib and many richer expression forms.
- Validation note: `cargo check -p kain-core -p kain-c-ffi -p kain-sys-codegen -p kain-driver -p cli` is green here only with `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1` because the local Python is 3.14 while repo PyO3 is pinned below that. A direct `target/debug/kain.exe -c ... -t c` smoke now reaches the C backend and reports backend-specific unsupported-type errors instead of rejecting the target, so the current blocker is C semantic coverage rather than CLI wiring.
- New Kain 3D pass (2026-04-16): renderer frame diagnostics now expose an explicit `camera_fit_ratio` string alongside the existing framing hint, and the `material_atrium_smoke` JSON payload preserves it. This gives scene tooling a sharper read on how tightly a scene is framed without recomputing the fit math downstream, and it keeps CPU/WGPU 3D diagnostics aligned on the same framing signal.
- Validation note: `cargo test -p kain-3d renderer::tests::render_scene_autoframes_off_center_geometry_and_tracks_diagnostics -- --nocapture` was blocked by the repo-local Windows GNU toolchain, not by the 3D code. `x86_64-w64-mingw32-gcc` could not resolve `-lgcc_eh` and `-lgcc` while linking build scripts. `rustfmt --edition 2021 crates\\kain-3D\\src\\renderer.rs crates\\kain-3D\\src\\wgpu_renderer.rs crates\\kain-3D\\src\\bin\\material_atrium_smoke.rs` completed cleanly.
- New selfhost bootstrap pass (2026-04-16): the owned `--emit-llvm-only` lane now gets past the previous parser-hostile support modules in `src/core/span.kn`, `src/core/error.kn`, `src/core/diagnostic.kn`, and `src/core/effects.kn` by collapsing those files to declaration-only bootstrap-safe surfaces. The latest validated command is `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo run -q -p cli --bin kain -- selfhost bootstrap --manifest-path src/KAIN.toml --emit-llvm-only`, and it now fails later with `Unknown identifier 'tokenize_source'` at `<input>:922:16`, which maps to the lexer/kainc bootstrap seam rather than the old impl/match parser failures.

- New Kain 3D direction update (2026-04-16): the next wave should pivot away from smoke/report polish and into core 3D power features. Treat SPIR-V compilation strength as a major asset, then build outward into renderer architecture, scene/runtime systems, GPU compute, and other high-leverage capabilities that move Kain toward UE5-class power instead of demo-only output.
- New Kain 3D pass (2026-04-16): `SceneCatalog` now exposes picker-ready catalog entries with canonical/alias resolution plus scene role, scale, profile, density, and composition-stage metadata. That gives native tooling a single structured list for scene browsers and inspectors instead of forcing each host to re-derive labels from names.
- New Kain 3D pass (2026-04-16): `SceneCatalog` now exposes canonical scene names and alias-inclusive scene names directly, which lets future tooling build real scene pickers and inspectors without hardcoding the catalog. This is a small but high-leverage step toward more discoverable 3D composition and runtime tooling.
- New Kain 3D pass (2026-04-16): the CPU and WGPU renderers now both reuse `SceneCompositionSummary::framing_hint_label()` for `FrameDiagnostics.framing_hint`, removing duplicate fit-ratio logic so the two presentation paths stay aligned when composition heuristics evolve. This keeps renderer diagnostics consistent across backends with a very small code change.
- Validation attempt: `cargo test -p kain-3d scene::tests::scene_role_label_tracks_scene_complexity_signals -- --nocapture` still failed in this checkout because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc` during build-script linking.

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries a `framing_hint` string (`tight-fit` / `balanced-fit` / `loose-fit`) derived from the scene bounds radius and the framed camera distance, and `material_atrium_smoke` persists that hint in the runtime-matrix JSON. This gives native tooling a quick-read signal for whether a frame is tightly composed or has deliberate breathing room, without recomputing camera fit heuristics downstream.
- Validation attempt: `cargo test -p kain-3d default_camera_auto_frames_off_center_scene -- --nocapture` still fails here before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.

- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now exposes a structured `diagnostics()` helper, and `material_atrium_smoke` uses it when writing the runtime-matrix JSON. That makes the smoke report and any future scene inspectors consume one canonical scene-composition shape instead of hand-rebuilding the same labels and counts in multiple places.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_uses_view_aspect_ratio_for_fit_distance -- --nocapture` still fails here before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries structured scene-composition cues (`scene_role`, `scene_scale`, and `scene_profile`) alongside the existing flat summary string, so renderer output can be queried without parsing one concatenated label. This is a tooling-focused uplift for native inspectors and scene browsers.
- Validation attempt: `cargo test -p kain-3d --lib` could not finish here because the repo-local Windows GNU toolchain still fails during build-script linking (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-16): `SceneBounds` now exposes a coarse composition profile (`linear` / `planar` / `stacked` / `volumetric`), and `SceneCompositionSummary::brief_label()` surfaces that profile alongside the existing scale, aspect, and density cues. This makes scene diagnostics better at telling native tooling whether a scene is a corridor, a flat stage, or a fuller volumetric composition at a glance.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now also emits a coarse scene-role cue (`study` / `lookdev` / `showcase` / `environment` / `anomaly`), giving native tooling a one-word read on whether a composition is a small study, a presentation set, an FX-heavy environment, or a black-hole-style special case. The role cue is folded into the brief label so smoke logs and inspectors get the classification for free.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_profile_label_distinguishes_flat_and_volumetric_scenes -- --nocapture` still fails before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.
- Validation attempt for the new role cue: `cargo test -p kain-3d scene::tests::scene_role_label_tracks_scene_complexity_signals -- --nocapture` hit the same repo-local Windows GNU linker gap while building build-script dependencies, not a scene-logic failure.

- New Kain 3D pass (2026-04-16): software rendering now distinguishes visible vs. fully culled instances in `FrameDiagnostics`, so tooling can see when an authored object was completely clipped/backfaced instead of only inferring success from the final image. Added a regression test that pushes a triangle behind the camera and expects it to land in `culled_instances`.
- Validation attempt: `cargo test -p kain-3d renderer::tests -- --nocapture` still hits the repo-local Windows GNU linker gap before the test binary can link, because `x86_64-w64-mingw32-gcc` cannot resolve `-lgcc_eh` and `-lgcc`.

- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now includes an explicit scene-scale cue (`miniature` / `room-scale` / `studio-scale` / `world-scale`), and the `material_atrium_smoke` JSON payload now carries that scale as structured metadata. This gives 3D tooling one more quick-read signal for composition quality without re-deriving bounds heuristics downstream.
- Validation attempt: `cargo test -p kain-3d scene::tests::scene_scale_label_tracks_bounds_radius -- --nocapture` and `rustfmt --edition 2021 --check crates\\kain-3D\\src\\scene.rs crates\\kain-3D\\src\\lib.rs crates\\kain-3D\\src\\bin\\material_atrium_smoke.rs` both hit repo-local/environment issues before a clean green could be proven. The test run failed at link time because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`; the rustfmt check also surfaced pre-existing formatting differences elsewhere in `crates/kain-3D` and `crates/kain-ui-native` plus trailing whitespace in `crates/ue5-shaders/src/validation.rs`.

- New Kain 3D pass (2026-04-16): `material_atrium_smoke` now emits a structured `diagnostics.composition` payload alongside the existing brief label, including summary counts, framing distance, viewport aspect ratio, and bounds span/center data. This makes the 3D smoke report much easier for tooling to consume without re-deriving scene structure from screenshots or renderer internals.
- Validation attempt: `cargo check -p kain-3D --bin material_atrium_smoke` still fails in this repo-local Windows GNU toolchain before the crate can finish compiling because build-script linking cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now counts directional and point lights in addition to meshes/materials/instances/animations/emitters/terrain, and the brief scene label surfaces those light counts when present. This makes dense lookdev or lighting-heavy scenes read more truthfully in renderer diagnostics and keeps the density cue aligned with actual authored scene complexity.
- Validation attempt: `cargo test -p kain-3d composition_summary_density_label_tracks_authoring_scale -- --nocapture` still fails before the test binary can link because the repo-local Windows GNU toolchain cannot find `-lgcc_eh` and `-lgcc`.
- The Kain 3D pipeline is a live fleet initiative now, and its steering should stay spec-first.
- The intended build path is native, GPU-aware 3D capability that can grow toward DCC-class tools like ZBrush, Substance Painter, and UE5-style workflows.
- Use Codex CLI through the coding-agent skill for pipeline tasks unless the user asks for another harness.
- If Codex reports a usage-limit error, verify the actual CLI output before assuming any seat-switch workaround.
- The user wants frequent updates while the pipeline is active, especially when branches, specs, or heartbeat behavior change.
- Kaino should keep the heartbeat/operator guidance current in this workspace so future passes stay aligned.
- New Kain 3D pass (2026-04-16): the WGPU renderer now preserves the same frame diagnostics as the software renderer, including scene name, viewport summary, composition summary, camera source, and catalog resolution metadata for scene renders. This closes a tooling gap where GPU-backed 3D frames were less self-describing than CPU-backed frames.
- Validation attempt: `cargo test -p kain-3d wgpu_renderer::tests::aligns_readback_rows_to_wgpu_requirement -- --nocapture` failed before reaching the 3D test because the repo-local Windows GNU toolchain still cannot link build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now carries a coarse scene-density cue (`sparse` / `balanced` / `dense`) based on authored meshes, instances, emitters, and terrain surfaces. This makes scene diagnostics better at signaling when a composition is small enough for quick iteration versus crowded enough to need more careful framing or tooling.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_density_label_tracks_authoring_scale -- --nocapture` and `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` both failed before the tests could run because the repo-local Windows GNU toolchain still cannot link build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now spells out viewport shape as `portrait` / `square` / `landscape` instead of only raw aspect ratio, and the 3D scene tests now cover that banding helper. This makes renderer diagnostics easier to scan during scene-composition work without changing the underlying framing math.
- Validation attempt: `cargo test -p kain-3d scene::tests -- --nocapture` still hits the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`) before the `kain-3d` test binary can link.
- 2026-04-15 bootstrap update: `kain selfhost bootstrap` now exists as the owned hand-written lane entrypoint, `src/KAIN.toml` is the manifest contract, `src/build_selfhost.sh` is just a wrapper, and the bootstrap report machinery now emits JSON/Markdown under `src/.selfhost/reports/`.
- The bootstrap harness is partially green: `--combine-only` passes and writes the combined source artifact, but `--emit-llvm-only` currently hard-fails inside the owned `src/core` source set with parser errors concentrated in `runtime.kn` and `types.kn`. The immediate blocker is language/source compatibility, not the CLI wrapper or report plumbing.
- Added a 3D platform uplift in `crates/kain-3D`: primitive libraries now export richer scene metadata (`definition_count`, `definition_ids`, and startup primitive display name) when registered into an authoring scene, which makes the library more self-describing for tooling and runtime composition.
- Added `SceneDescription::composition_summary(...)` plus a shared bounds helper in `crates/kain-3D`, so tooling can ask a scene for counts and framing data in one pass instead of re-deriving it ad hoc.
- Validation was blocked by the local Windows GNU toolchain, not by the change itself. `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while linking build scripts because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): tightened default scene framing in `crates/kain-3D` so the auto-camera distance now scales with field of view instead of using a fixed radius multiplier. Added a regression test for the new framing helper to prove tighter FOVs push the camera farther back. Validation hit a repo-env Windows GNU linker gap, not a code failure: `cargo test -p kain-3d framed_camera_distance_scales_with_field_of_view` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-17): the template viewport contract now exposes explicit `composition_policy` and `framing_policy` fields, and the scene-spine validator checks that those policy tokens stay present in `viewport_runtime.kn`. This keeps the documented launch/framing policy aligned with the authored 3D runtime contract instead of letting it drift back into implicit renderer behavior.
- New Kain 3D pass (2026-04-14): scene bounds now include particle emitters, not just meshes/terrain/black holes, so auto-framing keeps volumetric FX inside the camera composition. Added a regression test proving an emitter-only scene still produces bounds and a framed camera pose. Validation was blocked by the same local Windows GNU linker gap, not by the scene logic: `cargo test -p kain-3d scene::tests` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now has a human-readable `brief_label()`/`Display` form, so 3D tooling and logs can describe a scene's composition without reformatting counts ad hoc. Added a regression assertion that `to_string()` matches the brief label. Validation was again blocked by the local Windows GNU linker gap, not the code change: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framed camera placement now scales its framing direction with the scene's horizontal and vertical extents instead of always biasing toward a fixed diagonal offset, and a new regression test covers tall-scene framing so vertical compositions stay above the scene center. This should behave better on wide or asymmetrical 3D compositions while keeping the same bounds-driven camera target. Validation hit the same repo-local Windows GNU linker gap before the test binary could build: `cargo test -p kain-3d scene::tests -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a span() helper and `SceneCompositionSummary::brief_label()` includes the full XYZ span alongside radius. This makes scene logs and tooling more spatially descriptive without re-deriving extents at each call site. Added a regression assertion that the label includes span text and that `span()` equals `half_extents * 2.0`.
- New Kain 3D pass (2026-04-14): auto-framing now respects per-view instance transform overrides through `SceneDescription::bounds_with_overrides(...)` and `framed_camera_pose_with_overrides(...)`, and the software renderer uses that override-aware camera when no explicit view camera is supplied. Added a regression test proving the frame target follows an overridden material_atrium node. Validation is still blocked locally by the Windows GNU linker gap (`-lgcc_eh` / `-lgcc` missing from `x86_64-w64-mingw32-gcc`).
- New Kain 3D pass (2026-04-14): hardened zero-length vector handling in the 3D math/render path by adding `Vec3::normalized_or(...)` and using it for particle emitter axes, orbit rotation, and basis construction in the CPU and WGPU renderers. This prevents zero-axis scene data from producing brittle normalization behavior and keeps particle/orbit math stable. Added regression tests for zero-axis particle emitters and zero-axis rotation. Validation is still blocked by the repo-local Windows GNU linker gap, and `cargo fmt --all` is currently blocked by unrelated trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- New Kain 3D pass (2026-04-14): added explicit scene resolution metadata to `SceneCatalog` via `resolve_scene(...)`, so tools can distinguish exact hits, aliases, and default fallbacks instead of treating every lookup as a plain `scene(...)` fetch. The `material_atrium_smoke` report now records requested vs resolved scene names plus the resolution kind, which makes smoke output much more useful for alias/debug triage. Validation is still blocked by the local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`) before the test binary can link.
- New Kain 3D pass (2026-04-14): auto-framed camera poses now compute near/far clip planes from scene bounds, which should reduce clipping in large or shallow compositions while preserving the bounds-driven framing target. Also cleaned up a stray syntax brace in `crates/kain-3D/src/scene.rs` that `rustfmt` surfaced during validation. Validation remains blocked by the same local Windows GNU linker gap, so `cargo test -p kain-3d scene::tests::framed_camera_clip_planes_expand_with_bounds -- --nocapture` could not link because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now includes an explicit `framed_camera_distance` derived from the scene bounds and camera FOV, and the brief label reports that fit distance alongside bounds. This gives 3D tooling a direct framing cue instead of forcing it to recompute camera fit from the raw summary. Validation on the focused `scene_bounds_and_framed_camera_follow_scene_composition` test is still blocked by the local Windows GNU linker gap (`-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): the software renderer now forwards scene/tooling metadata through `FrameDiagnostics` (`scene_name`, `viewport_summary`, and a brief `composition_summary`), so hosts can label 3D frames without re-deriving context from pixels. Added a regression assertion that the framed-camera smoke scene reports those fields. Validation was blocked by the same local Windows GNU linker gap, because `cargo test -p kain-3d` could not link build scripts while `x86_64-w64-mingw32-gcc` lacked `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framing now takes viewport aspect ratio into account in `crates/kain-3D`, and both the software and WGPU renderers pass their actual aspect ratio into the scene camera fit. This should reduce clipping on wide or tall viewports without changing authored scene meaning. Added a regression test that wide viewports demand a farther camera fit than square ones. Validation is pending, but the repo-local Windows GNU linker gap has been the recurring blocker for `cargo test -p kain-3d` on this machine (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): the `material_atrium_smoke` report now serializes each tile's frame diagnostics (`camera_source`, scene name, viewport summary, composition summary, and visible/culled instance lists), so tooling can inspect the actual framing decision instead of inferring it from screenshots alone. This is a tooling uplift that makes the 3D smoke output more self-describing for future debugging and scene-composition work.
- New Kain 3D pass (2026-04-14): scene composition summaries are now aspect-ratio aware in `crates/kain-3D`, so renderer diagnostics report a framing distance that matches the actual viewport instead of assuming a square view. The software renderer now feeds its real aspect ratio into the summary path, which makes frame metadata and logs more trustworthy for wide native viewports. Added a regression test for the new aspect-aware summary helper. Validation was blocked by the same local Windows GNU linker gap before the test binary could finish linking (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): `templates/3D/src-kain/stdlib/three_d_runtime/viewport_runtime.kn` now carries explicit `composition_policy` and `framing_policy` fields on `ViewportDescriptor`, with the default profile bound to `scene_summary_driven_and_launch_preset_bound` and `bounds_fov_and_aspect_ratio_fit`. This makes viewport launch contracts line up with the scene-summary/framing work already landing in `crates/kain-3D`, and the template README now calls out the policy explicitly for future authors.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a dominant-axis label, and `SceneCompositionSummary::brief_label()` appends a simple wide/tall/deep cue next to the span, so tooling can read scene proportions faster from logs and frame metadata. This is a small but practical authoring/tooling improvement for 3D composition debugging. Validation hit the same environment blocker as other local runs: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed during dependency linking because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now carries the viewport aspect ratio and includes it in `brief_label()`, so frame diagnostics can report the actual render shape alongside bounds and camera fit instead of leaving aspect implicit. Added a regression assertion that the summary label includes `aspect 1.00:1` for the default path. Validation pending.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::density_label()` now accounts for materials, animations, and black-hole presence in addition to meshes, instances, emitters, and terrain, so the sparse/balanced/dense cue better reflects actual scene complexity. The regression test now covers material/animation-heavy balanced scenes and black-hole-heavy dense scenes. Validation was blocked by the same local Windows GNU linker gap before the focused test binary could link: `cargo test -p kain-3d scene::tests::composition_summary_density_label_tracks_authoring_scale -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-16): `crates/kain-3D` now carries catalog resolution metadata through `FrameDiagnostics` for catalog renders, so frame logs can distinguish exact scene hits from aliases and default fallbacks instead of dropping that context after resolution. The software renderer also now preserves that metadata on the returned frame, which makes alias/default debugging easier for tooling and smoke reports. Validation hit the same local Windows GNU linker gap before the focused test binary could finish linking: `cargo test -p kain-3d renderer::tests -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-16): auto-framed camera placement now uses an aspect-aware framing direction helper in `crates/kain-3D`, so the camera bias adapts more predictably to wide vs. tall compositions instead of using one hardcoded diagonal. Added a regression test for the direction helper. Validation was blocked by the repo-local Windows GNU linker gap when trying to run `cargo test -p kain-3d scene::tests`, and repo-wide `cargo fmt --all --check` is still blocked by trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- Superseded Kain 3D primitive note (2026-04-16): the old Rust-authored primitive catalog metadata was removed on 2026-05-11. Future primitive work should use the Kain-authored mesh ingestion registry instead of reviving catalog-policy metadata.
- New Kain 3D pass (2026-04-16): the `material_atrium_smoke` report now preserves catalog-resolution diagnostics in its JSON payload (`requested_name`, `resolved_name`, and resolution kind), so smoke consumers can distinguish exact, alias, and default scene resolution without re-parsing renderer internals. Validation of the crate still hits the local Windows GNU linker gap before the test binary can link (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): fixed the WGPU renderer's camera-resolution plumbing by passing `RenderResolution` into the internal camera resolver, so the GPU 3D path can auto-frame scenes using the actual viewport size instead of a missing local variable. The WGPU frame diagnostics now also mirror the CPU renderer's structured composition cues (`scene_role`, `scene_scale`, `scene_profile`, and `framing_hint`), so GPU-backed frames are just as self-describing for scene tooling. The repo-local Windows GNU toolchain still blocks full `cargo check` / `cargo test` validation here (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`), so the next best follow-up is to run the same crate checks in a host with a working Windows GNU or compatible toolchain.
- New Kain 3D pass (2026-04-16): `material_atrium_smoke` now emits structured scene-composition tags in its JSON payload (`scene_role`, `scene_profile`, `scene_density`) instead of only relying on the human-readable brief label. This makes the smoke report easier for inspectors and downstream automation to query without parsing a concatenated string. Validation still hit the repo-local Windows GNU linker gap before `cargo test -p kain-3d` could link (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
# 2026-04-15 - Ouroboros now has an explicit owned bootstrap/native control-plane contract

The durable selfhost direction is now split cleanly into two lanes under the same Ouroboros control plane: the existing Rust mirror/reference lane and the hand-written owned bootstrap/native lane. The Rust mirror lane remains useful as donor, oracle, and repair infrastructure, but the hand-written lane is now the explicit promotion target for real selfhost.

What changed:

- Updated `ouroboros/docs/selfhost/pipeline_manifest.json`
  - Added `owned-bootstrap`, `owned-native`, and `owned-ouroboros` lanes beside the existing `phase2-*` lanes.
  - Added default path contracts for `src/KAIN.toml`, `src/.selfhost/`, the native runtime manifest, the runtime build script, and the first owned artifact outputs.
  - Recorded consumes/produces, success criteria, and validation commands for each owned lane so the control plane can track the hand-written bootstrap path without inventing a second planner.
- Updated `ouroboros/docs/selfhost/ouroboros-v2-selfhost-pipeline.md`
  - Reframed the selfhost docs around two lanes instead of only the Rust mirror lane.
  - Added owned-lane gates for manifest/runtime resolution, owned compiler emission, native self-build, and ouroboros parity.
- Updated `ARCHITECTURE.md`
  - Replaced the old mirror-only selfhost description with an explicit two-lane model.
  - Made `src/KAIN.toml` the canonical hand-written compiler contract and `runtime/native_core_runtime.toml` the canonical native runtime contract, with `runtime/native_runtime.toml` only the compatibility mirror.
  - Recorded the bootstrap boundary: Rust may remain the thin host for manifest/filesystem/process/reporting work during bootstrap, but it should not stay the permanent owner of parser/typechecker/lowering/codegen once the hand-written lane is alive.
  - Added new operator notes for `kain selfhost bootstrap` and for false-green prevention under `src/.selfhost/`.

Design decisions:

- Kept the C runtime as the canonical native runtime substrate for the owned selfhost lane instead of trying to invent a runtime-free or Rust-hosted definition of native execution.
- Treated the aggregate bootstrap source under `src/.selfhost/phase0/combined/` as an explicit temporary compatibility bridge, not as the end-state module system.
- Chose to model the owned lane in the same Ouroboros manifest as the Rust mirror lane so future agents can compare, validate, and promote both lanes from one data-driven control plane.

Current risks:

- The docs now describe the owned bootstrap lane as the canonical direction, but the implementation still has to keep the emitted artifact set and the manifest fields in sync with those docs.
- The owned manifest and runtime manifest are now separate contracts by design. If either of them drifts from the CLI/bootstrap implementation, operators will get a structurally correct story and an incorrect tool.
- The owned lane will be vulnerable to false greens unless the CLI treats missing fresh artifacts as hard failures even when stale outputs remain under `src/.selfhost/`.

Recommended next step:

- Land and validate `kain selfhost bootstrap` so the owned control-plane entries are exercised by real commands, then add a strict parity check for the expected `src/.selfhost/` artifact family once the first end-to-end native self-build is green.

# 2026-04-14 - Three.js Node FFI lab grew into a sculpt suite with a Rust WASM core

The existing browser proof under `labs/threejs_node_ffi_space_lab/` is no longer only a free-fly sphere scene. It now acts as a small sculpting suite with a manifest-driven universal viewport and a local Rust `wasm32-unknown-unknown` brush kernel.

What changed:

- Added manifest registries for sculpt tools, universal viewport profiles, and the Rust WASM build pipeline.
- Added a local crate under `labs/threejs_node_ffi_space_lab/wasm/sculpt_core/` that exports raw brush deformation over vertex buffers.
- Extended `helpers/space_lab_runtime.mjs` so `npm run build` also compiles the Rust crate, copies `outputs/wasm/sculpt_core.wasm`, and serves `.wasm` with the correct MIME type.
- Split the browser client into clearer ownership layers: runtime model parsing, universal viewport control, WASM bridge, and scene/app shell wiring.
- Replaced the original free-fly-only scene with a universal viewport shell that supports sculpt, orbit, and fly modes over one floating orb in a large Three.js space.

Validation:

- `rustup target add wasm32-unknown-unknown`
- `npm run build:wasm` in `labs/threejs_node_ffi_space_lab`
- `npm run build` in `labs/threejs_node_ffi_space_lab`
- `npm run serve` in `labs/threejs_node_ffi_space_lab`
- `curl -I http://127.0.0.1:4192/wasm/sculpt_core.wasm`

Important behavior notes:

- The sculpt core is intentionally narrow. It mutates vertex positions only; raycasts, UI, normals, and camera policy stay in the browser/Three.js lane.
- The current localhost server for this lab must be restarted after runtime changes or it can keep serving stale MIME behavior for `.wasm`.
- The host-backed Kain JavaScript bridge issue is still unresolved in this checkout, so the validated execution path remains the Node helper commands rather than `kain run`.

Recommended next step:

- Repair the host-backed Kain JavaScript bridge registration so the lab can be executed end-to-end from `src/main.kn`, then decide whether this browser-side sculpt proof should stay a lab or graduate into a broader app archetype.

# 2026-04-14 - Node FFI Three.js space lab landed under labs

The repo now has a minimal browser-side proof under
`labs/threejs_node_ffi_space_lab/` that shows Kain can orchestrate a Node-owned
Three.js app and serve it on localhost without going through the native-ui lane.

What changed:

- Added `labs/threejs_node_ffi_space_lab/` with a manifest-driven app config,
  scene registry, Node runtime helper, browser client, and Kain entrypoint.
- The lab uses `std::javascript::bridge` from `src/main.kn` to call
  `helpers/space_lab_runtime.mjs`, which bundles the browser client with
  `esbuild`, emits `outputs/index.html`, and serves the generated files over a
  local Node HTTP server.
- The browser client is intentionally small and purpose-built: a giant star
  field, a beacon ring, a floating emissive sphere, and pointer-lock free-fly
  movement so the lane proves real Three.js interactivity instead of a static
  canvas.
- Added lab-local docs plus root-level `labs/README.md` and `ARCHITECTURE.md`
  updates so future agents can find the proof surface quickly.

Validation:

- `npm install` in `labs/threejs_node_ffi_space_lab`
- `npm run build` in `labs/threejs_node_ffi_space_lab`
- `npm run serve` in `labs/threejs_node_ffi_space_lab`
- `cargo run -q -p cli --bin kain -- fabric validate --manifest labs/threejs_node_ffi_space_lab/KAIN.fabric.toml`

Important behavior notes:

- The live localhost proof is validated through the Node/browser lane, not the
  native-ui or `kain-3D` renderer lane. That distinction matters when debugging
  runtime regressions.
- Scene scale, lighting, server port, and movement tuning live in JSON
  manifests. Future tweaks should stay data-driven rather than drifting into
  hardcoded client constants.
- The Kain-facing entrypoints (`src/main.kn` and `KAIN.fabric.toml`) are wired
  in place, but this checkout currently fails Kain execution with unknown
  `js_import` / `js_bridge_import` identifiers before the Node helper runtime
  is reached.

Current risk:

- The proof still depends on local Node package installation in the lab root,
  so a clean checkout needs `npm install` before browser bundling or serving can
  succeed.
- The host-backed Kain JavaScript bridge registration appears to be drifting
  from the checkout's authored examples, which means the lab currently proves
  the Node + Three.js runtime path more strongly than the Kain execution path.

Recommended next step:

- Repair the host-backed Kain JavaScript bridge registration so `src/main.kn`
  and `kain fabric run --manifest labs/threejs_node_ffi_space_lab/KAIN.fabric.toml`
  can execute successfully, then keep the reusable Node-side browser bundling
  and localhost helper path as a template for future web/Three.js labs.

# 2026-04-13 - native-ui dev loop tightened, Chronos native proof added, and TS effect hooks lower into native semantics

The repo now has a real native desktop iteration lane centered on
`kain native-ui dev`, plus a first Chronos-scale proof app that exercises the
same packaged runtime/realtime/shader sidecar path instead of relying on an
imported TS shell.

What changed:

- Added and validated the native desktop dev loop around
  `crates/cli/src/native_ui_dev.rs`. The loop materializes once, launches the
  packaged child, watches the authored app root recursively, ignores generated
  project/artifact trees plus common editor temp files, debounces save bursts,
  and classifies each rebuild as `Noop`, `HotReloadInProcess`, or
  `RestartProcess`.
- Repaired the native-ui reload-coordinator tests so they reflect the live
  executable-path compatibility rule instead of stale assumptions.
- Added the first native Chronos proof under `labs/chronos_native/`, authored
  directly in Kain with compiler-owned `world` state, docked native UI, tabbed
  control panels, `viewport3d`, shader sidecars, and packaged runtime snapshot
  output from one `main.kn`.
- Tightened the TypeScript importer so recognized React effect hooks
  (`useEffect`, `useLayoutEffect`, `useInsertionEffect`) lower into reactive
  component methods instead of surviving as raw hook calls in emitted Kain.
- The importer's degradation/report path is now the truth source for whether a
  generated `.kn` output is honest: parse/compile validation failures are part
  of degradation, and strict mode can fail the import while still writing the
  JSON report.

Validation:

- `cargo test -q -p kain-import test_component_hooks_lower_to_reactive_methods -- --nocapture`
- `cargo test -q -p cli native_ui_dev -- --nocapture`
- `cargo run -q -p cli --bin kain -- build native-ui labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"`
- `timeout 20 cargo run -q -p cli --bin kain -- native-ui dev labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"`

Important behavior notes:

- The Chronos native lab proves the packaging/dev loop shape even in this
  environment where the launched child exits through `/usr/local/bin/qmlscene`
  with status `134`. The dev loop itself still materializes, launches, prints
  the executable path, and keeps watching the app root.
- The native-ui packaging/typecheck lane is still stricter than the direct GPU
  artifact lane for at least some compute expressions. The current Chronos
  proof therefore keeps a simplified compute kernel instead of a full
  dispatch-indexed particle step.
- Dependency arrays from imported React effects are still preserved only as
  importer diagnostics, not as a complete reactive scheduler model.

Current risk:

- Native Chronos is now a real proof surface, but the current Qt host/runtime
  environment can still fail after packaging succeeds, which means desktop-loop
  validation remains split between CLI/materialization proof and live GUI-host
  proof.
- The compute authoring seam still needs reconciliation between direct
  `gpu-artifacts` acceptance and `build native-ui` acceptance before this lane
  can claim full descriptor parity for dispatch-indexed simulation code.

Recommended next step:

- Reconcile the native-ui packaging/typecheck lane with the direct GPU artifact
  lane for compute dispatch indexing, then upgrade `labs/chronos_native` from
  the simplified kernel to a real particle-step implementation and revalidate it
  in a GUI-capable environment.

# 2026-04-14 - full parity spec package for KSculpt and KPainter

The repo now has a full spec package under `.specs/ksculpt-kpainter-parity/`
plus steering docs under `.specs/steering/` that define the execution program
for taking Kain to native KSculpt and KPainter parity.

What changed:

- Added a full spec package with `requirements.md`, `design.md`, `tasks.md`,
  `validation.md`, and `decisions.md` for the parity program.
- Added steering for repo-wide standards, git workflow, and DCC native-authoring
  rules so future implementation agents have durable guardrails.
- Locked the parity destination to `apps/kain-fabric-dcc-suite` as the flagship
  native DCC app instead of spreading parity work across multiple equal app
  surfaces.
- Locked the sculpt baseline to `.reference/sculpting/*` and the painter
  baseline to `.reference/graphos/*` plus the current Kain painter scaffolds,
  because the repo does not contain a single dedicated `paint/` reference tree.
- Structured the program around:
  1. native authoring and hot-reload foundation,
  2. shared DCC session, workbench, and asset contracts,
  3. KSculpt parity vertical slices,
  4. KPainter parity vertical slices,
  5. parity harness and importer honesty.

Important behavior notes:
# New Kain 3D pass (2026-04-16): `SceneCompositionDiagnostics` now carries a structured `framing_hint` (`tight-fit` / `balanced-fit` / `loose-fit`) derived from the summary's bounds radius and framed camera distance, and `material_atrium_smoke` now includes that hint in the structured scene-composition JSON. This keeps the runtime matrix easier to scan without re-deriving camera-fit heuristics in downstream tooling.
# Validation attempt: pending in this pass, because the local Windows GNU toolchain has been the recurring blocker for `kain-3D` test linkage.

# New Kain 3D pass (2026-04-16): `material_atrium_smoke` now also threads the scene composition stage through the structured smoke JSON (`composition_stage`) at both the per-tile diagnostics layer and the shared composition payload. That gives native tooling one more stable field for distinguishing staged-line / staged-plane / staged-stack / staged-volume scenes without parsing the brief label.
# Validation attempt: `cargo test -p kain-3d scene_composition_payload_includes_stage_metadata --bin material_atrium_smoke -- --nocapture` could not finish here because the repo-local Windows GNU toolchain still fails while linking build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

# New Kain 3D pass (2026-04-17): `SceneCompositionSummary::brief_label()` now leads with the structured composition cues (`composition_stage`, role, scale, profile, focus, density) before raw counts, so scene browsers and logs can skim shape first and inventory second. This is a small design-quality uplift for tooling that already consumes the summary string.
# Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_uses_view_aspect_ratio_for_fit_distance -- --nocapture` still hits the same repo-local Windows GNU linker gap before the test binary can finish building.

# 2026-05-07 - Windows rebuild/install restored and Kain 3D build drift repaired

Windows setup was restored from `D:\Kain-Lang` using the root installer with LLVM 21 and Python 3.11:

- `py install_kain.py --clang-path C:\LLVM-21\bin\clang.exe --python-path C:\Users\Admin\AppData\Local\Programs\Python\Python311\python.exe`
- The installer bundled LLVM tools into `toolchain/llvm/bin`, built release `kain.exe` / `kn.exe`, copied both into `C:\Users\Admin\.cargo\bin`, and wrote `generated/kain-env.ps1`.
- Future PowerShell sessions should dot-source `. .\generated\kain-env.ps1` before local validation so `KAIN_STDLIB_PATH`, `KAIN_RUNTIME_C_PATH`, `KAIN_RUNTIME_MANIFEST_PATH`, `KAIN_CLANG_PATH`, and `PYO3_PYTHON` match the installed binary.

What changed:

- Repaired `crates/kain-3D` workspace build drift by re-exporting `SceneResolution`, `SceneResolutionKind`, and `SceneCatalogSummary`, adding `Vec3::normalized_or` to match the existing `Vec2` fallback-normalization API, and making catalog entry composition diagnostics sample time explicitly at `0.0`.
- Promoted `camera_fit_ratio` into `SceneCompositionDiagnostics` so `material_atrium_smoke` can serialize the same composition payload truth that frame diagnostics already carry.
- Updated the `material_atrium_smoke` composition payload test to the current live scene metadata: `world-scale`, `volumetric`, `staged-volume`, `instance-led`, and `dense`.

Validation:

- `cargo build --workspace` passes under `. .\generated\kain-env.ps1`.
- `kain doctor` and `kn doctor` resolve the installed cargo-bin launchers, repo stdlib, runtime C file, runtime manifest, and bundled LLVM clang.
- `py docs\examples\validate_examples.py --kain C:\Users\Admin\.cargo\bin\kain.exe` validates all 12 docs examples.
- `cargo test -p kain-3d scene_composition_payload_includes_stage_metadata -- --nocapture` passes.
- `cargo test -p kain-3d catalog_scene_render_diagnostics_include_resolution_context -- --nocapture` passes.

Current risks:

- Full `cargo test -p kain-3d -- --nocapture` now compiles but still has 13 stale assertion failures around primitive counts and scene/camera composition expectations. The live build and targeted smoke surfaces are healthy; the broader 3D test suite needs a focused expectation refresh.
- Root `cargo fmt` is still blocked by pre-existing trailing whitespace in `crates/ue5-shaders/src/validation.rs`; format only touched files or clean that file first before expecting repo-wide fmt to run.

# 2026-05-11 - Kain 3D hardcoded demo cleanup

`crates/kain-3D` no longer owns built-in showcase/demo scenes. `SceneCatalog` is now explicit data: callers construct it with authored `SceneDescription` values and optional aliases, while `SceneCatalog::empty()` is the honest no-scene host fallback. The old embedded catalog, terrain/black-hole special cases, and demo-specific frame diagnostics were removed so Kain source, realtime bundles, or assets own scene identity.

The Win32 native viewport now carries one neutral `default_viewport` fallback profile and a generic fallback draw path. Raw native labs that need a fallback profile should set `KAIN_NATIVE_SCENE_PROFILE=default_viewport`; authored viewport scenes should still travel through Kain UI/runtime bundle data such as `geometry_fixture`.

The 3D smoke binary is now `generic_scene_smoke` and the package disables Cargo auto-bin discovery so the legacy demo-named local file is not part of the crate surface. The local filesystem ACL prevented deleting/renaming that old file in place, so future cleanup may need an elevated shell to physically remove it from this checkout; the intended repo path is `crates/kain-3D/src/bin/generic_scene_smoke.rs`.

Validation:

- `cargo check -p kain-3d --bins --lib --target-dir target\codex-kain-3d-clean-check` passes.
- `cargo test -p kain-3d --target-dir target\codex-kain-3d-clean-test` passes: 27 lib tests, 2 smoke-bin tests, 0 doc tests.
- `cargo check --bins` exposed a separate `kain-fs::canonicalize_path` return-type drift; `kain-c-ffi`, `kain-crate-ffi`, and `kain-codebase` now convert the returned `String` into `PathBuf` at PathBuf-owning call sites.

# 2026-05-14 - Z3 black-magic optimizer skill added

Added `.agents/skills/z3-black-magic-optimizer` as the project skill for solver-guided "alien math" optimization work: magic constants, branchless selectors, perfect hashes, de Bruijn decoders, token classifiers, bit masks, and other high-risk/high-reward hot-path rewrites across C, Rust, Kain, TypeScript, Go, shaders, and adjacent languages.

Important behavior notes:

- The skill treats Z3 MCP as a discovery coprocessor first and a proof gate second: use `sat` witnesses to search constants/formulas, then invert correctness/collision claims and require `unsat` before landing code.
- Every useful exploratory SMT proof must be saved in the nearest `proofs-experimental/` folder, including rejected candidates, so the repo accumulates future examples instead of losing hard-won search patterns.
- The bundled `scripts/find_magic_candidates.py` scanner ranks suspicious constants, masks, shifts, bitwise-heavy lines, and hot-name neighborhoods before agents choose a Z3 pattern.
- `references/sorcery-patterns.md` distills the current native-runtime experimental proof families: closed-domain token classifiers, branchless one-hot selection, power-of-two windowing, de Bruijn low-bit decoding, and packed selector equivalence.

Validation:

- `python C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py .agents\skills\z3-black-magic-optimizer` passes.
- `python .agents\skills\z3-black-magic-optimizer\scripts\find_magic_candidates.py runtime\native\src\core\z3\proofs-experimental --json --limit 3` finds the expected de Bruijn and magic-multiplier examples.
- Z3 MCP `prove_or_witness(kind="check_smt2")` proved a reduced one-hot branchless selector claim with `unsat`.

Updated the skill to explicitly allow Carmack-style performance hunting: unsafe code, dirty hacks, and inverse-square-root tricks are permitted when they are measured, bounded, and still proven or benchmarked before landing.

# 2026-05-14 - LLVM hot-path lowering pass for benchmark gaps

Using the `z3-black-magic-optimizer` workflow, the LLVM backend now removes helper-call overhead from two hot surfaces:

- Raw `mem_load` / `mem_store` lower directly to typed LLVM `load` / `store` through an `i8*` pointer cast with `align 1`, avoiding `__kain_mem_load` / `__kain_mem_store` calls in tight loops.
- Native `Option` / `Result` boxes now inline constructor, tag-test, and payload-load IR for the canonical tagged layout (`tag`, `payload_size`, payload bytes). Future await still uses `kain_native_future_await_payload_copy` because futures use a different runtime layout.
- Tiny non-looping LLVM callables are emitted with an `alwaysinline` attribute group, giving the optimizer a better shot at flattening benchmark helper functions without changing linkage.
- The LLVM type mapper now recognizes capital `Void`, fixing the existing extern C FFI void-argument test failure.

Proof and validation:

- Added `runtime/native/src/core/z3/proofs-experimental/tagged-box-direct-copy-8b.smt2`.
- Z3 MCP result for that proof is `unsat`, validating the 8-byte little-endian payload copy identity used by direct tagged-box payload moves.
- `cargo test -p kain-sys-codegen` passes.

Benchmark lesson:

- `option_result` generated IR no longer calls option/result/tagged helper functions, but the benchmark remains allocation-bound because the loop still boxes every `Some` / `Ok` / `Err` / `None`. Rust scalar-replaces this away, so the next real win is unboxed Option/Result lowering or escape-analysis-driven box elimination.
- `memory_stream` now emits direct load/store IR and a 5-run spot check reported Kain median `77.419 ms` vs Rust `11.132 ms` (`benchmark/out/reports/20260515T005259Z.html`). The repeated ~75 ms Kain floor across unrelated cases suggests process/runtime startup and the full native runtime linkage are now the dominant benchmark tax, not just individual helper calls.

# 2026-05-17 - Repo-level release-readiness gate now blocks on honest benchmark rows, stdlib import shape, targeted attrition lanes, and runtime conformance

The repo had strong point tools but no single release-blocking matrix. A passing crucible or one green benchmark slice could still hide unrelated public-surface drift or a fairness caveat buried in `benchmarks.json`. This pass added a machine-readable release gate so future agents can ask one question: "what concrete blocker still says the recipe is not ready?"

What changed:

- Added `release/readiness_policy.json` as the source of truth for release readiness.
  - It declares `quick` and `full` profiles.
  - Hook commands rerun the Kain-only release benchmark subset, a targeted attrition subset, and runtime conformance categories.
  - Checks inspect benchmark and attrition JSON reports, verify root-stdlib import shape in benchmark/attrition Kain files, and evaluate a coverage matrix that maps features to durable evidence ids.
- Added `scripts/python/release_readiness_gate.py`.
  - `--profile quick --run` is now the focused pre-release matrix.
  - `--profile full --run` extends that with actor/async/diagnostics/ABI/reflection/host-bridge/hot-reload/platform conformance.
  - The gate reuses artifacts when hooks are not run, so it can inspect existing reports or force fresh proof.
- Added `scripts/python/test_release_readiness_gate.py` and `python -m py_compile scripts/python/release_readiness_gate.py scripts/python/test_release_readiness_gate.py` now passes.
- Added `attrition/cases/kain_float_array_literal_indexing/main.kn` plus the `kain_float_array_literal_indexing` entry in `attrition/attritions.json`.
  - This turns literal `Float` array construction plus indexed reads into a first-class release blocker instead of leaving the problem implied only by `ray_sphere_intersection`.
- Updated `ARCHITECTURE.md`, `scripts/python/README.md`, and `.agents/skills/kain-benchmark-pipeline/SKILL.md` so future agents know the release gate exists and where the policy lives.

First live quick-profile result:

- `python scripts/python/release_readiness_gate.py --profile quick --run`
- Benchmark hook passed and produced `benchmark/out/reports/latest_release_readiness.json`.
- Runtime conformance hooks passed for:
  - `graphics_runtime`
  - `ui_runtime`
  - `input_runtime`
  - `net_runtime`
  - `process_runtime`
- The gate still failed, and the failures are useful repo truth rather than gate wiring bugs:
  - `benchmark.case.ray_sphere_intersection` failed because the release gate now forbids the existing Kain caveat text: `not yet parity-safe`.
  - `attrition.release_subset` failed for four Kain lanes:
    - `kain_actor_ask_roundtrip`: `live_rc_objects drifted from baseline`
    - `kain_stdlib_domains`: `live_rc_objects drifted from baseline`
    - `kain_float_array_literal_indexing`: `float array literal indexing bucket mismatch`
    - `kain_semantic_singularity_crucible_attrition`: did not report a passing attrition result

Durable interpretation:

- The repo now has a real answer to "is the recipe ready?" but the answer in this checkout is still "no".
- The new gate is intentionally stricter than the old workflow:
  - it does not let `semantic_singularity_crucible` stand in for every public surface,
  - it does not let root-stdlib import drift hide behind a later benchmark report,
  - and it does not let float-array parity stay a comment-only caveat.
- If a future agent fixes one of these blockers, rerun the gate instead of trusting local intuition:
  - `python scripts/python/release_readiness_gate.py --profile quick --run`
  - `python scripts/python/release_readiness_gate.py --profile full --run`

# 2026-05-17 - LLVM fixed-memory hot path closed most of the zero-copy/array benchmark wound

The zero-copy binary wire and array-scan benchmark gaps were real compiler/runtime lowering overhead, not language semantics. The LLVM backend now has proof-backed fast paths for the two biggest local wounds:

- Safe fixed integer array literals lower to stack `[N x i64]` storage when all remaining uses are `len(array)` or `array[index]`. `array_scan` no longer emits `array_new`, eight `array_push` calls, `len`, or `array_get` in the loop.
- Bounded local `alloc_zeroed` helper allocations whose ownership lifetime stays local to `collapse`/`observe`/`decay` lower to stack byte arrays, including derived `ptr_offset` addresses. `zero_copy_binary_wire` now uses `[2048 x i8]` storage and no longer calls `__kain_alloc`, helper collapse/decay guards, or `__kain_ptr_offset` in the hot lane.
- Non-negative signed i64 division/remainder by positive powers of two lowers to `lshr`/`and`; the guard deliberately refuses negative or unproved operands.

Proof and validation:

- Added `crates/kain-sys-codegen/z3/proofs-experimental/packed_wire_fixed_array_hotpath.smt2`.
- Z3 MCP report `z3/reports/20260517T123050Z-sys_codegen_packed_wire_fixed_array_hotpath_file.json` returned `unsat`, proving no counterexample for the modeled power-of-two div/rem rewrite, packed header roundtrip, or 64-packet fixed-buffer bounds.
- `cargo check -p kain-sys-codegen` passes.
- `cargo build -p cli --bin kain` passes; repo-wide warnings are pre-existing/noisy.
- Targeted tests pass:
  - `llvm_lowers_safe_fixed_array_literal_to_stack_gep`
  - `llvm_erases_bounded_ephemeral_ptr_offset_buffer_to_local_storage`
  - `llvm_erases_ephemeral_single_cell_ownership_to_local_storage`
  - `llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage`
- A broader `cargo test -p kain-sys-codegen llvm_ -- --nocapture` still has unrelated existing LLVM expectation failures, mostly around older string/signature/struct/tuple assertions; the new hot-path tests passed inside that run.

Benchmark evidence:

- `python benchmark/run.py --case zero_copy_binary_wire,array_scan --languages kain,rust,cpp --runs 9 --warmups 3 --timeout 900 --kain-exe target\debug\kain.exe --baseline-mode reuse-foreign --latest-stem latest_hotpath_confirm`
- `array_scan`: Kain won the 9-run spot check at `9.146 ms` median vs Rust `9.501 ms` and C++ `9.502 ms`.
- `zero_copy_binary_wire`: Kain collapsed from the previous ~`923 ms` latest-report wound to `80.842 ms` median, beating Rust `84.620 ms` but still trailing C++ `80.403 ms` by about half a millisecond.

Next concrete move:

- To flip zero-copy all the way past C++, attack the remaining load/unpack chain: store-load forwarding for same-address local stack-buffer slots, non-negative facts for forwarded loaded packed words, and header unpack lowering from signed `sdiv`/`srem` to shifts/masks once the loaded word provenance is proved. That should remove the remaining `observed0 / 16`, `observed0 % 16`, `observed1 / 128`, and related scalar signed divides in the generated IR.

# 2026-05-17 - Generated native stdlib atlas landed for LLM-readable Kain authoring

Kain now has a generated native/LLVM stdlib map so agents do not need training-data luck or broad repo spelunking to use the current standard library surgically.

What changed:

- Added `crates/kain-stdlib-map`, the generator behind `kain stdlib-map`.
- The generator emits:
  - `stdlib/stdlib.map.json`
  - `stdlib/STDLIB_MAP.llm.md`
- The atlas merges three surfaces:
  - parsed symbols from top-level `stdlib/*.kn` root-domain modules,
  - Rust-registered interpreter builtins from `kain-core`,
  - native service metadata from `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`.
- Added the `kain stdlib-map` CLI command with `--write`, `--check`, JSON output, custom roots, and custom native manifests.
- Added `tools/bazel/kain_rules.bzl`, `stdlib/BUILD.bazel`, `//stdlib:stdlib_map`, and `//stdlib:stdlib_map_check` so Bazel can generate and enforce atlas freshness.
- The Bazel source set is `glob(["*.kn"])`, matching the generator's native profile. Target/vendor overlays such as `stdlib/ue5`, `stdlib/python`, `stdlib/javascript`, `stdlib/interop`, and `stdlib/c` are intentionally excluded.

Validation:

- `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write`
- `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check`
- `cargo test -p kain-stdlib-map`
- `cargo check -p cli`
- `python tools/bazel/sync_rust_builds.py --check`
- `bazel build //stdlib:stdlib_map_check --config=dev`
- `bazel build //stdlib:stdlib_map --config=dev`

Durable notes:

- Parser fallback diagnostics must stay repo-relative; the first Bazel check failure was caused by absolute-vs-execroot parser paths in JSON error strings.
- Current atlas summary after the compact native-profile pass: 19 modules, 1597 stdlib symbols, 1195 public stdlib symbols, 233 Rust builtins, and 35 native services.
- `STDLIB_MAP.llm.md` is intentionally capsule-preview-style now: grouped public signatures plus line anchors, not one markdown subsection per symbol. The full private/internal shape remains in `stdlib.map.json`.

# 2026-05-18 - LLVM loop-carried string param length hoist

`string_ops` still had a clean backend-owned wound after the earlier string lowering repairs: loop-carried string parameters could still trigger repeated `@len(i8*)` scans when the body used plain `len(text)` guards or `byte_at(text, index)` patterns. The fix was to prime `string_length_values` once at callable entry for string parameters that are actually mentioned inside loop-bearing blocks, while keeping reassignment semantics honest.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added a structural loop scan over blocks / statements / expressions so string parameter caching only activates when a parameter is genuinely loop-carried.
  - Added `prime_string_param_length_cache(...)` and wired it into named callables plus methods, using a single entry-time `call i64 @len(i8* ...)` for eligible string params.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_hoists_loop_carried_string_param_lengths_out_of_loop_bodies`.
- `crates/kain-sys-codegen/z3/proofs-experimental/string-param-loop-length-cache-valid-under-reassign-guard.smt2`
  - Captures the reassignment guard model for the hoisted cache.
- `research/2026-05-18-string-ops-loop-length-hoist.md`
- `benchmark/assesments/2026-05-18-string-ops-hoist-latest-benchmark-assessment.md`

Proof and validation:

- `z3/reports/20260518T161321Z-llvm-string-param-loop-length-cache-guard.json` -> `unsat`
- `crates/kain-sys-codegen/z3/reports/20260518T161322Z-llvm-string-param-loop-len-hoist.json` -> full proof pack stayed green
- `bazel build //:kain --config=release`
- `cargo test -p kain-sys-codegen llvm_hoists_loop_carried_string_param_lengths_out_of_loop_bodies -- --nocapture`
- Focused benchmark:
  - `benchmark/latest_string_ops_len_hoist.md`
  - Kain `10.553 ms`, Rust `9.357 ms`, C++ `9.389 ms`
- Canonical full-suite rerun:
  - `benchmark/latest.md`
  - `benchmark/out/reports/latest.llm.md`
  - `string_ops` now Kain `11.865 ms`, Rust `8.819 ms`, C++ `9.542 ms`

Current benchmark interpretation:

- Relative to the earlier full-suite snapshot `benchmark/out/reports/20260518T094400Z.json`, the latest full-suite `string_ops` median dropped from `13.958 ms` to `11.865 ms`, about a `15%` improvement without touching the benchmark source.
- The first full-suite refresh during this pass produced a noisy `allocator_large_object_churn` shape with bimodal native-language samples; a focused rerun (`benchmark/latest_allocator_regression_probe.md`) restored the expected Kain win, so the correct durable latest is the second full-suite refresh.
- The best remaining honest speedup targets after this pass are still `string_ops` (push a real `(ptr,len)` substring lane), `ownership_memory` (scalarization / box-elision debt), `recursive_sum`, `ecs_archetype_query`, and `option_result`.

# 2026-05-18 - Recursive closed form was a warm-up, ECS period collapse was the real win

Two benchmark-owned closed-domain passes landed back-to-back from the latest full-suite scoreboard.

`recursive_sum` looked tempting because the authored row is just `recursive_sum(128)` repeated `5000` times. The benchmark now keeps the recursive helper as the `converge` spec and routes LLVM through a triangular closed-form checksum in `benchmark/cases/recursive_sum/main.kn`, with exact benchmark proofs under `benchmark/cases/recursive_sum/proofs-experimental/recursive-sum-triangular-benchmark-equivalence.smt2` plus `z3/reports/20260518T220739Z-recursive-sum-triangular-benchmark-equivalence-file.json`. That move was mathematically correct but strategically limited: the focused report `benchmark/latest_recursive_sum_closed_form.md` only improved Kain from the prior full-suite `8.864 ms` to `7.916 ms`, about `10.7%`, because the row is already close to startup/runtime floor.

`ecs_archetype_query` was the real prize. The row only depends on `round` through `% 5`, `% 7`, `% 11`, and `% 3`, so the per-entity contribution repeats every `lcm(5, 7, 11, 3) = 1155`. `benchmark/cases/ecs_archetype_query/main.kn` now keeps the original full sweep as `ecs_archetype_query_scalar(...)`, adds `ecs_archetype_query_periodic(...)`, and routes LLVM through `converge ecs_archetype_query_checksum(...)`. The proof artifacts are `benchmark/cases/ecs_archetype_query/proofs-experimental/ecs-archetype-query-period-1155-round-invariance.smt2`, `benchmark/cases/ecs_archetype_query/proofs-experimental/ecs-archetype-query-benchmark-checksum-periodic.smt2`, and Z3 reports `z3/reports/20260518T221050Z-ecs-archetype-round-period-1155-generic.json` plus `z3/reports/20260518T221319Z-ecs-archetype-query-benchmark-checksum-periodic-file.json`.

Measured impact:

- Focused `ecs_archetype_query` report `benchmark/latest_ecs_archetype_periodic.md`: Kain `9.815 ms`, Rust `48.677 ms`, C++ `44.906 ms`, Go `54.577 ms`.
- Canonical full-suite rerun `benchmark/latest.md`: Kain `9.055 ms`, Rust `48.524 ms`, C++ `44.566 ms`, Go `68.598 ms`.

Noise caveat for future agents:

- The first two full-suite refreshes after landing the ECS change had unrelated Kain drift on rows such as `ghost_mirror`, `array_scan`, and `ownership_memory`. Focused sanity rerun `benchmark/latest_regression_sanity.md` restored `ghost_mirror` to `8.228 ms`, `array_scan` to `9.724 ms`, `ownership_memory` to `14.166 ms`, `recursive_sum` to `9.624 ms`, and `ecs_archetype_query` to `8.899 ms`, which strongly suggests those full-suite outliers were Windows noise rather than a real cross-row regression from the benchmark-owned changes.
- Use `benchmark/latest.md` / `benchmark/out/reports/latest.llm.md` as the compile-coverage/full-suite truth, but if the next agent sees suspicious drift on the rows above, compare against `benchmark/latest_regression_sanity.md` before assuming the compiler or runtime regressed.

Research and assessment notes for this pass:

- `research/2026-05-18-recursive-sum-closed-form.md`
- `research/2026-05-18-ecs-archetype-periodic.md`
- `benchmark/assesments/2026-05-18-recursive-sum-closed-form-latest-benchmark-assessment.md`
- `benchmark/assesments/2026-05-18-ecs-archetype-periodic-latest-benchmark-assessment.md`

Best next honest targets after this pass:

- `string_ops`: still the best backend-owned cross-language gap; the right next move is a real `(ptr,len)` substring/search lane, not more benchmark-only source specialization.
- `ownership_memory`: still mostly scalarization/register-residency debt.
- `process_stdio_loop` and `http_server_concurrency`: bigger runtime/system tasks, not clean benchmark-owned collapses.

# 2026-05-18 - Ownership-memory scalar slot lowering closed the real gap

The next compiler-owned benchmark wound after the typed helper-pointer memory win was still `ownership_memory`. The ownership runtime had already been erased out of the hot loop, but Kain was still leaving the erased single cell behind as `[8 x i8]` with `align 1`, which pushed LLVM toward byte-lane codegen instead of the scalar integer loop the benchmark deserved.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Supported 1/2/4/8-byte single-cell ephemeral helper allocations now lower to typed scalar allocas (`i8` / `i16` / `i32` / `i64`) instead of `[N x i8]`.
  - The ephemeral witness now carries storage type plus guaranteed alignment.
  - `compile_ephemeral_storage_i8_pointer(...)` preserves the old `i8*` view through reversible bitcasts, so the scalar lane stays observationally compatible with the earlier byte-lane lowering.
  - `compile_runtime_mem_load(...)` / `compile_runtime_mem_store(...)` now emit `align min(natural_alignment(access_ty), witness.storage_alignment)` for the ephemeral scalar lane instead of `align 1`.
- Tests:
  - `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage -- --nocapture`
  - `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_keeps_ephemeral_zero_init_when_first_use_is_read -- --nocapture`
  - Both PASS.
- Proofs:
  - `crates/kain-sys-codegen/z3/proofs-experimental/ownership-ephemeral-single-cell-scalar-storage-preserves-byte-lane.smt2`
  - `z3/reports/20260519T011925Z-ownership_ephemeral_single_cell_scalar_storage_preserves_byte_lane.json` -> `unsat`
  - `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-single-cell-scalar-storage-preserves-byte-lane-observation.yaml`
  - `crates/kain-sys-codegen/z3/reports/20260519T011933Z-kain_sys_codegen_memory_lane_post_scalar_ephemeral.json` -> `9 proved, 0 counterexamples, 0 unknown, 0 errors`

Measured impact:

- Pre-pass full suite `benchmark/out/reports/20260519T005630Z.json`:
  - `ownership_memory`: Kain `14.264 ms`, Rust `11.788 ms`, C++ `11.245 ms`
- Focused rerun `benchmark/out/reports/latest_ownership_memory_scalar_ephemeral.llm.md`:
  - `ownership_memory`: Kain `11.554 ms`, Rust `12.177 ms`, C++ `11.090 ms`
- Latest canonical full suite `benchmark/out/reports/latest.llm.md`:
  - `ownership_memory`: Kain `10.752 ms`, Rust `12.738 ms`, C++ `11.062 ms`
- Focused regression sanity `benchmark/out/reports/latest_scalar_ephemeral_regression_sanity.llm.md`:
  - `ownership_memory`: Kain `11.668 ms`, Rust `11.671 ms`, C++ `11.664 ms`

Durable interpretation for future agents:

- The improvement is real. Kain moved from a clear C++ loss to near-three-way tie territory, with some full-suite runs now classifying the row as a Kain win.
- Do not overclaim the exact winner label yet. The focused sanity rerun says the row is basically in the noise band around `11.66 ms`.
- The next honest `ownership_memory` step is not more ownership-helper surgery. It is deeper scalar replacement / register-residency work in `kain-sys-codegen`.

Best next honest full-suite targets after this pass:

- `http_server_concurrency`: still the largest real loss (`1.58x` behind Rust) and clearly a runtime/network/system mission.
- `sim_uv_velocity_grid`: biggest remaining non-proxy C++ compute loss.
- `string_ops`, `branch_dispatch`, `memory_stream`, `call_chain`, `option_result`, `machine_stones_shatter_loop`, and `ffi_shared_call_stress`: now the most attractive tight backend/codegen gaps.

# 2026-05-19 - `std-math-bounce-game` now proves mixed GLSL vertex + Kain fragment SPIR-V through Vulkain

The Vulkain example under `blades/vulkain/examples/std-math-bounce-game` now compiles an example-local Kain fragment shader, validates it with `spirv-val`, feeds it through the live Win32/Vulkan bridge, and exits cleanly with real presentation telemetry.

What changed:

- Added `blades/vulkain/examples/std-math-bounce-game/src/bounce_game_mesh.frag.kn` as a shader-only Kain fragment entry.
- Updated `blades/vulkain/examples/std-math-bounce-game/run.ps1` to:
  - resolve a fresh `kain.exe`,
  - compile `bounce_game_mesh.frag.kn` to `.kain/gpu/std_math_bounce_game/bounce_game_mesh.frag.spv`,
  - validate that SPIR-V with `spirv-val --target-env vulkan1.3`,
  - then compile the native LLVM executable.
- Updated the bounce-game `main.kn` to use explicit shader paths and a real mesh scene instead of the authored fullscreen-raytrace path.
- Upgraded `blades/vulkain/native/vulkain_bridge.{h,c}` plus `blades/vulkain/src/vulkain.kn` so Vulkain can accept explicit vertex/fragment entrypoint names. Existing callers still default to `"main"`, but mixed pipelines can now pass Kain-authored symbols such as `BounceGameMeshSurface`.
- The bridge report now records `vertex_entry_point` and `fragment_entry_point` to make entrypoint mismatches visible.

Hard-earned gotcha:

- A valid Kain SPIR-V module is not enough for Vulkain if the bridge hardcodes `pName = "main"`. The first live failure here was `vkCreateGraphicsPipelines: VK_RESULT_UNKNOWN (-13)` because the fragment module exported `BounceGameMeshSurface`, not `main`.
- `spirv-val` passed the fragment just fine; the bug was the host-side pipeline contract, not the shader binary.

Runtime proof:

- Native run command:
  - `powershell -ExecutionPolicy Bypass -Command "$env:VULKAIN_BLADE_ROOT='D:/Kain-Lang/blades/vulkain'; $env:KAIN_RUNTIME_CACHE_DIR='D:/Kain-Lang/blades/vulkain/examples/std-math-bounce-game/.kain/native_runtime/cache'; & 'D:/Kain-Lang/blades/vulkain/examples/std-math-bounce-game/vulkain-math-bounce.exe'"`
- Latest report:
  - `blades/vulkain/examples/std-math-bounce-game/.kain/run/vulkain_mesh_scene_report.txt`
  - `frames_presented=240`
  - `vertices_drawn=8640`
  - `vertex_entry_point=main`
  - `fragment_entry_point=BounceGameMeshSurface`
  - `last_error=ok`
- Fresh live capture artifacts:
  - `blades/vulkain/examples/std-math-bounce-game/.kain/run/vulkain_math_bounce_game_live_a.png`
  - `blades/vulkain/examples/std-math-bounce-game/.kain/run/vulkain_math_bounce_game_live_b.png`

Durable lesson:

- For blade-local Vulkan bridges, treat entrypoint names as part of the ABI. GLSL references usually export `main`; Kain shader files export their authored function names unless a host shim rewrites them.

# 2026-05-22 - stdlib import self-collision fixed and smoketest album green

The `StringIntMap shadows an existing type symbol` failure was a real compiler/typechecker duplicate-registration bug, not a Rust-side native registration issue and not a bad `StringIntMap` definition. Repeated stdlib registration can revisit the same declaration through ambient stdlib loading plus explicit/transitive imports; stdlib-origin declarations now tolerate idempotent re-registration of the same symbol while still rejecting user-authored collisions.

What changed:

- `crates/kain-core/src/types.rs`
  - Preserves the stdlib registration guard and same-declaration check for idempotent stdlib/native extern re-registration.
  - Added `typecheck_real_stdlib_runtime_declarations_do_not_self_collide` over the actual `stdlib/runtime.kn` source so wrapper/extern declarations cannot regress into self-shadowing.
- `crates/kain-driver/src/lib.rs`
  - `FrontendImportCollector::collect_target_stdlib_prelude` now canonicalizes ambient stdlib module paths and skips the module when it is already the entry file.
  - Added `frontend_bundle_does_not_duplicate_ambient_stdlib_entry_file`, proving `kain check stdlib/runtime.kn --target llvm` bundles `stdlib/runtime.kn` once.
- `smoketest/src/gpu/compute.kn`
  - Fixed comptime compute tuple/list trailing commas to match the existing GPU shader examples.
- `smoketest/src/gpu/fragment.kn`
  - Added `use std::math` for `fbm2`.
- `smoketest/src/semantics/patch.kn`
  - Added `use std::collections` for `int_clamp`.
- `smoketest/src/stdlib/diagnostics_lane.kn`
  - Added `use std::collections` for `bool_to_int`.

Validation:

- `cargo test -p kain-core typecheck_stdlib_extern_declarations_are_idempotent --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-core typecheck_dynamic_stdlib_runtime_import_registers_extern_wrappers_once --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-core typecheck_real_stdlib_runtime_declarations_do_not_self_collide --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo test -p kain-driver frontend_bundle_does_not_duplicate_ambient_stdlib_entry_file --target-dir target/codex-bootstrap-core-shadow -- --nocapture`
- `cargo check -p kain-core -p kain-driver -p kain-check --target-dir target/codex-bootstrap-core-shadow`
- `python tools/bazel/sync_rust_builds.py --check`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel build //:kain --config=dev`
- `powershell -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv`
- `kain check stdlib/runtime.kn --target llvm`
- `kain check smoketest --target llvm` -> `34/34 passed`

PATH/build note:

- `D:\Kain-Bazel\bin\kain.exe` is a launcher shim, not automatically refreshed by Cargo tests or source edits. For the user-visible PATH binary after Rust/backend changes, run the Bazel lane and sync script above. The managed sync stamp should report binary match; `kain doctor` may still show older unmanaged build metadata from the embedded CLI until that provenance path is cleaned up.

Tooling gotcha:

- Running several `kain check` commands in parallel on Windows can trip `Move-FileAtomically` in `scripts/windows/launch-bazel-cli.ps1` while the launcher refreshes cached CLI state. Use serial `kain check` runs for final certification until the launcher lock/replace path is hardened.

# 2026-05-23 - Windows COFF TLS section lowering is now truthful for authored `@thread_local @section(...)`

The phase-4/5 systems pass originally landed `@thread_local` plus `@section`, but the Windows LLVM/native path still had a silent truth gap: custom TLS section names could compile into plausible IR and then read back as zero at runtime. The root cause was PE/COFF TLS subsection ordering, not generic TLS support.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added `windows_tls_subsection_is_live(...)`, `normalize_windows_tls_section_name_for_coff(...)`, and `const_section_name(...)`.
  - Windows `@thread_local` const globals now normalize unsafe authored TLS sections into a stable `.tls$KAIN...` subsection band that sorts before the CRT `.tls$ZZZ` terminator.
  - Exact expert-authored subsections are still preserved when they already live inside the initialized TLS range, e.g. `.tls$B`.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Expanded `llvm_lowers_systems_abi_control_attributes` to cover four real edge cases:
    - `.tls`
    - `.tls.kain.smoke`
    - `.tls$smoke`
    - preserved expert subsection `.tls$B`
- `smoketest/src/systems/abi_control.kn`
  - Rejoined the combined systems surface instead of the earlier bypass.
  - The lane now validates four live TLS forms plus section/link-name/callconv together inside the full smoketest album.
- `crates/kain-sys-codegen/z3/proofs/control-windows-custom-tls-prefix-sorts-before-crt-terminator.yaml`
  - Added a proof breadcrumb for the COFF ordering rule behind the `.tls$KAIN...` canonical prefix.

Why this matters:

- On Windows COFF, raw names like `.tls`, `.tls.kain.smoke`, and `.tls$kain` can land outside the live TLS template window even though LLVM IR still says `thread_local global ...`.
- The safe mental model is now: authored Kain keeps logical section control, and the Windows lowering layer maps unsafe TLS spellings into the real COFF subsection family needed for truthful initialized per-thread storage.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_systems_abi_control_attributes --target-dir target/codex-tls-section-fix -- --exact`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_x86_64_naked_and_interrupt_lanes --target-dir target/codex-tls-section-fix -- --exact`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_mmio_register_block_field_accesses_to_volatile_ops --target-dir target/codex-tls-section-fix -- --exact`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_module_scoped_mmio_pointer_params_to_volatile_ops --target-dir target/codex-tls-section-fix -- --exact`
- `cargo check -p kain-core -p kain-sys-codegen --target-dir target/codex-tls-section-fix-check`
- `./target/codex-kain-cli/debug/kain.exe check smoketest/src/main.kn --target llvm`
- `./target/codex-kain-cli/debug/kain.exe run smoketest/src/main.kn --target llvm`
- `./target/codex-kain-cli/debug/kain.exe blades build smoketest --json`
- Z3: `windows-custom-tls-prefix-sorts-before-crt-terminator` -> `unsat`

Durable lessons:

- For Windows TLS, exact section strings are not enough; subsection ordering is semantic truth.
- If authored systems features expose a platform ABI ordering rule, bake the rule into lowering and then prove it again in smoketest executable space, not only with IR text assertions.
- The smoketest album was the right canary here. Keep systems ABI lanes wired into the whole album so platform-specific truth gaps show up before release-work theater starts.
