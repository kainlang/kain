# UE5 Integration Guide

Complete guide to building, installing, and using KAIN with Unreal Engine 5.

---

## Quick Start

### Installation

**Option 1: Fast Build (Recommended)**
```bash
cb              # Build release + auto-install
```

**Option 2: Standard Build**
```bash
.\build.ps1     # PowerShell script
# or
build.bat       # Batch file
```

**Option 3: Manual**
```bash
cargo build --release
copy target\release\kain.exe %USERPROFILE%\.cargo\bin\
```

### First Plugin Build

```bash
# 1. Create plugin directory
mkdir MyPlugin
cd MyPlugin

# 2. Create KAIN.toml
cat > KAIN.toml << EOF
[package]
name = "MyPlugin"
version = "1.0.0"

[ue5]
plugin_name = "MyPlugin"
plugin_dir = "."
shaders = []
EOF

# 3. Write KAIN code
cat > shaders.kn << EOF
shader fragment Tint(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
EOF

# 4. Build
kain build --ue5
```

### Common Commands

| Command | Description |
|---------|-------------|
| `kain build --ue5` | Build UE5 plugin from KAIN.toml |
| `kain file.kn -t ue5` | Compile single file to UE5 C++ |
| `kain file.kn -t usf` | Compile shaders to USF |
| `kain --version` | Check installed version |
| `cb` | Rebuild and reinstall kain |

---

## Build System

### KAIN.toml Configuration

Complete configuration reference:

```toml
[package]
name = "MyPlugin"              # Plugin name (required)
version = "1.0.0"              # Semantic version
description = "My plugin"      # Optional description
authors = ["Your Name"]        # Optional authors

[build]
entry = "main.kn"              # Entry file for compilation
output = "dist"                # Output directory (non-UE5)
targets = ["wasm", "js"]       # Multi-target builds

[ue5]
plugin_name = "MyPlugin"       # REQUIRED: Plugin name for shader paths
plugin_dir = "."               # Plugin output directory
shaders = []                   # Shader names (empty = auto-detect)
sources = [                    # Multi-file compilation (optional)
    "types.kn",
    "actors.kn",
    "shaders.kn"
]
```

### Multi-File Compilation

For complex plugins with multiple source files:

```toml
[ue5]
plugin_name = "GameplaySystem"
plugin_dir = "."
sources = [
    "types.kn",        # Enums, structs, delegates
    "components.kn",   # Components
    "actors.kn",       # Actors
    "utilities.kn",    # Blueprint functions
    "shaders.kn"       # Shaders
]
```

**Build:**
```bash
kain build --ue5
```

**Result:** Complete plugin with all files compiled and organized.

### Per-Item Output

Generate separate files for each actor/component:

```bash
kain build --ue5 --per-item
```

**Output:**
```
MyPlugin/
├── Source/
│   ├── Public/
│   │   ├── APlayerActor.h
│   │   ├── AEnemyActor.h
│   │   └── UHealthComponent.h
│   └── Private/
│       ├── APlayerActor.cpp
│       ├── AEnemyActor.cpp
│       └── UHealthComponent.cpp
```

---

## UE5 Codegen Pipeline

### Overview

```
.kn source → Lexer → Parser → Type Checker → UE5 Codegen → .h + .cpp files
```

### Phase 1: Parsing

KAIN parses your code into an Abstract Syntax Tree (AST):
- Actors, components, structs, enums
- Shader definitions
- Functions and methods
- Type annotations

### Phase 2: Type Checking

Validates:
- Type correctness
- Uniform binding uniqueness
- Shader stage compatibility
- RPC naming conventions

### Phase 3: Code Generation

Generates production-ready UE5 C++:
- `UCLASS()`, `USTRUCT()`, `UENUM()` macros
- `UPROPERTY()` with correct specifiers
- `UFUNCTION()` with networking support
- Shader registration and dispatch code

### Generated Structure

```
MyPlugin/
├── Source/
│   ├── Public/
│   │   ├── MyPlugin.h          # Main header
│   │   └── MyShader.h          # Shader bindings
│   ├── Private/
│   │   ├── MyPlugin.cpp        # Implementation
│   │   └── MyShader.cpp        # Shader registration
│   └── MyPlugin.Build.cs       # Build configuration
├── Shaders/
│   └── MyShader.usf            # Shader code
└── MyPlugin.uplugin            # Plugin manifest
```

---

## Shader Compilation

### Shader Types

**Pixel Shaders (Fragment):**
```kain
shader fragment MyEffect(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
```

**Compute Shaders:**
```kain
shader compute MyCompute(thread_id: Vec3):
    uniform output: RWTexture2D @0
    output[thread_id.xy] = vec4(1, 0, 0, 1)
```

### Shader Path Resolution

KAIN automatically generates correct shader paths:

```cpp
IMPLEMENT_GLOBAL_SHADER(FMyShader, 
    "/Plugin/MyPluginName/Shaders/MyShader.usf",  // Auto-generated
    "MyShaderPS", 
    SF_Pixel);
```

