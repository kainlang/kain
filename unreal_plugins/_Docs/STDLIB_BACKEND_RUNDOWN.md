# KAIN Stdlib — Full Backend Rundown for Agents
**Date:** February 22, 2026  
**Scope:** Complete map of how the stdlib system works, where it's broken, and exactly what to implement to restore full data-driven `.kn` stdlib injection.

---

## The Two Stdlib Systems (They Are Separate)

There are **two completely separate stdlib mechanisms** in the codebase. Understanding both is critical.

### System 1 — `kain-core/src/stdlib.rs` (Hardcoded Rust Registry)
A `HashMap<String, BuiltinFn>` of ~50 built-in function signatures registered in Rust code. This is the **type-checker's knowledge** of built-in functions like `print`, `sqrt`, `vec3`, `push`, etc.

- **File:** `m:\Code\Kain\crates\kain-core\src\stdlib.rs`
- **Used by:** Type checker to resolve built-in function calls
- **`load_stdlib()`** at line 135 — **returns empty string** (`String::new()`) with a `// TODO` comment
- This is the stub that was supposed to load `.kn` files but never got implemented

### System 2 — `stdlib/ue5/*.kn` (Data-Driven KAIN Source Files)
The actual `.kn` files in `m:\Code\Kain\stdlib\ue5\` that get prepended to user source before compilation. This is the **source-level stdlib** — pure KAIN code that compiles through the normal pipeline.

- **Files:** `common.kn`, `math.kn`, `gameplay.kn` (3 of 12 planned)
- **Loaded by:** `ue5_pipeline.rs` `load_and_parse_sources()` — **but only if `stdlib_path` is set in KAIN.toml**
- Currently **disabled by default**

---

## How the Pipeline Works (Full Flow)

### Path A — Single-file `compile()` in `cli/src/lib.rs`
Used by `kain run`, `kain build --target js/rust/wasm/etc`, and all non-UE5 targets.

```
compile(source, target)
  → stdlib::load_stdlib()          ← RETURNS EMPTY STRING (broken)
  → format!("{}\n{}", stdlib, source)
  → Lexer → Parser → Comptime → TypeCheck → Monomorphize → Codegen
```

**Every single-file compile path calls `load_stdlib()` and prepends it.** The hook is already there — it just returns nothing. This includes:
- `compile()` — line 38
- `compile_ue5_with_context()` — line 161
- `generate_usf_header()` — line 267
- `generate_usf_implementation()` — line 280
- `compile_ue5editor()` — line 293

### Path B — Multi-file `build_ue5_plugin()` in `cli/src/packager/ue5_pipeline.rs`
Used by `kain build --ue5`. This is the production path for the Factory plugins.

```
build_ue5_plugin()
  → load_and_parse_sources()
      → check ue5_config.stdlib_path    ← DISABLED BY DEFAULT (returns empty vec)
      → if stdlib_path set: read *.kn from that dir, add to all_source_files FIRST
      → then add user source files
      → parse ALL files together as one merged program
  → codegen, shader compile, etc.
```

The multi-file path has a **complete, working stdlib injection implementation** — it just requires `stdlib_path` to be set in `KAIN.toml`. The code at lines 1108-1141 of `ue5_pipeline.rs` reads all `.kn` files from the path, skips READMEs, sorts them, and prepends them before user files.

### Path C — Runtime Interpreter in `kain-core/src/runtime.rs`
Used by `kain run`. Has its own stdlib discovery via `find_stdlib_roots()` at line 1816.

```
find_stdlib_roots()
  → checks KAIN_STDLIB_PATH env var
  → walks up from exe location looking for stdlib/
  → walks up from CWD looking for stdlib/
