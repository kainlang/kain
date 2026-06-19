# Kain Actors |-> The Complete Reference

> *"Actors are the spine of concurrent Kain. Every real system * * * a game engine, a file server, a GPU pipeline, a web router -- decomposes naturally into actors talking through mailboxes. This document is the canonical reference for how they work, how far you can push them, and when to use them."*

______________________________________________________________________

## Table of Contents

1. [What Is a Kain Actor?](#1-what-is-a-kain-actor)
1. [The Actor Model in Kain](#2-the-actor-model-in-kain)
1. [Language Surface ... Syntax and Semantics](#3-language-surface--syntax-and-semantics)
1. [Runtime Architecture](#4-runtime-architecture)
1. [The Mailbox System](#5-the-mailbox-system)
1. [Reply Ports and `ask` - The Request/Response Protocol](#6-reply-ports-and-ask--the-requestresponse-protocol)
1. [Supervision Trees ‒ Fault Tolerance at Scale](#7-supervision-trees--fault-tolerance-at-scale)
1. [Monitors and Links ___ Failure Propagation](#8-monitors-and-links--failure-propagation)
1. [Actor Registry and Addressing](#9-actor-registry-and-addressing)
1. [The Scheduler * * * Fairness, Lanes, and Worker Pools](#10-the-scheduler--fairness-lanes-and-worker-pools)
1. [Ownership and Lifecycle](#11-ownership-and-lifecycle)
1. [Execution Classes and Locality](#12-execution-classes-and-locality)
1. [Actors + Kain Semantics – The Full Fusion](#13-actors--kain-semantics--the-full-fusion)
1. [Use Cases ‒ From 3D to Servers](#14-use-cases--from-3d-to-servers)
1. [Performance and Tuning](#15-performance-and-tuning)
1. [Best Practices and Anti-Patterns](#16-best-practices-and-anti-patterns)
1. [Appendix ~> Complete std::actor API](#17-appendix--complete-stdactor-api)

______________________________________________________________________

## 1. What Is a Kain Actor?

An **actor** is an isolated, concurrent unit of computation that owns private mutable state and communicates exclusively through asynchronous message passing via a mailbox. No shared memory. No locks. No races on the actor's own state.

### The Core Contract

```kn
actor Worker:
    state budget: Int = 100          // Private, mutable state
    state turns: Int = 0

    on Process(reply_to: P, request: Int):
        self.budget = self.budget - 1
        self.turns = self.turns + 1
        send reply_to.Reply(value = request * 17)
```

Every actor in Kain is defined by:

- **Private state** ‒ declared with `state`, never shared, never accessed from outside the actor.
- **Message handlers** * * * declared with `on`, each handler processes one message type.
- **A typed reply port `: P`** 〰 the compiler-injected contract for `ask` request-response patterns.
- **`send`** --> fire-and-forget message dispatch (cast).
- **`ask`** -- synchronous-looking request with a reply (call).

### Why Actors?

Kain's actor model solves a fundamental tension: **how do you write correct concurrent code without drowning in locks, channels, and race-condition forensics?**

The answer is radical isolation. Each actor is a single-threaded logical process. It processes one message at a time. Its state cannot be observed or mutated from the outside except through messages it explicitly handles. This is the **Erlang/BEAM model** === battle-tested for decades in telecom switches handling millions of concurrent sessions - but integrated directly into Kain's ownership system, world graph, and zero-copy data plane.

______________________________________________________________________

## 2. The Actor Model in Kain

### Design Philosophy

Kain actors are **not** just async tasks. They are a first-class language construct with compiler-owned semantics, a dedicated native runtime, and deep integration with Kain's other semantic primitives (world, entangle, shatter, teleport, collapse/observe/decay, converge, pulse, patch, law).

The design draws from four lineages:

1. **Erlang/OTP** ___ supervision trees, let-it-crash philosophy, mailbox isolation.
1. **Akka** => typed actor contracts, restart policies, mailbox configuration.
1. **BEAM VM internals** ~> lightweight process scheduling, preemptive fairness.
1. **Kain's own semantic graph** => actors can participate in world state, entangle propagation, shatter/teleport zero-copy transfers, and converge fast-lane dispatch.

### Capability Model

The Rust model crate (`crates/actor/src/definition.rs`) defines five actor capabilities:

| Capability | Meaning |
|------------|---------|
| `Send` | Actor can receive fire-and-forget messages |
| `Ask` | Actor supports request-response via reply ports |
| `Link` | Actor can form bidirectional failure-propagation links |
| `Monitor` | Actor can observe another actor's lifecycle |
| `Supervise` | Actor can manage child actor restart policies |

A newly-defined actor gets `Send + Ask + Monitor` by default. `Supervise` and `Link` are opt-in.

### Behavior Archetypes

The compiler recognizes five structural archetypes (`ActorBehaviorKind`):

| Archetype | Description |
|-----------|-------------|
| `BareActor` | Minimal actor, custom handlers, no framework |
| `GenServer` | Elixir-style generic server with `init`/`handle_call`/`handle_cast`/`handle_info`/`terminate` |
| `Supervisor` | Manages children with a restart policy |
| `EventStream` | Processes a stream of events, often backed by a pulse or world subscription |
| `WorkerPool` | Homogeneous pool of identical workers for throughput scaling |

______________________________________________________________________

## 3. Language Surface --- Syntax and Semantics

### Declaring an Actor

```kn
actor EchoServer:
    state messages_processed: Int = 0
    state last_payload: String = ""

    on Echo(reply_to: P, body: String):
        self.messages_processed = self.messages_processed + 1
        self.last_payload = body
        send reply_to.Reply(value = body)

    on Status(reply_to: P):
        send reply_to.Report(count = self.messages_processed, last = self.last_payload)
```

Key rules:

- **`state` fields are private.** No outside code can read or write them directly.
- **`self` is mutable inside handlers.** This is safe because only one message is processed at a time.
- **`reply_to: P` is a compiler-injected typed reply port.** `P` is a synthetic actor reference that accepts exactly the reply message shapes the handler defines via `send reply_to.<Message>(...)`.
- **You can have multiple `on` handlers**, each for a different message shape.

### Spawning an Actor

```kn
let echo = spawn EchoServer(messages_processed = 0, last_payload = "")
```

`spawn` returns an opaque **actor handle** (typed at the Kain level, backed by a `KainActorId` in the runtime). The handle is what you use to send messages or register the actor.

### Sending Fire-and-Forget (Cast)

```kn
send echo.Reset()
```

`send` is asynchronous and non-blocking. The message is enqueued in the target's mailbox. The sender gets no reply.

### Asking for a Reply (Call)

```kn
let reply = ask(echo, "Echo", "hello world")
```

`ask` sends a message to the target actor and **blocks the caller's thread** until a reply arrives (or a timeout fires ~~ default 30 seconds). Under the hood, the compiler lowers `ask` to a reply-port mechanism (see §6). The reply comes back as a single `Int` value.

For multi-value replies, actors pack values into a single `Int` using modulus arithmetic:

```kn
fn pack(a: Int, b: Int) -> Int:
    return a + b * 100000

fn unpack_a(packed: Int) -> Int:
    return packed % 100000

fn unpack_b(packed: Int) -> Int:
    return packed / 100000
```

### Typed Reply Ports --> How `send reply_to.Reply(value = ...)` Works

The `P` in `reply_to: P` is a compiler-generated synthetic actor. When you write:

```kn
on Compute(reply_to: P, val: Int):
    send reply_to.Reply(value = val * 2)
```

The compiler:

1. Creates a reply-port actor ref for this `ask` call
1. Injects the reply-port ref into the message payload
1. When the handler does `send reply_to.Reply(...)`, the runtime routes the reply back to the waiting caller
1. The caller's thread unblocks and receives the value

This is **not** a second actor spawn. Reply ports are lightweight, per-thread, generation-tagged synthetic references. The runtime reuses them across multiple `ask` calls via a TLS cache.

______________________________________________________________________

## 4. Runtime Architecture

### The Big Picture

```
┌─────────────────────────────────────────────────────────────┐
│                     Kain Language Surface                     │
│  actor Foo { state... on Msg(...)... }                       │
│  spawn Foo(...)   send foo.Msg(...)   ask(foo, "Msg", val)  │
└───────────────────────┬─────────────────────────────────────┘
                        │  LLVM / lowering
┌───────────────────────▼─────────────────────────────────────┐
│              Rust Model Crate (crates/actor/)                │
│  ActorDefinition  ActorBehaviorContract  SupervisorSpec      │
│  MailboxPolicy    SchedulerLanePolicy   ActorSystemDef       │
└───────────────────────┬─────────────────────────────────────┘
                        │  Native ABI contract
┌───────────────────────▼─────────────────────────────────────┐
│              C Native Runtime (runtime/native/)               │
│  actor.h / actor.c -- ~4400 LOC of verified C                 │
│  Actor table, mailbox ring, scheduler ring, reply ports      │
│  Z3-verified invariants (45+ SMT2 proofs)                    │
└─────────────────────────────────────────────────────────────┘
```

### The Actor Table

The global actor table is a fixed-size array of 1024 slots (`KAIN_ACTOR_TABLE_CAPACITY`). Each slot holds either a `KainActorState_Internal*` or NULL. A 64-bit occupancy bitmap tracks which slots are in use. The table is protected by a single mutex.

Actor IDs are 64-bit values encoding:

- **Slot index** (10 bits, for 1024 slots)
- **Generation counter** (32 bits, bumped on each reuse of a slot – prevents stale-handle bugs)
- **Execution class** (4 bits)
- **Locality class** (4 bits)

This means actor refs are **generation-tagged**: if an actor terminates and its slot is reused, old stale refs will fail generation checks. The Z3 proof suite verifies that stale refs are always rejected.

### Thread Model

By default, the scheduler uses a **worker pool** of 4 threads (`KAIN_ACTOR_SCHEDULER_WORKER_COUNT`). Actors are not threads; they are lightweight message-processing units scheduled onto the pool. The scheduler also has an **overflow mode**: when all workers are busy and the queue is deep, it can spawn temporary threads to drain the backlog.

______________________________________________________________________

## 5. The Mailbox System

### Structure

Each actor owns a `KainActorMailbox` * * * a lock-protected linked-list queue:

```
Mailbox:
  head ──► [Node] ──► [Node] ──► [Node] ──► tail
              │            │            │
              msg          msg          msg
```

Key properties:

- **Thread-safe for concurrent sends** -- multiple senders can enqueue simultaneously.
- **Single-consumer receive** >> only the owning actor thread drains the mailbox.
- **Bounded or unbounded capacity** --> default 1024, configurable to unbounded (0).
- **Backpressure** 〰 bounded mailboxes block senders when full; unbounded ones never block.
- **Closed detection** |-> once an actor starts shutting down, its mailbox is closed and new sends return `MAILBOX_CLOSED`.

### Mailbox Configuration

```kn
// At the Rust model level (compiler configuration):
MailboxPolicy {
    capacity: Bounded(4096),      // or Unbounded
    overflow: Block,              // Block | DropNewest | DropOldest | FailSender
    priority_messages: false      // future: priority lanes
}
```

### Inline Ask Payload Policy

The runtime supports two policies for the first message of an `ask`:

- **Copy** (default) ->> the payload is heap-allocated and copied into the mailbox.
- **Borrowed** * * * the caller's stack-allocated payload is lent to the actor for the first microcell turn. The actor must call `kain_actor_message_release()` to acknowledge release. This avoids an allocation for hot-path ask calls.

### Ownership of Message Data

Message data ownership flows:

1. **Sender** allocates and owns the data before `send`.
1. On successful `send`, ownership **transfers to the mailbox**.
1. On `receive`, ownership **transfers to the receiving actor**.
1. The receiver **must release** message data with `kain_actor_message_release()`.

If `send` fails (e.g., mailbox full), the sender **retains ownership** and must free.

______________________________________________________________________

## 6. Reply Ports and `ask` ___ The Request/Response Protocol

### How `ask` Works Under the Hood

The compiler lowers `ask(actor, "MessageName", payload)` into:

1. **Prepare a reply port** - `kain_actor_reply_port_prepare_direct()` creates a per-thread synthetic actor ref with a fresh generation token.
1. **Send the ask message** >> `kain_actor_ask_send_ref()` enqueues the message with the reply-port ref embedded.
1. **Inline fast path** >> if the target actor's mailbox is empty and the actor is not already owned by the scheduler, the runtime processes the message **inline on the caller's thread** as a single microcell turn. This avoids context-switch overhead for low-load actors.
1. **Wait for reply** ~> `kain_actor_reply_port_wait()` blocks the caller's thread, spinning for 256 iterations, then falling back to OS wait primitives.
1. **Timeout** ... if no reply arrives within the timeout (default 30s), the reply port is destroyed and the caller gets an error.
1. **Reply routing** === when the handler does `send reply_to.Reply(value = N)`, the runtime uses `kain_actor_reply_port_send_handle()` to match the generation-tagged ref and deliver the reply directly to the waiting caller, without a global actor-table lookup.

### The Reply-Port Lifecycle

```
[ask starts] → prepare_direct() → ask_send_ref()
    → [maybe inline turn] → wait()
    → [handler processes] → send reply_to.Reply(...) → send_handle() → wait() unblocks
    → [or timeout] → destroy() → error returned
```

Reply ports are **reused**. After a successful roundtrip, the port is rearmed by bumping its generation. After a timeout, it's returned to the TLS cache. This means thousands of `ask` calls per second don't create thousands of temporary actors.

### Why Not Just Futures?

`ask` looks like a future but it's **not** ⁓ it's a direct synchronous block on the caller's thread. This is intentional: actors in Kain can also participate in Kain's world graph, where state propagation is deterministic and synchronous within a tick. `ask` preserves this determinism boundary. For truly async work, Kain has a separate `async`/`future`/`poll` mechanism in `runtime/native/include/async.h`.

______________________________________________________________________

## 7. Supervision Trees - Fault Tolerance at Scale

### The Philosophy: Let It Crash

Kain inherits the Erlang philosophy: **don't write defensive handler code for every edge case; let actors crash and let their supervisors decide what to do.** This is not neglect ~ it's a **provable** strategy for building systems that stay up.

### Supervision Strategies

Four strategies control how a supervisor reacts to child failure:

| Strategy | Behavior |
|----------|----------|
| **OneForOne** | Only the failed child is restarted. Siblings are unaffected. |
| **OneForAll** | All children are terminated and restarted if any one fails. |
| **RestForOne** | The failed child and all children spawned *after* it are terminated and restarted. |
| **SimpleOneForOne** | All children are identical. Only the failed instance is restarted. |

### Restart Policies

Three policies determine *when* a child is restarted:

| Policy | Behavior |
|--------|----------|
| **Permanent** | Always restart, regardless of exit reason. |
| **Temporary** | Never restart. Child stays dead. |
| **Transient** | Restart only on abnormal exit (crash, kill, escalation). Normal exits leave the child dead. |

### Restart Intensity

Supervisors have a safety valve: **restart intensity** limits the rate of restarts to prevent restart storms.

```c
#define KAIN_SUPERVISION_MAX_RESTARTS 5
#define KAIN_SUPERVISION_RESTART_WINDOW_MILLIS 60000ULL
```

If a child restarts more than 5 times within 60 seconds, the supervisor considers its strategy exhausted and **escalates** the failure to *its own* supervisor. This cascading escalation is the key to partial failure containment: the blast radius is always bounded.

### The Supervision Tree at Runtime

```
                    ┌──────────────┐
                    │ RootSup      │  (OneForOne, Permanent)
                    └──┬───┬───┬──┘
          ┌────────────┘   │   └────────────┐
          ▼                ▼                ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │ DBWorker │    │HTTPWorker│    │ AuthSvc  │
    │ (2 children)   │ (Transient)   │ (Permanent)
    └──┬───┬──┘    └──────────┘    └──────────┘
       │   │
       ▼   ▼
    [child] [child]
```

### The SupervisionState Snapshot

The runtime tracks per-supervisor:

```c
typedef struct {
    size_t observed_child_exit_count;
    KainActorId last_observed_child_id;
    KainActorExitReason last_observed_child_exit_reason;
    int supervision_limit_hits;
    size_t restart_attempt_count;
    size_t strategy_shutdown_count;
    size_t escalation_count;
} // ...via KainActorSupervisionSnapshot
```

These are queryable at the Kain level via `actor_supervision_observed_child_exit_count()`, `actor_supervision_restart_attempt_count()`, `actor_supervision_escalation_count()`, and `actor_supervision_limit_hit()`.

______________________________________________________________________

## 8. Monitors and Links ~~ Failure Propagation

### Monitors

A **monitor** is a unidirectional relationship: actor A watches actor B. When B exits (for any reason -- normal or abnormal), A receives a notification message with type tag `0xDEAD0000 | exit_reason`.

```kn
// At the Kain level:
let monitor_id = actor_monitor(watcher_id, watched_id)

// Later, watcher's handler receives:
on ExitNotification(type_tag: Int, data: ptr<Int>):
    // type_tag = 0xDEAD0000 | exit_reason
```

Monitors are:

- Unidirectional (A watches B, B doesn't know about A)
- Many-to-many (many actors can watch one; one can watch many)
- Idempotent (duplicate registrations are no-ops)
- Auto-cleaned on termination of either party

### Links

A **link** is a bidirectional, symmetric relationship. If either linked actor exits **abnormally** (crashed, killed, or supervisor escalation), the other is **terminated** with `EXIT_KILLED`.

Links are for crash containment: "we live together or we die together."

```
A ←── link ──→ B
   (if A crashes, B is killed)
```

Normal exits (normal shutdown, graceful stop) do **not** propagate through links. This is intentional: graceful shutdown should not kill cooperators.

### When to Use What

| Mechanism | Use When |
|-----------|----------|
| **Monitor** | You need to know *when* another actor dies, but don't want to die with it. E.g., a metrics collector watching workers. |
| **Link** | You want crash co-destiny. E.g., a request handler linked to its session actor --- if the session dies, the handler should die too. |
| **Supervisor** | You control the lifecycle. You *own* the children and decide restart policy. E.g., a database connection pool supervisor. |

______________________________________________________________________

## 9. Actor Registry and Addressing

### The Registry

The actor registry maps **string names** to **actor IDs**. It's a hash table of 256 buckets with a dedicated lock.

```kn
actor_registry_register("global-logger", logger_id)
let logger_id = actor_registry_lookup("global-logger")  // returns ID or INVALID
actor_registry_unregister("global-logger")
let exists = actor_registry_has("global-logger")  // Bool
```

Use patterns:

- **Named services** ->> register your HTTP router, logging sink, or telemetry collector.
- **Discovery** ⁓ actors can find each other by well-known names.
- **External entry points** => C FFI code can look up actors by name.

### Actor Addresses and Paths

At the Rust model level, actors have hierarchical addresses:

```rust
ActorPath {
    system: "my-system",
    segments: ["root-supervisor", "http-pool", "worker-3"]
}
// Renders as: kain://my-system/root-supervisor/http-pool/worker-3
```

This is the model |-> the native runtime currently uses flat string names.

______________________________________________________________________

## 10. The Scheduler --- Fairness, Lanes, and Worker Pools

### Architecture

The scheduler is a **ring-buffer work queue** (`KAIN_SCHEDULER_QUEUE_CAPACITY = 1024`) with a **worker pool** of 4 threads:

```
┌──────────────────────────────────────────────────┐
│                 Scheduler Ring                     │
│  [slot 0] [slot 1] ... [slot 1023]                 │
│     ↑enqueue_cursor          ↑dequeue_cursor        │
│                                                    │
│  Worker Pool: [W0] [W1] [W2] [W3]                  │
│     Each worker: loop { dequeue → execute → poll }  │
└──────────────────────────────────────────────────┘
```

Key scheduler metrics (queryable at runtime):

| Metric | Function |
|--------|----------|
| Queue depth | `actor_scheduler_queue_depth()` |
| Max queue depth (ever) | `actor_scheduler_max_queue_depth()` |
| Total enqueued (lifetime) | `actor_scheduler_total_enqueued()` |
| Total dequeued (lifetime) | `actor_scheduler_total_dequeued()` |
| Active workers | `actor_scheduler_active_workers()` |
| Busy workers | `actor_scheduler_busy_workers()` |
| Overflow thread spawns | `actor_scheduler_overflow_thread_spawns()` |

### Scheduler Lanes

At the model level, actors are assigned to scheduler lanes:

```rust
enum SchedulerLaneKind { Control, Cpu, Io, Realtime }

struct SchedulerLanePolicy {
    name: String,
    kind: SchedulerLaneKind,
    worker_threads: usize,     // dedicated threads for this lane
    mailbox_poll_budget: usize, // messages to process before yielding
}
```

This allows **lane isolation**: CPU-heavy actors run on CPU lanes, I/O-bound actors on I/O lanes, and soft-realtime actors (like audio or game ticks) on realtime lanes. The lane system prevents a compute-heavy actor from starving a latency-sensitive one.

### Scheduling Fairness

Actors are scheduled round-robin. When a message arrives for an idle actor, the actor is enqueued in the scheduler ring. Workers dequeue and execute one **microcell turn** per actor, then re-enqueue if more messages remain. This prevents a single busy actor from monopolizing a worker.

The default microcell turn budget is 64 messages (`KAIN_ACTOR_DEFAULT_MICROCELL_TURN_BUDGET`). After processing 64 messages (or when the mailbox is empty), the actor yields.

### Cooperative Yielding

Actors can cooperatively yield by returning `YIELDED` from their turn function. The scheduler re-enqueues them at the back of the ring.

______________________________________________________________________

## 11. Ownership and Lifecycle

### Actor State Machine

```
UNINITIALIZED
    ↓ spawn()
INITIALIZING
    ↓ bootstrap / first message
RUNNING ←──────────┐
    ↓ shutdown()   │ resume
SHUTTING_DOWN      │
    ↓ cleanup      │
TERMINATED          │
    (or)            │
FAILED ────────────┘ (if restart policy triggers)
```

### Lifecycle States

| State | Meaning |
|-------|---------|
| `UNINITIALIZED` | Slot allocated, not yet started |
| `INITIALIZING` | Thread spawned, bootstrap not yet entered |
| `RUNNING` | Processing messages normally |
| `SUSPENDED` | Blocked on mailbox or external resource |
| `SHUTTING_DOWN` | Shutdown requested, draining mailbox |
| `TERMINATED` | Clean exit |
| `FAILED` | Abnormal exit (crash, kill, escalation) |

### Termination Cleanup Order

The runtime guarantees a strict cleanup order during actor termination:

1. Set state to `SHUTTING_DOWN`
1. Close mailbox (reject new sends)
1. Wait for bootstrap/turn function to return
1. Optionally drain remaining mailbox messages
1. Notify all monitors of exit
1. Propagate to linked actors (if abnormal exit)
1. Notify supervisor for restart decision
1. Remove from registry
1. Free all monitor node allocations
1. Free all link node allocations
1. Free all queued mailbox messages
1. Destroy synchronization primitives
1. Release retained `user_data` reference
1. Join/close thread
1. Set state to `TERMINATED` or `FAILED`
1. Free actor state structure
1. Clear slot and bump generation in actor table

This is verified by Z3 proofs in `runtime/native/src/core/z3/proofs-experimental/`.

______________________________________________________________________

## 12. Execution Classes and Locality

### Execution Classes

Every actor has an **execution class** that defines *how* it executes:

| Class | Description |
|-------|-------------|
| `MICROCELL (1)` | Standard actor, scheduled in worker pool. This is the default. |
| `WORLDCELL (2)` | Actor bound to a world's tick cycle. Processes messages in lockstep with world updates. |
| `NETCELL (3)` | Actor representing a remote (networked) actor. Messages are serialized and sent over the wire. |
| `HOSTCELL (4)` | Actor backed by a host-language thread (Python, C++, etc.) through FFI. |
| `ACCELERATOR_CELL (5)` | Actor running on a GPU compute lane. Messages are GPU kernel invocations. |
| `SYNTHETIC_REPLY_PORT (6)` | Generated reply port for `ask`. Not user-facing. |

### Locality Classes

| Class | Description |
|-------|-------------|
| `LOCAL (1)` | Local to this process. Default. |
| `WORLD_AFFINE (2)` | Lives on a specific world node. |
| `HOST_AFFINE (3)` | Lives on a host-language runtime (Python, Node.js, etc.). |
| `REMOTE (4)` | Lives on another machine. Address is a network endpoint. |

These are carried in the generation-tagged actor ref, so the runtime always knows *where* and *how* an actor runs ~> no second-guessing or dispatch-table indirection.

______________________________________________________________________

## 13. Actors + Kain Semantics – The Full Fusion

This is where Kain actors transcend the Erlang/Akka model. Kain's actors are **not isolated islands** -- they are participants in the full semantic graph.

### Actors + World + Entangle

Actors can read world state and have their handlers triggered by world changes. The `FUSION CHAIN` benchmark (`benchmark/cases_v2/fusion_chain.kn`) demonstrates:

```kn
// World mutation → resonate → actor receives cascaded value
let tick = fusion_strike_signal(FusionAuthority, new_signal)  // patch writes world
let shadow = FusionAuthority.shadow                            // resonate updated shadow
let reply = ask(relay, "Signal", fusion_pack(shadow, tick))    // actor processes shadow
```

The causal chain: **patch → resonate → orchestrate → entangle → actor reads mirror**. All synchronous within a tick. The actor becomes just one more node in the reactive propagation graph.

### Actors + Collapse/Observe/Decay (Ownership)

Actors can own and operate on raw memory inside message handlers:

```kn
actor FusionTeleporter:
    on ShatterAndSend(reply_to: P, payload: Int):
        let mut cells: ptr<Int> = alloc_zeroed(4, "Int")
        collapse cells:
            // write to cells
        0
        let head: Int = observe cells:
            mem_load(ptr_offset(cells, 0, "Int"), "Int")
        decay cells
        // ... use head
```

This means actors can do FFI, manage GPU buffers, handle network packet parsing, or operate on mmap'd files |-> all within the actor's single-threaded safety zone. The compiler's Z3-backed ownership verifier (collapse/observe/decay) ensures the memory is safe.

### Actors + Shatter/Teleport (Zero-Copy)

Actors can shatter and teleport data across world boundaries:

```kn
let shard = FusionShard { bias: 42, phase: 13, tick: 7, checksum: 99, alive: true }
let moved = teleport shard from FusionAuthority to FusionMirror via fusion_shard_bus
```

This is a **zero-copy** data transfer: the shard's memory is remapped, not copied. The actor becomes a crossing guard at world boundaries, shattering structured data for zero-copy movement.

### Actors + Converge (Multi-Lane Dispatch)

Actors can call `converge` functions inside handlers. The converge system selects the fastest implementation lane at runtime:

```kn
converge fusion_fast_mix(value: Int, seed: Int) -> Int:
    spec reference:
        return fusion_mix_scalar(value, seed)
    fast closed_form_lane when target("llvm"):
        return fusion_mix_closed(value, seed)
```

Inside an actor handler:

```kn
on Signal(reply_to: P, payload: Int):
    let mixed = fusion_fast_mix(signal_val + seed, self.bias + self.turns)
```

### Actors + Pulse + Orchestrate

Actors can be driven by pulse clocks and participate in orchestrate pipelines:

```kn
pulse fusion_tick_driver every 8 ms:
    // Pulse fires → world ticks → actor reads tick
    let reply = ask(teleporter, "ShatterAndSend", fusion_pack(mirror_tick, mirror_sig))
```

And actors can appear as stages in `orchestrate` pipelines:

```kn
orchestrate fusion_signal_pipeline(value: Int, tick: Int) -> Int:
    stage host_mix: cpu fusion_mix(value + tick) ...
    stage fast_mix: converge fusion_fast_mix(host_mix, tick) ...
```

### Actors + Patch + Law

Actors can trigger patches:

```kn
on Signal(reply_to: P, payload: Int):
    let tick = fusion_strike_signal(FusionAuthority, new_signal)  // patch from actor
```

And law invariants guard the world state before patches commit:

```kn
law fusion_signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < FUSION_MODULUS
```

### The Fusion Chain ___ All Together

The `FUSION CHAIN` benchmark runs all of these simultaneously in a single scenario:

```
world mutation (patch)
    → resonate fires (tripwire)
        → orchestrate pipeline (multi-stage)
            → entangle propagates to mirror
                → pulse ticks observe mirror
                    → actor receives ask with live mirror value
                        → actor executes collapse/observe/decay
                            → actor shatter-teleports payload
                                → destination world receives result
```

This is 7 layers of Kain semantics, all fused through an actor as the coordination point. This isn't a theoretical demo ->> it's a benchmark that runs every CI cycle.

______________________________________________________________________

## 14. Use Cases ⁓ From 3D to Servers

### 14.1 Game Engine / 3D Tool

Actors are natural for game engine subsystems. From the Pong benchmark:

```kn
actor InputWorker:        // Reads input, applies drift corrections
    state pulses: Int = 0
    on Drift(left_delta: Int, right_delta: Int): ...

actor PhysicsWorker:      // Steps simulation, tracks bounces/goals
    state steps: Int = 0
    on Step(bounced: Int, goal_scored: Int): ...

actor RenderWorker:       // Presents frames, counts draw calls
    state frames: Int = 0
    on Present(draw_count: Int): ...
```

These three actors are fire-and-forget (`send`). The game loop advances world state via `patch`, which triggers `entangle` propagation to a mirror world, and the actors read the mirror. This is lock-free, race-free concurrency with no explicit synchronization.

**3D tool example:** A DCC (digital content creation) tool could use actors for:

- `UndoManager` ~~ serialized command history, runs on a dedicated thread so undo is never blocked by rendering.
- `GPUCompiler` ~> receives shader source, returns compiled SPIR-V via `ask`. Runs on an accelerator lane.
- `AssetImporter` * * * watches a directory, imports FBX/glTF on change. Collapses raw file data, observes parsed meshes, decays the raw buffer.
- `ViewportFramer` - on a pulse clock, reads world state (camera, scene graph) and issues draw commands.

### 14.2 HTTP Server / API Gateway

From the network domains blade:

```kn
actor NetworkDomainProbe:
    state hits: Int = 0
    on HttpRequest(payload: String):
        self.hits = self.hits + len(payload)

// Route POST /domains to this actor:
route_actor(server, "POST", "/domains", handler, "HttpRequest")
```

The server actor model scales naturally:

```kn
actor HttpRouter:
    state routes: ptr<Route> = ...
    on Request(reply_to: P, method: String, path: String, body: String): ...

actor RateLimiter:
    state window_hits: Int = 0
    on Check(reply_to: P, client_ip: String): ...

actor AuthGate:
    state tokens: ptr<TokenCache> = ...
    on Validate(reply_to: P, token: String): ...

// Supervision tree:
//   RootSupervisor (OneForOne)
//     → HttpRouter (Permanent)
//     → RateLimiter (Permanent)
//     → AuthGate (Transient)
//     → WorkerPool of 8 (Permanent)
```

Each request flows through: Router → ask RateLimiter → ask AuthGate → delegate to Worker. If a worker crashes, its supervisor restarts it. If AuthGate crashes transiently (e.g., token cache corruption), its supervisor restarts it. If the router crashes, the whole server restarts (OneForAll at root).

### 14.3 File System / I/O Pipeline

A Kain file system could use actors for I/O scheduling:

```kn
actor IoDispatcher:
    state pending: Int = 0
    on Read(reply_to: P, path: String, offset: Int, len: Int):
        // Dispatch to I/O lane, return data via reply port
        let data = native_read_chunk(path, offset, len)
        send reply_to.Data(chunk = data)

actor FileWatcher:
    state watched: ptr<WatchList> = ...
    on Notify(reply_to: P, event: String, path: String): ...

actor BufferCache:
    state cache: ptr<HashMap> = ...
    on Lookup(reply_to: P, key: Int): ...
    on Evict(): ...
```

The `IoDispatcher` runs on the I/O scheduler lane. `FileWatcher` uses `collapse/observe/decay` for the OS-level inotify/WatchService buffer. `BufferCache` uses raw memory (`alloc`, `realloc_mem`, `mem_load`, `mem_store`) for a lock-free cache :: safe because only one message is processed at a time.

### 14.4 GPU Compute Pipeline

```kn
actor ShaderCompiler:
    state cache_hits: Int = 0
    on Compile(reply_to: P, source: String, entry: String):
        let spirv = compile_spirv(source, entry)
        send reply_to.Spirv(bytecode = spirv)

actor ComputeDispatcher:
    state pending_kernels: Int = 0
    on Dispatch(reply_to: P, kernel: String, grid: GridDim, block: BlockDim):
        // Launch on GPU via Vulkan/CUDA
        let result = launch_compute(kernel, grid, block)
        send reply_to.Done(result = result)

actor GpuMemoryPool:
    state allocations: ptr<Pool> = ...
    on Alloc(reply_to: P, size: Int): ...
    on Free(handle: Int): ...
```

These actors run on `ACCELERATOR_CELL` execution class. When a compute dispatch is ready, the runtime routes it to the GPU scheduler rather than a CPU worker thread.

### 14.5 Audio / Realtime Processing

```kn
actor AudioEngine:
    state buffer: ptr<Float> = ...
    state position: Int = 0

    on FillBuffer(reply_to: P, frames: Int):
        collapse self.buffer:
            // Write audio samples directly into native buffer
            var i: Int = 0
            while i < frames:
                let sample = sin(f32(self.position + i) * 0.01)
                mem_store(ptr_offset(self.buffer, i, "Float"), sample, "Float")
                i = i + 1
        decay self.buffer
        send reply_to.Filled(samples = frames)
```

This runs on a `Realtime` scheduler lane with dedicated thread affinity. The actor uses raw pointer math (collapse/observe/decay) to write directly into the audio driver's DMA buffer |-> no copies, no allocation. Kain's ownership system verifies the buffer is used safely.

### 14.6 Distributed Systems / Microservices

```kn
actor ClusterMember:
    state peers: ptr<PeerList> = ...
    state term: Int = 0

    on AppendEntries(reply_to: P, term: Int, leader_id: Int): ...
    on RequestVote(reply_to: P, term: Int): ...
    on Heartbeat(term: Int): ...
```

Raft/Paxos-style consensus actors. Each member is a `NETCELL` 〰 its messages are serialized and sent over the wire. The supervision tree handles network partition recovery: if a peer actor fails to respond, the supervisor restarts connectivity.

### 14.7 FFI / Host Language Bridge

```kn
actor PythonBridge:
    state py_state: ptr<Int> = ...
    on Execute(reply_to: P, code: String):
        let result = py_exec(self.py_state, code)
        send reply_to.PythonResult(value = result)
```

This is a `HOSTCELL` actor. Its state is a pointer to CPython's interpreter state. Messages are Kain requests that get forwarded to Python execution. The actor provides a **safe Kain interface** to an inherently unsafe FFI boundary.

______________________________________________________________________

## 15. Performance and Tuning

### Mailbox Tuning

| Scenario | Recommendation |
|----------|---------------|
| High-throughput fire-and-forget | Unbounded mailbox, `Block` overflow |
| Request-response with backpressure | Bounded (256–1024), `Block` overflow |
| Bursty telemetry (don't care if dropped) | Bounded, `DropOldest` overflow |
| Critical control messages | Bounded, `FailSender` === caller must retry |

### Scheduler Tuning

| Scenario | Lane | Workers |
|----------|------|---------|
| CPU-heavy compute actors | `Cpu` | `num_cores` |
| I/O-bound (file, network) | `Io` | 2× for async I/O depth |
| Audio / game tick | `Realtime` | 1, with RT thread priority |
| Supervision / control plane | `Control` | 1 |

### Turn Budget

The microcell turn budget (default 64 messages) controls fairness vs. throughput:

- **Lower** (16–32): More context switches, fairer scheduling. Good for latency-sensitive workloads.
- **Higher** (128–256): Fewer switches, better throughput for batch workloads. Risk of head-of-line blocking.

### Inline Ask Fast Path

The inline ask fast path (executing the first message directly on the caller's thread) is a significant optimization for low-load actors. It avoids:

- Enqueuing into the scheduler ring
- Waking a worker thread
- Context switching

For hot `ask` calls where the target actor is idle, the roundtrip time approaches a function call. This is what makes actor-based request-response viable for things like per-frame game logic queries.

### Overflow Thread Spawning

When the scheduler ring is full and all workers are busy, the runtime spawns temporary threads. Tracked by `actor_scheduler_overflow_thread_spawns()`. If this number is climbing, either:

- Increase worker count
- Increase queue capacity
- Add more actors (to shard work) rather than making individual actors do too much

______________________________________________________________________

## 16. Best Practices and Anti-Patterns

### ✅ Do

1. **Supervision trees for fault isolation.** Always design your system as a tree. The root supervisor restarts critical infrastructure; leaf supervisors restart pools.
1. **`ask` for request-response, `send` for notifications.** Don't add reply ports to fire-and-forget messages.
1. **Bounded mailboxes for services.** Unbounded mailboxes are a memory leak waiting to happen if a consumer falls behind.
1. **Monitor critical actors.** A metrics collector that monitors your database pool can alert on unexpected exits.
1. **Use actor names in the registry for well-known services.** Don't pass raw actor IDs through startup code.
1. **Pack multi-value replies into `Int` using modulus arithmetic for `ask`.** The single-`Int` reply is a temporary limitation; packing is the established pattern.
1. **Use `collapse/observe/decay` inside actor handlers for FFI, GPU buffers, and raw I/O.** The actor's single-threaded invariants make ownership tracking tractable.
1. **Register scheduler telemetry in production.** `actor_scheduler_queue_depth()` and `actor_scheduler_overflow_thread_spawns()` are your early-warning system for overload.

### ❌ Don't

1. **Don't share mutable state between actors.** Use `entangle`/world state for shared reactive state, or pass data in messages.
1. **Don't block inside an actor handler.** Long-running work (file I/O, network calls) should delegate to I/O lanes or async tasks, not block the worker thread.
1. **Don't create flat actor systems without supervision.** If every actor is a peer, a single crash can cascade. Use supervision trees.
1. **Don't ignore `ask` timeouts.** The default is 30 seconds. If your handler might take longer (e.g., GPU compilation), set realistic timeouts.
1. **Don't leak message data.** Every `receive` must be paired with `kain_actor_message_release()`. The Z3 proofs verify this at the runtime level; don't circumvent it at the Kain level.
1. **Don't use actors where a function would suffice.** Not everything needs an actor. Simple data transformations, pure computations, and stateless utilities should be functions. Actors are for stateful, concurrent, long-lived processes.

______________________________________________________________________

## 17. Appendix --> Complete std::actor API

The `std::actor` module exposes 74 public symbols. Here are all the Kain-level wrappers (each is a thin wrapper around a `native_` or `abi_` runtime call):

### Lifecycle & Identity

| Function | Signature |
|----------|-----------|
| `actor_invalid_id()` | `→ Int` |
| `actor_abi_version()` | `→ Int` |
| `actor_id_is_valid(id)` | `(Int) → Bool` |
| `actor_state_invalid(id)` | `(Int) → Bool` |
| `actor_get_state(id)` | `(Int) → Int` |
| `actor_is_running(id)` | `(Int) → Bool` |
| `actor_is_terminal(id)` | `(Int) → Bool` |

### Spawn & Send

| Function | Signature |
|----------|-----------|
| `actor_spawn(name, payload)` | `(String, String) → Int` |
| `actor_send(id, msg_name, payload)` | `(Int, String, String) → Int` |

### Shutdown & Kill

| Function | Signature |
|----------|-----------|
| `actor_shutdown(id)` | `(Int) → Int` |
| `actor_kill(id)` | `(Int) → Int` |

### Registry

| Function | Signature |
|----------|-----------|
| `actor_registry_lookup(name)` | `(String) → Int` |
| `actor_registry_register(name, id)` | `(String, Int) → Int` |
| `actor_registry_unregister(name)` | `(String) → Int` |
| `actor_registry_has(name)` | `(String) → Bool` |

### Monitoring & Linking

| Function | Signature |
|----------|-----------|
| `actor_monitor(monitor_id, monitored_id)` | `(Int, Int) → Int` |
| `actor_demonitor(monitor_id, monitored_id)` | `(Int, Int) → Int` |
| `actor_link(a_id, b_id)` | `(Int, Int) → Int` |
| `actor_unlink(a_id, b_id)` | `(Int, Int) → Int` |

### Supervision Telemetry

| Function | Signature |
|----------|-----------|
| `actor_supervision_max_restarts()` | `→ Int` |
| `actor_supervision_restart_window_millis()` | `→ Int` |
| `actor_supervision_observed_child_exit_count(id)` | `(Int) → Int` |
| `actor_supervision_restart_attempt_count(id)` | `(Int) → Int` |
| `actor_supervision_escalation_count(id)` | `(Int) → Int` |
| `actor_supervision_limit_hit(id)` | `(Int) → Bool` |

### Scheduler Telemetry

| Function | Signature |
|----------|-----------|
| `actor_scheduler_queue_depth()` | `→ Int` |
| `actor_scheduler_max_queue_depth()` | `→ Int` |
| `actor_scheduler_total_enqueued()` | `→ Int` |
| `actor_scheduler_total_dequeued()` | `→ Int` |
| `actor_scheduler_worker_count()` | `→ Int` |
| `actor_scheduler_active_workers()` | `→ Int` |
| `actor_scheduler_busy_workers()` | `→ Int` |
| `actor_scheduler_overflow_thread_spawns()` | `→ Int` |

### Configuration

| Function | Signature |
|----------|-----------|
| `actor_default_mailbox_capacity()` | `→ Int` (1024) |
| `actor_unbounded_mailbox_capacity()` | `→ Int` (0) |
| `actor_default_ask_timeout_ms()` | `→ Int` (30000) |
| `actor_default_shutdown_grace_ms()` | `→ Int` (5000) |

______________________________________________________________________

## Further Reading

- **Runtime ABI**: `runtime/native/include/actor.h` >> the complete C API (600+ lines of documented declarations).
- **Ownership Rules**: `runtime/native/include/ACTOR_RUNTIME_OWNERSHIP.md` – detailed ownership and cleanup semantics.
- **Rust Model**: `crates/actor/src/lib.rs` and its 14 submodules ... the compiler-side vocabulary for actor definitions, validation, and lowering.
- **Z3 Proofs**: `runtime/native/src/core/z3/proofs-experimental/` --- 45+ SMT2 proofs covering mailbox invariants, reply-port generation, scheduler bounds, and atomic state transitions.
- **Benchmarks**: `benchmark/cases_v2/fusion_chain.kn` ... the definitive 7-layer semantic fusion benchmark using actors as the coordination spine.
- **Blades**: `blades/experiments/convergence/`, `blades/experiments/pong/`, `blades/network/domains/` – real-world actor usage in complete Kain projects.
- **Smoketest**: `smoketest/src/semantics/actor.kn` --- the canonical actor correctness proof.

______________________________________________________________________

> *Actors are not an add-on. They are the concurrency primitive Kain was built around. When in doubt, spawn an actor. When sure, spawn a supervisor. When very sure, check the Z3 proofs.*
