---
name: kain-native-llvm-runtime
description: Work on Kain's LLVM native runtime semantic parity, including crates/kain-sys-codegen LLVM lowering, runtime/native C facade ABI, stdlib/native wrappers, native Option/Result/Future/async support, CPU capability/converge autotune services, and compiler-owned intent runtime hooks for patch, world, entangle, converge, and orchestrate. Use when adding, changing, debugging, validating, or reviewing Kain native LLVM runtime/codegen behavior.
---

# Kain Native LLVM Runtime

## Start Here

- Work from `D:\Kain-Lang`.
- Read `ARCHITECTURE.md` and `MEMORY.md` before changing runtime/codegen behavior.
- Treat `crates/kain-core` as language truth, `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` as LLVM lowering truth, and `runtime/native` plus `stdlib/native` as the native ABI/std-facing runtime truth.

## Key Files

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: LLVM lowering for semantic values, async/await, `?`, actor/runtime calls, worlds, entangle, patch, converge, and orchestrate.
- `crates/cli/src/main.rs`: native clang/link orchestration, native runtime bundle caching, benchmark-release toolchain tuning, and the link-time section-GC flags that decide whether dead stdlib/runtime wrapper forests survive into the final executable.
- `crates/kain-sys-codegen/z3`: durable LLVM proof pack for ABI layout math, match CFG label invariants, integer/bool cast semantics, and runtime bridge span preconditions in `codegen_llvm`.
- `runtime/native/include/stdlib_abi.h`: stdlib-facing C ABI declarations used by generated LLVM.
- `runtime/native/include/cpu.h` and `runtime/native/src/core/cpu.c`: CPU capability discovery for keys such as `cpu.x86.avx2` and `cpu.x86.avx512f`.
- `runtime/native/include/converge.h` and `runtime/native/src/core/converge.c`: process-local converge lane selector, tuning cache, telemetry ring, and winner commit substrate.
- `runtime/native/src/core/stdlib_abi.c`: C facade implementation for native semantic handles, filesystem, actor wrappers, and intent counters.
- `runtime/native/src/core/z3`: durable native-core proof pack for the C runtime substrate. It now covers actor, graphics, net, process, entangle, realtime, services, and stdlib arithmetic/bounds lanes.
- `runtime/native/src/core/core.c`: scalar builtin substrate including strings, arrays, the low-level map, threads, and socket helpers. The map hot path now relies on a power-of-two capacity invariant plus cached key `hash`, `key_length`, folded 32-byte `key_prefix` state, cached `mask`, and an 8-slot branchless probe window. The string hot path now matters just as much: `to_string(...)` is an exact-allocation decimal encoder, and long string-add trees can route through fixed-arity `str_concat3(...)` ... `str_concat10(...)` helpers that compute lengths once and copy into one RC string buffer.
- `runtime/native/include/async.h` and `runtime/native/src/core/async.c`: task/future/sleep substrate.
- `runtime/native_core_runtime.toml`: lean native manifest used by normal LLVM file builds.
- `stdlib/native/*.kn`: Kain-visible wrappers loaded for LLVM and direct C targets.
- `runtime/fixtures/native_option_result_future/main.kn`: focused LLVM proof for tagged `Option`, `Result`, `Future`, `async`, `await`, `?`, and unwrap helpers.
- `runtime/fixtures/native_world_actor_intent/main.kn`: broad LLVM/direct-C proof for worlds, entangle, actors, patch, law, converge, orchestrate, and native runtime counters.
- `runtime/native` now lazily initializes the pooled actor scheduler on first actor spawn or actor-registry use. Pure compute programs should not assume `native_runtime_init()` pays the actor-thread startup cost.

## Current ABI Shape