```

The runtime has `load_module()` at line 1847 that handles `use std/option` style imports. This is a third, separate mechanism.

---

## Current State: What's Broken and Why

### Problem 1: `load_stdlib()` is a stub
**File:** `m:\Code\Kain\crates\kain-core\src\stdlib.rs` lines 134-139

```rust
pub fn load_stdlib() -> String {
    // For now, return empty string
    // TODO: Load actual stdlib .kn files from stdlib/ directory
    String::new()
}
```

This is called in 5 places in `cli/src/lib.rs`. The hook exists, the prepend logic exists — the function just returns nothing.

### Problem 2: UE5 pipeline stdlib disabled by default
**File:** `m:\Code\Kain\crates\cli\src\packager\ue5_pipeline.rs` lines 1107-1114

```rust
// STDLIB IS NOW DISABLED BY DEFAULT - only load if explicitly configured
let stdlib_search_paths: Vec<PathBuf> = if let Some(custom_path) = &ue5_config.stdlib_path {
    vec![custom_path.clone()]
} else {
    // STDLIB DISABLED BY DEFAULT - return empty vec
    vec![]
};
```

Someone explicitly disabled it. The feature was working, then got turned off. The implementation is complete — it just needs the default path restored.

### Problem 3: `stdlib/ue5/` is the wrong subdirectory
The pipeline loads from `stdlib_path` directly (flat directory). The stdlib files are in `stdlib/ue5/`. So even if you set `stdlib_path = "stdlib"`, it would look for `.kn` files in `stdlib/` root, not `stdlib/ue5/`. The path needs to point to `stdlib/ue5/`.

### Problem 4: `gameplay.kn` has syntax bugs
- Line 71-72: `var total_weight`, `var i` → should be `let`
- Line 75: `&&` → should be `and`
These will cause parse errors when the stdlib is loaded.

---

## The Fix — Exact Steps

### Fix 1: Restore `load_stdlib()` to read from disk
**File:** `m:\Code\Kain\crates\kain-core\src\stdlib.rs`

Replace the stub with a real implementation that finds and reads the stdlib files:

```rust
pub fn load_stdlib() -> String {
    // Search for stdlib directory in order:
    // 1. KAIN_STDLIB_PATH env var
    // 2. Walk up from exe location
    // 3. Walk up from CWD
    let search_roots = find_stdlib_search_roots();
    
    for root in &search_roots {
        // Try root/ue5/ first (UE5 target stdlib)
        let ue5_path = root.join("ue5");
        if ue5_path.exists() {
            if let Some(src) = load_kn_files_from_dir(&ue5_path) {
                return src;
            }
        }
        // Try root/ directly
        if root.exists() {
            if let Some(src) = load_kn_files_from_dir(root) {
                return src;
            }
        }
    }
    
    String::new()
}

fn find_stdlib_search_roots() -> Vec<std::path::PathBuf> {
    let mut roots = Vec::new();
    
    if let Ok(env_path) = std::env::var("KAIN_STDLIB_PATH") {
        roots.push(std::path::PathBuf::from(env_path));
    }
    
    if let Ok(exe_path) = std::env::current_exe() {
        let mut dir = exe_path.parent().map(|p| p.to_path_buf());
        while let Some(d) = dir {
            roots.push(d.join("stdlib"));
            dir = d.parent().map(|p| p.to_path_buf());
        }
    }
    
    if let Ok(mut dir) = std::env::current_dir() {
        loop {
            roots.push(dir.join("stdlib"));
            if !dir.pop() { break; }
        }
    }
    
    roots
}

