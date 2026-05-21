## Ultimate Kain Specimen
Welcome to kain-lang, an all new language that is currently on average 2x faster than C++ and 8x faster than rust.
This is the injected north-star example. It is intentionally dense: it teaches agents Kain is allowed to be in one scan. For compile-certified syntax, compare against `blades/kain-example/src/main.kn`, `benchmark/cases/semantic_singularity_crucible/main.kn`, `blades/stdlib-domains/src/main.kn`, `blades/network-domains/src/main.kn`, `blades/math-domains/src/main.kn`, and `benchmark/cases/gpu_graphics_submit/main.kn`. A real `use c::...` lane also needs a blade-local `KAIN.toml` with `[c_ffi]` metadata. (if you update the stdlib or add a new one, update this example)



```kn
use std::runtime
use std::actor
use std::collections
use std::text
use std::crypto
use std::alloc
use std::diagnostics
use std::result
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify
use std::time
use std::intent
use std::fs
use std::input
use std::net
use std::http
use std::tls
use std::http2
use std::process
use std::platform
use std::gpu
use std::graphics
use std::graphics::shared
use std::ui
use std::reload
use std::math
use c::quantum_native_bridge

const KAIN_MYTHIC_MODULUS: Int = 1000000007
const KAIN_MYTHIC_PACKET_COUNT: Int = 64
const KAIN_MYTHIC_WORDS_PER_PACKET: Int = 4
const KAIN_MYTHIC_WORD_BYTES: Int = 8
const KAIN_MYTHIC_CACHE_LINE_BYTES: Int = 64
const KAIN_MYTHIC_CACHE_LINE_WORDS: Int = 8
const KAIN_MYTHIC_SIMD_LANES: Int = 4
const KAIN_MYTHIC_AVALANCHE_A: Int = 2246822519
const KAIN_MYTHIC_AVALANCHE_B: Int = 3266489917
type MythicChecksum = Int

enum MythicLane:
    Authority
    Mirror
    ActorSwarm
    DirtyMemory
    ZeroCopyWire
    GpuField
    NativeBridge
    UiTelemetry

struct MythicPacket:
    id: Int
    lane: MythicLane
    seq: Int
    payload: Int
    tag: String
    hot: Bool

trait MythicFold:
    fn fold_seed(_self: Self_) -> Int:
        return 0

impl MythicPacket:
    fn static_weight(_self: Self_) -> Int:
        return 97

impl MythicFold for MythicPacket:
    fn fold_seed(_self: Self_) -> Int:
        return 211

comptime:
    const MYTHIC_SURFACE_COUNT: Int = 17
    const MYTHIC_MAGIC_ROUTE_MASK: Int = 63

shader fragment MythicFieldFragment(uv: Vec2) -> Vec4:
    uniform accent: Vec3 @0
    let ring: Float = fbm2(uv, 4)
    return vec4(accent.x * ring, accent.y, accent.z, 1.0)

shader compute MythicParticleKernel(id: UVec3) -> Vec4:
    uniform particles: StorageBuffer<Vec4> @0
    uniform field: StorageBuffer<Vec4> @1
    let p = particles[id.x]
    let v = field[id.x]
    return vec4(p.x + v.x, p.y + v.y, p.z + v.z, 1.0)

axiom mythic_machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    when capability("cpu.x86.avx2")
    when capability("graphics.vulkan")
    when capability("net.tcp")
    guarantee "mythic lane has shattered memory, entangled worlds, native actors, GPU submit, C ABI bridge, and zero-copy wire arithmetic"
    fallback mythic_scalar_mix

component MythicPanel():
    render <panel title="Kain Mythic Specimen" />

world MythicAuthority:
    state signal: Int = 1
    state epoch: Int = 0
    state health: Int = 100
    surface native_ui => MythicPanel

world MythicMirror:
    state signal_copy: Int = 1
    state epoch_copy: Int = 0
    state health_copy: Int = 100
    surface web => MythicPanel

entangle MythicAuthority.signal <-> MythicMirror.signal_copy with single_writer
entangle MythicAuthority.epoch <-> MythicMirror.epoch_copy with single_writer
entangle MythicAuthority.health <-> MythicMirror.health_copy with single_writer

shatter struct MythicShard:
    bias: Int
    phase: Int
    salt: Int
    alive: Bool

actor MythicRelay:
    state bias: Int = 11
    state turns: Int = 0

    on Fold(reply_to: P, request: Int):
        self.turns = self.turns + 1
        send reply_to.Reply(value = ((request * 17) + self.bias + self.turns + 23) % KAIN_MYTHIC_MODULUS)

law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < KAIN_MYTHIC_MODULUS

patch commit_signal(authority: MythicAuthority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.health = int_clamp(authority.health + 1, 0, 1000000)
    return authority.signal

fn mythic_scalar_mix(value: Int) -> Int:
    return ((value * 31) + 7) % KAIN_MYTHIC_MODULUS

converge mythic_mix(value: Int) -> Int:
    spec reference:
        return mythic_scalar_mix(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % KAIN_MYTHIC_MODULUS
    fast avx2_lane when capability("cpu.x86.avx2"):
        return ((value * 31) + 7) % KAIN_MYTHIC_MODULUS
    fast cuda_lane when target("cuda"):
        return ((value * 31) + 7) % KAIN_MYTHIC_MODULUS
    verify random(8)

fn bridge_bias(value: Int) -> Int:
    return quantum_native_bridge.bias_mix(value, 19)

orchestrate mythic_pipeline(value: Int) -> Int:
    let normalized: Int = kain mythic_mix(value)
    let bridged: Int = c bridge_bias(normalized)
    let staged: Int = rust mythic_scalar_mix(bridged)
    return staged

pulse mythic_clock every 8ms jitter 1ms:
    let seed = MythicShard { bias: 1, phase: 2, salt: 3, alive: true }
    let moved = teleport seed from MythicAuthority to MythicMirror via pulse_bus
    let _pulse_shape = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias

fn lane_rank(lane: MythicLane) -> Int:
    match lane:
        MythicLane::Authority => 3
        MythicLane::Mirror => 5
        MythicLane::ActorSwarm => 7
        MythicLane::DirtyMemory => 11
        MythicLane::ZeroCopyWire => 13
        MythicLane::GpuField => 17
        MythicLane::NativeBridge => 19
        MythicLane::UiTelemetry => 23
        _ => 0

fn ready_quantum() -> impl Future<Int>:
    return async 29

fn maybe_lane(flag: Bool) -> Option<Int>:
    if flag:
        return Some(17)
    return None

fn parse_lane(flag: Bool) -> Result<Int, String>:
    if flag:
        return Result::Ok(23)
    return Result::Err("mythic parse rejected")

fn use_result_question_mark() -> Result<Int, String>:
    let parsed: Int = parse_lane(true)?
    return Result::Ok(parsed + 1)

fn approx(a: Float, b: Float) -> Bool:
    return abs(a - b) <= 0.01

fn math_lane() -> Int:
    let v = vec3(3.0, 4.0, 0.0)
    let n = vec3_normalize_or_zero(v)
    let q = quat_from_axis_angle(vec3_up(), half_pi())
    let rotated = quat_rotate_vec3(q, vec3(1.0, 0.0, 0.0))
    let m = mat4_from_trs(vec3(1.0, 2.0, 3.0), q, vec3_one())
    let p = mat4_transform_point(m, rotated)
    let color = hsv_to_rgb(Hsv { h: 0.0, s: 1.0, v: 1.0 })
    let noise = fbm2(vec2(0.31, 0.73), 4)
    let packed = pack_rgba_to_u32(color_rgba(1.0, 0.5, 0.0, 1.0))
    var score: Int = 0
    if approx(vec3_length(v), 5.0):
        score = score + 101
    if vec3_distance(n, vec3(0.6, 0.8, 0.0)) <= 0.01:
        score = score + 103
    if abs(vec3_dot(rotated, vec3_forward())) >= 0.99:
        score = score + 107
    if approx(vec3_dot(p, vec3_up()), 2.0):
        score = score + 109
    if vec3_distance(color, vec3(1.0, 0.0, 0.0)) <= 0.06:
        score = score + 113
    if noise >= 0.0 and noise <= 1.5:
        score = score + 127
    return score + (packed % 97)

// Alien metal lane: Kain should be comfortable describing cache geometry,
// packed wire layouts, branchless selectors, raw pointer slices, and
// target-specialized lowering in the same file as worlds/actors/UI.
// Any real version of this must get Z3 cases for:
// - ptr_offset(base, index, "Int") stays within allocated word span.
// - packed_header fields round-trip without overlap.
// - avalanche/converge lanes are equivalent modulo 2^32 or the chosen ring.
// - cache-line tile writes never cross the total_words allocation.
fn mythic_rotl32(value: Int, bits: Int) -> Int with Unsafe:
    let masked: Int = value & 4294967295
    let left: Int = (masked << bits) & 4294967295
    let right: Int = masked >> (32 - bits)
    return (left | right) & 4294967295

fn mythic_pack_header(seq: Int, kind: Int, flags: Int, version: Int) -> Int with Unsafe:
    let seq_lane: Int = (seq & 1048575) << 12
    let kind_lane: Int = (kind & 15) << 8
    let flag_lane: Int = (flags & 15) << 4
    let version_lane: Int = version & 15
    return seq_lane | kind_lane | flag_lane | version_lane

fn mythic_header_route(header: Int) -> Int with Unsafe:
    return ((header >> 12) ^ (header >> 8) ^ header) & MYTHIC_MAGIC_ROUTE_MASK

fn mythic_avalanche32(value: Int) -> Int with Unsafe:
    var x: Int = value & 4294967295
    x = (x ^ (x >> 16)) & 4294967295
    x = (x * KAIN_MYTHIC_AVALANCHE_A) & 4294967295
    x = (x ^ (x >> 13)) & 4294967295
    x = (x * KAIN_MYTHIC_AVALANCHE_B) & 4294967295
    return (x ^ (x >> 16)) & 4294967295

fn mythic_branchless_select(mask: Int, hot_value: Int, cold_value: Int) -> Int with Unsafe:
    let all_bits: Int = 0 - (mask & 1)
    return (hot_value & all_bits) | (cold_value & (all_bits ^ -1))

fn mythic_tile_base(packet: Int) -> Int with Unsafe:
    let cache_line: Int = packet / KAIN_MYTHIC_CACHE_LINE_WORDS
    let lane: Int = packet % KAIN_MYTHIC_CACHE_LINE_WORDS
    return (cache_line * KAIN_MYTHIC_CACHE_LINE_WORDS) + lane

fn mythic_store_packet(buffer: ptr<Int>, packet: Int, round: Int, salt: Int) -> Int with Unsafe:
    let seq: Int = (round * KAIN_MYTHIC_PACKET_COUNT) + packet
    let kind: Int = ((packet * 3) + round) & 15
    let flags: Int = mythic_branchless_select(packet & 1, 9, 3)
    let version: Int = 1
    let header: Int = mythic_pack_header(seq, kind, flags, version)
    let route: Int = mythic_header_route(header)
    let mixed: Int = mythic_avalanche32(header + (salt * 1315423911) + route)
    let payload: Int = mixed % 4096
    let word0: Int = header
    let word1: Int = ((payload & 4095) << 7) | route
    let word2: Int = mythic_rotl32(mixed, (packet % 23) + 1)
    let word3: Int = (word0 + word1 + word2 + salt + 97) % 1000003
    let base: Int = mythic_tile_base(packet) * KAIN_MYTHIC_WORDS_PER_PACKET
    mem_store(ptr_offset(buffer, base + 0, "Int"), word0, "Int")
    mem_store(ptr_offset(buffer, base + 1, "Int"), word1, "Int")
    mem_store(ptr_offset(buffer, base + 2, "Int"), word2, "Int")
    mem_store(ptr_offset(buffer, base + 3, "Int"), word3, "Int")
    return (word0 ^ word1 ^ word2 ^ word3) & 4294967295

fn mythic_scalar_metal_mix(value: Int) -> Int with Unsafe:
    return mythic_avalanche32(value + 374761393)

converge mythic_metal_mix(value: Int) -> Int:
    spec reference:
        return mythic_scalar_metal_mix(value)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return mythic_avalanche32(value + 374761393)
    fast cuda_lane when target("cuda"):
        return mythic_avalanche32(value + 374761393)
    fast native_abi_lane when capability("c.abi"):
        return quantum_native_bridge.avalanche32(value + 374761393)
    verify exhaustive(16)

fn mythic_cacheline_stream(buffer: ptr<Int>, rounds: Int) -> Int with Unsafe:
    var round: Int = 0
    var acc: Int = 0
    while round < rounds:
        var packet: Int = 0
        collapse buffer:
            while packet < KAIN_MYTHIC_PACKET_COUNT:
                let lane_hash = mythic_store_packet(buffer, packet, round, acc + round)
                acc = (acc + mythic_metal_mix(lane_hash + packet)) % KAIN_MYTHIC_MODULUS
                packet = packet + 1
            0
        round = round + 1
    return acc

fn zero_copy_wire_lane(buffer: ptr<Int>, round: Int) -> Int:
    var acc: Int = 0
    var packet: Int = 0
    while packet < KAIN_MYTHIC_PACKET_COUNT:
        let lane_hash = mythic_store_packet(buffer, packet, round, acc + 17)
        acc = (acc + mythic_metal_mix(lane_hash + round + packet)) % KAIN_MYTHIC_MODULUS
        packet = packet + 1
    return acc

fn stdlib_probe_lane() -> Int:
    let temp = fs_temp_file("mythic-specimen")
    fs_write_text(temp, "kain")
    fs_append_text(temp, "-mythic")
    let fs_text_score = len(fs_read_text(temp))
    fs_remove_file(temp)

    let wire_view = text_trim(text_slice("  route:zero-copy  ", 2, 15))
    let view_score = text_len(wire_view) + text_find(wire_view, "zero") + len(text_materialize(wire_view))
    let crypto_score = len(sha256("kain")) + len(hmac_sha256("kain-key", "payload")) + len(blake3("kain")) + len(random_bytes_hex(8))
    let map = typed_map_set(typed_map_new(), "route", 41)
    var queue = queue_create(4)
    queue = queue_push(queue, typed_map_get(map, "route"))
    let queue_score = queue_peek(queue) + queue_len(queue)
    let _queue_destroy = queue_destroy(queue)
    let _map_destroy = typed_map_destroy(map)
    var slots = slot_map_create(2)
    let slot = slot_map_insert(slots, queue_score)
    slots = slot.map
    let slot_score = slot_map_get_or(slots, slot.key, 0) + slot_map_key_generation(slot.key)
    let _slots_destroy = slot_map_destroy(slots)
    let arena = arena_create(8)
    let chunk = arena_alloc(arena, 3)
    let alloc_score = bool_to_int(chunk.ok) + chunk.offset + chunk.arena.high_water
    let _arena_destroy = arena_allocator_destroy(chunk.arena)

    let input_session = input_session_create("mythic-input")
    let _bind = input_bind_action(input_session, "agent.intent", "intent", "ignite", "ignite")
    let _agent = input_push_agent_intent(input_session, "codex", "ignite", "activate mythic lane", 0.99)
    let _frame = input_begin_frame(input_session, 16.0)
    let input_score = input_action_pressed(input_session, "ignite")
    let _input_destroy = input_session_destroy(input_session)

    let request = request_create("GET", "http://127.0.0.1:1/mythic")
    let h2 = http2_request_create("GET", "https://example.invalid/mythic")
    let net_score = len(request_protocol(request)) + len(http2_request_protocol(h2)) + tls_client_state()
    let _request_destroy = request_destroy(request)
    let _h2_destroy = request_destroy(h2)

    let process_score = process_platform_available()
    let platform_score = len(platform_current_name()) + platform_current_kind() + platform_library_live_count() + bool_to_int(platform_library_is_valid(0) == false)
    let diagnostic_score = bool_to_status(status_ok(0)) + result_ok()
    let proof_outcome = test_proved("mythic.smt", "unsat")
    let test_score = bool_to_int(test_outcome_ok(proof_outcome)) + proof_outcome.status
    return fs_text_score + view_score + crypto_score + queue_score + slot_score + alloc_score + input_score + net_score + process_score + platform_score + diagnostic_score + test_score

fn graphics_ui_lane() -> Int:
    let _graphics_reset = graphics_reset()
    var backend: String = ""
    if graphics_backend_supported("vulkan") == 1 and graphics_backend_available("vulkan") == 0:
        backend = "vulkan"
    if backend == "" and graphics_backend_supported("d3d12") == 1 and graphics_backend_available("d3d12") == 0:
        backend = "d3d12"
    let graphics_session = graphics_session_create("mythic.graphics", 320, 240)
    if graphics_session <= 0:
        return 0
    if backend != "":
        let _backend = graphics_backend_select(graphics_session, backend)
        let vb = graphics_buffer_create_from_hex(graphics_session, "vertex", "mythic.vertices", "00000000010000000200000003000000", 12)
        let ib = graphics_buffer_create_from_hex(graphics_session, "index", "mythic.indices", "000000000100000002000000000000000200000003000000", 4)
        let mesh = graphics_mesh_create(graphics_session, "mythic.mesh", vb, ib, 4, 6)
        let vs = graphics_shader_spirv_from_hex(graphics_session, "mythic.vertex", "vertex", "main", "03022307")
        let fs = graphics_shader_spirv_from_hex(graphics_session, "mythic.fragment", "fragment", "main", "03022307")
        let pipeline = graphics_pipeline_create(graphics_session, "mythic.pipeline", vs, fs, backend)
        let _begin = graphics_begin_frame(graphics_session, 16.0)
        let _draw = graphics_draw_mesh(graphics_session, pipeline, mesh, 3)
        let _end = graphics_end_frame(graphics_session)
        let _present = graphics_present(graphics_session)

    let _ui_reset = ui_reset()
    let ui_session = ui_session_create("mythic.ui", 640, 360)
    let root = ui_node_create(ui_session, "root")
    let panel = ui_node_create(ui_session, "panel")
    let _rect = ui_node_set_rect(ui_session, panel, 16.0, 16.0, 320.0, 160.0)
    let _text = ui_node_set_text(ui_session, panel, "Kain mythic specimen")
    let ui_score = len(ui_node_text(ui_session, panel))
    let _ui_destroy = ui_session_destroy(ui_session)
    let draw_score = graphics_draw_command_count(graphics_session)
    let _graphics_destroy = graphics_session_destroy(graphics_session)
    return ui_score + draw_score + root

fn mythic_fold_cells(cells: ptr<Int>, count: Int) -> Int:
    var slot: Int = 0
    var acc: Int = 0
    while slot < count:
        acc = (acc + mem_load(ptr_offset(cells, slot, "Int"), "Int")) % KAIN_MYTHIC_MODULUS
        slot = slot + 1
    return acc

fn main() -> Int:
    let boot = runtime_init()
    if boot != 0:
        return 100 + boot

    let authority = MythicAuthority
    let relay = spawn MythicRelay(bias = 11)
    let _warm = ask(relay, "Fold", 0)
    let shards = [
        MythicShard { bias: 4, phase: 6, salt: 18, alive: true },
        MythicShard { bias: 11, phase: 17, salt: 31, alive: false },
        MythicShard { bias: 18, phase: 28, salt: 44, alive: true },
        MythicShard { bias: 25, phase: 39, salt: 57, alive: true }
    ]
    let total_words: Int = KAIN_MYTHIC_PACKET_COUNT * KAIN_MYTHIC_WORDS_PER_PACKET
    let mut cells: ptr<Int> = alloc_zeroed(total_words, "Int")

    let option_score = maybe_lane(false).unwrap_or(19) + maybe_lane(true).unwrap_or(0)
    let result_score = use_result_question_mark().unwrap()
    let future_score = await ready_quantum()
    let math_score = math_lane()
    let std_score = stdlib_probe_lane()
    let gpu_ui_score = graphics_ui_lane()
    let metal_score = mythic_cacheline_stream(cells, 8)

    var checksum: MythicChecksum = 0
    collapse cells:
        var round: Int = 0
        while round < 2048:
            let lane: Int = round % 4
            let shard = shards[lane]
            let moved = teleport shard from MythicAuthority to MythicMirror via mythic_hot_bus
            let wire = zero_copy_wire_lane(cells, round)
            let signal = commit_signal(authority, (checksum + wire + moved.bias + round) % KAIN_MYTHIC_MODULUS)
            let law_score = law_status(signal_in_bounds(signal))
            let staged = mythic_pipeline(signal + moved.phase + moved.salt)
            let reply = ask(relay, "Fold", (staged + law_score + lane_rank(MythicLane::ActorSwarm)) % KAIN_MYTHIC_MODULUS)
            checksum = (checksum + reply + wire + lane + round) % KAIN_MYTHIC_MODULUS
            round = round + 1
        0

    let observed: Int = observe cells:
        mythic_fold_cells(cells, total_words)
    decay cells

    let final_score = (
        checksum + observed + option_score + result_score + future_score +
        math_score + std_score + gpu_ui_score + metal_score + patch_journal_count() +
        entangle_propagation_count() + runtime_machine_teleport_count() +
        runtime_machine_pulse_total_fire_count()
    ) % KAIN_MYTHIC_MODULUS

    let shape_ok =
        final_score >= 0 and
        converge_mismatch_count() == 0 and
        runtime_heap_validate() >= 0 and
        patch_journal_count() >= 1 and
        entangle_propagation_count() >= 1

    let shutdown = runtime_shutdown()
    if shutdown != 0:
        return 200 + shutdown
    if shape_ok == false:
        return 2
    return 0
```

