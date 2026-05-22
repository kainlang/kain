# Kain Bug Log
## 2026-05-22 - runtime/memory — atomic ordering audit (tool-z3-bug-hunter)

### CAS Failure-Order Stronger Than Success-Order Produces C11 UB

- Categories: correctness, soundness, UB
- Severity: High
- Status: Solver-Proved
- Surface: runtime
- Trigger: Kain user calls `__kain_atomic_compare_exchange_ordered` with `success_ordering=KAIN_MEMORY_ORDER_RELAXED` and any failure ordering that maps to a C11 strength greater than relaxed (e.g. `KAIN_MEMORY_ORDER_SEQ_CST`).
- Symptom: Silent C11 undefined behavior. `atomic_compare_exchange_strong_explicit` is invoked with `failure_order > success_order` in C11 enum strength, violating C11 7.17.7.4. The behavior of the program is undefined; compilers may eliminate the surrounding code or produce incorrect machine code.
- Why this is a bug: Z3 solver found witness `kain_success=0` (RELAXED→c11_relaxed, strength 0) and `kain_failure=4` (SEQ_CST→c11_seq_cst, strength 5). `5 > 0` violates C11 7.17.7.4. The implementation calls `atomic_compare_exchange_strong_explicit` directly with the two mapped orderings without validating `failure_order ≤ success_order`. No clamping, no diagnostic.
- Minimal repro: `__kain_atomic_compare_exchange_ordered(ptr, expected, desired, KAIN_MEMORY_ORDER_RELAXED, KAIN_MEMORY_ORDER_SEQ_CST)`
- Evidence: Z3 `sat` model — `kain_success=0, kain_failure=4, violation=true, c11_failure_strength=5, c11_success_strength=0, delta=5`
- Z3 angle: The contract `∀(s,f)∈[0..4]²: failure_c11(f) ≤ success_c11(s)` is **sat** (violated). Witness found in < 30ms.
- Z3 Proof: [native-memory-cas-failure-order-stronger-than-success-ub.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-cas-failure-order-stronger-than-success-ub.yaml)
- Suggested follow-up: In `__kain_atomic_compare_exchange_ordered` (memory.c:444-462), clamp `failure_ordering` before the `kain_memory_failure_order_from_code` call so failure_c11 never exceeds success_c11. Add a `KAIN_ASSERT` in debug builds that fires when the ordering constraint is violated before UB occurs.

---

### Silent ACQUIRE→RELEASE Remap in Atomic Store With No Diagnostic

- Categories: developer-experience, correctness
- Severity: Medium
- Status: Solver-Proved
- Surface: runtime
- Trigger: Kain user passes `KAIN_MEMORY_ORDER_ACQUIRE` or `KAIN_MEMORY_ORDER_ACQ_REL` to `__kain_atomic_store_ordered`.
- Symptom: The store silently uses `memory_order_release` instead of the requested ordering. No compile-time or runtime diagnostic is emitted. ACQ_REL on a store loses the acquire half entirely with no warning.
- Why this is a bug: Z3 proof: `store_mapping(ACQUIRE=1) → RELEASE=2`. The user-specified Kain ordering constant (1) differs from the C11 ordering actually applied (2) with no diagnostic. For ACQ_REL, the acquire half of the intended bidirectional fence is silently dropped.
- Minimal repro: `__kain_atomic_store_ordered(ptr, value, KAIN_MEMORY_ORDER_ACQUIRE)` — the applied C11 ordering is `memory_order_release`, not `memory_order_acquire`.
- Evidence: Z3 `sat` model — `user_ordering=1 (ACQUIRE), c11_ordering=2 (RELEASE), delta=1`
- Z3 angle: `∃user_ordering: store_mapping(user_ordering) ≠ user_ordering` is **sat**. Witness: ACQUIRE→RELEASE.
- Z3 Proof: [native-memory-store-order-acquire-silently-downgrades-to-release.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-store-order-acquire-silently-downgrades-to-release.yaml)
- Suggested follow-up: Add `KAIN_ASSERT` or `KAIN_DIAGNOSTICS_WARN` in `kain_memory_store_order_from_code` (memory.c:352) for ACQUIRE/ACQ_REL inputs. Consider making the Kain type system reject invalid store orderings at the language level so the mapping function never receives a semantically-invalid code.

---

## 2026-05-22 - runtime/ownership — write-ordering audit (tool-z3-bug-hunter)

### kain_ownership_clear_slot_unlocked Write-Order Creates OCCUPIED=1/DECAYED Transient (Lock-Free Future Risk)

- Categories: ordering, soundness
- Severity: Low
- Status: Solver-Proved
- Surface: ownership
- Trigger: Future code adds a lock-free fast-path reader to the ownership registry that reads `occupied` and `state` fields without holding `KAIN_OWNERSHIP_REGISTRY_LOCK`.
- Symptom: The lock-free reader could observe `occupied=1, state=DECAYED` as a stable-looking state since `kain_ownership_clear_slot_unlocked` writes `state=DECAYED` (line 240) before `occupied=0` (line 242). Under the current single-lock model this transient is never visible externally. If a lock-free reader is added, the ordering creates a semantic hole.
- Why this is a bug (potential): Z3 proves `occupied=1, state=DECAYED` is **sat** (geometrically reachable as a transient). A lock-free reader between lines 240–242 would see a logically inconsistent region (alive but decayed). The fix: write `occupied=0` BEFORE `state=DECAYED`.
- Minimal repro: Not currently triggerable without a lock-free reader — this is a documented future-risk proof.
- Evidence: Z3 `sat` with model `occupied=1, state=5 (DECAYED)` — the intermediate is real.
- Z3 angle: The transient occupancy×state combination is **sat** (exists). Safe ONLY because the lock makes it invisible.
- Z3 Proof: [native-ownership-clear-slot-occupied-decayed-write-order-assumption.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/native-ownership-clear-slot-occupied-decayed-write-order-assumption.yaml)
- Suggested follow-up: Add a comment above `kain_ownership_clear_slot_unlocked` documenting the write-ordering invariant and the lock dependency. If any lock-free fast path is ever added, reorder writes: `occupied=0` first, then `state=DECAYED`, then clear the occupancy bitmap bit.

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
