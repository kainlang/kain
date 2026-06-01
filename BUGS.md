# Kain Bug Log

## 2026-06-01 - stdlib/python-runtime
### Python async future close can hang when the callback worker never settles

- Categories: runtime, stdlib, python, async, smoketest
- Severity: Medium
- Status: Open in tree (2026-06-01)
- Surface: authored Kain `std::python` async future and actor callback cleanup.
- Trigger: The smoketest Python async lane created a Python-backed future/callback and then tried to close it directly after bounded polling.
- Symptom: The lane can wedge the smoketest album instead of returning a failure result when the Python callback worker does not settle on this Windows host.
- Why this is a bug: Closing or canceling a pending host future should be bounded and observable. A stdlib proof lane should not be able to hang the whole `smoketest/build.kn` album just because an external async callback failed to complete.
- Minimal repro:
  - Restore the older direct close path in `smoketest/src/stdlib/python_async_lane.kn`.
  - Run `kain build smoketest` and watch the album stall in the stdlib Python async path or in a direct focused run of that lane.
- Evidence:
  - The 2026-06-01 low-level smoketest pass had to rewrite `python_async_lane.kn` to bounded-poll, cancel, and rely on a Kain relay actor tick instead of directly closing an unsettled callback future.
  - The same pass found and fixed a separate native heap-corruption bug in `python_runtime_async.c`: `settled_message` is allocated by the RC string path and must be released with `rc_release`, not `free`.
  - After that RC fix, the bounded async lane passed inside the full `kain build smoketest` album; the broader pending-close boundedness issue remains tracked here.
- Current workaround:
  - Use bounded polling and `python_future_cancel` for async proof lanes; do not call callback/future close paths after the worker fails to settle.
- Suggested direction:
  - Make Python future close/cancel idempotent and bounded, expose settlement/error state clearly, and add a focused runtime-backed stdlib test that proves pending callback cleanup cannot hang the caller.

---

## 2026-05-30 - sys-codegen/llvm
### Small authored Kain helper shapes can pass check but emit invalid LLVM PHI IR

- Categories: llvm, lowering, typechecker, developer-experience
- Severity: High
- Status: Open in tree (2026-05-30)
- Surface: authored Kain helpers lowered through `kain run --target llvm`.
- Trigger: Semantic oracle ranking helpers in `crates/semantic/src/search_engine.kn`, including an inline scalar `if` value and a small helper that accumulated `Array<String>` query tokens.
- Symptom: `kain check` passed, but `kain run src\main.kn --target llvm -- search kain "unknown identifier prntln expected println" 8` failed during LLVM validation with invalid PHI IR.
- Why this is a bug: The frontend/lowering pipeline accepted source that could not be emitted as valid LLVM, so the failure surfaced late as backend IR breakage rather than as a Kain diagnostic.
- Minimal repro:
  - Reintroduce an inline scalar `if` expression for the ranker's `best_raw` value or the small query-token `Array<String>.push` helper shape in `crates/semantic/src/search_engine.kn`.
  - Run `kain run src\main.kn --target llvm -- search kain "unknown identifier prntln expected println" 8` from `X:\crates\semantic`.
- Evidence:
  - `.kain\reports\run\session-1780180550066-3304.json`
  - `.kain\reports\run\session-1780180709584-31932.json`
  - `.kain\reports\run\session-1780180864800-32180.json`
- Current workaround:
  - Rewrite inline scalar `if` expressions as explicit `var` assignments.
  - Avoid accumulating `Array<String>` tokens in tiny helpers; use `text_tokenize_whitespace` and direct streaming metadata scoring.

---

## 2026-05-29 - c-ffi/include-lane
### Natural `include ... as ...` C bridge can emit duplicate LLVM declarations at link time

- Categories: importer, c-ffi, llvm, codegen
- Severity: Medium
- Status: Open in tree (2026-05-29)
- Surface: natural local-header include lane for C bridges.
- Trigger: Running a Kain file that uses natural include for a local bridge header (repro in `scratch/kdoom/interop/main.kn`).
- Symptom: `kain run --target llvm` fails in LLVM parse/link phase with duplicate declarations like:
  - `error: invalid redefinition of function 'kdoom_bridge_pixel_count'`
