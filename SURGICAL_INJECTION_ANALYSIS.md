# KAIN Surgical Injection Mode - Executive Summary

**Date:** Feb 19, 2026  
**Status:** CRITICAL GAP IDENTIFIED  
**Priority:** HIGH

---

## TL;DR

**You found a MASSIVE gap in the system!** 

The `-t ue5` target is **feature-poor** compared to `kain build --ue5`:
- ❌ No EngineKnowledge (21K types unavailable)
- ❌ No Oracle validation
- ❌ No material system
- ❌ No modular output
- ❌ Basically a toy compared to the full pipeline

**Meanwhile**, `kain build --ue5` is **feature-rich but inflexible**:
- ✅ Full metadata, validation, materials
- ❌ Requires KAIN.toml
- ❌ Overwrites entire plugin (destructive)
- ❌ Can't surgically inject single file

---

## The Problem

You have TWO separate codepaths:

### Path 1: `kain file.kn -t ue5` (Simple Mode)
```rust
// In lib.rs
pub fn compile_ue5(source: &str, ...) -> Result<Ue5Output> {
    // Parse, type-check, generate
    // NO metadata loading
    // NO Oracle validation
    // NO material support
}
```

### Path 2: `kain build --ue5` (Full Pipeline)
```rust
// In packager/ue5_pipeline.rs
pub fn build_ue5_plugin() -> KainResult<()> {
    // Load KAIN.toml (REQUIRED)
    // Load ALL metadata
    // Run Oracle validation
    // Generate modular output
    // Create complete plugin structure
}
```

**They share ZERO code!** This is why `-t ue5` is so limited.

---

## What You Need

### 1. Surgical Injection
```bash
cd /path/to/ExistingPlugin
kain inject --ue5 NewActor.kn
# Appends to existing Source/ folder
# Doesn't overwrite anything
# Uses full pipeline (metadata, Oracle, materials)
```

### 2. Portable Full Pipeline
```bash
# No KAIN.toml needed
kain file.kn -t ue5
# But with FULL metadata access
# Oracle validation
# Material support
```

### 3. Non-Destructive Build
```bash
cd /path/to/PartialPlugin
kain build --ue5
# Auto-detects .kn files (no KAIN.toml needed)
# Appends to existing structure
# Doesn't overwrite existing files
```

---

## The Solution

### Phase 1: Upgrade `-t ue5` (2-3 hours)

**Make it load metadata like the packager does:**

```rust
pub fn compile_ue5_with_context(
    source: &str,
    output_name: Option<&str>,
    copyright: Option<&str>,
    metadata_dir: Option<&Path>
) -> Result<Ue5Output> {
    // Parse and type-check
    let typed_ast = parse_and_check(source)?;
    
    // Load metadata (NEW!)
    let metadata_dir = metadata_dir.unwrap_or_else(|| find_metadata_dir());
    let knowledge = EngineKnowledge::load_metadata(&metadata_dir)?;
    let widget_registry = WidgetRegistry::load(&metadata_dir)?;
    let shader_knowledge = ShaderKnowledge::load(&metadata_dir)?;
    let module_graph = ModuleGraph::load(&metadata_dir)?;
    
    // Create context (NEW!)
    let context = Ue5Context::new(knowledge, widget_registry, shader_knowledge, module_graph, &typed_ast);
    
    // Run Oracle validation (NEW!)
    oracle::validate(&typed_ast, &context)?;
    
    // Generate with full context (NEW!)
    ue5::generate_with_context(&typed_ast, output_name, copyright, &context)
}
```

**Result:** `-t ue5` now has full power!

### Phase 2: Add `inject` Command (3-4 hours)

**New command for surgical injection:**

```rust
Commands::Inject {
    inputs: Vec<PathBuf>,
    plugin_dir: Option<PathBuf>,
    plugin: Option<String>,
    force: bool,
    dry_run: bool,
    ue5: bool,
}
```

