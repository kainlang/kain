# CLI Crate Reference

> **Last Updated:** 2026-02-20  
> **Purpose:** Complete command reference for the KAIN CLI  
> **Status:** Production-ready

---

## Table of Contents

1. [Overview](#overview)
2. [Commands](#commands)
3. [Global Flags](#global-flags)
4. [Compilation Targets](#compilation-targets)
5. [Examples](#examples)

---

## Overview

The KAIN CLI (`kain`) is the command-line interface for the KAIN compiler. It provides commands for project initialization, building, running, and injecting code into existing projects.

**Binary:** `kain` or `kain.exe`  
**Version:** Check with `kain --version`

---

## Commands

### `kain init`
Initialize a new KAIN project with KAIN.toml configuration.

**Usage:**
```bash
kain init [PATH] [OPTIONS]
```

**Arguments:**
- `[PATH]` - Project directory (default: current directory `.`)

**Options:**
- `--name <NAME>` - Explicit project name (default: directory name)

**Examples:**
```bash
# Initialize in current directory
kain init

# Initialize in new directory
kain init MyProject

# Initialize with explicit name
kain init MyProject --name "My Awesome Plugin"
```

**Generated Files:**
- `KAIN.toml` - Project configuration
- `src/` - Source directory
- `.gitignore` - Git ignore file

---

### `kain build`
Build project or file. Without input, reads KAIN.toml for multi-target build.

**Usage:**
```bash
kain build [INPUT] [OPTIONS]
```

**Arguments:**
- `[INPUT]` - Optional input file. If omitted, builds all targets from KAIN.toml

**Options:**
- `--targets <TARGETS>` - Override targets (comma-separated: wasm,js,rust)
- `--ue5` - Build UE5 plugin from KAIN.toml [ue5] config
- `-o, --output <OUTPUT>` - Output file
- `-v, --verbose` - Verbose output
- `--emit-ast` - Emit AST for debugging
- `--emit-typed` - Emit typed AST
- `--dry-run` - Print planned actions without executing
- `--strict` - Treat warnings as errors
- `--analyze` - Analyze shader complexity (USF target only)

**Examples:**
```bash
# Build entire project from KAIN.toml
kain build

# Build specific file
kain build src/main.kn

# Build UE5 plugin
kain build --ue5

# Build with multiple targets
kain build --targets wasm,js,rust

# Build with verbose output
kain build --ue5 --verbose

# Dry run (show what would be built)
kain build --ue5 --dry-run
```

**UE5 Plugin Build:**
When using `--ue5`, the CLI:
1. Reads `KAIN.toml` [ue5] section
2. Parses all `.kn` files in `src/`
3. Generates C++ code in `Source/`
4. Generates Blueprints in `Content/Blueprints/`
5. Generates Materials in `Content/Materials/`
6. Generates Shaders in `Shaders/`
7. Creates `.uplugin` and `.Build.cs` files

---

### `kain run`
Run a KAIN file immediately (interpret mode).

**Usage:**
```bash
kain run <INPUT>
```

**Arguments:**
- `<INPUT>` - Input file to run

**Examples:**
```bash
# Run a script
kain run examples/hello.kn

# Run with verbose output
kain run examples/test.kn --verbose
```

---

### `kain inject`
Inject KAIN file(s) into existing UE5 plugin (non-destructive).

**Usage:**
```bash
kain inject <INPUTS>... [OPTIONS]
```

**Arguments:**
- `<INPUTS>...` - Input .kn file(s) to inject

**Options:**
- `--plugin-dir <DIR>` - Target plugin directory (auto-detected if omitted)
- `--plugin <NAME>` - Plugin name (auto-detected if omitted)
- `--force` - Force overwrite existing files
- `--dry-run` - Show what would be generated without writing
- `--ue5` - Use UE5 codegen (required)

**Examples:**
```bash
# Inject into auto-detected plugin
kain inject src/new_actor.kn --ue5

# Inject multiple files
kain inject src/actor1.kn src/actor2.kn --ue5

# Inject with explicit plugin directory
kain inject src/new_actor.kn --ue5 --plugin-dir /path/to/MyPlugin

# Inject with explicit plugin name
kain inject src/new_actor.kn --ue5 --plugin MyPlugin

# Dry run (preview changes)
kain inject src/new_actor.kn --ue5 --dry-run

# Force overwrite existing files
kain inject src/new_actor.kn --ue5 --force
```

**How It Works:**
1. Detects existing plugin structure
2. Parses input `.kn` files
3. Generates new C++ files in `Source/Private/Generated/`
4. Updates existing headers if needed
5. Preserves existing code (non-destructive)
6. Registers new types in module

---

### `kain lsp`
Start the KAIN Language Server for IDE integration.

**Usage:**
```bash
kain lsp
```

**Purpose:**
- Provides IDE features (autocomplete, diagnostics, hover, etc.)
- Used by VS Code extension
- Communicates via JSON-RPC over stdin/stdout

**Note:** This command is typically invoked by your IDE, not manually.

---

## Global Flags

These flags work with most commands:

### `-o, --output <OUTPUT>`
Specify output file or directory.

```bash
kain build src/main.kn -o dist/output.wasm
```

### `-t, --target <TARGET>`
Compilation target (legacy, use `build --targets` instead).

**Available targets:**
- `wasm` / `w` - WebAssembly
- `js` / `javascript` - JavaScript
- `rust` / `rs` - Rust
- `llvm` / `native` / `n` - Native LLVM
- `spirv` / `gpu` / `shader` / `s` - SPIR-V shader
- `hlsl` / `h` - HLSL shader
- `usf` - Unreal Shader Format
- `cpp` / `c++` - C++
- `ue5` / `unreal` - UE5 C++
- `ue5editor` / `editor` / `slate` - UE5 Editor UI
- `run` / `r` / `interpret` / `i` - Interpret
- `test` / `t` - Test mode
- `hybrid` / `web` - Hybrid (WASM + JS)

```bash
kain src/main.kn --target wasm
kain src/shader.kn --target hlsl
```

### `-r, --run`
Run immediately after compilation.

```bash
kain build src/main.kn --target wasm --run
```

### `-w, --watch`
Watch for file changes and recompile automatically.

```bash
kain build src/main.kn --watch
```

### `-v, --verbose`
Enable verbose output (shows detailed compilation steps).

```bash
kain build --ue5 --verbose
```

### `--emit-ast`
Emit AST (Abstract Syntax Tree) for debugging.

```bash
kain build src/main.kn --emit-ast
```

### `--emit-typed`
Emit typed AST after type checking.

```bash
kain build src/main.kn --emit-typed
```

### `--dry-run`
Print planned actions without executing.

```bash
kain build --ue5 --dry-run
kain inject src/actor.kn --ue5 --dry-run
```

### `--strict`
Treat transpiler warnings as errors.

```bash
kain build --ue5 --strict
```

### `--analyze`
Analyze shader complexity (USF target only).

```bash
kain build src/shader.kn --target usf --analyze
```

### `--plugin <NAME>`
Target plugin name for UE5 operations.

```bash
kain build --ue5 --plugin MyPlugin
```

### `--plugins-dir <DIR>`
Base plugins directory (defaults to `u:\ue_factory\src-plugins`).

```bash
kain build --ue5 --plugins-dir /path/to/plugins
```

---

## Compilation Targets

### WebAssembly (`wasm`)
Compile to WebAssembly for web browsers.

```bash
kain build src/main.kn --target wasm
```

**Output:** `.wasm` file

---

### JavaScript (`js`)
Compile to JavaScript for Node.js or browsers.

```bash
kain build src/main.kn --target js
```

**Output:** `.js` file

---

### Rust (`rust`)
Transpile to Rust source code.

```bash
kain build src/main.kn --target rust
```

**Output:** `.rs` file

---

### LLVM (`llvm`)
Compile to native code via LLVM.

```bash
kain build src/main.kn --target llvm
```

**Output:** Native executable

---

### HLSL (`hlsl`)
Compile to HLSL shader code.

```bash
kain build src/shader.kn --target hlsl
```

**Output:** `.hlsl` file

---

### USF (`usf`)
Compile to Unreal Shader Format.

```bash
kain build src/shader.kn --target usf
```

**Output:** `.usf` file

---

### UE5 (`ue5`)
Generate UE5 C++ plugin code.

```bash
kain build --ue5
```

**Output:** Complete UE5 plugin with:
- `Source/` - C++ code
- `Content/` - Assets
- `Shaders/` - Shader files
- `.uplugin` - Plugin descriptor
- `.Build.cs` - Build configuration

---

### UE5 Editor (`ue5editor`)
Generate UE5 editor UI code (Slate, Details, Viewports).

```bash
kain build src/editor.kn --target ue5editor
```

**Output:** Editor UI C++ code

---

### Interpret (`run`)
Run immediately without compilation.

```bash
kain run src/main.kn
```

**Output:** Execution output

---

## Examples

### Example 1: Create New UE5 Plugin

```bash
# Initialize project
kain init MyPlugin --name "My Awesome Plugin"

# Edit KAIN.toml to configure UE5 settings
# Add your .kn files to src/

# Build plugin
cd MyPlugin
kain build --ue5

# Output: MyPlugin/ with complete UE5 plugin structure
```

---

### Example 2: Inject New Actor into Existing Plugin

```bash
# Create new actor file
cat > src/new_enemy.kn << 'EOF'
actor Enemy:
    state health: Float = 100.0
    state damage: Float = 10.0
    
    on BeginPlay():
        println("Enemy spawned!")
    
    on Tick(delta_time: Float):
        // AI logic here
EOF

# Inject into existing plugin
kain inject src/new_enemy.kn --ue5 --plugin MyPlugin

# Output: New C++ files in MyPlugin/Source/Private/Generated/
```

---

### Example 3: Multi-Target Build

```bash
# Build for multiple targets at once
kain build --targets wasm,js,rust

# Output:
# - dist/output.wasm
# - dist/output.js
# - dist/output.rs
```

---

### Example 4: Watch Mode Development

```bash
# Start watch mode
kain build src/main.kn --target wasm --watch

# Now edit src/main.kn
# CLI automatically rebuilds on save
```

---

### Example 5: Shader Development

```bash
# Compile shader to HLSL
kain build src/particle.kn --target hlsl -o shaders/particle.hlsl

# Compile shader to USF with analysis
kain build src/particle.kn --target usf --analyze

# Output: Complexity analysis + .usf file
```

---

## Environment Variables

### `KAIN_RUNTIME_C_PATH`
Path to runtime C library.

```bash
export KAIN_RUNTIME_C_PATH=/path/to/runtime.c
kain build src/main.kn --target llvm
```

---

## Exit Codes

- `0` - Success
- `1` - Compilation error
- `1` - Command failed

---

## Configuration File (KAIN.toml)

The CLI reads `KAIN.toml` for project configuration:

```toml
[package]
name = "MyPlugin"
version = "1.0.0"
authors = ["Your Name"]

[ue5]
plugin_name = "MyPlugin"
engine_version = "5.3"
category = "Gameplay"
description = "My awesome UE5 plugin"

[[ue5.modules]]
name = "MyPlugin"
type = "Runtime"
loading_phase = "Default"

[build]
targets = ["wasm", "js"]
output_dir = "dist"
```

---

## Summary

The KAIN CLI provides 5 main commands:
1. `init` - Initialize new project
2. `build` - Build project or file
3. `run` - Run file immediately
4. `inject` - Inject into existing plugin
5. `lsp` - Start language server

With 15+ compilation targets and comprehensive flags for debugging, analysis, and customization.
