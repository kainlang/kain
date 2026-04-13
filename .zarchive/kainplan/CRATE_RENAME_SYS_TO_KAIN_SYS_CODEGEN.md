# Crate Rename: sys → kain-sys-codegen

## Date: March 7, 2026

## Overview

Renamed the `sys` crate to `kain-sys-codegen` for better clarity and consistency with other KAIN crates.

## Rationale

**Old name:** `sys` (too vague, unclear purpose)
**New name:** `kain-sys-codegen` (clear purpose: systems programming code generation)

The crate generates code for native/compiled targets:
- LLVM IR
- Rust
- C++

## Changes Made

### 1. Folder Rename
```
Kain/crates/sys → Kain/crates/kain-sys-codegen
```

### 2. Crate Cargo.toml
**File:** `Kain/crates/kain-sys-codegen/Cargo.toml`
```toml
[package]
name = "kain-sys-codegen"  # Changed from "sys"
version = "0.1.0"
edition = "2021"
```

### 3. CLI Cargo.toml
**File:** `Kain/crates/cli/Cargo.toml`

**Dependencies:**
```toml
kain-sys-codegen = { path = "../kain-sys-codegen", optional = true }  # Changed from sys
```

**Features:**
```toml
sys = ["dep:kain-sys-codegen"]  # Changed from ["dep:sys"]
```

### 4. Workspace Cargo.toml
**File:** `Kain/Cargo.toml`
```toml
members = [
    "crates/kain-sys-codegen",  # Changed from "crates/sys"
    # ... other members
]
```

### 5. CLI lib.rs Import Alias
**File:** `Kain/crates/cli/src/lib.rs`
```rust
#[cfg(feature = "sys")]
use kain_sys_codegen as sys;  // Added alias to keep existing code working
```

This alias ensures all existing `sys::generate_*()` calls continue to work without modification.

## Verification Results

✅ **kain-sys-codegen crate build:** SUCCESS (5.38s)
✅ **Full workspace build:** SUCCESS (30.84s)
✅ **Rust codegen test:** SUCCESS (4488 bytes generated)
✅ **C++ codegen test:** SUCCESS (4682 bytes generated)

## Backward Compatibility

The rename is 100% backward compatible:
- Feature flag remains `sys` (no breaking change)
- Public API unchanged via import alias: `use kain_sys_codegen as sys;`
- All existing `sys::generate_*()` calls work identically

## Naming Convention

The crate now follows KAIN's naming convention:
- `kain-core` - Core compiler (AST, parser, type checker)
- `kain-asm` - Assembly import
- `kain-import` - C/Rust import
- `kain-sys-codegen` - Systems programming code generation ✨ NEW
- `ue5` - UE5 runtime codegen
- `ue5-editor` - UE5 editor codegen
- `ue5-shaders` - UE5 shader codegen
- `ue5-materials` - UE5 material codegen
- `ue5-blueprints` - UE5 blueprint codegen
- `ue5-graphs` - UE5 graph codegen
- `gpu` - GPU codegen (SPIR-V, HLSL)
- `web` - Web codegen (WASM, JS, TS)

## Structure

```
Kain/crates/kain-sys-codegen/
├── Cargo.toml
└── src/
    ├── lib.rs
    ├── codegen_llvm/
    │   └── mod.rs
    ├── codegen_rust/
    │   └── mod.rs
    └── codegen_cpp/
        └── mod.rs
```

Each backend is now in its own folder, ready for expansion with additional modules.

## Related Documentation

- Sys Crate Refactoring: `Kain/docs/SYS_CRATE_REFACTORING.md`
- Automatic Directory Creation: `Kain/docs/AUTOMATIC_DIRECTORY_CREATION.md`
