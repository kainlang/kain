---
name: lang-stdlib
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain code that consumes the public root `std::*` surface: runtime, actor, alloc, collections, crypto, diagnostics, fs, gen_server, gpu, graphics, graphics::shared, hash, http, http2, input, intent, math, net, platform, process, reload, result, test, text, time, tls, and ui. Use for choosing imports, finding symbols without loading the whole stdlib map, writing Kain examples over stdlib domains, and routing stdlib bugs to runtime/bootstrap owners without changing stdlib implementation underneath.
---

# Lang Stdlib

This is the authored Kain stdlib field manual. Use it when code is writing IN Kain and needs the public root `std::*` domains instead of re-discovering the generated 120KB stdlib atlas by hand.

## Prime Directive

- Prefer public root imports: `use std::fs`, `use std::math`, `use std::http`, `use std::graphics`, `use std::ui`.
- Keep authored examples on public symbols. Private `abi_*` helpers are implementation wiring unless the task is stdlib maintenance.
- Do not read the entire generated map by default. Query it for the module or symbol family you need.
- Treat `@extern` stdlib functions as runtime-backed ABI calls. If the behavior is wrong, hand off to `runtime-stdlib`, `runtime-core`, or the owning portable crate.
- If authored Kain exposes an import/type/lowering bug, preserve the public stdlib design and escalate to `bootstrap-core` or the relevant bootstrap/runtime skill.
- If you change any top-level `stdlib/*.kn` file, regenerate and check the map. Never edit `stdlib/STDLIB_MAP.llm.md` or `stdlib/stdlib.map.json` by hand.

## Fast Lookup Loop

Use the bundled query helper before loading giant generated files:

```powershell
python query_stdlib.py --summary
python query_stdlib.py --imports
python query_stdlib.py --module math --contains vec3 --limit 40
python query_stdlib.py --module ui --contains clipboard --limit 40
python query_stdlib.py --search fs_read --limit 20
python query_stdlib.py --search GPU_DESCRIPTOR --kind const --limit 40
```

Then inspect exact source only when needed:

```powershell
rg -n "^use std::" library_of_kain blades benchmark smoketest
rg -n "\bfs_read_text\b|\bvec3_normalize_or_zero\b|\bgraphics_session_create\b" stdlib blades benchmark smoketest
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

## Live Root Profile

The generated native root profile currently says:

- `modules=27`
- `public_symbols=1549`
- `total_symbols=1976`
- `rust_builtins=233`
- `native_services=36`
- Scope is top-level `stdlib/*.kn` for LLVM/native root profile.
- Excluded overlays include `stdlib/ue5`, `stdlib/python`, `stdlib/javascript`, `stdlib/c`, and other target/vendor subtrees.

Current root imports:

```kn
use std::actor
use std::alloc
use std::collections
use std::crypto
use std::diagnostics
use std::fs
use std::gen_server
use std::gpu
use std::graphics
use std::graphics::shared
use std::hash
use std::http
use std::http2
use std::input
use std::intent
use std::math
use std::net
use std::platform
use std::process
use std::reload
use std::result
use std::runtime
use std::test
use std::text
use std::time
use std::tls
use std::ui
```

## Stdlib Flow

Authored stdlib code normally crosses these layers:

```text
.kn authored source
-> public root import such as std::fs or std::math
-> top-level stdlib/*.kn wrappers and pure Kain helpers
-> private abi_* declarations for runtime-backed services when needed
-> portable Rust contract crates for input/net/fs/process/actor/etc. when present
-> runtime/native C ABI substrate for OS, graphics, UI, network, process, actor, heap
-> blades, smoketest, benchmark, attrition, and Z3 evidence when the claim matters
```

Source anchors:

- Generated atlas: `stdlib/STDLIB_MAP.llm.md`, `stdlib/stdlib.map.json`.
- Atlas generator: `crates/kain-stdlib-map`.
- Stdlib loading/import truth: `crates/kain-core/src/stdlib.rs`, parser/typechecker import paths in `crates/kain-core/src`.
- Root authored modules: `stdlib/*.kn`.
- Runtime-backed native surface: `runtime/native/include`, `runtime/native/src/core`, `runtime/native/src/ui`, `runtime/native/src/graphics`.
- Portable contract crates: `crates/kain-fs`, `crates/kain-input`, `crates/kain-net`, `crates/kain-process`, `crates/kain-actor`.
- Proof blades: `blades/stdlib-domains`, `blades/math-domains`, `blades/network-domains`, `blades/hash-domains`.

## Choose By Need

| Need | Use | First Symbols / Patterns | Co-trigger |
| --- | --- | --- | --- |
| Start or stop native services | `std::runtime` | `runtime_init`, `runtime_shutdown`, `runtime_heap_validate` | `runtime-core` if broken |
| Actors and scheduler telemetry | `std::actor` | `actor_spawn`, `actor_send`, `actor_shutdown`, `actor_scheduler_queue_depth` | `lang-actors`, `lang-systems` |
| Arena/bump/pool memory helpers | `std::alloc` | `arena_create`, `arena_alloc`, `bump_alloc`, `pool_alloc` | `lang-systems` |
| Maps, queues, slot maps, clamps | `std::collections` | `typed_map_*`, `queue_*`, `slot_map_*`, `int_clamp`, `bool_to_int` | `lang-systems` |
| Digests and random bytes | `std::crypto` | `sha256`, `hmac_sha256`, `blake3`, `random_bytes_hex` | `lang-interop` if external crypto |
| Status and error composition | `std::diagnostics` | `status_ok`, `status_failed`, `bool_to_status`, `first_error` | `test-harness` |
| Files, paths, temp, metadata | `std::fs` | `fs_read_text`, `fs_write_text`, `fs_temp_file`, `fs_hash_file` | `runtime-stdlib`, `bootstrap-fs` |
| OTP-shaped actor service | `std::gen_server` | `gen_server_start`, `gen_server_call`, `gen_server_cast` | `lang-actors` |
| GPU resource contracts | `std::gpu` | `gpu_resource_policy`, `gpu_shared_buffer_zeroed`, descriptor constants | `lang-gpu` |
| Native graphics handles | `std::graphics` | `graphics_session_create`, `graphics_shader_spirv_from_hex`, `graphics_draw_mesh` | `lang-gpu`, `runtime-gpu` |
| Shared GPU/graphics resources | `std::graphics::shared` | `graphics_shared_vertex_policy`, `graphics_shared_sampled_image` | `lang-gpu` |
| Deterministic hashes | `std::hash` | `hash_u32`, `hash_mix32`, `hash_fnv1a32_update_u32`, `hash_bucket_mod` | `tool-z3-black-magic` for bit tricks |
| HTTP request/response/server | `std::http` | `request_create`, `server_create_localhost`, `route_actor`, `respond_text` | `lang-interop` |
| HTTP/2 request wrappers | `std::http2` | `http2_request_create`, `http2_client_state`, `http2_get_text` | `runtime-stdlib` if broken |
| Input events and action maps | `std::input` | `input_session_create`, `input_bind_action`, `input_push_key_down` | `lang-ui`, `runtime-stdlib` |
| Semantic runtime counters | `std::intent` | `entangle_reset`, `law_status`, `patch_journal_count`, `converge_mismatch_count` | `lang-semantics` |
| Engine math, color, noise, layout | `std::math` | `vec3_*`, `quat_*`, `mat4_*`, `ray_vs_aabb`, `fbm2`, `std140_*` | `lang-gpu`, `lang-ui`, `lang-systems` |
| TCP and network platform state | `std::net` | `net_reset`, `tcp_connect`, `tcp_listen`, `tcp_read_text` | `runtime-stdlib` |
| OS/platform dynamic libraries | `std::platform` | `platform_current_name`, `platform_library_open`, `platform_library_resolve` | `lang-interop` |
| Processes and PTYs | `std::process` | `process_spec_create`, `process_spawn`, `process_wait`, `process_stdout_text` | `runtime-stdlib` |
| Hot reload generations | `std::reload` | `reload_begin`, `reload_commit`, `reload_generation`, `reload_snapshot_key` | `lang-ui`, `package-kaintana` |
| Numeric result status codes | `std::result` | `result_ok`, `result_is_error`, `result_combine` | `test-harness` |
| Test/proof outcomes | `std::test` | `test_bool`, `test_proved`, `test_outcome_ok` | `test-harness` |
| Text slices and views | `std::text` | `text_slice`, `text_trim`, `text_find`, `text_materialize` | `lang-systems` for zero-copy |
| Time and deadlines | `std::time` | `now_millis`, `sleep_millis`, `deadline_millis` | `runtime-stdlib` |
| TLS HTTPS client wrappers | `std::tls` | `tls_https_request_create`, `tls_client_state`, `tls_https_get_text` | `runtime-stdlib` |
| Native UI handles | `std::ui` | `ui_session_create`, `ui_node_create`, `ui_node_set_rect`, `ui_draw_command_count` | `lang-ui`, `package-kaintana` |

## Module Atlas

Use this as the one-scan map. Query exact symbols with repo-root `query_stdlib.py`.

| Module | Source | Public Shape |
| --- | --- | --- |
| `std::actor` | `stdlib/actor.kn` | 74 public actor lifecycle, registry, monitor/link, supervision, scheduler telemetry helpers. Prefer language `actor`/`spawn`/`send`/`ask` for semantic authoring; use `std::actor` when you need runtime service handles or telemetry. |
| `std::alloc` | `stdlib/alloc.kn` | 27 public bump, arena, and pool allocator structs/functions. Use when authored Kain needs explicit allocation domains beyond raw `alloc_zeroed`; destroy/reset allocators deliberately. |
| `std::collections` | `stdlib/collections.kn` | 87 public helpers for typed string-int maps, queues, deques, priority queues, slot maps, `int_min/max/clamp`, and bool/int conversion. |
| `std::crypto` | `stdlib/crypto.kn` | 9 public digest/random helpers: SHA-256, HMAC-SHA-256, BLAKE3, random bytes/hex. Use for authored proof/demo crypto, not external provider integration. |
| `std::diagnostics` | `stdlib/diagnostics.kn` | 18 public status helpers. Use to normalize `0 == ok` style runtime results into Bool/status folds. |
| `std::fs` | `stdlib/fs.kn` | 30 public filesystem helpers for text/ranges/hex bytes, atomic write, metadata, dirs/walk, temp, hash, join, and last-error queries. |
| `std::gen_server` | `stdlib/gen_server.kn` | 7 public OTP-ish actor service helpers plus `GenServer`. Use for call/cast/info shape when plain actor examples become service-shaped. |
| `std::gpu` | `stdlib/gpu.kn` | 121 public GPU policy constants, layout structs, buffer/image resource constructors, descriptor helpers, residency and queue/access flags. This is authored resource contract, not executor internals. |
| `std::graphics` | `stdlib/graphics.kn` | 84 public native graphics session/backend/frame/buffer/shader/mesh/pipeline/draw/status helpers. Use for low-level graphics proof blades, not full app UI architecture. |
| `std::graphics::shared` | `stdlib/graphics_shared.kn` | 32 public bridge helpers that turn `std::gpu` shared resources into graphics vertex/index/uniform/storage/image attachments. |
| `std::hash` | `stdlib/hash.kn` | 42 public 32-bit hash constants, wrappers, rotate/mix/fnv/crc/fingerprint/bucket helpers. Use for deterministic routing and packed lane math. |
| `std::http` | `stdlib/http.kn` | 36 public HTTP request/response/client/server helpers, route-to-actor, server pump, response builders, and handle destruction. |
| `std::http2` | `stdlib/http2.kn` | 10 public HTTP/2 state/support/request/client helpers layered over request handles. |
| `std::input` | `stdlib/input.kn` | 38 public input source/session/action/axis/event/frame/query/replay helpers. It supports keyboard, pointer, CLI, UI runtime, agent, synthetic, and native sources. |
| `std::intent` | `stdlib/intent.kn` | 50 public helpers for entangle registration, patch journal counters, law/patch status, converge choices, orchestrate telemetry, and semantic runtime counters. |
| `std::math` | `stdlib/math.kn` | 250 public engine-facing math symbols: scalar helpers, Vec2/3/4, Vec3A, Quat, Mat3/4, affine transforms, geometry, colors, noise, GPU layouts, and SIMD-ish lane packs. |
| `std::net` | `stdlib/net.kn` | 56 public network platform/capability, TCP client/server, HTTP parsing/server helpers, local status/error helpers. |
| `std::platform` | `stdlib/platform.kn` | 24 public platform identity and dynamic library open/close/resolve/status helpers. This is Kain's OS contract seam for package/platform work. |
| `std::process` | `stdlib/process.kn` | 49 public child-process and PTY helpers: specs, args/cwd/env, stdio modes, spawn, poll/wait, stdin/stdout/stderr text/hex, terminate/kill. |
| `std::reload` | `stdlib/reload.kn` | 31 public hot-reload generation, snapshot, package surface, migration, and lane-class helpers. |
| `std::result` | `stdlib/result.kn` | 20 public numeric result-code helpers. Use when a runtime service returns status integers and you need stable composition. |
| `std::runtime` | `stdlib/runtime.kn` | 56 public runtime init/shutdown, heap validation, attrition checkpointing, CPU feature/capability, SIMD lane, converge cache/telemetry, machine teleport/pulse counters. |
| `std::test` | `stdlib/test.kn` | 17 public test/proof outcome constants and helpers: pass/fail/skip/proved/witness, bool outcome, status combine. |
| `std::text` | `stdlib/text.kn` | 26 public zero-copy text slice and string view helpers: slice/from/view/len/find/contains/trim/materialize. |
| `std::time` | `stdlib/time.kn` | 8 public native time helpers: now, sleep, deadline, elapsed. |
| `std::tls` | `stdlib/tls.kn` | 11 public TLS/HTTPS client state and request helpers. |
| `std::ui` | `stdlib/ui.kn` | 336 public native UI session/window/frame/node/style/event/resource/font/text/clipboard/IME/drag/drop/menu/dialog/accessibility/draw/canvas helpers. Use `lang-ui` for higher authored UI semantics. |

## Public vs Native vs Private

- Prefer root aliases like `runtime_init`, `actor_abi_version`, `graphics_session_create`, `ui_session_create`, and `fs_read_text`.
- `native_*` symbols are often public compatibility or lower-level wrappers. Use them only when nearby proof blades already do, or when verifying the native ABI directly.
- Private `abi_*` symbols are stdlib-internal declarations that bind to `@extern`. Do not author normal app code against them.
- If a root alias is missing for a capability you need, query the map before inventing it. The name may already exist under another domain.

## Foundation Probe

This is the compact "does the root stdlib breathe?" lane. It shows runtime lifecycle, result/status/test helpers, files, text, crypto, time, and cleanup.

```kn
use std::runtime
use std::result
use std::diagnostics
use std::test
use std::fs
use std::text
use std::crypto
use std::time

fn stdlib_foundation_probe() -> Int:
    let boot = runtime_init()
    if boot < 0:
        return 100 + boot

    if result_is_ok(result_ok()) == false:
        return 1
    if status_ok(bool_to_status(true)) == false:
        return 2

    let temp = fs_temp_file("stdlib-foundation")
    fs_write_text(temp, "  kain-stdlib  ")
    let trimmed = text_trim(fs_read_text(temp))
    let digest = sha256(trimmed)
    fs_remove_file(temp)

    let outcome = test_bool("stdlib.foundation.digest", len(digest) == 64)
    let deadline = deadline_millis(0)
    if test_outcome_ok(outcome) == false or deadline < now_millis():
        return 3

    let shutdown = runtime_shutdown()
    if shutdown < 0:
        return 200 + shutdown
    return len(trimmed) + outcome.status
```

## Math Is Engine-Ready

`std::math` is not just `sin` and `cos`; it is the seed crystal for graphics, simulation, UI layout, GPU data layout, procedural generation, and 3D engine work.

Use these families:

- Scalar: `abs`, clamp/min/max-style helpers, radians/degrees constants and helpers.
- Vectors: `vec2`, `vec3`, `vec4`, length/distance/dot/cross/normalize.
- Quaternions: identity, axis-angle, rotate-vector, composition helpers.
- Matrices/affines: `mat4_identity`, `mat4_from_trs`, transform point/vector, affine2/3 transforms.
- Geometry: `Ray3`, `Aabb`, ray/AABB, ray/triangle, planes, hit helpers.
- Color: `ColorRgba`, `Hsv`, pack/unpack RGBA, HSV/RGB conversion.
- Noise/curves: `fbm2`, `worley_noise`, Bezier helpers.
- GPU layout: `std140_*`, `std430_*`, C-buffer layout wrappers.
- Lane packs: `Vec3x4`, `Vec4x8`, `Float8` style SIMD-ish authoring surfaces.

```kn
use std::math

fn engine_math_seed() -> Int:
    let rotation = quat_from_axis_angle(vec3_up(), half_pi())
    let forward = quat_rotate_vec3(rotation, vec3_right())
    let transform = mat4_from_trs(vec3(1.0, 2.0, 3.0), rotation, vec3_one())
    let point = mat4_transform_point(transform, forward)

    let bounds = Aabb { min: vec3(-1.0, -1.0, -1.0), max: vec3(1.0, 1.0, 1.0) }
    let ray = ray3(vec3(0.0, 0.0, -4.0), vec3_forward())
    let hit = ray_vs_aabb(ray, bounds)

    let color = hsv_to_rgb(Hsv { h: 0.0, s: 1.0, v: 1.0 })
    let packed = pack_rgba_to_u32(color_rgba(color.x, color.y, color.z, 1.0))
    let layout = std140_mat4(mat4_identity())
    let noise = fbm2(vec2(0.31, 0.73), 4)

    var score: Int = 0
    if vec3_dot(point, vec3_up()) >= 2.0:
        score = score + 1
    if ray_hit_is_hit(hit):
        score = score + 1
    if std140_mat4_alignment_bytes(layout) == 16:
        score = score + 1
    if noise >= 0.0:
        score = score + 1
    return score + (packed % 97)
```

## IO, Network, Process, Platform

These domains are Kain's authored OS contract surface. Use them when Kain needs to touch the outside world without becoming a C/Rust app in disguise.

- `std::fs` is for deterministic file/path work. Always clean temp files/dirs in examples.
- `std::net` owns platform capability state, TCP, and lower-level server pumping.
- `std::http` owns request/response handles, HTTP client/server convenience, and route-to-actor.
- `std::tls` and `std::http2` layer secure/protocol-specific clients over request handles.
- `std::process` owns child processes and PTYs; build a spec, spawn, wait/poll, read output, close handles.
- `std::platform` owns platform identity and dynamic-library contracts; co-trigger `lang-interop` for DLL/package work.

```kn
use std::runtime
use std::net
use std::http
use std::tls
use std::http2
use std::actor

actor HttpProbe:
    state hits: Int = 0

    on HttpRequest(payload: String):
        self.hits = self.hits + len(payload)

fn network_probe() -> Int:
    let _boot = runtime_init()
    let _reset = net_reset()
    if net_platform_available() != 1:
        let _shutdown_unavailable = runtime_shutdown()
        return 0

    let server = server_create_localhost(0)
    if server <= 0 or server_listen(server) != 0:
        return 1

    let handler = actor_spawn("HttpProbe", "hits=0")
    let _route = route_actor(server, "POST", "/probe", handler, "HttpRequest")
    let client = tcp_connect("127.0.0.1", server_local_port(server), 5000)
    let _write = tcp_write_text(client, "POST /probe HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 4\r\n\r\nkain")
    let request = server_pump(server, 5000)
    let _reply = respond_text(request, 200, "ok")
    let wire = tcp_read_text(client)

    let secure = tls_https_request_create("GET", "https://example.invalid/")
    let h2 = http2_request_create("GET", "https://example.invalid/")
    let score = len(wire) + tls_client_state() + http2_client_state()

    let _destroy_secure = request_destroy(secure)
    let _destroy_h2 = request_destroy(h2)
    let _close_client = tcp_close(client)
    let _close_server = server_close(server)
    let _shutdown = runtime_shutdown()
    return score
```

## GPU, Graphics, Shared Resources, UI

Do not confuse these layers:

- `std::gpu` describes resource policy, memory residency, descriptor kind, layout, and buffer/image resources.
- `std::graphics::shared` adapts shared GPU resources into graphics-facing vertex/index/uniform/storage/image contracts.
- `std::graphics` owns native graphics sessions, backend selection, buffers, shader modules, meshes, pipelines, frame/draw/present handles.
- `std::ui` owns low-level native UI sessions, windows, nodes, style/state/event/resource/text/draw handles.
- Components/JSX/world surfaces belong to `lang-ui` and `lang-semantics`; `std::ui` is the handle-level ABI-backed substrate.

```kn
use std::gpu
use std::graphics
use std::graphics::shared
use std::ui

fn graphics_ui_probe() -> Int:
    let policy = gpu_resource_policy(
        gpu_shared_memory_policy(
            GPU_ACCESS_READ_WRITE,
            GPU_QUEUE_COMPUTE | GPU_QUEUE_TRANSFER | GPU_QUEUE_HOST,
            GPU_LAYOUT_STD430,
            GPU_DESCRIPTOR_STORAGE_BUFFER
        ),
        GPU_BUFFER_USAGE_STORAGE | GPU_BUFFER_USAGE_TRANSFER_SRC | GPU_BUFFER_USAGE_TRANSFER_DST,
        "probe.storage"
    )
    let storage = gpu_shared_buffer_zeroed("f32", [4], "f32", "application/octet-stream", policy)
    let descriptor = gpu_buffer_descriptor(storage)
    if json_get_string(descriptor, "descriptor_kind") != GPU_DESCRIPTOR_STORAGE_BUFFER:
        return 1

    let vertex = gpu_shared_buffer_zeroed("u32", [4], "u32", "application/octet-stream", graphics_shared_vertex_policy("probe.vertex"))
    let vertex_buffer = graphics_shared_vertex_buffer(vertex, 4)
    if vertex_buffer.ready == false:
        return 2

    let _graphics_reset = graphics_reset()
    let graphics_session = graphics_session_create("probe.graphics", 320, 180)
    let _ui_reset = ui_reset()
    let ui_session = ui_session_create("probe.ui", 320, 180)
    let panel = ui_node_create(ui_session, "panel")
    let _rect = ui_node_set_rect(ui_session, panel, 8.0, 8.0, 180.0, 48.0)
    let _text = ui_node_set_text(ui_session, panel, "Kain stdlib")
    var ready_score: Int = 0
    if vertex_buffer.ready:
        ready_score = 1
    let score = len(ui_node_text(ui_session, panel)) + graphics_session + ready_score
    let _ui_destroy = ui_session_destroy(ui_session)
    let _graphics_destroy = graphics_session_destroy(graphics_session)
    return score
```

## Collections, Text, Hash, Crypto, Alloc

These modules let Kain examples avoid fake placeholder logic:

- `std::collections`: use typed maps, queues, priority queues, slot maps, and clamps to model real data movement.
- `std::text`: use slices/views for low-copy string inspection; materialize only when needed.
- `std::hash`: use deterministic bit/hash helpers for routing, bucketing, fingerprints, branchless table work.
- `std::crypto`: use digest/random helpers for proof/demo security surfaces.
- `std::alloc`: use arena/bump/pool helpers when authored code needs allocator shape, not just one-off raw pointers.

Cleanup matters. Destroy queues, maps, slots, allocators, sessions, request handles, process handles, network handles, graphics sessions, and UI sessions when the API offers a destroy/close helper.

## Runtime, Intent, Tests

Use these as proof glue:

- `std::runtime` starts/stops native runtime services, checks heap health, exposes CPU capability and SIMD/converge/machine counters.
- `std::intent` observes semantic runtime behavior from authored Kain: entangle registrations, patch journal count, law status, converge mismatches, orchestrate stages, teleport/pulse counters.
- `std::test` returns durable `TestOutcome` values for source-side pass/fail/skip/proved/witness semantics.
- `std::diagnostics` and `std::result` normalize status-code composition across runtime services.

```kn
use std::runtime
use std::intent
use std::test

fn semantic_runtime_shape_ok() -> Bool:
    let heap_ok = runtime_heap_validate() >= 0
    let laws_ok = law_status(true) == 0
    let converge_ok = converge_mismatch_count() == 0
    let proof = test_proved("semantic-runtime-shape.smt2", "unsat")
    return heap_ok and laws_ok and converge_ok and test_outcome_ok(proof)
```

## Validation Ladder

For authored stdlib usage:

```powershell
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

For root stdlib changes:

```powershell
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check
kain check blades/stdlib-domains/src/main.kn --target llvm
kain check blades/math-domains/src/main.kn --target llvm
kain check blades/network-domains/src/main.kn --target llvm
```

For performance or safety claims:

- Put speed claims in `benchmark/` and run `test-bench`.
- Put lifecycle/teardown claims in `attrition/` and run `test-attrition`.
- Put pointer/layout/bit/hash/buffer math into Z3 proof packs when the invariant matters.
- Use `std::test` for authored proof outcomes, but remember solver `unsat` is stronger than a handful of examples.

## Hand Off Matrix

- Use `lang-semantics` when stdlib is part of `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `pulse`, `teleport`, shader, component, or actor fusion.
- Use `lang-systems` when stdlib is part of raw memory, effects, allocator pressure, actor pressure, branchless lanes, or ownership-sensitive code.
- Use `lang-gpu` for shader/compute authoring around `std::gpu`, `std::graphics`, or `std::graphics::shared`.
- Use `lang-ui` for UI component/layout/experience authoring over `std::ui`; use `package-kaintana` for Kaintana package work.
- Use `lang-interop` for `std::platform`, native dynamic libraries, vendor DLL contracts, host JSON bridges, and OS integration surfaces.
- Use `runtime-stdlib` when a runtime-backed stdlib function is wrong or missing at the native service layer.
- Use `runtime-core` when runtime init/shutdown, heap, actor substrate, ABI service tables, or native core telemetry is wrong.
- Use `runtime-gpu` when graphics/GPU executor behavior is wrong below authored `std::gpu`/`std::graphics` usage.
- Use `bootstrap-core` when imports, typechecking, stdlib loading, or authored call resolution are wrong.
- Use `bootstrap-fs`, `bootstrap-actors`, `bootstrap-gpu`, or `bootstrap-ownership` when the compiler/frontend truth for that semantic domain must change.
- Use `test-harness` for `std::test` directive/report behavior.

## Anti-Patterns

- Do not paste the whole generated stdlib map into context just to find one symbol.
- Do not author against private `abi_*` functions in examples.
- Do not invent `std::native::*` or parallel stdlib trees for new user-facing code.
- Do not silently use `native_*` when a public root alias already exists.
- Do not skip `runtime_init`/`runtime_shutdown` in native service proof blades unless the nearest working blade intentionally does.
- Do not leak handles in examples: close/destroy/remove when the public API provides a cleanup call.
- Do not copy Vulkain/Kaintana/example shapes blindly. Use the stdlib domain, then write a fresh Kain-shaped example for the task.
- Do not claim a stdlib implementation change is complete until the generated map checks and the relevant proof blade still checks.
