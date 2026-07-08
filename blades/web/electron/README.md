# Kain Electron Playground

A Jupyter-like 3D engine where **Kain programs** drive live **Three.js visualizations** via a JSON-over-stdout bridge. Drop a `.kn` file in `demos/`, build it, and the Electron app discovers and runs it automatically.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Electron Window (main.js → renderer.js)                     │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  Three.js Scene + OrbitControls                       │  │
│  │  SceneManager (renderer.js) interprets JSON commands  │  │
│  │  Props tab — HUD telemetry from Kain                  │  │
│  └────────────────────────────────────────────────────────┘  │
│                     ▲ stdin (JSON lines)                      │
│                     │ stdout (JSON ⇐ Kain println)            │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  Kain .exe (child_process spawned by main.js)          │  │
│  │  → electron_bridge.kn emits structured JSON            │  │
│  │  → println(json_stringify(cmd)) is the one-way door    │  │
│  │  → electron_extras.kn adds visual effects              │  │
│  │  → electron_interact.kn reads input backchannel        │  │
│  └────────────────────────────────────────────────────────┘  │
│                     ▲ stdin (bridge-event JSON)               │
│                     │ ipcRenderer → main.js backchannel       │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  renderer.js DOM event listeners                        │  │
│  │  → click, mousemove, keydown, keyup, raycaster         │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

**How it works:**

1. `main.js` spawns a Kain `.exe` as a child process (`child_process.spawn`)
2. The Kain program imports `electron_bridge` and runs its animation loop
3. Scene commands: `println(json_stringify(cmd))` — a JSON command on stdout
4. `main.js` pipes stdout line-by-line to `renderer.js` via IPC
5. `SceneManager` (renderer.js) interprets each JSON command and updates Three.js
6. Input backchannel: renderer.js DOM events → IPC → main.js stdin pipe → `interact_poll()`
7. The user sees real-time 3D animation in the Electron window

No shared memory, no Node addons, no C++. Just a Kain EXE printing JSON to stdout.

---

## Bridge API

Three bridge modules compose the full API surface. Import what you need.

### electron_bridge.kn — Core Render Protocol

Scene lifecycle, object CRUD, transforms, camera, telemetry.

| Function | Signature | Description |
|---|---|---|
| `scene_clear` | `()` | Wipes the entire scene |
| `camera_look_at` | `(px, py, pz, tx, ty, tz)` | Camera position + look-at target |
| `create_mesh` | `(id, geometry, color_hex)` | Single mesh. Geo: `box`, `sphere`, `cylinder`, `cone`, `torus`, `torusKnot`, `ring`, `plane`, `circle`, `tube`, `tetrahedron`, `octahedron`, `icosahedron`, `dodecahedron` |
| `create_instanced` | `(id, geometry, count, color_hex)` | Instanced mesh — one draw call for N objects. Use for 10K+ |
| `create_particles` | `(id, count, color_hex)` | Particle system with N points |
| `create_lines` | `(id, color_hex)` | Line geometry |
| `create_custom_mesh` | `(id, verts: JsonArray, indices: JsonArray, color_hex)` | Raw vertex+index buffers for procedural geometry |
| `update_transform` | `(id, x, y, z, sx, sy, sz)` | Position + scale (no rotation) |
| `update_transform_r` | `(id, x, y, z, rx, ry, rz, sx, sy, sz)` | Position + Euler rotation + scale |
| `update_instanced_transforms` | `(id, transforms: JsonArray)` | Bulk transform for instanced meshes. Hot path for 10K+ |
| `update_particle_positions` | `(id, positions: JsonArray)` | Update particle positions |
| `update_line_points` | `(id, points: JsonArray)` | Rebuild line geometry |
| `update_color` | `(id, color_hex)` | Change object color |
| `update_custom_verts` | `(id, verts: JsonArray)` | Modify vertex positions of custom mesh |
| `remove` | `(id)` | Delete object by ID |
| `telemetry` | `(key: String, value: Int)` | Int counter to HUD Props tab |
| `telemetry_float` | `(key: String, value: Float)` | Float value to HUD Props tab |
| `telemetry_string` | `(key: String, value: String)` | String status to HUD Props tab |
| `transform` | `(x, y, z, sx, sy, sz) -> JsonObject` | Build `{x,y,z,sx,sy,sz}` for bulk updates |
| `transform_r` | `(x, y, z, rx, ry, rz, sx, sy, sz) -> JsonObject` | Build with rotation for bulk instanced updates |
| `point` | `(x, y, z) -> JsonObject` | Build `{x,y,z}` for particle/line updates |