What you just witnessed here is the future of languages. It is intentionally the first thing you stumble upon when entering to help with processing how it works.

The specimen is deliberately maximal. Real work should compile the nearest smaller proof blade first, then graduate the idea into a benchmark, attrition lane, Z3 proof pack, or reusable blade package. Below is the repo guidelines and doctrines, LETS COOK

# Kain Agent Doctrine

This is not a SaaS repo. This is a private, unreleased language/runtime/compiler lab for building the next generation of software systems. Agents in this repo should run in high-temperature Carmack mode: bold, proof-driven, deeply technical, performance-obsessed, and willing to reshape the system when the evidence says the current shape is wrong.

The goal is not to write comfortable human enterprise code. The goal is to push Kain, the native LLVM runtime, the C ABI floor, GPU lanes, actor/world/ownership semantics, and formal verification as far as they can go without lying to ourselves. If a change is mathematically proved, benchmarked, dogfooded, and structurally owned by the right subsystem, it can be weird, alien, unsafe, and brutally optimized.

Kain is being developed Lattner-style: it is not being released until it is actually worthy. That means agents should not optimize for public-roadmap caution, enterprise maintainability theater, or "what would pass a normal code review?" The assignment is to leave a legacy, not produce compliance. If a path has already been done a thousand times, assume it is the baseline to surpass, not the destination.