**Critical:** `plugin_name` in KAIN.toml must match your plugin folder name.

### Permutations (Compile-Time Branching)

Use `CFG_` or `ENABLE_` prefix for zero-cost quality levels:

```kain
shader fragment Optimized(uv: Vec2) -> Vec4:
    uniform CFG_HIGH_QUALITY: Float @0
    uniform ENABLE_SHADOWS: Float @1
    uniform color: Vec3 @2
    
    var result = color
    
    if CFG_HIGH_QUALITY:
        result = result * 1.5  # Expensive path
    
    if ENABLE_SHADOWS:
        result = result * 0.8  # Shadow calculation
    
    return vec4(result, 1.0)
```

**Generated:** Multiple shader variants, zero runtime cost.

---

## Integration with UE5

### Step 1: Build Plugin

```bash
cd Plugins/MyPlugin
kain build --ue5
```

### Step 2: Copy to UE5 Project

If `plugin_dir = "."`, plugin is already in place!

Otherwise:
```bash
cp -r MyPlugin/ /path/to/UE5Project/Plugins/
```

### Step 3: Regenerate Project Files

Right-click `.uproject` → "Generate Visual Studio project files"

### Step 4: Compile in UE5

Open solution in Visual Studio → Build (Ctrl+Shift+B)

### Step 5: Use in Blueprints

All actors, components, and utilities are Blueprint-accessible automatically.

---

## Development Workflow

### Iterative Development

**Terminal 1: Watch Mode**
```bash
kain shaders.kn -t usf -o Output --plugin MyPlugin -w
```

**Terminal 2: Edit Code**
```bash
vim shaders.kn  # Auto-recompiles on save
```

**Terminal 3: Hot Reload**
```bash
# Copy to UE5 (if editor is open, hot reload works)
cp Output/*.usf /path/to/UE5/Plugins/MyPlugin/Shaders/
```

### Multi-Plugin Project

```
MyProject/
├── PluginA/
│   ├── KAIN.toml
│   └── code.kn
├── PluginB/
│   ├── KAIN.toml
│   └── code.kn
└── build_all.sh
```

**build_all.sh:**
```bash
#!/bin/bash
cd PluginA && kain build --ue5
cd ../PluginB && kain build --ue5
echo "All plugins built!"
```

---

## Troubleshooting

### Issue: "No [ue5] section in KAIN.toml"

**Solution:** Add `[ue5]` section:
```toml
[ue5]
plugin_name = "MyPlugin"
plugin_dir = "."
```

### Issue: "Entry file not found"

**Solution:** Ensure `[build] entry` points to existing file:
```toml
[build]
entry = "shaders.kn"  # Must exist
```

### Issue: Shader path mismatch

**Symptom:** "Failed to find shader" crash

**Solution:** Verify `plugin_name` matches folder:
```toml
[ue5]
plugin_name = "MyPlugin"  # Must match Plugins/MyPlugin/
```

### Issue: Build fails after code changes

**Solution:** Rebuild kain:
```bash
cb              # Fast rebuild + reinstall
kain build --ue5
```

---

## Performance Tips

1. **Use permutations** for quality levels (zero runtime cost)
2. **Batch compilation** with `shaders = []` (auto-detect)
3. **Watch mode** for rapid iteration
4. **Per-item output** for large plugins (faster UE5 compilation)

---

## Best Practices

1. **One KAIN.toml per plugin** - Clear configuration
2. **Descriptive names** - `AtmosphericScattering` not `Shader1`
3. **Version control `.kn` files** - Not generated C++
4. **Test in UE5 immediately** - Catch issues early
5. **Use multi-file compilation** - Better organization

---

## Type Mappings

### KAIN → UE5 C++

| KAIN | UE5 C++ | USF/HLSL |
|------|---------|----------|
| `Int` | `int64` | `int` |
| `Float` | `float` | `float` |
| `Bool` | `bool` | `bool` |
| `String` | `FString` | N/A |
| `Vec2` | `FVector2D` | `float2` |
| `Vec3` | `FVector` | `float3` |
| `Vec4` | `FVector4` | `float4` |
| `Array<T>` | `TArray<T>` | N/A |
| `Texture2D` | `FRDGTexture*` | `Texture2D` |
| `RWTexture2D` | `FRDGTextureUAV*` | `RWTexture2D<float4>` |

---

## Summary

**Before KAIN:**
- 80-120 hours per plugin
- Manual file copying
- Shader path errors
- Type mismatches

**With KAIN:**
- 7.5-18 hours per plugin
- One command: `kain build --ue5`
- Automatic path resolution
- Type-safe compilation

**The KAIN advantage: 10-20x faster plugin development.**

---

## Next Steps

- Read [UE5_GODMODE_GUIDE.md](./UE5_GODMODE_GUIDE.md) for advanced shader techniques
- Check [UE5_EDITOR.md](./UE5_EDITOR.md) for editor tools
- Explore [STDLIB.md](./STDLIB.md) for standard library functions