fn load_kn_files_from_dir(path: &std::path::Path) -> Option<String> {
    let mut files: Vec<_> = std::fs::read_dir(path).ok()?
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| p.extension().map_or(false, |e| e == "kn"))
        .filter(|p| !p.file_name()
            .map_or(false, |n| n.to_string_lossy().to_uppercase().contains("README")))
        .collect();
    
    if files.is_empty() {
        return None;
    }
    
    files.sort();
    
    let mut combined = String::new();
    for file in &files {
        if let Ok(src) = std::fs::read_to_string(file) {
            combined.push_str(&src);
            combined.push('\n');
        }
    }
    
    Some(combined)
}
```

This mirrors exactly what `runtime.rs` already does with `find_stdlib_roots()` — just unified and applied to `load_stdlib()`.

### Fix 2: Restore default stdlib path in UE5 pipeline
**File:** `m:\Code\Kain\crates\cli\src\packager\ue5_pipeline.rs` lines 1107-1114

Replace the disabled block with auto-discovery:

```rust
// Auto-discover stdlib: check KAIN_STDLIB_PATH, then walk up from CWD
let stdlib_search_paths: Vec<PathBuf> = if let Some(custom_path) = &ue5_config.stdlib_path {
    // Explicit path from KAIN.toml takes priority
    vec![custom_path.clone()]
} else {
    // Auto-discover: check env var, then walk up from CWD looking for stdlib/ue5/
    let mut paths = Vec::new();
    
    if let Ok(env_path) = std::env::var("KAIN_STDLIB_PATH") {
        paths.push(PathBuf::from(env_path));
    }
    
    // Walk up from CWD looking for stdlib/ue5/
    if let Ok(mut dir) = std::env::current_dir() {
        loop {
            let candidate = dir.join("stdlib").join("ue5");
            paths.push(candidate);
            if !dir.pop() { break; }
        }
    }
    
    paths
};
```

### Fix 3: Fix syntax bugs in `gameplay.kn`
**File:** `m:\Code\Kain\stdlib\ue5\gameplay.kn`

- Line 71: `var total_weight = 0.0` → `let total_weight = 0.0`
- Line 72: `var i = 0` → `let i = 0`
- Line 75: `slot.item_id >= 0 && slot.item_id < item_weights.len()` → `slot.item_id >= 0 and slot.item_id < item_weights.len()`

### Fix 4: KAIN.toml opt-in (optional, for explicit control)
Any plugin can override the auto-discovered stdlib path:

```toml
[ue5]
plugin_name = "Materialize"
stdlib_path = "../../stdlib/ue5"  # relative to KAIN.toml location
```

Or use the env var: `KAIN_STDLIB_PATH=m:\Code\Kain\stdlib\ue5`

---

## What `@extern` and `@blueprint` Do (How stdlib.kn Functions Work)

### `@extern fn`
Declares a function that exists in C++ but not in KAIN source. The parser accepts it as a valid function signature. The type-checker registers it as a known function. The UE5 codegen **does not emit a C++ definition** for it — it assumes the function exists in the UE5 engine or plugin runtime.

```kain
@extern
fn GetWorldDeltaSeconds() -> Float
```

→ In generated C++, calls to `GetWorldDeltaSeconds()` emit as-is, relying on UE5's `GetWorld()->GetDeltaSeconds()` being available in scope.

**Important:** `@extern` functions are declaration-only. They need no body. They tell the type-checker "this function exists, trust me."

### `@blueprint fn`
Marks a pure KAIN function for Blueprint exposure. The function has a full KAIN body that gets compiled to C++. The UE5 codegen emits it as `UFUNCTION(BlueprintCallable)`.

```kain
@blueprint
fn apply_damage(current_health: Float, damage: Float, armor: Float) -> Float:
    let mitigated = damage * (1.0 - armor / 100.0)
    return max(current_health - mitigated, 0.0)
