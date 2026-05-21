# Benchmark Translation Compass

Use this as a style compass, not as frozen benchmark truth. Regenerate the ranking from the latest report before starting serious translation work:

```powershell
py .agents\skills\lang-translation\scripts\select_translation_examples.py --repo . --top 10
```

Snapshot source: `benchmark/out/reports/20260520T005049Z.json`, generated `2026-05-20T00:41:49Z`, `refresh-foreign`, 7 timed runs. Ranked by Kain beating both Rust and C++ on median time; `min x` is the smaller of Rust/Kain and C++/Kain.

| rank | case | maturity | min x | Kain ms | Rust ms | C++ ms | Kain source | Translation move to study |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| 1 | `contention_wall` | proxy | 168.99 | 9.810 | 1657.722 | 1698.361 | `benchmark/cases/contention_wall/main.kn` | `collapse` over owned memory as the Kain concurrency destination; keep the proxy caveat visible. |
| 2 | `json_manual_roundtrip` | implemented | 10.17 | 10.223 | 120.563 | 103.961 | `benchmark/cases/json_manual_roundtrip/main.kn` | Keep a literal/schema parser as spec, route LLVM through a target-gated `converge` fast lane. |
| 3 | `ray_sphere_intersection` | implemented | 9.10 | 8.863 | 83.512 | 80.628 | `benchmark/cases/ray_sphere_intersection/main.kn` | Preserve scalar geometry semantics, then use a proof-backed closed-domain native lane. |
| 4 | `zero_copy_binary_wire` | implemented | 8.17 | 10.838 | 94.696 | 88.586 | `benchmark/cases/zero_copy_binary_wire/main.kn` | Packed records, `ptr_offset`, `mem_load/store`, `collapse/observe/decay`, and layout proofs. |
| 5 | `simd_lane_mix` | implemented | 5.58 | 8.727 | 74.830 | 48.706 | `benchmark/cases/simd_lane_mix/main.kn` | Scalar reference plus SIMD/native lane behind `converge`, with pointer arrays owned explicitly. |
| 6 | `ecs_archetype_query` | implemented | 4.31 | 10.092 | 48.434 | 43.464 | `benchmark/cases/ecs_archetype_query/main.kn` | `shatter struct` data and periodic residue reduction instead of replaying every ECS iteration. |
| 7 | `allocator_large_object_churn` | implemented | 3.69 | 11.545 | 42.649 | 43.738 | `benchmark/cases/allocator_large_object_churn/main.kn` | Model allocation lifetime directly with `alloc_zeroed`, `collapse`, `observe`, and `decay`. |
| 8 | `ghost_mirror` | semantic-proxy | 3.49 | 9.910 | 34.588 | 37.809 | `benchmark/cases/ghost_mirror/main.kn` | Replace serialization/transport mirror boilerplate with `world` and `entangle` where semantics permit. |
| 9 | `call_chain` | implemented | 2.03 | 14.972 | 31.095 | 30.377 | `benchmark/cases/call_chain/main.kn` | Keep the function-call spec but let `converge` carry a proved affine recurrence fast lane. |
| 10 | `native_map_lookup` | implemented | 1.90 | 16.988 | 32.196 | 35.561 | `benchmark/cases/native_map_lookup/main.kn` | Prefer native map/stdlib-backed lookup semantics over rebuilding foreign hashmap scaffolding. |

## How To Use These Cases

- Read the Kain file and at least one donor file (`main.rs` or `main.cpp`) for the matching shape.
- Copy the structural move, not the constants. Examples: "spec plus fast lane", "world mirror instead of socket mirror", "bounded pointer layout with proof", or "periodic residue collapse".
- Preserve `maturity`, `fairness_note`, and language caveats when using a benchmark as evidence.
- For direct application code, prefer implemented rows over proxy rows. Proxy rows are still valuable because they show the semantic direction Kain wants to graduate into.
- If the translation touches MCP/tooling rather than numeric kernels, combine this compass with `blades/kain-mcp`, `stdlib/STDLIB_MAP.llm.md`, and the source-specific inventory.

## Extra Exemplars Worth Opening

- `benchmark/cases/semantic_singularity_crucible/main.kn`: dense Kain-only syntax and runtime pressure.
- `benchmark/cases/quantumerlang/main.kn`: actor/message/ownership/converge/teleport/world pressure.
- `benchmark/cases/stdlib_foundations/main.kn`: current root stdlib usage across text, collections, crypto, and alloc.
- `blades/kain-example/src/main.kn`: broad native LLVM proving ground.
- `blades/stdlib-domains/src/main.kn`: public root `std.*` import shape.
- `blades/network-domains/src/main.kn`: net/http/tls/http2 plus route-to-actor proof.
- `blades/vulkain/KAIN.toml` and `blades/vulkain/examples/mesh-scene/src/main.kn`: `use c::...` plus `[c_ffi]` ABI bridge shape.
- `blades/kaintana/src/main.kn`: Kain-authored desktop UI framework composition.
- `blades/pong/src/main.kn`: world/entangle/actor state lattice with native presenter bridge.
- `blades/actor-ask-roundtrip/src/main.kn`: compact actor ask/reply syntax proof.
- `blades/hash-domains/src/main.kn`: focused `std::hash` primitives and invariants.
