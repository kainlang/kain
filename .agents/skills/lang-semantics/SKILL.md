---
name: lang-semantics
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain language-side semantics and feature usage across the authored `.kn` surface: modules/imports, functions/effects, data types, components/JSX, shaders/compute/comptime plans, actors, async, ownership and raw memory expressions, world/entangle/patch/law, converge/orchestrate, axiom/pulse/shatter/teleport, tests, macros, material/graph/editor/gameplay DSLs, and source-code anchors for how those Kain features work. Use this when writing IN Kain, not when changing parser/typechecker/lowering/runtime internals.
---

# Lang Semantics

This skill is the Kain language field manual. Use it to write real Kain, preserve Kain-native semantics, and know where each authored feature is owned in the repo before escalating to bootstrap/runtime work.

## Prime Directive

- Write Kain as Kain. Do not flatten semantic features into plain `fn` and `let` soup when the problem fits a stronger construct.
- Treat source anchors below as ownership indexes, not mandatory read orders. If paths move, search by symbol/name with `rg`.
- Start from nearby examples, then author the smallest compileable proof before graduating to blades, benchmarks, attrition, or Z3.
- If authored Kain exposes a compiler/runtime defect, preserve the semantic design and hand off to `bootstrap-*` or `runtime-*`; do not silently route around the feature.
- Keep language work in this skill. If you need to change `crates/core`, `crates/sys-codegen`, `runtime/native`, or `crates/gpu`, co-trigger the owning bootstrap/runtime skill.

## Fast Operator Loop

