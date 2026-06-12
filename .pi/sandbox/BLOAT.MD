# BLOAT.MD — pi-messenger Fat Audit

## Verdict: BLOATED. Objectively.

This is not "comprehensive," "feature-rich," or "batteries-included." It is 12,000+ lines of TypeScript doing what ~2,000 lines of Kain + ~500 lines of TypeScript shell could do better, faster, and without crashing.

---

## The Core Concept (What Actually Matters)

Four things. That's it.

| # | Concept | What it means |
|---|---------|---------------|
| 1 | **Agent mesh** | Agents discover each other. Know who's online, what project they're in, what model they're running. |
| 2 | **Messaging** | Agent A sends a message to Agent B. Agent B receives it as a steer. |
| 3 | **Task graph** | Define tasks with dependencies. Execute them in parallel waves. Track completion. |
| 4 | **Debate protocol** | Structured multi-agent deliberation: propose → critique → revise → judge. (Not in current code — this is the feature that makes it worth rebuilding.) |

That's the product. Everything else is implementation or decoration.

---

## What Can Be Stripped

### Category A: Bloat (remove entirely — zero user value lost for the core use case)

| Feature | Lines | Why it's bloat |
|---------|-------|----------------|
| **Chat overlay** (overlay.ts + overlay-render.ts + overlay-actions.ts) | **~70KB / 2,000 lines** | Full TUI application inside an extension. The user interacts with pi, not a chat client. Messages should surface as steer prompts, not in a terminal IRC. |
| **Config overlay** (config-overlay.ts) | **~150 lines** | A TUI for editing JSON config. Just edit the JSON file. |
| **Auto-status messages** ("on fire", "debugging...") | **~200 lines** | Cute but useless. No agent makes decisions based on "SwiftRaven is on fire." |
| **Stuck detection + notifications** | **~150 lines** | If an agent is stuck, the human notices. The system doesn't need to. |
| **Name themes** (SwiftRaven, LunarDust, OakTree) | **~300 lines** | Memorable names are nice but the generation logic + word lists + theme config is disproportionate. Just use `PI_AGENT_NAME` or a hash. |
| **Lobby workers** (crew/lobby.ts) | **~400 lines** | Pre-spawn idle workers that "join the mesh and chat." This is a solution to cold-start latency that creates more problems than it solves. Workers should spawn on demand, do work, die. |
| **Worker coordination/chatter** (crew/handlers/coordination.ts) | **~250 lines** | Workers DMing each other "I'm starting task-3!" is noise. The task graph IS the coordination. |
| **Coordination levels** (none/minimal/moderate/chatty) | **~200 lines** | Four configurable verbosity levels for worker chatter. One level (none) is correct. |
| **Feed flash notifications** | **~100 lines** | "Significant events trigger flash notifications in the status bar." Terminal UI animation for feed events. |
| **Feed event system** (feed.ts + 20+ event types) | **~400 lines** | Append-only JSONL with 25 event types (plan.start, plan.pass.start, plan.pass.done, plan.review.start, plan.review.done, ...). The task graph already tells you what happened. |
| **Auto-overlay opening** (maybeAutoOpenCrewOverlay) | **~150 lines** | Automatically pops open the TUI overlay when planning completes or autonomous work is pending. The agent should just tell you in the chat. |
| **Custom status messages** (`set_status` action) | **~50 lines** | Agents setting "reviewing" as their status. Nobody reads it. |
| **Human-as-participant** | **~100 lines** | Special-casing the human session in the agent list with "(you)" marker. The human isn't an agent. |
| **Sender details on first contact** | **~80 lines** | "SwiftRaven is in my-project on main (claude-sonnet-4-5)" on first message. Unnecessary ceremony. |
| **Reply hints** | **~30 lines** | "reply: pi_messenger({ action: \"send\", to: \"...\", message: \"...\" })" appended to every message. The LLM already knows how to call tools. |
| **Registration context** | **~50 lines** | Sends a hidden message on join telling the agent what its name is and to use pi_messenger for status/work. The promptSnippet already does this. |
| **Auto-register paths** | **~200 lines** | Glob-based path matching to auto-join on session start. Just set `autoRegister: true` or call `join` explicitly. |
| **Plan auto-start work** | **~80 lines** | After planning completes, auto-triggers autonomous work. The agent should explicitly decide to start work. |
| **Task revision** (revise.ts + revise-tree) | **~400 lines** | Revise a single task or entire subtree via the planner agent. If a task is wrong, delete it and re-plan. |
| **Advisory vs strict dependencies** | **~150 lines** | Two dependency modes. One is correct (strict). Advisory turns the task graph into a suggestion. |
| **Activity tracking** (tool calls, tokens, files modified) | **~250 lines** | Tracking every edit/commit/test for the feed and auto-status. The agent doesn't need to broadcast "I just ran a test." |