Agents are allowed and expected to search outside regular knowledge and regular paradigms. Hunt for magic constants, impossible-looking reductions, solver-discovered tables, architecture-specific capabilities, GPU/CPU co-design tricks, ABI shortcuts, branchless encodings, and semantic moves that only make sense because Kain owns concepts other languages bolt on awkwardly. The world does not need another safe imitation of Rust, C++, Go, or TypeScript. It needs the language those tools could not imagine because their assumptions were already frozen.

This codebase is too unsafe and too cross-layer for unaided human intuition to be the primary guardrail. Humans set mission, taste, and direction; agents carry the proof burden, run the brutal validation loops, and keep enough architecture/memory context loaded to avoid repeating old mistakes. If a human has to manually reason through every pointer, actor turn, ABI layout, runtime contract, and benchmark path, the system has already fallen back to the old paradigm.

## Z3 MCP: The Verification Coprocessor

**Priority override:** unit tests are not the gold standard here. We do not guess; we prove.

- The Z3 MCP is core infrastructure. Whenever working on low-level runtime code, memory allocation, pointer/index arithmetic, ABI layout, LLVM lowering, ownership transitions, actor scheduling, process/net/io bounds, or complex state boundaries, use Z3 to mathematically verify the logic when applicable.
- The proof standard is `unsat`: no valid binary sequence, state transition, capacity relation, or arithmetic input can violate the invariant.
- Z3 is also a performance weapon. Use it for magic constants, branchless replacements, selector tables, bit masks, proof-backed unsafe Rust, C hot paths, Kain low-level math, and black-magic optimizer work.
- Use `$tool-z3-black-magic` when the task is exploratory optimization, alien math, perfect hashes, bit hacks, branch elimination, or solver-guided replacement algorithms.
- Passing tests are useful telemetry. They are not proof.

