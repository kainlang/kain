# Kain Glossary

This file is the live repo dictionary for Kain. It is not a tutorial. Each entry answers one question: what is this thing, and where does it live?

Location rule: paths in this glossary are live in this checkout. Older docs may still mention historical paths that are missing here; treat this file plus the actual folder tree as current truth.

## A

**`.agents/skills`** - The repo-local skill tree for agents. This is the behavior routing surface for authored Kain, bootstrap work, runtime work, testing lanes, package-owned surfaces, and tooling. Location: `.agents/skills/`.

**ABI floor** - The stable native C contract surface under `runtime/native`. It is the machine-facing substrate Kain lowers into without letting C define Kain semantics. Location: `runtime/native/`, especially `runtime/native/include/` and `runtime/native/src/core/`.

**Actor** - A first-class Kain stateful message-turn unit declared with `actor`. Language meaning lives in `crates/core` and `crates/actor`; native execution lives in the actor runtime substrate. Location: `crates/core/`, `crates/actor/`, `runtime/native/include/actor.h`, `runtime/native/src/core/actor.c`, `stdlib/actor.kn`.

**AGENTS.md** - The repo's agent doctrine, north-star specimen, command map, and operating rules. If an agent needs to know how to behave here, this is the social contract. Location: `AGENTS.md`.

**Amalgamate** - Kain's portable capsule lane for packing a file, blade, or whole workspace into a single `.kn` artifact. It keeps source searchable and materializes back into a real workspace under `.kain/cache/amalgamate/...`. Location: `crates/amalgamate/`, `crates/cli/src/amalgamate.rs`, `blades/amalgamate-capsule-probe/`.

**ARCHITECTURE.md** - The durable subsystem map and ownership document for the current checkout. Use it to understand folder ownership, main crates, runtime shape, and validation surfaces. Location: `ARCHITECTURE.md`.

**Axiom** - A compiler-owned machine-truth declaration. Use it to say a fast lane or semantic assumption only exists when target, arch, or capability predicates are true, plus a fallback when they are not. Location: language truth in `crates/core/`; native substrate in `runtime/native/include/machine_stones.h` and `runtime/native/src/core/machine_stones.c`.

**Attrition** - Kain's deterministic runtime abuse and teardown-certification lane. Benchmark asks "how fast is it"; attrition asks "does it stay structurally clean under stress, sabotage, replay, and shutdown." Location: `attrition/`.

## B

**Bazel** - The serious repo-scale build lane for fresh `kain`, `kn`, compiler, and runtime binaries. Cargo is still used for local Rust iteration, but Bazel is the heavyweight truth lane. Location: `BUILD.bazel`, `MODULE.bazel`, `tools/bazel/`, `.bazelrc` behavior described in `AGENTS.md`.

**Benchmark** - The performance truth lane under `benchmark/`. This is where Kain rows fight C++, Rust, Zig, Go, Erlang, JS, and Python on measurable work instead of vibes. Location: `benchmark/`.

**Blade** - A legacy Kain package/app term that still survives in internal discovery and compatibility plumbing, usually around projects under `blades/`. It is no longer the preferred public CLI mental model; `build.kn` project authority and package/project wording are the modern surface. Location: `blades/`, compatibility/discovery helpers in `crates/blades/`.

**bootstrap-semantic-authoring** - The repo-local skill for growing the semantic diagnostic error corpus with intentionally broken Kain fixtures, expected diagnostic metadata, generation/verification scripts, and optional error-system wiring updates. Location: `.agents/skills/bootstrap-semantic-authoring/`; corpus/scripts live in `crates/semantic/error_corpus/` and `crates/semantic/scripts/`.

**build.kn** - The preferred project-authority file for modern Kain workspaces. It is Kain source, not shell glue: package metadata, run defaults, platform requirements, evidence DAG tasks, and certification gates belong here. Location: blade roots, project roots, and `stdlib/build.kn` plus `docs/pipelines/build-kn-evidence-dag.md`.

## C

**C ABI** - The native foreign-function boundary Kain uses for runtime services, OS contracts, platform SDKs, and external libraries. In authored Kain this normally appears as `use c::...`; local header/source pairs can use the natural alias-aware form `include native/header.h as alias`, and known system/vendor headers can use registry-backed angle includes such as `include <stdio.h> as cstdio`, `include <math.h> as cmath`, `include <sys/mman.h> as posix`, `include <windows.h> as win`, or `include <vulkan/vulkan.h> as vk` without first writing a blade-local bridge manifest. Angle-bracket includes still need compiler-owned linker policy for the owning SDK family, so the current system lane is data-driven through `crates/c-ffi/system_headers.toml` rather than an arbitrary machine-wide header search. Location: import lane in `crates/c-ffi/`; native floor in `runtime/native/include/`.