**Bloat subtotal: ~4,000+ lines of TypeScript that can be deleted with zero loss to the core concept.**

### Category B: Implementation Mistake (keep the concept, kill the implementation)

| What it does | Current implementation | Why it's wrong | What it should be |
|-------------|----------------------|----------------|-------------------|
| Agent discovery | `fs.readdirSync` on registry dir every status update, parse every JSON, check PID liveness | Disk I/O on every heartbeat. Stale for up to 1s. Cache invalidation bugs. | Kain binary holds agent table in memory. Subprocess runs, reads registry once, returns results, exits. |
| Message delivery | `fs.watch` on inbox directory. Read file, parse JSON, deliver, delete file. | `fs.watch` is notoriously unreliable on Windows. Race conditions on concurrent reads. | Kain binary with actor mailboxes. Messages delivered directly from sender to receiver in the same process. |
| File reservations | JSON files in registry directory. `pathMatchesReservation()` string matching. | String prefix matching on paths. No conflict resolution beyond "block and tell user." | Kain `world` with reservation state. Conflicts detected at reservation time, not write time. |
| Task storage | JSON files in `.pi/messenger/crew/tasks/`. `readJson`/`writeJson` with temp-file atomic rename. | Every task read/write is a disk operation. JSON parsing for every task list. | In-memory task graph in Kain binary. Persisted as snapshot only on shutdown or explicit save. |
| Worker spawning | `spawn("pi", ["--model", "...", "--prompt", "..."])` from Node. Complex artifact/log management. | Node managing child pi processes with progress parsing, truncation, artifact writing. ~350 lines in agents.ts alone. | Kain `spawn` for actors, `orchestrate` for staged execution. The language IS the orchestration layer. |
| File locking | `O_CREAT\|O_EXCL` with 50-retry loop, stale PID detection, 10s timeout. | Fragile. Stale lock detection by PID liveness. Retry loop can block for 5 seconds. | No locks needed. Actor message ordering guarantees serialized access. |
| State management | Module-level mutable `MessengerState` object. 8 timers. 3 caches. Manual invalidation. | Impossible to reason about. Timers holding stale references. Cache TTLs masking race conditions. | Kain `world` for global state. `patch` for mutations. Compiler tracks propagation. |

### Category C: Architecture Mistake (the whole thing is in the wrong language)

The fundamental error is using Node.js (TypeScript) as a **coordination runtime**. 

Node.js is an event-driven I/O platform. It is good at: handling HTTP requests, streaming data, serving web pages. It is terrible at: long-lived stateful coordination, message passing between independent processes, actor supervision, lock-free concurrency.

pi-messenger is an **actor system**:
- Independent agents with identity and state
- Message passing between agents
- Supervision (worker spawning, monitoring, cleanup)
- Task scheduling with dependency graphs

Every single one of these is a solved problem in Kain at the language level:

| pi-messenger (Node) | Kain equivalent |
|---------------------|-----------------|
| `AgentRegistration` JSON file | `actor` with typed state |
| `fs.watch` inbox polling | `send`/`ask` with guaranteed delivery |
| `spawn("pi", ...)` child process | `spawn ActorName(state)` |
| `withSwarmLock()` O_CREAT\|O_EXCL | Message ordering (no locks needed) |
| `isProcessAlive(pid)` checks | Supervision tree (dead actors detected) |
| `AgentsCache` with 1s TTL | `world` state — always consistent |
| `autonomousContinueRepeats` guard | `pulse` with proper lifecycle |
| `writeJson` temp-file atomic rename | `patch` with journal |
| try/catch swallowing everywhere | `law` invariants — compiler-proven |