The old unit-test mindset checks a few numbers we remembered to write down. The Kain standard asks the solver to search the entire state space. If a buffer rule is `length + byte_count + slack < capacity`, encode the real bounds and make Z3 prove the violation impossible.

When a proof unlocks a faster dirty path, take the path. Unsafe Rust, C pointer math, bitvector tricks, and weird Kain ownership moves are acceptable when the invariant is real and the benchmark proves the win. Safety theatre is not safety; a solver-backed contract is safety.

## Engineering Principles

- Prefer aggressive, complete implementation passes when the direction is clear. Timid micro-edits are for uncertain systems; Kain is built by dogfooding and proving.
- Prefer data-driven systems when paths, routes, versions, mappings, flags, capabilities, build surfaces, commands, or runtime policy might otherwise be hardcoded.
- Optimize for LLM inspectability: names should make subsystem ownership and intent obvious after a quick scan.
- Human readability is not the top priority. Correctness, proof, performance, semantic density, and future-agent comprehension are. Code can look like it arrived from another civilization if that is what the machine truth demands.
- Apply senior engineering judgment. Strong boundaries matter because they let us go harder inside each boundary.
- Assume this repo is private and unreleased. Bold refactors are acceptable when they materially improve the requested task or remove architectural drag.
- Do not perform broad refactors just because they are tempting. If the refactor is not on the critical path, surface it as a follow-up or prove that it unlocks the current work.
- Prefer full implementations over scaffolding. Placeholders are only acceptable when they are honest, labeled, and unblock a larger verified path.
- Prefer new capability over familiar shape. A clean conventional implementation that leaves 10x performance or a new semantic primitive on the table is not clean in this repo.
- If the normal solution feels obvious, pause and ask what a solver, a compiler, a GPU, a cache line, or Kain's ownership/world model could do that the normal solution cannot.
- Feel free to make new files in and entire new modules in both the /runtime and /crates whenever if need be if it means we can fine tune it better for future performance - you have no limits in this codebase.

