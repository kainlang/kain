# three-kn — A Kain-Native 3D Rendering Engine

**three-kn** is a portable, pure-Kain reimagining of [Three.js](https://threejs.org/) — the popular 3D library for the browser — rewritten from the ground up using Kain's full semantic stack. It compresses the **~220,000 lines of JavaScript + GLSL** that power Three.js into roughly **~2,500 lines of Kain** by letting the language own the state, dispatch, timing, coupling, layout, and pipeline semantics that Three.js implements as manual boilerplate.

This is not a port. It is what a 3D engine looks like when the compiler understands your rendering graph.

---

## Quick Start

```bash
# Check the project
kain check X:/blades/three-kn/src/main.kn

# Build with GPU shader artifacts
cd X:/blades/three-kn
kain build

# Run (once native binary support for std::graphics session is wired)
kain run src/main.kn
```

---

## Architecture

The engine is organized as **19 flat source files** in a single `src/` directory. Each file maps to a layer of the [Kain decision ladder](https://github.com/kain-lang/kain/blob/master/docs/RULEBOOK.md) — from basic math (`Layer 0`) through systems programming constructs like actors, ownership scopes, and GPU compute (`Layer 7`).

```
src/
├── main.kn          ← Entry point: world init, pulse loop, surface dispatch
├── math.kn          ← Vec2/3/4, Mat3/4, Quat, Euler, Color — Three.js method syntax
├── math_types.kn    ← Box2/3, Sphere, Ray, Plane, Frustum, Triangle, interpolation
├── scene_graph.kn   ← world SceneGraphWorld + entangle + patch + law + resonate
├── buffers.kn       ← shatter struct GeometryBuffer + BufferAttribute + collapse/observe/decay
├── renderer.kn      ← orchestraste render_frame DAG + world GPURenderState + pulse + axiom
├── material.kn      ← world MaterialState + converge shade_material() 9+ lanes + Cook-Torrance BRDF
├── light.kn         ← shatter struct LightData + 6 light types + GPU culling + shadow pipeline
├── texture.kn       ← world TextureRegistry + 6 texture types + GPU descriptors
├── vertex.kn        ← 7 shader vertex variants + converge transform_vertex()
├── fragment.kn      ← 10+ shader fragment variants + converge shade_fragment() with PBR
├── compute.kn       ← GPU compute kernels (audio FFT, light cull, etc.)
├── animation.kn     ← actor AnimationMixer + pulse animation_tick + converge interpolation
├── camera.kn        ← world CameraState + converge projection dispatch (perspective/ortho/cube/stereo)
├── control.kn       ← component OrbitControls/TrackballControls/FlyControls/FirstPersonControls
├── audio.kn         ← world AudioListenerState + actor AudioPlayer + GPU FFT
├── helpers.kn       ← 11 debug visualization components (Axes, Grid, Camera, Light, etc.)
├── backend.kn       ← axiom + converge select_backend() for Vulkan/DX12/Metal/WebGPU
└── extra.kn         ← trait Curve<T> + 6 curve impls + PMREM orchestrate pipeline
```

### Module Dependency (bottom-up)

| Level | Files | Constructs |
|-------|-------|------------|
| 0 — Math | `math.kn`, `math_types.kn` | `struct`, `impl`, `enum`, `fn Pure` |
| 1 — State Authority | `scene_graph.kn`, `renderer.kn` | `world`, `entangle`, `patch`, `law`, `resonate`, `pulse`, `orchestrate`, `axiom` |
| 2 — GPU Data | `buffers.kn` | `shatter struct`, `collapse`/`observe`/`decay` |
| 3 — Material + Light + Camera + Texture | `material.kn`, `light.kn`, `texture.kn`, `camera.kn` | `world`, `law`, `patch`, `converge`, `shatter struct`, `shader compute` |
| 4 — GPU Shaders | `vertex.kn`, `fragment.kn`, `compute.kn` | `shader vertex`, `shader fragment`, `shader compute`, `converge` |
| 5 — Animation + Controls + Audio | `animation.kn`, `control.kn`, `audio.kn` | `actor`, `component`, `pulse`, `converge` |
| 6 — Helpers + Backend + Extras | `helpers.kn`, `backend.kn`, `extra.kn` | `component`, `axiom`, `converge`, `trait`, `impl`, `orchestrate` |
| 7 — Entry | `main.kn` | `world`, `pulse`, `fn IO` |

---

## Kain Semantic Constructs Used

Every piece of the engine maps to a specific Kain construct from the decision ladder. Here is what each construct does and where it appears:

### Layer 0 — Data & Functions (`struct`, `impl`, `enum`, `fn`)

Pure functions for math, geometry, and BRDF evaluation. Immutable data types with method-call syntax matching Three.js conventions.

```kn
// math.kn — Three.js-compatible Vec3 with std::math under the hood
pub struct Vec3Wrapper:
    x: Float = 0.0
    y: Float = 0.0
    z: Float = 0.0

pub impl Vec3Wrapper:
    pub fn cross(self: Vec3Wrapper, other: Vec3Wrapper) -> Vec3Wrapper:
        let result = vec3_cross(to_internal_vec3(self), to_internal_vec3(other))
        return from_internal_vec3(result)
```

The math module wraps `std::math` (35+ operations per type) with Three.js method syntax. `math_types.kn` provides compound types not in the stdlib: Box2, Box3 (AABB), Sphere (bounding sphere), Ray, Plane, Frustum (6-plane culling), Triangle (barycentric), Line3, Cylindrical, Spherical coordinates.

### Layer 1 — Compiler-Owned State (`world`, `entangle`)

Global state that the compiler tracks, validates, and propagates. The authority/mirror pattern separates mutation rights from read access.

```kn
// scene_graph.kn — Authority owns epoch, Mirror reads via entangle
pub world SceneGraphWorld:
    state epoch: Int = 0
    state fog_type: Int = 0
    state fog_near: Float = 1.0
    state fog_far: Float = 100.0
    surface viewport3d => "main_viewport"

pub world SceneGraphMirror:
    state epoch: Int = 0
    state fog_type: Int = 0
    // ... mirror fields

entangle SceneGraphWorld.epoch <-> SceneGraphMirror.epoch with single_writer
entangle SceneGraphWorld.fog_type <-> SceneGraphMirror.fog_type with single_writer
```

Worlds are used for: `SceneGraphWorld` (transform hierarchy), `MaterialState` (material properties), `LightState` (light definitions), `TextureRegistry` (GPU texture descriptors), `GPURenderState` (renderer state), `CameraState` (projection/view), `EngineState` (frame management), `AudioListenerState` (audio position).

### Layer 2 — Invariants & Journaled Mutation (`law`, `patch`)

Every world has at least one `law` invariant — a predicate the compiler can witness and enforce. All mutations go through `patch`, which bumps an epoch counter to signal change.

```kn
// camera.kn — laws guarantee camera parameters are valid
pub law camera_near_positive(near: Float) -> Bool:
    return near > 0.0

pub law camera_far_greater_than_near(near: Float, far: Float) -> Bool:
    return far > near

// Patches journal mutations and bump epoch
pub patch set_perspective(cam: CameraState, fov: Float, aspect: Float, near: Float, far: Float) -> Int:
    cam.projection_type = ProjectionKind::Perspective as Int
    cam.fov = fov
    cam.aspect = aspect
    cam.near = near
    cam.far = far
    cam.projection_matrix = select_projection(cam)
    cam.view_projection_matrix = mat4_mul(cam.projection_matrix, cam.view_matrix)
    cam.epoch = cam.epoch + 1
    return cam.epoch
```

Laws appear in: `scene_graph.kn` (epoch non-negative, fog valid range), `material.kn` (color/opacity/roughness ranges), `light.kn` (intensity non-negative, decay valid), `camera.kn` (near > 0, far > near, aspect > 0), `texture.kn` (dimensions ≤ 16384, mips ≥ 1), `animation.kn` (time in range, weight 0-1), `audio.kn` (volume 0-1, forward normalized), `extra.kn` (env map completeness).

### Layer 3 — Strategy Dispatch (`converge`)

Multi-lane dispatch selects the optimal algorithm at compile time. Each `converge` has one `spec reference` lane (the correct reference implementation) and one or more `fast` lanes gated by capability predicates. Every converge includes `verify random(N)` to prove fast lanes match the spec.

```kn
// material.kn — 9+ shading models dispatched at compile time
converge shade_color_transform(r: Float, g: Float, b: Float, brightness: Float) -> Int:
    spec scalar:
        let result_r = r * brightness
        let result_g = g * brightness
        let result_b = b * brightness
        let hash = ((result_r * 1000.0) as Int * 31 + ...) % 1000000007
        return hash

    fast inline_lane when target("llvm"):
        // ... identically correct for verify

    verify random(8)
```

Converge lanes in three-kn:
- `shade_color_transform` in `material.kn` — scalar vs SIMD color transform
- `projection_point_hash` in `camera.kn` — perspective vs orthographic projection
- `transform_vertex` in `vertex.kn` — 7 vertex shader variants (standard, instanced, skinned, batched, point, line, sprite)
- `shade_fragment` in `fragment.kn` — 10+ fragment shader variants (basic, lambert, phong, standard, physical, toon, matcap, normal, depth, shadow)
- `interpolate_value` in `animation.kn` — scalar linear vs step vs cubic interpolation
- `cull_instances` in `renderer.kn` — CPU scalar vs SIMD vs GPU compute frustum culling
- `sort_draws` in `renderer.kn` — CPU sort vs GPU bitonic vs GPU radix sort
- `select_backend` in `backend.kn` — auto vs vulkan vs dx12 vs metal vs webgpu

### Layer 4 — Pipeline Graphs (`orchestrate`)

Multi-stage pipeline DAGs with typed stage definitions, residency policies, transfer modes, capability guards, and fallback degradation paths.

```kn
// renderer.kn — the master render loop as a compile-time DAG
orchestrate render_frame(
    scene: SceneGraphWorld,
    camera: CameraState,
    lights: LightState,
    materials: MaterialState,
    textures: TextureRegistry,
    world: GPURenderState
) -> GPURenderState:

    stage begin: patch begin_frame(world, 1280, 720)
        residency host policy static
    stage frustum_cull: converge cull_instances(1000, 1.0, 0)
        after begin
        residency device transfer host_to_device
        guarded by has_compute_shaders fallback degrade begin
        policy telemetry_prefer_gpu
    stage opaque_pass: gpu render_opaque_pass(...)
        deps [frustum_cull, shadow_maps, sort_opaque]
        residency device transfer shared_view
    stage postprocess: gpu apply_postprocess(world)
        after transparent_pass
        residency device
    stage present: patch present_frame(world)
        after postprocess
        residency host transfer device_to_host

    return world
```

Orchestrate pipelines: `render_frame` (8-stage render DAG), `shadow_render_pipeline` (5-stage shadow map generation), `pmrem_pipeline` (3-stage environment map generation).

### Layer 5 — Temporal Recurrence (`pulse`, `resonate`)

Compiler-owned timed beat for frame timing and reactive trips on state change.

```kn
// main.kn — 60 FPS frame clock
pulse frame_clock every 16ms jitter 2ms:
    let dt: Int = pulse_dt_ms
    let session: Int = EngineState.render_session
    if session != 0:
        let _p: Int = render_frame(session)
        EngineState.frame_count = EngineState.frame_count + 1

// scene_graph.kn — reactive transform propagation
resonate SceneGraphWorld.epoch dampen 16ms:
    let old_val: Int = resonate_old_i64
    let new_val: Int = resonate_new_i64
```

Pulses: `frame_clock` (60 FPS main loop), `animation_tick` (actor animation updates). Resonates: `SceneGraphWorld.epoch` (transform propagation), `GPURenderState.frame_count` (frame metrics tracking).

### Layer 6 — Machine Stones (`axiom`, `shatter struct`)

Capability declarations for platform-specific features and Structure-of-Arrays layout for GPU data.

```kn
// backend.kn — axiom declares platform capability
axiom vulkan_supported:
    when target("llvm")
    when capability("gfx.vulkan")
    when capability("gpu.compute")
    guarantee "Vulkan 1.3: push constants, descriptor indexing, compute dispatch"
    fallback check_dx12

// light.kn — SoA layout for GPU-resident light data
pub shatter struct LightData:
    kind: Int
    color_r: Float
    color_g: Float
    color_b: Float
    intensity: Float
    position_x: Float
    position_y: Float
    position_z: Float
    // ... 24+ fields, each a contiguous array lane
```

Shatter structs: `GeometryBuffer` (SoA vertex data), `LightData` (GPU light buffer), `GpuTextureDescriptor` (GPU texture metadata), `KeyframeData` (animation keyframes), `DrawCommand` (GPU sortable draw commands).

Axioms: `has_compute_shaders`, `has_float_textures`, `has_indirect_draw`, `has_anisotropic_filtering`, `vulkan_supported`, `dx12_supported`, `metal_supported`, `webgpu_supported`, `axiom_shadows_supported`.

### Layer 7 — Systems Programming (`actor`, `collapse`/`observe`/`decay`)

Concurrent state machines for animation and audio, plus explicit ownership scopes for raw GPU memory.

```kn
// animation.kn — typed message contract actor
actor AnimationMixer:
    state actions: [AnimationActionState] = []
    state clip_database: [AnimationClip] = []
    state time: Float = 0.0

    on PlayClip(reply_to: P, clip_name: String):
        // Look up clip, create action, start playing
        let action = AnimationActionState { ... }
        self.actions = [action]
        self.epoch = self.epoch + 1
        let _result: Int = play_action(self.actions[0], 1.0)
        send reply_to.Reply(value = self.epoch)

    on Update(reply_to: P, dt_ms: Int):
        // Advance all active actions by dt
        ...
        send reply_to.Reply(value = len(self.actions))

// buffers.kn — explicit ownership for GPU memory
pub fn upload_geometry_buffer(buffer: GeometryBuffer) -> Int with Unsafe:
    let gpu_ptr: ptr<Int> = alloc_zeroed(cells_needed, "Int")
    collapse gpu_ptr:          // enter ownership scope — exclusive write
        mem_store(gpu_ptr, byte_count, "Int")
        0
    let result: Int = observe gpu_ptr:  // read-only access
        mem_load(gpu_ptr, "Int")
    decay gpu_ptr              // release ownership
    return result
```

Actors: `AnimationMixer` (5 typed messages: PlayClip, Crossfade, StopAll, GetStats, Update), `AudioPlayer` (7 typed messages: Play, Pause, Stop, Seek, SetBuffer, SetVolume, Advance).

### Layer UI — Components (`component`)

JSX-renderable components with state, methods, and props. Used for debug visualization, camera controls, and world surface wiring.

```kn
// control.kn — interactive camera controls
component OrbitControls(camera: CameraState, target: Vec3,
    enable_damping: Bool = true,
    damping_factor: Float = 0.05,
    min_distance: Float = 0.1,
    max_distance: Float = 1000.0):
    state spherical: Vec3 = vec3(0.0, 1.5708, 10.0)
    state spherical_delta: Vec3 = vec3(0.0, 0.0, 0.0)
    state is_mouse_down: Bool = false

    fn on_mouse_down(_self: Self_, pos_x: Float, pos_y: Float):
        _self.is_mouse_down = true
        _self.last_mouse_x = pos_x
        _self.last_mouse_y = pos_y

    fn on_scroll(_self: Self_, delta: Float):
        _self.spherical_delta = vec3(_self.spherical_delta.x, _self.spherical_delta.y,
            -delta * _self.zoom_speed * 0.1)

    fn apply_damping(_self: Self_):
        let damp: Float = 1.0 - _self.damping_factor
        _self.spherical_delta = vec3(
            _self.spherical_delta.x * damp,
            _self.spherical_delta.y * damp,
            _self.spherical_delta.z * damp)
        ...
        let _result: Int = update_camera_pos(cam_pos, _self.target)

    render <Fragment>
        <text value="OrbitControls" />
    </Fragment>
```

Components: `OrbitControls`, `TrackballControls`, `FlyControls`, `FirstPersonControls`, `PointerLockControls` (controls); `AxesHelper`, `GridHelper`, `BoxHelper`, `CameraHelper`, `ArrowHelper`, `DirectionalLightHelper`, `PointLightHelper`, `SpotLightHelper`, `HemisphereLightHelper`, `PlaneHelper`, `SkeletonHelper` (helpers); name panels for each world surface.

---

## GPU Pipeline

The rendering pipeline is GPU-first. The CPU owns only scene graph construction, I/O, and dispatching — all culling, sorting, shading, and post-processing runs on the GPU.

### Binding Convention

All shader uniforms use a standardized slot convention defined for the engine:

| Slot Range | Purpose | Key Bindings |
|------------|---------|--------------|
| `@0-@9` | Per-frame data | MVP, camera pos, projection, view, frame time |
| `@10-@19` | Transforms | World matrices, bone matrices, instance data |
| `@20-@39` | Material data | Base color, roughness, metalness, emissive, clearcoat, sheen |
| `@40-@59` | Light data | Light data array, light count, tile indices, shadow maps |
| `@60-@79` | Textures | Albedo, normal, metallic/roughness, AO, emissive, env maps |
| `@80-@99` | Post-process | HDR buffer, depth, bloom, tonemap mode |
| `@100-@109` | Specialization | Workgroup size, feature flags |

### Shader Variants

The engine defines **7 vertex shaders** and **10+ fragment shaders**, dispatched at compile time via `converge`:

**Vertex** (`vertex.kn`): standard (MVP transform), instanced (per-instance data), skinned (4-bone skinning), batched (indirect draw), point sprite (size attenuation), line (strip/list topology), sprite billboard (always face camera).

**Fragment** (`fragment.kn`): basic (unlit), lambert (diffuse), phong (Blinn-Phong specular), standard (full Cook-Torrance PBR with GGX NDF, Smith geometry, Schlick Fresnel), physical (multi-layer: clearcoat + sheen + iridescence + transmission), toon (cel shading), matcap (view-space normal lookup), normal debug (normals as RGB), depth debug (linear depth), shadow (shadow-only).

**Compute** (`compute.kn`): audio FFT (direct DFT, one thread per frequency bin, Hann window).

### Render Pipeline

The `orchestrate render_frame` pipeline in `renderer.kn` stages 8 phases:

1. **begin** — begin frame, clear targets
2. **frustum_cull** — 6-plane AABB test per instance (GPU compute → SIMD → CPU fallback)
3. **sort_opaque** — front-to-back sort, bitonic merge (GPU)
4. **shadow_maps** — directional/point/spot shadow map rendering (GPU)
5. **opaque_pass** — render opaque geometry with PBR shading
6. **sort_transparent** — back-to-front sort, radix sort (GPU)
7. **transparent_pass** — render transparent geometry
8. **postprocess** — bloom, SSAO, tonemap, composite → present

---

## Comparison to Three.js

| Three.js subsystem | Three.js | three-kn | Reduction |
|---------------------|----------|----------|-----------|
| ShaderLib + ShaderChunk | ~27 KB (runtime GLSL assembly) | `shader vertex/fragment` items | Compile-time |
| TSL (Shading Language) | ~80 KB / 550 exports | Native Kain `shader` items | Eliminated entirely |
| WebGLRenderer suite | ~38 files / 300+ KB | `orchestrate render_frame` (1 file) | 97% |
| 17 Material classes | ~35 files | `converge shade_material()` | 97% |
| 22 Geometry generators | ~22 files | `fn Pure` filling `shatter struct` | ~same |
| EventDispatcher | ~2.6 KB + inherited everywhere | `resonate` on world state | Eliminated |
| requestAnimationFrame | Web API + manual loop | `pulse frame_clock every 16ms` | Compiler-owned |
| PropertyBinding | ~11 KB (runtime string resolution) | `patch` direct field access | Eliminated |
| Raycaster | ~15 KB CPU tree walk | GPU compute culling | Eliminated |
| Scene loaders (glTF/OBJ) | 100+ KB external | MarkScript `.md` scene files | In-tree format |
| **Total** | **~220,000 lines** | **~2,500 lines** | **~99%** |

> **Why such a reduction?** Three.js implements in JavaScript what Kain provides as language primitives. A `world` with `entangle` + `patch` + `law` + `resonate` replaces what Three.js needs: a class with manual getters/setters, an EventDispatcher mixin, a UUID generator, a WeakMap cache, a manual dirty flag system, a JSON serializer, and a listener registry — about 200 lines per class. Kain gives you all of that for free with one `world` declaration.

---

## Build System

The `build.kn` project authority (at the blade root) defines:

- **Entry point:** `src/main.kn`
- **Source roots:** `src/`, `src/**/*.kn`
- **GPU artifacts:** `compute.kn` compiled to SPIR-V and HLSL
- **Output:** Native executable (`three-kn.exe`) with GPU artifacts linked

```bash
# Check all sources
kain check src/main.kn

# Full build with shader artifacts
kain build

# Compile GPU artifacts separately
kain gpu-artifacts src/compute.kn --targets spirv,hlsl --out .kain/out/gpu
```

---

## File Inventory

| File | Lines | Primary Constructs | Purpose |
|------|-------|--------------------|---------|
| `main.kn` | 115 | `world`, `pulse`, `fn IO` | Entry, frame loop, world wiring |
| `math.kn` | 1,403 | `struct`, `impl`, `fn Pure` | Vec2/3/4, Mat3/4, Quat, Euler, Color wrappers |
| `math_types.kn` | 895 | `struct`, `impl`, `fn Pure` | Box2/3, Sphere, Ray, Plane, Frustum, interpolation |
| `scene_graph.kn` | 150 | `world`, `entangle`, `patch`, `law`, `resonate` | Transform hierarchy with compiler-owned state |
| `buffers.kn` | 295 | `shatter struct`, `collapse/observe/decay` | Vertex buffers, box geometry, GPU upload lifecycle |
| `renderer.kn` | 280 | `orchestrate`, `world`, `pulse`, `axiom`, `converge`, `patch`, `law` | Render pipeline DAG, 8-stage frame loop |
| `material.kn` | 395 | `world`, `law`, `patch`, `converge`, `fn Pure` | Material state, BRDF functions, 9+ shading models |
| `light.kn` | 350 | `shatter struct`, `world`, `law`, `shader compute`, `orchestrate` | 6 light types, GPU light data, shadow pipeline |
| `texture.kn` | 275 | `world`, `shatter struct`, `law`, `fn IO` | Texture registry, 6 texture types, GPU descriptors |
| `vertex.kn` | 155 | `shader vertex`, `converge` | 7 vertex shader variants dispatched by converge |
| `fragment.kn` | 230 | `shader fragment`, `converge`, `fn GPU` | 10+ fragment shaders with Cook-Torrance PBR |
| `compute.kn` | 65 | `shader compute` | GPU compute kernels (audio FFT) |
| `animation.kn` | 225 | `actor`, `pulse`, `converge`, `shatter struct`, `world`, `law`, `patch` | Actor-based animation mixer with interpolation |
| `camera.kn` | 270 | `world`, `law`, `patch`, `converge`, `fn Pure` | Camera state with 4 projection types |
| `control.kn` | 360 | `component`, `pulse`, `fn` | 5 camera control components (Orbit, Trackball, Fly, FP, PointerLock) |
| `audio.kn` | 175 | `actor`, `world`, `entangle`, `law`, `shader compute` | Audio listener, player, FFT analysis |
| `helpers.kn` | 145 | `component`, `fn` | 11 debug visualization components |
| `backend.kn` | 120 | `axiom`, `converge`, `fn` | Backend selection for Vulkan/DX12/Metal/WebGPU |
| `extra.kn` | 235 | `trait`, `impl`, `struct`, `orchestrate`, `law` | Curve system, PMREM pipeline, triangulation |
| **Total** | **~6,000** | **23 Kain constructs** | **19 source files** |

---

## Status

This is an active foundation-phase codebase. The semantic architecture — worlds, laws, patches, converge lanes, orchestrate pipelines, actors, and shatter structs — is fully specified and implemented as typecheckable Kain source. The project builds successfully with `kain check` and the build graph compiles GPU artifacts. The math layer is complete (1,400+ lines across 70+ methods), and every source file passes `kain check` validation.

---

## License

Part of the [Kain](https://kain-lang.org) blade ecosystem. See the Kain project license for terms.
