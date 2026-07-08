# KAIN Render & UI Pipeline — Complete File Map

**Last updated:** 2026-06-25
**Scope:** All Rust crate files + C runtime files related to UI, component keyword, JSX, rendering, GPU surface, and shader pipeline.

---

## Table of Contents

1. [Layer 0: C Runtime Substrate](#layer-0-c-runtime-substrate)
2. [Layer 1: Kain Stdlib UI](#layer-1-kain-stdlib-ui)
3. [Layer 2: Compiler Frontend (Parser, AST, Typechecker)](#layer-2-compiler-frontend)
4. [Layer 3A: Codegen Backend — LLVM IR & VTable Calls](#layer-3a-codegen-backend--llvm-ir)
5. [Layer 3B: Codegen Backend — Rust/C++/C Transpilation](#layer-3b-codegen-backend--rustcppc-transpilation)
6. [Layer 3C: Codegen Backend — GPU Shader Emission](#layer-3c-codegen-backend--gpu-shader-emission)
7. [Layer 3D: Codegen Backend — WASM & Script (JS/TS/KS)](#layer-3d-codegen-backend--wasm--script)
8. [Layer 3E: Codegen Backend — UE5 (Unreal Engine)](#layer-3e-codegen-backend--ue5)
9. [Layer 3F: Codegen Backend — Web Hybrid](#layer-3f-codegen-backend--web-hybrid)
10. [Layer 4: UI Runtime Crates (kain-ui, ui-native, ui-tauri)](#layer-4-ui-runtime-crates)
11. [Layer 5: Driver & Build Pipeline](#layer-5-driver--build-pipeline)
12. [Layer 6: Service API & Bridge](#layer-6-service-api--bridge)
13. [Layer 7: GPU Runtime Executor Layer](#layer-7-gpu-runtime-executor-layer)
14. [Layer 8: 3D Scene & Rendering](#layer-8-3d-scene--rendering)
15. [Layer 9: Interop & Shared Memory](#layer-9-interop--shared-memory)
16. [Layer 10: External Host Bridges](#layer-10-external-host-bridges)
17. [Complete Dependency Graph](#complete-dependency-graph)

---

## Layer 0: C Runtime Substrate

The native C runtime provides the execution substrate that LLVM-emitted code calls into at runtime.

### Core UI System (`runtime/native/src/ui/`)

| File | Role in UI Pipeline | Key Symbols |
|------|--------------------|-------------|
| `ui_system.c` | Component state, invalidation, focus routing, event dispatch. The retained-mode UI node tree owner. | `KainUiSystem`, `ui_system_init`, `ui_system_begin_frame`, component invalidation |
| `ui_runtime.c` | UI runtime lifecycle: init, terminate, top-level tree management | `KainUiRuntime`, `kain_ui_runtime_create`, `kain_ui_runtime_destroy` |
| `ui_host_adapter.c/h` | Interface between platform window system and Kain UI component tree. Translates platform events → component updates | `KainUiHostAdapter`, `kain_ui_host_adapter_pump`, `kain_ui_host_adapter_present` |
| `ui_hot_reload.c` | Hot-reload support — monitors compiled UI bundles, triggers component swaps | `KainUiHotReload`, `kain_ui_hot_reload_check` |
| `ui_compiled_bundle.c` | Loads/validates compiled UI bundle payloads (pre-compiled component trees) | `KainUiCompiledBundle`, `kain_ui_compiled_bundle_load` |

### Component Surface ABI (`runtime/native/src/core/`)

| File | Role in UI Pipeline | Key Symbols |
|------|--------------------|-------------|
| `component_surface.c` | The `KainComponentSurface` vtable trait — 19 slots for element create/destroy, attribute set, frame lifecycle, state persistence. All compiled Kain component code calls through this vtable. | `KainComponentSurface`, `kain_component_surface_resolve`, vtable slot 0–18 |
| `vulkan_surface_shim.c` | Vulkan backend shim — registers `KainComponentSurface` vtable for Vulkan rendering | `kain_vulkan_surface_shim_init` |
| `d3d12_surface_shim.c` | D3D12 backend shim — registers vtable for Direct3D 12 rendering | `kain_d3d12_surface_shim_init` |
| `webgpu_surface_shim.c` | WebGPU backend shim — registers vtable for WebGPU (wgpu) rendering | `kain_webgpu_surface_shim_init` |
| `vulkan_stubs.c` | Stub implementations for Vulkan functions when Vulkan is not available | fallback stubs |

### Graphics System (`runtime/native/src/core/`)

| File | Role in UI Pipeline | Key Symbols |
|------|--------------------|-------------|
| `graphics_system.c` | Raw graphics kernel — buffer management, SPIR-V shader registration, pipeline state, draw commands. Called by the UI renderer. | `kain_graphics_system_create`, `gfx_buffer_create`, `gfx_shader_module_register` |
| `renderer_backend.c` | Renderer backend identity and capability descriptors (Vulkan, D3D12) | `KainRendererBackend`, `kain_renderer_backend_describe` |
| `renderer_session.c` | Renderer session lifecycle: frame begin/end, resource binding, command submission | `KainRendererSession`, `kain_renderer_session_begin_frame` |
| `input_system.c` | Input session management — keyboard/mouse/gamepad events routed to UI components | `KainInputSystem`, `kain_input_session_create`, `kain_input_bind_action` |
| `cuda_runtime.c` | CUDA PTX dispatch for GPU compute. Status: degraded | `kain_cuda_runtime_dispatch` |

### Headers (`runtime/native/include/`)

| File | Role in UI Pipeline | Key Symbols |
|------|--------------------|-------------|
| `component_surface.h` | The canonical `KainComponentSurface` vtable layout. 19 function pointer slots. Must match `OFF_*` constants in `crates/sys-codegen/.../component.rs` | `KainComponentSurface`, slot 0–18 |
| `ui_system.h` | Public ABI for UI system: node creation, state, invalidation, focus | `kain_ui_system_*` functions |
| `ui_runtime.h` | Public ABI for UI runtime lifecycle | `kain_ui_runtime_*` |
| `ui_bundle.h` | Compiled UI bundle loading and validation | `KainUiBundle` |
| `ui_color.h` | Color representation types for UI | `KainUiColor` |
| `ui_font.h` | Font handling types | `KainUiFont` |
| `ui_hot_reload.h` | Hot reload descriptor types | `KainUiHotReloadPlan` |
| `ui_layout.h` | Layout engine types (Auto, Native, Yoga) | `KainUiLayoutEngineKind` |
| `ui_renderer.h` | Renderer capability descriptors | `KainUiRendererKind`, renderer backend info |
| `vulkan_loader_subset.h` | Minimal Vulkan loader bindings for surface shim | Vulkan PFN declarations |
| `webgpu_loader_subset.h` | Minimal WebGPU loader bindings | WebGPU PFN declarations |
| `graphics_system.h` | Raw graphics kernel ABI | buffer/shader/pipeline functions |
| `graphics_bundle.h` | Graphics bundle metadata | bundle descriptors |

### Z3 Proof Packs (`runtime/native/src/core/z3/`)

| File | Role in UI Pipeline |
|------|--------------------|
| `proofs/ui_*.yaml` | Z3 proof packs validating UI runtime invariants |

---

## Layer 1: Kain Stdlib UI

In `X:/stdlib/`. The Kain-authored standard library surface for UI.

### Main Module

| File | Lines | Role in UI Pipeline |
|------|-------|--------------------|
| `stdlib/ui.kn` | ~1,677 | Central UI @extern bridge: 83 `abi_ui_*` externs backed by C runtime. Session create, node create/manage, frame lifecycle, state i64/string, style, focus, clipboard, host services. Entry point for all std::ui usage. |
| `stdlib/ui/component.kn` | ~158 | Bridge between Kain components (retained mode) and widget library (immediate mode). `begin_frame/end_frame`, `render_button`, `render_panel`, `render_checkbox`, etc. |
| `stdlib/ui/widget.kn` | ~185 | Immediate-mode widget wrappers over `abi_ui_widget_*` C ABI. `widget::create`, `widget::begin_frame`, `widget::button`, `widget::label`, `widget::panel_begin/end`, `widget::slider`, `widget::textbox`, etc. |
| `stdlib/ui/style.kn` | ~113 | Color constants (0xAARRGGBB), palette, widget size defaults, `ui_color_rgba()` helper |
| `stdlib/ui/font.kn` | Font handling helpers |

### UI-Adjacent Stdlib Modules

| File | Role in UI Pipeline |
|------|--------------------|
| `stdlib/reload.kn` | Hot-reload wrappers over `std::ui` hot_reload @extern |
| `stdlib/graphics.kn` | Graphics session management, buffer/shader/mesh/pipeline/draw commands — feeds GPU resources to UI surfaces |
| `stdlib/graphics_shared.kn` | Shared buffer/image/tensor view constructors for cross-runtime GPU interop |
| `stdlib/gpu.kn` | Pipeline library, resource policies, pipeline handles |
| `stdlib/input.kn` | Input session, action binding, keyboard/mouse/gamepad — feeds events to UI |
| `stdlib/js.kn` | JavaScript interop — calls through Node bridge, used by Tauri/Web targets |
| `stdlib/kain.kn` | Compiler service bindings (LSP, hover, completions) for UI tooling |
| `stdlib/mcp.kn` | MCP protocol server for model context protocol — UI-adjacent tooling |

---

## Layer 2: Compiler Frontend

### Parser (`crates/core/src/parser.rs` — 11,411 lines)

| Function/Area | Role in UI Pipeline | Key Lines |
|---------------|--------------------|-----------|
| `parse_component()` | Parses `component Name(props) -> UI { state ... methods ... render: ... }` | ~2,541 |
| `parse_component_with_attrs()` | Parses component with attributes (e.g., `@wasm`) | ~2,703 |
| `parse_jsx()` | Entry point for JSX parsing — `<element>`, `{expr}` | ~7,042 |
| `parse_jsx_element()` | Parses `<tag attr={value}>children</tag>` | ~7,054 |
| `parse_jsx_tag_name()` | Parses JSX tag name (with namespace colon) | ~7,285 |
| `parse_jsx_attribute_name()` | Parses attribute before `=` in JSX | ~7,310 |
| `finish_jsx_node()` | Completes JSX node after opening tag parsed | ~7,349 |
| `parse_jsx_braced_child()` | Parses `{expression}` inside JSX | ~7,377 |
| `parse_jsx_inline_node()` | Parses inline expression nodes within JSX | ~7,421 |
| `parse_world_surface_projection()` | Parses `world Foo surface: kind` declaration | ~2,392 |
| Reserved keywords | `"component"` is a reserved keyword | ~95 |

### AST (`crates/core/src/ast.rs` — 4,071 lines)

| Type/Enum | Role in UI Pipeline | Key Fields |
|-----------|--------------------|------------|
| `Component` | Component declaration AST node — props, state, methods, JSX body | `name, props, state, methods, effects, body: JSXNode` |
| `StateDecl` | Component state variable declaration | `name, ty, initial, weak` |
| `JSXNode` | Enum of all JSX node types | `Element, Expression, Text, ComponentCall, For, If, Fragment` |
| `JSXAttribute` | A single attribute on a JSX element | `name, value: JSXAttrValue` |
| `JSXAttrValue` | Value of a JSX attribute: literal string, expression, or boolean | `String, Expr, Bool` |
| `Item::Component` | Top-level item variant for component | wraps `Component` |
| `Shader` | Shader declaration (vertex/fragment/compute) — feeds GPU pipeline | `name, stage, body, inputs, uniforms` |
| `WorldSurfaceKind` | Enum of surface kinds a world can project onto | `NativeUI, Headless, None` |
| `WorldSurfaceProjection` | World surface projection metadata | `kind, size_hint, backend_preference` |

### Typechecker (`crates/core/src/types.rs` — 16,772 lines)

| Function/Type | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `TypedComponent` | Type-checked component — props typed, effects validated | ~7,954 |
| `check_component()` | Validates a component declaration: props, state, methods, effects | ~7,955 |
| `check_jsx_semantics()` | Typechecks JSX nodes against component/element contracts | ~12,662 |
| `check_world_surface_projection()` | Validates world → surface binding | ~6,969 |
| `ResolvedType` | Resolved types flowing into component prop/state type checking | throughout |

### UI Evaluation (`crates/core/src/ui.rs` — 3,672 lines)

| Function/Type | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `VNode` | Runtime virtual node — the interpreted JSX output before codegen | ~141 |
| `eval_jsx()` | Runtime-evaluates JSX nodes → VNode tree | ~243 |
| `eval_component_call()` | Resolves and renders component instances at runtime | ~378 |
| `render_component_definition()` | Renders a component definition evaluating state/methods | ~417 |
| `build_ui_output_from_source()` | Compiles source → UiBuildOutput (used by interpreter AND native binder) | ~317 |
| `build_ui_output_from_program()` | Builds UiBuildOutput from typed program | ~331 |
| `render_ui_output_debug()` | Debug renders UiBuildOutput tree to string | ~374 |
| `render_authored_expr_contract()` | Renders an expression as a contract string | ~998 |
| `UIBackendKind` | Named backend targets: Runtime, ReactDom, BrowserDom, Slate | ~44 |
| `UIBackendProfile` | Declarative backend capabilities for UI lowering | ~61 |
| `record_component_state_signals()` | Traces signal ids for component reactive state | ~2,372 |
| `record_surface_decl()` | Records surface declarations during UI output build | ~2,296 |
| `parse_surface_renderer_*()` | Parses surface backend preference strings | ~2,447 |

### Runtime Interpreter (`crates/core/src/runtime.rs` — 10,373 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `register_component()` | Registers a component in the runtime environment for evaluation | ~4,515 |
| `lookup_component()` | Looks up a registered component by name | ~5,136 |
| `eval_expr()` | Evaluates expressions, dispatches JSX/comptime/component calls | whole file |

### Realtime App Bundle (`crates/core/src/realtime_app_bundle.rs` — 3,177 lines)

| Type/Function | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `RealtimeAppBundle` | Full compiler-emitted metadata bundle for runtime execution | ~30 |
| `RenderSceneBundle` | Scene graph description for rendering | within bundle |
| `RealtimeUiContractsBundle` | Compiler-emitted UI contract bundle (workspace graphs, spatial ownership) | ~87 |
| `RealtimeShaderCanvasBinding` | A shader canvas surface within the UI | later in file |
| `build_ui_structure_index()` | Builds UI node index for tooling/reflection | ~653 |
| `collect_shader_canvas_surface_resources()` | Gathers GPU resources from shader canvas surfaces | ~1,192 |

### Runtime Contract (`crates/core/src/runtime_contract.rs` — 3,312 lines)

| Type | Role in UI Pipeline | Key Lines |
|------|--------------------|-----------|
| `RuntimeContractBundle` | Full compiler-emitted contract loaded by C runtime | ~47 |
| Service bindings | Runtime services required by surfaces (ui.component, gfx.backend.*) | ~70 |

### Stdlib Registration (`crates/core/src/stdlib.rs` — 2,037 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `register_stdlib_extension()` | Entry point for all stdlib @extern registration (including UI @extern) | ~21 |
| BuiltinFn | Metadata for UI @extern functions declared to the compiler | ~37 |

### Low-Level Memory / Lowering (`crates/core/src/low_level_memory.rs`)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `TypedComponent` handled in `lower_typed_program_memory_for_target()` | Lowers component memory layout for target ABI | ~594 |

### Monomorphize (`crates/core/src/monomorphize.rs`)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `monomorphize()` | Monomorphizes generic components during type checking |

### Shader Analysis (`crates/core/src/shader_analysis.rs`)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `analyze_shader()` | Stub — analyzes shader complexity metrics. WIP. |

### Shader Artifact (`crates/core/src/shader_artifact.rs`)

| Type | Role in UI Pipeline | Key Lines |
|------|--------------------|-----------|
| `ShaderArtifactBundle` | Serialized shader bundle for GPU consumption (SPIR-V, HLSL, PTX) | ~24 |
| `SpirvModuleArtifact` | SPIR-V module with hex-encoded bytes | ~52 |
| `ShaderReflectionSummary` | Reflection data for shader inputs/outputs/bindings | ~93 |
| `ShaderResourceLayout` | Per-shader resource binding layout | ~117 |

### Lib Exports (`crates/core/src/lib.rs`)

```rust
pub mod ui;       // UI eval, build_ui_output, VNode
pub mod types;    // TypedComponent, check_component, check_jsx
pub mod ast;      // Component, JSXNode, JSXAttribute, StateDecl
pub mod parser;   // parse_component, parse_jsx*
pub mod runtime;  // register_component, eval_jsx
pub mod realtime_app_bundle;  // RealtimeAppBundle (includes UI contracts)
pub mod runtime_contract;     // RuntimeContractBundle (service bindings)
pub mod shader_artifact;      // ShaderArtifactBundle
pub mod shader_analysis;      // Shader analysis (stub)
pub mod low_level_memory;     // Memory lowering for components
pub mod monomorphize;         // Monomorphization
```

---

## Layer 3A: Codegen Backend — LLVM IR

### Main LLVM Codegen (`crates/sys-codegen/src/codegen_llvm/mod.rs` — 21,711 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `compile_component()` | Top-level entry: delegates to `compile_component_render` | ~15,018 |
| `compile_shader()` | Generates LLVM IR for shader entry points + SPIR-V globals | ~16,362 |
| `compile_world_initializer()` | Emits world init + surface loop (calls `__kain_world_surface_loop_*`) | ~16,216 |
| `compile_jsx()` | Dispatches JSX node types to appropriate LLVM emission | ~12,514 |
| `emit_shader_spirv_globals()` | Emits SPIR-V byte arrays as LLVM global constants | ~15,226 |
| `collect_shader_spirv_hexes()` | Collects all shader SPIR-V into hex strings for embedding | ~15,199 |
| `jsx_span()` | Gets debug span for a JSX node | ~12,502 |
| `find_shader_by_name()` | Finds compiled shader by name | ~15,184 |

### Component VTable Codegen (`crates/sys-codegen/src/codegen_llvm/component.rs` — 1,513 lines)

| Function/Constant | Role in UI Pipeline | Key Lines |
|------------------|--------------------|-----------|
| `OFF_*` constants (OFF_SESSION_CREATE through OFF_SESSION_ATTACH_PLATFORM) | Vtable slot offsets that must match `KainComponentSurface` field order in C | ~14–40 |
| `OFF_GET_GPU_EXTENSION` | Slot 18 — returns `KainGpuSurfaceExtension*` or NULL | ~41 |
| `map_jsx_attr_to_surface_key()` | Maps JSX attribute names (`padding`, `background`, `title`, etc.) to vtable slot + style key | ~44 |
| `declare_surface_trait_types()` | Emits LLVM type declarations for `%KainComponentSurface` opaque struct | ~77 |
| `compile_component_render()` | Compiles a component's render method to LLVM IR through the vtable | ~106 |
| `compile_shader_surface_loop()` | Generates the world surface loop for shader-based surfaces | ~237 |
| `compile_surface_frame_loop()` | Generates the frame loop for native UI surfaces (begin frame / end frame / present) | ~584 |
| `compile_jsx_to_surface()` | Recursive JSX → vtable call emission | ~788 |
| `compile_jsx_text()` | Emits `element_set_text` vtable call for text nodes | ~856 |
| `compile_jsx_expression()` | Evaluates {expressions} inside JSX at compile time → vtable calls | ~887 |
| `compile_jsx_element()` | Emits `element_begin` + attrs + children + `element_end` for `<tag>` | ~931 |
| `compile_jsx_component_call()` | Emits a `<Component>` nested component via surface vtable | ~973 |
| `compile_jsx_for()` | Emits `for item in list: <child>` → iteration over surface | ~1,039 |
| `compile_jsx_if()` | Emits `if cond: <then> else: <else>` branching on surface | ~1,138 |
| `compile_jsx_attr()` | Emits attribute setting via matching vtable slot | ~1,200 |
| `compile_component_state_init()` | Emits persistent component state initialization via vtable | ~1,502 |
| `emit_vtable_call()` | Generates an indirect call through a vtable slot (with result) | ~1,341 |
| `emit_vtable_call_void()` | Generates an indirect vtable call with void return | ~1,399 |
| `emit_element_begin()` | Generates `element_begin` vtable call | ~1,415 |
| `emit_element_end()` | Generates `element_end` vtable call | ~1,438 |

### Attr Mapping (inside `component.rs`)

```
"padding"        → OFF_ELEMENT_SET_ATTR_F64    style_key: "padding"
"spacing"        → OFF_ELEMENT_SET_ATTR_F64    style_key: "spacing"
"corner_radius"  → OFF_ELEMENT_SET_ATTR_F64    style_key: "corner_radius"
"font_size"      → OFF_ELEMENT_SET_ATTR_F64    style_key: "font_size"
"opacity"        → OFF_ELEMENT_SET_ATTR_F64    style_key: "opacity"
"border_width"   → OFF_ELEMENT_SET_ATTR_F64    style_key: "border_width"
"width"          → OFF_ELEMENT_SET_ATTR_F64    style_key: "width"
"height"         → OFF_ELEMENT_SET_ATTR_F64    style_key: "height"
"background"     → OFF_ELEMENT_SET_ATTR_STRING style_key: "fill_color"
"border_color"   → OFF_ELEMENT_SET_ATTR_STRING style_key: "border_color"
"color"          → OFF_ELEMENT_SET_ATTR_STRING style_key: "ink_color"
"title"          → OFF_ELEMENT_SET_ATTR_STRING style_key: "title"
"value"          → OFF_ELEMENT_SET_TEXT        (text content)
"direction"      → OFF_ELEMENT_SET_ATTR_I64    style_key: "layout.direction"
"disabled"       → OFF_ELEMENT_SET_ATTR_I64    style_key: "disabled"
```

### LLVM IR Optimization (`crates/driver/src/llvm_ir.rs`)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `slice_llvm_native_executable_ir()` | Slices LLVM IR to only reachable functions. Dead-strips unreachable UI components. | ~25 |
| `analyze_llvm_ir_reachability()` | BFS reachability from entry points (main, surface loops) | within |

### LLVM Codegen Tests (`crates/sys-codegen/tests/llvm_codegen_test.rs` — 3,913 lines)

| Test | Role in UI Pipeline | Key Lines |
|------|--------------------|-----------|
| `llvm_lowers_single_file_native_ui_primitives_without_component_catalog()` | Verifies native UI primitives generate correct LLVM `abi_ui_*` calls | ~895 |
| `llvm_lowers_native_ui_host_services_without_component_catalog()` | Verifies host service calls (`abi_ui_host_*`) emitted correctly | ~985 |
| `llvm_generates_component_and_jsx_calls()` | Full test: component with JSX → LLVM IR vtable call verification | ~1,378 |

---

## Layer 3B: Codegen Backend — Rust/C++/C Transpilation

### Rust Codegen (`crates/sys-codegen/src/codegen_rust/mod.rs` — 3,130 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `gen_component()` | Generates Rust struct + render function from Kain Component AST | ~1,840 |
| `gen_component_props_struct()` | Generates `struct NameProps { ... }` from component props | ~1,824 |
| `gen_component_method_binding()` | Generates closures for component methods | ~1,896 |
| `gen_jsx()` | Generates Rust string-building code for JSX nodes | ~2,763 |
| `gen_jsx_attr_value_expr()` | Generates Rust expression for JSX attribute values | ~2,745 |
| `gen_jsx_children_expr()` | Generates children concatenation | ~2,753 |

### Rust GPU Artifacts (`crates/sys-codegen/src/codegen_rust/gpu_artifacts.rs`)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `collect_gpu_artifacts()` | Collects shader metadata for Rust GPU wrapper generation |

### Rust GPU Host (`crates/sys-codegen/src/codegen_rust/gpu_host.rs`)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `generate_gpu_host()` | Generates Rust host wrappers for GPU shader dispatch (used from UI shader canvases) |

### Rust Artifact Bundle (`crates/sys-codegen/src/codegen_rust/artifact_bundle.rs`)

| Type | Role in UI Pipeline |
|------|--------------------|
| `RustArtifactBundle` | Bundles Rust source + shader metadata for tooling |

### C++ Codegen (`crates/sys-codegen/src/codegen_cpp/mod.rs` — 1,043 lines)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `generate()` | Generates C++17 source from typed program (generic, not UE5-specific) |
| Note: No component/JSX support — components generate as `fn` + string concatenation |

### C Codegen (`crates/sys-codegen/src/codegen_c.rs` — 1,213 lines)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `generate()` | Generates C source (experimental, limited subset). No component/JSX support. |

---

## Layer 3C: Codegen Backend — GPU Shader Emission

### SPIR-V Codegen (`crates/gpu/src/codegen_spirv.rs` — 4,281 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `generate()` | Top-level: walks `TypedProgram` items, emits SPIR-V binary for each shader | ~50 |
| `emit_shader()` | Compiles single shader → SPIR-V through `rspirv` builder | ~134 |
| `compile_fragment_to_spirv_hex()` | Compiles fragment shader → hex-encoded SPIR-V for LLVM embedding | (in lib.rs) |

### PTX Codegen (`crates/gpu/src/codegen_ptx.rs` — 2,612 lines + `ptx_surface.rs`)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `generate()` | Top-level PTX emission for NVIDIA compute | ~50 |
| `generate_variant_modules()` | Generates PTX variant modules (different archs) | (in lib.rs) |
| `PtxCodegenOptions` | Config for PTX generation (arch, version) | ~60 |

### PTX Module (`crates/gpu/src/ptx_module.rs` — 1,960 lines)

| Type | Role in UI Pipeline |
|------|--------------------|
| `PtxArch` | NVIDIA GPU architectures (Sm30–Sm120) |
| `PtxKernelPlan` | Kernel launch configuration plan |

### PTX Surface (`crates/gpu/src/ptx_surface.rs` — 271 lines)

| Type | Role in UI Pipeline |
|------|--------------------|
| `PtxInstDef` | PTX instruction definition table (arithmetic, compare, convert ops) |
| `BINARY_OP_TABLE` | Maps Kain BinaryOp to PTX instruction mnemonic |

### HLSL Codegen (`crates/gpu/src/codegen_hlsl.rs` — 401 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `generate()` | Delegates to `kain_shader_text::hlsl::generate()` | thin wrapper |

### WGSL Codegen (`crates/gpu/src/codegen_wgsl.rs` — 384 lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `generate()` | Delegates to `kain_shader_text::wgsl::generate()` | thin wrapper |

### Shared Shader Text Helpers (`crates/shader-text/src/lib.rs` — 2,081 lines)

| Module/Function | Role in UI Pipeline | Key Lines |
|----------------|--------------------|-----------|
| `TextShaderBackend` | Enum: Hlsl, Wgsl, Usf | ~60 |
| `ShaderValueType` | Type mapping for shader values (scalar, vector, matrix, storage buffer, texture, sampler) | ~80 |
| `pub mod hlsl` | HLSL emitter — maps Kain types to HLSL types, emits functions | ~560 |
| `pub mod wgsl` | WGSL emitter — maps Kain types to WGSL types, emits entry points | ~911 |
| Type mapping | Kain → HLSL/WGSL scalar, vector, matrix, buffer types | ~150+ |

---

## Layer 3D: Codegen Backend — WASM & Script

### WASM Codegen (`crates/wasm/src/codegen_wasm.rs` — 8,300+ lines)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `compile_component()` | Compiles Kain component → WASM binary with JSX rendering | ~2,579 |
| `compute_component_layout()` | Computes WASM memory layout for component state | ~1,392 |
| `compile_jsx_node()` | Recursive JSX → WASM bytecode emission | ~8,299 |
| `collect_strings_in_jsx()` | String pool extraction for WASM-global strings | ~3,169 |
| `preallocate_locals_in_jsx()` | Pre-allocates WASM locals for JSX expressions | ~5,392 |

### C Runtime Shims (`crates/wasm/src/c_runtime_shims.rs`)

| File | Role in UI Pipeline |
|------|--------------------|
| `c_runtime_shims.rs` | WASM-side C runtime stubs for UI/host imports |

### JS Codegen (`crates/script/src/codegen_js.rs` — 42.9 KB)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `gen_component()` | Generates JS function from Kain component | ~264 |
| `gen_jsx()` | Generates JSX virtual DOM calls from JSXNode | ~785 |

### KS Codegen (`crates/script/src/codegen_ks.rs` — 67.9 KB)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `gen_component()` | Generates Kain Script (KS) component | ~557 |
| `jsx_to_str()` | Converts JSX nodes to KS string rendering | ~1,516 |

### TS Codegen (`crates/script/src/codegen_ts.rs` — 92.8 KB)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `gen_typed_component()` | Generates TypeScript component with typed props | ~463 |
| `gen_jsx()` | Generates TypeScript JSX from JSXNode | ~1,638 |

---

## Layer 3E: Codegen Backend — UE5

### UE5 Master Codegen (`crates/ue5/src/codegen_ue5.rs` — 335 KB)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `codegen_ue5.rs` | Main UE5 C++ codegen — handles components, actors, structs, shaders |

### UE5 Blueprints (`crates/ue5-blueprints/src/`)

| File | Role in UI Pipeline | Key Types |
|------|--------------------|-----------|
| `writer.rs` | Blueprint .uasset writer — `add_component_class_import()` | `BlueprintDef`, component handling |
| `ir.rs` | Blueprint IR (intermediate representation) | `BlueprintDef`, `BlueprintNode` |
| `factory.rs` | C++ factory code generation for Blueprints | factory patterns |
| `kismet.rs` | Kismet bytecode generation | Kismet statements |
| `conversion.rs` | Type conversion for Blueprint data | conversion helpers |
| `error.rs` | Error types | |

### UE5 Shaders (`crates/ue5-shaders/src/`)

| File | Role in UI Pipeline | Key Types |
|------|--------------------|-----------|
| `codegen_usf.rs` | USF (Unreal Shader Format) generation — 208 KB | main USF emitter |
| `pod_mirror.rs` | `@component` struct → POD mirror for GPU uniforms. `collect_component_mirrors()` extracts POD-compatible fields from component types used as shader uniforms | `PodMirrorStruct`, `PodField` |
| `shader_knowledge.rs` | Hardcoded UE5 shader knowledge: known parameters, types | `ShaderKnowledge` |
| `type_mapping.rs` | Kain → UE5 type mapping | `TypeMapper` |
| `validation.rs` | USF shader validation | `ShaderValidator` |

### UE5 Codegen Sub-modules (`crates/ue5/src/`)

| File | Role in UI Pipeline |
|------|--------------------|
| `blueprint_codegen.rs` | Blueprint C++ codegen from Kain |
| `state_machine_codegen.rs` | State machine C++ codegen |
| `async_task_codegen.rs` | Async task C++ codegen |
| `network_sync_codegen.rs` | Network sync C++ codegen |
| `ue5/oracle.rs` | Validation oracle — `validate_component()`, `validate_component_uht()`, `validate_components_enhanced()` |
| `ue5/types.rs` | UE5 type system — `register_component()`, `is_component()` |
| `ue5/engine_knowledge.rs` | Engine class knowledge base |
| `ue5/uht_rules.rs` | UHT (Unreal Header Tool) compliance rules |
| `ue5/module_graph.rs` | Module dependency graph for UE5 |
| `ue5/naming.rs` | UE5 naming conventions |
| `ue5/widget_registry.rs` | Widget type registry |

### UE5 Materials (`crates/ue5-materials/src/`)

| File | Role in UI Pipeline |
|------|--------------------|
| `material_graph.rs` | Material graph IR |
| `material_factory.rs` | Material factory code |
| `material_serializer.rs` | Material .uasset serialization |
| `ast_converter.rs` | Kain AST → material graph conversion |

### UE5 Config (`crates/ue5-config/src/`)

| File | Role in UI Pipeline |
|------|--------------------|
| Config codegen | Generates UDeveloperSettings, console variables for UI surfaces |

### UE5 Editor (`crates/ue5-editor/src/`)

| File | Role in UI Pipeline |
|------|--------------------|
| Editor codegen | Slate widgets, custom viewports, detail customizations |

### UE5 GAS (`crates/ue5-gas/src/`)

| File | Role in UI Pipeline |
|------|--------------------|
| GAS codegen | Gameplay Ability System codegen (UI-adjacent: gameplay UI widgets/effects) |

---

## Layer 3F: Codegen Backend — Web Hybrid

### Hybrid Codegen (`crates/web/src/codegen_hybrid.rs` — 17.5 KB)

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `generate()` | Produces HybridOutput: WASM bytecode + JS/TS glue + export bindings | top-level |
| `HybridOutput` | Contains wasm, js, ts, wasm_exports | ~25 |
| `WasmExport` | Per-function export metadata for JS bridge generation | ~37 |
| `has_wasm_attr()` | Checks if a function/component has `@wasm` attribute | ~56 |
| `component_has_wasm_attr()` | Checks if component should compile to WASM | ~62 |

### Web Crate (`crates/web/src/lib.rs`)

| Function | Role in UI Pipeline |
|----------|--------------------|
| `generate_js()` | Delegates to kain_script JS codegen |
| `generate_ks()` | Delegates to kain_script KS codegen |
| `generate_ts()` | Delegates to kain_script TS codegen |
| `generate_wasm()` | Delegates to kain_wasm WASM codegen |
| `generate_hybrid()` | WASM + JS/TS hybrid (components marked @wasm → WASM, rest → JS/TS) |

---

## Layer 4: UI Runtime Crates

### `crates/ui/src/lib.rs` — 4,355 lines

The central UI runtime model. Defines the semantic interface graph, retained tree model, and all UI data types shared between compiler and runtime.

| Type/Function | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `UiNodeId` | Stable u64 identifier for retained UI tree nodes | ~20 |
| `UiSignalId` | Stable u64 identifier for reactive signals | ~25 |
| `UiRendererKind` | Native, Web, Slate, Debug | ~30 |
| `UiLayoutEngineKind` | Auto, Native, Yoga, LegacyEgui | ~42 |
| `UiRenderEngineKind` | Auto, Native, Skia, Wgpu, Shader, Browser, LegacyEgui | ~57 |
| `UiHostBackendKind` | Auto, Native, LegacyEgui, Imgui, RmlUi, Slint, Qt, Cef, Tauri | ~75 |
| `UiBackendCapabilities` | Declarative backend capability profile | ~92 |
| `UiNode` | Retained UI node — tag, id, children, attributes, attached data | ~120+ |
| `UiTree` | Retained semantic tree — root nodes, node map | ~500+ |
| `UiBuildOutput` | Compiler output containing UiTree + patches + systems | ~700+ |
| `UiSurface` | Surface descriptor — kind, composition mode, preferred backends, GPU shader ref | ~1,100+ |
| `UiSurfaceKind` | Widget2D, Widget3D, Image2D, ShaderCanvas, Viewport, DockPanel | ~1,120 |
| `UiSurfaceCompositionMode` | Over, Under, Replace, Subtract, Add, etc. | ~1,130 |
| `UiSurfaceRendererPreference` | Auto, Immediate, Retained, Hybrid | ~1,138 |
| `UiSurfaceShaderBinding` | Shader reference attached to a surface | ~1,145 |
| `UiRuntimeBundle` | Runtime UI bundle — tree + systems + metadata | ~1,700+ |
| `UiRuntimeMetadata` | Preferred host backends, layout/render engines | ~1,900+ |
| `UiThemeRegistry` | Theme token registry for semantic styling | ~2,400+ |
| `UiThemeToken` | Named color/size/font tokens | ~2,370 |
| `UiThemeScope` | Theme application scope | ~2,350 |
| `UiStyleState` | Resolved style state for a node | ~2,450 |
| `UiValue` | Semantic UI value (Length, Color, Image, Text, etc.) | ~1,600+ |
| `UiWidgetKind` | Widget classification for tag dispatch | ~2,560 |
| `UiLength`, `UiLengthUnit` | Dimension representation (px, pct, auto, fr) | ~1,500+ |
| `UiDockNode`, `UiDockPlacement` | Dock layout support | ~2,700+ |
| `UiAnimationTrack`, `UiAnimationTrigger` | UI animation model | ~2,900+ |
| `UiComputed`, `UiDerivedExpr` | Computed/derived state expressions | ~1,100+ |
| `UiRect`, `UiResolvedLayout` | Layout geometry | ~1,300+ |
| `UiPatch`, `UiTransaction` | UI patch stream for incremental updates | ~1,350+ |
| `UiSchedulerEntry`, `UiSchedulerPhase` | UI scheduler state machine | ~2,600+ |
| `UiEventRoute`, `UiEventPhase` | Event routing — Capturing, AtTarget, Bubbling | ~2,450+ |
| `UiTreeBuilder` | Builder for constructing UiTrees | ~2,800+ |
| `default_layout_for_tag()` | Default layout algorithm per widget tag | ~3,000 |
| `widget_kind_for_tag()` | Maps tag string to UiWidgetKind | ~2,560 |
| `render_debug_tree()` | Renders UiTree as text debug output | ~3,050 |
| `ui_runtime_systems_from_tree()` | Builds runtime systems (computed, animations, bindings) from tree | ~3,080 |
| `ui_surface_for_node()` | Resolves surface descriptor from a node subtree | ~3,103 |
| `ui_runtime_bundle_from_output()` | UiBuildOutput → UiRuntimeBundle conversion | ~3,500 |
| `ui_runtime_bundle_to_json()` | Serialization for host-side consumption | ~3,550 |
| `validate_ui_runtime_bundle()` | Validation of runtime bundle | ~3,600 |

### `crates/ui/src/runtime_execution.rs` — 1,348 lines

| Type/Function | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `UiRuntime` | Runtime entrypoint: tree + systems + indexes + transactions | ~35 |
| `UiRuntime::reload()` | Hot-reload transfer between old and new UiBuildOutput | ~70 |
| Runtime execution model | Owns mutation + invalidation + transaction authority | whole file |

### `crates/ui-native/src/` — Native (Qt) UI Host Adapter

| File | Role in UI Pipeline | Key Exports |
|------|--------------------|-------------|
| `app.rs` | `KainUiNativeAppConfig`, `KainUiNativeBackendPlan` — configures shell/document/devtools hosts, layout/render engines | ~144 lines |
| `session.rs` | `KainUiNativeSessionManifest` — serialized session manifest with native projection, authored surfaces, backend plan | ~270 lines |
| `qt_host.rs` | `launch_qt_quick_host()` — launches Qt Quick runtime process for native rendering. Handles Qt runtime discovery, manifest serialization, process spawn | ~389 lines |
| `lib.rs` | Re-exports app, session modules. Types: `KainUiNativeRuntimeBundle`, `KainUiNativeRuntimeMetadata` | 78 bytes |
| `main.rs` | Binary entry point (137 bytes — stub) | ~137 bytes |

### `crates/ui-tauri/src/lib.rs` — 1,006 lines

| Function/Type | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `KAIN_TAURI_BRIDGE_SCHEMA_VERSION` | Bridge manifest schema version | ~38 |
| `render_tauri_project_files()` | Generates Tauri project structure (Cargo.toml, tauri.conf.json, capabilities) | ~250 |
| `render_frontend_bridge_js()` | Generates JS bridge code for Tauri ↔ Kain communication | ~400 |
| `render_frontend_index_html()` | Generates HTML entry point for Tauri webview | ~550 |
| `build_tauri_bridge_manifest()` | Creates TauriBridgeManifest describing invoke commands and permissions | ~650 |
| `retarget_ui_runtime_bundle_for_tauri()` | Adapts UiRuntimeBundle for Tauri host (promotes Tauri surface hosts) | ~750 |
| `patch_hybrid_wasm_reference()` | Patches WASM path references in Tauri frontend descriptor | ~850 |
| `TauriBridgeManifest` | Full bridge manifest struct for Tauri command/permission generation | ~100 |
| `TauriPluginPreset` | Tauri plugin enums (App, Window, Webview, Event, Fs, Dialog, Http, etc.) | ~55 |

---

## Layer 5: Driver & Build Pipeline

### `crates/driver/src/lib.rs` — 5,688 lines

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `DriverSession` | Main compilation session — source → typed program → codegen dispatch | ~500+ |
| `collect_implicit_root_stdlib_modules_from_jsx()` | Detects JSX usage and auto-imports UI stdlib modules | ~2,384 |
| `resolve_root_component_name()` | Finds the root component for a compilation target | ~3,063 |
| `fallback_root_component()` | Fallback when no explicit root component specified | ~3,209 |
| `collect_component_names()` | Collects all component names from TypedProgram | ~3,299 |
| `root_component_for()` | Resolves root component for a given WorldSurfaceKind | ~3,347 |
| `required_world_surface_for_target()` | Maps CompileTarget → required WorldSurfaceKind (NativeUI, Headless) | ~3,174 |
| `build_ui_output_from_frontend_bundle_source()` | Builds UI output specifically for frontend bundle | ~1,991 |
| `world_root_component_for_target()` | World-aware root component resolution | ~3,226 |

### `crates/driver/src/native_app.rs` — 3,460 lines

| Function/Type | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `NativeAppBundleConfig` | Config for native app bundle: app_name, window_title, root_component, initial_window_size, include_spirv | ~70 |
| `NativeAppBundle` | Full native app bundle with all sidecar files | ~200+ |
| `discover_native_app_root_component()` | Finds root component for native app from program items + surface kind | ~961 |
| `build_native_app_reload_participants()` | Builds hot-reload participant list (bundles, shaders, sidecars) | ~500+ |
| Runtime bundle emission | Emits `native_app_bundle.json`, `kain_realtime_app_bundle.json`, `kain_shader_bundle.json`, etc. | ~1,000+ |

### `crates/driver/src/tauri_app.rs` — 1,251 lines

| Function/Type | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `TauriAppBundleConfig` | Config wrapping NativeAppBundleConfig for Tauri | ~40 |
| `build_tauri_native_app()` | Builds full Tauri app bundle (native + frontend + wasm + config) | ~300 |
| Frontend file generation | Generates `kain_runtime_contract.json`, `kain_realtime_app_bundle.json`, `kain_shader_bundle.json` for Tauri | ~600+ |

### `crates/driver/src/compute_residency.rs` — 517 lines

| Type | Role in UI Pipeline |
|------|--------------------|
| `ComputeResidencyBundle` | Compute shader residency metadata for GPU surfaces |
| `ComputeResidencyEntry` | Per-shader entry: key, stage, workgroup, dispatch, CUDA stream/graph policy |

### `crates/build/src/workspace.rs` — 267.9 KB large

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `resolve_native_ui_host_sidecars()` | Resolves native UI host sidecar executables (Qt runtime, etc.) | ~5,724 |

### `crates/build/src/native_link.rs` — 485 lines

| Function | Role in UI Pipeline |
|----------|--------------------|
| `native_link()` | Links native executable with `libkain_runtime` (C runtime). The C runtime includes all UI system code. |
| `NativeEmit` | Emit mode: Exe, SharedLib, StaticLib, Object |

### `crates/build/src/evaluated_build.rs` — 83.6 KB

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `normalize_build_surface_source()` | Normalizes build surface source path references | ~46 |

### `crates/cli/src/native_ui_build.rs`

| Function | Role in UI Pipeline |
|----------|--------------------|
| `resolve_native_ui_host_sidecars()` | CLI-level host sidecar resolution |
| `build_native_ui_artifacts()` | Orchestrates native UI artifact build |

### `crates/cli/src/kain_launcher.rs`

| Function | Role in UI Pipeline |
|----------|--------------------|
| `parse_native_ui_host_kind()` | CLI arg parsing for native- ui host kind (software, headless, winit) |

---

## Layer 6: Service API & Bridge

### `crates/service-api/src/`

| File | Role in UI Pipeline | Key Exports |
|------|--------------------|-------------|
| `lib.rs` | Top-level service API for editor/tooling frontends | check, format, completions, hover, definition, references, symbols |
| `index.rs` | Symbol indexing — `WorkspaceIndex`, `SymbolRecord`, `SymbolKind`, `CompletionKind` | used by UI tooling |
| `queries.rs` | LSP-like queries: `completions_at()`, `hover_at()`, `definition_at()`, `references_at()`, `semantic_tokens()` | UI editor support |
| `workspace.rs` | Workspace management: `ServiceHost`, `OpenDocumentParams`, `UpdateDocumentParams` | document lifecycle |
| `diagnostics.rs` | Diagnostic checking for service consumers | check support |
| `formatting.rs` | Document formatting | format support |
| `abi.rs` | Flat C ABI surface for Kain-authored tooling to call service API | FFI bridge |

### `crates/service-bridge/src/lib.rs`

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| `register()` | Registers all service functions (`kain_service_open_workspace`, `kain_service_hover_at`, `kain_service_completions_at`, `kain_service_format_document`) into Kain stdlib runtime | ~20 |
| `register_service_stdlib()` | Registers 14 builtin function metadata entries for std::kain | ~60 |
| `register_service_env()` | Registers Kain → Rust function implementations | ~300+ |

### `crates/semantic/src/expert.rs`

| Function | Role in UI Pipeline |
|----------|--------------------|
| Semantic diagnostic coprocessor — classifies failure modes for compiler errors including JSX, component, and surface-related diagnostics |

---

## Layer 7: GPU Runtime Executor Layer

### `crates/gpu-runtime/src/executor.rs` — 1,312 lines

| Type/Function | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `GpuComputeExecutor` | Trait for GPU compute dispatch (used by shader canvas surfaces) | ~80 |
| `VulkanComputeExecutor` | Vulkan `ash`-based compute dispatcher | ~200+ |
| `GpuRuntimeDispatchRequest` | ABI struct for dispatch requests (shader bundle, residency, dispatch size) | ~150 |
| `GpuRuntimeDispatchResult` | ABI struct for dispatch results (status, invocations, bindings) | ~175 |
| `kain_gpu_runtime_create()` | FFI entry: creates GPU runtime instance | ~50 |
| `kain_gpu_runtime_dispatch_primary_compute()` | FFI entry: dispatches a compute shader | ~200+ |

### `crates/gpu-runtime/src/bindings.rs` — 278 lines

| Type | Role in UI Pipeline | Key Lines |
|------|--------------------|-----------|
| `BarrierMetadata` | Pipeline barrier descriptors for precise GPU synchronization | ~15 |
| `GpuQueuePolicy` | Default vs. PreferAsyncCompute | ~60 |
| `GpuDescriptorKind` | StorageBuffer, Sampler2D, Uniform, etc. | ~75 |
| `GpuBindingAccess` | Read, Write, ReadWrite | ~90 |
| `GpuDispatchBinding` | Per-binding: name, descriptor set, binding, kind, access | ~120 |
| `GpuDispatchRequest` | Full dispatch request: shader path, residency, bindings, dispatch size | ~180 |

### `crates/gpu-runtime/src/nvidia_ptx.rs` — 2,692 lines

| Type/Function | Role in UI Pipeline | Key Lines |
|--------------|--------------------|-----------|
| `NvidiaPtxExecutor` | CUDA PTX dispatcher via `cuda` driver API FFI | ~100 |
| Dynamic CUDA function loading | `CuInit`, `CuDeviceGet`, `CuModuleLoadData`, `CuLaunchKernel`, etc. | ~50–150 |
| `kain_gpu_runtime_create_nvidia_ptx_primary()` | FFI entry: creates NVIDIA PTX executor | ~2,000+ |
| `kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted()` | FFI entry: dispatches persisted PTX kernel | ~2,300+ |

---

## Layer 8: 3D Scene & Rendering

### `crates/3d/src/` — 293 KB across 13 files

| File | Role in UI Pipeline | Description |
|------|--------------------|-------------|
| `lib.rs` | 3D crate root — surface, scene, renderer types | Kain3dSession, Kain3dRenderer |
| `scene.rs` | Scene graph — stable handles, queries, mutations | scene lifecycle |
| `renderer.rs` | Software renderer — triangle rasterization, depth buffer | Kain3dSoftwareRenderer |
| `wgpu_renderer.rs` | WGPU GPU renderer — Vulkan/D3D12/Metal via wgpu | Kain3dWgpuRenderer |
| `authoring.rs` | Scene authoring — primitives, manipulators, picking | authoring widgets |
| `interaction.rs` | 3D interaction — raycasting, manipulation, selection | interaction system |
| `host.rs` | Host runtime bridge for Kain-authored 3D apps | Kain3dSessionHost |
| `math.rs` | 3D math — frustum, transforms, bounding | math helpers |
| `primitive.rs` | Authored primitives — boxes, spheres, meshes, splines | authored primitives |
| `prelude.rs` | Convenience re-exports | prelude |
| `shader_bundle.rs` | WGSL shader bundle management | shader bundle |
| `shaders/viewport_surface.wgsl` | WGSL viewport surface shader for 3D scenes | WGSL shader source |

---

## Layer 9: Interop & Shared Memory

### `crates/interop/src/lib.rs` — 31.6 KB

| Type | Role in UI Pipeline | Description |
|------|--------------------|-------------|
| `SharedBufferMetadata` | Shared buffer descriptor for cross-runtime (UI ↔ GPU ↔ Python) | buffer interop |
| `SharedImageMetadata` | Shared image descriptor for UI ↔ GPU surface interop | image interop |
| `KainSharedBuffer` | Shared buffer lifecycle | buffer handle |
| `KainSharedImage` | Shared image lifecycle | image handle |
| `shared_buffer_gpu_binding_view()` | Creates GPU binding view from shared buffer | binding generation |

---

## Layer 10: External Host Bridges

### `crates/node/src/lib.rs` — 2,026 lines

| Function | Role in UI Pipeline | Key Lines |
|----------|--------------------|-----------|
| Node.js bridge for JS interop — provides `js_eval`, `js_call`, `js_import`, `js_getattr` to Kain code | ~40 |
| Used by Tauri/Web targets for JavaScript interop in UI | throughout |

### `crates/python/src/lib.rs`

| Function | Role in UI Pipeline |
|----------|--------------------|
| Python bridge — `py_import`, `py_call`, `py_getattr`. Can be used from UI-adjacent Kain code for data processing |

### `crates/browser/src/`

| Function | Role in UI Pipeline |
|----------|--------------------|
| WASM-compiled Kain compiler for in-browser use — enables browser-based UI editors and playgrounds |

---

## Complete Dependency Graph

```
                            ┌──────────────────────┐
                            │    C Runtime Native   │
                            │  (ui_system.c, etc.)  │
                            └───────────┬──────────┘
                                        │ loaded by
                            ┌───────────▼──────────┐
                            │   kain_runtime.lib   │
                            │  (static/shared lib) │
                            └───────────┬──────────┘
                                        │ linked by
              ┌─────────────────────────┼─────────────────────────┐
              │                         │                         │
   ┌──────────▼──────────┐   ┌──────────▼──────────┐   ┌──────────▼──────────┐
   │  LLVM native .exe   │   │  Rust transpiled    │   │  WASM module        │
   │  (vtable calls to   │   │  (string-based UI)  │   │  (component+jsx     │
   │   KainComponentSurface)│ │                     │   │   compiled to wasm) │
   └────────────────────┘   └────────────────────┘   └────────────────────┘
                                        ▲
                                        │ generated by
              ┌─────────────────────────┼──────────────────────────────┐
              │                         │                              │
   ┌──────────▼─────────────────────────▼──────────────────────────┐   │
   │                   sys-codegen (LLVM/Rust/C++/C codegen)       │   │
   │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐   │   │
   │  │ component.rs │  │ mod.rs       │  │ codegen_rust/mod.rs│   │   │
   │  │ (vtable IR)  │  │ (compile_jsx,│  │ (gen_component,    │   │   │
   │  │              │  │  compile_comp)│  │  gen_jsx)          │   │   │
   │  └──────────────┘  └──────────────┘  └────────────────────┘   │   │
   └───────────────────────────┬────────────────────────────────────┘   │
                               │                                       │
                    ┌──────────▼──────────┐     ┌───────────────────┐   │
                    │  driver crate       │     │  gpu crate        │   │
                    │  (native_app.rs,    │     │  (SPIR-V/PTX/     │   │
                    │   tauri_app.rs)     │     │   HLSL/WGSL codegen)   │
                    └──────────┬──────────┘     └──────────┬────────┘   │
                               │                           │            │
                    ┌──────────▼──────────┐     ┌──────────▼────────┐   │
                    │  core crate (front) │     │  gpu-runtime      │   │
                    │                    │     │  (Vulkan/CUDA     │   │
                    │  ┌──────────────┐  │     │   executor)        │   │
                    │  │ parser.rs    │  │     └───────────────────┘   │
                    │  │ (parse_comp, │  │                              │
                    │  │  parse_jsx*) │  │     ┌───────────────────┐   │
                    │  ├──────────────┤  │     │  shader-text      │   │
                    │  │ ast.rs       │  │     │  (HLSL/WGSL/USF   │   │
                    │  │ (Component,  │  │     │   shared lowering) │   │
                    │  │  JSXNode)    │  │     └───────────────────┘   │
                    │  ├──────────────┤  │                              │
                    │  │ types.rs     │  │     ┌───────────────────┐   │
                    │  │ (TypedComp,  │  │     │  ui crate         │   │
                    │  │  check_comp) │  │     │  (UiNode, UiTree, │   │
                    │  ├──────────────┤  │     │   UiBuildOutput,  │   │
                    │  │ ui.rs        │  │     │   UiSurface, etc.) │   │
                    │  │ (VNode,      │  │     └────────┬──────────┘   │
                    │  │  eval_jsx,   │  │              │              │
                    │  │  build_ui_)  │  │     ┌────────▼──────────┐   │
                    │  ├──────────────┤  │     │  ui-native        │   │
                    │  │ realtime_    │  │     │  (Qt host,        │   │
                    │  │ app_bundle   │  │     │   native session) │   │
                    │  └──────────────┘  │     └───────────────────┘   │
                    └────────────────────┘                              │
                                                                        │
                    ┌────────────────────┐     ┌────────────────────┐   │
                    │  ui-tauri          │     │  wasm crate        │   │
                    │  (Tauri bridge,    │     │  (compile_component│   │
                    │   frontend gen)    │     │   compile_jsx_node)│   │
                    └────────────────────┘     └────────────────────┘   │
                                                                        │
                    ┌────────────────────┐     ┌────────────────────┐   │
                    │  script crate      │     │  web crate         │   │
                    │  (JS/TS/KS comp+jsx)│     │  (hybrid + deleg)  │   │
                    └────────────────────┘     └────────────────────┘   │
                                                                        │
                    ┌────────────────────┐     ┌────────────────────┐   │
                    │  ue5/* crates      │     │  3d crate          │   │
                    │  (UE5 comp/shader  │     │  (scene, renderer, │   │
                    │   blueprint codegen)│     │   wgpu renderer)   │   │
                    └────────────────────┘     └────────────────────┘   │
                                                                        │
                    ┌────────────────────┐                              │
                    │  stdlib/ui*.kn     │←──── Kain-authored UI ───────┘
                    └────────────────────┘       surface code
```

### Key Data Flow

```
Kain Source (component + JSX)
    │
    ▼
Parser (crates/core/src/parser.rs)
    │  parse_component(), parse_jsx*()
    ▼
AST (crates/core/src/ast.rs)
    │  Component, JSXNode, JSXAttribute, StateDecl
    ▼
Typechecker (crates/core/src/types.rs)
    │  check_component(), check_jsx_semantics()
    ▼
TypedProgram with TypedComponent + TypedShader
    │
    ├──→ Interpreter (crates/core/src/ui.rs, runtime.rs)
    │       eval_jsx() → VNode → runtime UI
    │
    ├──→ LLVM Codegen (crates/sys-codegen/src/codegen_llvm/)
    │       compile_component_render() → vtable calls → KainComponentSurface
    │       compile_shader() → SPIR-V globals + LLVM IR
    │       compile_world_initializer() → surface loop
    │       compile_jsx*() → element_begin/set_attr/element_end vtable calls
    │
    ├──→ Rust Codegen (crates/sys-codegen/src/codegen_rust/)
    │       gen_component() → Rust struct + string-based UI
    │
    ├──→ WASM Codegen (crates/wasm/src/codegen_wasm.rs)
    │       compile_component() → WASM bytecode with JSX rendering
    │
    ├──→ JS/TS/KS Codegen (crates/script/)
    │       gen_component() / gen_jsx() → JSX virtual DOM calls
    │
    ├──→ Hybrid Web (crates/web/src/codegen_hybrid.rs)
    │       @wasm components → WASM, rest → JS/TS
    │
    ├──→ UE5 Codegen (crates/ue5*/)
    │       component → UObject subclass + Blueprint
    │       shader → USF + C++ host
    │
    └──→ GPU Shader Codegen (crates/gpu/, crates/shader-text/)
            SPIR-V / PTX / HLSL / WGSL / USF emission
```

### Legend

- **C Runtime files**: Live in `runtime/native/` — 21 files mapped in original document
- **Rust crate files**: Live in `crates/*/` — **~95 files mapped in this document**
- **Stdlib Kain files**: Live in `stdlib/` — 6 files mapped in this document
- Total file count: ~122 files spanning the full UI pipeline
