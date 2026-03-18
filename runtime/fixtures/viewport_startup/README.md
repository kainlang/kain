# Native Viewport Startup Smoke Fixture

**Purpose:** Validates native viewport host startup with Win32 platform services

**Requirements:** 4.2, 8.1, 13.4, 13.5

**Platform:** Windows only (Win32 app host, OpenGL)

---

## What This Tests

This fixture validates the native viewport startup path:

1. **Win32 app host initialization** - Can the runtime initialize Win32 window?
2. **Win32 input host initialization** - Can the runtime initialize Win32 input capture?
3. **OpenGL context creation** - Can the runtime create an OpenGL context?
4. **Viewport scene loading** - Can the runtime load viewport scene metadata?
5. **Platform service availability** - Are Win32 platform services available?
6. **Startup diagnostics** - Does the runtime emit diagnostics on success?

---

## Artifacts

- `main.kn` - Minimal viewport program
- `kain_runtime_contract.json` - Contract with viewport requirements
- `kain_realtime_app_bundle.json` - Realtime bundle with viewport scene
- `README.md` - This file

---

## Usage

### Compile

```bash
cd runtime/fixtures/viewport_startup
kain build main.kn --target rust
```

### Run (Windows only)

```bash
# If a run script is generated
./run.ps1
```

### Expected Behavior

- Compilation succeeds
- Runtime contract is loaded successfully
- Realtime bundle is loaded successfully
- Win32 window opens
- OpenGL context is created
- Viewport renders (empty scene)
- No startup errors or warnings

### Failure Modes

If this fixture fails, it indicates:

- Win32 app host initialization is broken
- Win32 input host initialization is broken
- OpenGL context creation is broken
- Viewport scene loading is broken
- Platform service bindings are broken

---

## Platform Requirements

**Windows:**
- Win32 API available
- OpenGL drivers available
- Display/graphics hardware available

**Linux/macOS:**
- This fixture will fail gracefully with platform capability diagnostics
- Future phases will add Linux/macOS platform adapters

---

## Extension Points

Later phases may extend this fixture with:

- **Phase 3:** Reflection-driven scene metadata
- **Phase 8:** UI/viewport event routing, focus handling
- **Phase 9:** Actual shader/material artifacts, compute dispatch
- **Phase 12:** Linux and macOS platform adapters

When extending, preserve the minimal baseline and document changes here.

---

## Notes

- Scene has no actual geometry, materials, or shaders
- Focus is purely on platform initialization and viewport startup
- Windows-only for now (Win32 + OpenGL)
- This fixture tests:
  - `runtime/native/src/platform/win32/kain_win32_app_host.c`
  - `runtime/native/src/platform/win32/kain_win32_input_host.c`
  - `runtime/native/src/platform/win32/kain_runtime_viewport_win32.c`
  - `runtime/native/src/gfx/opengl/kain_gl_win32_host.c`
- For Rust-native target, tests `kain-ui-native` and `kain-3D` crates
