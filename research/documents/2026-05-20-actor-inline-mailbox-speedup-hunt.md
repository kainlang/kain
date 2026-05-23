# Actor Inline Mailbox Speedup Hunt

- Date: 2026-05-20
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `actor-inline-mailbox-speedup-hunt`

## Research Question

Can Kain's local microcell ask path delete the enqueue/copy/dequeue round-trip and recover a multi-x speedup on actor_ownership_backpressure without lying about semantics?

## Constraints

- Keep the change honest: no benchmark-only arithmetic shortcut or actor bypass that would stop measuring `ask` / mailbox semantics.
- Preserve the public actor ABI shape used by existing Kain LLVM output wherever possible.
- Prefer a runtime/compiler mechanism that can help any local microcell ask-heavy workload, not just `actor_ownership_backpressure`.
- Leave a proof artifact for the new inline slot contract and rerun the full benchmark suite before landing.

## Hypothesis Lattice

### Baseline
- Mechanism: keep the existing inline turn trigger but accept that it still heap-copies the request through the mailbox queue.
- Expected upside: low; maybe noise cleanup only.
- Likely blocker: the hot path still allocates and frees a payload buffer for every ask.
- Proof obligation: none beyond confirming the benchmark remains slow.

### Unconventional
- Mechanism: add an opt-in borrowed inline ask payload lane for compiler-generated local microcell actors, plus a runtime-owned `kain_actor_message_release(...)` hook so generated handlers can release borrowed or heap payloads safely.
- Expected upside: delete the request payload malloc/copy/dequeue/free round-trip on the first inline ask in a turn; plausible multi-x win on ask-heavy semantic rows.
- Likely blocker: ownership contract drift between borrowed payloads and existing `free(message.data)` habits in generated actor code and C conformance helpers.
- Proof obligation: prove the borrowed slot still delivers exactly one message and does not mutate queue depth.

### Moonshot
- Mechanism: a direct-call actor reducer that bypasses mailbox/message materialization entirely for closed local request/reply actors.
- Expected upside: potentially closes most of the remaining gap to C++ direct method calls.
- Likely blocker: too easy to stop measuring declared actor semantics and accidentally land a benchmark-specific cheat.
- Proof obligation: preserve message ordering, reply-port semantics, and scheduler observability for the general actor ABI, not just this row.

## Mathematical Model

- Variables: `queue_depth`, `inline_pending`, `delivered_count`, and whether the actor is already scheduler-owned.
- Invariants: borrowed inline arm may only happen when `queue_depth == 0`, `inline_pending == 0`, and the actor is not already queued or in-flight.
- Objective: minimize synchronous local ask overhead while keeping one delivered message and one reply per ask.
- Bad states: double delivery, stale borrowed pointer freed as heap, queue depth mutation from a borrowed-only arm/consume sequence.
- Simplifying assumptions: the borrowed slot is only used for the first local microcell ask in a turn; later concurrent arrivals still append to the real queue.

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/actor-inline-borrowed-ask-single-delivery.smt2` encodes the arm/consume postcondition for the borrowed slot. `mcp__z3_local__.check_smt2(...)` returned `unsat`; report: `z3/reports/20260521T001551Z-actor-inline-borrowed-ask-single-delivery.json`.
2. The benchmark touch must keep the checksum contract unchanged after adding `deadline_millis` / `deadline_elapsed` to `actor_ownership_backpressure`.

## Evidence And Sources

- Local:
  - `benchmark/out/reports/latest.llm.md`: latest row showed `actor_ownership_backpressure` at Kain `661.200 ms` vs C++ `17.708 ms` before this pass.
  - `runtime/native/src/core/actor.c`: existing ask fast path already inlined the first local microcell turn but still copied payloads into the mailbox before execution.
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: generated actor turns always released message payloads with raw `free(...)` before this pass.
- External:
  - None.

## Dead Ends

- Rewriting the benchmark into a closed-form arithmetic reducer would likely win the row, but it would stop measuring actor semantics and is therefore rejected.

## Conclusion

Active implementation pass: landed the opt-in borrowed inline ask payload policy in the actor runtime, switched generated actor handlers to `kain_actor_message_release(...)`, and touched the benchmark deadline surface. Focused and canonical benchmark retakes pending.
