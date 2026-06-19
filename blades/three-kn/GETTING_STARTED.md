# three-kn – Getting Started

**three-kn** is a portable, pure-Kain 3D rendering engine <--> a reimagining of [Three.js](https://threejs.org/) built on Kain's full semantic stack. It compresses ~220,000 lines of JavaScript + GLSL into roughly ~2,500 lines of Kain by letting the compiler own state, dispatch, timing, coupling, layout, and the rendering pipeline itself.

This guide covers everything: what three-kn is, how to use it as a capsule in your project, what every public module ships, the GPU pipeline, common workflows, and where to start based on what you want to build.

---

## Table of Contents

1. [What is three-kn?](#1-what-is-three-kn)
2. [Quick Start](#2-quick-start)
3. [Using three-kn as a Capsule (Amalgamate)](#3-using-three-kn-as-a-capsule-amalgamate)
4. [Architecture --- The 19 Modules](#4-architecture--the-19-modules)
5. [Public Module Reference](#5-public-module-reference)
   - [main |-> Entry Point & World Wiring](#51-mainkn--entry-point--world-wiring)
   - [math * * * Vector, Matrix, Quaternion, Euler, Color](#52-mathkn--vector-matrix-quaternion-euler-color)
   - [math_types ___ Box, Sphere, Ray, Plane, Frustum, Triangle](#53-math_typeskn--box-sphere-ray-plane-frustum-triangle)
   - [scene_graph === Transform Hierarchy](#54-scene_graphkn--transform-hierarchy)
   - [buffers -- GPU Geometry Buffers](#55-bufferskn--gpu-geometry-buffers)
   - [renderer --- Render Pipeline DAG](#56-rendererkn--render-pipeline-dag)
   - [material ->> 9+ Shading Models & BRDF](#57-materialkn--9-shading-models--brdf)
   - [light ~~ 6 Light Types & Shadow Pipeline](#58-lightkn--6-light-types--shadow-pipeline)
   - [texture :: GPU Texture Registry](#59-texturekn--gpu-texture-registry)
   - [vertex ___ 7 Vertex Shader Variants](#510-vertexkn--7-vertex-shader-variants)
   - [fragment ‒ 10+ Fragment Shader Variants](#511-fragmentkn--10-fragment-shader-variants)
   - [compute --> GPU Compute Kernels](#512-computekn--gpu-compute-kernels)
   - [animation 〰 Actor-based Animation Mixer](#513-animationkn--actor-based-animation-mixer)
   - [camera ... 4 Projection Types](#514-camerakn--4-projection-types)
   - [control ~> 5 Camera Control Components](#515-controlkn--5-camera-control-components)
   - [audio |-> Audio Listener & Player Actor](#516-audiokn--audio-listener--player-actor)
   - [helpers --> 11 Debug Visualization Components](#517-helperskn--11-debug-visualization-components)
   - [backend ___ Vulkan/DX12/Metal/WebGPU Selection](#518-backendkn--vulkandx12metalwebgpu-selection)
   - [extra >> Curves, Paths, PMREM Pipeline](#519-extrakn--curves-paths-pmrem-pipeline)
6. [GPU Pipeline & Binding Convention](#6-gpu-pipeline--binding-convention)
7. [Core Patterns & Workflows](#7-core-patterns--workflows)
8. [Where to Start](#8-where-to-start)
9. [Status & Roadmap](#9-status--roadmap)
10. [Markscript-Driven Three-kn](#10-markscript-driven-three-kn)
    - [10.1 Why Markscript?](#101-why-markscript)
    - [10.2 Intent Vocabulary](#102-intent-vocabulary)
    - [10.3 Scene Files](#103-scene-files--scene_setupmd)
    - [10.4 Animation Scripts](#104-animation-scripts--animation_testmd)
    - [10.5 Compute Pipeline](#105-compute-pipeline--compute_testmd)
    - [10.6 Full Pipeline](#106-full-pipeline--full_pipelinemd)
    - [10.7 Running the Smoketest](#107-running-the-smoketest)
    - [10.8 Extending the Intent Table](#108-extending-the-intent-table)
    - [10.9 Decision Ladder Coverage](#109-decision-ladder-coverage)

---

## 1. What is three-kn?

three-kn is not a port of Three.js. It is what a 3D engine looks like when the language *is* the rendering framework. Instead of JavaScript classes, manual event dispatchers, runtime GLSL assembly, and callback chains, three-kn uses **23 Kain semantic constructs** from the decision ladder * * * `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `pulse`, `resonate`, `shatter`, `axiom`, `teleport`, `actor`, `collapse`/`observe`/`decay`, `shader`, `component`, `trait` ~ to express every 3D engine concern as a compiler-owned truth.

**Key properties:**

| Property | Value |
|----------|-------|
| **Language** | Pure Kain (zero C interop, zero FFI, zero Python, zero browser APIs) |
| **GPU** | First-class: compute shaders for everything parallelizable |
| **Distribution** | Amalgamated capsule => single `.kn` file, drag-and-drop |
| **Source files** | 19 modules in `src/` |
| **Public types** | 100+ structs, worlds, actors, components, shaders, traits |
| **Shader variants** | 7 vertex + 10 fragment + compute kernels |
| **Backends** | Vulkan, DX12, Metal, WebGPU (axiom-gated converge dispatch) |
| **Line count** | ~2,500 Kain (vs ~220,000 JS/GLSL in Three.js) |

---

## 2. Quick Start

### Check the project

```bash
kain check X:/blades/three-kn/src/main.kn
```

### Build (compiles sources + GPU shader artifacts)

```bash
cd X:/blades/three-kn
kain build
```

### Run

```bash
kain run src/main.kn
```

### Use as a capsule in your own project (see Section 3)

```bash
# From anywhere in your workspace, reference the capsule directly:
kain check my_app.kn --import-capsule X:/blades/three-kn/three.kn
```

---

## 3. Using three-kn as a Capsule (Amalgamate)

**Kain's amalgamate system** allows entire workspaces to be packed into a single, self-contained `.kn` capsule file. The capsule is a legitimate Kain source file that can be imported, checked, and built *as-is* >> no unpacking required. Other projects reference it natively.

### The Capsule File: `three.kn`

The file `X:/blades/three-kn/three.kn` (276 KB) is the amalgamated capsule containing all 19 source modules, their public interfaces, and a capsule header with metadata.

```bash
# Inspect the capsule (see what's inside without unpacking)
kain amalgamate inspect X:/blades/three-kn/three.kn
```

This shows:
- **Capsule schema v2** ~> `name: "src"`, `kind: "directory"`, `storage: "editable"`
- **19 files, 19 modules**
- **40+ preview symbols** listed inline (the full public API index)
- **SHA-256 digest** for integrity verification

### Using the Capsule in Your Project

**Option A * * * Direct `import` from the capsule** (recommended for single-file apps):

```kn
// my_3d_app.kn
import "X:/blades/three-kn/three.kn" as three_kn

fn main() -> Int with IO:
    // Use any public export from three-kn
    let v: Vec3Wrapper = three_kn.Vec3Wrapper { x: 1.0, y: 2.0, z: 3.0 }
    let cam: CameraState = three_kn.CameraState { ... }
    return 0
```

**Option B >> Import via capsule reference in build.kn:**

```kn
// my_project/build.kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let album = project("my-3d-app")
        .version("0.1.0")
        .entry("src/main.kn")
        .module_roots(["src"])

    // Reference the three-kn capsule as a dependency
    let capsule_dep = capsule_reference("three-kn")
        .path("X:/blades/three-kn/three.kn")
    
    let exe = native_executable("my-app")
        .project(album)
        .entry("src/main.kn")
        .dependency(capsule_dep)
    
    return build_graph(album)
        .tasks(exe)
```

**Option C ‒ Unpack if you need to edit the source:**

```bash
kain amalgamate unpack X:/blades/three-kn/three.kn -o my-three-kn-copy/
```

**Option D --- Amalgamate a new capsule of your own app that includes three-kn:**

```bash
kain amalgamate src/ -o my-game.kn --capsule-set my-game
```

### Capsule Benefits

| Feature | What it means |
|---------|---------------|
| **Drag-and-drop** | One file. No `node_modules`, no `pip install`, no `cargo fetch`. |
| **Self-describing** | The capsule header lists every public symbol. IDE autocompletion works. |
| **Versioned & hashed** | SHA-256 digest. No dependency hell. |
| **Editable inline** | The `storage: "editable"` mode allows `kain check` to validate the capsule directly. |
| **No unpacking** | The compiler resolves imports against the capsule's internal directory index. |
| **Cross-project** | Use the same `three.kn` in a game, a visualization tool, a CI render farm, and a shader playground. |

---

## 4. Architecture >> The 19 Modules

The engine maps every subsystem to a specific layer of the [Kain decision ladder](https://github.com/kain-lang/kain/blob/master/docs/RULEBOOK.md):

```
src/
├── main.kn          ← Layer 1/5: world init, pulse frame_clock, component EnginePanel
├── math.kn          ← Layer 0: Vec2/3/4, Mat3/4, Quat, Euler, Color (1,404 lines, 70+ methods)
├── math_types.kn    ← Layer 0: Box2/3, Sphere, Ray, Plane, Frustum, Triangle, interpolation
├── scene_graph.kn   ← Layer 1/2/5: world SceneGraphWorld + entangle + patch + law + resonate
├── buffers.kn       ← Layer 6/7: shatter GeometryBuffer + collapse/observe/decay ownership
├── renderer.kn      ← Layer 4/5/6: orchestrate render_frame DAG + converge cull/sort + axiom
├── material.kn      ← Layer 1/2/3: world MaterialState + converge shade_material (9+ lanes)
├── light.kn         ← Layer 1/2/4/6: shatter LightData + 6 types + GPU cull shader + shadow pipeline
├── texture.kn       ← Layer 1/2/6: world TextureRegistry + 6 texture types
├── vertex.kn        ← Layer GPU: 7 shader vertex variants + converge transform_vertex
├── fragment.kn      ← Layer GPU: 10+ shader fragment variants + converge shade_fragment
├── compute.kn       ← Layer GPU: audio_fft compute shader
├── animation.kn     ← Layer 7/5/3: actor AnimationMixer + pulse + converge interpolate
├── camera.kn        ← Layer 1/2/3: world CameraState + converge projection dispatch
├── control.kn       ← Layer UI: component OrbitControls + Trackball + Fly + FP + PointerLock
├── audio.kn         ← Layer 1/7: world AudioListener + entangle → Camera + actor AudioPlayer
├── helpers.kn       ← Layer UI: 11 debug visualization components
├── backend.kn       ← Layer 6: axiom + converge select_backend (Vulkan/DX12/Metal/WebGPU)
└── extra.kn         ← Layer 0/4: trait Curve + 6 impls + orchestrate pmrem_pipeline
```

### Dependency Flow (bottom-up)

```
Layer 0 (Plain Code)       math.kn, math_types.kn
        ↓
Layer 1 (State)            scene_graph, renderer, material, light, texture, camera, audio
        ↓
Layer 2 (Invariants)       laws + patches on all worlds
        ↓
Layer 3 (Dispatch)         converge: cull_instances, sort_draws, shade_material, transform_vertex,
                           shade_fragment, interpolate_value, projection_point, select_backend
        ↓
Layer 4 (Pipelines)        orchestrate: render_frame, shadow_render_pipeline, pmrem_pipeline
        ↓
Layer 5 (Temporal)         pulse: frame_clock, animation_tick / resonate: SceneGraph.epoch
        ↓
Layer 6 (Stones)           shatter: GeometryBuffer, LightData, DrawCommand, GpuTextureDescriptor,
                           KeyframeData / axiom: vulkan, dx12, metal, webgpu, compute, shadows
        ↓
Layer 7 (Systems)          actor: AnimationMixer, AudioPlayer / collapse/observe/decay on GPU mem
        ↓
Layer UI (Components)      OrbitControls, TrackballControls, FlyControls, FirstPersonControls,
                           PointerLockControls, AxesHelper, GridHelper, BoxHelper, CameraHelper, ...
        ↓
Entry                     main.kn (world EngineState, pulse frame_clock, fn main)
```

---

## 5. Public Module Reference

Every module, every public export, every semantic construct. Listed in dependency order.

---

### 5.1 `main.kn` - Entry Point & World Wiring

**Layer:** 1 (world init), 5 (pulse frame_clock), UI (surface dispatch), 7 (actor spawn)

**What it does:** The master controller. Initializes the runtime, creates a graphics session, spawns the animation mixer and audio player actors, loads the demo scene, and drives the 60 FPS render loop via `pulse frame_clock`. All other subsystems are wired from here.

**Imports:** `std::runtime`, `std::graphics`, `std::actor`, `std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `EnginePanel` | `component` | Render panel for engine state (render session, frame count, running flag) |
| `EngineState` | `world` | Global engine state: `render_session: Int`, `frame_count: Int`, `running: Bool` |
| `DemoScene` | `struct` | Fields: `cube_node_id`, `ground_node_id`, `light_node_id`, `camera_id` |
| `load_demo_scene()` | `fn -> DemoScene with IO` | Builds and returns a demo scene descriptor |
| `init_worlds()` | `fn -> Int` | Initializes all subsystem worlds (stub) |
| `spawn_animator()` | `fn -> Int` | Spawns the AnimationMixer actor, registers it in the actor registry |
| `spawn_audio_player()` | `fn -> Int` | Spawns the AudioPlayer actor, registers it |
| `render_frame(session_id)` | `fn(Int) -> Int` | Pure render function: `graphics_begin_frame` → `graphics_end_frame` → `graphics_present` |
| `frame_clock` | `pulse every 16ms jitter 2ms` | 60 FPS main loop: sends Update to mixer actor, renders frame, tracks missed frames |
| `main()` | `fn -> Int with IO` | Entry point: init → create session → spawn actors → load scene → enter pulse loop |

**Entry point flow:**
```
main()
  → runtime_init()
  → init_worlds()            // bump all world epochs
  → graphics_session_create("three-kn", 1024, 768)
  → EngineState.render_session = session
  → spawn_animator()         // actor AnimationMixer
  → spawn_audio_player()     // actor AudioPlayer
  → load_demo_scene()        // DemoScene descriptor
  → [pulse frame_clock runs every 16ms]
  → graphics_session_destroy()
  → runtime_shutdown()
```

---

### 5.2 `math.kn` ___ Vector, Matrix, Quaternion, Euler, Color

**Layer:** 0 ... Plain Functions (`struct`, `impl`, `fn Pure`)

**What it does:** Wraps `std::math` native types (Vec2, Vec3, Vec4, Mat3, Mat4, Quat) with Three.js-compatible method-call syntax. All operations are Pure – no side effects, no mutation of inputs, fluent chaining (`v.clone().add(other).normalize()`).

**Imports:** `use std::math`

**Public exports: 8 wrapper structs + 1 enum, ~130 methods total**

#### `Vec2Wrapper` ~ 2D Vector
`pub struct Vec2Wrapper { x: Float, y: Float }` + `pub impl Vec2Wrapper` (24 methods)

| Method | Signature | Description |
|--------|-----------|-------------|
| `create` | `fn(x, y) -> Vec2Wrapper` | Constructor |
| `clone` | `fn() -> Vec2Wrapper` | Deep copy |
| `set` | `fn(x, y) -> Vec2Wrapper` | Set components |
| `copy` | `fn(other) -> Vec2Wrapper` | Copy from another Vec2 |
| `add` | `fn(other) -> Vec2Wrapper` | Element-wise addition |
| `sub` | `fn(other) -> Vec2Wrapper` | Element-wise subtraction |
| `multiply_scalar` | `fn(s) -> Vec2Wrapper` | Scalar multiply |
| `divide_scalar` | `fn(s) -> Vec2Wrapper` | Scalar divide |
| `dot` | `fn(other) -> Float` | Dot product |
| `cross` | `fn(other) -> Float` | 2D cross product (scalar) |
| `length` | `fn() -> Float` | Euclidean magnitude |
| `normalize` | `fn() -> Vec2Wrapper` | Unit vector |
| `distance_to` | `fn(other) -> Float` | Euclidean distance |
| `lerp` | `fn(other, t) -> Vec2Wrapper` | Linear interpolation |
| `equals` | `fn(other) -> Bool` | Approximate equality |
| `floor` | `fn() -> Vec2Wrapper` | Floor each component |
| `negate` | `fn() -> Vec2Wrapper` | Negate each component |
| `add_scalar` | `fn(s) -> Vec2Wrapper` | Add scalar to each component |
| `length_squared` | `fn() -> Float` | Squared magnitude (faster) |
| `to_vec3` | `fn(z) -> Vec3Wrapper` | Extend to 3D |
| `to_array` | `fn() -> [Float; 2]` | Convert to array |
| `from_array` | `fn([Float; 2]) -> Vec2Wrapper` | From array |

#### `Vec3Wrapper` ⁓ 3D Vector
`pub struct Vec3Wrapper { x: Float, y: Float, z: Float }` + `pub impl Vec3Wrapper` (38 methods)

All Vec2Wrapper methods plus:
`multiply` (Hadamard product), `cross` (3D), `min`, `max`, `clamp`, `project` (onto vector), `reject` (from vector), `reflect` (mirror across normal), `refract` (Snell's law), `apply_matrix4` (transform point), `apply_matrix3` (transform direction), `apply_quat` (rotate), `angle_to`, `to_vec2`, `to_vec4`, `zero`, `one`, `up`, `right`, `forward`

#### `Vec4Wrapper` * * * 4D Vector
`pub struct Vec4Wrapper { x: Float, y: Float, z: Float, w: Float }` + methods: create, clone, set, add, sub, multiply_scalar, divide_scalar, dot, length, normalize, lerp, equals, negate, to_vec3, to_array, from_array

#### `QuatWrapper` <--> Quaternion
`pub struct QuatWrapper { x: Float, y: Float, z: Float, w: Float }` + 18 methods

| Method | Description |
|--------|-------------|
| `create` | Constructor (x, y, z, w) |
| `identity` | Unit quaternion |
| `conjugate` | Flip sign of imaginary parts |
| `inverse` | Conjugate divided by squared length |
| `dot` | Inner product |
| `length` / `normalize` | Magnitude / unit quaternion |
| `mul` | Quaternion multiplication (compose rotations) |
| `from_axis_angle` | Rotation from axis + angle |
| `from_unit_vectors` | Rotation between two direction vectors |
| `rotate_vec3` | Apply rotation to a 3D vector |
| `nlerp` | Normalized linear interpolation (fast) |
| `slerp` | Spherical linear interpolation (constant angular velocity) |
| `angle_to` | Angular distance between quaternions |
| `to_mat3` / `to_mat4` | Convert to rotation matrix |
| `equals` / `to_array` / `from_array` | Utility |

#### `Mat3Wrapper` ⁓ 3×3 Matrix
`pub struct Mat3Wrapper { elements: [Float; 9] }` + 10 methods: create, clone, identity, copy, mul, multiply_vec3, transpose, determinant, inverse, from_mat4, get_normal_matrix, equals, to_array

#### `Mat4Wrapper` ... 4×4 Matrix
`pub struct Mat4Wrapper { elements: [Float; 16] }` + 22 methods

| Method | Description |
|--------|-------------|
| `create` / `clone` / `copy` | Construction |
| `identity` | Identity matrix |
| `mul` | Matrix multiplication |
| `transpose` | Transpose |
| `determinant` | Determinant |
| `inverse` | Matrix inverse |
| `transform_point` | Multiply Vec4 with w=1 |
| `transform_vector` | Multiply Vec4 with w=0 |
| `translation` / `scale` | Build translation / scale matrix |
| `rotation_x` / `rotation_y` / `rotation_z` | Build axis rotation |
| `from_quat` | Convert quaternion to rotation |
| `compose` | Build from translation + quaternion + scale (TRS) |
| `decompose` | Extract TRS components |
| `extract_position` / `set_position` | Position get/set |
| `make_perspective` | Perspective projection matrix |
| `make_orthographic` | Orthographic projection matrix |
| `look_at` | View matrix from eye/target/up |
| `from_trs` | Convenience TRS builder |
| `equals` / `to_array` / `from_array` | Utility |

#### `EulerWrapper` - Euler Angles
`pub enum EulerOrder { XYZ, YXZ, ZXY, ZYX, YZX, XZY }`  
`pub struct EulerWrapper { x, y, z: Float, order: EulerOrder }` + 6 methods

Methods: create, set, clone, to_quat (convert to quaternion respecting rotation order), set_from_quat, to_array, equals

#### `ColorWrapper` <--> RGB Color
`pub struct ColorWrapper { r: Float, g: Float, b: Float }` + 18 methods

| Method | Description |
|--------|-------------|
| `create` / `create_rgb` | Constructor (0-1 or 0-255 normalized) |
| `from_hex` / `to_hex` / `set_hex` | Hex string conversion (#ffffff) |
| `from_hsl` / `to_hsl` | HSL ↔ RGB |
| `from_hsv` / `to_hsv` | HSV ↔ RGB |
| `add` / `multiply` / `multiply_scalar` | Color arithmetic |
| `lerp` | Linear interpolation |
| `to_linear` / `to_srgb` | Color space conversion |
| `to_vec3` / `to_vec4` / `to_array` / `from_array` | Conversion |

---

### 5.3 `math_types.kn` - Box, Sphere, Ray, Plane, Frustum, Triangle

**Layer:** 0/3 ___ Compound Geometry Types

**What it does:** Provides all compound geometry types needed for a 3D engine beyond the basic vectors and matrices: bounding volumes, ray intersection, plane math, frustum culling, coordinate conversions, and interpolation utilities.

**Imports:** `use std::math`

**Public exports: 10 structs + 9 utility functions**

| Export | Kind | Description |
|--------|------|-------------|
| `Box2` | `struct` + `impl` | 2D axis-aligned bounding box. Methods: create, set, clone, make_empty, is_empty, expand_by_point, expand_by_vec2, contains_point, center, size, union_box, intersect, equals |
| `Box3` | `struct` + `impl` | 3D AABB. Methods: create, set, set_from_center_and_size, clone, make_empty, is_empty, expand_by_point, expand_by_box, contains_point, center, size, union_box, intersect, apply_matrix4, equals |
| `Sphere` | `struct` + `impl` | Bounding sphere (center + radius). Methods: create, create_with_center, clone, set, is_empty, make_empty, contains_point, distance_to_point, intersect_sphere, apply_matrix4, equals |
| `Ray` | `struct` + `impl` | Ray (origin + direction). Methods: create, create_from, clone, set, at (point at distance), distance_to_point, distance_sq_to_point, intersect_sphere, intersect_box, intersect_triangle, apply_matrix4, equals |
| `PlaneWrapper` | `struct` + `impl` | Infinite plane (normal + constant). Methods: create, create_from (normal + point), clone, set, set_from_point_normal, normalize, distance_to_point, project_point, coplanar_point, equals |
| `FrustumWrapper` | `struct` + `impl` | View frustum (6 planes). Methods: create, set_from_projection_matrix (extract 6 planes from VP matrix), contains_point, intersects_box, intersects_sphere |
| `Triangle` | `struct` + `impl` | Triangle (3 vertices). Methods: create, clone, area, midpoint, normal, barycoord_from_point (barycentric coordinates), contains_point, equals |
| `Line3` | `struct` + `impl` | Line segment (start + end). Methods: create, create_from, clone, set, center, delta, distance_sq, distance, at (point on segment), closest_point_to_point, apply_matrix4, equals |
| `Cylindrical` | `struct` + `impl` | Cylindrical coordinates (radius, theta, y). Methods: create, set, set_from_vec3, to_vec3 |
| `Spherical` | `struct` + `impl` | Spherical coordinates (radius, phi, theta). Methods: create, set, clone, set_from_vec3, to_vec3, make_safe (clamp phi to avoid gimbal lock) |
| `lerp_simple` | `fn(Float, Float, Float) -> Float` | Linear interpolation |
| `smoothstep_simple` | `fn(Float, Float, Float) -> Float` | Hermite smoothstep |
| `smootherstep_simple` | `fn(Float, Float, Float) -> Float` | Smootherstep (5th-order) |
| `inverse_lerp_simple` | `fn(Float, Float, Float) -> Float` | Inverse lerp (normalized position) |
| `deg_to_rad` | `fn(Float) -> Float` | Degrees to radians |
| `rad_to_deg` | `fn(Float) -> Float` | Radians to degrees |
| `clamp_simple` | `fn(Float, Float, Float) -> Float` | Clamp value to [min, max] |
| `euclidean_modulo` | `fn(Float, Float) -> Float` | Euclidean modulo (always positive) |
| `map_linear` | `fn(Float, Float, Float, Float, Float) -> Float` | Linear remap from one range to another |

**Usage example:**
```kn
let box: Box3 = Box3::create()
    .expand_by_point(Vec3Wrapper { x: -1.0, y: -1.0, z: -1.0 })
    .expand_by_point(Vec3Wrapper { x: 1.0, y: 1.0, z: 1.0 })

let ray: Ray = Ray::create(origin, direction)
let hit: Bool = ray.intersect_box(box)
let point: Vec3Wrapper = ray.at(hit_distance)
```

---

### 5.4 `scene_graph.kn` --> Transform Hierarchy

**Layer:** 1 (world, entangle), 2 (law, patch), 5 (resonate)

**What it does:** Manages a tree of `SceneNode` objects with compiler-owned state via the authority/mirror world pattern. Laws guarantee epoch integrity and fog range validity. Patches provide journaled mutations. The `resonate` trigger propagates transforms on epoch change.

**Imports:** `use std::math`, `use math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `SceneViewport` | `component` | 3D viewport stub panel |
| `SceneNode` | `struct` | Transform node: `id`, `name`, `parent_id`, `local_position/rotation/scale`, `visibility`, `layer_mask`, `cast_shadow`, `receive_shadow` |
| `SceneGraphWorld` | `world` | **Authority world**. State: `max_nodes: Int`, `epoch: Int`, `fog_type: Int`, `fog_near/far: Float`, `fog_color: Vec3Wrapper` |
| `SceneGraphMirror` | `world` | **Mirror world** (read-only). Mirrors all SceneGraphWorld state. |
| `epoch_valid(sg)` | `law` | `sg.epoch >= 0` |
| `fog_range_valid(sg)` | `law` | `sg.fog_near >= 0 and sg.fog_far > sg.fog_near` |
| `bump_epoch(state)` | `patch -> Int` | Increment epoch |
| `set_fog(state, fog_type, near, far)` | `patch -> SceneGraphWorld` | Set fog parameters |
| `SceneGraphWorld.epoch` | `resonate dampen 16ms` | Reactive tripwire on epoch change (transform propagation) |
| `look_at(eye, target, up)` | `fn -> Mat4Wrapper` | Standalone view matrix builder |

**Entanglements:**
```
SceneGraphWorld.epoch <-> SceneGraphMirror.epoch (single_writer)
SceneGraphWorld.fog_type <-> SceneGraphMirror.fog_type (single_writer)
SceneGraphWorld.fog_near <-> SceneGraphMirror.fog_near (single_writer)
SceneGraphWorld.fog_far <-> SceneGraphMirror.fog_far (single_writer)
```

---

### 5.5 `buffers.kn` :: GPU Geometry Buffers

**Layer:** 6 (shatter struct), 7 (collapse/observe/decay)

**What it does:** Defines the `GeometryBuffer` <--> a Structure-of-Arrays layout for vertex data that is GPU-optimal. Typed buffer attributes (`BufferAttribute`, `InterleavedBuffer`, `InstancedBufferAttribute`) separate CPU data from GPU memory. The `upload_geometry_buffer` and `release_geometry_buffer` functions demonstrate the `collapse`/`observe`/`decay` ownership lifecycle for raw GPU memory.

**Imports:** `use std::machine`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `DrawGroup` | `struct` | Sub-range for multi-material geometry: `start`, `count`, `material_index` |
| `GeometryBuffer` | `shatter struct` | SoA vertex data. Fields: `vertex_count`, `index_count`, `positions: [Float]`, `normals: [Float]`, `uvs: [Float]`, `tangents: [Float]`, `colors: [Float]`, `indices: [Int]`, `draw_groups: [DrawGroup]`, `gpu_handle: Int` |
| `GeometryBuffer` | `pub impl` | Methods: `get_vertex_count`, `get_index_count`, `has_normals`, `has_uvs`, `has_tangents`, `has_colors`, `is_indexed`, `gpu_byte_length`, `clear`, `upload_gpu` |
| `BufferAttribute` | `struct` + `impl` | Typed buffer: `array: [Float]`, `item_size: Int`, `count: Int`, `normalized: Bool`, `needs_update: Bool`. Methods: create, set_needs_update, clone, byte_length |
| `InterleavedBuffer` | `struct` + `impl` | Interleaved buffer: `array: [Float]`, `stride: Int`. Methods: create, set_needs_update, clone |
| `InstancedBufferAttribute` | `struct` + `impl` | Instanced attribute: `mesh_per_attribute: Int`. Methods: create, clone |
| `create_box_geometry(w, h, d)` | `fn -> GeometryBuffer` | Procedural cube: 24 vertices with normals + UVs |
| `upload_geometry_buffer(buffer)` | `fn -> Int with Unsafe` | GPU upload: `alloc_zeroed` → `collapse` (exclusive write) → `observe` (read) → `decay` (release) |
| `release_geometry_buffer(buffer)` | `fn -> Bool with Unsafe` | GPU free via collapse/observe/decay |

**Ownership lifecycle (explicit, no borrow checker):**
```kn
let gpu_ptr: ptr<Int> = alloc_zeroed(cells_needed, "Int")
collapse gpu_ptr:           // ENTER ~~ exclusive write access
    mem_store(gpu_ptr, byte_count, "Int")
    0
let result: Int = observe gpu_ptr:  // READ <--> shared read access
    mem_load(gpu_ptr, "Int")
decay gpu_ptr                // RELEASE ___ drop ownership
```

---

### 5.6 `renderer.kn` --- Render Pipeline DAG

**Layer:** 4 (orchestrate), 5 (pulse, resonate), 6 (axiom), 3 (converge), 1 (world), 2 (law, patch)

**What it does:** The GPU pipeline orchestrator. Defines the full `orchestrate render_frame` pipeline -- a compile-time DAG with 9 stages. Converge lanes select the optimal culling and sorting strategy at compile time. Axioms declare compulsory GPU capabilities with fallback chains. Laws enforce G-buffer size consistency and frame state validity.

**Imports:** `use std::graphics`, `use std::gpu`, `use std::runtime`, `use std::intent`, `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `DrawCommand` | `shatter struct` | GPU sortable draw command (SoA: instance count, index count, start index, base vertex, material, flags) |
| `GPURenderState` | `world` | Renderer state: session, resolution, G-buffer texture IDs (color, normal, metallic/roughness, AO, emissive, depth), bloom/SSAO/DOF/motion-blur toggles, frame_count |
| `gbuffer_size_matches` | `law` | G-buffer dimensions match framebuffer resolution |
| `session_valid` | `law` | Session ID > 0 |
| `render_targets_valid` | `law` | All G-buffer texture IDs > 0 |
| `no_double_begin_frame` | `law` | in_frame == false before begin_frame |
| `frame_stats_valid` | `law` | Frame stats non-negative |
| `visibility_count_matches` | `law` | Visibility count ≤ instance count |
| `begin_frame` | `patch -> Int` | Begin frame, clear targets |
| `end_frame` | `patch -> Int` | End frame |
| `present_frame` | `patch -> Int` | Present to swapchain |
| `GPURenderState.frame_count` | `resonate dampen 16ms` | Frame metrics tripwire |
| `has_compute_shaders` | `axiom` | `when capability("gpu.compute")` => cascade to has_float_textures |
| `has_float_textures` | `axiom` | `when capability("gpu.float_textures")` === cascade to has_indirect_draw |
| `has_indirect_draw` | `axiom` | `when capability("gpu.indirect_draw")` ‒ cascade to has_anisotropic |
| `has_anisotropic_filtering` | `axiom` | `when capability("gpu.anisotropy")` :: cascade to has_tesselation |
| `has_tesselation` | `axiom` | Terminal capability |
| `cull_instances` | `converge` | 3 lanes: `cpu_scalar` (reference), `cpu_simd` (fast when SIMD), `gpu_compute` (fast when gpu.compute) |
| `sort_draws` | `converge` | 3 lanes: `cpu` (reference), `gpu_bitonic` (fast when gpu.sort), `gpu_radix` (fast when gpu.sort + atomic) |
| `render_frame` | `orchestrate` | **9-stage render DAG**: begin → frustum_cull → sort_opaque → shadow_maps → opaque_pass → sort_transparent → transparent_pass → postprocess → present |
| `graphics_session_init` | `fn -> Int` | Create graphics session |
| `render_opaque_pass` | `fn -> Int` | GPU opaque pass (stub) |
| `render_transparent_pass` | `fn -> Int` | GPU transparent pass (stub) |
| `apply_postprocess` | `fn -> Int` | Bloom, SSAO, DOF, tonemap composite |
| `compute_shadow_maps` | `fn -> Int` | Shadow map dispatch |
| `create_render_targets` | `fn -> Int` | Create HDR + depth buffers |
| `update_frame_stats` | `fn -> Int` | Update GPU timing metrics |

**Render frame DAG stages:**
```
Stage 1: begin              → patch begin_frame (clear targets)
Stage 2: frustum_cull       → converge cull_instances (GPU compute → SIMD → CPU)
Stage 3: sort_opaque        → converge sort_draws (front-to-back sort)
Stage 4: shadow_maps        → compute_shadow_maps (GPU shadow pass)
Stage 5: opaque_pass        → render_opaque_pass (PBR shading)
Stage 6: sort_transparent   → converge sort_draws (back-to-front sort)
Stage 7: transparent_pass   → render_transparent_pass
Stage 8: postprocess        → apply_postprocess (bloom, SSAO, DOF, tonemap)
Stage 9: present            → patch present_frame (swapchain present)
```

---

### 5.7 `material.kn` ->> 9+ Shading Models & BRDF

**Layer:** 1 (world), 2 (law), 3 (converge)

**What it does:** The material system. Defines 9 material kinds from Basic (unlit) through full Cook-Torrance PBR (Standard) to Physical (multi-layer: clearcoat, sheen, iridescence, transmission). All BRDF fundamentals (GGX NDF, Smith visibility, Schlick Fresnel) are provided as Pure functions for both CPU verification and GPU emission.

**Imports:** `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `MaterialKind` | `enum` | 9 variants: `Basic`, `Lambert`, `Phong`, `Standard`, `Physical`, `Toon`, `Matcap`, `NormalDebug`, `Depth` |
| `MaterialDef` | `struct` | 37-field material property union covering all material kinds |
| `MaterialState` | `world` | Authority world: material array, dirty flags, epoch, default material |
| `MaterialStateMirror` | `world` | Read-only mirror |
| `material_color_valid` | `law` | RGB in [0,1] |
| `material_opacity_valid` | `law` | Opacity in [0,1] |
| `material_params_valid` | `law` | Roughness in [0,1], metalness in [0,1], specular ≥ 0 |
| `mark_materials_dirty` | `patch -> Int` | Bump epoch, set dirty flags |
| `sample_albedo` | `fn -> Vec3Wrapper` | Sample albedo texture |
| `sample_normal_default` | `fn -> Vec3Wrapper` | Sample normal map |
| `sample_roughness_value` | `fn -> Float` | Sample roughness |
| `sample_metalness_value` | `fn -> Float` | Sample metalness |
| `sample_ao_value` | `fn -> Float` | Sample ambient occlusion |
| `sample_emissive_value` | `fn -> Vec3Wrapper` | Sample emissive |
| `compute_view_dir_default` | `fn -> Vec3Wrapper` | Compute view direction |
| `compute_world_normal_default` | `fn -> Vec3Wrapper` | Compute world-space normal |
| `encode_view_space_normal` | `fn -> Vec3Wrapper` | Encode normal for G-buffer |
| `D_GGX` | `fn(Float, Float) -> Float with Pure` | Trowbridge-Reitz / GGX normal distribution function |
| `V_Smith` | `fn(Float, Float, Float) -> Float with Pure` | Smith joint masking-shadowing function |
| `F_Schlick` | `fn(Float, Float) -> Float with Pure` | Schlick Fresnel approximation |
| `compute_f0` | `fn(Float, Float) -> Float with Pure` | Dielectric/metal F0 blending |
| `evaluate_lambert_diffuse` | `fn -> Float with Pure` | Lambertian diffuse BRDF |
| `evaluate_blinn_phong_specular` | `fn -> Float with Pure` | Blinn-Phong specular |
| `evaluate_cook_torrance` | `fn -> Float with Pure` | Full Cook-Torrance PBR: GGX NDF × Smith V × Schlick F |
| `shade_basic` | `fn -> Vec4Wrapper with GPU` | Unlit: return albedo × opacity |
| `shade_lambert` | `fn -> Vec4Wrapper with GPU` | Diffuse Lambert |
| `shade_phong` | `fn -> Vec4Wrapper with GPU` | Blinn-Phong specular |
| `shade_standard` | `fn -> Vec4Wrapper with GPU` | Full Cook-Torrance PBR (GGX + Smith + Schlick) |
| `shade_physical` | `fn -> Vec4Wrapper with GPU` | Multi-layer: clearcoat + sheen + iridescence + transmission |
| `shade_toon` | `fn -> Vec4Wrapper with GPU` | Cel shading (4-level quantization) |
| `shade_matcap` | `fn -> Vec4Wrapper with GPU` | Matcap lookup (view-space normal) |
| `shade_normal_default` | `fn -> Vec4Wrapper with GPU` | Normal as RGB |
| `shade_depth_default` | `fn -> Vec4Wrapper with GPU` | Linear depth |
| `shade_result_to_hash` | `fn(Vec4Wrapper) -> Int with Pure` | Hash for converge verification |
| `shade_color_transform` | `converge` | 2 lanes: `scalar` (reference) vs `inline_lane` (fast when target("llvm")) |
| `dispatch_material_shading` | `fn(MaterialKind, ...) -> Int` | Routing table: MaterialKind → shade function |

---

### 5.8 `light.kn` => 6 Light Types & Shadow Pipeline

**Layer:** 1 (world), 2 (law), 4 (orchestrate), 6 (shatter struct, axiom)

**What it does:** Covers 6 light types (compressing Three.js's 12+ light classes). CPU-side `LightDef` structs are packed into GPU SoA `LightData` for coalesced access. A Forward+ tile-based light culling compute shader dispatches per-tile frustum tests. An `orchestrate shadow_render_pipeline` handles shadow maps for all shadow-casting light types.

**Imports:** `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `LightKind` | `enum` | 6 variants: `Ambient`, `Directional`, `Point`, `Spot`, `Hemisphere`, `RectArea` |
| `LightDef` | `struct` | CPU light: kind, color, intensity, decay, position, direction, range, cone angles, shadow bias, shadow map size, etc. (19 fields) |
| `LightData` | `shatter struct` | GPU SoA light data: kind, color_r/g/b, intensity, position_x/y/z, direction_x/y/z, range, inner_cone, outer_cone, decay, shadow_enabled, shadow_bias, shadow_map_id, etc. (28 fields) |
| `LightState` | `world` | Authority world: light array, count, shadow map count, epoch |
| `LightStateMirror` | `world` | Mirror world |
| `light_count_valid` | `law` | ≤ max lights |
| `light_intensity_valid` | `law` | Intensity ≥ 0 |
| `light_decay_valid` | `law` | Decay ≥ 0 |
| `shadow_map_count_valid` | `law` | ≤ max shadow maps |
| `create_ambient_light` | `fn -> LightDef` | Ambient light factory |
| `create_directional_light` | `fn -> LightDef` | Directional (sun) light factory |
| `create_point_light` | `fn -> LightDef` | Point (omni) light factory ~ decay defaults to 2.0 |
| `create_spot_light` | `fn -> LightDef` | Spot light factory ->> cone angle, penumbra |
| `create_hemisphere_light` | `fn -> LightDef` | Hemisphere light * * * sky + ground color |
| `create_rect_area_light` | `fn -> LightDef` | Rectangular area light |
| `pack_light_data` | `fn([LightDef]) -> LightData` | Pack CPU lights to GPU SoA |
| `light_cull_tiled` | `shader compute` | Forward+ tile-based light culling: per-tile frustum test |
| `shadow_render_pipeline` | `orchestrate` | 5-stage shadow pipeline: directional → point → spot → CSM → atlas_pack |
| `render_directional_shadow_maps` | `fn -> Int` | Directional shadow pass |
| `render_point_shadow_maps` | `fn -> Int` | Point shadow (cubemap) pass |
| `render_spot_shadow_maps` | `fn -> Int` | Spot shadow pass |
| `render_csm_shadow_maps` | `fn -> Int` | Cascaded shadow maps |
| `pack_shadow_atlas` | `fn -> Int` | Pack into atlas |
| `axiom_shadows_supported` | `axiom` | `when capability("gpu.shadows")` -- guarantee shadow map support |
| `degrade_shadows_fallback` | `fn -> Int` | Fallback: disable shadows |

---

### 5.9 `texture.kn` <--> GPU Texture Registry

**Layer:** 1 (world), 2 (law), 6 (shatter struct)

**What it does:** GPU texture descriptor management with a fixed-capacity registry (256 slots). Supports 6 texture types: data, cube, depth, compressed, render target, and cube render target. Laws enforce valid dimensions (≤ 16384), mip levels, and MSAA sample counts.

**Imports:** none

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `TextureKind` | `enum` | 6 variants: `Data`, `Cube`, `Depth`, `Compressed`, `RenderTarget`, `CubeRenderTarget` |
| `GpuTextureDescriptor` | `shatter struct` | GPU-resident SoA: handle, width, height, depth, mip_levels, format, sample_count, kind, usage_flags |
| `TextureCreateInfo` | `struct` | CPU creation params: dimensions, format, mips, kind, usage |
| `TexturePanel` | `component` | UI panel for texture registry |
| `TextureRegistry` | `world` | Authority world: descriptor array (256 max), count, default_texture_id, epoch |
| `TextureRegistryMirror` | `world` | Mirror world |
| `texture_dimensions_valid` | `law` | Width & height ≤ 16384 |
| `texture_mip_levels_valid` | `law` | Mip levels ≥ 1 |
| `texture_sample_count_valid` | `law` | MSAA samples = 0, 1, 2, 4, 8 |
| `create_data_texture` | `fn -> Int` | Create data texture (CPU buffer upload) |
| `create_cube_texture` | `fn -> Int` | Create cube map (6 faces) |
| `create_depth_texture` | `fn -> Int` | Create depth/stencil attachment |
| `create_compressed_texture` | `fn -> Int` | Create block-compressed texture |
| `create_render_target` | `fn -> Int` | Create color+depth render target |
| `create_cube_render_target` | `fn -> Int` | Create cube render target |
| `destroy_texture` | `fn(Int) -> Int` | Zero descriptor slot |
| `get_texture_descriptor` | `fn(Int) -> GpuTextureDescriptor` | Read from mirror |
| `sample_texture_at` | `fn(Int, Float, Float) -> Vec4Wrapper` | Sample stub (returns white) |

---

### 5.10 `vertex.kn` ___ 7 Vertex Shader Variants

**Layer:** GPU (shader items)

**What it does:** 7 vertex shader variants covering the full rendering gamut. Each shader accepts position/normal/uv/tangent inputs with uniform bindings at @0-@13. A `converge transform_vertex` dispatches across all lanes at compile time with capability gating.

**Imports:** `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `standard_vertex` | `shader vertex` | Standard MVP transform with world matrix, normal matrix, UV passthrough |
| `instanced_vertex` | `shader vertex` | Per-instance world matrix from instance data buffer |
| `skin_vertex` | `shader vertex` | 4-bone skeletal animation: blend bone matrices by weights |
| `batch_vertex` | `shader vertex` | Indirect draw: instance ID from draw command buffer |
| `point_vertex` | `shader vertex` | Point sprite: size attenuation by distance |
| `line_vertex` | `shader vertex` | Line strip/list topology |
| `sprite_vertex` | `shader vertex` | Billboard: always face camera |
| `transform_vertex` | `converge` | 7-lane dispatch: `reference` (spec), `instanced` (fast when draw.instanced), `skinned` (fast when draw.skinned), `batched` (fast when draw.indirect), `point_sprite` (fast when draw.point), `line_top` (fast when draw.line), `sprite_billboard` (fast when draw.sprite) |

**Uniform binding (@0–@13):**
| Slot | Name | Type | Description |
|------|------|------|-------------|
| @0 | MVP | mat4 | Model-view-projection |
| @1 | model_matrix | mat4 | Model-to-world |
| @2 | view_matrix | mat4 | World-to-view |
| @3 | projection_matrix | mat4 | View-to-clip |
| @4 | camera_pos | vec3 | Camera world position |
| @5 | time | float | Frame time (seconds) |
| @6 | resolution | vec2 | Viewport resolution |
| @10 | world_matrices | mat4[] | Per-instance world matrices |
| @11 | instance_data | InstanceData[] | Per-instance metadata |
| @12 | bone_matrices | mat4[] | Skeleton bone matrices |
| @13 | bind_matrices | mat4[] | Bone inverse bind matrices |

---

### 5.11 `fragment.kn` – 10+ Fragment Shader Variants

**Layer:** GPU (shader items)

**What it does:** 10 fragment shader variants from unlit to full multi-layer PBR. Three Cook-Torrance BRDF helper functions are declared `with GPU, Pure` for both CPU-side verification and GPU emission. The standard shader inlines full PBR math. A `converge shade_fragment` dispatches across all 10 lanes.

**Imports:** `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `three_D_GGX` | `fn with GPU, Pure` | GGX/Trowbridge-Reitz NDF (GPU + CPU) |
| `three_V_Smith` | `fn with GPU, Pure` | Smith visibility function |
| `three_F_Schlick` | `fn with GPU, Pure` | Schlick Fresnel approximation |
| `shade_basic_frag` | `shader fragment` | Unlit: albedo × opacity |
| `shade_lambert_frag` | `shader fragment` | Diffuse Lambert (N·L) |
| `shade_phong_frag` | `shader fragment` | Blinn-Phong specular |
| `shade_standard_frag` | `shader fragment` | Full Cook-Torrance PBR with inlined GGX/Smith/Schlick math |
| `shade_physical_frag` | `shader fragment` | Multi-layer: clearcoat, sheen, iridescence, transmission |
| `shade_toon_frag` | `shader fragment` | Cel shading: 4-level diffuse quantization + specular |
| `shade_matcap_frag` | `shader fragment` | Matcap: view-space normal → texture lookup |
| `shade_normal_frag` | `shader fragment` | Normal debug: RGB = (world_normal + 1) × 0.5 |
| `shade_depth_frag` | `shader fragment` | Depth debug: linearized perspective depth |
| `shade_shadow_frag` | `shader fragment` | Shadow-only: black output |
| `shade_fragment` | `converge` | 10-lane dispatch: `reference`, `lambert`, `phong`, `standard`, `physical`, `toon`, `matcap`, `normal_view`, `depth_view`, `shadow_only` |

---

### 5.12 `compute.kn` >> GPU Compute Kernels

**Layer:** GPU (shader compute)

**What it does:** GPU compute shader library. Currently contains `audio_fft` => a direct DFT compute shader for audio spectrum analysis.

**Imports:** none

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `audio_fft` | `shader compute` | Direct DFT: one thread per frequency bin, Hann window, workgroup 256×1×1. Each thread independently computes one bin by iterating over all N input samples (no shared memory, no barriers). |

**Audio FFT design:**
```
Input:  audio_buffer (uniform @0, Float[])
Output: fft_output  (buffer @1, Float[])
Params: sample_count, bin_count (push constants)

For thread i in [0, bin_count):
  real, imag = 0
  For j in [0, sample_count):
    window = 0.5 * (1 - cos(2π * j / (sample_count - 1)))  // Hann
    angle = -2π * i * j / sample_count
    real += audio_buffer[j] * window * cos(angle)
    imag += audio_buffer[j] * window * sin(angle)
  fft_output[i] = sqrt(real² + imag²) / sample_count  // magnitude
```

---

### 5.13 `animation.kn` |-> Actor-based Animation Mixer

**Layer:** 7 (actor AnimationMixer), 5 (pulse animation_tick), 3 (converge interpolate_value), 2 (law, patch)

**What it does:** Actor-based animation system. The `AnimationMixer` actor manages a database of `AnimationClip` instances (each containing keyframe tracks). Message handlers support PlayClip, Crossfade (blend between clips), StopAll, GetStats, and Update (advance time with loop detection). A `converge interpolate_value` dispatches between linear, step, and cubic interpolation. A `pulse animation_tick` at 16ms drives the mixer update cycle.

**Imports:** `use std::actor`, `use std::runtime`, `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `AnimationActionState` | `world` | Action state: clip, target_node_id, weight, speed, playing, paused, fade params, epoch. Surface: `AnimationPanel` component. |
| `animation_time_valid` | `law` | time ≥ 0 and time ≤ clip.duration |
| `animation_weight_valid` | `law` | weight ∈ [0, 1] |
| `play_action` | `patch -> Int` | Start playing: set weight to 1.0, bump epoch |
| `stop_action` | `patch -> Int` | Stop: clear playing, weight to 0 |
| `crossfade_action` | `patch -> Int` | Crossfade to another action |
| `interpolate_value` | `converge` | 3 lanes: `reference` (lerp-based), `step_lane` (snap to nearest), `cubic_lane` (Catmull-Rom cubic) |
| `AnimationMixer` | `actor` | **Message contract**: `PlayClip(reply_to, clip_name)`, `Crossfade(reply_to, target_name, duration)`, `StopAll(reply_to)`, `GetStats(reply_to)`, `Update(reply_to, dt_ms)`, `Reply(value)` |
| `animation_tick` | `pulse every 16ms jitter 2ms` | Drives mixer dispatch |
| `spawn_animator()` | `fn -> Int` | Spawn and register the mixer actor |

**Actor message flow:**
```
main() → actor_spawn("animation-mixer", "{\"time\":0.0}")
pulse frame_clock → actor_send(mixer_id, "Update", str(dt))
AnimationMixer.Update:
  for each action:
    advance time += dt * speed
    if time > clip.duration and loop_mode:
      time -= clip.duration  // loop
    interpolate tracks → apply to target node
  send reply_to.Reply(value = active_count)
```

**Private types (used internally):** `InterpolationMode` (Linear, Step, CubicSpline, QuaternionSlerp, Constant), `TrackPath` (Position, Rotation, Scale, Quaternion, MorphWeight, MaterialProperty, BoneProperty), `shatter struct KeyframeData` (times, values, interpolation_modes, value_stride, track_path, count, duration), `struct KeyframeTrack`, `struct AnimationClip`

---

### 5.14 `camera.kn` >> 4 Projection Types

**Layer:** 1 (world), 2 (law), 3 (converge)

**What it does:** Camera system compressing six Three.js camera classes into 4 projection variants. The authority/mirror world pattern holds projection parameters and derived matrices. Laws validate near/far ordering, aspect ratio, and FOV range. Patches recompute derived matrices on parameter change. A `converge projection_point_hash` dispatches between perspective and orthographic projection.

**Imports:** `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `ProjectionKind` | `enum` | 4 variants: `Perspective`, `Orthographic`, `Cube` (6-face 90° FOV), `Stereo` (dual-eye) |
| `CameraState` | `world` | Authority world: projection_type, fov, aspect, near, far, left/right/top/bottom/zoom, position, target, up, projection_matrix, view_matrix, view_projection_matrix, epoch. Surface: `CameraPanel` component. |
| `CameraStateMirror` | `world` | Mirror (read-only) |
| `camera_near_positive` | `law` | near > 0.0 |
| `camera_far_greater_than_near` | `law` | far > near |
| `camera_aspect_positive` | `law` | aspect > 0.0 |
| `camera_fov_valid` | `law` | fov ∈ [1.0, 179.0] |
| `camera_state_valid` | `law` | Composite: all above |
| `select_projection(cam)` | `fn -> Mat4` | Dispatch to proper projection matrix builder |
| `projection_point_hash` | `converge` | 2 lanes: `perspective` (spec), `orthographic` (fast when camera.ortho) |
| `set_perspective` | `patch -> Int` | Set perspective params, rebuild matrices |
| `set_orthographic` | `patch -> Int` | Set orthographic params, rebuild matrices |
| `update_view` | `patch -> Int` | Rebuild view matrix from position/target/up |
| `set_aspect` | `patch -> Int` | Update aspect, rebuild projection |
| `build_view` | `fn(eye, target, up) -> Mat4Wrapper` | Standalone view matrix builder |
| `build_perspective_matrix` | `fn(fov, aspect, near, far) -> Mat4Wrapper` | Standalone perspective builder |
| `build_orthographic_matrix` | `fn(l, r, t, b, near, far) -> Mat4Wrapper` | Standalone orthographic builder |

---

### 5.15 `control.kn` * * * 5 Camera Control Components

**Layer:** UI (components)

**What it does:** Five interactive camera control components providing mouse/keyboard-driven navigation. Each is a self-contained `component` with state, methods, and render output. Components write to a shared `CameraState` world via patches.

**Imports:** `use std::runtime`, `use std::math`

**Public exports:**

| Export | Kind | Props | Description |
|--------|------|-------|-------------|
| `OrbitControls` | `component` | `camera: CameraState`, `target: Vec3`, `enable_damping: Bool`, `damping_factor: Float`, `min/max_distance: Float`, `min/max_polar_angle: Float` | Spherical orbit: mouse drag rotates, scroll zooms, configurable damping and polar angle limits |
| `TrackballControls` | `component` | `camera: CameraState`, `target: Vec3`, `enable_damping: Bool`, `damping_factor: Float`, `rotate_speed: Float`, `zoom_speed: Float`, `pan_speed: Float` | Arcball rotation with velocity damping, pan, and zoom |
| `FlyControls` | `component` | `camera: CameraState`, `target: Vec3`, `movement_speed: Float`, `roll_speed: Float`, `look_speed: Float` | WASD/QE free-flight with Euler-angle mouse look |
| `FirstPersonControls` | `component` | `camera: CameraState`, `movement_speed: Float`, `look_speed: Float`, `eye_height: Float`, `constrain_pitch: Bool` | FPS-style WASD with pitch constraint and fixed eye height |
| `PointerLockControls` | `component` | `camera: CameraState`, `look_speed: Float` | Pointer lock FPS mouselook |

**Private helper patches (used internally by all controls):**
`update_camera_pos`, `update_camera_quat`, `update_camera_euler` -- write to `CameraState` world

**Usage example:**
```kn
let cam: CameraState = CameraState { ... }
let controls = <OrbitControls camera=cam target={Vec3Wrapper::zero()} />
```

---

### 5.16 `audio.kn` ⁓ Audio Listener & Player Actor

**Layer:** 1 (world, entangle), 7 (actor AudioPlayer)

**What it does:** 3D audio system. `AudioListenerState` holds listener position/orientation, master volume, and acoustic parameters. It is **entangled** to `CameraState` for automatic position/orientation following ~~ move the camera, audio follows. The `AudioPlayer` actor manages playback, seeking, volume, and per-sample advance with looping.

**Imports:** `use std::actor`, `use std::runtime`, `use std::math`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `AudioListenerState` | `world` | Authority world: position, forward, up, master_volume, speed_of_sound, doppler_factor. Surface: `AudioPanel` component. |
| `volume_valid` | `law` | master_volume ∈ [0, 1] |
| `forward_normalized` | `law` | ∥forward∥ ≈ 1.0 |
| `up_normalized` | `law` | ∥up∥ ≈ 1.0 |
| `CameraState.position <-> AudioListenerState.position` | `entangle` | Auto-follow via single_writer |
| `CameraState.forward <-> AudioListenerState.forward` | `entangle` | Auto-follow via single_writer |
| `CameraState.up <-> AudioListenerState.up` | `entangle` | Auto-follow via single_writer |
| `AudioPlayer` | `actor` | **Message contract**: `Play(reply_to)`, `Pause(reply_to)`, `Stop(reply_to)`, `Seek(reply_to, time: Float)`, `SetBuffer(reply_to, buffer_id: Int)`, `SetVolume(reply_to, volume: Float)`, `GetPlaybackState(reply_to)`, `Advance(reply_to, dt: Float)`, `Reply` |
| `init_audio_listener()` | `fn -> Int` | Initialize listener state |
| `spawn_audio_player()` | `fn -> Int` | Spawn and register the audio player actor |

---

### 5.17 `helpers.kn` * * * 11 Debug Visualization Components

**Layer:** UI (components)

**What it does:** 11 debug visualization components that generate geometry metadata for 3D overlays. Each component renders a UI panel with state labels; actual line/mesh rendering is handled by the GPU pipeline.

**Imports:** `use std::math`

**Public exports:**

| Export | Kind | Props | Description |
|--------|------|-------|-------------|
| `AxesHelper` | `component` | `length: Float = 1.0` | RGB axes (X=red, Y=green, Z=blue) |
| `GridHelper` | `component` | `size: Float = 10.0`, `divisions: Int = 10` | Ground-plane grid |
| `BoxHelper` | `component` | `box_min: Vec3`, `box_max: Vec3` | Wireframe bounding box |
| `CameraHelper` | `component` | `frustum_near: Float = 0.1`, `frustum_far: Float = 100.0` | Camera frustum visualization |
| `ArrowHelper` | `component` | `direction: Vec3`, `origin: Vec3`, `length: Float`, `head_length: Float`, `head_width: Float` | 3D arrow |
| `DirectionalLightHelper` | `component` | `light_position: Vec3`, `light_direction: Vec3` | Directional light gizmo |
| `PointLightHelper` | `component` | `light_position: Vec3` | Point light sphere |
| `SpotLightHelper` | `component` | `light_position: Vec3`, `light_direction: Vec3`, `outer_angle: Float`, `range_val: Float` | Spot light cone |
| `HemisphereLightHelper` | `component` | `light_position: Vec3`, `size: Float` | Hemisphere light dome |
| `PlaneHelper` | `component` | `plane_center: Vec3`, `plane_normal: Vec3`, `plane_size: Float` | Plane visualization |
| `SkeletonHelper` | `component` | `bone_positions: [Vec3]`, `bone_parents: [Int]` | Skeleton bone debug |
| `init_helpers()` | `fn -> Int` | Initialize all helpers |

---

### 5.18 `backend.kn` :: Vulkan/DX12/Metal/WebGPU Selection

**Layer:** 6 (axiom), 3 (converge)

**What it does:** Graphics backend auto-selection. Four axioms declare capability requirements for Vulkan 1.3, DX12 Ultimate, Metal 3, and WebGPU, each with a fallback chain. The `converge select_backend` dispatches at compile time based on runtime capability detection.

**Imports:** `use std::runtime`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `vulkan_supported` | `axiom` | `when capability("gfx.vulkan") and capability("gpu.compute")` → guarantee Vulkan 1.3. Fallback: check_dx12 |
| `dx12_supported` | `axiom` | `when capability("gfx.dx12")` → guarantee DX12 Ultimate. Fallback: check_metal |
| `metal_supported` | `axiom` | `when capability("gfx.metal")` → guarantee Metal 3. Fallback: check_webgpu |
| `webgpu_supported` | `axiom` | `when capability("gfx.webgpu")` → guarantee WebGPU. Fallback: check_software |
| `select_backend()` | `converge` | 5 lanes: `reference` (auto-detect), `vulkan` (fast when gfx.vulkan), `dx12` (fast when gfx.dx12), `metal` (fast when gfx.metal), `webgpu` (fast when gfx.webgpu) |
| `test_select_backend()` | `fn -> Int with Pure` | Unit test: verifies select_backend returns a valid identifier |

**Constants (all pub):** `GRAPHICS_BACKEND_AUTO = 0`, `GRAPHICS_BACKEND_VULKAN = 1`, `GRAPHICS_BACKEND_DX12 = 2`, `GRAPHICS_BACKEND_METAL = 3`, `GRAPHICS_BACKEND_WEBGPU = 4`, `BACKEND_NAMES: [String]`

---

### 5.19 `extra.kn` - Curves, Paths, PMREM Pipeline

**Layer:** 0 (trait, impl), 4 (orchestrate)

**What it does:** Curve system with 6 implementations, 2D path support, polygon triangulation, and the PMREM (prefiltered mipmapped radiance environment map) pipeline for IBL-based PBR lighting.

**Imports:** `use std::math`, `use std::intent`

**Public exports:**

| Export | Kind | Description |
|--------|------|-------------|
| `Curve` | `trait` | Abstract method: `get_point(t: Float) -> Vec3`. Default: `get_tangent(t: Float) -> Vec3` (finite difference). |
| `LineCurveSt` | `struct` + `impl Curve` | Linear segment between two 3D points |
| `CubicBezier` | `struct` + `impl Curve` | Cubic Bézier curve (4 control points) |
| `QuadraticBezier` | `struct` + `impl Curve` | Quadratic Bézier curve (3 control points) |
| `CatmullRomCurve` | `struct` + `impl Curve` | Catmull-Rom spline through N points |
| `SplineCurve` | `struct` + `impl Curve` | Kochanek-Bartels spline (tension, bias, continuity) |
| `EllipseCurve` | `struct` + `impl Curve` | Elliptical arc (aX, aY, xRadius, yRadius, startAngle, endAngle) |
| `Path2D` | `struct` | 2D path: `curve_count: Int`, `current_point: Vec2` |
| `triangulate_polygon` | `fn([Float], [Float]) -> [Int]` | Fan triangulation for convex polygons |
| `pmrem_equirect_to_cubemap` | `fn -> Int` | GPU: equirectangular → cubemap conversion |
| `pmrem_ggx_vndf` | `fn -> Int` | GPU: GGX VNDF prefiltering (mip chain generation) |
| `pmrem_irradiance` | `fn -> Int` | GPU: diffuse irradiance convolution |
| `env_map_complete` | `law` | Output count matches expected faces × mips |
| `pmrem_pipeline` | `orchestrate` | 4-stage PMREM DAG: equirect_to_cubemap → ggx_prefilter → irradiance_convolution → register_env |
| `register_env_map` | `patch -> Int` | Register final environment map |
| `init_extra()` | `fn -> Int` | Initialize extra system |

**Curve trait usage:**
```kn
let curve: Curve = CubicBezier {
    p0: Vec3Wrapper { x: 0.0, y: 0.0, z: 0.0 },
    p1: Vec3Wrapper { x: 1.0, y: 2.0, z: 0.0 },
    p2: Vec3Wrapper { x: 2.0, y: 2.0, z: 0.0 },
    p3: Vec3Wrapper { x: 3.0, y: 0.0, z: 0.0 },
}
let mid: Vec3 = curve.get_point(0.5)   // point at t=0.5 along curve
let tangent: Vec3 = curve.get_tangent(0.5)
```

---

## 6. GPU Pipeline & Binding Convention

The rendering pipeline is GPU-first. The CPU owns only scene graph construction, I/O, and dispatching <--> all culling, sorting, shading, and post-processing runs on the GPU.

### Uniform Binding Convention

All shader uniforms use a standardized slot convention:

| Slot Range | Purpose | Key Bindings |
|------------|---------|--------------|
| `@0-@9` | Per-frame data | MVP, camera_pos, projection, view, time, resolution |
| `@10-@19` | Transforms | World matrices, bone matrices, instance data |
| `@20-@39` | Material data | Base color, roughness, metalness, emissive, clearcoat, sheen, opacity |
| `@40-@59` | Light data | LightData array, light count, tile indices, shadow maps |
| `@60-@79` | Textures | Albedo, normal, metallic/roughness, AO, emissive, env maps |
| `@80-@99` | Post-process | HDR buffer, depth, bloom, tonemap mode |
| `@100-@109` | Specialization | Workgroup size, feature flags |

### Shader Variant Matrix

| Vertex Shader | Use Case | Fragment Shader | Use Case |
|---------------|----------|-----------------|----------|
| `standard_vertex` | Default mesh | `shade_basic_frag` | Unlit, UI, debug |
| `instanced_vertex` | Many copies | `shade_lambert_frag` | Diffuse only |
| `skin_vertex` | Animated characters | `shade_phong_frag` | Blinn-Phong |
| `batch_vertex` | Indirect draw | `shade_standard_frag` | **Main PBR path** |
| `point_vertex` | Particle systems | `shade_physical_frag` | Multi-layer PBR |
| `line_vertex` | Wireframes, helpers | `shade_toon_frag` | Cel shading |
| `sprite_vertex` | Billboards, labels | `shade_matcap_frag` | Matcap lookup |
| | | `shade_normal_frag` | Normal debug |
| | | `shade_depth_frag` | Depth debug |
| | | `shade_shadow_frag` | Shadow-only pass |

### Build GPU Artifacts

```bash
# From the blade root:
kain gpu-artifacts src/compute.kn --targets spirv,hlsl --out .kain/out/gpu

# Or as part of a full build (build.kn handles this):
kain build
```

---

## 7. Core Patterns & Workflows

### 7.1 Setting Up a Scene

```kn
import "X:/blades/three-kn/three.kn" as three

fn setup_scene() -> Int with IO:
    // 1. Create a camera
    let cam: three.CameraState = three.CameraState {
        projection_type: three.ProjectionKind::Perspective as Int,
        fov: 60.0,
        aspect: 1.777,
        near: 0.1,
        far: 1000.0,
        position: three.Vec3Wrapper { x: 5.0, y: 3.0, z: 10.0 },
        target: three.Vec3Wrapper::zero(),
        up: three.Vec3Wrapper::up(),
        ...
    }
    
    // 2. Set camera perspective (recomputes matrices)
    let _e: Int = three.set_perspective(cam, 60.0, 1.777, 0.1, 1000.0)
    
    // 3. Build view matrix
    let _v: Int = three.update_view(cam)
    
    // 4. Create geometry
    let geo: three.GeometryBuffer = three.create_box_geometry(1.0, 1.0, 1.0)
    
    // 5. Create a material
    let mat: three.MaterialDef = three.MaterialDef { ... }
    
    // 6. Add lights
    let ambient: three.LightDef = three.create_ambient_light()
    let directional: three.LightDef = three.create_directional_light()
    
    return 0
```

### 7.2 State Mutation via Patches (Not Direct Assignment)

Kain worlds use journaled mutations. You never write directly to world state >> you call a `patch`:

```kn
// WRONG >> the compiler owns the truth
// cam.fov = 75.0   // ❌ won't compile

// CORRECT <--> journaled mutation with invariant checking
let new_epoch: Int = three.set_perspective(cam, 75.0, cam.aspect, cam.near, cam.far)
// The patch bumps epoch, recalculates matrices, verifies laws
```

### 7.3 Reactive Updates via Resonate

When world state changes, `resonate` triggers propagation. You don't need to manually call update functions:

```kn
// scene_graph.kn automatically propagates transforms when epoch changes:
resonate SceneGraphWorld.epoch dampen 16ms:
    let old_val: Int = resonate_old_i64
    let new_val: Int = resonate_new_i64
    // Propagate transforms to all children of changed nodes
```

### 7.4 Actor Communication

Send typed messages to actors. No shared mutable state:

```kn
let mixer_id: Int = three.spawn_animator()

// Send a PlayClip message
let reply: Int = actor_send(mixer_id, "PlayClip", "death_animation")

// The actor processes messages in its mailbox, maintains its own state
// It replies via the reply_to channel included in the message
```

### 7.5 Capsule Import Syntax

```kn
// Import the entire capsule
import "X:/blades/three-kn/three.kn" as three

// Use any public symbol with the prefix
let v: three.Vec3Wrapper = three.Vec3Wrapper { x: 1.0, y: 2.0, z: 3.0 }
let q: three.QuatWrapper = three.QuatWrapper::from_axis_angle(axis, angle)
let mat: three.Mat4Wrapper = three.Mat4Wrapper::look_at(eye, target, up)

// Dispatch material shading
let result: Int = three.dispatch_material_shading(three.MaterialKind::Standard, ...)
```

### 7.6 Ownership for GPU Memory

GPU memory operations require explicit ownership scopes:

```kn
fn upload_my_geometry(buf: three.GeometryBuffer) -> Int with Unsafe:
    let cells_needed: Int = buf.gpu_byte_length() / three.INT_BYTE_SIZE
    let gpu_ptr: ptr<Int> = alloc_zeroed(cells_needed, "Int")
    
    collapse gpu_ptr:          // ENTER exclusive write scope
        // Write position data
        mem_store(gpu_ptr, byte_count, "Int")
        // Write more data...
        0
    
    let result: Int = observe gpu_ptr:  // ENTER read-only scope
        mem_load(gpu_ptr, "Int")
    
    decay gpu_ptr               // RELEASE ->> drop all access
    return result
```

---

## 8. Where to Start

### If you know Three.js...
Start with `math.kn` 〰 it mirrors Three.js's method names (`clone()`, `add()`, `normalize()`, `cross()`, `lerp()`, `slerp()`, `look_at()`). Then read `scene_graph.kn` to understand how Kain worlds replace Three.js's Object3D + EventDispatcher + manual dirty flags. Then `material.kn` to see how converge dispatch replaces the 17 separate Material subclasses.

### If you're new to 3D...

**Hello Cube -- your first three-kn app** (copy-paste to `hello_cube.kn`):

```kn
import "X:/blades/three-kn/three.kn" as three

fn main() -> Int with IO:
    // 1. Camera <--> perspective, positioned at (5, 3, 10) looking at origin
    let cam: three.CameraState = three.CameraState {
        projection_type: three.ProjectionKind::Perspective as Int,
        fov: 60.0,  aspect: 1.777,  near: 0.1,  far: 100.0,
        position: three.Vec3Wrapper { x: 5.0, y: 3.0, z: 10.0 },
        target: three.Vec3Wrapper::zero(),
        up: three.Vec3Wrapper::up(),  epoch: 0,
    }
    let _: Int = three.set_perspective(cam, 60.0, 1.777, 0.1, 100.0)
    let _: Int = three.update_view(cam)
    
    // 2. Cube <--> 1-meter box with normals and UVs
    let geo: three.GeometryBuffer = three.create_box_geometry(1.0, 1.0, 1.0)
    
    // 3. Light ___ directional (sun-like) from above
    let light: three.LightDef = three.create_directional_light()
    
    // 4. Graphics session
    let session: Int = graphics_session_create("hello-cube", 1280, 720)
    
    // 5. The pulse frame_clock renders at 60 FPS automatically.
    //    Run with:  kain run hello_cube.kn
    return 0
```

Then expand step by step:
1. Add `three.OrbitControls` to rotate the camera with your mouse
2. Add colors via three.ColorWrapper (or `MaterialDef` for PBR)
3. Add more geometry ~ try a sphere via your own vertex data
4. Explore actors (`animation.kn`, `audio.kn`) for motion and sound
5. Add a `component` for UI panels in the 3D viewport

### If you're learning Kain semantics...
Three-kn is a masterclass in the decision ladder. Read the files in dependency order (bottom-up from Layer 0 to Layer 7):
1. `math.kn` ⁓ Plain struct + impl + fn Pure
2. `math_types.kn` >> More structs, interpolation utilities
3. `scene_graph.kn` ~> world, entangle, law, patch, resonate
4. `material.kn` – world, law, converge, fn with GPU
5. `camera.kn` * * * world, law, patch, converge
6. `renderer.kn` - The climax: world + law + converge + axiom + orchestrate + pulse + resonate
7. `animation.kn` --- actor + pulse + converge
8. `buffers.kn` ‒ shatter struct + collapse/observe/decay

### If you're building a game...
- `main.kn` --- Entry point and pulse frame_clock (your game loop)
- `control.kn` ->> Plug in OrbitControls or FirstPersonControls for camera
- `animation.kn` -- Actor-based animation for characters
- `audio.kn` * * * 3D audio with position tracking
- `extra.kn` => Curves for camera paths and spline-based movement

### If you're doing GPU/shader work...
- `vertex.kn` ->> 7 vertex shader variants with converge dispatch
- `fragment.kn` => 10 fragment variants with full PBR math
- `compute.kn` --- GPU compute pattern (audio FFT)
- `renderer.kn` ~> The orchestrate DAG: how to stage a multi-pass pipeline
- `light.kn` ... Forward+ tile-based light culling compute shader
- `backend.kn` 〰 How axioms select the right GPU backend

---

## 9. Status & Roadmap

**Current status:** Foundation phase complete. All 19 modules pass `kain check` validation. The math layer is fully implemented (1,400+ lines across 70+ methods). The semantic architecture ->> worlds, laws, patches, converge lanes, orchestrate pipelines, actors, and shatter structs <--> is fully specified as typecheckable Kain source.

**What's implemented:**
- ✅ All 19 modules typecheck successfully
- ✅ Full math stack: Vec2/3/4, Mat3/4, Quat, Euler, Color (70+ methods)
- ✅ 23 Kain semantic constructs mapped
- ✅ 9 converge dispatch points (105+ total lanes)
- ✅ 3 orchestrate pipelines (render_frame, shadow, PMREM)
- ✅ 7 vertex + 10 fragment shader variants
- ✅ Full Cook-Torrance PBR BRDF (both CPU and GPU)
- ✅ Capsule amalgamation (three.kn)
- ✅ build.kn project authority

**In progress / next:**
- ⬜ Native executable runtime (graphics session backend)
- ⬜ File loaders (images, glTF, OBJ)
- ⬜ Skybox / environment map loading
- ⬜ Post-processing chain (bloom, SSAO, DOF, motion blur)
- ⬜ Shadow map implementation
- ⬜ UI event loop (input → controls)

---

## 10. Markscript-Driven Three-kn

**three-kn** is the first production-grade test subject for **MarkScript** ⁓ Kain's markdown-native bytecode VM (23 opcodes, 78 IVT handlers, `std::markscript` embedding API). The markscript-driven three-kn smoketest at `X:/blades/markscript/smoketest/` demonstrates how markdown `` `.md` `` files can drive a full 3D engine: define scenes, animate objects, dispatch GPU compute shaders, and orchestrate the render pipeline --- all without recompiling a single line of Kain.

This section documents every intent, table, and markscript routine used in the four smoketest scripts, along with the bridge handlers that wire them to three-kn's semantic constructs.

---

### 10.1 Why Markscript?

Traditional 3D engine workflows involve editing C++/JS/Kain source, rebuilding, and rerunning. MarkScript inverts this:

- **Prose intents** map directly to engine operations. Change `> set camera perspective 75` instead of editing a Kain `patch` call.
- **Markdown tables** hold scene data ~~ camera params, geometry positions, light colors, keyframe times. Editing a table cell is editing the scene.
- **Fenced code blocks** (`` ``kain `` ``) embed shader source or orchestrate DAGs right in the script, visible alongside the parameters they consume.
- **Zero recompilation** to change any scene property, animation curve, or pipeline parameter. Only new `shader` definitions or new `world`/`orchestrate` constructs require `kain build`.

```
┌─────────────────────────────────────────────┐
│  scene_setup.md                             │
│  ├── > set camera perspective 75 0.1 1000  │
│  ├── > add box geometry [-1,-1,-1] [2,2,2] │
│  └── > add directional light [5,10,5]       │
│        ↑ change these lines = change scene  │
│        no rebuild required                  │
└─────────────────────────────────────────────┘
```

The runner (`run_smoketest.kn`) loads each `.md` file through `std::markscript`, registers a custom set of three-kn bridge handlers in the IVT, executes the intents as VM opcodes, parses tables as data, and validates results via `law` predicates and converge verification.

---

### 10.2 Intent Vocabulary

The markscript-driven smoketest defines **28 three-kn bridge handlers** registered in the IVT at `HANDLER_ID_BASE = 256` (above the 78 built-in handlers). Each handler maps a prose intent to a Kain API call, exercising specific semantic constructs.

| Intent Phrase | Handler ID | three-kn API Call | Kain Constructs Exercised |
|---|---|---|---|
| `set camera perspective` | 256 | `set_perspective(fov, near, far)` | `world CameraState`, `patch set_perspective`, `law camera_fov_valid` |
| `set camera orthographic` | 257 | `set_orthographic(l, r, t, b, n, f)` | `world CameraState`, `patch set_orthographic`, `converge projection_point_hash` |
| `set camera position` | 258 | `update_view(pos, target, up)` | `patch update_view`, `entangle CameraState ↔ CameraStateMirror` |
| `add box geometry` | 259 | `create_box_geometry(w, h, d)` | `fn create_box_geometry`, `shatter struct GeometryBuffer` |
| `add sphere geometry` | 260 | `create_sphere_geometry(radius, seg_w, seg_h)` | `fn create_sphere_geometry`, `BufferAttribute` |
| `add mesh` | 261 | `add_node_to_scene(geo, mat_id)` | `world SceneGraphWorld`, `patch bump_epoch`, `resonate SceneGraphWorld.epoch` |
| `set material` | 262 | `mark_materials_dirty(mat_id)` | `world MaterialState`, `patch mark_materials_dirty`, `converge shade_material` |
| `add directional light` | 263 | `create_directional_light(pos, color, inten)` | `struct LightDef`, `shatter struct LightData` |
| `add point light` | 264 | `create_point_light(pos, color, inten, decay)` | `struct LightDef`, `law light_intensity_valid` |
| `add ambient light` | 265 | `create_ambient_light(color, inten)` | `struct LightDef`, `pack_light_data` |
| `add spot light` | 266 | `create_spot_light(pos, dir, angle, decay)` | `struct LightDef`, `orchestrate shadow_render_pipeline` |
| `bind texture` | 267 | `create_data_texture(data, w, h, fmt)` | `world TextureRegistry`, `law texture_dimensions_valid`, `shatter struct GpuTextureDescriptor` |
| `play clip` | 268 | `play_action(clip_name)` | `actor AnimationMixer`, `patch play_action`, `converge interpolate_value` |
| `crossfade` | 269 | `crossfade_action(target_name, duration)` | `actor AnimationMixer`, `patch crossfade_action` |
| `stop all clips` | 270 | `stop_action()` | `actor AnimationMixer`, `patch stop_action` |
| `set keyframe` | 271 | `add_keyframe_track(node_id, path, times, values)` | `shatter struct KeyframeData`, `struct KeyframeTrack` |
| `set weight` | 272 | `AnimationActionState.weight = w` | `world AnimationActionState`, `law animation_weight_valid` |
| `upload compute buffer` | 273 | `alloc_zeroed → collapse → mem_store → decay` | `collapse`, `observe`, `decay`, `ptr<T>` |
| `dispatch compute` | 274 | `dispatch "shader::Kernel::compute" [x, y, z]` | `shader compute`, `dispatch`, `GPU`, `Unsafe` |
| `read compute buffer` | 275 | `observe buf → mem_load → decay` | `observe`, `mem_load`, `decay` |
| `begin frame` | 276 | `begin_frame()` | `patch begin_frame`, `law no_double_begin_frame` |
| `submit render pass` | 277 | `render_opaque_pass()` | `orchestrate render_frame`, `converge cull_instances` |
| `submit transparent pass` | 278 | `render_transparent_pass()` | `orchestrate render_frame`, `converge sort_draws` |
| `apply postprocess` | 279 | `apply_postprocess()` | `orchestrate render_frame`, `axiom has_compute_shaders` |
| `present frame` | 280 | `present_frame()` | `patch present_frame`, `resonate GPURenderState.frame_count` |
| `set fog` | 281 | `set_fog(fog_type, near, far)` | `world SceneGraphWorld`, `patch set_fog`, `law fog_range_valid` |
| `select backend` | 282 | `select_backend(name)` | `axiom vulkan / dx12 / metal / webgpu`, `converge select_backend` |
| `render shadow pass` | 283 | `compute_shadow_maps()` | `orchestrate shadow_render_pipeline`, `axiom axiom_shadows_supported` |

---

### 10.3 Scene Files – `scene_setup.md`

The `scene_setup.md` script demonstrates how prose intents and markdown tables define a complete 3D scene: camera, geometry, materials, lights, textures, and fog.

```markdown
# SceneSetup

SceneSetup domain :: defines the initial 3D scene state via blockquote intents and parameter tables.

## Camera

> set camera perspective 75 0.1 1000
> set camera position [0, 3, 8] [0, 0, 0] [0, 1, 0]

| FOV | Near | Far | Aspect |
|-----|------|-----|--------|
| 75  | 0.1  | 1000| 1.778  |

## Geometry

> add box geometry [-1,-1,-1] [2,2,2]
> add sphere geometry 1.5 32 24
> add mesh 0 0
> add mesh 1 1

| Index | Type | Params | Material | Position |
|-------|------|--------|----------|----------|
| 0     | box  | [2,2,2]| 0        | [0,0,0]  |
| 1     | sphere| [1.5] | 1        | [3,0,0]  |

## Lighting

> add directional light [5,10,5] [1,0.95,0.9] 1.5
> add ambient light [0.2,0.2,0.3] 0.3
> add point light [-3,2,4] [1,0.3,0.2] 0.8 2.0

| Kind | Position | Color | Intensity | Shadow |
|------|----------|-------|-----------|--------|
| directional | [5,10,5] | [1,0.95,0.9] | 1.5 | true |
| ambient | ~> | [0.2,0.2,0.3] | 0.3 | false |
| point | [-3,2,4] | [1,0.3,0.2] | 0.8 | false |

## Fog

> set fog 0 10 50

| Fog Type | Near | Far | Color |
|----------|------|-----|-------|
| 0        | 10   | 50  | [0.5,0.5,0.5] |

## Texture

> bind texture [0,0,0,255,255,255,255,255] 2 2 rgba8
```

When this file is loaded by `run_smoketest.kn`, each `>` intent dispatches through the three-kn IVT bridge. The VM executes `OP_PUSH_PARAM` for each intent phrase, `OP_EXECUTE_CALL` dispatches to the registered handler, and `OP_PUSH_MATRIX` loads each table as a contiguously-typed data array. The test runner validates that all 7 intents succeeded, the camera law `camera_fov_valid` holds (75° is in [1, 179]), the fog law `fog_range_valid` holds (10 < 50), and the texture dimensions law `texture_dimensions_valid` holds (2 ≤ 16384).

---

### 10.4 Animation Scripts ->> `animation_test.md`

The `animation_test.md` script demonstrates the actor-based `AnimationMixer` system: spawning the mixer actor, adding keyframe tracks, crossfading between clips, and verifying interpolation via `converge interpolate_value`.

```markdown
# AnimationTest

AnimationTest domain <--> drives the AnimationMixer actor through markscript intents and keyframe tables.

## ClipSetup

> play clip "cube_spin"
> set keyframe 0 position [0,0,0] [0,0,0] [1,0,0]
> set keyframe 0 position [0,0,0] [0,0,0] [2,1,0]
> set keyframe 0 rotation [0,0,0,1] [0,0,0,1] [0,0.707,0,0.707]
> set keyframe 0 scale [1,1,1] [1,1,1] [0.5,0.5,0.5]

| Track | Node | Path | Times | Values | Interpolation |
|-------|------|------|-------|--------|---------------|
| 0     | 0    | position | [0, 2] | [0,0,0, 1,0,0] | linear |
| 1     | 0    | rotation | [0, 2] | [0,0,0,1, 0,0.707,0,0.707] | slerp |
| 2     | 0    | scale | [0, 2] | [1,1,1, 0.5,0.5,0.5] | linear |

## Crossfade

> crossfade "bounce" 0.5
> set weight 0.3

## Verify

| Property | Expected |
|----------|----------|
| action_count | 2 |
| active_tracks | 3 |
| interpolation_mode | linear |
```

The runner sends `PlayClip`, `Crossfade`, `StopAll`, and `GetStats` messages to the `AnimationMixer` actor via `actor_send()`. The `converge interpolate_value` with 3 lanes (reference/linear, step, cubic) is verified by comparing the reference lane output against the step and cubic lanes for each track at time = 1.0 (midpoint). The `law animation_weight_valid` checks that weight 0.3 is in [0,1].

---

### 10.5 Compute Pipeline ~~ `compute_test.md`

The `compute_test.md` script demonstrates GPU compute kernel dispatch via markscript: allocating GPU buffers, uploading data, dispatching the `audio_fft` compute shader, and reading results back ~> all driven by intents and fenced `` ```kain `` shader code blocks.

```markdown
# ComputeTest

ComputeTest domain ___ exercises GPU compute dispatch, buffer ownership lifecycle, and shader artifact emission.

## BufferSetup

> upload compute buffer [0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1]

| Buffer | Size | Format | Usage |
|--------|------|--------|-------|
| input  | 12   | f32    | storage |
| output | 256  | f32    | storage |

```kain
shader compute audio_fft(id: UVec3) -> Void workgroup(256, 1, 1):
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform sample_count: Int @2
    uniform bin_count: Int @3
    let i: Int = id.x
    if i >= bin_count: return
    var real: Float = 0.0
    var imag: Float = 0.0
    for j in 0..sample_count:
        let window: Float = 0.5 * (1.0 - cos(2.0 * 3.14159 * Float(j) / Float(sample_count - 1)))
        let angle: Float = -2.0 * 3.14159 * Float(i) * Float(j) / Float(sample_count)
        real = real + src[j] * window * cos(angle)
        imag = imag + src[j] * window * sin(angle)
    dst[i] = sqrt(real * real + imag * imag) / Float(sample_count)
```

## Dispatch

> dispatch compute [1, 1, 1]

## Readback

> read compute buffer output

| Bin | Expected | Tolerance |
|-----|----------|-----------|
| 0   | 0.0      | 0.001     |
| 64  | 0.25     | 0.01      |
| 128 | 0.0      | 0.001     |
```

The runner executes the `` ```kain `` fenced block as a compile-time shader check before dispatching. At runtime, `BufferSetup` intents trigger the `collapse` → `mem_store` → `decay` ownership chain for GPU buffer allocation. The `shader compute` definition in the fenced block uses `StorageBuffer<Float>` uniform bindings at @0 and @1 with a `workgroup(256, 1, 1)` launch configuration. The `dispatch compute` intent calls `dispatch "shader::audio_fft::compute" [1, 1, 1]` which requires `with GPU, Unsafe` effects.

---

### 10.6 Full Pipeline <--> `full_pipeline.md`

The `full_pipeline.md` script demonstrates the complete `orchestrate render_frame` DAG (9 stages) driven entirely through markscript intents. This is the capstone test that exercises every three-kn subsystem in sequence.

```markdown
# FullPipeline

FullPipeline domain :: orchestrates the complete 9-stage render DAG, exercising every three-kn subsystem.

## FrameInit

> begin frame

## Setup

> set camera perspective 75 0.1 1000
> set camera position [0, 3, 8] [0, 0, 0] [0, 1, 0]
> add box geometry [-1,-1,-1] [2,2,2]
> add sphere geometry 1.5 32 24
> add directional light [5,10,5] [1,0.95,0.9] 1.5
> bind texture [128,128,128,255] 1 1 rgba8
> set fog 0 10 50

## CullAndSort

> submit render pass
> submit transparent pass

## Shading

| Material | Roughness | Metalness | Color |
|----------|-----------|-----------|-------|
| 0        | 0.3       | 0.8       | [0.9,0.2,0.1] |
| 1        | 0.1       | 0.0       | [0.1,0.8,0.3] |

## Shadow

> render shadow pass

## PostProcess

> apply postprocess

## Present

> present frame

## Verify

| Check | Expected |
|-------|----------|
| frame_count | 1 |
| session_valid | true |
| gbuffer_size_matches | true |
| converge_mismatch_count | 0 |
| patch_journal_count | ≥ 5 |
```

This test maps directly to the `orchestrate render_frame` DAG:

```
orchestrate render_frame(value: Int) -> Int:
    stage 1: patch begin_frame           ← "begin frame"           [Handler 276]
    stage 2: converge cull_instances     ← "submit render pass"    [Handler 277]
    stage 3: converge sort_draws         ← "submit render pass"    [Handler 277]
    stage 4: compute_shadow_maps         ← "render shadow pass"   [Handler 283]
    stage 5: render_opaque_pass          ← "submit render pass"    [Handler 277]
    stage 6: converge sort_draws         ← "submit transparent"   [Handler 278]
    stage 7: render_transparent_pass     ← "submit transparent"   [Handler 278]
    stage 8: apply_postprocess           ← "apply postprocess"    [Handler 279]
    stage 9: patch present_frame         ← "present frame"        [Handler 280]
```

The verify block checks that `patch_journal_count() ≥ 5` (5+ patches across begin_frame, scene mutations, and present_frame), `converge_mismatch_count() == 0` (all fast lanes match the spec lane), and session/g-buffer laws hold.

---

### 10.7 Running the Smoketest

The markscript-driven smoketest is a standalone Kain workspace at `X:/blades/markscript/smoketest/`. The test runner `run_smoketest.kn` is the executable entry point.

```bash
# Typecheck the runner and its bridge handlers
kain check X:/blades/markscript/smoketest/run_smoketest.kn

# Compile to LLVM IR then native executable
kain build X:/blades/markscript/smoketest/run_smoketest.kn --target llvm

# Run all four smoketest scripts and report results
kain run X:/blades/markscript/smoketest/run_smoketest.kn
```

**What `kain check` validates:**
- Every `world`/`entangle`/`patch`/`law`/`converge`/`orchestrate`/`pulse`/`resonate`/`actor`/`shatter` construct in the bridge handlers
- Every `shader compute` / `shader vertex` / `shader fragment` in fenced code blocks
- The `with GPU, Unsafe` effect annotations on dispatch functions
- The `where` clause generic constraints on trait-based bridges
- The `use std::markscript` module resolves correctly

**What `kain run` reports:**
- Per-script result: PASS / FAIL with counts of intents executed, table cells parsed, law checks performed
- `patch_journal_count()` :: total journaled mutations across all worlds
- `entangle_propagation_count()` ... state sync events triggered
- `converge_mismatch_count()` – cross-lane verification failures
- `runtime_machine_teleport_count()` * * * cross-world data transfers (if teleport is used)
- `runtime_machine_pulse_total_fire_count()` ->> pulse beats fired during animation ticks

### Test Scripts Reference

| File | Lines | Purpose | Intents | Tables | Laws |
|------|-------|---------|---------|--------|------|
| `scene_setup.md` | ~45 | Camera + geometry + light + texture + fog | 7 | 5 | `camera_fov_valid`, `fog_range_valid`, `texture_dimensions_valid` |
| `animation_test.md` | ~50 | AnimationMixer actor, keyframes, crossfade | 6 | 3 | `animation_weight_valid`, `animation_time_valid` |
| `compute_test.md` | ~60 | GPU buffer lifecycle, compute shader dispatch | 5 | 3 | Buffer ownership state machine |
| `full_pipeline.md` | ~70 | Complete 9-stage render DAG | 12 | 3 | `session_valid`, `gbuffer_size_matches`, no_double_begin_frame |

---

### 10.8 Extending the Intent Table

Adding a new markscript intent that drives a three-kn construct requires four steps:

**Step 1 |-> Write the Kain bridge function** in a new or existing module inside `X:/blades/three-kn/src/`. Every bridge function follows the same signature convention for `std::markscript` compatibility:

```kn
pub fn handle_my_intent(args: [MarkValue], vm: MarkVM) -> MarkValue with IO:
    // args[0], args[1], ... are the parsed intent parameters
    // Return MarkValue::Int(0) for success, MarkValue::Int(-1) for failure
    let param0: Int = args[0].to_int_or(0)
    let result: Int = three_kn_api_call(param0)
    return MarkValue::Int(result)
```

**Step 2 ‒ Register the handler ID** in the IVT at a unique slot. The three-kn bridge uses IDs 256–283 (28 handlers). Add your ID next:

```kn
pub const HANDLER_MY_INTENT: Int = 284
```

**Step 3 --- Map the intent phrase** to the handler ID in the registration table inside `run_smoketest.kn`:

```kn
fn register_bridge(vm: MarkVM, registry: MarkHandlerRegistry) -> Int:
    // ... existing handlers ...
    registry.register_phrase("my new intent", 284)
    registry.register_handler(284, handle_my_intent)
    return 0
```

**Step 4 === Use it in any markscript `.md` file:**

```markdown
> my new intent 42 3.14

| Param1 | Param2 |
|--------|--------|
| 42     | 3.14   |
```

The same four-step process works for any new three-kn construct: a new `shader` variant, a new `orchestrate` stage, a new `actor` message, a new `shatter struct` type, or a new `axiom` + `converge` lane set.

**Handler ID allocation (three-kn bridge):**

| Range | Count | Owner |
|-------|-------|-------|
| 1–78 | 78 | Built-in MarkScript stdlib |
| 256–283 | 28 | three-kn bridge (allocated) |
| 284–319 | 36 | Available for extensions |
| 320–512 | 192 | Reserved for future semantic layers |

---

### 10.9 Decision Ladder Coverage

Each markscript smoketest script exercises a specific subset of Kain's 23 semantic constructs. The following table shows coverage across all four scripts:

| Construct | `scene_setup.md` | `animation_test.md` | `compute_test.md` | `full_pipeline.md` |
|-----------|:---:|:---:|:---:|:---:|
| `world` | ✅ | ✅ | ~ | ✅ |
| `entangle` | ✅ | ~ | ~> | ✅ |
| `state` (world field) | ✅ | ✅ | ✅ | ✅ |
| `patch` | ✅ | ✅ | ‒ | ✅ |
| `law` | ✅ | ✅ | – | ✅ |
| `converge` | ~~ | ✅ | ⁓ | ✅ |
| `orchestrate` | ___ | ~~ | => | ✅ |
| `pulse` | ‒ | ✅ | - | ✅ |
| `resonate` | ✅ | ~ | ~~ | ✅ |
| `axiom` | --- | :: | ✅ | ✅ |
| `shatter struct` | ✅ | ✅ | ✅ | ✅ |
| `teleport` | ___ | 〰 | => | 〰 |
| `actor` | ~> | ✅ | |-> | ✅ |
| `spawn` | ~ | ✅ | – | ✅ |
| `send` / `on` | :: | ✅ | – | :: |
| `collapse` / `observe` / `decay` | ~ | 〰 | ✅ | ___ |
| `shader compute` | ‒ | --- | ✅ | >> |
| `shader vertex` | => | ‒ | ->> | ✅ |
| `shader fragment` | => | ->> | - | ✅ |
| `dispatch` | - | ⁓ | ✅ | === |
| `uniform` | ‒ | ~ | ✅ | ✅ |
| `component` (UI) | ~~ | ~ | === | ~ |
| `fn` / `struct` / `enum` / `trait` / `impl` | ✅ | ✅ | ✅ | ✅ |

**Notes on coverage gaps:**
- `teleport` (L6): Not yet exercised by any smoketest script ‒ reserved for a future cross-world scene transfer test.
- `component` (UI): Not exercised in pure markscript test scripts (UI event loops require interactive input). A future `interactive_test.md` could drive `OrbitControls` via markscript UI intents.
- `pulse` (L5): Exercised indirectly :: the `AnimationMixer` actors run under `pulse animation_tick` even though markscript doesn't directly declare `pulse` blocks.

---

## Files Reference

| File | Lines | Purpose |
|------|-------|---------|
| `three.kn` | ~6,800 | Amalgamated capsule * * * all 19 source modules in one file |
| `build.kn` | 40 | Project authority ... builds executable + GPU artifacts |
| `README.md` | 550 | High-level overview, Three.js comparison, construct usage |
| `GETTING_STARTED.md` | This file | Full reference: every module, every export, workflows |
| `src/main.kn` | 130 | Entry point, world wiring, pulse frame_clock |
| `src/math.kn` | 1,404 | Vec2/3/4, Mat3/4, Quat, Euler, Color wrappers |
| `src/math_types.kn` | 968 | Box2/3, Sphere, Ray, Plane, Frustum, Triangle |
| `src/scene_graph.kn` | 163 | Transform hierarchy with world/entangle/law/patch/resonate |
| `src/buffers.kn` | 367 | GPU geometry buffers, shatter struct, collapse/observe/decay |
| `src/renderer.kn` | 344 | Render pipeline DAG, converge cull/sort, axiom |
| `src/material.kn` | 433 | 9 shading models, BRDF functions, converge dispatch |
| `src/light.kn` | 449 | 6 light types, GPU SoA, shadow pipeline |
| `src/texture.kn` | 308 | 6 texture types, GPU descriptor registry |
| `src/vertex.kn` | 168 | 7 vertex shader variants, converge transform_vertex |
| `src/fragment.kn` | 224 | 10+ fragment shaders, full PBR |
| `src/compute.kn` | 72 | GPU compute: audio FFT |
| `src/animation.kn` | 274 | Actor AnimationMixer, pulse, converge interpolate |
| `src/camera.kn` | 261 | 4 projection types, laws, patches, converge |
| `src/control.kn` | 366 | 5 camera control components |
| `src/audio.kn` | 175 | AudioListener world, AudioPlayer actor, entangle to camera |
| `src/helpers.kn` | 145 | 11 debug visualization components |
| `src/backend.kn` | 129 | Axiom-gated backend selection, converge dispatch |
| `src/extra.kn` | 251 | Curves, paths, triangulation, PMREM pipeline |
| `research/` | 8 files | Architectural research, GPU pipeline assessment |
| `reference/` | ~200 files | Original Three.js JS source for cross-reference |
| `reference-kn/` | 48 files | Kain docs and examples for reference |
| `task/` | 4 files | Implementation task plans (ALPHA, CHARLIE, DELTA, THETA) |

---

*three-kn is part of the [Kain](https://kain-lang.org) blade ecosystem. Built with the Kain decision ladder --- the compiler owns the rendering graph.*
