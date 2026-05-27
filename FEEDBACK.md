# Kain Feedback Log

## 2026-05-24 - std::z3 / std::proof runtime dogfood

### Running a proof-heavy Kain lane trips RC release-after-free in the LLVM runtime
- Categories: correctness, regression, runtime, interop
- Status: Active
- Surface: runtime
- Symptom: `kain run runtime/native/src/core/z3/kain/main.kn --target llvm` exits with multiple `[MEMORY] ERROR: RC release after free` diagnostics and status code `9002`.
- Workflow impact: The new Kain-authored proof dogfood lane compiles cleanly, but end-to-end execution of `std::z3 -> std::proof -> std::test` on the LLVM/runtime path is blocked by a runtime lifetime bug instead of proof logic.
- Minimal repro: `.\target\debug\kain.exe run runtime\native\src\core\z3\kain\main.kn --target llvm`
- Evidence: `D:\Kain-Lang\.kain\reports\run\session-1779662265878-35024.json` and stderr containing repeated `RC release after free` diagnostics for string payloads while executing the Python-backed proof lane.
- Suggested direction: Audit refcount ownership across `std::python`, `std::z3`, and `std::proof` result/model/evidence strings; the failure shape suggests a host-backed object or bridged string is being released twice on the LLVM runtime path.

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
- Symptom: `kain run ... --target interpret` can fail with `Runtime error: Undefined: abi_fs_create_dir_all` or `Undefined: abi_fs_path_join` even when the authored wrapper only wants to use `std::python` plus a small helper import.
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

---

## 2026-05-23 - native JSON runtime / stdlib
### native JSON bridge mis-handled string values and broader non-int value lanes under LLVM
- Categories: correctness, developer-experience, runtime
- Status: Verified
- Surface: stdlib
- Symptom: Historical LLVM lowering and return-cleanup behavior flattened `JsonValue = Any` wrapper arguments to raw `i64` and released owned JSON locals before `return` cleanup finished, which broke string values, float values, bool arrays, and parsed string reads even though the public `std::json` wrappers typechecked cleanly.
- Workflow impact: This previously blocked honest runtime certification of the new `std::json` typed field/decode surface, forced an int-only narrowed probe, and kept `stdlib/requirements.md` at `PARTIAL`.
- Minimal repro: `.\target\debug\kain.exe run blades\stdlib-foundations\src\fmt_json_probe.kn --target llvm` on the pre-fix tree before narrowing the probe from strings/floats/bool-arrays to int-only JSON.
- Evidence: Fixed by JSON-aware direct-call lowering and owned-return retention in `crates/sys-codegen/src/codegen_llvm/mod.rs`, `json_retain` in `runtime/native/src/core/json.c`, and string helper cleanup in `stdlib/json.kn`. Z3 reports `z3/reports/20260524T012653Z-json-any-tag-partition.json` and `z3/reports/20260524T012653Z-json-owned-return-transfer.json` are `unsat`; the old broken return model still has a `sat` counterexample in `z3/reports/20260524T012706Z-json-old-return-transfer-counterexample.json`. Focused runtime validation now passes in `blades/stdlib-foundations/src/fmt_json_probe.kn` and `attrition/cases/kain_stdlib_foundations/main.kn`.
- Suggested direction: Keep the proof-linked comments near `compile_direct_call` and `Stmt::Return`; if JSON regressions reappear, inspect `JsonValue` argument boxing/tagging and owned return transfer before blaming `stdlib/json.kn`.
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
- Status: Patched
- Surface: lowering
- Symptom: a `@thread_local` `const` with a custom TLS section read back as `0` at runtime even though the emitted LLVM IR showed `thread_local global i64 7`.
- Workflow impact: The new systems ABI smoke lane initially failed with exit code `2002` because `@thread_local @section(".tls.kain.smoke") const ABI_TLS_COUNTER: Int = 7` behaved like zero-initialized storage in the executable path. The fix was to make Windows COFF lowering canonicalize unsafe authored TLS sections into a live `.tls$KAIN...` subsection band while still preserving expert-authored subsections that already sort before the CRT terminator.
- Minimal repro: Author a file with `@thread_local @section(".tls.kain.smoke") const TLS_COUNTER: Int = 7` and a `main()` that returns `TLS_COUNTER + 9`, then run `kain run <file> --target llvm` on Windows; the observed result comes back as zero-bias behavior instead of the expected initialized value.
- Evidence: `smoketest/.kain/cache/run/abi_control_probe.ll` contained `@__kain_smoke_tls_counter = thread_local global i64 7, section ".tls.kain.smoke"`, but the executable from `./target/debug/kain.exe run smoketest/src/systems/abi_control_probe.kn --target llvm` exited with `16`, implying the TLS read observed `0` while the plain `@thread_local` + separate sectioned const probe exited with `5007023`.
- Suggested direction: Keep the COFF TLS normalization rule documented and covered by both LLVM IR regression tests and full smoketest runtime coverage so future ABI/section-control work does not regress back into zero-reading custom TLS storage.

---