**Capsule** - A portable `.kn` container produced by `kain amalgamate`. Editable capsules keep file blocks inline; archive capsules keep a sealed payload block. Location: owned by `crates/amalgamate/`; materialized under `.kain/cache/amalgamate/`.

**Collapse** - The exclusive ownership mutation scope in Kain. It says a region is being mutated under an owned, single-writer phase. Location: language truth in `crates/core/`; ownership model in `crates/ownership/`; native ownership substrate in `runtime/native/`.

**Compiler-owned intents** - The semantic declaration family Kain treats as first-class language machinery instead of library sugar. The main set is `law`, `patch`, `converge`, `world`, `entangle`, and `orchestrate`, with machine-stone siblings such as `axiom`, `pulse`, `shatter`, and `teleport`. Location: `crates/core/`, `crates/sys-codegen/`, `runtime/native/`, `stdlib/intent.kn`.

**Conformance** - The runtime-facing proof and contract lane that validates ABI and subsystem behavior below authored Kain demos. Location: `runtime/conformance/`.

**Converge** - Kain's spec-plus-fast-lanes construct. A `converge` block defines one reference lane and one or more target or capability gated fast lanes, then verifies the fast lanes against the spec lane. Location: language truth in `crates/core/`; LLVM lowering in `crates/sys-codegen/`; runtime selector in `runtime/native/include/converge.h`; public helpers in `stdlib/runtime.kn`.

**Crates** - The Rust workspace where compiler, runtime bridges, import lanes, UI/GPU systems, and toolchain support live. If Kain source meaning changes, the relevant owning crate is usually here. Location: `crates/`.

**CUDA/PTX lane** - The NVIDIA-specific GPU path for Kain compute shaders. Authored kernels can import `std::cuda` for device intrinsics such as lane id, warp collectives, cp.async group gates, and tensor-core capability assertions; lowering lives in the PTX backend and execution lives in the NVIDIA driver runtime. Location: `stdlib/cuda.kn`, `crates/gpu/src/codegen_ptx.rs`, `crates/gpu/src/ptx_module.rs`, `crates/gpu-runtime/src/nvidia_ptx.rs`, `benchmark/lanes/gpu/run_cuda.py`.

## D

**Decay** - The deterministic ownership teardown operation in Kain. If `collapse` is exclusive mutation and `observe` is scoped reading, `decay` is the terminal cleanup step. Location: `crates/core/`, `crates/ownership/`, `runtime/native/`.

**Defer** - A block-scoped cleanup statement written `defer expr`. The deferred expression runs in strict LIFO order when the block exits by fallthrough, `return`, `break`, or `continue`; return and break payloads are evaluated before cleanup starts. Location: language truth in `crates/core/`; LLVM cleanup lowering in `crates/sys-codegen/`.

**Dispatch statement** - A host-side GPU launch statement written `dispatch "compute.key" [x, y, z]`. The compute key stays backend-agnostic, and the three dimensions override artifact or metadata dispatch defaults for that launch. Location: parsing and lowering in `crates/core/` and `crates/sys-codegen/`; runtime service contract in `crates/gpu-runtime/` and `runtime/native/include/cuda_runtime.h`.

**Docs** - The live long-form documentation tree for this checkout. Use it for conceptual explanations, CLI behavior, runtime docs, guide maps, and reference pages, but verify against source when something smells stale. Location: `docs/`.

## E

**Entangle** - A compiler-owned state-coupling declaration between world fields. In practice it is how one world mirrors or propagates state into another under explicit policy such as `single_writer`. Location: `crates/core/`, `crates/entangle/`, `runtime/native/include/entangle.h`, `runtime/native/src/core/entangle.c`, `stdlib/intent.kn`.

## F

**Fabric** - Kain's local-first polyglot manifest lane. Use it when a workflow genuinely needs mixed-language execution contracts; do not confuse it with Kain's preferred native project-authority story, which is now `build.kn`. Location: CLI/docs lane under `docs/cli/selfhost-omni-fabric-lsp.md` and pipeline docs under `docs/pipelines/fabric.md`.

**Foreign ABI** - The normalized type and bridge model owned by `crates/foreign-abi`. It classifies scalars, pointers, callbacks, aggregates, ownership tags, and bridge safety across import lanes. Location: `crates/foreign-abi/`.

