# pi-squared Runtime & Greeble Audit

> **Date:** 2026-06-13
> **Scope:** Greeble v0.2 architecture, native runtime features, pi-squared v0.1 gaps
> **Purpose:** Identify concrete integration opportunities, unused runtime features, and architecture recommendations

---

## 1. What Greeble Can Offer Pi-Squared

### 1.1 HTTP Server (REST API + Web UI)

Greeble proves that `std::http` works end-to-end for serving HTTP in Kain. Pi-squared currently has **zero network surface** <--> it's a CLI-only agent. With greeble's pattern:

**What pi-squared gets:**
- REST API endpoints for IDE integration (VS Code, JetBrains)
- Web UI dashboard via MCP or HTTP
- `/v1/chat/completions` style LLM proxy
- Telemetry endpoint (`/_telemetry`)

**Greeble's proven pattern** (from `greeble/src/main.kn` + `greeble/src/router.kn`):

```kain
// std::http gives us:
pub fn server_create_localhost(port: Int) -> Int:      // create server
pub fn server_listen(server_id: Int) -> Int:            // start listening
pub fn route_actor(server_id, method, path, actor_id, msg_kind) -> Int:  // wire route
pub fn server_pump(server_id, timeout_ms) -> Int:       // event loop tick
```

**Integration sketch** ‒ pi-squared HTTP server using greeble's pattern:

```kain
use std::http

pub fn pi_http_start(port: Int) -> Int:
    let server = http_server_create_localhost(port)
    if server <= 0: return 1
    if http_server_listen(server) != 0: return 2
    
    // Wire routes to pi-squared actors
    let _ = http_route_actor(server, "GET", "/_telemetry", telemetry_actor_id, "GetTelemetry")
    let _ = http_route_actor(server, "POST", "/v1/chat/completions", provider_actor_id, "Complete")
    let _ = http_route_actor(server, "POST", "/agent/prompt", agent_actor_id, "Prompt")
    
    return http_server_local_port(server)
```

### 1.2 Supervision Tree (Fault Containment)

Pi-squared spawns 5 actors with **zero supervision** === if `SessionTree` crashes, the agent dies silently. Greeble's supervision tree is the proven pattern:

| Feature | Pi-Squared (current) | Greeble (available) | Integration |
|---------|---------------------|---------------------|-------------|
| Restart strategy | None | OneForOne, OneForAll, RestForOne, SimpleOneForOne | Wrap each actor in a supervisor |
| Restart limits | None | 5 restarts / 60s window | Prevent crash loops |
| Escalation | None | Escalated to parent supervisor | Root supervisor catches unmanageable children |
| Monitors | None | `actor_monitor()`, `actor_link()` | Track child liveness |
| Registry | None | `actor_registry_register()`, `actor_registry_lookup()` | Name-based actor discovery |

**Greeble's root supervisor pattern** (from `greeble/src/supervisor.kn`):

```kain
actor RootSupervisor:
    state children: Array<Int> = []
    state child_names: Array<String> = []
    state restart_counts: Array<Int> = []
    state escalation_count: Int = 0

    on ChildExited(child_id: Int, exit_reason: Int):
        var i: Int = 0
        while i < len(self.children):
            if self.children[i] == child_id:
                self.restart_counts[i] = self.restart_counts[i] + 1
                if self.restart_counts[i] > SUP_MAX_RESTARTS:
                    self.escalation_count = self.escalation_count + 1
                else:
                    let name = self.child_names[i]
                    let new_id = actor_spawn(name, "")
                    self.children[i] = new_id
                    self.restart_counts[i] = 0
                break
            i = i + 1
```

**Integration sketch** -- pi-squared supervision tree:

```kain
// PiSquaredSupervisor (OneForOne)
//   ├── SessionTree      |-> Append/Branch/GetContext/AppendMessage/AppendCompaction
//   ├── AgentActor       ‒ Prompt/Continue/Steer/Abort/SetModel/GetState
//   ├── AgentEventBus    ___ Subscribe/Emit/Unsubscribe
//   ├── ResourceLoader   ->> LoadSkill/LoadPrompt/LoadContextFile/...
//   ├── PiSettingsManager- GetSettings/SetSettings/MergeConfig/...
//   └── ProviderRouter   ->> Complete/Stream/Cancel (if HTTP mode)
//
// Restart policies:
//   PERMANENT  → always restart (SessionTree, AgentActor)
//   TRANSIENT  → restart on non-zero exit (ProviderRouter)
//   TEMPORARY  → never restart
```

### 1.3 Telemetry Feed for TUI Status Bar

Greeble's `collect_telemetry()` captures **12 runtime counters** that pi-squared's TUI could display in a status bar:

| Counter | Source | What It Tells Pi-Squared |
|---------|--------|--------------------------|
| `actor_scheduler_queue_depth()` | runtime | How many messages are queued (pressure indicator) |
| `actor_scheduler_busy_workers()` | runtime | How many scheduler threads are busy |
| `actor_scheduler_overflow_thread_spawns()` | runtime | Scheduler is overloaded * * * spawned extra OS threads |
| `patch_journal_count()` | intent | How many world mutations have been journaled |
| `entangle_propagation_count()` | intent | How many entangle syncs have propagated |
| `converge_mismatch_count()` | intent | If tool dispatch mismatch count > 0, tool routing is broken |
| `runtime_machine_pulse_total_fire_count()` | runtime | How many pulse beats have fired (compaction pulses) |
| `supervisor_restart_count()` | actor | Actor crash rate (health signal) |
| `supervisor_escalation_count()` | actor | Unrecoverable crashes (red alert) |

**Greeble's telemetry pattern** (from `greeble/src/telemetry.kn`):

```kain
pub fn collect_telemetry(router_hits: Int, sup_restarts: Int, sup_escalations: Int) -> TelemetrySnapshot:
    return TelemetrySnapshot {
        scheduler_queue_depth: actor_scheduler_queue_depth(),
        scheduler_busy_workers: actor_scheduler_busy_workers(),
        scheduler_worker_count: actor_scheduler_worker_count(),
        scheduler_overflow_spawns: actor_scheduler_overflow_thread_spawns(),
        patch_journal_count: patch_journal_count(),
        entangle_propagation_count: entangle_propagation_count(),
        converge_mismatch_count: converge_mismatch_count(),
        pulse_total_fire_count: runtime_machine_pulse_total_fire_count(),
        supervision_restart_count: sup_restarts,
        supervision_escalation_count: sup_escalations,
    }
```

### 1.4 LLM Provider Calls Through Greeble's Router

Pi-squared's LLM provider calls are currently **pure stubs** (see `src/providers/`). Greeble already has the HTTP router → actor dispatch pattern that could serve as transport:

```kain
// Client sends POST /v1/chat/completions
// http_route_actor dispatches to ProviderRouter actor
// ProviderRouter selects the LLM provider, sends request, returns streaming response

// Provider routing via converge:
converge route_completion(model_id: String, messages: String) -> String:
    spec reference:
        return http_client_request("POST", "https://api.openai.com/v1/chat/completions", messages)
    fast local_lane when capability("provider.local"):
        return ollama_complete(model_id, messages)
    fast cached_lane when capability("provider.response_cache"):
        return cache_lookup(model_id, messages)
    verify random(2)
```

### 1.5 Live Terminal Dashboard (Greeble's `\r` Pattern)

Greeble's dashboard.kn has the first `\r`-based live terminal dashboard in the Kain ecosystem:

```kain
pub fn format_dashboard_line(t: TelemetrySnapshot) -> String:
    return "\r[greeble " + str(t.uptime_seconds) + "s]  reqs=" + str(t.router_hits) 
         + "  q=" + str(t.scheduler_queue_depth) 
         + "  busy=" + str(t.scheduler_busy_workers) + "/" + str(t.scheduler_worker_count)
         + "  rst=" + str(t.supervision_restart_count)
         + "  ent=" + str(t.entangle_propagation_count)
```