- LLVM lowers semantic `Option<T>`, `Result<Ok, Err>`, and `Future<T>` to native tagged `i8*` handles.
- Native constructors and checks live behind `abi_option_*`, `abi_result_*`, `abi_tagged_*`, and `abi_future_*`.
- `?` residual propagation currently returns the existing `i8*` native handle when the enclosing function returns native `Option` or `Result`.
- Payload copy/extraction is strongest for scalar payloads. Add explicit fixtures before claiming parity for structs, tuples, arrays, slices, nested semantic handles, or owned strings.
- Tagged payload ownership is conservative; nested RC-managed payloads may leak until the ABI has type-aware destructors or retain/release callbacks.
- LLVM now guards raw `rc_retain(...)` / `rc_release(...)` emission on `i8*` values with a heap-only low-bit check. Immediate tagged handles and the `None` null sentinel must not pay external RC call overhead just to no-op in `core.c`; if Option/Result/native-handle rows regress, inspect `emit_heap_owned_i8_guard(...)` in `codegen_llvm/mod.rs`, the proof `runtime/native/src/core/z3/proofs-experimental/tagged-immediate-lowbits-defeat-heap-rc-guard.smt2`, and the optimized assembly before changing the C runtime.
- Native LLVM now has first-class lowering for enum `match`, numeric `for` loops over `range`, `print`/`println` via `stdout_write`, and built-in `vec!` / `format!` macro calls. Keep those paths live in fixtures instead of commenting them out once they work.
- Non-void function lowering must consume the final block expression through `compile_block_with_result` and emit a typed `ret` before the fallback `unreachable`. If an expression-bodied enum `match` function builds a PHI and then traps as `0x80000003` / `int3` in benchmark-release, inspect `compile_named_callable` before rewriting the Kain source to add explicit `return`.
- Native LLVM actor ask/reply is no longer an `i64`-only special case. `ask` / `ask_timeout` now use a real reply-port handle type (`P` -> `%KainReplyPort`), typed waits go through `kain_actor_reply_port_wait(...)` when codegen has target-type context, and scalar `i64` waits keep the narrower `kain_actor_reply_port_wait_i64(...)` fast wrapper.
- Native LLVM converge lowering emits a spec function plus up to eight fast-lane functions. Purely static eligible lanes collapse to the first fast lane; CPU-gated lanes build an eligible bitmask from `abi_cpu_*` calls and dispatch through `abi_converge_select_lane_for_key(...)`.
- In `KAIN_NATIVE_PROFILE=benchmark-release`, LLVM orchestrate stage begin/end telemetry wrappers are elided unless `KAIN_LLVM_ORCHESTRATE_TRACE=1` is explicitly set. Keep this benchmark-only fast path result-preserving; do not hide semantic work inside begin/end hooks.
- In non-debug native builds, the CLI now passes `-ffunction-sections` and `-fdata-sections`, then enables linker dead-stripping (`/OPT:REF` + `/OPT:ICF` on Windows, platform equivalents elsewhere). The ready-future async benchmark depends on this to drop the giant unused stdlib/native wrapper forest after lowering collapses the actual `await` work away.
- The reply leg of that roundtrip is intentionally specialized: `send reply_to.Reply(value = ...)` lowers to `kain_actor_reply_port_send(...)` instead of generic `kain_actor_send(...)`, and `runtime/native/src/core/actor.c` keeps a tiny inline payload fast path inside reply-port state before falling back to heap-backed reply storage.
- String-length caching is now lazy, not eager. `compile_string_length_value(...)` memoizes `strlen(...)` on first use for string locals, but `compile_named_callable(...)` and method lowering no longer emit entry `strlen(...)` calls for every string parameter up front. If a benchmark-shaped renderer suddenly shows dead `call i64 @strlen(i8* %argN)` instructions at function entry again, inspect that seam before rewriting source.
- Long string-add trees now have a dedicated lowering path in `compile_string_concat_expression(...)`. For 3-10 terms, LLVM should emit one fixed-arity `@str_concatN(...)` call instead of a left-growing ladder of binary `@str_concat(...)` calls. The manual JSON row depends on this; if `render_payload(...)` falls back to nested concat calls, the benchmark will get ugly fast.
- Top-level `const` values are real LLVM globals. Literal scalar consts can lower as `internal constant`; runtime-backed consts use internal mutable globals with a lazy initializer that stores before setting the init flag. Every const identifier load must emit the init call before loading.
- Native artifact staging exists in both the CLI helper (`crates/cli/src/llvm_native_stage.rs`) and the blade build path (`crates/kain-build/src/workspace.rs`). Mixed native+shader files must extract shader-only source before optional SPIR-V shader bundle compilation so native stdlib wrappers are not typechecked under the GPU target.
- The builtin map in `core.c` is no longer the old djb2 plus `%` plus unconditional `strcmp` probe path. It now uses a chunk-mixed 64-bit hash, power-of-two masking, a folded 32-byte key prefix state, branchless 8-slot probe selection, and metadata-preserving rehash. If you touch `map_get`, `map_set`, `kain_map_probe_window`, or `MapEntry` / `KainMap`, rerun the native proof lane before trusting a benchmark.
- `Expr::Match` lowering is sensitive to CFG shape: arm-condition blocks, guard-fail cleanup blocks, no-match fallback blocks, and the merge/PHI block must stay distinct. Reusing the merge label as the last condition block or letting guard-fail paths skip scope cleanup produces invalid LLVM IR or RC leaks.
- Textual LLVM stack slots must be inserted through `emit_entry_alloca`. Emitting `alloca` in the active loop/control block can create per-frame stack growth in long-running native UI loops; the Win32/GL workbench once crashed reproducibly at frame 711 from this class. Pair entry-block insertion with named SSA locals from `next_reg()` (`%rN`) so later entry alloca insertion cannot violate LLVM's ordered unnamed `%0` register rules.
- `crates/kain-sys-codegen/z3` now proves the current `align_abi_size`/`abi_layout_for_ty` arithmetic, `next_label`/`next_reg` uniqueness, and integer/bool cast lowering. Rerun it after touching those helpers before trusting test-only evidence.
- Float coercions still need care: `double -> i1` currently behaves like `fcmp one value, 0.0`, so `NaN` becomes `false`, and `double -> i64/i32/i8` only stays defined when the source value is finite and within the destination integer domain.