## G

**GPU lane** - The authored shader and graphics path in Kain. Shader code compiles through the SPIR-V/PTX/HLSL artifact pipeline, while host orchestration stays in normal Kain and lowers through LLVM plus runtime ABI calls. Location: `crates/gpu/`, `crates/gpu-runtime/`, `stdlib/gpu.kn`, `stdlib/graphics.kn`, `stdlib/graphics_shared.kn`, `benchmark/lanes/gpu/`, `blades/vulkain/`.

**Generic where clause** - A contextual `where` clause on generic-bearing items such as functions, structs, enums, traits, impls, and type aliases. The AST stores it separately from inline bounds so semantic passes can normalize and validate all constraints together. Location: `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/types.rs`.

## I

**Interop** - The layer where Kain crosses into native, OS, vendor, or host-language boundaries. Kain should still own policy and semantics here; C, DLLs, SDKs, and handles should stay tight and boring. Location: `crates/c-ffi/`, `crates/foreign-abi/`, `crates/crate-ffi/`, `crates/interop/`, `mcp/`, `stdlib/platform.kn`, `stdlib/c/`, `stdlib/interop/`.

## K

**KAIN.toml** - The older compatibility manifest. It still matters, especially for leftover metadata not yet promoted into `build.kn`, but it is no longer the mental center of Kain project design. Location: blade and project roots where compatibility metadata still exists.

**Kaintana** - The authored Kain UI framework family under `blades/kaintana*`. Think "Kain-owned desktop/UI vocabulary and acceptance surfaces," not "generic runtime UI substrate." Location: `blades/kaintana/`, `blades/kaintana-test/`, `blades/kaintana-vulkan/`, `blades/kaintana-vulkan-test/`.

**kain-3D** - The native 3D renderer and viewport runtime crate. This is the 3D runtime lane above the raw graphics ABI floor. Location: `crates/3d/`.

**kain-actor** - The reusable actor-system model crate. It owns typed actor IDs, message contracts, mailbox/scheduler metadata, lifecycle/supervision structures, and native ABI descriptors consumed by `kain-core`. Location: `crates/actor/`.

**kain-blades** - The crate that discovers blade roots, resolves package metadata, and feeds workspace graph/build/run behavior. It is the package graph brain for local Kain workspaces. Location: `crates/blades/`.

**kain-build** - The workspace build planner and evidence DAG executor. It turns blade/workspace authority into typed build tasks, artifacts, caches, and reports. Location: `crates/build/`.

**kain-c-ffi** - The C import lane behind `use c::...`, `kain import-c`, and `kain import platform`. It extracts headers, classifies symbols, emits Kain extern modules, and handles bridge metadata. Location: `crates/c-ffi/`.

**kain-check** - The reusable source-checking crate behind `kain check`. Keep this separate in your head from CLI wrappers. Location: `crates/check/`.

**kain-commands** - The command-routing brain for `kain`, `kn`, and `blade`. It owns command registries and argument surfaces, not domain implementation logic. Location: `crates/commands/`.

**kain-core** - The language truth crate. Parser, AST, typechecker, interpreter/runtime semantics, comptime, stdlib loading, diagnostics, and compiler-owned feature meaning all live here. Location: `crates/core/`.

**kain-driver** - The artifact materialization and target-orchestration crate. It owns runtime contracts, shader bundles, native app packaging sidecars, and emitted bundle truth. Location: `crates/driver/`.

**kain-entangle** - The deterministic state-coupling primitive crate behind compiler-owned entangle metadata and policy. Location: `crates/entangle/`.

**kain-foreign-abi** - The shared type graph and pointer/callback/aggregate classifier used by foreign import lanes. This is where ABI shape gets normalized before bridges are generated. Location: `crates/foreign-abi/`.

**kain-fs** - The portable filesystem contract crate. Root stdlib `std::fs` and several build/run surfaces reduce through this model instead of inventing ad hoc path behavior. Location: `crates/fs/`, public authored surface `stdlib/fs.kn`.

**kain-gpu-runtime** - The concrete GPU execution layer that consumes emitted shader bundles and residency metadata. Vulkan is the portable compute path; PTX is the NVIDIA-specific derived lane. Location: `crates/gpu-runtime/`.

**kain-input** - The portable input contract crate. It owns typed events, frame reduction, action/axis bindings, replay, and first-class `agent.intent` input semantics. Location: `crates/input/`, public authored surface `stdlib/input.kn`.