### electron_extras.kn — Visual Effects Layer

Atmosphere, materials, lights, camera controls.

| Function | Signature | Description |
|---|---|---|
| `set_background` | `(color_hex)` | Change scene background color |
| `set_fog` | `(color_hex, near, far)` | Exponential fog for depth atmosphere |
| `update_emissive` | `(id, color_hex, intensity)` | Make an object glow from within. 0.0–5.0+ |
| `update_particle_size` | `(id, size)` | Change particle size. 0.01 (dust) to 1.0+ (orbs) |
| `update_rotation` | `(id, rx, ry, rz)` | Rotation-only update (Euler radians) |
| `set_auto_rotate` | `(enabled, speed)` | Enable/disable automatic camera orbit |
| `create_light` | `(id, light_type, color_hex, intensity, x, y, z)` | Add dynamic light. Types: `ambient`, `directional`, `point`, `spot` |
| `update_light` | `(id, color_hex, intensity)` | Change existing light color/intensity |
| `update_light_position` | `(id, x, y, z)` | Move an existing light |
| `remove_light` | `(id)` | Delete a light |
| `set_ambient` | `(color_hex, intensity)` | Replace ambient light |
| `create_label` | `(id, text, x, y, z, color_hex, size)` | 3D text label via Sprite |
| `set_visible` | `(id, visible)` | Show/hide any object |
| `create_trail` | `(id, target_id, max_points, color_hex, width)` | Smooth trail following a target |
| `enable_selection` | `(id, enabled)` | Enable click-to-select with glow |
| `get_selected_id` | `() -> String` | Currently selected object id, "" if none |

### electron_interact.kn

Keyboard, mouse, click, and 3D raycast interaction. Uses `std::input` as the
backbone. **New API** (preferred): interact_init + interact_poll + interact_begin_frame
+ interact_action_pressed/down/released + interact_axis.
**Legacy API** (backward-compatible): click_count, key_pressed, mouse_x, ray_*.

| Function | Signature | Description |
|---|---|---|
| `interact_init` | `() -> Int` | Create std::input session, bind default actions. Call ONCE at startup |
| `interact_poll` | `()` | Read one stdin event, push into std::input + world state. Call once/frame |
| `interact_begin_frame` | `(delta_ms)` | Process queued events, compute action states for this frame |
| `interact_action_pressed` | `(action: String) -> Int` | Was action pressed THIS frame? (1/0) |
| `interact_action_down` | `(action: String) -> Int` | Is action currently held? (1/0) |
| `interact_action_released` | `(action: String) -> Int` | Was action released THIS frame? (1/0) |
| `interact_axis` | `(axis: String) -> Float` | Analog axis value (mouse_x, mouse_y) |
| `interact_mouse_x` | `() -> Float` | Convenience: mouse X from axis system |
| `interact_mouse_y` | `() -> Float` | Convenience: mouse Y from axis system |
| `interact_clear` | `()` | Reset per-frame state. Call at END of frame (no-op in new design) |
| `click_count` | `() -> Int` | Clicks since last clear (max 8/frame) |
| `click_x` | `(index: Int) -> Float` | Screen X of click #index |
| `click_y` | `(index: Int) -> Float` | Screen Y of click #index |
| `key_pressed` | `(key: String) -> Bool` | Is key currently held? |
| `key_just_pressed` | `(key: String) -> Bool` | Was key pressed THIS frame? |
| `mouse_x` | `() -> Float` | Current mouse X (screen coords) |
| `mouse_y` | `() -> Float` | Current mouse Y (screen coords) |
| `ray_hit` | `() -> Bool` | Did last click hit a 3D object? |
| `ray_hit_id` | `() -> String` | Kain-side id of hit object |
| `ray_hit_x` | `() -> Float` | World-space X of hit point |
| `ray_hit_y` | `() -> Float` | World-space Y of hit point |
| `ray_hit_z` | `() -> Float` | World-space Z of hit point |
| `ray_hit_nx` | `() -> Float` | Surface normal X at hit point |
| `ray_hit_ny` | `() -> Float` | Surface normal Y at hit point |
| `ray_hit_nz` | `() -> Float` | Surface normal Z at hit point |
| `ray_distance` | `() -> Float` | Distance from camera to hit point |