## Intent Runtime Notes

- `patch` lowering should call `abi_patch_begin`, record typed mutations such as `abi_patch_record_i64`, and commit on normal returns.
- Compiler-owned entangle registration should call `abi_entangle_register`, not the public Kain wrapper name `entangle_register`. The wrapper can be generated from `stdlib/native`, so reusing that name in preemitted LLVM declarations creates duplicate-definition IR.
- Authority-side world writes should propagate through entangle metadata and call `abi_entangle_record_i64`; mirror writes should remain rejected.
- `converge` should keep all fast lanes alive until lowering. Static targets can direct-call; CPU capability lanes should use the native CPU/converge selector service. Keep real SIMD kernels as C/FFI or runtime-native kernels first, then prove spec/fast equivalence before adding Kain-authored SIMD IR.
- `orchestrate` stage calls should emit stage begin/end records around the lowered call except in explicit benchmark-release lowering, where wrappers may be elided if the stage result is unchanged and trace override is off.
- Runtime counters are process-local parity hooks, not durable transaction logs.

## Validation

Use a fresh or recently rebuilt CLI when proving executable fixtures:

- `cargo build -p cli` builds the Rust compiler host only. The native C runtime bundle compiles on demand during `kain build ... -t llvm` / `-t c`, or explicitly through `runtime/compile_native_runtime.sh` and `runtime/compile_native_runtime.ps1`.
- Prefer `kain runtime build` when you want the first-class CLI operator entrypoint for standalone native runtime bundle compilation. It forwards to the existing platform wrapper after resolving the repo root.
- Prefer `kain runtime validate` when you want the aggregate operator lane for CLI build + runtime build + fixtures + conformance. It forwards to the existing validation wrapper and supports the skip flags needed for narrow smokes.
- Prefer `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="full")` when you need the durable native-core solver pass, and use focused lanes like `graphics`, `realtime`, `services`, or `stdlib` for fast hotspot reruns after local C changes.
- Prefer `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen", lane="layout|control|casts|memory|full")` after touching `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`, especially ABI layout math, match CFG lowering, or integer/bool cast helpers.
- Prefer the aggregate runtime validation entrypoints when you need to prove the operator surface instead of a single fixture:
  `./runtime/validate_native_runtime.sh`
  `powershell -ExecutionPolicy Bypass -File runtime\validate_native_runtime.ps1`