**kain-net** - The portable networking contract crate. Root `std.net`, `std.http`, `std.tls`, and `std.http2` ultimately reduce through this contract plus native ABI surfaces. Location: `crates/net/`, public authored surfaces `stdlib/net.kn`, `stdlib/http.kn`, `stdlib/tls.kn`, `stdlib/http2.kn`.

**kain-ownership** - The portable ownership-state kernel behind `collapse`, `observe`, and `decay`. It owns the state machine and conservative region policy consumed by authored Kain and native lowering. Location: `crates/ownership/`.

**kain-process** - The portable child-process and PTY contract crate. Root `std.process` rides on this model plus native process ABI surfaces. Location: `crates/process/`, public authored surface `stdlib/process.kn`.

**kain-run** - The unified immediate execution planner behind `kain run`, `kain run dev`, and `kain watch`. It resolves files, projects, manifests, Cargo/C/Node/Fabric inputs, and run metadata through one pipeline, while still carrying some legacy blade compatibility internally. Location: `crates/run/`.

**kain-semantic** - The reusable semantic coprocessor crate for compiler diagnostics, corpus intelligence, and oracle tooling. It owns the CPU compiler-side semantic pack consumer, the offline Kain/CUDA oracle forge, and domain failure modes for typo, import, shader, CUDA/PTX, Python interop, C ABI, ownership, world, converge, parser, and actor diagnostics. This is the primary crate path after the `error-semantic` rename. Location: `crates/semantic/`.

**kain-error-semantic** - Compatibility shim crate that re-exports `kain-semantic` for legacy dependency paths. Use this only for transition; new code should depend on `kain-semantic` directly. Location: `crates/error-semantic/`.

**kain-stdlib-map** - The crate behind `kain stdlib-map`. It generates the root stdlib atlas consumed by agents, docs, and build checks. Location: `crates/stdlib-map/`.

**kain-sys-codegen** - The backend emitter crate. LLVM lowering lives here, along with other codegen lanes such as direct C and target-specific lowerings. Location: `crates/sys-codegen/`.

**kain-test** - The reusable source test harness crate behind `kain test`. This is the compiletest-style certification lane, not just ad hoc unit tests. Location: `crates/test/`.

**kain-ui** - The semantic UI graph and patch-oriented UI meaning crate. This is the authored UI meaning layer above the raw UI ABI floor. Location: `crates/ui/`.

**kain-ui-native** - The authored native desktop host/runtime lane above the raw UI ABI floor. It should project authored Kain UI intent, not become a second host-owned widget catalog. Location: `crates/ui-native/`.

## L

**Law** - A compiler-owned invariant predicate that must return `Bool`. Laws are the semantic "this condition is part of the contract" surface, not just ad hoc `if` statements. Location: `crates/core/`, public status helpers in `stdlib/intent.kn`.

**Library of Kain** - The repo's dense authored example corpus. Use it as a vocabulary and proof surface for what Kain files can look like without loading giant subsystems first. Location: `library_of_kain/`.

## M

**Machine stones** - The machine-facing semantic quartet `axiom`, `pulse`, `shatter`, and `teleport`, plus the native substrate that backs them. This is where Kain talks directly about machine truth, cadence, layout intent, and destructive handoff. Location: `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c`, public counters in `stdlib/runtime.kn`.

**MCP** - The repo's model-context-protocol workspace lane and root `std::mcp` stdlib surface. This is where the permanent repo MCP server is meant to live instead of only treating MCP as an experimental blade. Locations: `mcp/`, especially `mcp/kain-agent-mcp/`, and `stdlib/mcp.kn`.

**MEMORY.md** - The repo's durable task memory board. Search it for prior fixes, proof names, regressions, commands, and subsystem lessons before you rediscover history the hard way. Location: `MEMORY.md`.

## O

**Observe** - The read-only ownership observation scope in Kain. It says a region can be inspected without taking exclusive mutation ownership. Location: `crates/core/`, `crates/ownership/`, `runtime/native/`.

**Omni** - Kain's mixed-language orchestration lane. Use it when the job is explicitly multi-runtime or staged across ecosystems, not when a normal Kain project should just use `build.kn`. Location: CLI/docs lane under `docs/cli/selfhost-omni-fabric-lsp.md` and pipeline docs under `docs/pipelines/omni.md`.

