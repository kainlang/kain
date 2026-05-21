---
name: kain-net-system
description: Use when adding, changing, debugging, validating, or reviewing Kain's native networking pipeline, including crates/kain-net, stdlib/native/net.kn, runtime/native/include/kain_native_net_system.h, runtime/native/src/core/kain_native_net_system.c, io.net service-table wiring, TCP/HTTP conformance tests, actor route integration, and native_net_http fixtures.
---

# Kain Net System

## Start Here

- Work from `D:\Kain-Lang`.
- Read `ARCHITECTURE.md` and `MEMORY.md` before changing network runtime behavior.
- Keep `crates/kain-net` as the portable contract crate, `runtime/native/include/kain_native_net_system.h` as the raw C ABI, `runtime/native/src/core/kain_native_net_system.c` as the host implementation, `stdlib/native/net.kn` plus `http.kn` / `tls.kn` / `http2.kn` as the native-profile wrappers, and root `stdlib/net.kn` / `http.kn` / `tls.kn` / `http2.kn` as the public `std.*` import surface.

## Architecture Rules

- Keep networking capability-shaped. Do not hardcode MCP, web app, game server, router, or RPC policy into the ABI.
- `io.net` exposes the Kain-owned native net function table. Do not route new Kain-facing network behavior through the older vendor/libuv placeholder.
- V1 is TCP plus protocol-aware HTTP. HTTPS client support is Windows-first through WinHTTP, the secure client lane can express/request HTTP/2 and report the negotiated response protocol, and capability state is queryable from Kain. Full server TLS, portable HTTP/2 sessions, WebSockets, HTTP/3, UDP, DNS policy, and connection pooling are still follow-up layers.
- Actor routes should dispatch request-handle metadata to actor messages, while manual request polling remains available for deterministic tests and low-level Kain code.
- Entangle is a higher layer for replicated state or distributed actors; do not bake entangle semantics into sockets or HTTP parsing.
- Reset/shutdown must close sockets, listeners, request handles, response handles, and server handles through `kain_native_net_reset()`.
- Keep `Content-Length` parsing strict and keep request/response body sizing on checked `size_t` arithmetic. Do not reintroduce signed parses, unchecked casts, or raw `header_length + body_length` / `body_length + 1` math in the HTTP lane.
- Successful `http_respond_*` calls must continue to destroy the consumed incoming request handle immediately. That auto-release is what fixed the old repeated-local-POST request-capacity failure.

## Key Files

- `crates/kain-net/src/lib.rs`: portable Rust contracts and typed errors, including HTTP protocol preference, TLS client spec, and capability-state metadata.
- `runtime/native/include/kain_native_net_system.h`: exported ABI and `KainNativeNetFunctionTable`.
- `runtime/native/src/core/kain_native_net_system.c`: TCP sockets, HTTP/1.1 parsing/responding, raw HTTP client, and WinHTTP HTTPS client.
- `runtime/native/src/core/kain_runtime_services.c`: `io.net` service descriptor and function-table pointer.
- `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`: include the net C source and required Windows libraries.
- `stdlib/native/net.kn`, `stdlib/native/http.kn`, `stdlib/native/tls.kn`, `stdlib/native/http2.kn`: native-profile Kain wrappers used by LLVM/direct-C targets.
- `stdlib/net.kn`, `stdlib/http.kn`, `stdlib/tls.kn`, `stdlib/http2.kn`: public stdlib mirrors so authored Kain code can import the networking domains as `std.net`, `std.http`, `std.tls`, and `std.http2`.
- `runtime/conformance/net_runtime/`: C conformance harness for TCP and HTTP behavior.
- `runtime/fixtures/native_net_http/main.kn`: executable Kain proof for authored TCP plus HTTP server flow.
- `blades/network-domains/src/main.kn`: public-surface proof blade for the built-in networking domains.

## Validation

Run the focused checks before claiming net runtime changes are done:

```powershell
cargo fmt -p kain-net
cargo test -p kain-net
clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_native_net_system.c
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane net
cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_net_tcp_http_and_actor_route_primitives -- --exact
cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_net_symbols_as_declarations -- --exact
bash runtime/conformance/net_runtime/run_tests.sh --verbose
target\debug\kain.exe runtime\fixtures\native_net_http\main.kn -t llvm -o runtime\fixtures\native_net_http\native_net_http.exe
runtime\fixtures\native_net_http\native_net_http.exe
target\debug\kain.exe blades\network-domains\src\main.kn -t llvm -o blades\network-domains\network-domains.exe
blades\network-domains\network-domains.exe
```

Delete `runtime\fixtures\native_net_http\generated` before committing unless generated fixture outputs become intentionally tracked.
Do not commit `runtime\native\src\core\z3\reports` or root `z3\reports`; those JSON files are validation output.
