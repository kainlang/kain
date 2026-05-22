# Kain Feedback Log

## 2026-05-22 - stdlib module loading / typechecker

### StringIntMap Shadow: stdlib/collections.kn loaded twice under multi-import workspace
- Categories: correctness, regression, developer-experience
- Status: Patched
- Surface: typechecker
- Symptom: `error[TYPE:KAIN-TYPE-0001]: struct 'StringIntMap' shadows an existing type symbol` at `stdlib/collections.kn:88` — fires even on completely unrelated source files as long as they use `use std::runtime` alongside any second import that transitively reaches `std::collections`
- Workflow impact: Blocked the entire smoketest workspace (`kain check smoketest/src/main.kn --target llvm`) for the full session. Every file imports `std::runtime`; adding `std::alloc` or `std::text` (both of which do `use std::collections`) caused collections.kn to load twice and the typechecker to error on re-registration. Renaming `StringIntMap` to avoid the shadow did not help because the bug is the double-load, not the name. Three stdlib lanes had to be completely stubbed until you shipped the module dedup fix.
- Minimal repro: Any `.kn` file with `use std::runtime` plus `use std::alloc` (or `use std::text`) in a multi-file workspace. `kain check <file> --target llvm`
- Evidence: `error[TYPE:KAIN-TYPE-0001]: struct 'StringIntMap' shadows an existing type symbol --> stdlib/collections.kn:88:5`
- Suggested direction: Module registry dedup was the confirmed fix. Verify the dedup applies globally to the workspace import graph, not just per-file, so that any two files importing the same stdlib module through different transitive paths don't double-register its types.

---

## 2026-05-22 - typechecker / effect system

### comptime block constants are not visible in runtime function bodies
- Categories: correctness, developer-experience
- Status: Active
- Surface: typechecker
- Symptom: `error[TYPE:KAIN-TYPE-0001]: Unknown identifier 'SMOKE_COMPTIME_MAGIC'` when referencing a constant declared inside a `comptime:` block from a normal `pub fn` in the same file
- Workflow impact: Constants that semantically belong at comptime (e.g., version tags, surface counts, magic values) cannot be reused at runtime without duplicating them as top-level `const`. The `comptime` block in the specimen uses constants like `MYTHIC_SURFACE_COUNT` without issues, so either the specimen pattern works differently than expected or there is a scoping discrepancy between the spec and the live typechecker.
- Minimal repro: `comptime:\n    const FOO: Int = 42\npub fn bar() -> Int:\n    return FOO` — `kain check <file> --target llvm` errors on FOO
- Evidence: `error[TYPE:KAIN-TYPE-0001]: Unknown identifier 'SMOKE_COMPTIME_MAGIC' --> smoketest/src/semantics/comptime.kn:9`
- Suggested direction: Decide whether `comptime` constants should be promoted to module scope (accessible at runtime as regular consts) or remain strictly compile-time only with a clear diagnostic that says so. If they are compile-time only, the language spec/specimen should be updated to avoid implying they can be used in runtime fn bodies.

---

## 2026-05-22 - stdlib / naming