> **Quick reference TSV:** `electron.tsv.json` — every public function with JS handler locations.

---

## How to Add a Demo

### Quick start

1. **Write** `demos/your_demo.kn` — import `electron_bridge`, use its API
2. **Register** in `build.kn` — copy an existing demo block, rename it
3. **Build** — `kain build .` from `X:/blades/web/electron`
4. **Run** — `npm start` from `X:/blades/web/electron`

The Electron app discovers all `.exe` files in `demos/` at runtime via `discoverSources()` in `main.js`. No settings.js edits required.

### Minimal Template

```kain
use std::runtime
use std::math
use std::time
use electron_bridge
use electron_extras
use electron_interact

// ── CONSTANTS ──────────────────────────────────────────
const PARTICLE_N: Int = 200

// ── WORLD: ready-gate ──────────────────────────────────
world MyScene:
    state ready: Int = 0

// ── PULSE: animation (~60fps) ─────────────────────────
pulse my_clock every 16 ms:
    if MyScene.ready == 0:
        return

    // 1. Poll input
    interact_poll()

    // 2. Handle keys
    if key_just_pressed("c"):
        scene_clear()
    if key_just_pressed("r"):
        set_auto_rotate(true, 2.0)

    // 3. Clear per-frame state
    interact_clear()

    // 4. Animate
    let t: Float = pulse_tick * 0.06
    // ... compute positions, update objects ...

    // 5. Telemetry
    telemetry("tick", pulse_tick)

// ── MAIN: setup scene, then keep-alive ─────────────────
fn main() -> Int with IO:
    let init = runtime_init()
    if init != 0: return 100 + init

    // Atmosphere
    set_background("#0a0a1e")
    set_fog("#0a0a1e", 8.0, 45.0)
    set_ambient("#202040", 0.35)
    set_auto_rotate(true, 1.5)

    // Camera
    camera_look_at(0.0, 4.0, 12.0, 0.0, 0.0, 0.0)

    // Objects
    create_particles("my_particles", PARTICLE_N, "#ff8844")
    create_light("my_light", "point", "#ff8844", 3.0, 5.0, 0.0, 0.0)
    telemetry_string("status", "my demo v1")

    // Unlock pulse
    MyScene.ready = 1

    // Keep-alive
    var tick: Int = 0
    while tick < 999999:
        let _ = sleep_millis(1000)
        tick = tick + 1

    let sd = runtime_shutdown()
    if sd != 0: return 200 + sd
    return 0
```

### Build.kn entry (add after existing demos)

```kain
let my_demo_project = project("demo-mydemo")
    .kind("kain_executable")
    .version("0.1.0")
    .description("My custom demo")
    .entry("demos/my_demo.kn")
    .source_root(demos_dir)
    .module_root(demos_dir)
    .targets("llvm")
    .artifact_root(out_root)
    .cache_root(blade_cache)
    .profile("debug")

let my_demo_check = check_task("check-mydemo")
    .project(my_demo_project)
    .target("llvm")
    .input(input_glob)
    .input(build_kn)

let my_demo_exe = native_executable("exe-mydemo")
    .project(my_demo_project)
    .target("llvm")
    .output("$blade/demos/my_demo.exe")
    .requires("check-mydemo")
    .input(input_glob)
    .input(build_kn)
```