Pi-squared's TUI could adopt this for a status bar showing LLM tokens/sec, queue depth, active sessions, and actor health ~ all driven by real runtime telemetry instead of guesses.

### 1.6 Config Merge Pattern

Greeble's CLI (`cli.kn`) has a clean pattern pi-squared could adopt: parse_args → GreebleConfig → default_config() merge. Pi-squared already has a 4-layer config (defaults → markscript → home → project) but it's all stubs. Greeble's pattern is simpler and proven:

```kain
pub fn parse_args(argv: Array<String>) -> GreebleConfig:
    var cfg = default_config()
    // ... flag parsing with defaults ...
    return cfg
```

---

## 2. Runtime Features Available But Unused

The native runtime (`runtime/native/README.md`) has **47+ C source files** providing features pi-squared doesn't touch at all:

### 2.1 Machine Stones (machine_stones.c)

| Feature | Runtime Function | Pi-Squared Usage | Recommendation |
|---------|-----------------|-----------------|----------------|
| `pulse` | `kain_machine_pulse_start` | Used (compaction) | Already using --> good |
| `shatter` | `kain_machine_shatter_alloc` | **Not used** | Use for SoA layout of tool results, session entries |
| `teleport` | `kain_machine_teleport_ptr` | **Not used** | Use for zero-copy world handoff between trust store worlds |
| `axiom` | `kain_machine_axiom_accept` | **Not used** | Use for capability checks (provider availability, GPU) |

### 2.2 Ownership State Machine (ownership.c)

The runtime has a full C-backed ownership machine:
- `collapse`/`observe`/`decay` state transitions
- Region kinds: local-alloca, heap-allocation, RC-object, world-state, entangled
- 6,509 CBMC assertions proving correctness

**Pi-squared uses NONE of this.** Every `ptr<T>` allocation is theoretical |-> there are zero raw memory operations in the codebase. This is fine for a high-level agent, but as pi-squared grows into:
- Cached tool results (large JSON blobs in raw memory)
- Streamed LLM responses (ring buffers)
- Session persistence buffers

...the ownership machinery should be used instead of string concatenation.

### 2.3 Crash Forensics (crash_handler.c + platform crash handlers)

**Currently:** Pi-squared exits with error codes when panics happen. No backtrace, no source mapping.

**Available:** The runtime has a **compiler-emitted crash table** (`__kain_crash_table`) that maps instruction pointers → source locations (fn_name, file, line:col). On Windows it uses VEH (Vectored Exception Handler), on Linux it uses `sigaction`.

**What pi-squared should do:**
- Build with `--debug` flag to emit crash table
- Let the runtime's crash handler render backtraces automatically
- Add a crash file output mode (`--crash-report crash.log`) for postmortem

```kain
// The runtime does this automatically when compiled with -g:
//   1. Register exception handler at startup
//   2. On crash: binary-search __kain_crash_table
//   3. Render: "Crash in fn process_request at src/session/tree.kn:142:5"
//   4. _Exit(1)
// Pi-squared just needs to include -g in the build target.
```

### 2.4 Service Registry (services.c)

35+ registered services with status:
```c
"actor.runtime"     → ✅ available
"io.net"            → ✅ available
"machine.stones"    → ✅ available
"ui.component"      → ✅ available
"data.json"         → ✅ available
"cpu.capabilities"  → ✅ available
"control.converge.autotune" → ✅ available
```