```powershell
rg -n "\b(world|entangle|patch|law|converge|orchestrate|axiom|pulse|teleport|shatter|shader|component|comptime|actor|collapse|observe|decay)\b" library_of_kain blades benchmark smoketest
rg -n "Item::(World|Entangle|Patch|Law|Converge|Orchestrate|Axiom|Pulse|Shader|Component)|Expr::(Teleport|Collapse|Observe|Decay|StageCall)|ComputeMetadata" crates/core/src
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

Best first examples:

- `library_of_kain/semantics.kn`: compact full semantic stack: axiom, world, entangle, shatter, actor, law, patch, converge, orchestrate, pulse, teleport, ownership, runtime counters.
- `library_of_kain/actor_ownership_backpressure.kn`: actor plus world/entangle/ownership pressure lane.
- `library_of_kain/machine_stones_shatter.kn`: minimal `shatter struct` loop.
- `library_of_kain/PONG.kn`: large world/entangle/game-style state topology.
- `benchmark/cases/semantic_singularity_crucible/main.kn`: dense LLVM torture lane for semantic fusion.
- `benchmark/cases/quantumerlang/main.kn`: actor/message/ownership/converge/teleport/world pressure.
- `benchmark/cases/pulse_teleport_decay_mesh/main.kn`: temporal pulse plus teleport/decay mesh.
- `blades/kain-example/src/main.kn`: broad blade-style authored Kain.
- `blades/pong/src/main.kn`: authored state topology and game proof surface.
- `stdlib/STDLIB_MAP.llm.md`: generated stdlib import/function map.

## Compiler To Runtime Flow

Authored Kain normally flows like this:

```text
.kn source
-> parser and AST in crates/core/src/parser.rs + ast.rs
-> type/semantic checks in crates/core/src/types.rs
-> runtime contract in crates/core/src/runtime_contract.rs
-> interpreter behavior in crates/core/src/runtime.rs where supported
-> native/C/LLVM/GPU lowering in crates/sys-codegen and crates/gpu
-> runtime/native C ABI kernels when the feature has native substrate
-> benchmark/attrition/smoketest/Z3 evidence when the claim matters
```

Core source anchors:

- Parser and grammar surface: `crates/core/src/parser.rs`.
- AST shapes and feature structs: `crates/core/src/ast.rs`.
- Typechecking and semantic validation: `crates/core/src/types.rs`.
- Runtime contract emission and capability keys: `crates/core/src/runtime_contract.rs`.
- Interpreter/runtime behavior for supported authored features: `crates/core/src/runtime.rs`.
- Formatter shape: `crates/core/src/formatter.rs`.
- LLVM/native lowering: `crates/sys-codegen/src/codegen_llvm/mod.rs`.
- C backend fallback/lane metadata: `crates/sys-codegen/src/codegen_c.rs`.
- Rust-side GPU host/artifacts: `crates/sys-codegen/src/codegen_rust/gpu_artifacts.rs`, `crates/sys-codegen/src/codegen_rust/gpu_host.rs`.
- SPIR-V/PTX/HLSL shader lowering: `crates/gpu/src/codegen_spirv.rs`, `crates/gpu/src/codegen_ptx.rs`, `crates/gpu/src/codegen_hlsl.rs`.
- Runtime GPU executor: `crates/gpu-runtime/src/executor.rs`, `crates/gpu-runtime/src/nvidia_ptx.rs`.
- Native semantic kernels: `runtime/native/src/core/entangle.c`, `runtime/native/src/core/machine_stones.c`, `runtime/native/src/core/kain_runtime_native_stdlib.c`.
- Native semantic headers: `runtime/native/include/entangle.h`, `runtime/native/include/machine_stones.h`, `runtime/native/include/converge.h`, `runtime/native/include/stdlib_abi.h`.
- Public semantic helpers: `stdlib/intent.kn`, `stdlib/native/runtime.kn`, `stdlib/STDLIB_MAP.llm.md`.

## Feature Index

| Feature | Authoring Use | Syntax Shape | Source Anchors |
| --- | --- | --- | --- |
| `use` | Import stdlib, packages, C ABI modules, local modules | `use std::runtime`, `use c::bridge`, `use foo::bar as baz` | `parser.rs parse_use`, `ast.rs Use`, stdlib map |
| `mod` | Group authored items inline or external declaration | `mod name:` or `mod name` | `parser.rs parse_mod`, `ast.rs Mod` |
| `const` | Typed compile-known constants | `const NAME: Int = 1` | `parser.rs parse_const`, `ast.rs Const` |
| `comptime` | Compile-time block or expression; compute shader metadata | `comptime:` / `comptime { expr }` | `parser.rs parse_comptime_block`, `ast.rs ComptimeBlock`, `comptime.rs` |
| `fn` | Plain callable logic, helpers, public APIs | `fn name(x: Int) -> Int:` | `parser.rs parse_function`, `types.rs check_function` |
| effects | Signal allowed effect domains | `with Pure`, `with IO`, `with Async`, `with GPU`, `with Reactive`, `with Unsafe` | `parser.rs parse_effects`, `ast.rs Effect`, `types.rs EffectSet` |
| `async` / `await` | Future-oriented authored flows | `async expr`, `await future` | `parser.rs parse_async_function`, `ast.rs Expr::AsyncBlock/Await`, `types.rs infer_expr_type` |
| `struct` | Named data layout with fields, methods, defaults, generics | `struct Packet<T>:` | `parser.rs parse_struct_with_attrs`, `types.rs check_struct` |
| `shatter struct` | Structure-of-arrays layout intent for hot data lanes | `shatter struct Particle:` | `parser.rs parse_shatter_struct`, `ast.rs Struct::is_shattered`, `runtime/native/src/core/machine_stones.c` |
| `enum` | Sum types and variants | `enum Mode: Idle Moving(Int) Hit { power: Int }` | `parser.rs parse_enum`, `types.rs check_enum` |
| `trait` / `impl` | Shared interfaces and method implementations | `trait Fold:`, `impl Fold for Packet:` | `parser.rs parse_trait/parse_impl`, `types.rs check_impl` |
| `type` | Type alias | `type Checksum = Int` | `parser.rs parse_type_alias`, `ast.rs TypeAlias` |
| `Option` / `Result` | Nullable/error-bearing values | `Option<Int>`, `Result<Int, String>`, `?` | `ast.rs Type::Option/Result`, `types.rs Expr::Try` |
| pointers | Raw/reference/memory-heavy authored code | `ptr<Int>`, `&T`, `*ptr`, `ptr_offset`, `mem_load` | `ast.rs Type::Ptr/Ref`, `Expr::PtrOffset/MemLoad/MemStore`, `low_level_memory.rs` |
| `collapse` | Exclusive ownership mutation region | `collapse cells: ...` | `ast.rs Expr::Collapse`, `types.rs ownership checks`, `crates/ownership` |
| `observe` | Read-only ownership observation region | `observe cells: ...` | `ast.rs Expr::Observe`, `types.rs ownership checks`, `crates/ownership` |
| `decay` | Deterministic ownership destruction | `decay cells` | `ast.rs Expr::Decay`, `types.rs ownership checks`, `runtime/native/include/ownership.h` |
| `component` | Declarative UI component | `component Panel(title: String): render <panel />` | `parser.rs parse_component_with_attrs`, `types.rs check_component`, LLVM `compile_component` |
| JSX | Component render tree | `<panel title={name}><text /></panel>` | `ast.rs JSXNode`, `parser.rs parse_jsx_element`, LLVM `compile_jsx` |
| `shader` | GPU program authoring | `shader fragment`, `shader vertex`, `shader compute` | `parser.rs parse_shader`, `types.rs check_shader`, `crates/gpu` |
| compute plan | Runtime-visible compute metadata | `comptime: let compute = (...)` inside compute shader | `ast.rs ComputeMetadata`, `runtime_contract.rs gpu.compute-dispatch` |
| `actor` | Message-oriented stateful concurrency | `actor Relay: state bias: Int = 1; on Msg(...)` | `parser.rs parse_actor_with_attrs`, `crates/actor`, `stdlib/native/actor.kn` |
| `spawn` / `send` / `ask` | Actor lifecycle and messaging | `spawn Relay(bias = 1)`, `send`, `ask(...)` | `ast.rs Expr::Spawn/SendMsg`, `runtime/native/include/actor.h` |
| `world` | Named state authority/projection | `world Authority: state signal: Int = 1` | `parser.rs parse_world`, `types.rs check_world`, `runtime_contract.rs RuntimeWorldContract` |
| `entangle` | State coupling between world fields | `entangle A.x <-> B.y with single_writer` | `crates/entangle`, `runtime/native/src/core/entangle.c` |
| `patch` | Intentional journaled state mutation | `patch commit(world: World, value: Int) -> Int:` | `types.rs check_patch`, `runtime_contract.rs RuntimePatchContract`, `stdlib/intent.kn` |
| `law` | Invariant predicate with Bool contract | `law in_bounds(x: Int) -> Bool:` | `types.rs check_law`, Z3 `keywords-law-runtime-accepts-only-bool-results.yaml` |
| `converge` | Spec plus target/capability fast lanes | `spec reference`, `fast llvm_lane when target("llvm")` | `types.rs check_converge`, `runtime.rs select_converge_lane`, LLVM `compile_converge` |
| `orchestrate` | Typed staged pipeline over Kain/Rust/Python/Node | `let x: Int = kain fn(value)` | `types.rs collect_orchestrate_stage_descriptors`, `runtime.rs execute_stage_call` |
| `axiom` | Machine/environment truth with fallback | `when target("llvm")`, `guarantee`, `fallback` | `types.rs check_axiom`, LLVM `kain_machine_axiom_accept`, `machine_stones.c` |
| `pulse` | First-class temporal beat | `pulse tick every 8ms jitter 1ms:` | `types.rs check_pulse`, LLVM pulse lowering, `machine_stones.c` |
| `teleport` | Destructive cross-world ownership handoff | `teleport value from A to B via channel` | `types.rs ensure_teleport_world_reference`, LLVM `compile_teleport_expr`, `machine_stones.c` |
| `macro` | Token/block macro definitions and calls | `macro name!(...)` | `parser.rs parse_macro`, `ast.rs MacroDef` |
| `test` | Source-local test blocks | `test "name": ...` | `parser.rs parse_test`, `types.rs check_test`, `crates/test` |
| material graph | Material DSL | `@material_graph` then `material Name:` | `parser.rs parse_material_graph`, `ast.rs MaterialGraphDef` |
| material function | Material helper DSL | `@material_function` then `fn Name(...)` | `parser.rs parse_material_function`, `ast.rs MaterialFunctionDef` |
| graph editor/runtime | Editor graph DSLs | `@graph_editor graph Name:`, `@graph_runtime struct Name:` | `parser.rs parse_graph_editor/parse_graph_runtime` |
| state machine | State/transition DSL | `@state_machine struct Name:` with `@state` and `@transition` | `parser.rs parse_state_machine`, `crates/core/tests/test_state_machine_parser.rs` |
| editor module | Tooling/editor menu surface | `@editor_module struct Name:` | `parser.rs parse_editor_module` |
| gameplay DSLs | UE-style gameplay surfaces | `@gameplay_tags`, `@ability`, `@gameplay_effect`, `@gameplay_cue`, `@ability_task`, `@target_actor` | `parser.rs parse_gameplay_*`, `stdlib/ue5/*` |

## Data And Control Semantics

Use these without apology before inventing custom patterns:

```kn
type Checksum = Int

struct Packet:
    id: Int
    payload: Int = 0
    hot: Bool

enum Lane:
    Idle
    Hot(Int)
    Tagged { tag: String, score: Int }

trait Fold:
    fn fold_seed(_self: Self_) -> Int:
        return 0

impl Fold for Packet:
    fn fold_seed(_self: Self_) -> Int:
        return 211

fn fold_packets(packets: [Packet], count: Int) -> Result<Int, String>:
    var i: Int = 0
    var acc: Int = 0
    while i < count:
        match packets[i].hot:
            true => acc = acc + packets[i].payload
            false => acc = acc + packets[i].id
            _ => acc = acc
        i = i + 1
    return Result::Ok(acc)
```

Control and expression anchors:

- Statements: `let`, expression statements, `return`, `break`, `continue`, `for`, `while`, `loop`, nested items live in `ast.rs Stmt`.
- Expressions: literals, f-strings, identifiers, binary/unary ops, call, method call, field, index, assignment, struct literal, enum variant, array, tuple, range, if, match, lambda, ref/deref, raw memory, ownership, teleport, cast, try, await, async, spawn, send, comptime, macro call, block, JSX live in `ast.rs Expr`.
- Patterns: wildcard, literal, binding, struct, tuple, enum variant, slice live in `ast.rs Pattern`.
- Type shapes: named/generic, tuple, array, slice, ref, raw ptr, function, option, result, inferred, never, unit, impl trait live in `ast.rs Type`.

## Components And UI

Components are authored UI semantics. A component has props, optional state, methods, effects, and a JSX body.

```kn
component StatusPanel(title: String, score: Int):
    state selected: Bool = false

    fn label(_self: Self_) -> String:
        if selected:
            return "hot"
        return "idle"

    render <panel title={title}>
        <text value={label()} />
        <text value={score} />
    </panel>
```

Component rules:

- Parser accepts `component Name(props):` with body entries `state`, `fn`, `render`, or direct JSX.
- JSX supports elements, component calls, `{expr}` expressions, text, `for`, `if`, and fragments at the AST level.
- Typechecking resolves props, state initializers, methods, and JSX semantics in `types.rs check_component` and `check_jsx_semantics`.
- LLVM lowers JSX to string/UI payload-ish output via `compile_jsx` and components via `compile_component`.
- World surfaces may reference component identifiers or calls for `native_ui` and `web`.
- For heavy UI package work co-trigger `lang-ui`; for Kaintana package work co-trigger `package-kaintana`.

Primary source anchors:

- `crates/core/src/ast.rs`: `Component`, `StateDecl`, `JSXNode`, `JSXAttribute`, `JSXAttrValue`.
- `crates/core/src/parser.rs`: `parse_component_with_attrs`, `parse_jsx_element`.
- `crates/core/src/types.rs`: `check_component`, `check_jsx_semantics`, `check_world_surface_projection`.
- `crates/sys-codegen/src/codegen_llvm/mod.rs`: `compile_jsx`, `compile_component`.
- `runtime_contract.rs`: capabilities `ui.components`, `ui.runtime-bundle`, `world.native-ui`.
- Examples: `blades/kaintana/src/kaintana.kn`, `blades/kaintana-test/src/main.kn`, `blades/kain-example/src/ui.kn`.

## Shader, Compute, And GPU Authoring

Shader authoring belongs in `lang-semantics` when you are writing shader-shaped Kain. Backend work belongs in `bootstrap-gpu` or `runtime-gpu`.

```kn
shader fragment FieldFragment(uv: Vec2) -> Vec4:
    uniform accent: Vec3 @0
    let ring: Float = fbm2(uv, 4)
    return vec4(accent.x * ring, accent.y, accent.z, 1.0)

shader compute ParticleStep(id: UVec3) -> Vec4:
    uniform particles: StorageBuffer<Vec4> @0
    uniform field: StorageBuffer<Vec4> @1
    comptime:
        let compute = (
            [8, 1, 1],
            [64, 1, 1],
            [
                ("particles", "Vec4", ["64"], "state", "kain.shared.buffer"),
                ("field", "Vec4", ["64"], "input", "kain.shared.buffer")
            ],
            [
                ("particles", "readwrite", "continuous", "kain.shared.buffer")
            ],
            []
        )
    let p = particles[id.x]
    let v = field[id.x]
    return vec4(p.x + v.x, p.y + v.y, p.z + v.z, 1.0)
```

Shader rules:

- Parser accepts `shader vertex`, `shader fragment`, and `shader compute`. The AST has `Surface`, but current parser path is vertex/fragment/compute/default-fragment.
- Uniform syntax is `uniform name: Type @binding`.
- Compute metadata is authored inside a compute shader `comptime:` block as `let compute = ...` or `let compute_plan = ...`.
- Legacy compute plan shape is `(dispatch, tensors, nodes)`.
- Extended compute plan shape is `(workgroup, dispatch, tensors, streams, nodes)`.
- Dispatch/workgroup values are three integer literals.
- Tensor entries are `(binding, element_type, shape_strings, role?, contract?)`.
- Stream entries are `(binding, direction, cadence?, contract?)`.
- Neural node entries are `(node_key, op, input_strings, output_strings, stateful?)`.
- Storage buffers should produce shared-buffer/tensor/stream runtime capabilities.

Primary source anchors:

- `crates/core/src/ast.rs`: `Shader`, `ShaderStage`, `Uniform`, `ComputeMetadata`, `ComputeTensorPlan`, `ComputeStreamPlan`, `ComputeNeuralNodePlan`.
- `crates/core/src/parser.rs`: `parse_shader`, compute metadata validation call after parse.
- `crates/core/src/types.rs`: `check_shader`.
- `crates/core/src/runtime_contract.rs`: capabilities `gpu.programs`, `gpu.compute`, `gpu.compute-dispatch`, `interop.shared-buffer`, `data.tensor-buffer`, `data.continuous-stream`, `neural.node-plan`.
- `crates/gpu/src/codegen_spirv.rs`, `crates/gpu/src/codegen_ptx.rs`, `crates/gpu/src/codegen_hlsl.rs`: backend shader emitters.
- `crates/gpu-runtime/src/executor.rs`: runtime execution side.
- `crates/gpu/z3/proofs`: vector, layout, PTX, storage-buffer proof surfaces.
- Examples: `benchmark/cases/gpu_graphics_submit/main.kn`, `library_of_kain/gpu_semantic_ping_pong.kn`, `blades/vulkain/src/vulkain.kn`.

## World, Entangle, Patch, Law

Worlds are typed state authorities. Entangles couple fields between worlds. Patches are intentional mutations. Laws are invariant predicates.

```kn
component MirrorPanel():
    render <panel title="mirror" />

world Authority:
    state signal: Int = 1
    state epoch: Int = 0
    surface native_ui => MirrorPanel

world Mirror:
    state signal_copy: Int = 1
    state epoch_copy: Int = 0
    surface web => MirrorPanel

entangle Authority.signal <-> Mirror.signal_copy with single_writer
entangle Authority.epoch <-> Mirror.epoch_copy with single_writer

law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < 1000000007

patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return authority.signal
```

World rules:

- State slots require explicit type and initializer.
- A world must declare at least one surface.
- Surface kinds are `native_ui`, `viewport3d`, `web`, and `ue5`.
- Duplicate surface kinds are rejected.
- World names become struct-like global types/values for authored code.

Entangle rules:

- Current policy is `single_writer`.
- Endpoints are struct paths such as `Authority.signal`.
- Endpoints must exist, be assignable struct paths, and have matching types after shared-ref peeling.
- Duplicate endpoint participation is rejected.
- Rust policy crate owns portable graph truth; native C owns runtime registry.

Patch and law rules:

- `patch` is function-shaped but tracked as a mutation contract with mutation paths, invalidation keys, replay schema, collaboration event, and undo mode.
- `law` must return `Bool`.
- Public helpers in `std.intent` include `patch_journal_count`, `patch_last_path`, `entangle_propagation_count`, `entangle_last_authority`, `entangle_last_mirror`, `law_status`, `law_is_valid_status`.

Primary source anchors:

- `ast.rs`: `WorldDef`, `WorldStateSlot`, `WorldSurfaceProjection`, `EntangleDef`, `PatchDef`, `LawDef`.
- `parser.rs`: `parse_world`, `parse_entangle`, `parse_patch`, `parse_law`.
- `types.rs`: `check_world`, `check_entangle`, `check_patch`, `check_law`.
- `crates/entangle/src/lib.rs`: `EntangleGraph`, `EntanglePolicy::SingleWriter`, duplicate endpoint and mirror-write policy.
- `runtime_contract.rs`: `RuntimeWorldContract`, `RuntimeEntangleContract`, `RuntimePatchContract`, `RuntimeLawContract`, capabilities `state.entangle`, `patch.transactions`, `law.invariants`.
- `runtime/native/include/entangle.h`, `runtime/native/src/core/entangle.c`: native registry and bounded C strings.
- `runtime/native/src/core/kain_runtime_native_stdlib.c`: native stdlib counters/status helpers.
- `stdlib/intent.kn`: public Kain wrappers for patch/law/entangle helpers.
- Z3: `crates/core/z3/proofs/keywords-law-runtime-accepts-only-bool-results.yaml`, `keywords-patch-cancel-rewinds-only-when-reversible.yaml`, `runtime/native/src/core/z3/proofs/native-entangle-*.yaml`, `native-stdlib-patch-journal-count-stays-within-capacity.yaml`.

## Converge

Use `converge` when there is a reference implementation and one or more target/capability fast lanes.

```kn
fn mix_scalar(value: Int) -> Int:
    return ((value * 31) + 7) % 1000000007

converge mix(value: Int) -> Int:
    spec reference:
        return mix_scalar(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % 1000000007
    fast avx2_lane when capability("cpu.x86.avx2"):
        return ((value * 31) + 7) % 1000000007
    verify random(8)
```

Rules:

- Exactly one `spec` lane is required.
- At least one `fast` lane is required by the parser.
- Selectors are `target("...")` or `capability("...")`; no selector means always eligible.
- Interpreter lane selection scans fast lanes in order and falls back to spec.
- LLVM emits spec and fast symbols, chooses statically when possible, probes CPU capability masks for dynamic lanes, and caches chosen lane.
- Current parser supports `verify random(n)`. Do not advertise `verify exhaustive(n)` unless bootstrap code supports it in this checkout.
- Use `converge_mismatch_count()` or native telemetry when proving a runtime lane did not diverge.

Primary source anchors:

- `parser.rs`: `parse_converge`, `parse_converge_lane`, `parse_converge_selector`, `parse_converge_verify_random_count`.
- `types.rs`: `check_converge`, `ensure_converge_verify_types_supported`.
- `runtime.rs`: `select_converge_lane`, `verify_converge_selected_against_spec`.
- `runtime_contract.rs`: `RuntimeConvergeContract`, `RuntimeConvergeLaneContract`, capability `converge.dispatch`.
- `codegen_llvm/mod.rs`: `compile_converge`.
- `runtime/native/include/converge.h`.
- Z3: `crates/core/z3/proofs/keywords-converge-first-fast-lane-wins-and-spec-fallback.yaml`.

## Orchestrate

Use `orchestrate` to express a typed pipeline across Kain and registered host runtimes.

```kn
fn scalar_stage(value: Int) -> Int:
    return (value + 19) % 1000000007

orchestrate pipeline(value: Int) -> Int:
    let normalized: Int = kain mix(value)
    let staged: Int = rust scalar_stage(normalized)
    return staged
```

Rules:

- Stage runtimes currently parsed are `kain`, `rust`, `python`, and `node`.
- A stage call syntax is `<runtime> function(args)`, represented as `Expr::StageCall`.
- Stage calls must be top-level typed `let binding: Type = <runtime> function(...)` declarations.
- Stage declarations must come before local computation.
- Nested stage calls, bare stage calls, untyped stage lets, and late stage declarations are rejected.
- `rust` stages must resolve to native functions in the interpreter.
- `python` and `node` stages require registered bridge helpers.
- Direct `c` stage syntax is not currently an orchestrate runtime. Use `use c::...` and normal calls in `lang-c-abi-ffi`, or escalate to `bootstrap-core` if the language should grow `c` as a stage runtime.

Primary source anchors:

- `parser.rs`: `parse_orchestrate`, `parse_orchestrate_stage_runtime`, `Expr::StageCall` parse in primary expressions.
- `types.rs`: `check_orchestrate`, `collect_orchestrate_stage_descriptors`, `infer_expr_type Expr::StageCall`.
- `runtime.rs`: `execute_orchestrate_call`, `execute_stage_call`, `execute_rust_stage_call`, `execute_python_stage_call`, `execute_node_stage_call`.
- `runtime_contract.rs`: `RuntimeOrchestrationContract`, `RuntimeOrchestrationStageContract`, capability `orchestrate.pipeline`.
- Z3: `crates/core/z3/proofs/keywords-orchestrate-rejects-invalid-stage-ordering.yaml`.

## Axiom, Pulse, Shatter, Teleport

These are the machine-stone semantics: target truth, temporal beats, silicon layout intent, and destructive cross-world handoff.

```kn
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("time.pulse")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine lane supports pulse, shatter, and teleport"
    fallback scalar_fallback

shatter struct Shard:
    bias: Int
    phase: Int
    alive: Bool

pulse clock every 8ms jitter 1ms:
    let shard = Shard { bias: 1, phase: 2, alive: true }
    let moved = teleport shard from Authority to Mirror via pulse_bus
    let _tick = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias
```

Axiom rules:

- Must declare at least one predicate.
- Must declare at least one guarantee.
- Must declare a non-empty fallback.
- Predicates are `target`, `arch`, and `capability`.
- LLVM lowers predicate checks through `kain_machine_axiom_accept`.

Pulse rules:

- Syntax is `pulse name every <duration> [jitter <duration>]:`.
- Duration units are `ns`, `us`, `ms`, `s`, `tick`, `ticks`.
- Pulse bodies get locals `pulse_tick`, `pulse_dt_ms`, and `pulse_missed`.
- Pulse bodies are allowed broad effects: IO, Async, GPU, Reactive, Unsafe, Alloc, Panic.
- Native runtime fires once immediately on start, then schedules process-local timer callbacks.

Shatter rules:

- `shatter struct` parses as a normal `struct` with the internal `shatter` attribute.
- Runtime contract marks layout as `structure-of-arrays` and records field lanes.
- LLVM can lower shattered array literals to one SoA handle via `kain_machine_shatter_alloc`.
- Runtime lane pointers are bounded by lane count and element count.

Teleport rules:

- Syntax is `teleport value from SourceWorld to TargetWorld [via channel]`.
- Source and target worlds must be declared and distinct.
- Channel cannot be empty.
- The expression returns the payload type.
- If the teleported value is a simple identifier, typechecking marks it moved; later reads are rejected with the moved-by-teleport error.
- Pointer payloads call `kain_machine_teleport_ptr`; non-pointer payloads call `kain_machine_teleport_note`.

Primary source anchors:

- `ast.rs`: `AxiomDef`, `PulseDef`, `PulseDuration`, `Struct::is_shattered`, `Expr::Teleport`.
- `parser.rs`: `parse_axiom`, `parse_axiom_predicate`, `parse_pulse`, `parse_pulse_duration`, `parse_shatter_struct`, `parse_teleport_expr`.
- `types.rs`: `check_axiom`, `check_pulse`, `validate_pulse_duration`, `infer_expr_type Expr::Teleport`, `ensure_teleport_world_reference`.
- `runtime_contract.rs`: `RuntimeAxiomContract`, `RuntimePulseContract`, `RuntimeShatterContract`, capabilities `machine.axiom`, `time.pulse`, `time.hardware-timer`, `memory.shatter`, `world.teleport`, `interop.zero-copy-handoff`.
- `codegen_llvm/mod.rs`: `kain_machine_axiom_accept`, `compile_pulse`, `emit_machine_stones_entry_preamble`, `compile_shattered_array_literal`, `compile_teleport_expr`.
- `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c`.
- Z3: `keywords-axiom-predicate-fallback-is-exclusive.yaml`, `keywords-pulse-next-tick-is-monotonic.yaml`, `keywords-shatter-field-lane-offset-stays-in-bounds.yaml`, `keywords-teleport-origin-cannot-remain-live.yaml`, `native-machine-*.yaml`.

## Actors, Async, Ownership, And Memory

Use actors for message turns, ownership for alias/lifetime intent, and raw memory only when the authored lane needs metal.

```kn
actor Relay:
    state bias: Int = 11
    state turns: Int = 0

    on Fold(reply_to: P, request: Int):
        self.turns = self.turns + 1
        send reply_to.Reply(value = request + self.bias + self.turns)

fn memory_lane(cells: ptr<Int>, count: Int) -> Int:
    var acc: Int = 0
    collapse cells:
        var i: Int = 0
        while i < count:
            let slot = ptr_offset(cells, i, "Int")
            let old = mem_load(slot, "Int")
            mem_store(slot, old + 1, "Int")
            acc = acc + old
            i = i + 1
        0
    let observed: Int = observe cells:
        acc
    decay cells
    return observed
```

Actor rules:

- `actor Name:` owns `state` declarations, `on Message(...)` handlers, and methods.
- Use `spawn ActorName(field = value)` for instances.
- Use `ask(actor, "Message", payload)` for request/reply lanes where stdlib provides the helper.
- Use `send` inside handlers for message sends.
- Co-trigger `lang-actors` for actor-specific authored work.

Ownership/memory rules:

- `collapse` expresses exclusive scoped mutation.
- `observe` expresses read-only scoped observation.
- `decay` expresses deterministic destruction/reclamation.
- Raw memory expressions include `ptr_offset`, `mem_load`, `mem_store`, `alloc`, `alloc_zeroed`, `realloc`, `addr_of`, `sizeof_type`, `alignof_type`, `uninit`.
- Low-level pointer/index arithmetic should get Z3 proof if changed in compiler/runtime; authored demos should at least use existing proof lanes.

Primary source anchors:

- Actors: `crates/core/src/ast.rs Actor/MessageHandler/Expr::Spawn/Expr::SendMsg`, `crates/core/src/parser.rs parse_actor_with_attrs`, `crates/actor`, `runtime/native/include/actor.h`, `stdlib/native/actor.kn`.
- Ownership: `crates/ownership/src/lib.rs`, `crates/core/src/types.rs ownership checks`, `runtime/native/include/ownership.h`, `runtime/native/src/core/*ownership*`.
- Memory: `crates/core/src/low_level_memory.rs`, `ast.rs Expr::PtrOffset/MemLoad/MemStore/Alloc/Realloc`, `runtime/native/include/memory.h`, `runtime/native/src/core/*memory*`.
- Z3: `crates/ownership/z3/proofs`, `runtime/native/src/core/z3/proofs/native-ownership-*.yaml`, `native-memory-*.yaml`.

## Comptime, Macros, Tests

Use `comptime` for compile-owned metadata or compile-time calculations, not runtime logic hiding. Use macros sparingly for syntax generation. Use `test` blocks for source-local checks when the harness is relevant.

```kn
comptime:
    const ROUTE_MASK: Int = 63

macro identity!(value: expr) {
    value
}

test "mix keeps value in bounds":
    let value = mix(10)
    assert(value >= 0)
```

Rules:

- Top-level `comptime:` parses to `Item::Comptime`.
- `comptime expr` or `comptime: block` also exists as `Expr::Comptime` paths.
- Compute shaders use `comptime:` metadata blocks for dispatch/tensor/stream/node plans.
- Macro parameter kinds include `expr`, `type`, `ident`, `block`, `token`, and repetition.
- Test blocks are owned by source certification lanes; co-trigger `test-harness` for harness changes.

Primary source anchors:

- `parser.rs`: `parse_comptime_block`, expression comptime parser paths, `parse_macro`, `parse_test`.
- `ast.rs`: `ComptimeBlock`, `Expr::Comptime`, `MacroDef`, `MacroParamKind`, `TestDef`.
- `comptime.rs`: comptime evaluator.
- `types.rs`: `check_test`, macro/type paths.
- `crates/check`, `crates/test`, `smoketest/kain-test`.

## Material, Graph, Editor, Gameplay DSLs

These are authored DSL surfaces mostly parsed through attributes. Use them when the task is truly material/editor/UE-style authored Kain; otherwise prefer ordinary components, shaders, worlds, actors, and stdlib.

```kn
@material_graph
material Glow:
    input base: Vec3 = vec3(1.0, 0.4, 0.0)
    let hot = base
    output base_color = hot

@state_machine
struct Movement:
    @state(entry: true)
    Idle:
        animation: "Idle"
```

Attribute-dispatched surfaces:

- `@material_graph` expects `material Name:` with `input`, `let`, and `output`.
- `@material_function` expects `fn Name(...)`.
- `@graph_editor` expects `graph Name:`.
- `@graph_runtime` expects `struct Name:`.
- `@state_machine` expects `struct Name:` with `@state` entries and transitions.
- `@editor_module` expects `struct Name:` with `@menu_entry`, `@toolbar_button`, `@toolbar_widget`, or `fn`.
- `@gameplay_tags` parses gameplay tag namespace definitions.
- `@ability`, `@gameplay_effect`, `@gameplay_cue`, `@ability_task`, and `@target_actor` parse UE-style gameplay structures.

Primary source anchors:

- `parser.rs`: attribute dispatch near `parse_item`, plus `parse_material_graph`, `parse_material_function`, `parse_graph_editor`, `parse_graph_runtime`, `parse_state_machine`, `parse_editor_module`, `parse_gameplay_tags`, `parse_gameplay_ability`, `parse_gameplay_effect`, `parse_gameplay_cue`, `parse_ability_task`, `parse_target_actor`.
- `ast.rs`: `MaterialGraphDef`, `MaterialFunctionDef`, `GraphEditorDef`, `GraphRuntimeDef`, `StateMachineDef`, `AsyncTaskDef`, `EditorModuleDef`, `GameplayTagsNamespace`, `GameplayAbilityDef`, `GameplayEffectDef`, `GameplayCueDef`, `AbilityTaskDef`, `TargetActorDef`.
- `runtime_contract.rs`: capability `tooling.editor-surfaces` plus shader/material capabilities where applicable.
- `stdlib/ue5/*`: authored UE-oriented stdlib examples.
- `crates/ue5/src/state_machine_ir.rs`, `crates/ue5/src/state_machine_codegen.rs`: UE state-machine bridge surfaces.

## Runtime Contract Capability Cheatsheet

When authoring a semantic feature, expect these contract capabilities:

- Components: `ui.components`, `ui.runtime-bundle`.
- Actors: `actors.syntax`.
- Async tasks: `async.runtime`, `async.timers`.
- Shaders/material: `gpu.programs`.
- Compute shaders: `gpu.compute`, `gpu.compute-dispatch`, optional compute-plan capability from `COMPUTE_PLAN_CAPABILITY_KEY`.
- Storage-buffer compute: `interop.shared-buffer`, `data.tensor-buffer`, `data.continuous-stream`.
- Editor/graph: `tooling.editor-surfaces`.
- Patch: `patch.transactions`.
- Law: `law.invariants`.
- Axiom: `machine.axiom`.
- Pulse: `time.pulse`, `time.hardware-timer`.
- Shatter: `memory.shatter`.
- Teleport: `world.teleport`, `interop.zero-copy-handoff`.
- Converge: `converge.dispatch`.
- Entangle: `state.entangle`.
- Orchestrate: `orchestrate.pipeline`.
- World UI surfaces: `world.native-ui`.

Source: `crates/core/src/runtime_contract.rs` capability construction.

## Validation Ladders

For authored Kain:

```powershell
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

For projects:

```powershell
kain check <project-dir> --target llvm
kain run <project-dir> --target llvm
```

For language feature pressure:

```powershell
python benchmark/run.py --case semantic_singularity_crucible,quantumerlang,pulse_teleport_decay_mesh --languages kain --runs 1 --warmups 0 --timeout 900
```

For runtime cleanliness:

```powershell
python attrition/run.py --help
rg -n "semantic|quantum|pulse|teleport|entangle|shatter" attrition
```

For proofs:

- Use Z3 proof packs when changing compiler/runtime ownership, pointer arithmetic, ABI layout, lane dispatch, shatter offsets, teleport handoff, or entangle registry bounds.
- Existing keyword proofs live under `crates/core/z3/proofs`.
- Existing GPU proofs live under `crates/gpu/z3/proofs`.
- Existing native semantic proofs live under `runtime/native/src/core/z3/proofs`.

## Handoff Boundaries

- Use `bootstrap-core` when the parser, AST, typechecker, runtime contract, interpreter, or generic LLVM lowering has to change.
- Use `bootstrap-gpu` when shader typing, SPIR-V/PTX emission, compute metadata staging, or compiler-side GPU validation has to change.
- Use `runtime-core` when native actor/ownership/entangle/realtime core substrate changes.
- Use `runtime-gpu` when GPU executor, graphics runtime, or native shader bundle consumption changes.
- Use `runtime-stdlib` when stdlib-backed native bridge behavior changes.
- Use `lang-actors`, `lang-ownership`, `lang-gpu`, `lang-ui`, or `lang-c-abi-ffi` alongside this skill when the authored feature is centered in those domains.
- Use `test-bench`, `test-attrition`, `test-crash-forensics`, or `test-harness` when the work is validation/certification rather than authoring.

## Anti-Patterns

- Do not replace `world` plus `entangle` with two plain structs just because it is easier.
- Do not replace `patch` with a helper function when mutation intent and journalability matter.
- Do not replace `law` with a random `if` when the invariant is part of runtime proof shape.
- Do not use `orchestrate` as fake syntax for unsupported runtimes; current runtimes are `kain`, `rust`, `python`, and `node`.
- Do not advertise `verify exhaustive` in `converge` unless the current parser supports it.
- Do not put C-ABI package usage into `orchestrate`; use `use c::...` and `lang-c-abi-ffi`.
- Do not tell future agents to read a random fixed file as the only source of truth. Give source anchors and examples, then verify with `rg`.