Then add to the return chain:
```kain
    .project(my_demo_project)
    .task(my_demo_check)
    .task(my_demo_exe)
```

---

## Patterns to Follow

### 1. World Ready-Gate

Use a `world` with a `ready` flag. The pulse handler checks it before animating.

```kain
world MyScene:
    state ready: Int = 0

pulse main_loop every 16 ms:
    if MyScene.ready == 0:
        return
    // ... animation code ...
```

Set `ready = 1` after `runtime_init()` and all scene setup. This ensures the scene is fully built before the first pulse fires.

### 2. Pulse for Animation

Use `pulse` (L5 temporal) instead of busy-wait loops. Pulse fires on its own OS thread at the declared interval.

```kain
pulse anim every 16 ms:           // ~60fps
    let t: Float = pulse_tick * 0.06
    // compute transforms
    update_particle_positions("particles", positions)
    telemetry("tick", pulse_tick)
```

Pulse body locals: `pulse_tick` (monotonic counter), `pulse_dt_ms` (actual elapsed), `pulse_missed` (missed beats).

### 3. While Loop for Keep-Alive

After `runtime_init()`, keep the process alive. The pulse handler does the actual work on its own thread.

```kain
fn main() -> Int with IO:
    let init = runtime_init()
    // ... scene setup ...
    MyScene.ready = 1

    var tick: Int = 0
    while tick < 999999:
        let _ = sleep_millis(1000)
        tick = tick + 1

    let sd = runtime_shutdown()
    return sd
```

### 4. Interaction Polling

Every frame: `interact_poll()` → check keys/clicks → `interact_clear()`.

```kain
pulse game_loop every 16 ms:
    interact_poll()

    if key_just_pressed("c"):
        scene_clear()
    if key_just_pressed("r"):
        set_auto_rotate(true, 1.5)

    var ci: Int = 0
    while ci < click_count():
        let cx = click_x(ci)
        let cy = click_y(ci)
        // handle click at (cx, cy)...
        ci = ci + 1

    interact_clear()
```

`interact_poll()` uses `read_line()` which blocks until a line arrives. In practice, Electron sends `mousemove` events frequently, keeping the pipe fed.

### 5. Inline JSON for Hot Paths

For particle systems with hundreds or thousands of particles, avoid `json_object()` + `json_array_push_object()` overhead. Build JSON strings directly:

```kain
var json: String = "{\"cmd\":\"update\",\"id\":\"particles\",\"flat\":["
var i: Int = 0
while i < N:
    if i > 0: json = json + ","
    json = json + str(x) + "," + str(y) + "," + str(z)
    i = i + 1
json = json + "]}"
println(json)
```

The `"flat"` key on the JS side unpacks the flat `[x0,y0,z0, x1,y1,z1, ...]` array directly into the position buffer without per-point object parsing.

---

## Interaction System Overview

The interaction system is **bidirectional** — Kain sends scene commands to Electron, and Electron sends input events back to Kain.

### Forward Channel (Kain → Electron)
```
Kain println(json) → stdout pipe → main.js → IPC 'kain-data' → renderer.js SceneManager.execute()
```

### Backchannel (Electron → Kain)
```
renderer.js DOM events → ipcRenderer.send('bridge-event') → main.js → bridgeProcess.stdin.write(line + '\n')
                                                                    ↓
                                                              Kain read_line() → interact_poll() → world state
```

### Event Types