## 2026-05-23 - native CLI authoring / LLVM executable lane
### Native LLVM executables do not currently offer a trustworthy argv surface for authored Kain CLIs
- Categories: correctness, developer-experience, tooling, lowering
- Status: Patched
- Surface: lowering
- Symptom: authoring a grep-like native CLI exposed that the normal `args()` path was not trustworthy in the LLVM executable lane, forcing the blade to fall back to wrapper-written temp files just to try to pass CLI arguments.
- Workflow impact: This blocked the last mile of `blades/kg`: the blade compiled, built `kg.exe`, and the actor/file-search logic was in place, but the actual command-line UX could not be trusted. That turns a would-be first-class Kain CLI into a launcher hack and makes native tool authoring feel unfinished.
- Minimal repro: author a small `main() -> Int` entrypoint that reads `args()` and run `kain run <file> --target llvm -- hello`; compare that with interpreter-mode behavior where `args()` is explicitly defined in `crates/core/src/runtime.rs`.
- Evidence: during `blades/kg` bring-up on 2026-05-23, direct `args()` usage had to be removed after the native path behaved as if argv ingress was missing; a PowerShell wrapper then wrote `kg.args` beside `kg.exe`, and even that workaround became part of the debugging path instead of normal CLI forwarding.
- Suggested direction: make argv a first-class native-runtime contract for LLVM/direct-C executables, cover it with a tiny authored CLI smoke test, and document whether `args()` is guaranteed across interpreter, `kain run --target llvm`, and `native_executable` blade outputs.

---

## 2026-05-23 - actor ask / LLVM native authoring
### `ask(...)->String` actor replies used to materialize as integer-looking output in LLVM/native runs
- Categories: correctness, developer-experience, lowering, runtime
- Status: Patched
- Surface: lowering
- Symptom: an authored actor reply that should carry a `String` can arrive at the caller as a large integer-looking value instead of the text payload.
- Workflow impact: `blades/kg` initially searched correctly but printed nonsense like `1459838791848` in place of matching lines when `main()` asked worker actors for their accumulated output strings. The blade had to switch to actor-local `Flush` messages and reserve `ask(...)` for integer telemetry only.
- Minimal repro: author an actor with `state text: String = "hello"` and an `Output` handler that replies with `self.text`, then `print(ask(worker, "Output", 0))` under `kain run <file> --target llvm`.
- Evidence: before the `Flush` reroute, `D:\Kain-Lang\kg.exe 'kg_parse_config' 'D:\Kain-Lang\blades\kg\src\main.kn' --line-number` produced a bare integer-like payload instead of the two matching source lines. On 2026-05-23, LLVM/native lowering was patched to carry actor reply payload types, handler reply-contract inference was fixed for generic reply-port parameter names, and actor spawn lowering was fixed to honor authored state defaults instead of silently zero-filling omitted fields. `D:\Kain-Lang\runtime\fixtures\native_actor_ask_roundtrip\main.kn` now proves `Int`, `Bool`, and `String` ask/reply roundtrips under `kain run ... --target llvm`, including a `String` reply through a non-`reply_to` generic port name.
- Suggested direction: keep the fixture in the native proof lane so future actor/lowering work cannot regress `String` ask replies or actor state default initialization.

---

## 2026-05-23 - stdlib random proof lane
### `verify random(n)` currently rejects converge functions with pointer parameters
- Categories: correctness, developer-experience, proof
- Status: Active
- Surface: proof
- Symptom: `kain check stdlib/random.kn --target llvm` fails with `error[TYPE:KAIN-TYPE-0001]: converge 'shattered_rng_buffer_update' verify random(n) does not support parameter 'buf' of type ptr<Int>`
- Workflow impact: album-scale validation for unrelated stdlib work was blocked after targeted semver checks passed, because `smoketest/src/main.kn` transitively pulls in `std::random` and the proof rule rejects the existing pointer-based converge surface before full workspace validation can finish.
- Minimal repro: `kain check stdlib/random.kn --target llvm`
- Evidence: failure points at `stdlib/random.kn:258` on `pub converge shattered_rng_buffer_update(buf: ptr<Int>, output: ptr<Int>, lanes: Int) -> Int:` with the `verify random(n)` diagnostic above.
- Suggested direction: either extend `verify random(n)` so pointer-bearing converge signatures can be proved when the pointer arguments are not part of the randomized domain, or emit a more structured diagnostic with the supported parameter shapes and a sanctioned escape hatch for pointer-oriented converge kernels.

---

## 2026-05-23 - wasm authored semantics / codegen
### `alloc_zeroed` plus `collapse/observe/decay` in authored wasm specimens hit `Function 'map_new' not found`
- Categories: correctness, developer-experience, wasm
- Status: Active
- Surface: wasm codegen / authored semantics
- Symptom: `kain build website/kain/src/demonstration/neural_sieve.kn --target wasm` failed with `Kain error: Codegen error at Span { start: 82315, end: 82326 }: Function 'map_new' not found` when the specimen used `use std::alloc`, `alloc_zeroed(...)`, then `collapse cells: ...`, `observe cells: ...`, and `decay cells` inside helper functions.
- Workflow impact: the website’s new neural lattice wasm specimen only shipped after those helper functions were rewritten to scalar equivalents. `world`, `entangle`, `patch`, `actor`, `teleport`, and `converge` all lowered to wasm successfully in the same module, so future authored demos that try to bring ownership/allocation semantics into the wasm lane are likely to rediscover this exact blocker.
- Minimal repro: author a wasm-target `.kn` file that imports `std::alloc`, allocates a temporary pointer buffer with `alloc_zeroed`, and runs `collapse/observe/decay` over it, then build with `--target wasm`.
- Evidence: `website/kain/.kain/reports/build/session-1779568374054-2088.json`
- Suggested direction: inspect the wasm lowering path for authored allocation/ownership helpers and trace why it reaches a missing `map_new` dependency. This looks like a real authored-surface lowering gap rather than a website-specific bug.

