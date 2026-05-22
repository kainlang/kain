# Kain Bug Log

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