| Event | JS Source | Kain Handler |
|---|---|---|
| `click` | `renderer.domElement click` | `__handle_click()` → `click_count()` / `click_x()` / `click_y()` |
| `mousemove` | `renderer.domElement mousemove` (throttled 16ms) | `__handle_mousemove()` → `mouse_x()` / `mouse_y()` |
| `keydown` | `window keydown` (non-repeat, non-Space/Ctrl+O) | `__handle_keydown()` → `key_pressed()` / `key_just_pressed()` |
| `keyup` | `window keyup` | `__handle_keyup()` → removes from held keys |
| `raycast` | raycaster on click (intersect scene objects) | `__handle_raycast()` → `ray_hit()` / `ray_hit_id()` / `ray_hit_x()` etc. |

### 3D Raycaster

On every click, renderer.js casts a ray through the mouse position against all scene objects. If it hits, a `raycast` event is sent to Kain with: hit point (world-space), surface normal, object ID, and distance.

This enables **click-to-deform terrain** (raycast.kn), **click-to-select lanes** (converge.kn), and **click-to-spawn** patterns.

---

## Custom Mesh / Procedural Geometry

For meshes not covered by the built-in geometry primitives (box, sphere, torus, etc.), use `create_custom_mesh`.

### Pattern

```kain
// Build vertex array: flat [x0,y0,z0, x1,y1,z1, ...]
var verts: JsonArray = json_array()
// ... push Float values ...

// Build index array: flat [i0,i1,i2, i3,i4,i5, ...] — 3 per triangle
var indices: JsonArray = json_array()
// ... push Int values ...

create_custom_mesh("my_terrain", verts, indices, "#448866")
```

### Inline JSON Pattern (faster for large meshes)

```kain
println("{\"cmd\":\"create\",\"id\":\"terrain\",\"type\":\"custom_mesh\",\"verts\":" +
    vert_json + ",\"indices\":" + idx_json + ",\"color\":\"#448866\"}")
```

### Updating

```kain
update_custom_verts("terrain", new_verts)  // verts must match original count
```

This rebuilds the vertex buffer on the JS side. The index array is preserved.

### Examples

- **engine_terrain.kn** — 50×50 grid (2,500 verts, ~4,800 triangles) with multi-octave wave height
- **cosmic_waves.kn** — Same 50×50 grid with 5-octave wave physics
- **raycast.kn** — Static terrain deformed by click raycast hits

---

## Visual Effects

All effects live in `electron_extras.kn`. They wire into the same `SceneManager.execute()` dispatch as core commands.

### Lights

```kain
create_light("sun", "directional", "#ffcc88", 4.0, 7.0, 4.0, 0.0)
create_light("glow", "point", "#ff6644", 2.5, -3.0, 1.5, 2.0)
create_light("fill", "ambient", "#334466", 0.3, 0.0, 0.0, 0.0)

// Animate each frame
update_light_position("glow", cos(t) * 5.0, sin(t * 1.5) * 2.0, sin(t) * 5.0)
update_light("glow", "#ff8844", 2.5 + sin(t * 2.0) * 0.8)

// Change ambient
set_ambient("#1a1a3a", 0.4)
```

### Fog

```kain
set_fog("#0a0a2e", 8.0, 45.0)     // visible atmospheric depth
set_fog("#0a0a2e", 999.0, 1000.0) // effectively off
```

### Emissive Glow

```kain
update_emissive("core", "#ff5500", 2.5 + sin(t * 2.0) * 0.8)   // pulsing glow
update_emissive("ring", "#4488ff", 0.4)                          // dim / off
```

### Rotation

```kain
update_rotation("ring", 0.0, t * 0.7, 0.0)  // Y-axis spin
update_rotation("tilted", 0.15, t * 0.4, 0.0)  // tilted + spin
```

### Particle Sizing

```kain
update_particle_size("dust", 0.04 + sin(t * 5.0) * 0.02)     // shimmering
update_particle_size("orbs", 0.10 + sin(t * 1.5) * 0.03)     // breathing
```

### Camera Auto-Rotate

