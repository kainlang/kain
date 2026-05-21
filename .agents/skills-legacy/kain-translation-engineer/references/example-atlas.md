# Kain Example Atlas

Use this as a thin pointer map. Do not bulk-load every file; pick the closest example, read its `.kn` entrypoint and `KAIN.toml`, then follow only the modules it imports. The future Kain MCP/RAG layer should own semantic indexing over this corpus.

## Network And HTTP

- `blades/network-domains/src/main.kn`: first stop for server/client network translation. Shows `use std::net`, `std::http`, `std::tls`, `std::http2`, localhost server creation, route-to-actor, raw TCP request text, parsed request fields, response headers/body, TLS request creation, HTTP/2 request creation, state checks, close/destroy cleanup.
- `blades/stdlib-domains/src/main.kn`: compact public root-domain proof that includes request creation, protocol checks, TLS/H2 state, and runtime init/shutdown alongside other stdlib domains.
- `benchmark/cases/http_server_concurrency/main.kn` and `benchmark/cases/http_server_frameworks/main.kn`: open only when the translation claims HTTP performance or server concurrency.
- `benchmark/cases/tcp_loopback_tokio/main.kn`: open when mapping Rust Tokio TCP semantics to Kain native networking.

Translation rule: use the root `std::net`, `std::http`, `std::tls`, and `std::http2` APIs before adding runtime/native substrate. If a primitive is missing, keep the Kain spec/module clean and isolate the platform bridge behind runtime or C FFI.

## C ABI And Native Bridges

- `blades/vulkain/KAIN.toml`: clean `[c_ffi]` metadata example with inline C bridge, header/source/include paths, defines, and link libs.
- `blades/vulkain/examples/mesh-scene/src/main.kn`: simple `use c::vulkain_bridge` plus Kain-authored scene parameters driving native Vulkan.
- `blades/pong/KAIN.toml` and `blades/pong/src/main.kn`: blade-local presenter bridge plus Kain-owned state and UI logic.
- `blades/kaintana/KAIN.toml` and `blades/kaintana/src/main.kn`: desktop adapter bridge with Kain-authored UI composition.

Translation rule: keep OS/driver/windowing code in C/native substrate, but keep scene, UI, state, and policy authored in Kain.

## UI And App Structure

- `blades/kaintana/src/main.kn`: full Kain-authored UI shell over `std::alloc`, `std::collections`, `std::graphics`, `std::input`, `std::math`, `std::text`, `std::ui`, and a desktop C bridge.
- `blades/kaintana/src/api/*.kn` and `blades/kaintana/src/core/*.kn`: reusable UI framework vocabulary, layout, theme, reconciliation, render command, input, and type modules.
- `blades/pong/src/main.kn`: stateful game/app structure using `world`, `entangle`, `actor`, config modules, UI helpers, and a native presenter.

Translation rule: translate React/TS UI state into Kain state modules, layout helpers, and UI render commands. Do not recreate React component ceremony unless the target is a web/TS backend.

## Actors, Worlds, And State

- `blades/actor-ask-roundtrip/src/main.kn`: minimal `actor`, `spawn`, `ask`, `ask_timeout`, typed Bool/Int replies, runtime init/shutdown.
- `blades/pong/src/main.kn`: large state lattice with two `world`s, many `entangle` bindings, actors, UI, and native bridge.
- `benchmark/cases/quantumerlang/main.kn`: pressure row for actor/message/ownership/converge/teleport/world translation.

Translation rule: use actors for request/reply and service boundaries. Use worlds/entangle for mirrored state. Do not preserve Rust channels or TS event emitters as a one-for-one module graph.

## Stdlib Foundation

- `blades/stdlib-domains/src/main.kn`: one-screen root stdlib import proof for runtime, actor, collections, diagnostics, result, time, intent, fs, input, net/http/tls/http2, process, graphics, and UI.
- `blades/stdlib-foundations/src/main.kn`: deeper `std::text`, `std::collections`, `std::crypto`, and `std::alloc` proof, including typed maps, queues, deque, priority queue, SlotMap, hashes, HMAC/BLAKE3, and arena/bump/pool allocators.
- `blades/hash-domains/src/main.kn`: focused `std::hash` proof for masks, rotates, word mixing, pair hashing, buckets, FNV-1a, CRC32, and fingerprints.

Translation rule: check `stdlib/STDLIB_MAP.llm.md` and these blades before writing local helpers for text, collections, crypto, allocators, hashing, or domain wrappers.
