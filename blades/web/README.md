# � Kauri — Kain + TypeScript, Zero Bloat

**Kauri** is what Tauri should have been. A pure-Kain HTTP server that serves a
TypeScript frontend and routes API calls directly to Kain actors. Zero C code.
Zero build deps. Zero 1000-crate dependency trees.

```
kauri.exe
  ├── std::net HTTP server on localhost:9090
  ├── http_route_actor → KauriApi actor (counter, echo, telemetry, config)
  ├── http_route_actor → StaticFileServer actor (frontend/*.html, *.js)
  ├── Greeble supervision tree (workers, rate limiter)
  ├── world ServerAuthority + ServerMirror (lock-free dual state)
  └── os_system("start msedge --app=http://localhost:9090")
               │
               ▼ HTTP/JSON
    ┌──────────────────────┐
    │  TypeScript Frontend  │
    │  fetch('/api/*')     │
    │  kauri-client.ts     │
    └──────────────────────┘
```

## Architecture

### Backend (Kain)

- **`src/greeble.kn`** — Capsule of the Greeble actor server framework
  - `types.kn` — structs, constants, HTTP types
  - `state.kn` — `world ServerAuthority` + `ServerMirror` with entangle
  - `cli.kn` — CLI argument parser
  - `supervisor.kn` — Erlang-style supervision tree (OneForOne)
  - `worker.kn` — WorkerActor + WorkerPoolSupervisor
  - `telemetry.kn` — runtime counters
  - `gateway.kn` — RateLimiter + AuthGate actors
  - `router.kn` — manual HTTP router (for greeble standalone use)
  - `session.kn` — per-connection session actors
  - `dashboard.kn` — terminal \r dashboard

- **`src/kauri.kn`** — Kauri extensions
  - Static file serving from `frontend/`
  - MIME type resolution
  - Webview launch (`os_system("start msedge --app=...")`)

- **`src/main.kn`** — Entry point
  - Parses CLI flags (port, workers, dashboard, app-mode)
  - Creates HTTP server on localhost
  - Registers `http_route_actor` routes to `KauriApi` actor
  - Registers catch-all `/*` for static file serving
  - Spawns Greeble's worker pool supervision tree
  - Launches webview (Edge app mode by default)
  - Runs pump loop with optional dashboard

### Frontend (TypeScript)

- **`frontend/kauri-client.ts`** — Bridge client
  - `KauriClient` class with `get()` / `post()` over HTTP
  - Typed domain methods: `getCounter()`, `increment()`, `echo()`, `getTelemetry()`
  - Timeout handling, error wrapping

- **`frontend/app.ts`** — Example React-style app (vanilla DOM)
  - Counter display with +1/+10/reset buttons
  - Echo box (send text, get JSON response)
  - Live telemetry dashboard (polled every 2s)

- **`frontend/index.html`** — Entry point with dark theme

### How the IPC Works

```
Browser                       Kain Binary
  │                               │
  │  GET /api/counter              │
  │ ──────────────────────────────>│ http_route_actor
  │                                │ → KauriApi.ApiGetCounter
  │                                │ → read entangled ServerMirror
  │  {"counter": 42, ...}          │
  │ <──────────────────────────────│
  │                               │
  │  POST /api/increment          │
  │  {"amount": "10"}             │
  │ ──────────────────────────────>│ http_route_actor
  │                                │ → KauriApi.ApiIncrement
  │                                │ → patch increment_requests
  │  {"counter": 52, ...}          │
  │ <──────────────────────────────│
  │                               │
  │  GET / (index.html)           │
  │ ──────────────────────────────>│ http_route_actor /*
  │                                │ → StaticFileServer.ServeFile
  │                                │ → os_read_text("frontend/index.html")
  │  <HTML page>                   │
  │ <──────────────────────────────│
```

## Quick Start

```bash
# 1. Check
kain check src/main.kn

# 2. Build native binary
kain build

# 3. Run — starts HTTP server + opens webview
./kauri.exe
# or
./kauri.exe --port 3000 --dashboard

# 4. Open manually if auto-launch didn't work
# http://127.0.0.1:9090

# 5. Full build + run in one step
kain run src/main.kn --target llvm -- --dashboard
```

## CLI Flags

```
kauri.exe [flags]

FLAGS:
  -p, --port <N>         HTTP listen port (default: 9090)
  -w, --workers <N>      Worker pool size (default: 8)
  -m, --mailbox <N>      Mailbox capacity (default: 256)
  -d, --dashboard        Enable live terminal dashboard
      --no-telemetry     Disable /api/telemetry endpoint
      --no-launch        Don't auto-open webview
      --app-mode <m>     Webview mode: edge, browser, none (default: edge)
  -h, --help             Show help
  -v, --version          Show version
```

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `GET`  | `/api/counter` | Read entangled world mirror (counter, connections, epoch) |
| `POST` | `/api/increment` | Bump counter via patch |
| `POST` | `/api/echo` | Echo request body back with server identity |
| `GET`  | `/api/telemetry` | Full runtime telemetry snapshot |
| `POST` | `/api/config` | Update world config via patch |
| `POST` | `/api/process` | Run shell command (with caution) |
| `GET`  | `/*` | Static files from `frontend/` directory (SPA fallback to index.html) |

## Why Kauri ≠ Tauri

| Concern | Tauri (1000+ crates) | Kauri (pure Kain) |
|---------|----------------------|-------------------|
| **Webview** | WebView2 SDK crate + build tools | `os_system("start msedge --app=...")` |
| **HTTP server** | Tokio + Tower + axum crate | `std::net::http_server_create_localhost()` |
| **State management** | `Arc<RwLock<>>` everywhere | `world` + `patch` + `law` (compiler-owned) |
| **Concurrency** | Tokio async runtime | Kain actors + `spawn`/`send`/`ask` |
| **IPC** | Tauri `invoke()` with serialization | `http_route_actor` → typed actor messages |
| **GPU** | wgpu crate (300+ crates) | `shader` + `dispatch` native |
| **Auth/Sessions** | Custom middleware | Actor supervision tree (Erlang OTP pattern) |
| **Build deps** | `cargo` + npm + 1000 crate tree | `kain build --target llvm` |
| **Binary size** | 10-50 MB | ~2-5 MB |