```kain
set_auto_rotate(true, 1.5)   // gentle orbit
set_auto_rotate(false, 0.0)  // stop
```

### 3D Text Labels

```kain
// Place a label at a world-space position
create_label("title", "Hello World", 0.0, 2.0, 0.0, "#ffffff", 2.0)
create_label("warning", "DANGER", -2.0, 1.5, 1.0, "#ff4444", 3.0)
```

Labels use `THREE.Sprite` with a dynamically-generated canvas texture. The text
is rendered to a small canvas, then used as a sprite material. Labels always
face the camera and are depth-sorted.

### Visibility Control

```kain
// Hide any object by id (works on meshes, instanced, particles, lines, labels, trails)
set_visible("my_mesh", false)    // hide
set_visible("my_mesh", true)     // show
```

### Motion Trails

```kain
// Create a 100-point trail following a moving object
create_trail("ghost_trail", "flying_cube", 100, "#ff8844", 2.0)
```

The trail accumulates the target object's world position each frame, trims to
`max_points`, and updates a `THREE.Line` geometry. Works with any movable object.

### Click-to-Select

```kain
// Enable selection on an object (click → emissive glow highlight)
enable_selection("my_mesh", true)

// Later, check which object is selected
let sel = get_selected_id()    // "my_mesh" if clicked, "" if nothing selected

// Disable selection
enable_selection("my_mesh", false)
```

When a selectable object is clicked:
- The object glows with a blue emissive highlight (0x4488ff, intensity 0.5)
- The original material is saved and restored on deselect
- A `select` bridge-event is sent back to Kain
- `get_selected_id()` returns the object's id

Clicking a different selectable object deselects the previous one. Clicking empty
space does NOT deselect (use `enable_selection(id, false)` to clear).

---

## Working Demos

| Demo | File | Description | Layers |
|---|---|---|---|
| **bridge** | `bridge.kn` | Live entangle visualization. Two glowing spheres (source + mirror) connected by particle beam. Keyboard P=flash, C=clear. Orbiting particles + beam flow at 60fps. | L1 (world, entangle), L5 (pulse), extras (bg/fog/emissive/lights/rotation/particle-size), interact (keys) |
| **actor** | `actor.kn` | Actor message-passing model. Central emissive sphere (actor core), rotating mailbox torus, 60 message particles that fly in and bounce away. M=burst, C=clear. | L1 (world), L5 (pulse), extras (bg/fog/emissive/lights/rotation/auto-rotate), interact (keys) |
| **converge** | `converge.kn` | Three concentric particle rings (gold=spec, blue=fast, red=fallback) with spinning guide toruses. Converge selects winning lane each tick. Center core pulses matching color. Keys 1/2/3=manual, C=auto. | L1 (world), L3 (converge: spec+fast+verify random(8)), L5 (pulse), extras (emissive/lights/rotation/bg/fog/ambient/auto-rotate), interact (keys) |
| **pulse** | `pulse.kn` | Glowing clock face with sweeping second hand and 8 expanding ripple rings. Heartbeat point light pulses on each tick. T=speed toggle, R=rotate toggle, Click=burst. | L1 (world), L5 (pulse 16ms), extras (bg/fog/emissive/lights/rotation/auto-rotate), interact (keys+click) |
| **blackhole** | `blackhole.kn` | Kerr black hole simulation. 1,500 accretion disk particles, 400 jet particles, 200 orbiting stars, 3 pulsing rings (photon, einstein, event horizon), 3 orbiting point lights. F=fog, L=lights, R=rotate. 500µs microsecond test pulse. | L1 (world), L5 (pulse 16ms + 500µs), extras (bg/fog/emissive/lights/rotation/particle-size/auto-rotate/ambient), interact (keys) |
| **engine_terrain** | `engine_terrain.kn` | 50×50 procedural terrain grid (~2,500 verts) with multi-octave sin/cos height generation. Click to warp. Keys 1/2/3=frequency modes. F=toggle fog. 3 orbiting lights. | L1 (world), L5 (pulse 60ms), extras (bg/fog/lights/ambient/auto-rotate), interact (keys+click), custom_mesh |
| **cosmic_waves** | `cosmic_waves.kn` | 50×50 wave-terrain with 5-octave physics. Orbiting particle ring, central beacon with pulsing emissive, 3 dynamic lights. Single wave model drives terrain height + particle elevation. | L1 (world), L5 (pulse 32ms), extras (bg/fog/emissive/lights/auto-rotate), custom_mesh |
| **cosmic_forge** | `cosmic_forge.kn` | Dynamic cosmic forge. torusKnot core with intense emissive glow, golden halo particles, 1,000 accretion disk particles, blue outer ring, 3 colored orbiting point lights. | L1 (world), L5 (pulse 16ms), extras (bg/fog/emissive/lights) |
| **raycast** | `raycast.kn` | Static terrain sculpting. 50×50 grid. Click anywhere on the terrain to create persistent bumps. 3 concurrent bumps stored in world state. No pulse — pure interaction-driven. | L1 (world), extras (bg/fog/ambient), interact (raycast+click), custom_mesh |
| **pong** | `pong.kn` | Classic Pong vs AI. Left paddle W/S or Arrows, right paddle AI follows ball. Ball bounce with paddle angle deflection. Score tracking via telemetry. P=pause, R=reset. | L1 (world), L5 (pulse 20ms), L7 (std::input via interact), extras (bg/fog/emissive/lights/ambient), interact (new action API) |