- For ready-future async work, also keep this lane live after changing `stdlib_abi.c`, `async.c`, `codegen_llvm/mod.rs`, benchmark manifests, or native link flags:
  `py -3 benchmark\run.py --case async_ready_chain --languages kain,rust --runs 7 --warmups 2 --timeout 900 --kain-exe target\debug\kain.exe`

- For POD aggregate and native tagged-handle work, also keep these live:
  `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_option_result_future_to_native_tagged_runtime -- --nocapture`
  `py -3 benchmark\\run.py --case struct_method,option_result --languages kain,rust,cpp --runs 5 --warmups 1 --timeout 900 --kain-exe target\\debug\\kain.exe`

- For converge CPU/autotune work, also keep these live:
  `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_world_patch_converge_and_orchestrate_paths -- --nocapture`
  `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_enum_match_parameters_as_native_enum_pointers -- --nocapture`
  `cargo build -p cli`
  `toolchain\\llvm\\bin\\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/cpu.c`
  `toolchain\\llvm\\bin\\clang.exe -fsyntax-only -Iruntime/native/include runtime/native/src/core/converge.c`
  `py -3 tools/bazel/sync_native_runtime_builds.py --check`
  `target\\debug\\kain.exe blades\\converge-autotune-probe\\src\\main.kn -t llvm -o blades\\converge-autotune-probe\\.kain\\out\\converge-autotune-probe.ll`
  `blades\\converge-autotune-probe\\converge-autotune-probe.exe`
  `py -3 benchmark\\run.py --case evolutionary_loop --languages kain,rust,cpp --runs 3 --warmups 1`
  Z3 MCP: `check_smt2` or proof-pack pass for `runtime/native/src/core/z3/proofs-experimental/converge-autotune-selector-ring.smt2`; expect `unsat`.

- For actor ask/reply work, also keep these live:
  `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply -- --nocapture`
  `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply -- --nocapture`
  `target\\debug\\kain.exe check runtime\\fixtures\\native_actor_ask_roundtrip\\main.kn --target llvm`

```powershell
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'
cargo test -p kain-core --test semantic_typecheck_test --target-dir target\codex-actor-runtime-cli -- --nocapture
cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\codex-actor-runtime-cli -- --nocapture
cargo build -p cli --target-dir target\codex-actor-runtime-cli
target\codex-actor-runtime-cli\debug\kain.exe check runtime\fixtures\native_option_result_future\main.kn --target llvm
target\codex-actor-runtime-cli\debug\kain.exe build runtime\fixtures\native_option_result_future\main.kn -t llvm -o target\codex-native-runtime-proofs\native_option_result_future.ll
target\codex-native-runtime-proofs\native_option_result_future.exe
target\codex-actor-runtime-cli\debug\kain.exe check runtime\fixtures\native_world_actor_intent\main.kn --target llvm
target\codex-actor-runtime-cli\debug\kain.exe build runtime\fixtures\native_world_actor_intent\main.kn -t llvm -o target\codex-native-runtime-proofs\native_world_actor_intent.ll
target\codex-native-runtime-proofs\native_world_actor_intent.exe
target\debug\kain.exe check blades\kain-example\src\ui.kn --target llvm
powershell -NoProfile -ExecutionPolicy Bypass -File .\blades\kain-example\run-ui.ps1
```

For UI-loop stack regressions, inspect the emitted workbench IR before profiling deeper:

```powershell
rg -n " alloca " target\kain-example\kain_example_workbench.ll
toolchain\llvm\bin\llvm-as.exe target\kain-example\kain_example_workbench.ll -o target\kain-example\kain_example_workbench.bc
```

If `cargo build -p cli` hits disk exhaustion, delete only target directories created for the current pass after verifying their resolved absolute paths stay under `D:\Kain-Lang\target`.