---

## 2026-05-23 - benchmark v2 semantics authoring
### `check-llvm` and native LLVM compile disagree on direct writes to entangle mirror state
- Categories: correctness, developer-experience, tooling
- Status: Active
- Surface: lowering
- Symptom: an authored benchmark case in `benchmark/cases_v2/classic_systems.kn` passed `kain check ... --target llvm` even when it directly assigned `ClassicGhostMirror.signal_copy = 1`, but the native executable compile later failed with `cannot write entangle mirror 'ClassicGhostMirror.signal_copy' directly; write authority 'ClassicGhostAuthority.signal'`.
- Workflow impact: the v2 benchmark pack looked green at the check stage, but the real root executable build failed later in the blade/native compile lane, which burned time chasing a seeming checksum issue that was actually a lowering contract mismatch.
- Minimal repro: author an entangled world pair, assign the mirror-side field directly in a function, run `kain check <file> --target llvm`, then compile the same entry through the native executable lane.
- Evidence: `D:\Kain-Lang\benchmark\.kain\out\llvm\x86_64-windows\dev\x86_64-windows\benchmark-v2\benchmark-v2-root-executable\kain-evidence.json` included `error[Codegen Error]: while compiling 'ghost_mirror_checksum': cannot write entangle mirror 'ClassicGhostMirror.signal_copy' directly; write authority 'ClassicGhostAuthority.signal'` even though `check-llvm` had already passed.
- Suggested direction: make the `check --target llvm` path reject the same mirror-side writes that native lowering rejects, or downgrade native lowering to a shared earlier diagnostic pass so authored Kain gets one consistent truth.

---

## 2026-05-24 - python import gauntlet / test harness
### repo-root `kain test` loses importer-relative Python sibling/package resolution that direct `kain run` preserves
- Categories: correctness, developer-experience, interop, tooling
- Status: Active
- Surface: interop
- Symptom: a blade that uses first-class Python `import ...` with sibling `ecosystem_local.py` and local package imports under the same folder runs successfully with `kain run ... --target interpret`, but `kain test <blade-path> --target interpret` from repo root fails with `Runtime error: Python import error for 'ecosystem_local': ModuleNotFoundError: No module named 'ecosystem_local'`.
- Workflow impact: importer-relative local Python modules currently feel reliable in direct execution but not in the common repo-root source-test workflow, which makes the new Python import lane look flaky unless agents discover the working-directory workaround.
- Minimal repro: from `D:\Kain-Lang`, run `kain test blades/test/fabric_FFI/python/python_import_gauntlet --target interpret` and compare with `kain run blades/test/fabric_FFI/python/python_import_gauntlet/smoke.kn --target interpret`. The direct run succeeds; the repo-root test fails. Running `kain test smoke.kn --target interpret` from inside the blade directory passes.
- Evidence: direct run emitted the full gauntlet report and artifacts under `blades/test/fabric_FFI/python/python_import_gauntlet/outputs/`, while repo-root test failed with `ModuleNotFoundError: No module named 'ecosystem_local'`.
- Suggested direction: preserve/import `source_file` context into the source-test harness for Python `import` items, or teach the harness to seed importer-relative search roots before evaluating tests so blade-local sibling `.py` and package imports behave the same in `test` and `run`.

---

## 2026-05-27 - semantic_search native dogfood
### `check --target llvm` accepts tuple destructuring shape that native LLVM lowering rejects
- Categories: correctness, developer-experience, lowering
- Status: Bypass-Applied
- Surface: lowering
- Symptom: `kain check X:\mcp\semantic_search --target llvm` passed after adding `let (meta, norm, next_cursor, ok) = parse_one_meta(...)`, but native `kain run ... --target llvm` failed in codegen with `Unknown tuple storage type for pattern: __kain_tuple__IndexMeta__double_i64_i1`.
- Workflow impact: validation looked green until executable build, so the semantic-search reader fix had to be reshaped from tuple return into a named `ParsedMeta` struct before native proof could continue.
- Minimal repro: return a tuple containing a struct plus scalar values from a helper, destructure it in another function, then compare `kain check <project> --target llvm` with `kain run <entry> --target llvm`.
- Evidence: `X:\mcp\semantic_search\.kain\reports\run\session-1779861427025-8704.json`
- Suggested direction: either teach LLVM lowering this tuple storage shape or make `check --target llvm` reject unsupported tuple destructuring with the same diagnostic before run/build.