**Orchestrate** - Kain's compiler-owned typed stage graph for silicon and semantic state management. Current stage kinds include `kain`, first-class `c`/`python`, silicon-native `cpu`, `gpu`, `dispatch`, `converge`, `law`, `patch`, and `world`, plus compatibility adapters `rust` and `node`. The contextual `stage name: kind function(args) when capability("...")` form preserves selectors in runtime contracts, realtime bundles, LLVM telemetry, and `std::intent` counters. Location: `crates/orchestrate/`, `crates/core/`, `crates/sys-codegen/`, `runtime/native/`, and `stdlib/intent.kn`.

**Ownership** - Kain's first-class region-state model behind `collapse`, `observe`, and `decay`. Portable ownership truth lives in `crates/ownership`; runtime enforcement lives in native ownership substrate code. Location: `crates/ownership/`, `runtime/native/`.

## P

**Package** - A stable first-party artifact intended to graduate beyond the blade proving ground. In repo terms, `blades/` are the forge; `packages/` are the official surfaces meant to become real ecosystem inventory. Location: `packages/`.

**Patch** - A compiler-owned journaled mutation declaration. It is the "intentional state change with runtime-tracked semantics" surface, not just a helper function that mutates something. Location: `crates/core/`, public helpers in `stdlib/intent.kn`.

**Platform package** - A deterministic native SDK import lane managed through `kain import platform`, generated locks, and package metadata under `.kain/platform/...`. Use this when Kain needs a real vendor or system SDK without devolving into raw loader chaos. Location: import tooling in `crates/c-ffi/`; public platform helpers in `stdlib/platform.kn`; proof blade `blades/platform-package-smoke/`.

**Proof pack** - A durable Z3-backed invariant collection stored near the subsystem it protects. Kain uses proof packs to make unsafe math, bounds, layout, and state-machine claims machine-checkable. Location: root `z3/` plus subsystem-local packs such as `crates/core/z3/`, `crates/ownership/z3/`, `crates/sys-codegen/z3/`, `crates/gpu/z3/`, and `runtime/native/src/core/z3/`.

**Pulse** - Kain's first-class temporal beat declaration. It is how the language talks about recurring cadence, jitter, missed beats, and time-driven work as semantics instead of hand-written loops. Location: `crates/core/`; native substrate in `runtime/native/include/machine_stones.h` and `runtime/native/src/core/machine_stones.c`.

## Q

**`query_stdlib.py`** - The fast stdlib lookup helper. New agents should use this before manually opening the huge stdlib atlas. Location: `query_stdlib.py`.

## R

**Reference** - The donor/reference corpus for external code, harvested material, and salvage surfaces. This is useful for research and assimilation, not as automatic truth. Location: `reference/`.

**Research** - The repo's frontier-notes surface for optimization hunts, semantic frontier plans, and subsystem investigations. If a performance or architecture idea is not yet a landed subsystem truth, it usually starts here. Location: `research/`.

**Runtime blades** - The Kain-authored runtime-policy workspace above the raw C ABI floor. Use this phrase when talking about authored runtime policy, not the low-level native substrate itself. Location: `runtime/blades/`.

**Runtime contract** - The structured semantic metadata emitted from `kain-core` that downstream runtimes and codegen consume. It is how worlds, laws, patches, converges, shaders, machine-stones, and other compiler-owned features stay explicit after parsing. Location: emission in `crates/core/`; consumed by `crates/driver/`, `crates/sys-codegen/`, and runtime lanes.

**runtime/conformance** - The low-level C and runtime ABI proof surface for native behavior. If a runtime contract claim needs substrate-level validation, it lands here rather than only in authored Kain demos. Location: `runtime/conformance/`.

**runtime/native** - The canonical C runtime and ABI floor. This is where startup, service contracts, actor substrate, ownership guards, machine-stones, graphics/UI/network/process ABI, and other low-level host contact live. Location: `runtime/native/`.

## S

**Scripts** - The repo's operational helper tree. This is where docs helpers, Kain automation, platform scripts, Python utilities, Rust helpers, and test utilities live. Location: `scripts/`.

**Selfhost** - The lane where Kain increasingly owns more of its own semantics and project pipeline instead of depending on bootstrap Rust forever. It is the direction of travel, not a marketing word. Location: command surface in `kain selfhost ...`; historical and mirror references still appear through docs and imported source corpora.

**Semantic pack** - A frozen offline artifact bundle consumed by the compiler semantic coprocessor on CPU. The current v1 shape is `manifest.json`, `prototypes.bin`, and `reranker.i8`; CUDA/Kain forge can build richer data offline, but shipped `kain check` must run without CUDA and fall back to rules when the pack is absent. The pack now carries domain repair prototypes for Python imports, C ABI/native imports, CUDA/PTX contracts, shader host boundaries, shader resource contracts, and core Kain semantic surfaces rather than only typo examples. Location: producer/consumer in `crates/semantic/src/pack.rs`; dev artifact root `crates/semantic/.kain/oracle/sempack/current`.

