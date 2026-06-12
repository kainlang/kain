# Kain DX12 Starter Template

A ready-to-use DirectX 12 3D starter template for Kain — open a window, render a colored cube, fly around with WASD+mouse controls.

**Zero dependencies** — pure Kain `std::graphics` with the DX12 backend, plus Win32 input polling.

## Quick Start

```bash
# Check syntax
kain check blades/templates/dx12/build.kn

# Build to native executable (LLVM)
kain build blades/templates/dx12/build.kn -t llvm

# Run
kain run blades/templates/dx12/build.kn
```

## Controls

| Key | Action |
|-----|--------|
| `W` / `S` | Move forward / backward |
| `A` / `D` | Strafe left / right |
| `Q` / `E` | Move down / up |
| `Shift` | Speed boost (×3) |
| Mouse | Look around (yaw + pitch) |
| `Escape` | Exit |

## Architecture

```
blades/templates/dx12/
├── build.kn              # Project build authority
├── README.md
├── shaders/
│   ├── vertex.hlsl       # HLSL source — vertex shader
│   └── fragment.hlsl     # HLSL source — fragment shader
├── tools/
│   └── generate_shaders.py  # Python script to regenerate SPIR-V hex
└── src/
    ├── main.kn           # Entry point + render loop + input
    ├── camera.kn         # Flyby camera (WASD + mouse look)
    ├── mesh.kn           # Cube geometry (vertices + indices)
    ├── shaders.kn        # Embedded SPIR-V shaders (hex)
    └── math.kn           # 3D math helpers (thin std::math wrapper)
```

## Module Overview

### `src/main.kn` — Entry Point & Render Loop

Creates the DX12 graphics session, loads shaders/buffers/mesh/pipeline, and runs the main loop:

```
while running:
    poll keyboard (GetAsyncKeyState)
    poll mouse (GetCursorPos → SetCursorPos)
    camera_update()
    graphics_begin_frame()
    graphics_draw_mesh()
    graphics_end_frame()
    graphics_present()
```

### `src/camera.kn` — Flyby Camera

- **Struct `FlybyCamera`**: position, yaw, pitch, speed, sensitivity, FOV
- **Struct `CameraInput`**: per-frame movement + mouse deltas
- **`camera_update(cam, input) → FlybyCamera`**: immutable update returning new state
- **`camera_view_projection(cam, aspect) → Mat4`**: combined view×projection matrix

### `src/mesh.kn` — Cube Geometry

Precomputed hex strings for a unit cube with per-face colors:

| Face | Color |
|------|-------|
| +X | Red |
| -X | Cyan |
| +Y | Green |
| -Y | Magenta |
| +Z | Blue |
| -Z | Yellow |

Vertex format: `position(3×f32) + color(3×f32)` = 24 bytes per vertex, 24 vertices, 36 indices.

### `src/shaders.kn` — SPIR-V Shaders

Minimal passthrough shaders embedded as hex:

- **Vertex**: passes `position` → `SV_POSITION`, `color` → output
- **Fragment**: outputs `float4(color, 1.0)`

> **To upgrade**: compile the HLSL sources in `shaders/` with `dxc -spirv`, then update the hex strings.

### `src/math.kn` — Math Helpers

Thin convenience layer over `std::math`:
- `clamp_float()`, `forward_from_yaw_pitch()`, `right_from_yaw()`, `build_mvp()`

## Shader Workflow

The template ships with SPIR-V shaders compiled offline. To modify them:

1. Edit `shaders/vertex.hlsl` or `shaders/fragment.hlsl`
2. Compile with DirectX Shader Compiler:
   ```bash
   dxc -T vs_6_0 -E main -spirv shaders/vertex.hlsl -Fo vertex.spv
   dxc -T ps_6_0 -E main -spirv shaders/fragment.hlsl -Fo fragment.spv
   ```
3. Convert to hex:
   ```bash
   xxd -p vertex.spv | tr -d '\n' > vertex.hex
   xxd -p fragment.spv | tr -d '\n' > fragment.hex
   ```
4. Update the hex strings in `src/shaders.kn`

Or regenerate with the bundled script:
```bash
python tools/generate_shaders.py
```

## Decision Ladder Used

All modules use **Layer 0** (plain `fn` + `struct` + `Pure`/`Unsafe` effects):

| Module | Construct | Why |
|--------|-----------|-----|
| `main.kn` | `fn` with `Unsafe` | Imperative game loop with raw Win32 input |
| `camera.kn` | `struct` + `fn` with `Pure` | Stateless update returning new camera — no mutation needed |
| `mesh.kn` | `fn` with `Pure` | Deterministic hex generation from constants |
| `shaders.kn` | `fn` with `Pure` | Constant hex strings |
| `math.kn` | `fn` with `Pure` | Pure math operations |

> For larger projects, consider upgrading to `world` for app state, `pulse` for frame timing, and `resonate` for reactive state changes.

## APIs Used

| API | Module | Purpose |
|-----|--------|---------|
| `graphics_session_create` | `std::graphics` | Create DX12 rendering context + window |
| `graphics_backend_select` | `std::graphics` | Select `"dx12"` backend |
| `graphics_buffer_create_from_hex` | `std::graphics` | Upload vertex/index hex data |
| `graphics_shader_spirv_from_hex` | `std::graphics` | Load SPIR-V shaders |
| `graphics_mesh_create` | `std::graphics` | Bind vertex + index buffers |
| `graphics_pipeline_create` | `std::graphics` | Create render pipeline |
| `graphics_draw_mesh` | `std::graphics` | Submit draw call |
| `graphics_begin/end_frame` | `std::graphics` | Frame lifecycle |
| `graphics_present` | `std::graphics` | Present to swapchain |
| `GetAsyncKeyState` | Win32 | Keyboard polling |
| `GetCursorPos` / `SetCursorPos` | Win32 | Mouse look |
| `ShowCursor` | Win32 | Hide/show cursor |

## Next Steps

This template gives you a working 3D window. From here, extend it with:

- **MVP uniform buffer**: Add a `Mat4` uniform to the vertex shader and upload the camera's view-projection matrix each frame
- **Custom geometry**: Replace the cube hex strings with your own mesh data
- **Textures**: Use `std::gpu` for texture loading and binding
- **More shaders**: Add lighting, PBR, post-processing
- **Physics**: Add a physics engine or simple collision detection
- **UI overlay**: Use `std::ui` in hybrid mode for HUD elements