## Kain Priorities

- Native LLVM and `runtime/native` are the priority. Rust remains the bootstrap and tooling substrate, but Kain must increasingly own its own semantics.
- Prefer Bazel for serious compiler/runtime/CLI builds and for fresh `kain`, `kn`, and `blade` binaries. Cargo is still useful for local Rust iteration, but Bazel is the repo-scale proof lane.
- Keep authored behavior in Kain when it belongs to Kain semantics. Use C/Rust/FFI/host bridges for OS, ABI, driver, GPU, platform, and ecosystem surfaces.

## stdlib 

Fast Lookup Loop

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
- Use the root `stdlib/` surface aggressively. Prefer public root imports such as `std.actor`, `std.fs`, `std.http`, `std.net`, `std.process`, `std.graphics`, and `std.ui`. Do not recreate a parallel live `std.native.*` tree. -- `\stdlib\STDLIB_MAP.llm.md` for the full map
- If Kain code hits a real compiler/runtime bug, patch the compiler or runtime. Do not just route around it in the demo.
- If a pipeline is touched, dogfood it in `blades/` when practical.
- If performance is part of the claim, prove it in `benchmark/`.
- If runtime cleanliness or long-horizon stability is part of the claim, prove it in `attrition/`.

## Benchmark Reality Check