- Why this is a bug: generated LLVM IR includes duplicated `declare` entries for the same symbol in one module, making valid C bridge contracts non-runnable through this path.
- Minimal repro:
  - `kain run scratch/kdoom/interop/main.kn --target llvm`
- Evidence:
  - `.kain/cache/run/llvm/main-7d38610ca5ada4de.ll` contains repeated declarations for `kdoom_bridge_*` symbols.
- Current workaround:
  - Use explicit `use c::...` with `[c_ffi] tier = "dynamic"` and a bridge dll+import-lib pair (`scratch/kdoom/interop/main_usec.kn` + `kdoom_bridge.dll/.lib`), which runs successfully.

---

## 2026-05-29 - import/c-ffi
### Duplicate declaration inflation from `kain import-c` causes immediate type collisions on large legacy C trees

- Categories: importer, c-ffi, parser, type-system
- Severity: Medium
- Status: Open in tree (2026-05-29)
- Surface: `kain import-c` output hygiene for macro-heavy C codebases (repro'd with SM64).
- Trigger: Importing large C trees with repeated forward declarations and header fanout, e.g. `scratch/SUPERKAIN64/supermario64-c`.
- Symptom: Generated `.kn` contains many duplicate type/function declarations (`OSMesgQueue`, `Animation`, etc.), then `kain check` fails quickly on duplicate-symbol and shadowing diagnostics before deeper semantic validation.
- Why this is a bug: C allows repeated forward declarations under one tag namespace, but generated Kain currently materializes those repetitions as colliding declarations instead of coalescing/aliasing them.
- Minimal repro:
  - `kain import-c scratch/SUPERKAIN64/supermario64-c ... -o scratch/SUPERKAIN64/superkain64/sm64_full_import.kn`
  - `kain check scratch/SUPERKAIN64/superkain64/sm64_full_import.kn --target llvm`
- Evidence:
  - Duplicate-type error examples observed: `Animation`, `OSMesgQueue`, `fu`.
  - After first-pass post-dedupe, additional global conflicts remain (e.g. imported `clamp` shadowing builtins/global names).
- Suggested direction:
  - Add importer-side declaration coalescing keyed by ABI-equivalent type/function signatures before emitting `.kn`.
  - Preserve module ownership while avoiding global symbol collisions (auto-prefixing or namespaced lowering for repeated external C tags/functions).

---
## 2026-05-24 - runtime/stdlib_abi — patch journal audit (tool-z3-bug-hunter)

### Concurrent Slot Overwrite and Lost Update in Native Patch Journal

- Categories: correctness, race, runtime, stdlib
- Severity: High
- Status: Open in tree (2026-05-24)
- Surface: runtime
- Trigger: Concurrent patch recording calls that reach `abi_patch_record_i64` against the shared native patch journal.
- Symptom: Two writers can claim the same journal slot and publish only one count increment even when there is room for two sequential appends. When only one slot remains, both callers can still individually pass the capacity guard and report success on the same final slot.
- Why this is a bug: `abi_patch_record_i64` selects `g_kain_native_patch_journal[g_kain_native_patch_journal_count]` and increments `g_kain_native_patch_journal_count` without any writer serialization, so the slot claim and count publish are not atomic as a unit.
- Minimal repro: Two host threads concurrently call any patch-recording surface that reaches `abi_patch_record_i64`; the same risk applies to future Kain-side patch/test/proof flows that dogfood the patch journal concurrently.
- Evidence: The Z3 witness admits `initial_count = 0`, `read_a = 0`, `read_b = 0`, shared `slot_a = slot_b = 0`, and `final_count = 1`, which violates the expected two-success contract of distinct slots plus `initial_count + 2`.
- Z3 Proof: [native-stdlib-patch-journal-concurrent-slot-overwrite.yaml](file:///D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-stdlib-patch-journal-concurrent-slot-overwrite.yaml)
- Suggested direction: Reuse the service-registry commit gate pattern or add a dedicated patch-journal mutation lock so slot selection, entry population, and count publication happen in one serialized commit step.

---

## 2026-05-22 - runtime/memory — atomic ordering audit (tool-z3-bug-hunter)

### CAS Failure-Order Stronger Than Success-Order Produces C11 UB

- Categories: correctness, soundness, UB
- Severity: High
- Status: Fixed in tree (2026-05-22)
- Surface: runtime
- Trigger: Historical runtime ABI calls or old lowering paths could request a compare_exchange failure ordering whose C11 strength exceeded the chosen success ordering.
- Symptom: Silent C11 undefined behavior. `atomic_compare_exchange_strong_explicit` could be invoked with `failure_order > success_order`, violating C11 7.17.7.4.
- Why this was a bug: The old helper forwarded success and failure orderings without validating `failure_order <= success_order`, so bad pairs were able to reach the C11 primitive directly.
- Minimal repro: Historical bug only. Example shape: `__kain_atomic_compare_exchange_ordered(ptr, expected, desired, KAIN_MEMORY_ORDER_RELAXED, KAIN_MEMORY_ORDER_SEQ_CST)`.
- Evidence: Historical Z3 counterexample admitted a relaxed success ordering paired with a stronger failure ordering, which violates the compare_exchange contract.
- Historical counterexample: [native-memory-cas-failure-order-stronger-than-success-ub.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-cas-failure-order-stronger-than-success-ub.yaml)
- Fix landed: The runtime now normalizes invalid failure-order shapes, clamps any stronger-than-success failure ordering before the C11 primitive executes, and emits warning-once diagnostics when it had to repair the request. LLVM lowering now rejects invalid failure orderings up front, and the parser preserves explicit failure orderings so the lowering validation is not silently masked.
- Regression evidence: [native-memory-cas-failure-order-clamp-prevents-ub.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-cas-failure-order-clamp-prevents-ub.yaml), [memory-atomic-compare-exchange-validation-rejects-invalid-failure-orderings.yaml](/D:/Kain-Lang/crates/sys-codegen/z3/proofs/memory-atomic-compare-exchange-validation-rejects-invalid-failure-orderings.yaml), `cargo test -p kain-core compare_exchange_ -- --nocapture`, `cargo test -p kain-sys-codegen rejects_ -- --nocapture`, `bazel test //runtime:native_test_atomic_memory_ordering`

---

### Silent ACQUIRE→RELEASE Remap in Atomic Store With No Diagnostic

- Categories: developer-experience, correctness
- Severity: Medium
- Status: Fixed in tree (2026-05-22)
- Surface: runtime
- Trigger: Historical callers passed `KAIN_MEMORY_ORDER_ACQUIRE` or `KAIN_MEMORY_ORDER_ACQ_REL` to `__kain_atomic_store_ordered`, or lowering handed those invalid orderings to the plain-store ABI path.
- Symptom: The store silently used release semantics instead of surfacing a diagnostic. ACQ_REL on a store lost its acquire half with no warning.
- Why this was a bug: The old runtime mapping accepted invalid plain-store orderings and remapped them to release without any compile-time or runtime signal.
- Minimal repro: Historical bug only. Example shape: `__kain_atomic_store_ordered(ptr, value, KAIN_MEMORY_ORDER_ACQUIRE)`.
- Evidence: Historical Z3 counterexample admitted the silent `ACQUIRE -> RELEASE` remap.
- Historical counterexample: [native-memory-store-order-acquire-silently-downgrades-to-release.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-store-order-acquire-silently-downgrades-to-release.yaml)
- Fix landed: The runtime now emits a warning once before canonicalizing invalid plain-store orderings to release semantics, and LLVM lowering rejects acquire/acq_rel orderings for plain stores instead of silently remapping them.
- Regression evidence: [memory-atomic-store-validation-rejects-acquire-and-acqrel.yaml](/D:/Kain-Lang/crates/sys-codegen/z3/proofs/memory-atomic-store-validation-rejects-acquire-and-acqrel.yaml), `cargo test -p kain-sys-codegen rejects_ -- --nocapture`, `bazel test //runtime:native_test_atomic_memory_ordering`

---

## 2026-05-22 - runtime/ownership — write-ordering audit (tool-z3-bug-hunter)

### kain_ownership_clear_slot_unlocked Write-Order Creates OCCUPIED=1/DECAYED Transient (Lock-Free Future Risk)

- Categories: ordering, soundness
- Severity: Low
- Status: Fixed in tree (2026-05-22)
- Surface: ownership
- Trigger: Historical `kain_ownership_clear_slot_unlocked` published `state=DECAYED` before clearing `occupied`.
- Symptom: A future or accidental lock-free reader could observe `occupied=1` and `state=DECAYED` together and misread the slot as still alive while already terminal.
- Why this was a bug: The historical write order admitted the transient `occupied=1, state=DECAYED`, which was safe only because the current registry stayed under one lock.
- Minimal repro: Historical write-order hazard only.
- Evidence: Historical Z3 counterexample admitted `occupied=1, state=DECAYED`.
- Historical counterexample: [native-ownership-clear-slot-occupied-decayed-write-order-assumption.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-ownership-clear-slot-occupied-decayed-write-order-assumption.yaml)
- Fix landed: `kain_ownership_clear_slot_unlocked` now clears `occupied` before publishing `DECAYED` and leaves a proof breadcrumb in the helper, so the transient is gone even if a future agent experiments with a lock-free read path.
- Regression evidence: [native-ownership-clear-slot-clears-occupied-before-decayed.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-ownership-clear-slot-clears-occupied-before-decayed.yaml), `bazel test //runtime:native_test_ownership_memory`

## 2026-05-22 - runtime/ownership
### Stale Pointer Aliasing & Registry Capacity Leak in Decay
- Categories: correctness, resource-leak, aliasing, runtime
- Severity: Critical
- Status: Fixed in tree (2026-05-22)
- Surface: ownership
- Trigger: Normal usage of `__kain_ownership_decay` on heap allocations.
- Symptom: Long-running actor or memory-heavy programs will eventually fail with `KAIN_OWNERSHIP_ERR_CAPACITY` (ENOMEM). Reused pointer addresses will hit stale index collisions.
- Why this is a bug: In `ownership.c`, `kain_ownership_decay_slot_unlocked` called `__kain_free(ptr)` on heap regions but skipped `kain_ownership_clear_slot_unlocked` on the generic path. The region stayed marked as occupied, the old pointer remained in `KAIN_OWNERSHIP_POINTER_INDEX`, and the global slot leaked permanently.
- Minimal repro: Allocate and decay heap memory in a loop exceeding `KAIN_OWNERSHIP_MAX_REGIONS`.
- Evidence: Historical code inspection of `kain_ownership_decay_slot_unlocked` showed the free path returning without clearing slot occupancy or pointer-index state.
- Z3 angle: A state machine model of slot lifecycle showed the transition `IDLE -> DECAYED` never reached `UNOCCUPIED`, monotonically decreasing available slots.
- Historical counterexample: [native-ownership-decay-heap-fast-path-leaks-slot.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-ownership-decay-heap-fast-path-leaks-slot.yaml)
- Fix landed: Heap decay now always clears the helper slot and pointer index after a successful free, so generic heap decay no longer leaves stale occupied regions behind.
- Regression evidence: `bazel test //runtime:native_test_ownership_memory`

## 2026-05-22 - runtime/stdlib_abi
### UNC Path Creation Failure on Windows
- Categories: correctness, fs, runtime
- Severity: High
- Status: Fixed in tree (2026-05-22)
- Surface: runtime
- Trigger: Attempting to write a file or create directories using a UNC path (for example `\\\\server\\share\\file.txt`) on Windows.
- Symptom: The file write or directory creation fails immediately with a parent-directory error.
- Why this is a bug: In `stdlib_abi.c`, `abi_fs_create_parent_dirs` walked separators from index `1`, so UNC and extended Windows prefixes could be truncated into invalid root markers before any real path component was reached.
- Minimal repro: Call `fs_write_text("\\\\\\\\127.0.0.1\\\\C$\\\\temp\\\\test.txt", "data")` on Windows, or use an extended-length path such as `\\\\?\\C:\\temp\\nested\\artifact.txt`.
- Evidence: Historical code inspection showed the traversal loop treated root-prefix separators like creatable directory boundaries.
- Z3 angle: The old parser model admitted split points inside the Windows root prefix, producing invalid root strings for `CreateDirectoryA`.
- Historical counterexample: [native-stdlib-abi-unc-path-creation-fails.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-stdlib-abi-unc-path-creation-fails.yaml)
- Fix landed: The stdlib ABI now computes a Windows root-prefix span and skips drive, UNC, and extended-length prefixes before attempting intermediate directory creation.
- Regression evidence: `bazel test //runtime:native_test_stdlib_abi_fs`

## 2026-05-22 - runtime/actor
### Actor Monitor Record Memory Leak
- Categories: correctness, resource-leak, performance, runtime
- Severity: High
- Status: Fixed in tree (2026-05-22)
- Surface: actor
- Trigger: Termination of a monitored actor when the monitoring actor continues to run.
- Symptom: `KainActorMonitor` structs allocated via `malloc` are never freed or dequeued on exit notification, leaking heap memory and degrading list lookup performance over time.
- Why this is a bug: When a monitored actor exited, `kain_actor_notify_monitors` notified the monitoring actor but did not unlink or free the associated `KainActorMonitor` node from the monitor list.
- Minimal repro: Monitor short-lived actors in a loop using a single long-lived supervisor and observe the monitor list growing monotonically.
- Evidence: Historical code inspection of `kain_actor_notify_monitors` showed sends without a matching unlink or `free(monitor)`.
- Z3 angle: The old state transition model admitted a dead actor while an active monitoring record referencing its ID still existed.
- Historical counterexample: [actor-monitor-notification-leaks-relationship.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/actor-monitor-notification-leaks-relationship.yaml)
- Fix landed: `kain_actor_notify_monitors` now unlinks and frees matching monitor nodes as it emits exit notifications.
- Regression evidence: `bazel test //runtime:native_test_actor_monitor_link`

## 2026-05-22 - runtime/memory
### Unchecked Relocation Failure in Realloc
- Categories: correctness, soundness, UB, runtime, memory
- Severity: Critical
- Status: Fixed in tree (2026-05-22)
- Surface: memory
- Trigger: Relocation during `__kain_realloc` when `__kain_ownership_relocate_helper_allocation` fails.
- Symptom: The old implementation could physically move the allocation while the ownership registry kept tracking the stale address, causing heap-to-registry split-brain.
- Why this was a bug: The old `__kain_realloc` path called `realloc` first, then attempted to relocate ownership metadata. If metadata relocation failed, the function still returned the new payload instead of preserving the documented failure contract.
- Minimal repro: Historical bug only. The original write-up overstated an easy non-idle trigger, but the bad return path was real if relocation failed after the physical move.
- Evidence: Historical code inspection of `__kain_realloc` showed `return payload` even when `__kain_ownership_relocate_helper_allocation(...) != KAIN_OWNERSHIP_OK`.
- Z3 angle: The historical model captured the old split-brain outcome when physical relocation succeeded but registry relocation failed.
- Historical counterexample: [native-memory-realloc-relocation-failure-bypass.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-realloc-relocation-failure-bypass.yaml)
- Fix landed: `__kain_realloc` now stages a new allocation, copies bytes, commits the ownership relocation first, and only then releases the old allocation. If relocation fails, it releases the staged block and returns `NULL`, leaving the original pointer valid.
- Regression evidence: `bazel test //runtime:native_test_ownership_memory`

## 2026-05-22 - runtime/actor
### Lazy Init Double-Enter Race in Actor Runtime
- Categories: correctness, race, startup, runtime
- Severity: Critical
- Status: Fixed in tree (2026-05-22)
- Surface: actor
- Trigger: Concurrent first-touch actor calls that both force lazy runtime initialization.
- Symptom: Two threads could both enter actor runtime initialization, double-initialize global locks, and race scheduler startup.
- Why this was a bug: `kain_actor_runtime_init` historically guarded global startup with a plain `g_actor_runtime_initialized` flag, so the cold check and the transition into the init body were not serialized.
- Minimal repro: Two host threads concurrently call an actor API that reaches `kain_actor_runtime_ensure_initialized()` on a cold process.
- Evidence: Historical code inspection showed unsynchronized flag reads and writes before lock and scheduler initialization.
- Z3 angle: The historical model admitted two first callers both seeing the cold state. The landed atomic once-state proof closes that state space.
- Historical counterexample: [actor-runtime-init-plain-flag-double-enter.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/actor-runtime-init-plain-flag-double-enter.yaml)
- Fix landed: The runtime now uses an atomic `cold -> busy -> ready` once gate, a single CAS claim for the winner, spin-wait fallback for contenders, and a matching `ready -> busy -> cold` shutdown transition.
- Regression evidence: [actor-runtime-atomic-once-prevents-double-enter.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/actor-runtime-atomic-once-prevents-double-enter.yaml), `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 20`, `python runtime/native/src/core/z3/scripts/05_benchmark_sync_pathways.py`

## 2026-05-22 - runtime/services
### Concurrent Slot Overwrite and Lost Update in Service Registry
- Categories: correctness, race, runtime, registry
- Severity: Critical
- Status: Fixed in tree (2026-05-22)
- Surface: services
- Trigger: Concurrent service registration or native catalog population against the same registry.
- Symptom: Two writers could claim the same destination slot or publish an incorrect `service_count`, leading to lost registrations or torn visibility.
- Why this was a bug: `kain_service_registry_register` historically wrote `registry->services[registry->service_count]` and incremented `service_count` without any writer serialization.
- Minimal repro: Two threads concurrently register distinct services into the same cold or partially populated registry.
- Evidence: Historical code inspection showed descriptor copy and count publish occurring without a lock, atomic gate, or once-serialized commit phase.
- Z3 angle: The historical model admitted both writers landing on the same slot. The landed commit-gate proof excludes both slot collision and lost increments.
- Historical counterexample: [native-services-register-concurrent-slot-overwrite.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-services-register-concurrent-slot-overwrite.yaml)
- Fix landed: The registry now uses an atomic mutation gate, lock-free read-side count snapshots, publish-after-copy semantics for `service_count`, and a batched single-lock native catalog populate path.
- Regression evidence: [native-services-commit-gate-prevents-slot-overwrite.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-services-commit-gate-prevents-slot-overwrite.yaml), `bash runtime/conformance/02_service_registry/compile_test.sh`, `python runtime/native/src/core/z3/scripts/05_benchmark_sync_pathways.py`

## 2026-05-23 - crates/sys-codegen/codegen_llvm
### Double-to-Bool Truthiness Treats NaN As False On LLVM
- Categories: correctness, soundness, miscompile
- Severity: High
- Status: Fixed in tree (2026-05-24)
- Surface: lowering
- Trigger: Any LLVM-lowered `Float -> Bool` coercion or truthiness cast when the float value is `NaN`.
- Symptom: The compiled LLVM path returns `false` while Kain semantic truth returns `true`.
- Why this is a bug: `cast_numeric_value` lowers `double -> i1` with `fcmp one double value, 0.0`, which is an ordered comparison and therefore rejects `NaN`. The interpreter/runtime path in `crates/core/src/runtime.rs` defines float truthiness as `number != 0.0`, which treats `NaN` as non-zero and therefore truthy.
- Minimal repro: Compile any Kain program on the LLVM path that converts a `Float` produced from a `NaN`-yielding expression such as `0.0 / 0.0` into `Bool`.
- Evidence: `crates/sys-codegen/src/codegen_llvm/mod.rs:8870`, `crates/core/src/runtime.rs:119`, `crates/sys-codegen/z3/generated/float_semantic_audit.md`, and `crates/sys-codegen/z3/reports/20260524T000203Z-casts-double-to-bool-nan-truthiness-mismatch-pack-local.json`.
- Z3 angle: A floating-point model proves a concrete witness `x = NaN` where runtime truthiness and LLVM truthiness disagree.
- Z3 Proof: [casts-double-to-bool-nan-truthiness-mismatch.yaml](file:///D:/Kain-Lang/crates/sys-codegen/z3/proofs/casts-double-to-bool-nan-truthiness-mismatch.yaml)
- Fix landed: LLVM float truthiness now lowers through `fcmp une`, and float conditions in `if` / `while` now route through the same `i1` coercion helper instead of trusting the source expression to already be boolean.
- Regression evidence: [casts-double-to-bool-unordered-nonzero-aligns-with-runtime-truthiness.yaml](/D:/Kain-Lang/crates/sys-codegen/z3/proofs/casts-double-to-bool-unordered-nonzero-aligns-with-runtime-truthiness.yaml), `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_float_truthiness_inequality_and_int_casts_through_total_ieee_paths --target-dir target\codex-float-semantics`

### Raw `fptosi` Lowering Admits Undefined Double Inputs
- Categories: correctness, soundness, UB, miscompile
- Severity: Critical
- Status: Fixed in tree (2026-05-24)
- Surface: lowering
- Trigger: Any LLVM-lowered `double -> int` path fed `NaN`, `+oo`, `-oo`, or a finite value outside the signed destination range.
- Symptom: The backend emits LLVM IR whose `fptosi` precondition is violated, so compiled behavior is undefined or poison-prone exactly where Kain semantic casts are still total in the interpreter/runtime.
- Why this is a bug: `cast_numeric_value`, `coerce_to_i64_storage`, `stringify_value`, `compile_numeric_floor_builtin`, and integer `pow` lowering all emit raw `fptosi double ...` with no preceding domain guard. LLVM requires the operand to be finite and representable in the destination signed integer type.
- Minimal repro: Compile any Kain program on the LLVM path that narrows a float from `1.0 / 0.0`, `-1.0 / 0.0`, `0.0 / 0.0`, or a very large finite magnitude into `Int`; the same hazard also exists in integer `pow` and floor-based lowering.
- Evidence: `crates/sys-codegen/src/codegen_llvm/mod.rs:6514`, `:8593`, `:8858-8866`, `:14764`, `:15411`, `:16744`, `crates/core/src/runtime.rs:98`, and `crates/sys-codegen/z3/reports/20260524T000211Z-casts-double-to-int-unguarded-fptosi-precondition-gap-pack-local.json`.
- Z3 angle: A floating-point domain proof finds `x = +oo` as an immediate witness where the emitted LLVM `fptosi` precondition is false.
- Z3 Proof: [casts-double-to-int-unguarded-fptosi-precondition-gap.yaml](file:///D:/Kain-Lang/crates/sys-codegen/z3/proofs/casts-double-to-int-unguarded-fptosi-precondition-gap.yaml)
- Fix landed: LLVM now centralizes `double -> int` lowering through the saturating intrinsic family `llvm.fptosi.sat.*.f64`, covering shared casts, `floor(Float) -> Int`, stringification narrowing, explicit casts, and integer `pow` postprocessing.
- Regression evidence: [casts-double-to-int-saturating-intrinsic-preserves-in-range-raw-cast.yaml](/D:/Kain-Lang/crates/sys-codegen/z3/proofs/casts-double-to-int-saturating-intrinsic-preserves-in-range-raw-cast.yaml), `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_floor_builtin_with_llvm_intrinsic --target-dir target\codex-float-semantics`, `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_float_truthiness_inequality_and_int_casts_through_total_ieee_paths --target-dir target\codex-float-semantics`

### Float Equality And Inequality Ignore Kain's Epsilon Semantics
- Categories: correctness, soundness, miscompile
- Severity: High
- Status: Fixed in tree (2026-05-24)
- Surface: lowering
- Trigger: Any LLVM-lowered float `==` or `!=` comparison where the operands differ by less than `f64::EPSILON`.
- Symptom: The compiled LLVM path reports exact IEEE ordered equality/inequality while the interpreter/runtime reports equality within Kain's epsilon window.
- Why this is a bug: The interpreter/runtime in `crates/core/src/runtime.rs` evaluates float `==` with `(a - b).abs() < f64::EPSILON` and float `!=` with the complementary `>=` test. LLVM lowering in both `compile_value_eq` and `compile_expr` instead emits raw `fcmp oeq` and `fcmp one`.
- Minimal repro: Compile any Kain program on the LLVM path that compares `0.0` with `0.0000000000000001` using `==` or `!=`.
- Evidence: `crates/sys-codegen/src/codegen_llvm/mod.rs:8939`, `:16750`, `:16758`, `crates/core/src/runtime.rs:8058-8062`, `crates/sys-codegen/z3/generated/float_semantic_audit.md`, and `crates/sys-codegen/z3/reports/20260524T000218Z-control-float-equality-ignores-epsilon-runtime-semantics-pack-local.json`.
- Z3 angle: A minimized arithmetic witness uses `a = 0.0` and `b = 1e-16`, which is inside the runtime epsilon window but not exactly equal, so both equality and inequality semantics diverge.
- Z3 Proof: [control-float-equality-ignores-epsilon-runtime-semantics.yaml](file:///D:/Kain-Lang/crates/sys-codegen/z3/proofs/control-float-equality-ignores-epsilon-runtime-semantics.yaml)
- Fix landed: The runtime semantic owner now uses exact IEEE float `==` / `!=`, and LLVM float inequality uses `fcmp une` so `NaN != x` stays true like the interpreter and the other compiled backends.
- Regression evidence: [control-float-exact-equality-aligns-with-compiled-ieee-semantics.yaml](/D:/Kain-Lang/crates/core/z3/proofs/control-float-exact-equality-aligns-with-compiled-ieee-semantics.yaml), `cargo test -p kain-core runtime_float --lib --target-dir target\codex-float-semantics`, `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_float_truthiness_inequality_and_int_casts_through_total_ieee_paths --target-dir target\codex-float-semantics`

### `asyncio.Future.result()` Crashes The Native Python Bridge
- Categories: runtime, interop, crash
- Severity: High
- Status: Fixed in tree (2026-05-27)
- Surface: native Python bridge / `std::python` / `asyncio`
- Trigger: Calling `python_call_attr_raw(future, "result", [])` on an `asyncio.Future` created from `new_event_loop()` and `create_future()`, even after `set_result(...)`.
- Symptom: The process exited with `0xc0000005` instead of returning the Python value. `create_future`, `set_result`, `done`, `cancelled`, `set_event_loop`, and `close` all survived the same probe.
- Root cause: raw Python ints and bools were materialized as values that later traveled through Kain's tagged `Any` lane. Boxed ints use low tag `1`, so payloads such as `24` can become aligned after `value >> 3`; `kain_py_unbox_tagged_handle` then allowed `kain_py_type_tag_matches` to read an RC header before proving the payload was tracked.
- Fix landed: raw scalar materialization now returns tagged bool/int values, and `kain_py_type_tag_matches` verifies `kain_rc_is_tracked_pointer(ptr)` before reading the RC header.
- Z3 Proof: [native-python-scalar-tagged-handle-guard.yaml](/X:/runtime/native/src/core/z3/proofs/native-python-scalar-tagged-handle-guard.yaml)
- Regression evidence: direct Kain probe crossing the old crash value (`Future.result()` returning values through `24`), `kain check benchmark/cases_v2/python_stdlib_fused.kn --target llvm`, and filtered v2 benchmark run `KAIN_BENCH_V2_FILTER=python_stdlib ... kain run X:\benchmark --target llvm --json` with all four rows `status=ok`.

### Native LLVM exes can crash when Python host objects cross helper-function boundaries
- Categories: runtime, interop, crash, codegen
- Severity: High
- Status: Fixed in tree (2026-05-28)
- Surface: native Python bridge / authored Kain / LLVM executable lane
- Trigger: Historical LLVM lowering forced non-`main` callables whose semantic return type was `void` into `i64`, including ordinary authored helpers such as `fn pack(target: Any): ...` and `impl` methods with no explicit `->`.
- Symptom: The process exited with `0x80000003` during otherwise valid Tkinter work because the helper body finished, cleaned up locals, and then hit an emitted `unreachable` instead of `ret void`.
- Minimal repro: A native LLVM program that creates `let frame = python_call_attr_raw(tk, "Frame", [root])`, then calls a helper like `fn pack(target: Any): let _pack = python_call_attr_raw(target, "pack", [])`; old LLVM lowered `define internal i64 @pack(...)` and ended the helper with `unreachable`.
- Root cause: `crates/sys-codegen/src/codegen_llvm/mod.rs` historically rewrote resolved `void` callable signatures to `i64` for non-`main` functions and `impl` methods. Side-effect-only helpers with no explicit return therefore compiled as value-returning callables even when their bodies had no final expression result.
- Fix landed: LLVM callable lowering now preserves semantic `void` for ordinary helpers and `impl` methods, while `main` still widens to `i64` at the ABI boundary. The old helper repros now lower to `define internal void @...` and return normally.
- Regression evidence: `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_implicit_void_ -- --nocapture`, compiled native repro `X:\tmp_python_helper_boundary_repro.kn` printing `ok`, and compiled native helper-return repro `X:\tmp_python_helper_return_repro.kn` printing `return_ok`.
