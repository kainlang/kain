# Kaintana Backend Inventory

## Available Backends

| Backend          | Type      | Est. Lines | Platform     | Status |
|------------------|-----------|-----------|--------------|--------|
| null             | Host      | ~100      | All          | Stub   |
| win32 (GDI)      | Host+SW   | ~800      | Windows      | Stub   |
| win32 (render)   | Software  | ~400      | Windows      | Stub   |
| x11              | Host      | ~600      | Linux        | Stub   |
| wayland          | Host      | ~700      | Linux        | Stub   |
| macos            | Host      | ~600      | macOS        | Stub   |
| terminal         | Host+TUI  | ~300      | All          | Stub   |
| wasm             | Host      | ~400      | Browser      | Stub   |
| vulkan           | GPU       | ~2000     | Windows/Linux | Stub   |
| d3d12            | GPU       | ~1500     | Windows      | Stub   |
| webgpu           | GPU       | ~1200     | All          | Stub   |

## Selection Logic (Priority Order)

The active backend is chosen by a 4-layer stack:

### L0: Compile-time — only link what you need
The build system (CMake/Bazel) decides which backends enter the binary.
- Release for Windows: `host_win32.c` + optionally `render_vulkan.c` or `render_d3d12.c`
- CI / testing: `host_null.c` only
- Backends not linked CANNOT be selected.

### L1: Code-time — application chooses (PRIMARY)
```c
kt_backend_register(session, "win32", &kaintana_win32_backend);
int ok = kt_backend_select(session, "vulkan");
if (!ok) ok = kt_backend_select(session, "win32");
```

### L2: Platform default (if application didn't call kt_backend_select)
- Windows    → `win32` (GDI software)
- Linux      → `x11` or `wayland` (wayland preferred if compositor supports it)
- macOS      → `macos` (with vulkan via MoltenVK as GPU upgrade)
- WASM       → `wasm`
- Unknown    → `null` (headless safe default)

### L3: Env var override (DEBUG/CI ONLY — not for production)
```bash
RENDERER_BACKEND=vulkan ./myapp      # GPU path for testing
RENDERER_BACKEND=null   ./myapp      # Headless for CI
```

The `RENDERER_BACKEND` env var is only checked INSIDE `kt_backend_probe()`.
If the application calls `kt_backend_select()` explicitly, the env var is IGNORED.
Production code MUST use `kt_backend_select()`, not `kt_backend_probe()`.

### Contract
```c
// Every backend implements exactly this:
typedef struct KaintanaBackendVTable {
    const char* name;
    int  (*init)(const KaintanaBackendConfig* config);
    void (*shutdown)(void);
    int  (*new_frame)(KaintanaInput* input);
    void (*present)(const KaintanaDrawData* draw_data);
} KaintanaBackendVTable;
```