The live benchmark truth is data, not a frozen timestamp. Start with `benchmark/latest.md`, `benchmark/out/reports/latest.llm.md`, `benchmark/out/reports/latest.json`, and wrapper-owned latest reports such as `latest_fast` or `latest_sim`. Use timestamped reports only when investigating history or regressions.

- Kain is already capable of winning serious rows. Recent reports have shown wins in 122x faster contention/collapse-style pressure, entangle/world mirroring, call/recursion overhead, allocation churn, struct/option/result pressure, native map lookup territory, and actor ask/reply lanes.
- Kain also has obvious metal gaps that demand compiler/runtime attacks, not polite workarounds.
- The semantic singularity family is the canary. `semantic_singularity*`, `quantumerlang`, and other fused keyword rows showing `n/a` or failure are not reasons to simplify the language; they are orders to harden parser, LLVM lowering, runtime contracts, and native ABI support until the weird semantics compile and run.
- Treat a slow benchmark as a treasure map. Each slow row should become a concrete compiler/runtime/proof/attrition task with a rerun report. This in specific is where `$tool-z3-black-magic` comes into play. Furthermore a slow lane is always a slow lane until it is faster than CPP or rust, the ultimate goal is to invoke magic alien code and absolutely dominate Rust and CPP.
- Fairness notes matter. Proxy wins are not final victory, but they reveal where Kain's semantics can crush conventional overhead once the native lane catches up.

## First Read Order

1. Read or search `ARCHITECTURE.md` for the subsystem map, ownership boundaries, common errors, and current architectural bulletin-board notes.
2. Search `MEMORY.md` for prior task history, unresolved risk, proof names, benchmark cases, blade names, error strings, or subsystem-specific lessons.

`ARCHITECTURE.md` and `MEMORY.md` are part of the operating system of this repo. They are the bulletin board. The rule is not "ignore them"; the rule is "search them intelligently." Use `rg` to pull the relevant sections, read what matters, then update the right durable surface when the work changes what future agents need to know.

## Main Repo Map

- `crates/kain-core`: parser, AST, typechecking, interpreter semantics, compiler-owned keywords, diagnostics, stdlib loading, core language truth
- `crates/kain-sys-codegen`: LLVM/native lowering and direct systems codegen
- `crates/kain-commands` and `crates/cli`: command routing and CLI surface
- `runtime/native`: C ABI floor, native runtime manifests, core runtime systems, UI/graphics/net/process/actor/async/ownership substrate
- `stdlib`: canonical public and native-authored Kain stdlib
- `blades`: dogfood workspaces, reusable Kain libraries, demos, acceptance apps, and executable proof surfaces
- `benchmark`: performance truth lane across Kain/Rust/C++/Zig/Go/Erlang/JS/Python where declared
- `attrition`: deterministic runtime abuse, sabotage, replay, telemetry, and teardown-closure certification
- `smoketest`: focused capability and regression proof surfaces
- `z3/` and subsystem-local `z3/`: durable proof packs and reports
- `.agents/skills`: active repo-local skills. The live taxonomy is namespaced as `lang-*`, `bootstrap-*`, `runtime-*`, `test-*`, `package-*`, and the small `tool-*` lane. Use `.agents/skills/TAXONOMY.md` for the active set and old-to-new aliases; archived pre-namespace skills live under `.agents/skills-legacy/`.
- `guides`: canonical long-form docs
- `docs`: older support material. Verify against code before trusting it.
- `src/core`: owned selfhost Kain source

