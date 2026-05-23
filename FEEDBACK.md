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

---

## 2026-05-22 - runtime / smoketest certification

### Native RC release-underflow diagnostics still leak through successful smoketest telemetry lanes
- Categories: correctness, developer-experience, runtime
- Status: Active
- Surface: runtime
- Symptom: `album-attrition`, `album-benchmark`, and `telemetry-full` all succeeded, but each task message still printed repeated native runtime `[MEMORY] ERROR: RC release underflow` diagnostics with code `9002`.
- Workflow impact: The C ABI album, telemetry flow, and full smoketest DAG now certify successfully, but the runtime still emits memory-failure noise on the most important proving-ground path. That makes it harder to trust a green certification run at a glance, because stderr looks like teardown corruption even when exit status is `0`.
- Minimal repro: `D:\\Kain-Lang\\target\\codex-smoketest-cabi-driverfix\\debug\\kain.exe blades build smoketest --json`
- Evidence: `smoketest/.kain/reports/build/session-1779457854992-28372.json` shows `smoketest:album-attrition`, `smoketest:album-benchmark`, and `smoketest:telemetry-full` succeeding while their task messages include repeated `RC release underflow` diagnostics; the embedded run reports include `session-1779458535954-22196.json`, `session-1779458747313-7452.json`, and `session-1779458958960-39684.json`.
- Suggested direction: Root-cause the surviving signed ref-count teardown path and either eliminate the underflow or promote it into a fail-fast runtime/attrition result so certification cannot look clean while the native substrate still reports RC corruption.

---

## 2026-05-22 - run reporting / telemetry workflow

### `kain run` reports success even when an interpret-target `main() -> Int` returns nonzero
- Categories: correctness, developer-experience, tooling
- Status: Active
- Surface: tooling
- Symptom: the outer run report records `status: "succeeded"` and `exit_code: 0` even when the Kain program's numeric output is a nonzero failure code such as `3001`.
- Workflow impact: The smoketest telemetry runner can look green at the `kain run` / build-task layer while `telemetry/full/summary.json` says the album failed, which makes certification triage materially harder.
- Minimal repro: `cargo run -q -p cli --bin kain -- run smoketest/telemetry/run_smoketest_mode.kn --target interpret -- --mode full --executable D:/Kain-Lang/smoketest/smoketest.exe --output-dir D:/Kain-Lang/smoketest/telemetry/full` when the inner executable returns a nonzero album failure code.
- Evidence: `smoketest/.kain/reports/run/session-1779458958960-39684.json` records `status: "succeeded"`, `exit_code: 0`, and `output: "3001"` for the failed full-mode run; the corresponding `smoketest/telemetry/full/summary.json` from that run reported `failure_code: 3001` and `failure_track: "interop.c_abi_album"`.
- Suggested direction: Treat a nonzero numeric `main() -> Int` result as a failed run status for interpret-target `kain run`, or add a separate explicit failure field that build/telemetry tasks honor instead of only the host process exit code.

---

## 2026-05-22 - project helper / native executable

### `native_executable` can fail under the default Bazel-resolved `kain.exe` even when the same project compiles cleanly with a working CLI binary
- Categories: correctness, developer-experience, build
- Status: Active
- Surface: build
- Symptom: the `compile_kain_project_to_root.ps1` helper can fail with `clang: error: no such file or directory: 'D:\\Kain-Lang\\smoketest\\smoketest.ll'` during the root-executable link step, despite the compile step having just printed `Compiled to: D:\\Kain-Lang\\smoketest\\smoketest.ll ...`.
- Workflow impact: `smoketest:root-executable` can fail inside `kain blades build smoketest --json` unless the run is forced to use a known-good CLI binary through `KAIN_BIN`, which undermines confidence in the default project authority path.
- Minimal repro: `kain blades build smoketest --json` from `D:\\Kain-Lang` with the default `native_executable` helper resolution path; compare with rerunning the same build while setting `KAIN_BIN=D:\\Kain-Lang\\target\\codex-smoketest-cabi-driverfix\\debug\\kain.exe`.
- Evidence: `smoketest/.kain/reports/build/session-1779459491862-32992.json` failed at `smoketest:root-executable`, and `smoketest/.kain/out/llvm/x86_64-windows/dev/x86_64-windows/smoketest/smoketest-root-executable/kain-evidence.json` captured the missing-`.ll` clang error. The same DAG later succeeded as `smoketest/.kain/reports/build/session-1779460677572-36848.json` when `KAIN_BIN` pointed at the cargo-built CLI.
- Suggested direction: Audit the Bazel-resolved CLI / helper-script path for raw-native output staging so the emitted `.ll` survives through the clang link step, or let `native_executable` prefer an explicitly resolved working `kain.exe` without requiring a manual env override.

---

## 2026-05-22 - LLVM / TLS section control
### `@thread_local` plus custom `@section` loses the authored initializer on Windows LLVM runs
- Categories: correctness, developer-experience, lowering, runtime
- Status: Bypass-Applied
- Surface: lowering
- Symptom: a `@thread_local` `const` with a custom TLS section reads back as `0` at runtime even though the emitted LLVM IR shows `thread_local global i64 7`.
- Workflow impact: The new systems ABI smoke lane initially failed with exit code `2002` because `@thread_local @section(".tls.kain.smoke") const ABI_TLS_COUNTER: Int = 7` behaved like zero-initialized storage in the executable path. I had to split the smoke so `thread_local` is exercised separately from custom section/link-name control instead of validating the combined surface directly.
- Minimal repro: Author a file with `@thread_local @section(".tls.kain.smoke") const TLS_COUNTER: Int = 7` and a `main()` that returns `TLS_COUNTER + 9`, then run `kain run <file> --target llvm` on Windows; the observed result comes back as zero-bias behavior instead of the expected initialized value.
- Evidence: `smoketest/.kain/cache/run/abi_control_probe.ll` contained `@__kain_smoke_tls_counter = thread_local global i64 7, section ".tls.kain.smoke"`, but the executable from `./target/debug/kain.exe run smoketest/src/systems/abi_control_probe.kn --target llvm` exited with `16`, implying the TLS read observed `0` while the plain `@thread_local` + separate sectioned const probe exited with `5007023`.
- Suggested direction: Audit the Windows LLVM/native link path for authored TLS globals placed in custom sections. If the backend cannot preserve initialized TLS semantics under arbitrary `@section`, either remap supported TLS sections to the platform's canonical TLS segment machinery or reject the combination with a target-specific diagnostic instead of silently emitting a zero-reading runtime artifact.
