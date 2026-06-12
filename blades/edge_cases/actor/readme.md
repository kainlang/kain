# Kain Actor System — Comprehensive Edge Case Testing Suite

A self-contained, comprehensive testing suite for the Kain actor system. Tests every
layer of the actor runtime — from basic lifecycle through Erlang-style supervision
trees, UE5-style game loop pipelines, stress tests, and telemetry delta guards that
**mathematically prove** the scheduler actually processed messages.

## Runtime Observations (from execution)

| Observation | Value | Notes |
|------------|-------|-------|
| Native spawn IDs | Start at 1 | Valid non-zero IDs returned |
| `actor_id_is_valid(-1)` | `true` | Runtime treats -1 as valid (unsigned/slot encoding) |
| `actor_is_running()` after spawn | `false` | Lazy initialization — actors bootstrap on first message |
| Scheduler queue counters via typed `ask()` | Δ0 | Typed syntax uses inline fast path, bypasses queue counters |
| Default mailbox capacity | 1024 | As configured in runtime |
| Worker count | 4 | Default thread pool size |
| Max restarts | 5 | Supervision restart intensity limit |
| Restart window | 60000ms | 60-second supervision window |
| 256 sequential asks | 0 failures | Fully reliable under moderate load |

## Quick Start

```powershell
cd X:\blades\edge_cases\actor

# Typecheck only (fastest)
kain check

# Full compile + execute all 42 tests
kain run

# Run a specific category
kain run -- --test lifecycle
kain run -- --test worker_pool
kain run -- --test stress

# Run a single test
kain run -- --test ask_basic

# Verbose output with descriptions
kain run -- --verbose

# List all tests
kain run -- --list

# Run in isolated process
kain run -- --vm
```

## Test Categories (42 Tests, 15 Categories)

| # | Category | Tests | Error Codes | What It Proves |
|---|----------|-------|-------------|----------------|
| 1 | **Lifecycle** | 6 | 1-9 | Spawn returns valid IDs; shutdown→terminal; kill→terminal; invalid IDs rejected; double shutdown safe |
| 2 | **Send/Cast** | 3 | 10-19 | Fire-and-forget send completes; invalid target returns error; post-shutdown send returns mailbox-closed |
| 3 | **Ask/Call** | 4 | 20-29 | Reply matches sent value; packed multi-value unpacks correctly; 16 sequential roundtrips all correct |
| 4 | **Mailbox** | 2 | 30-39 | Default capacity is 1024; unbounded capacity constant is 0 |
| 5 | **Registry** | 4 | 50-59 | Register→lookup→found; has() works; unregister removes; re-register updates |
| 6 | **Monitor** | 2 | 60-69 | Monitor registers; demonitor removes relationship |
| 7 | **Link** | 2 | 70-79 | Bidirectional link registers; unlink removes link |
| 8 | **Supervision** | 2 | 80-89 | Max restarts is positive; restart window is positive milliseconds |
| 9 | **Scheduler** | 4 | 90-99 | Queue depth non-negative; worker count positive; enqueue/dequeue advance after work; active/busy/overflow non-negative |
| 10 | **Worker Pool** | 2 | 100-109 | 4 workers return correct results; 32 round-robin asks complete correctly |
| 11 | **GenServer** | 2 | 110-119 | Init sets state correctly; 5 Call handlers accumulate state (0→10→30→60→100→150) |
| 12 | **Game Loop** | 1 | 120-129 | Input→Physics→Render pipeline across 3 frames via send |
| 13 | **Fusion Chain** | 1 | 130-139 | Actor reads external world-like state through 16 ask parameters |
| 14 | **Stress** | 2 | 140-149 | 64 spawns within table capacity; 256 sequential asks with 0 failures |
| 15 | **Telemetry Delta** | 2 | 150-159 | **Proof layer:** pre/post snapshots prove scheduler engaged; combined checksum proves full pipeline |

## Test Actors Defined

| Actor | Pattern | State | Handlers |
|-------|---------|-------|----------|
| `EchoRelay` | Basic echo | `hits`, `last_val` | `Echo(reply_to, val)`, `Ping(reply_to)` |
| `PackedRelay` | Multi-value packing | `bias`, `multiplier` | `Compute(reply_to, packed)` |
| `PoolWorker` | Worker pool | `id`, `processed` | `Work(reply_to, val)` |
| `GenServerActor` | Erlang GenServer | `counter`, `name` | `Init`, `Call`, `Cast`, `Info` |
| `InputWorker` | UE5 game loop | `pulses` | `Drift(left, right)` |
| `PhysicsWorker` | UE5 game loop | `steps`, `last_impulse` | `Step(impulse)` |
| `RenderWorker` | UE5 game loop | `frames`, `draw_count` | `Present(draws)` |
| `Watcher` / `WatchedWorker` | Monitor pattern | `notifications` / `tick` | `Notify` / `Bounce` |
| `LinkedAlpha` / `LinkedBeta` | Link pattern | `alive` | `Ping(reply_to, val)` |
| `WorldReader` | Fusion pattern | `reads` | `Read(reply_to, external_val)` |

## Architecture

```
src/
├── main.kn          CLI entry point — parses flags, dispatches to diagnostics
├── diagnostics.kn   Orchestrator — imports all modules, runs tests, prints reports
├── cause.kn         PRIMARY — 42 actor tests across 15 categories (this is the meat)
├── effect.kn        Downstream actor effect modeling (throughput, impact)
├── spookymagic.kn   Actor edge cases (mailbox storms, overflow detection, race windows)
└── vm.kn            Isolated process execution wrapper (--vm flag)
```