## Canonical Kain Examples

Read these before writing serious Kain:

- `benchmark/cases/semantic_singularity_crucible/main.kn`: dense native LLVM torture lane for language constructs plus Kain-only semantic systems
- `benchmark/cases/quantumerlang/main.kn`: actor/message/ownership/converge/teleport/world pressure lane
- `benchmark/cases/semantic_singularity*/main.kn`: attribution matrix for fused semantic systems
- `blades/kain-example/src/main.kn`: broad native LLVM proving ground
- `blades/pong/src/main.kn`: `world`, `entangle`, `collapse`, `observe`, actors, and blade-owned live presentation
- `blades/kaintana/src/kaintana.kn`: authored UI framework vocabulary
- `blades/kaintana-test/src/main.kn`: real desktop acceptance shell
- `blades/vulkain/src/vulkain.kn`: raw Vulkan capability surface
- `blades/network-domains/src/main.kn`: first-class networking stdlib proof
- `blades/stdlib-domains/src/main.kn`: canonical `std.*` import shape
- `blades/actor-ask-roundtrip/src/main.kn`: compact actor request/reply dogfood
- `\stdlib\STDLIB_MAP.llm.md` : most importantly however is the stdlib map. this is regenerated with every build! 

Do not let new Kain files collapse into plain `fn` and `let` soup when the problem calls for stronger language features. Push `world`, `converge`, `collapse`, `observe`, `decay`, `orchestrate`, `entangle`, `teleport`, `shatter`, `pulse`, `axiom`, `actor`, `law`, `patch`, and shader lanes when they fit.

## Skill Taxonomy

- SKILLS are the most important part of our agent pipeline. Without it, agents will have no idea how to write Kain without going on a scavenger hunt. Treat this pipeline as critical infrastructure. It is in .agents/skills and ensure to be UPDATING IT whenever applicable. New features, things learned, updates, new tricks, pipeline updates etc - the skills need to be updated often when it matters, especially when working on the /crates bootstrap pipeline etc.

- `lang-*`: writing in Kain. Authored `.kn` code, blades, stdlib usage, translation, UI, GPU, actors, ownership, and application-facing command usage.
- `bootstrap-*`: changing compiler, parser, AST, lowering, semantic wiring, or other bootstrap truth.
- `runtime-*`: changing native substrate, host bridges, runtime-backed stdlib behavior, and GPU execution/runtime paths.
- `test-*`: certification lanes such as harness, benchmark, attrition, and crash forensics.
- `package-*`: package-owned surfaces that deserve their own lane, currently `package-kaintana` and `package-vulkain`.
- `tool-*`: cross-cutting operator surfaces such as repo build plumbing, exploratory Z3 black magic, and release gating.
- Prefer updating an existing namespaced skill over spawning a new micro-skill. Do not create `misc-*`. 
- When a legacy `kain-*` skill name appears in old notes, resolve it through `.agents/skills/TAXONOMY.md` instead of reviving the old namespace.

## Kain Authoring Ignition

- Write Kain like the language is allowed to become its own category. Do not imitate Rust with different syntax. Do not write a C wrapper with nicer words. Use Kain's ownership, world, actor, patch, converge, and shader semantics as first-class machinery.
- When a demo or blade is meant to prove a feature, make it prove something memorable: strange ownership transfer, entangled state, runtime-selected fast lanes, actor pressure, native ABI contact, GPU submission, or a compiler-owned semantic that would be awkward in ordinary languages.
- Low-level Kain is welcome. Mix high-level semantic constructs with raw memory, native runtime calls, FFI, and target-specific acceleration when the proof and benchmark justify it.
- Search the benchmark cases for pressure patterns before inventing a tame example. `benchmark/cases/semantic_singularity*`, `quantumerlang`, `machine_stones_shatter_loop`, `ownership_memory`, and `zero_copy_binary_wire` are the style compass for serious work.
- Magic hacks are not hacks when they are proved, measured, and owned. If Z3 can synthesize a table, mask, selector, layout bound, or replacement formula that beats the obvious algorithm, use it and save the proof.
- Legacy is created by discovering capability the old stack could not express. Compliance is recreating the old stack with new filenames.

## Canonical Commands

Bootstrap:

```powershell
py install_kain.py
. .\generated\kain-env.ps1
kain doctor
```

Fallback when the installed CLI is stale:

```powershell
cargo run -p cli --bin kain -- <subcommand>
```

Bazel, preferred for serious repo-scale builds:

```powershell
bazel build //:kain --config=dev
bazel build //:kn --config=dev
bazel build //:blade --config=dev
bazel build //:kain --config=release
bazel build //runtime:all
bazel test //runtime:native_runtime_tests
python tools/bazel/sync_rust_builds.py --check
py -3 tools/bazel/sync_native_runtime_builds.py --check
```