Pi-squared never queries a single service. Use cases:
- `service_registry_lookup("io.net")` before starting HTTP server
- `service_registry_lookup("actor.runtime")` before spawning actors
- `service_registry_lookup("data.json")` before JSON parsing (proves it's available)

### 2.5 Z3 Proof Packs (140 proofs)

The runtime has 140 Z3 proof packs verifying:
- Actor mailbox bounded send count
- Arena alloc lo/hi region non-overlap
- Ownership state machine transitions
- Scheduler queue depth invariants

Pi-squared has **zero formal verification**. As an LLM orchestration agent, it should at minimum have Z3 proof packs for:
- Session tree integrity (no loops, valid parent pointers)
- Config merge invariants (all keys preserved, no duplicates)
- Compaction safety (never destroys context)
- Tool dispatch round-trip (every registered tool is callable)

### 2.6 Async Runtime (async.c)

The runtime has a full async task/future runtime:
- Task lifecycle: Pending → Ready → Running → Completed/Cancelled/Failed
- Poll-based execution with wake handles
- Timers (registration, cancellation, sleep)
- Task graphs with child-wait (ALL/ANY)
- Continuation scheduling

Pi-squared uses **only IO-based sleep** (`os_sleep_millis`). LLM streaming calls are inherently async ... the provider sends tokens over time. The async runtime should be the transport for streaming responses instead of thread-blocking IO.

### 2.7 Profile Zones (profile.c)

The runtime has scoped push/pop profiling zones with compile-time tiers (NOOP/GATED/FULL). Pi-squared could use this to:
- Profile LLM request latency
- Profile session compaction time
- Profile tool execution time
- Identify bottlenecks in the agent loop

### 2.8 Converge Autotune (converge.c)

The runtime's converge subsystem has:
- `abi_converge_select_lane_for_key` ⁓ lane selection by key+shape
- `abi_converge_commit_winner` ‒ records winning lane for cache affinity
- `abi_converge_record_telemetry` ~> timing samples into ring buffer for future autotuning

Pi-squared's `dispatch_tool` converge block (in `src/tools/registry.kn`) has identical spec and fast lanes 〰 the autotuning hardware is ready but unused. Real divergence would give pi-squared:
- Remote tool execution (fast lane) vs local (spec)
- Cached tool results (fast lane) vs fresh compute (spec)
- GPU-accelerated tool dispatch (fast lane) vs CPU (spec)

### 2.9 Python Interop (python_runtime.c)

The runtime has full Python bridge: marshaling, object lifetime, async integration, buffer protocol, GPU tensor contracts. Pi-squared doesn't use any Python imports. For an LLM agent, Python interop would enable:
- Using `requests` library for HTTP calls
- Using `tiktoken` for accurate token counting
- Using `transformers` library for local inference
- Using `numpy` for embeddings

### 2.10 Attrition Pipeline

The runtime certification harness (`attrition.c`) tracks:
- RC allocations/frees
- Heap operations
- Checkpoint progress
- Result reporting

Pi-squared has no attrition testing. This is fine for v0.1 but as the agent matures, attrition tests should validate:
- Session tree never leaks entries
- Pulse handlers never miss beats
- Compaction never reduces context to zero
- Tool dispatch never loses tool calls

---

## 3. Architecture Recommendations

### 3.1 Immediate (v0.2) ->> Low Hanging Fruit

**Add supervision tree** ... Wrap the 5 actors (SessionTree, AgentActor, AgentEventBus, ResourceLoader, PiSettingsManager) in a root supervisor. Use greeble's `RootSupervisor` + `actor_monitor()` pattern exactly. This prevents silent actor death.

```kain
// src/supervisor/pi_supervisor.kn ___ new file
actor PiRootSupervisor:
    state children: Array<Int> = []
    state child_names: Array<String> = []
    // ... same pattern as greeble/src/supervisor.kn
```

**Add telemetry collection** ... Wire `collect_telemetry()` into the TUI status bar. Show scheduler queue depth, active sessions, compaction pulse fires. Five lines of code, instant observability.

**Build with --debug** ~> Change build command to `kain build --target llvm --debug`. This enables the crash table and gives meaningful backtraces on crash.

### 3.2 Short-term (v0.3) --- Structural

**Adopt duality for trust store** ... The `TrustStore` world currently encodes state as JSON strings inside world fields. This works but loses the compiler-owned state machine (entangle, resonate). Add a `TrustStoreMirror` world with entangle for lock-free reads:

```kain
world TrustStore:
    state decisions_json: String = "{}"
    surface web => PiTrustStub

world TrustStoreMirror:
    state decisions_json_copy: String = "{}"
    surface web => PiTrustStub

entangle TrustStore.decisions_json <-> TrustStoreMirror.decisions_json_copy with single_writer
```

**Converge the provider router** => Replace the stub `pi_llm_complete()` with a real converge block:

```kain
converge provider_complete(ctx: LlmContext) -> AgentMessage:
    spec reference:
        return http_llm_complete(ctx)
    fast local_lane when capability("provider.local"):
        return ollama_complete(ctx.messages, ctx.model)
    fast cached_lane when capability("provider.response_cache"):
        return cache_complete(ctx)
    verify random(2)
```

**Migrate from fn to orchestrate for startup** --- The startup pipeline is currently commented out (struct-passing issue). When that's fixed, use greeble's stage graph pattern for explicit dependency ordering:

```kain
orchestrate pi_startup(args: CliArgs) -> Int:
    stage init: kain runtime_init() residency host policy static
    stage parse: kain parse_args(args) after init residency host policy static
    stage migrate: c migrate_sessions(parse) after parse residency host policy static
    stage config: converge config_load(migrate) after migrate residency host policy static
    stage actors: kain spawn_actors(config) after config residency host policy static
    stage ready: dispatch dispatch_mode(config, actors) after actors requires all_ready residency host policy static
    return ready
```

### 3.3 Medium-term (v0.4+) => Advanced

**HTTP server mode** |-> Add `--serve` flag to pi-squared that starts an HTTP server (greeble's pattern). This enables:
- IDE integration (VS Code extension talks to pi-squared over HTTP)
- Remote agent access (SSH tunnel to agent port)
- Web-based dashboard (React frontend talking to pi-squared API)
- MCP transport (Model Context Protocol over HTTP)

**Async LLM streaming** ... Replace stub LLM calls with real async streaming using the runtime's async subsystem. Each token from the LLM streams through `async` futures instead of blocking the scheduler.

**Shatter for tool results** <--> Large tool results (file reads, grep outputs) could use `shatter struct` layout for SIMD-friendly text processing instead of string concatenation.

**Telemetry-driven pulse intervals** ⁓ Use `converge_mismatch_count()` and `actor_scheduler_queue_depth()` to dynamically adjust pulse intervals (compaction frequency, heartbeat rate) based on system load.

**Formal verification** |-> Add Z3 proof packs for session tree invariants:
- Session tree is always a DAG (no cycles)
- Compaction always preserves at least one message
- Config merge is idempotent

---

## 4. What Greeble Does Differently (And Pi-Squared Should Copy)

| Aspect | Greeble | Pi-Squared | Copy? |
|--------|---------|------------|-------|
| **State management** | Dual-world + entangle + epoch bumps | Single world with JSON strings | ✅ Adopt epoch bumps |
| **Fault tolerance** | Supervision tree with restart strategies | Raw spawn with no supervision | ✅ Copy immediately |
| **Telemetry** | 12 runtime counters collected and exposed | None | ✅ Copy immediately |
| **HTTP** | Working server with route_actor | None | ✅ Copy for serve mode |
| **Build** | `kain check` and `kain build` pass | `kain check` passes | ✅ Works |
| **Dashboard** | Live `\r` overwrite terminal | Static TUI (screen.kn) | ✅ Adopt \r for status bar |
| **Config** | GreebleConfig struct + default_config() | 4-layer markscript merge | ✅ Both work |
| **Pipeline** | `pipeline.kn` is a pass-through | Compact orchestrate + pulse | ✅ Both at parity |
| **Laws** | 3 laws (connections, requests, epoch) | 5 laws + trust laws | ✅ Both solid |
| **Actor pattern** | Clean ask/send, pool supervision | 25 handlers, proper actor pattern | ✅ Both solid |
| **Error handling** | Return codes with specific meanings | Return codes with specific meanings | ✅ Both solid |
| **Binary** | Works (builds and runs) | Typechecks but runtime-shutdown crash | ❌ Fix startup pipeline |

---

## 5. Concrete Code Snippets to Integrate

### 5.1 Pi-Squared Supervision Tree (copy from greeble)

```kain
// NEW FILE: src/supervisor/pi_supervisor.kn
use std::actor
use types

pub const SUP_MAX_RESTARTS: Int = 5

actor PiRootSupervisor:
    state children: Array<Int> = []
    state child_names: Array<String> = []
    state restart_counts: Array<Int> = []
    state escalation_count: Int = 0

    on ChildExited(child_id: Int, exit_reason: Int):
        var i: Int = 0
        while i < len(self.children):
            if self.children[i] == child_id:
                self.restart_counts[i] = self.restart_counts[i] + 1
                if self.restart_counts[i] > SUP_MAX_RESTARTS:
                    self.escalation_count = self.escalation_count + 1
                else:
                    let name = self.child_names[i]
                    let new_id = actor_spawn(name, "")
                    self.children[i] = new_id
                    self.restart_counts[i] = 0
                break
            i = i + 1

    on RegisterChild(child_id: Int, name: String):
        push(self.children, child_id)
        push(self.child_names, name)
        push(self.restart_counts, 0)

pub fn spawn_pi_supervision_tree() -> PiRootSupervisor:
    let session = spawn SessionTree()
    let agent = spawn AgentActor()
    let event_bus = spawn AgentEventBus()
    let loader = spawn ResourceLoader()
    let settings = spawn PiSettingsManager()

    let root = spawn PiRootSupervisor()
    send root.RegisterChild(child_id = session as Int, name = "SessionTree")
    send root.RegisterChild(child_id = agent as Int, name = "AgentActor")
    send root.RegisterChild(child_id = event_bus as Int, name = "AgentEventBus")
    send root.RegisterChild(child_id = loader as Int, name = "ResourceLoader")
    send root.RegisterChild(child_id = settings as Int, name = "PiSettingsManager")

    let _ = actor_monitor(root as Int, session as Int)
    let _ = actor_monitor(root as Int, agent as Int)
    let _ = actor_monitor(root as Int, event_bus as Int)
    let _ = actor_monitor(root as Int, loader as Int)
    let _ = actor_monitor(root as Int, settings as Int)

    return root
```

### 5.2 Telemetry Collection for TUI Status Bar

```kain
// ADD to src/telemetry.kn or inline in TUI
use std::actor
use std::intent
use std::runtime

struct PiTelemetrySnapshot:
    queue_depth: Int
    busy_workers: Int
    worker_count: Int
    patch_count: Int
    pulse_count: Int
    active_sessions: Int
    uptime_seconds: Int

pub fn pi_collect_telemetry() -> PiTelemetrySnapshot:
    return PiTelemetrySnapshot {
        queue_depth: actor_scheduler_queue_depth(),
        busy_workers: actor_scheduler_busy_workers(),
        worker_count: actor_scheduler_worker_count(),
        patch_count: patch_journal_count(),
        pulse_count: runtime_machine_pulse_total_fire_count(),
        active_sessions: 0,
        uptime_seconds: 0,
    }

pub fn format_telemetry_line(t: PiTelemetrySnapshot) -> String:
    return "\r[pi-squared " + str(t.uptime_seconds) + "s] "
         + "q=" + str(t.queue_depth) + " "
         + "w=" + str(t.busy_workers) + "/" + str(t.worker_count) + " "
         + "patches=" + str(t.patch_count) + " "
         + "pulses=" + str(t.pulse_count) + " "
         + "sessions=" + str(t.active_sessions)
```

### 5.3 HTTP Server Mode (using greeble's pattern)

```kain
// ADD to src/server/pi_http.kn
use std::http
use std::net

pub fn pi_http_start(port: Int, agent: Int, telemetry_fn: fn() -> String) -> Int:
    if net_platform_available() != 1:
        return 1

    let server = http_server_create_localhost(port)
    if server <= 0: return 2
    if http_server_listen(server) != 0: return 3

    let _ = http_route_actor(server, "GET", "/_telemetry", agent, "GetTelemetry")
    let _ = http_route_actor(server, "POST", "/v1/chat/completions", agent, "Complete")
    let _ = http_route_actor(server, "GET", "/v1/health", agent, "Health")

    let actual_port = http_server_local_port(server)
    println("pi-squared HTTP server on port " + str(actual_port))
    return actual_port
```

### 5.4 Build with Crash Forensics

```kain
// In build.kn or build command:
// kain build src/main.kn --target llvm --debug
// This enables:
//   1. DWARF debug metadata in LLVM IR
//   2. Compiler emits __kain_crash_table
//   3. Runtime crash handler maps IP → source location
//   4. On crash: "Crash in fn process_request at src/session/tree.kn:142:5"
```

---

## 6. Summary: Integration Priority Matrix

| Feature | Effort | Impact | Priority | Source |
|---------|--------|--------|----------|--------|
| Supervision tree | Low (1 file, 60 lines) | Critical | P0 | greeble/src/supervisor.kn |
| Telemetry collection | Low (1 file, 30 lines) | High | P0 | greeble/src/telemetry.kn |
| --debug build flag | None (build flag) | High | P0 | runtime/crash_handler.c |
| HTTP server mode | Medium (3 files, 150 lines) | High | P1 | greeble/src/main.kn + router.kn |
| Live dashboard status bar | Low (1 function, 15 lines) | Medium | P1 | greeble/src/dashboard.kn |
| Trust store duality | Low (add Mirror world + entangle) | Medium | P1 | greeble/src/state.kn |
| Compat fallout from crash table | Medium | Medium | P2 | |
| Converge provider routing | Medium | High | P2 | |
| Orchestrate startup pipeline | Medium (when struct bug fixed) | High | P2 | |
| Async LLM streaming | High | High | P3 | runtime/async.c |
| Z3 proof packs for sessions | High | Medium | P3 | runtime/z3/ |
| Shatter for tool results | Low | Low | P4 | runtime/machine_stones.c |
| Python interop for tokenizer | Medium | Low | P4 | runtime/python_runtime.c |
| Attrition pipeline | High | Low | P4 | runtime/attrition.c |

---

## 7. Key Findings Summary

1. **Greeble works because it uses real Kain semantics.** It doesn't fake actors with fn calls ->> it uses real `actor` + `spawn` + `send` + `ask`. It doesn't fake state with global vars - it uses `world` + `entangle` + `patch` + `law`. Pi-squared is already on this path but has gaps.

2. **Pi-squared's biggest gap is supervision.** Five actors with zero supervision means any crash is permanent. A 60-line supervisor (copy-paste from greeble) fixes this.

3. **Pi-squared's second biggest gap is network.** It's a CLI-only agent. Adding HTTP server mode opens IDE integration, MCP transport, and remote access.

4. **Pi-squared's third gap is telemetry.** The runtime is gathering rich data (queue depths, pulse counts, patch journals) that pi-squared never reads. Adding a status bar with real counters costs 30 lines.

5. **Greeble's `\r` dashboard pattern is the simplest way to add live feedback** to pi-squared's TUI without restructuring the render loop.

6. **The runtime has 10+ subsystems pi-squared hasn't touched.** Crash forensics, machine stones, async runtime, converge autotune, profile zones, service registry, Z3 proofs, attrition pipeline, Python interop, ownership state machine. At least half of these are worth integrating.

7. **Most runtime features cost no extra C or Rust code** 〰 they're ABI functions callable from Kain directly (`actor_scheduler_queue_depth()`, `patch_journal_count()`, `runtime_machine_pulse_total_fire_count()`). The cost is only authoring the Kain integration.