### stdlib function name collision: first_error defined in both std::diagnostics and user code
- Categories: correctness, developer-experience
- Status: Active
- Surface: stdlib / typechecker
- Symptom: `error[TYPE:KAIN-TYPE-0001]: function 'first_error' collides with an existing global from function --> stdlib/diagnostics.kn:58` when a user-authored workspace file defines a helper function named `first_error`
- Workflow impact: `first_error` is a natural helper name for any harness that tracks first-failing lane. The stdlib silently owns this name in the global namespace, causing a collision that is only surfaced at check time with no warning that the name is reserved. Required a rename to `smoke_first_error` throughout `main.kn`.
- Minimal repro: Define `fn first_error(offset: Int, result: Int) -> Int:` in any file that also imports `use std::runtime` (which triggers stdlib global loading). `kain check <file> --target llvm`
- Evidence: `error[TYPE:KAIN-TYPE-0001]: function 'first_error' collides with an existing global from function --> smoketest/src/main.kn:44, label 58:5: previous function 'first_error' is here (stdlib/diagnostics.kn:58:5)`
- Suggested direction: Either namespace stdlib globals under a module prefix at the typechecker level (so `first_error` in user code doesn't collide with `diagnostics::first_error`), or document all globally-injected stdlib names somewhere scannable so authors know which names are reserved. The current situation is silent until check time.

---

## 2026-05-22 - Windows launcher / check workflow

### Parallel `kain check` commands can race launcher cache replacement
- Categories: developer-experience, performance
- Status: Active
- Surface: tooling
- Symptom: One of four parallel `kain check <file> --target llvm` invocations failed with `Move-FileAtomically : Exception calling "Replace" with "4" argument(s): "Unable to move the replacement file to the file to be replaced."`
- Workflow impact: Parallel targeted checks are attractive for smoke triage, but the shared Windows Bazel launcher path can fail before Kain checking starts. This can look like a language failure unless rerun serially.
- Minimal repro: Launch multiple `kain check smoketest/src/... --target llvm` commands concurrently from `D:\Kain-Lang` immediately after a Bazel sync or launcher refresh.
- Evidence: Failure came from `scripts/windows/launch-bazel-cli.ps1:530` while three sibling targeted checks passed; rerunning the failed command serially passed.
- Suggested direction: Add a cross-process lock or retry loop around launcher replacement in `launch-bazel-cli.ps1`, or make `kain check` skip launcher cache replacement when the active synced binary already matches the stamp.

---

## 2026-05-22 - interpreter / stdlib / python bridge

### `std::fs` externs are not reliably usable from interpreter-mode Python-FFI runners
- Categories: correctness, developer-experience, interop
- Status: Bypass-Applied
- Surface: stdlib
- Symptom: `kain run ... --target interpret` can fail with `Runtime error: Undefined: abi_fs_create_dir_all` or `Undefined: abi_fs_path_join` even when the authored wrapper only wants to use `std::python::bridge` plus a small helper import.
- Workflow impact: The new `smoketest/telemetry/run_smoketest_mode.kn` wrapper could not write runner notes or even import `src/telemetry/report.kn` safely in interpret mode, which blocked the all-Kain telemetry/attrition runner until the wrapper was rewritten to keep checksum logic local and route note writes back through Python FFI instead of `std::fs`.
- Minimal repro: `cargo run -q -p cli --bin kain -- run smoketest/telemetry/run_smoketest_mode.kn --target interpret -- --mode attrition --executable <abs-smoketest.exe> --output-dir <abs-out>` with the wrapper importing `report::smoke_telemetry_track_checksum` or calling `fs_create_dir_all` / `fs_path_join`.
- Evidence: Initial failures were `Kain error: Runtime error: Undefined: abi_fs_path_join` and then `Kain error: Runtime error: Undefined: abi_fs_create_dir_all` from the interpret-target runner lane.
- Suggested direction: Either register the `std::fs` ABI surface for interpreter-mode runs that already support Python FFI, or make interpreter extern resolution lazy so importing a module with unused `@extern` fs helpers does not fail the whole run.

---

## 2026-05-22 - GPU / HLSL lowering

### HLSL shader lowering does not support `std::math::fbm2`
- Categories: correctness, developer-experience, gpu
- Status: Bypass-Applied
- Surface: gpu
- Symptom: the `gpu-hlsl` build task failed with `Kain error: Codegen error ... Unsupported function call in shader: 'fbm2'`.
- Workflow impact: The smoketest album built and certified almost completely, but the final DAG still failed because the fragment shader track used `fbm2`, which currently works in the math/std surface but not in HLSL shader lowering.
- Minimal repro: `cargo run -q -p cli --bin kain -- blades build . --json --clean` in `D:\\Kain-Lang\\smoketest`, or any HLSL-target fragment shader that calls `fbm2(uv, octaves)`.
- Evidence: `smoketest:gpu-hlsl` failed in `smoketest/.kain/reports/build/session-1779448114057-30408.json` with `Unsupported function call in shader: 'fbm2'`.
- Suggested direction: Teach the HLSL backend to lower `fbm2` (and likely adjacent std math shader helpers), or emit an earlier target-specific diagnostic during `check` so authors know which shader helpers are unavailable before the GPU artifact task runs.