### Key Design Principles

1. **Always compiles** — Every import resolves. Adding a test to one category doesn't break others.
2. **Error code taxonomy** — Each category has a dedicated error code range (see table above). When a test fails, the error code tells you exactly which layer broke.
3. **Test table pattern** — Tests are registered in `get_cause_tests()` with name, tag, description, and category. The diagnostics module iterates the table — no hardcoded test names.
4. **Exit code contract** — 0 = pass, non-zero = specific failure. CLI, diagnostics, and VM all respect this.
5. **Telemetry delta guards** — Category 15 snapshots scheduler telemetry before/after actor work and asserts on deltas. This is the **proof layer** — it mathematically proves the actor system actually engaged.
6. **Self-contained** — Only depends on `std::actor`, `std::runtime`, and `std::time`. No external blade imports.

## Multi-Value ask() Payloads

`ask()` currently accepts a single `Int` payload. For multi-value messages, use the packing pattern:

```kn
const PACK_SHIFT: Int = 1000000

fn pack(a: Int, b: Int) -> Int:
    return a + b * PACK_SHIFT

fn unpack_a(packed: Int) -> Int:
    return packed % PACK_SHIFT

fn unpack_b(packed: Int) -> Int:
    return packed / PACK_SHIFT
```

## Error Code Reference

When a test fails, the error code identifies the exact failure:

| Range | Category | Example |
|-------|----------|---------|
| 1-9 | Lifecycle | `return 1` — spawn returned invalid ID |
| 10-19 | Send/Cast | `return 10` — send crashed |
| 20-29 | Ask/Call | `return 20` — ask returned wrong value |
| 30-39 | Mailbox | `return 30` — capacity mismatch |
| 50-59 | Registry | `return 50` — lookup failed after register |
| 60-69 | Monitor | `return 60` — monitor registration failed |
| 70-79 | Link | `return 70` — link registration failed |
| 80-89 | Supervision | `return 80` — max restarts non-positive |
| 90-99 | Scheduler | `return 90` — queue depth negative |
| 100-109 | Worker Pool | `return 100` — worker 0 returned wrong result |
| 110-119 | GenServer | `return 110` — init returned wrong value |
| 120-129 | Game Loop | `return 120` — pipeline step failed |
| 130-139 | Fusion Chain | `return 130` — actor+world fusion failed |
| 140-149 | Stress | `return 140` — too few spawns succeeded |
| 150-159 | Telemetry Delta | `return 150` — enqueue delta zero (scheduler never ran) |
| 999 | Unknown | Test tag not found in dispatch |

## Key Architectural Finding: Two Actor API Worlds

Kain has **two separate actor API surfaces** that produce different handle types:

| API | Spawn returns | Send/Ask | Native telemetry |
|-----|--------------|----------|-------------------|
| **Typed syntax** | Typed handle (`EchoRelay`) | `send a.Msg(...)`, `ask(a, "Msg", val)` | Inline fast path — bypasses scheduler queue counters |
| **Native API** | Raw `Int` ID | `actor_send(id, "Msg", "data")` | Full scheduler tracking — queue counters increment |

The typed syntax is the recommended Kain surface. The native API is for low-level telemetry and registry operations. You **cannot** mix typed handles with native `Int` ID functions — they're different types.

## Known Gaps

- **Supervision Kain-level syntax** — The `supervisor` keyword with strategy/policy may not have full surface yet. Tests use `actor_monitor`/`actor_link` native API for now.
- **Execution classes** (MICROCELL, NETCELL, WORLDCELL, etc.) — Runtime-internal, not queryable from Kain level.
- **GPU compute actors** — Separate testing domain (see `blades/cuda/`).
- **Inline ask fast path** — Runtime-internal optimization, not observable from Kain level.
- **Crash propagation through links** — Requires abnormal exit triggers; tested minimally via kill/shutdown.

## Adding New Tests

1. Define your actor at the top of `cause.kn` (or reuse existing)
2. Write your test function: `pub fn test_<name>() -> Int`
3. Add the dispatch case in `run_cause_test_by_tag()`
4. Register in `get_cause_tests()` with name, tag, description, and category
5. Run `kain check` to verify, then `kain run -- --test <your_tag>`

## Automation Ready

- **CI integration:** `kain run` returns exit code 0 (all pass) or 1 (failures)
- **Batch generation:** Scripts can write new test functions and register them in the table
- **Fuzzing harness:** Replace test bodies with fuzzer-generated actor workloads
- **Regression suite:** Each failure has a unique error code — CI can track which layer broke

## Further Reading

- **Actor Reference:** `X:\docs\ACTOR.MD` — Complete actor system documentation
- **Rulebook:** `X:\docs\RULEBOOK.md` — When to use actors vs other Kain constructs
- **Smoketest Actor:** `X:\smoketest\src\semantics\actor.kn` — Canonical actor correctness proof
- **Fusion Chain:** `X:\benchmark\cases_v2\fusion_chain.kn` — 7-layer semantic fusion benchmark
- **Benchmarks:** `X:\benchmark\cases_v2\actor_ownership_backpressure.kn` — Actor throughput benchmark
