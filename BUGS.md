# Kain Bug Log

## 2026-05-22 - runtime/ownership
### Stale Pointer Aliasing & Registry Capacity Leak in Decay
- Categories: correctness, resource-leak, aliasing, runtime
- Severity: Critical
- Status: Active
- Surface: ownership
- Trigger: Normal usage of `__kain_ownership_decay` on heap allocations.
- Symptom: Long-running actor or memory-heavy programs will eventually fail with `KAIN_OWNERSHIP_ERR_CAPACITY` (ENOMEM). Reused pointer addresses will hit stale index collisions.
- Why this is a bug: In `ownership.c`, `kain_ownership_decay_slot_unlocked` calls `__kain_free(ptr)` when `reclaim_helper_slot == 0` but bypasses `kain_ownership_clear_slot_unlocked`. The region stays marked as `occupied = 1`, the old pointer remains in `KAIN_OWNERSHIP_POINTER_INDEX`, and the global slot is permanently leaked.
- Minimal repro: Allocate and decay heap memory in a loop exceeding `KAIN_OWNERSHIP_MAX_REGIONS`.
- Evidence: Code inspection of `kain_ownership_decay_slot_unlocked` shows the early return after `__kain_free(ptr)` without clearing the slot or occupancy bits.
- Z3 angle: A state machine model of slot lifecycle shows the transition `IDLE -> DECAYED` never reaches `UNOCCUPIED`, monotonically decreasing available slots.
- Z3 Proof: [native-ownership-decay-heap-fast-path-leaks-slot.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/native-ownership-decay-heap-fast-path-leaks-slot.yaml)
- Suggested follow-up: Modify `kain_ownership_decay_slot_unlocked` to unconditionally call `kain_ownership_clear_slot_unlocked(slot)` for heap regions after a successful `__kain_free`, regardless of `reclaim_helper_slot`.

## 2026-05-22 - runtime/stdlib_abi
### UNC Path Creation Failure on Windows
- Categories: correctness, fs, runtime
- Severity: High
- Status: Active
- Surface: runtime
- Trigger: Attempting to write a file or create directories using a UNC path (e.g., `\\server\share\file.txt`) on Windows.
- Symptom: The file write or directory creation fails immediately with a parent directory error.
- Why this is a bug: In `stdlib_abi.c`, `abi_fs_create_parent_dirs` loops from `index = 1`. For UNC paths, `buffer[1]` is `\`. The code truncates the buffer to `\` and calls `abi_fs_create_one_dir("\\")`, which fails on Windows because it's a root/server marker, causing the entire traversal to abort.
- Minimal repro: Call `fs_write_text("\\\\127.0.0.1\\C$\\temp\\test.txt", "data")` on Windows.
- Evidence: Code inspection of `abi_fs_create_parent_dirs` loop condition and `CreateDirectoryA` behavior on root strings.
- Z3 angle: String bounds and path parser model shows that `index = 1` truncation produces an invalid root path on UNC inputs.
- Z3 Proof: [native-stdlib-abi-unc-path-creation-fails.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/native-stdlib-abi-unc-path-creation-fails.yaml)
- Suggested follow-up: Update the `abi_fs_create_parent_dirs` traversal loop to advance `index` past the UNC server and share prefix (e.g., `\\server\share\`) before attempting to create intermediate directories.

## 2026-05-22 - runtime/actor
### Actor Monitor Record Memory Leak
- Categories: correctness, resource-leak, performance, runtime
- Severity: High
- Status: Solver-Proved
- Surface: actor
- Trigger: Termination of a monitored actor when the monitoring actor continues to run.
- Symptom: KainActorMonitor structs allocated via malloc are never freed or dequeued on exit notification, leaking heap memory and degrading list lookup performance (O(N) search) over time.
- Why this is a bug: When a monitored actor exits, the system invokes `kain_actor_notify_monitors`. This correctly loops over all actors to notify the monitor, but fails to remove or free the associated `KainActorMonitor` node from the `monitor_actor->monitors` list. As short-lived workers spawn and die under a long-lived coordinator or supervisor, the monitors list grows infinitely, leading to memory exhaustion and lookup overhead.
- Minimal repro: Monitor short-lived actors in a loop using a single long-lived supervisor and verify that the monitors list length grows monotonically.
- Evidence: Code inspection of `kain_actor_notify_monitors` at `runtime/native/src/core/actor.c:3200-3220` shows the loop performs sends but does not invoke `free(monitor)` or unlink from the monitors list. SMT2 verification yields `sat` (counterexample) demonstrating `act_exists` transitioning to dead while `mon_exists` remains active.
- Z3 angle: Verified using the state transition safety invariant that if an actor is dead, no active monitoring record referencing its ID should exist. The solver successfully found a counterexample (sat) in `actor-monitor-notification-leaks-relationship.yaml`.
- Z3 Proof: [actor-monitor-notification-leaks-relationship.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/actor-monitor-notification-leaks-relationship.yaml)
- Suggested follow-up: Update `kain_actor_notify_monitors` to cleanly remove and free the monitor record from the monitoring actor's list.

## 2026-05-22 - runtime/memory
### Unchecked Relocation Failure in Realloc
- Categories: correctness, soundness, UB, runtime, memory
- Severity: Critical
- Status: Solver-Proved
- Surface: memory
- Trigger: Relocation during `__kain_realloc` when `__kain_ownership_relocate_helper_allocation` fails.
- Symptom: Memory block is physically relocated to a new address while the ownership registry keeps tracking the old invalid address, causing severe heap corruption, double-free vulnerabilities, and registry split-brain.
- Why this is a bug: In `memory.c`, if `realloc` succeeds in moving a memory block, `__kain_realloc` invokes `__kain_ownership_relocate_helper_allocation`. If this call fails (due to slot mismatches, busy/lock states, or registry capacity limits), `__kain_realloc` ignores the failure and returns the new payload anyway. This leaves the ownership registry in a stale state (tracking the old ptr) while the program continues using the new pointer.
- Minimal repro: Trigger a relocation failure during `__kain_realloc` (e.g., when the region is in a non-idle state) and observe that the returned pointer remains untracked by the ownership registry while the old pointer continues to block the registry slot.
- Evidence: Code inspection of `__kain_realloc` at `runtime/native/src/core/memory.c:621-624` shows the return of the payload when `__kain_ownership_relocate_helper_allocation != KAIN_OWNERSHIP_OK`. Z3 solver verification in `native-memory-realloc-relocation-failure-bypass.yaml` yields `sat` (counterexample).
- Z3 angle: Proved that under relocation failure, the physical block address returned to the caller diverges from the tracked registry address. Verification of `native-memory-realloc-relocation-failure-bypass.yaml` yields `sat`.
- Z3 Proof: [native-memory-realloc-relocation-failure-bypass.yaml](file:///d:/Kain-Lang/runtime/native/src/core/z3/proofs/native-memory-realloc-relocation-failure-bypass.yaml)
- Suggested follow-up: Modify `__kain_realloc` to handle relocation failures. If relocation fails, free the newly relocated block (or revert) and return NULL to the caller to maintain heap-to-registry consistency.

