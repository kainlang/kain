# Greeble: Erlang-Style Actor Server Framework

**Greeble** is a portable, pure-Kain Erlang/OTP-style actor server framework. It compresses what would be thousands of lines of Erlang/OTP, Akka, or Elixir/Phoenix into a small set of Kain source files by letting the language own the state, dispatch, timing, coupling, and pipeline semantics that traditional frameworks implement as manual boilerplate.

Like [three-kn](https://github.com/kain-lang/kain/tree/master/blades/three-kn) for 3D rendering, greeble is for actor servers — a template you clone, a framework you import, a reference architecture you study.

---

## Quick Start

```bash
# Build
kain build

# Run with defaults (port 8080, 8 workers)
./greeble.exe

# Dev mode with live terminal dashboard
./greeble.exe --port 3000 --dashboard

# Production tuning
./greeble.exe --workers 32 --mailbox 512 --no-telemetry

# Help
./greeble.exe --help
```

---

## CLI Reference

```
greeble.exe [flags]

FLAGS:
  -p, --port <N>         HTTP listen port (default: 8080)
  -w, --workers <N>      Worker pool size (default: 8)
  -m, --mailbox <N>      Mailbox capacity, 0=unbounded (default: 256)
  -d, --dashboard        Enable live terminal dashboard
      --dashboard-ms <N> Dashboard tick interval in ms (default: 1000)
      --no-telemetry     Disable /_telemetry JSON endpoint
  -h, --help             Show help and exit
  -v, --version          Show version and exit

EXAMPLES:
  greeble.exe                                    # defaults
  greeble.exe --port 3000 --dashboard            # dev mode with ticker
  greeble.exe --workers 32 --mailbox 512         # tuned for load
  greeble.exe --port 8080 --no-telemetry         # minimal production
```

---

## Architecture

The framework is organized as 11 flat source files, each mapped to a layer of the [Kain decision ladder](docs/RULEBOOK.md):

```
src/
├── main.kn           Entry point — CLI, bootstrap, pump loop, shutdown
├── types.kn          L0 — shared structs, constants, pack/unpack
├── cli.kn            L0 — CLI argument parsing → GreebleConfig
├── state.kn          L1+L2 — dual-world pattern, laws, patches
├── telemetry.kn      L0 — runtime telemetry collection, JSON export
├── dashboard.kn      L5 — live terminal dashboard (pulse + \r in-place)
├── gateway.kn        L7 — RateLimiter + AuthGate actors
├── worker.kn         L7 — WorkerActor + WorkerPoolSupervisor
├── session.kn        L7 — SessionActor + link/monitor lifecycle
├── router.kn         L7 — RouterActor with HTTP parsing + route dispatch
├── supervisor.kn     L7 — RootSupervisor + GatewaySupervisor
└── pipeline.kn       L0 — request pipeline passthrough (orchestrate in v0.2)
```

### Decision Ladder Usage

| Layer | Construct | Where | Why |
|-------|-----------|-------|-----|
| **L0** | `struct`, `const`, `fn` | `types.kn`, `cli.kn`, `telemetry.kn` | Pure data, config, parsing |
| **L1** | `world`, `entangle` | `state.kn` | Compiler-owned global state with lock-free mirror reads |
| **L2** | `law`, `patch` | `state.kn` | Invariant enforcement + journaled mutation |
| **L5** | `pulse` | `dashboard.kn` | Compiler-owned periodic beat for live terminal ticker |
| **L7** | `actor`, `spawn`, `send`, `ask` | `gateway.kn`, `worker.kn`, `session.kn`, `router.kn`, `supervisor.kn` | Isolated concurrent state machines with typed message contracts |

### Server Startup Flow

```
main()
  ├── parse_args(argv) → GreebleConfig
  ├── runtime_init() + net_reset()
  ├── greeble_start(cfg)
  │     ├── http_server_create_localhost(cfg.port)
  │     ├── spawn_supervision_tree(cfg)
  │     │     ├── spawn RouterActor
  │     │     ├── spawn RateLimiter
  │     │     ├── spawn AuthGate
  │     │     ├── spawn WorkerPoolSupervisor + N WorkerActors
  │     │     └── spawn RootSupervisor (OneForAll)
  │     └── Print startup banner
  └── greeble_run(cfg)
        └── while true: dashboard pulse + sleep
```

### Request Flow

```
HTTP Request
  → http_route_actor() dispatches to RouterActor
  → RouterActor parses HTTP, matches route table
  → actor_send(handler_id, message_kind, body)
  → Handler actor processes, optionally reads ServerMirror (lock-free)
  → Handler optionally triggers patch on ServerAuthority
  → Response returned through HTTP server
```

---

## Key Features

### Supervision Tree

```kn
RootSupervisor (OneForAll, Permanent)
  ├── RouterActor        — HTTP routing, Io lane
  ├── RateLimiter        — sliding window, bounded mailbox
  ├── AuthGate           — token validation, Transient restart
  └── WorkerPoolSupervisor (SimpleOneForOne)
        ├── WorkerActor₁
        ├── WorkerActor₂
        └── ... (N workers, Cpu lane)
```

- **OneForAll:** If any core service crashes, restart all. Escalates after 5 restarts in 60s.
- **OneForOne:** GatewaySupervisor restarts only the failed child.
- **SimpleOneForOne:** Worker pool restarts only the failed worker instance.

### Dual-World Global State

Workers read global server state **lock-free** through the Mirror world. Mutations are journaled through law-guarded patches on the Authority world. Zero-copy propagation via entangle.

```kn
world ServerAuthority:          // Owns mutable state
    state total_requests: Int = 0
    state epoch: Int = 0

world ServerMirror:             // Lock-free reads for all workers
    state total_requests_copy: Int = 0

entangle ServerAuthority.total_requests <-> ServerMirror.total_requests_copy
    with single_writer
```

### Live Terminal Dashboard (opt-in)

```
[greeble 1s]   reqs=0     q=0  busy=0/4  rst=0  ent=0
[greeble 2s]   reqs=42    q=1  busy=2/4  rst=0  ent=17
[greeble 3s]   reqs=89    q=0  busy=3/4  rst=0  ent=36
```

Uses `pulse` (L5 Temporal) with `\r` carriage-return in-place overwrite. The first `\r`-based live dashboard in the Kain ecosystem. Enable with `--dashboard`.

### Telemetry Endpoint

`GET /_telemetry` returns JSON with scheduler queue depth, busy workers, supervision restarts, patch journal count, entangle propagation count, and more. Disable with `--no-telemetry`.

---

## Build & Run

```bash
# Typecheck
kain check src/main.kn

# Build native executable
kain build

# Run
./greeble.exe --port 8080

# Run with dashboard
./greeble.exe --port 3000 --dashboard

# Full build + run (one step)
kain run src/main.kn --target llvm -- --port 8080 --dashboard
```

Binary output: `.kain/out/x86_64-windows/dev/ll/main/compile/main.exe`

---

## Project Structure

```
blades/network/greeble/
├── build.kn              Build authority (Kain project DSL)
├── README.md             This file
├── spec/                 Spec-driven development docs
│   ├── 01-requirements.md
│   ├── 02-design.md
│   ├── 03-tasks.md
│   └── 04-dashboard-research.md
├── reference/            Reference material for implementation
│   ├── ACTOR.MD
│   ├── core_actor.kn
│   ├── http_server.kn
│   ├── http_server_frameworks.kn
│   ├── quantumerlang.kn
│   ├── researchfromscholar.md
│   └── serverrrr.kn
└── src/                  11 Kain source files
    ├── main.kn
    ├── types.kn
    ├── cli.kn
    ├── state.kn
    ├── telemetry.kn
    ├── dashboard.kn
    ├── gateway.kn
    ├── worker.kn
    ├── session.kn
    ├── router.kn
    ├── supervisor.kn
    └── pipeline.kn
```

---

## Kain Constructs Used

| Construct | Count | Files |
|-----------|-------|-------|
| `actor` | 9 | gateway, worker, session, router, supervisor |
| `world` | 2 | state |
| `entangle` | 5 | state |
| `law` | 3 | state |
| `patch` | 5 | state |
| `struct` | 6 | types |
| `fn` | 30+ | all files |
| `pulse` | 1 (commented) | dashboard |
| `component` | 1 (stub) | state |

---

## Status

v0.1 — Foundation phase. All 11 source files pass `kain check` and `kain build --target llvm`. The semantic architecture — worlds, laws, patches, actors, supervision tree, dual-world state, and dashboard pulse — is fully specified and typechecked. The HTTP server boots, actors spawn and communicate, and the telemetry pipeline collects runtime counters.

**Next (v0.2):** Full HTTP pump loop with `server_pump`, orchestrate request-processing DAG, component-based admin dashboard, and CBMC/Z3 verification of supervision invariants.

---

## License

Part of the [Kain](https://kain-lang.org) blade ecosystem.