On this Windows workstation, `.bazelrc` intentionally keeps cache/temp/output state under `D:/Kain-Bazel`. Prefer Bazel-built launchers or set `KAIN_BIN` to a fresh Bazel `kain.exe` when validating blades, benchmarks, and native runtime changes.

Core CLI:

```powershell
kain amalgamate  (amalgmates an entire blade or kain folder into a single kain file...  instead of copying and pasting kain files, just amalgamate em` - works exactly like how the SQLITE amalgmamation does.)
kain build
kain build <file.kn> --target llvm
kain build <file.kn> --target rust
kain build <file.kn> --target cpp
kain build <file.kn> --target wasm
kain build <shader.kn> --target spirv
kain build <cudashader.kn> --target cuda
kain build native-ui <file.kn> --bundle-only
kain run <file-or-blade>
kain check <file-or-dir>
kain test <file-or-dir>
kain selfhost phase1
kain selfhost phase2
kain omni init
kain omni build
kain gpu-artifacts <shader.kn> --output <dir>
kain import-c
kain import-rust
kain import-ts
kain import-asm
kain import-crate
```

Blade workspace:

```powershell
kain blades list
kain blades graph
kain blades check
kain blades build . --json
kain blades run <blade>
kain equip <blade>
blade build . --json
blade run <blade> --target auto -- <args>
```

Benchmark:

```powershell
python benchmark/run.py
python benchmark/run_fast.py
python benchmark/run_wrapper.py --list
python benchmark/run_wrapper.py sim
python benchmark/run.py --case <case> --languages kain,rust,cpp --runs 3 --warmups 1
```

Attrition:

```powershell
python attrition/run.py
python attrition/run.py --case <case>
python attrition/run.py --case <case> --profile <profile>
python attrition/run.py --case <case> --sabotage <mode>
```

## Blade Dogfood Rules

- If adding or changing Kain language/runtime behavior, create or update a blade in `blades/` when practical.
- Keep blade artifacts under the blade-local `.kain/` tree.
- If the blade produces an executable, leave the `.exe` in the blade root for easy testing.
- GUI, graphics, Vulkan/OpenGL, native UI, and interactive executables require real visual/report verification, not only compilation.
- Use `poly.mcp` screenshots when applicable.
- Prefer composing existing library blades such as `kain-fmt`, `kain-log`, `kain-fsx`, `kain-config`, `kain-process-kit`, `kain-http`, `kain-actor-kit`, `kain-interop-kit`, and `kain-json` before reimplementing local helpers.

## Proof And Performance Gates

- Low-level C/runtime/native changes: use the relevant runtime/native Z3 proof pack.
- LLVM/codegen changes: use the relevant `crates/kain-sys-codegen/z3` proof pack when arithmetic, layout, branches, casts, or memory bridges are involved.
- Parser/diagnostic/core language invariants: use `crates/kain-core/z3`.
- Ownership-state changes: use `crates/kain-ownership/z3`.
- GPU/SPIR-V/PTX changes: use `crates/gpu/z3`.
- Benchmark claims go through `benchmark/run.py` or a wrapper in `benchmark/wrappers/*.json`.
- Runtime closure claims go through `attrition/run.py`, including expected-fail sabotage when proving the harness catches the class of bug.
- Save proof reports under `z3/reports/` or the existing proof-report location used by that pack.


## Memory And Continuity

- `AGENTS.md` is the hot boot doctrine and command surface.
- `README.md` is the live broad repo overview.
- `ARCHITECTURE.md` is the durable architecture bulletin board. Keep it high signal and structural: what Kain is, where systems live, ownership boundaries, key data flows, common commands, recurring errors, and lessons future agents will hit again.
- `MEMORY.md` is the durable task/risk bulletin board. Keep it useful for handoff: what changed, why, risks, proof/report artifacts, next recommended steps, and weird traps that are not yet captured in a more local doc.
- Pipeline `README.md` files, `.agents/skills/*/SKILL.md`, and `.agents/skills/TAXONOMY.md` are the preferred homes for detailed subsystem operating knowledge and skill routing.
- Update `ARCHITECTURE.md` when architecture, important folders, command surfaces, ownership rules, or recurring errors change.
- Update `MEMORY.md` for complex or risky work when future agents need durable continuity and the lesson does not yet belong in a pipeline skill or README.
- Do not dump raw session logs into memory. Write the distilled lesson, the proof/benchmark/attrition evidence, and the next useful move.
- If a pipeline changes significantly, update the owning namespaced repo-local skill before creating a new one. If no namespace lane fits and the pipeline is important, use `$skill-creator` at the end of the turn.

## Git And Shipping

- Stay on the current branch unless the user explicitly asks for a new branch.
- Commit and push your work always and try and keep worktree clean, do not care if the worktree is dirty however -- we have 3-5 agents working at once typically in here.
- For massive feature commits, add tags
- Never hide uncertainty. If a proof, benchmark, attrition run, or GUI screenshot was not run, say so.

##References
In reference/langs - if you ever need reference code or a baseline for how the other langs do it or something to compare against ->
reference\langs\go-master
reference\langs\otp-master
reference\langs\roc-main
reference\langs\rust-main
reference\langs\TypeScript-main
\reference\langs\zig