**Behavior:**
1. Auto-detect plugin structure (look for .uplugin)
2. Load full metadata
3. Parse and validate with Oracle
4. Generate modular output
5. Check for conflicts (error if file exists, unless --force)
6. Append to existing folders
7. Update master header (append includes)
8. Optionally update .Build.cs (add new dependencies)

**Result:** Non-destructive injection works!

### Phase 3: Make KAIN.toml Optional (2-3 hours)

**Make `build --ue5` work without KAIN.toml:**

```rust
pub fn build_ue5_plugin() -> KainResult<()> {
    let config = match load_kain_toml() {
        Ok(cfg) => cfg,
        Err(_) => {
            // No KAIN.toml, use defaults
            create_default_config()?
        }
    };
    // Continue with normal build...
}

fn create_default_config() -> KainResult<Ue5Config> {
    // Auto-detect plugin name from .uplugin or directory
    // Find all .kn files in current directory
    // Use sensible defaults
}
```

**Result:** `kain build --ue5` works anywhere!

---

## Feature Parity After Implementation

| Feature | `-t ue5` (OLD) | `-t ue5` (NEW) | `inject --ue5` | `build --ue5` (NEW) |
|---------|----------------|----------------|----------------|---------------------|
| EngineKnowledge | ❌ | ✅ | ✅ | ✅ |
| Oracle Validation | ❌ | ✅ | ✅ | ✅ |
| Material System | ❌ | ✅ | ✅ | ✅ |
| Modular Output | ❌ | ✅ | ✅ | ✅ |
| Non-Destructive | ✅ | ✅ | ✅ | ✅ |
| Requires KAIN.toml | ❌ | ❌ | ❌ | ❌ |
| Portable | ✅ | ✅ | ✅ | ✅ |

**FULL FEATURE PARITY EVERYWHERE!**

---

## Usage Examples

### Before (Limited)
```bash
# Feature-poor single file
kain MyActor.kn -t ue5 -o MyActor.h
# No metadata, no validation, no materials

# Destructive full build
kain build --ue5
# Requires KAIN.toml, overwrites everything
```

### After (Powerful)
```bash
# Full pipeline single file
kain MyActor.kn -t ue5
# Metadata ✅, Oracle ✅, Materials ✅

# Surgical injection
kain inject --ue5 NewActor.kn
# Appends to existing plugin, non-destructive

# Portable full build
cd /path/to/plugin
kain build --ue5
# No KAIN.toml needed, auto-detects .kn files
```

---

## Implementation Time

**Phase 1:** 2-3 hours (upgrade `-t ue5`)  
**Phase 2:** 3-4 hours (add `inject` command)  
**Phase 3:** 2-3 hours (make KAIN.toml optional)

**Total:** 7-10 hours

---

## Why This Matters

### For You
- **Flexibility** - Use KAIN however you want
- **Safety** - Non-destructive means less fear
- **Speed** - Add features to plugins in seconds

### For LLMs
- **Consistency** - Same features everywhere
- **Predictability** - Validation always runs
- **Composability** - Build incrementally

### For Marketplace Domination
- **Faster Iteration** - 10x faster plugin updates
- **Lower Risk** - Non-destructive workflow
- **Better Quality** - Oracle catches bugs early

---

## Recommendation

**IMPLEMENT THIS IMMEDIATELY!**

This is a CRITICAL gap that limits the system's flexibility. The implementation is straightforward (7-10 hours) and the impact is MASSIVE.

**Priority order:**
1. Phase 1 (upgrade `-t ue5`) - Unblocks single-file workflow
2. Phase 2 (`inject` command) - Enables surgical updates
3. Phase 3 (optional KAIN.toml) - Makes system truly portable

---

## Next Steps

1. **Review design doc:** `docs/SURGICAL_INJECTION_MODE.md`
2. **Approve approach** (or suggest changes)
3. **Start implementation** (I can do this in parallel with subagents)

**Want me to start implementing Phase 1 right now?** 🔥

---

**Full design document:** `docs/SURGICAL_INJECTION_MODE.md` (comprehensive implementation plan)