---

## Building

### From the command line

```bash
# Build all demos
cd X:/blades/web/electron
kain build .

# Build a single demo
kain build demos/your_demo.kn --target llvm
```

Outputs land at:
```
.kain/out/demos/<name>.exe
```

### Via Bazel (full repo build)

```bash
bazel build //blades/web/electron/...
```

---

## Running

```bash
cd X:/blades/web/electron
npm start
```

The Electron app launches. The sidebar shows all discovered demos. Click any demo to switch — the current Kain process is killed and the new one spawned.

**Controls:**
- **Sidebar** — click a `.kn` file to build and run, or click the ✅ icon for prebuilt `.exe`
- **🔄 button** — rebuild a demo from source
- **Ctrl+O** — load any `.kn` file from disk
- **OrbitControls** — drag to orbit, scroll to zoom
- **Props tab** — live telemetry from Kain (`telemetry()`, `telemetry_float()`, `telemetry_string()`)

---

## File Structure

```
electron/
├── build.kn                 — Build DAG — compiles every demo
├── main.js                  — Electron main process — spawns Kain exe, IPC bridge
├── renderer.js              — SceneManager — interprets JSON, manages Three.js scene
├── index.html               — Electron window HTML (canvas container + sidebar + tabs)
├── package.json             — npm dependencies (electron, three)
├── package-lock.json        — dependency lockfile
├── README.md                — This file
├── electron.tsv             — Quick-reference TSV of all API functions
├── models/                  — GLB/GLTF model files for the rig system
│   └── README.md            — Sourcing and export guide
│
└── demos/
    ├── electron_bridge.kn   — Core render protocol bridge
239|    ├── electron_rig.kn      — 3D rig & animation bridge (GLB, bones, IK, SoA pose)
    ├── electron_extras.kn   — Visual effects layer (lights, fog, emissive, etc.)
    ├── electron_interact.kn — Bidirectional input bridge (keys, mouse, click, raycast)
    │
    ├── bridge.kn            — Entangle visualization demo
    ├── actor.kn             — Actor message-passing demo
    ├── converge.kn          — Converge multi-lane demo
    ├── pulse.kn             — Pulse clock demo
    ├── cosmic_forge.kn      — Cosmic forge visual spectacle
    ├── rig_mannequin.kn     — Procedural skeleton + gait + IK demo
    ├── blackhole.kn         — Kerr black hole simulation
    ├── engine_terrain.kn    — Procedural terrain with click deformation
    ├── cosmic_waves.kn      — 5-octave wave physics
    ├── cosmic_forge.kn      — Cosmic forge visual spectacle
    └── raycast.kn           — Terrain sculpting via raycast
```

