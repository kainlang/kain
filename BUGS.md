# Kain Bug Log
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
- Regression evidence: [native-memory-cas-failure-order-clamp-prevents-ub.yaml](/D:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-cas-failure-order-clamp-prevents-ub.yaml), [memory-atomic-compare-exchange-validation-rejects-invalid-failure-orderings.yaml](/D:/Kain-Lang/crates/kain-sys-codegen/z3/proofs/memory-atomic-compare-exchange-validation-rejects-invalid-failure-orderings.yaml), `cargo test -p kain-core compare_exchange_ -- --nocapture`, `cargo test -p kain-sys-codegen rejects_ -- --nocapture`, `bazel test //runtime:native_test_atomic_memory_ordering`

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
- Regression evidence: [memory-atomic-store-validation-rejects-acquire-and-acqrel.yaml](/D:/Kain-Lang/crates/kain-sys-codegen/z3/proofs/memory-atomic-store-validation-rejects-acquire-and-acqrel.yaml), `cargo test -p kain-sys-codegen rejects_ -- --nocapture`, `bazel test //runtime:native_test_atomic_memory_ordering`

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