```

→ Generates a full `UFUNCTION(BlueprintCallable)` C++ function in the plugin.

**These are the two patterns the stdlib uses.** `@extern` for engine bindings, `@blueprint` for pure logic.

---

## The `StdLib` Rust Struct — What It's For

`kain-core/src/stdlib.rs` has a `StdLib` struct with a `HashMap<String, BuiltinFn>`. This is **separate from the `.kn` file loading**. It's used by the type-checker to know about built-in functions that are hardcoded into the compiler (like `vec3`, `push`, `len`, `print`).

These are functions that exist in the interpreter/runtime but have no `.kn` source file — they're implemented directly in Rust. The `StdLib` struct tells the type-checker their signatures so it doesn't reject calls to them.

**The `.kn` stdlib files are a layer on top of this** — they add higher-level functions implemented in KAIN itself, which then compile to C++ via the normal pipeline.

The two systems are complementary:
- `StdLib` struct → low-level builtins (vec3, push, len) — Rust-implemented, always available
- `stdlib/ue5/*.kn` → high-level helpers (apply_damage, fresnel_schlick) — KAIN-implemented, target-specific

---

## File Map — Every Relevant Location

```
m:\Code\Kain\
├── stdlib\
│   └── ue5\
│       ├── common.kn          ← 3 @extern functions (skeleton)
│       ├── math.kn            ← 11 @extern math/vector functions  
│       ├── gameplay.kn        ← 20 @blueprint functions (HAS BUGS: var, &&)
│       └── README.md          ← Documents 12 files, 9 don't exist yet
│
└── crates\
    ├── kain-core\src\
    │   ├── stdlib.rs          ← load_stdlib() STUB (line 135) — FIX THIS
    │   └── runtime.rs         ← find_stdlib_roots() (line 1816) — already works for interpreter
    │
    └── cli\src\
        ├── lib.rs             ← 5 calls to load_stdlib() + format prepend (lines 38,161,267,280,293)
        └── packager\
            ├── config.rs      ← Ue5Config.stdlib_path: Option<PathBuf> (line 32)
            └── ue5_pipeline.rs ← load_and_parse_sources() stdlib loading (lines 1102-1149)
                                   DISABLED AT LINE 1112 — FIX THIS
```

---

## Priority Order for Agents

### Step 1 — Fix `gameplay.kn` syntax bugs (2 min, no backend)
```
var → let  (lines 71, 72)
&& → and   (line 75)
```

### Step 2 — Fix `load_stdlib()` in `kain-core/src/stdlib.rs`
Replace the stub with the disk-reading implementation above. This fixes ALL single-file compile paths simultaneously (5 call sites in `cli/src/lib.rs` all benefit automatically).

### Step 3 — Restore default stdlib path in `ue5_pipeline.rs`
Replace the disabled block (lines 1107-1114) with the auto-discovery implementation above. This fixes `kain build --ue5` for all Factory plugins.

### Step 4 — `cargo install --path crates/cli --force`
Rebuild and install. Test with a simple plugin that calls `apply_damage()` without defining it.

### Step 5 — Write the missing stdlib files
In priority order:
1. `patterns.kn` — type definitions (`LootRarity`, `BuffType`, `InventorySlot`) that `gameplay.kn` depends on
2. `actor.kn` — `@extern` bindings for the 20 most common actor functions
3. `world.kn` — `@extern` bindings for world/time/spawn functions
4. `shaders.kn` — `@blueprint`/`@shader_fn` PBR math, noise, color grading (highest LOC savings)
5. `utilities.kn` — pure KAIN helpers: `remap`, `smooth_step`, `lerp_color`, `random_range`
6. `materials.kn` — `@extern` material parameter functions
7. `particles.kn` — `@extern` Niagara spawn/control functions
8. `skeletal_mesh.kn` — `@extern` animation/bone/socket functions
9. `components.kn` — `HealthComponent`, `InventorySlot`, `TimerHandle` struct definitions

---

## The `@shader_fn` Gap (Edge Case)

The README mentions `shaders.kn` with functions like `fresnel_schlick`, `fbm`, `distribution_ggx`. These need to be **inlined into USF shader bodies**, not compiled to C++ Blueprint functions.

Currently there is no `@shader_fn` annotation. The parser doesn't know about it. The USF codegen doesn't handle it.

**What needs to happen:**
1. Parser: add `@shader_fn` as a recognized attribute on `fn` declarations
2. AST: store the attribute on `FnDef`
3. USF codegen (`codegen_usf.rs`): when generating a shader body, check if a called function is `@shader_fn` — if so, inline its body rather than emitting a function call
4. C++ codegen: skip `@shader_fn` functions entirely (they're shader-only)

**Workaround until then:** Write shader stdlib functions as regular KAIN functions. The USF codegen will attempt to call them as HLSL function calls. Since HLSL supports function definitions in `.usf` files, this actually works — the function just needs to be emitted as a HLSL function definition at the top of the shader file. The USF codegen already handles this for user-defined functions in the same file.

---

## Summary Table

| Location | Status | Action |
|---|---|---|
| `kain-core/src/stdlib.rs:135` | `load_stdlib()` returns `""` | Implement disk-reading |
| `cli/src/lib.rs:38,161,267,280,293` | Calls `load_stdlib()` correctly | No change needed |
| `cli/src/packager/ue5_pipeline.rs:1112` | Stdlib disabled by default | Restore auto-discovery |
| `cli/src/packager/config.rs:32` | `stdlib_path: Option<PathBuf>` | No change needed |
| `stdlib/ue5/gameplay.kn:71,72,75` | `var` and `&&` syntax bugs | Fix before loading |
| `stdlib/ue5/` | 3 of 12 files exist | Write 9 missing files |
| `@shader_fn` annotation | Doesn't exist | Future work |
| `KAIN_STDLIB_PATH` env var | Supported in runtime, not in pipeline | Add to pipeline auto-discovery |