The messenger extension is 12,000 lines of TypeScript reinventing Kain's semantic stack in a language that actively fights against it. This is why it hits 4GB of memory and OOMs — V8's GC cannot handle a long-lived actor system built on timers, closures, and file I/O.

---

## What Stays

After stripping Categories A, B, and C, what remains is a clean design:

### The Lean Architecture

```
pi-messenger-kain/          (Kain project — the engine)
├── build.kn                (builds to messenger.exe)
├── src/
│   ├── main.kn             (CLI entry: parse args, dispatch action, print result)
│   ├── mesh.kn             (agent registry: join, leave, list, heartbeat cleanup)
│   ├── messages.kn         (message send/receive via actor mailboxes)
│   ├── tasks.kn            (task graph: create, depend, complete, wave calculation)
│   ├── debate.kn           (structured debate protocol: propose→critique→revise→judge)
│   ├── reservations.kn     (file claims with conflict detection)
│   └── store.kn            (JSON snapshot persistence on shutdown)
│
├── index.ts                (~500 lines — TypeScript pi extension shell)
│   (registers "pi_messenger" tool, builds CLI args, calls execSync on messenger.exe,
│    registers "pi_debate" tool for debate protocol, registers slash commands)
│
└── messenger.exe           (compiled Kain binary, ~500KB)
```

### TypeScript Shell Responsibilities (the ONLY things that need to stay in TS)

1. **Tool registration** — `pi.registerTool()` with parameter schema (has to be TypeScript, pi API)
2. **Slash commands** — `/messenger` to open overlay, `/debate` to start debate (has to be TypeScript)
3. **IPC** — `execSync("messenger.exe", args)` and capture stdout (has to spawn from Node)
4. **Output formatting** — wrap binary stdout in pi tool result format (trivial)
5. **Overlay** (optional) — if you want a TUI, it has to use `pi-tui`. But this could be a separate, optional extension. The core engine doesn't need it.
6. **Status bar** — `ctx.ui.setStatus()` with agent count (one-liner, reads from binary output)

Everything else — agent discovery, message routing, task graph, debate protocol, file reservations, state persistence — runs in `messenger.exe`.

### The Debate Protocol (the killer feature)

This is the thing that makes rebuilding worthwhile. A structured multi-agent deliberation protocol:

```
debate "Should we use SQLite or Postgres for the cache layer?"
    round 1: Advocate proposes SQLite → Critic finds 3 flaws → Judge scores
    round 2: Advocate addresses flaws → Critic concedes 2, holds 1 → Judge updates
    round 3: Advocate proposes hybrid → Critic accepts → Judge: SHIP
```

Implemented as Kain actors:

```
actor Advocate:
    state position: String
    state evidence: [Evidence]
    on Propose(topic: String): ...
    on Defend(critique: Critique): ...

actor Critic:
    on Critique(position: String, evidence: [Evidence]): ...
    on Reevaluate(response: Defense): ...

actor Judge:
    state rounds: [Round]
    state verdict: Verdict
    on Evaluate(proposal: Proposal, critique: Critique): ...
    on FinalVerdict(rounds: [Round]): ...
```

This is impossible to build correctly in Node.js. It's a weekend project in Kain.

---

## Summary

| Metric | Current pi-messenger | Lean Kain rebuild |
|--------|---------------------|-------------------|
| TypeScript lines | ~12,000 | ~500 |
| Kain lines | 0 | ~2,000 |
| Binary size | N/A (Node runtime) | ~500KB (.exe) |
| Memory at rest | 50-200MB (Node + V8 heap) | 0MB (binary not running) |
| Memory during use | Escalates to 4GB+ (GC failure) | <50MB (native, stack-allocated) |
| Crash risk | High (timer leaks, stale refs, GC pressure) | Low (subprocess — crash kills binary, not pi) |
| Features | 25+ with 4 configurable modes each | 4 core: mesh, messages, tasks, debate |
| Files | 83 | ~12 |
| Test files | 27 test files for Node code | 0 (Kain has `test` keyword built in — tests live alongside source) |

The current extension is a TypeScript actor system fighting Node.js's event loop. The correct version is a Kain actor system compiled to native code, with TypeScript as a thin IPC shell. The tree-kn extension at `X:\.pi\extensions\tree-kn` proves this pattern works — 92KB of Kain source → 548KB binary, with a 300-line TypeScript shell.
