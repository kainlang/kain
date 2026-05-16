# Universal Actor Architecture

- Date: 2026-05-16
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `universal-actor-architecture`

## Research Question

Can Kain evolve from a BEAM-inspired native actor runtime into a universal actor substrate that works across low-latency local compute, Unreal-style world execution, and distributed networking without collapsing into thread-per-actor or engine-specific object semantics?

## Constraints

- Must beat the current native thread-blocking actor path on local ask/reply latency.
- Must preserve a single authored actor model across native compute, world simulation, and distributed networking.
- Must not equate "actor" with "OS thread", "UE AActor", or "TCP socket".
- Must support host-affine execution classes where work is required to stay on a game-thread, render-thread, IO-thread, or platform thread.
- Must preserve isolation / supervision / backpressure semantics even when transport and scheduler class differ.
- Must leave room for Kain-native ownership features such as `world`, `entangle`, `teleport`, `pulse`, and `shatter`.

## Hypothesis Lattice

### Baseline
- Mechanism: Move Kain toward a BEAM-like process scheduler with reductions, run queues, stealing, buffered signal queues, and generation-tagged actor refs.
- Expected upside: Massive local latency improvement and far better scalability than thread-per-actor or blocking mailbox loops.
- Likely blocker: Still too compute-centric; UE/world-frame execution and distributed transport remain adapters bolted on top.
- Proof obligation: Show that scheduler-owned readiness and reduction-bounded execution strictly dominate the current blocking-thread actor model on the actor benchmark.

### Unconventional
- Mechanism: Define a universal actor as an isolated state cell plus an address plus an execution class. Mailboxes become transport-agnostic signal queues; scheduler class decides where/how the actor may run.
- Expected upside: One actor model can span local runtime actors, UE host/world actors, and remote network actors without pretending they are the same physical thing.
- Likely blocker: Requires explicit locality / affinity metadata so "transparent remote actor" fantasy does not destroy semantics or performance.
- Proof obligation: Show that message ordering, supervision, timeout, and ownership rules remain coherent when actors move between local, host-affine, and remote execution classes.

### Moonshot
- Mechanism: Replace "actor" with a compiler/runtime execution cell lattice:
  - microcell: hot local reduced-scheduler actor
  - worldcell: frame/pulse-scheduled actor with engine affinity
  - netcell: distributed replicated/routed actor
  - hostcell: OS/UE/UI thread-affine actor
  - accelerator cell: GPU/async device command actor
- Expected upside: Kain gets a single universal concurrency substrate instead of separate "actors", "entities", "services", "UI callbacks", and "net replication" systems.
- Likely blocker: Complexity explosion unless the type system and runtime contracts make capability and locality explicit.
- Proof obligation: Prove that each cell class refines one shared abstract actor contract instead of becoming five unrelated runtimes wearing the same name.

## Mathematical Model

- Variables:
  - `A`: actor/cell identity
  - `G`: generation tag
  - `C`: execution class
  - `Q`: mailbox or signal queue state
  - `L`: locality = local | host-affine | remote
  - `B`: execution budget per scheduling turn
  - `H`: handoff class = copy | shared-fragment | teleport | serialize
- Invariants:
  - Actor refs are never raw slot ids; `ref = (slot, generation, class, locality capabilities)`.
  - Send preserves per-sender ordering within a mailbox lane.
  - Supervision trees may cross classes only through explicit failure-translation rules.
  - A host-affine actor cannot execute on a scheduler that violates its affinity contract.
  - A teleport handoff consumes source ownership before target execution begins.
- Objective:
  - Minimize roundtrip latency and scheduler overhead while maximizing semantic portability across runtime classes.
- Bad states:
  - Thread-per-actor fallback under normal load.
  - Hidden remote latency masquerading as local actor behavior.
  - UE object lifetime coupled directly to actor identity.
  - Cross-class supervision ambiguity.
  - Reused actor ids without generation fencing.
- Simplifying assumptions:
  - "Universal" means one authored semantic model, not identical physical cost.
  - Some classes will require explicit capability annotations and different budgets.

## Z3 Claims

1. Generation-tagged actor refs prevent stale-address aliasing after slot reuse.
2. Scheduler-owned readiness with bounded budgets cannot enqueue the same actor twice without an intervening dequeue/execute transition.
3. Cross-class handoff rules can enforce that `teleport` ownership transfer is single-consumer.
4. Message ordering remains coherent under buffered local enqueue plus routed remote forwarding.

## Evidence And Sources

- Local:
  - `runtime/native/src/core/kain_runtime_actor.c`
  - `reference/erl_message.c`
  - `reference/erl_process.c`
  - `reference/erl_process_lock.c`
  - `reference/Actor.cpp`
- External:
  - None yet; this note is currently grounded in local repo/runtime references.

## Dead Ends

- None yet.

## Conclusion

Current thesis:

The strongest surviving direction is not "make Kain actors more like Erlang actors" in a narrow sense. It is to make Kain actors into a universal execution substrate with BEAM-quality local scheduling as the baseline, then layer world affinity, host affinity, and remote transport as first-class execution classes rather than side systems.

What appears strongest so far:

- `proved/plausible`: The current blocking mailbox + thread-owned actor loop is the wrong local baseline.
- `plausible`: A BEAM-style scheduler and buffered signal queue are necessary for local performance.
- `plausible`: UE and networking do not want a separate concurrency model; they want explicit execution classes beneath one actor contract.
- `speculative`: Kain's ownership/world features could make this stronger than plain actors by giving explicit zero-copy teleport/handoff semantics between classes.

Implementation update on 2026-05-16:

- The first prototype is now real in the native ABI and LLVM lane:
  - `KainActorRef` exists with generation / execution-class / locality metadata.
  - LLVM actor state field 0 now stores that ref instead of a raw id.
  - reply ports are synthetic refs, not waiting actor threads.
  - TLS reply-port reuse now rebinds a fresh synthetic actor generation so stale late replies are rejected.

Best next experiment:

- Prototype a scheduler-owned ready queue with bounded actor turns and execution-class-aware dispatch before attempting deeper UE/network unification.