---

## electron_rig.kn — 3D Rig & Animation Module

Full rig/animation control for the Electron playground. Supports GLB model loading,
procedural bone hierarchy creation, baked animation playback with crossfade, procedural
bone posing via SoA flat arrays (set_bone_pose), look-at targeting, and analytical
two-bone IK.

### Module Functions

| Function | Signature | Description |
|---|---|---|
| `load_glb` | `(id, url)` | Load a GLB/GLTF model with skeleton + animations |
| `unload_glb` | `(id)` | Remove model and free resources |
| `create_skeleton` | `(id, bones: JsonArray)` | Create a procedural skeleton from Kain (no GLB needed) |
| `play_animation` | `(id, name, loop, crossfade_sec)` | Play a named animation with smooth crossfade |
| `stop_animation` | `(id)` | Stop all animations |
| `pause_animation` | `(id, paused)` | Pause/resume the animation mixer |
| `set_anim_speed` | `(id, speed)` | Global playback speed (1.0=normal) |
| `set_anim_weight` | `(id, name, weight)` | Per-animation blend weight (0.0-1.0) |
| `set_bone_pose` | `(id, rx[], ry[], rz[], tx[], ty[], tz[])` | **HOT PATH:** Set ALL bone transforms via SoA flat Float arrays |
| `set_bone_transform` | `(id, bone_idx, rx, ry, rz, tx, ty, tz)` | Single-bone FK transform override |
| `look_at_bone` | `(id, bone, tx, ty, tz, up)` | Orient a bone toward a world-space point |
| `two_bone_ik` | `(id, root, mid, tip, tx, ty, tz, px, py, pz)` | Analytical 2-bone IK solver (solved on JS side) |

### SoA Hot Path (set_bone_pose)

Instead of sending N individual JSON objects (one per bone), `set_bone_pose` sends 6 flat
Float arrays — one lane per transform component. The JS side unpacks into `Float32Array`
and assigns directly:

```js
for (let i = 0; i < n; i++) {
  bones[i].position.set(tx[i], ty[i], tz[i]);
  bones[i].rotation.set(rx[i], ry[i], rz[i]);
}
```

This avoids per-bone JSON object overhead and is the recommended hot path for per-frame
procedural animation.

### GLB Models

Place `.glb` files in `models/` and load them from Kain:
```kain
load_glb("my_char", "models/rig1.glb")
// Wait for load, then:
play_animation("my_char", "idle", true, 0.3)
```

See `models/README.md` for sourcing and exporting GLB files.

### Demo: rig_mannequin

| Demo | File | Description | Layers |
|---|---|---|---|
| **rig_mannequin** | `rig_mannequin.kn` | Procedural 18-bone skeleton driven by sine-wave gait, arm swing, body bob. Head tracking via look_at_bone. IK foot planting. G=gait, T=head track, I=IK, 1/2/3=speed, C=clear. | L1 (world ready-gate), L5 (pulse 16ms), L7 (std::input via interact), electron_rig |

## See Also

- **`electron.tsv`** — Quick-reference TSV: every public function, signature, JS handler, description
- **`BRIDGE_ASSESSMENT.md`** — Full bridge assessment and coverage analysis
- **`electron_rig.kn`** — Rig & animation bridge module (GLB, procedural skeleton, IK, SoA flat arrays)
- **`models/README.md`** — GLB model sourcing and export guide
- **`build.kn`** — All demo build targets and dependency graph
- **`renderer.js`** — `SceneManager` class, all `_create/_update/_remove/_clear/_camera/_telemetry/_background/_fog/_autoRotate/_light/_ambient` handlers
