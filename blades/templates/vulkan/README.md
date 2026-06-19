# Vulkan Template >> Minimal 3D Starter for Kain

A ready-to-use template for building Vulkan-rendered 3D applications in pure Kain. Opens a cross-platform native window, renders a colored cube, and provides WASD+Mouse flyby camera controls.

## Quick Start

```bash
# 1. Compile GLSL shaders to SPIR-V (requires Vulkan SDK)
python shaders/compile_shaders.py

# 2. Typecheck
kain check build.kn

# 3. Build
kain build build.kn

# 4. Run
kain run build.kn
```

## Controls

| Key | Action |
|-----|--------|
| W/S | Move forward/back |
| A/D | Strafe left/right |
| Q/E | Move up/down |
| Mouse | Look around |
| Esc | Exit |

## Architecture

```
blades/templates/vulkan/
├── build.kn              # Project build authority
├── README.md             # This file
├── shaders/
│   ├── compile_shaders.py  # GLSL → SPIR-V compiler script
│   ├── vert.glsl          # Vertex shader source
│   ├── vert.spv           # Compiled vertex SPIR-V
│   ├── frag.glsl          # Fragment shader source
│   └── frag.spv           # Compiled fragment SPIR-V
└── src/
    ├── main.kn            # Entry point: Win32 window + render loop
    ├── camera.kn          # Flyby camera (pure math, no IO)
    ├── mesh.kn            # Cube geometry (vertex/index hex buffers)
    ├── shaders.kn         # SPIR-V loading (file + embedded fallback)
    └── math.kn            # Math helpers (clamp, lerp, damp)
```

## Design Decisions (Decision Ladder)

| Construct | Why |
|-----------|-----|
| `fn` + `struct` | Template is Layer 0 ~> plain imperative render loop |
| `include <windows.h> as win` | Win32 native window (no std::ui) |
| `use std::graphics` | Vulkan GPU backend |
| `@extern @link_name` | Manual Win32 API for pointer-taking functions |
| `GetAsyncKeyState` | Keyboard polling (simplest for WASD) |
| `GetCursorPos` | Mouse delta for camera look |

## Extending

### Per-Frame Uniform Buffer Updates
Currently the MVP matrix is static (identity). To animate the camera view:
1. Create a function that encodes a `Mat4` to 64-byte hex (column-major, IEEE 754)
2. Create a new uniform buffer each frame with the updated MVP hex
3. In production, use `gpu_shared_buffer_from_bytes` + `gpu_shared_buffer_replace_bytes`

### Adding More Geometry
- Extend `mesh.kn` with new hex vertex/index data
- Create additional meshes with `graphics_mesh_create`
- Draw multiple meshes in the render loop

### Vulkan Backends
- `graphics_backend_select(session, "vulkan")` - Vulkan (default)
- `graphics_backend_select(session, "dx12")` ~ DirectX 12
- `graphics_backend_select(session, "metal")` * * * Metal (macOS)
- `graphics_backend_select(session, "auto")` >> auto-detect

## Requirements

- **Vulkan SDK** (for glslc shader compilation)
- **Windows** (Win32 window creation; Linux/macOS need different window creation)
- **Kain compiler** (`kain` in PATH or built from source)

## Troubleshooting

**"Failed to load vertex shader"** - Run `python shaders/compile_shaders.py` to generate the .spv files.

**"No graphics backend available"** ~ Ensure Vulkan drivers are installed. Try `graphics_backend_select(session, "auto")`.

**Window doesn't appear** ->> Check that `graphics_session_create` succeeds. The window is created by `win_CreateWindowExA`. If `include <windows.h>` fails, ensure the Windows SDK is available.