**Shatter** - Kain's structure-of-arrays layout intent, written as `shatter struct`. It says the language should preserve lane-wise hot data shape instead of pretending every hot structure wants vanilla AoS layout. Location: language truth in `crates/core/`; native substrate in `runtime/native/include/machine_stones.h` and `runtime/native/src/core/machine_stones.c`.

**Smoketest** - The primary mixed-surface proof album under `smoketest/`. It is where many Kain features are meant to compile and interact together through one connected workspace, not as isolated toy snippets. Validate the album through `kain build smoketest` so `smoketest/build.kn` owns the DAG; direct `kain run smoketest/src/main.kn` is only a focused debug lane and can leave misleading generated telemetry under `smoketest/src/telemetry/`. Location: `smoketest/`, especially `smoketest/src/` and `smoketest/build.kn`.

**SPIR-V lane** - The canonical GPU shader artifact path. In Kain terms, SPIR-V is the primary shader output, while HLSL and PTX sidecars are derived lanes. Location: `crates/gpu/`, `kain gpu-artifacts`, `benchmark/lanes/gpu/`, `blades/vulkain/`.

**Stdlib** - The public root `stdlib/*.kn` surface exposed through imports like `std::fs`, `std::math`, `std::gpu`, `std::graphics`, `std::ui`, and `std::runtime`. New authored Kain code should prefer these root imports over private or native-only paths. Location: `stdlib/`.

**STDLIB_MAP** - The generated atlas at `stdlib/STDLIB_MAP.llm.md` plus `stdlib/stdlib.map.json`. It is the one-scan map of the native root stdlib and should be queried, not hand-maintained. Location: `stdlib/STDLIB_MAP.llm.md`, `stdlib/stdlib.map.json`.

## T

**Teleport** - Kain's destructive cross-world handoff expression. It is a zero-copy style ownership transfer between worlds, not a normal clone or assignment. Location: `crates/core/`; native substrate in `runtime/native/include/machine_stones.h` and `runtime/native/src/core/machine_stones.c`.

## U

**UE5** - The Unreal-facing target and historical DNA strand of Kain. It is still an important adapter family and authoring influence even though the core language/runtime now matter more than UE-specific generation. Location: `crates/ue5*/`, `unreal/`, `stdlib/ue5/`, and `docs/ue5/`.

**UI substrate** - The low-level session/node/event/resource ABI exposed through `std::ui` and the native UI system. It is intentionally generic so authored Kain UI frameworks can sit above it instead of being hardcoded into the host. Location: `stdlib/ui.kn`, `runtime/native/include/ui_system.h`, `runtime/native/src/ui/`.

## V

**Vulkain** - The raw Vulkan package and blade family. Use it when you need an actual presentable Vulkan window or bridge today; do not confuse it with the generic runtime graphics ABI. Location: `blades/vulkain/`, plus dependent blades such as `blades/kaintana-vulkan/`.

## W

**Website** - The repo's website and packaging/registry-facing web application tree. This is adjacent infrastructure, not the core language/runtime truth surface. Location: `website/`.

**Workgroup clause** - Static compute-shader local geometry declared in the shader header as `workgroup(x, y, z)`. It is canonical shader artifact truth, not dynamic dispatch size; host `dispatch [x, y, z]` controls grid dimensions separately. Location: parser and metadata truth in `crates/core/`; shader backend consumption in `crates/gpu/`.

**World** - A compiler-owned state authority or projection surface declared with `world`. Worlds hold named state slots and surfaces such as `native_ui`, `web`, `viewport3d`, or `ue5`, and they become the backbone for `entangle`, `patch`, and `teleport`. Location: `crates/core/`, with runtime-facing helpers in `stdlib/intent.kn` and related runtime contracts.

## Z

**Z3** - The repo's proof engine for unsafe invariants, bounds, layout arithmetic, actor/runtime state machines, ownership transitions, and optimizer black magic. In Kain culture, tests are telemetry; proofs are contracts. Location: root `z3/`, subsystem-local `z3/` packs, and `z3-mcp/`.

**Z3 MCP** - The solver coprocessor server and workflow layer used for proof-backed validation, counterexample hunting, and reusable verification flows. Location: `z3-mcp/`.
