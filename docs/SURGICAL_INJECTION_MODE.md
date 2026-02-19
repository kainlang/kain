# KAIN Surgical Injection Mode - Design Document

**Date:** Feb 19, 2026  
**Status:** ✅ PHASE 2 COMPLETE - Inject Command Implemented  
**Priority:** HIGH - Critical for workflow flexibility

---

## Problem Statement

Currently, KAIN has TWO separate compilation paths with DIFFERENT capabilities:

### Path 1: `kain file.kn -t ue5` (Single-File Mode)
**What it does:**
- Compiles single .kn file
- Generates .h + .cpp files
- Outputs to current directory or specified path
- Uses `compile_ue5()` function (simple, no context)

**What it LACKS:**
- ❌ No EngineKnowledge loading (21K types unavailable)
- ❌ No WidgetRegistry (2.3K widgets unavailable)
- ❌ No ShaderKnowledge (7.2K functions unavailable)
- ❌ No ModuleGraph (dependency resolution unavailable)
- ❌ No Oracle validation (semantic checks skipped)
- ❌ No material system support
- ❌ No modular file output (single monolithic file)
- ❌ No .uplugin/.Build.cs generation
- ❌ No two-module split (runtime vs editor)
- ❌ No shader directory mapping
- ❌ No delegate header generation
- ❌ No EditorTypes header
- ❌ No master header with forward declarations

### Path 2: `kain build --ue5` (Plugin Mode)
**What it does:**
- Reads KAIN.toml configuration
- Loads ALL metadata (EngineKnowledge, WidgetRegistry, ShaderKnowledge, ModuleGraph)
- Runs Oracle validation
- Generates modular per-item files
- Creates complete plugin structure
- Generates .uplugin, .Build.cs
- Handles two-module split
- Material system support
- Shader directory mapping

**What it LACKS:**
- ❌ Requires KAIN.toml (not portable)
- ❌ Overwrites entire plugin structure (destructive)
- ❌ Can't surgically inject single file into existing plugin

---

## The Gap

**User needs:**
1. **Surgical injection** - Add single .kn file to existing plugin without destroying it
2. **Portable mode** - Use full pipeline power without KAIN.toml
3. **Non-destructive** - Append to existing Source/ and Shaders/ folders
4. **Full feature parity** - Access EngineKnowledge, Oracle, materials, etc.

**Current reality:**
- `-t ue5` is portable but feature-poor (no metadata, no validation)
- `build --ue5` is feature-rich but requires KAIN.toml and is destructive

---

## Solution Design

### New Mode: `kain inject --ue5 file.kn`

**Behavior:**
1. **Detect existing plugin structure** (if present)
2. **Load full metadata** (EngineKnowledge, WidgetRegistry, etc.)
3. **Parse and validate** with Oracle
4. **Generate modular output** (per-item files)
5. **Append to existing folders** (non-destructive)
6. **Skip .uplugin/.Build.cs** if they exist (or offer to update)
7. **Work standalone** if no plugin structure exists

### Command Syntax

```bash
# Inject into existing plugin (auto-detect structure)
kain inject --ue5 MyActor.kn

# Inject with explicit plugin name
kain inject --ue5 MyActor.kn --plugin MyPlugin

# Inject with output directory
kain inject --ue5 MyActor.kn --output /path/to/plugin

# Inject multiple files
kain inject --ue5 Actor1.kn Actor2.kn Shader1.kn

# Inject with material support
kain inject --ue5 MyMaterial.kn --materials

# Dry run (show what would be generated)
kain inject --ue5 MyActor.kn --dry-run

# Force overwrite (if file exists)
kain inject --ue5 MyActor.kn --force
```

### Upgrade `-t ue5` to Full Pipeline

**Alternative approach:** Make `-t ue5` use the full pipeline by default.

```bash
# Old behavior (feature-poor)
kain file.kn -t ue5 -o output.h

# New behavior (full pipeline, non-destructive)
kain file.kn -t ue5 --inject
# OR
kain file.kn -t ue5 --plugin MyPlugin
```

---

## Implementation Plan

### Phase 1: Metadata Loading in Single-File Mode (2-3 hours)

**Goal:** Make `-t ue5` load metadata like `build --ue5` does.

**Changes:**

1. **Update `compile_ue5()` in `crates/cli/src/lib.rs`:**
```rust
#[cfg(feature = "ue5")]
pub fn compile_ue5_with_context(
    source: &str, 
    output_name: Option<&str>, 
    copyright: Option<&str>,
    metadata_dir: Option<&Path>
) -> Result<ue5::Ue5Output, KainError> {
    // Load stdlib
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    
    // Parse and type-check
    let tokens = Lexer::new(&full_source).tokenize()?;
    let mut ast = Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast)?;
    
    // Load metadata (like packager does)
    let metadata_dir = metadata_dir.unwrap_or_else(|| {
        // Search for metadata like packager does
        find_metadata_dir()
    });
    
    let knowledge = EngineKnowledge::load_metadata(&metadata_dir)?;
    let widget_registry = WidgetRegistry::load(&metadata_dir)?;
    let shader_knowledge = ShaderKnowledge::load(&metadata_dir)?;
    let module_graph = ModuleGraph::load(&metadata_dir)?;
    
    // Create Ue5Context
    let context = Ue5Context::new(
        knowledge,
        widget_registry,
        shader_knowledge,
        module_graph,
        &typed_ast,
    );
    
    // Run Oracle validation
    oracle::validate(&typed_ast, &context)?;
    
    // Generate with full context
    ue5::generate_with_context(&typed_ast, output_name, copyright, &context)
}
```

2. **Add metadata search function:**
```rust
fn find_metadata_dir() -> PathBuf {
    // Same logic as packager uses
    if let Ok(kain_root) = std::env::var("KAIN_ROOT") {
        return PathBuf::from(kain_root).join("unreal/metadata");
    }
    
    // Walk up from CWD
    let mut current = std::env::current_dir().unwrap();
    loop {
        let candidate = current.join("unreal/metadata");
        if candidate.exists() {
            return candidate;
        }
        if !current.pop() {
            break;
        }
    }
    
    // Fallback
    PathBuf::from("unreal/metadata")
}
```

3. **Update main.rs to use new function:**
```rust
if target == CompileTarget::Ue5 {
    match cli::compile_ue5_with_context(&source, output_name, None, None) {
        Ok(ue5_output) => {
            // Write files...
        }
        Err(e) => {
            // Error handling...
        }
    }
}
```

**Result:** `-t ue5` now has full metadata access!

---

### Phase 2: Non-Destructive Injection (3-4 hours)

**Goal:** Make output append to existing plugin structure instead of overwriting.

**Changes:**

1. **Add injection mode to packager:**
```rust
pub fn inject_into_plugin(
    source_file: &Path,
    plugin_dir: Option<&Path>,
    plugin_name: Option<&str>,
    force: bool,
) -> KainResult<()> {
    // 1. Detect plugin structure
    let plugin_dir = plugin_dir.unwrap_or_else(|| detect_plugin_dir());
    let plugin_name = plugin_name.unwrap_or_else(|| detect_plugin_name(&plugin_dir));
    
    // 2. Load metadata
    let metadata_dir = find_metadata_dir();
    let knowledge = EngineKnowledge::load_metadata(&metadata_dir)?;
    // ... load other metadata
    
    // 3. Parse source file
    let source = fs::read_to_string(source_file)?;
    let tokens = Lexer::new(&source).tokenize()?;
    let mut ast = Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast)?;
    
    // 4. Create context
    let context = Ue5Context::new(knowledge, widget_registry, shader_knowledge, module_graph, &typed_ast);
    
    // 5. Run Oracle validation
    oracle::validate(&typed_ast, &context)?;
    
    // 6. Detect existing files
    let existing_files = scan_existing_files(&plugin_dir);
    
    // 7. Generate new files
    let new_files = generate_modular_output(&typed_ast, &plugin_name, &context)?;
    
    // 8. Check for conflicts
    for (filename, _) in &new_files {
        if existing_files.contains(filename) && !force {
            return Err(format!("File {} already exists. Use --force to overwrite.", filename).into());
        }
    }
    
    // 9. Write files (append mode)
    for (filename, content) in new_files {
        let path = plugin_dir.join(filename);
        fs::create_dir_all(path.parent().unwrap())?;
        fs::write(&path, content)?;
        println!(" Generated: {}", path.display());
    }
    
    // 10. Update master header (append includes)
    update_master_header(&plugin_dir, &plugin_name, &new_files)?;
    
    // 11. Optionally update .Build.cs (add new dependencies)
    if should_update_build_cs(&new_files) {
        update_build_cs(&plugin_dir, &plugin_name, &new_files)?;
    }
    
    Ok(())
}
```

2. **Add detection functions:**
```rust
fn detect_plugin_dir() -> PathBuf {
    // Look for .uplugin file in current dir or parent
    let cwd = std::env::current_dir().unwrap();
    if cwd.join(format!("{}.uplugin", cwd.file_name().unwrap().to_str().unwrap())).exists() {
        return cwd;
    }
    // Check parent
    if let Some(parent) = cwd.parent() {
        if parent.join(format!("{}.uplugin", parent.file_name().unwrap().to_str().unwrap())).exists() {
            return parent.to_path_buf();
        }
    }
    // Default to current dir
    cwd
}

fn detect_plugin_name(plugin_dir: &Path) -> String {
    // Look for .uplugin file
    if let Ok(entries) = fs::read_dir(plugin_dir) {
        for entry in entries.flatten() {
            if let Some(ext) = entry.path().extension() {
                if ext == "uplugin" {
                    return entry.path().file_stem().unwrap().to_str().unwrap().to_string();
                }
            }
        }
    }
    // Fallback to directory name
    plugin_dir.file_name().unwrap().to_str().unwrap().to_string()
}

fn scan_existing_files(plugin_dir: &Path) -> HashSet<String> {
    let mut files = HashSet::new();
    // Scan Source/Plugin/Public/*.h
    // Scan Source/Plugin/Private/*.cpp
    // Scan Shaders/*.usf
    // Return set of filenames
    files
}
```

3. **Add CLI command:**
```rust
// In main.rs Commands enum
#[derive(clap::Subcommand, Debug)]
enum Commands {
    // ... existing commands
    
    /// Inject KAIN file into existing plugin (non-destructive)
    Inject {
        /// Input .kn file(s)
        inputs: Vec<PathBuf>,
        
        /// Target plugin directory (auto-detected if omitted)
        #[arg(long)]
        plugin_dir: Option<PathBuf>,
        
        /// Plugin name (auto-detected if omitted)
        #[arg(long)]
        plugin: Option<String>,
        
        /// Force overwrite existing files
        #[arg(long)]
        force: bool,
        
        /// Dry run (show what would be generated)
        #[arg(long)]
        dry_run: bool,
        
        /// Use UE5 codegen
        #[arg(long)]
        ue5: bool,
    },
}
```

**Result:** `kain inject --ue5 file.kn` works non-destructively!

---

### Phase 3: Upgrade `build --ue5` to Support No-KAIN.toml (2-3 hours)

**Goal:** Make `kain build --ue5` work without KAIN.toml by using sensible defaults.

**Changes:**

1. **Make KAIN.toml optional:**
```rust
pub fn build_ue5_plugin() -> KainResult<()> {
    let cwd = std::env::current_dir()?;
    
    // Try to load KAIN.toml
    let config = match load_kain_toml(&cwd) {
        Ok(cfg) => cfg,
        Err(_) => {
            // No KAIN.toml, use defaults
            println!(" No KAIN.toml found, using defaults...");
            create_default_config(&cwd)?
        }
    };
    
    // Continue with normal build...
}

fn create_default_config(cwd: &Path) -> KainResult<Ue5Config> {
    // Detect plugin name from directory or .uplugin
    let plugin_name = detect_plugin_name(cwd);
    
    // Find all .kn files in current directory
    let sources = find_kn_files(cwd)?;
    
    if sources.is_empty() {
        return Err("No .kn files found in current directory".into());
    }
    
    println!(" Found {} .kn files", sources.len());
    
    Ok(Ue5Config {
        plugin_name,
        plugin_dir: cwd.to_path_buf(),
        sources,
        version: "1.0.0".to_string(),
        description: None,
        modular_output: true,
    })
}

fn find_kn_files(dir: &Path) -> KainResult<Vec<PathBuf>> {
    let mut files = Vec::new();
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.extension().map_or(false, |e| e == "kn") {
            files.push(path);
        }
    }
    Ok(files)
}
```

**Result:** `kain build --ue5` works without KAIN.toml!

---

## Feature Parity Matrix

| Feature | `-t ue5` (OLD) | `-t ue5` (NEW) | `inject --ue5` | `build --ue5` |
|---------|----------------|----------------|----------------|---------------|
| EngineKnowledge | ❌ | ✅ | ✅ | ✅ |
| WidgetRegistry | ❌ | ✅ | ✅ | ✅ |
| ShaderKnowledge | ❌ | ✅ | ✅ | ✅ |
| ModuleGraph | ❌ | ✅ | ✅ | ✅ |
| Oracle Validation | ❌ | ✅ | ✅ | ✅ |
| Material System | ❌ | ✅ | ✅ | ✅ |
| Modular Output | ❌ | ✅ | ✅ | ✅ |
| Two-Module Split | ❌ | ❌ | ✅ | ✅ |
| .uplugin Generation | ❌ | ❌ | Optional | ✅ |
| .Build.cs Generation | ❌ | ❌ | Optional | ✅ |
| Non-Destructive | ✅ | ✅ | ✅ | ❌ |
| Requires KAIN.toml | ❌ | ❌ | ❌ | ❌ (NEW) |
| Portable | ✅ | ✅ | ✅ | ✅ (NEW) |

---

## Usage Examples

### Example 1: Quick Single-File Compile (Upgraded)
```bash
# Old way (feature-poor)
kain MyActor.kn -t ue5 -o MyActor.h

# New way (full pipeline)
kain MyActor.kn -t ue5
# Generates: MyActor.h, MyActor.cpp with full metadata support
```

### Example 2: Inject into Existing Plugin
```bash
cd /path/to/MyPlugin

# Add new actor
kain inject --ue5 NewActor.kn
# Generates: Source/MyPlugin/Public/NewActor.h
#            Source/MyPlugin/Private/NewActor.cpp
# Appends to: Source/MyPlugin/Public/MyPlugin.h (master header)

# Add shader
kain inject --ue5 NewShader.kn
# Generates: Shaders/NewShader.usf
#            Source/MyPlugin/Public/NewShader.h
#            Source/MyPlugin/Private/NewShader.cpp

# Add material
kain inject --ue5 NewMaterial.kn --materials
# Generates: Source/MyPlugin/Private/Generated/MaterialFactories.h/cpp
```

### Example 3: Build Plugin Without KAIN.toml
```bash
cd /path/to/MyPlugin

# Just run build (auto-detects .kn files)
kain build --ue5
# Finds: Actor1.kn, Actor2.kn, Shader1.kn
# Generates complete plugin structure
```

### Example 4: Surgical Injection with Conflict Detection
```bash
# Try to inject file that exists
kain inject --ue5 MyActor.kn
# Error: File MyActor.h already exists. Use --force to overwrite.

# Force overwrite
kain inject --ue5 MyActor.kn --force
# Overwrites: MyActor.h, MyActor.cpp

# Dry run first
kain inject --ue5 MyActor.kn --dry-run
# Shows what would be generated without writing files
```

---

## Benefits

### For Users
1. **Flexibility** - Use KAIN however you want (single file, injection, full build)
2. **Non-Destructive** - Add to existing plugins without fear
3. **Portable** - No KAIN.toml required for quick work
4. **Full Power** - Access all metadata and validation everywhere

### For LLMs
1. **Consistent** - Same features everywhere, no surprises
2. **Predictable** - Clear error messages, validation always runs
3. **Composable** - Build plugins incrementally, file by file

### For Marketplace Domination
1. **Faster Iteration** - Add features to plugins in seconds
2. **Lower Risk** - Non-destructive means less fear of breaking things
3. **Better Quality** - Oracle validation catches bugs early

---

## Implementation Priority

### Must-Have (Phase 1 + 2)
- ✅ Metadata loading in `-t ue5` (2-3 hours)
- ✅ `inject --ue5` command (3-4 hours)
- ✅ Non-destructive file writing (included in Phase 2)
- ✅ Conflict detection (included in Phase 2)

**Total: 5-7 hours**

### Nice-to-Have (Phase 3)
- ✅ No-KAIN.toml support in `build --ue5` (2-3 hours)
- ✅ Auto-detection of plugin structure (included)
- ✅ Smart .Build.cs updating (included)

**Total: 2-3 hours**

### Future Enhancements
- Incremental compilation (only rebuild changed files)
- Dependency tracking (rebuild dependents when type changes)
- Hot reload integration (auto-reload in UE5 Editor)

---

## Risks & Mitigations

### Risk 1: Metadata Loading Performance
**Impact:** Loading 12MB of JSON on every `-t ue5` call could be slow.

**Mitigation:**
- Cache parsed metadata in memory (singleton pattern)
- Use `include_bytes!()` to embed metadata in binary
- Lazy load only needed metadata

### Risk 2: Conflict Detection False Positives
**Impact:** Users might get blocked when they shouldn't be.

**Mitigation:**
- Smart diffing (only warn if content actually changed)
- `--force` flag for overrides
- `--dry-run` to preview changes

### Risk 3: Master Header Corruption
**Impact:** Appending to master header could break it.

**Mitigation:**
- Parse existing header before modifying
- Validate header after modification
- Keep backup before writing

---

## Testing Plan

### Unit Tests
```rust
#[test]
fn test_metadata_loading_in_single_file_mode() {
    let source = "actor Player: state health: Float = 100.0";
    let output = compile_ue5_with_context(source, None, None, None).unwrap();
    assert!(output.header.contains("APlayer"));
}

#[test]
fn test_inject_non_destructive() {
    let temp_dir = tempdir().unwrap();
    // Create existing plugin structure
    // Inject new file
    // Verify existing files unchanged
    // Verify new file created
}

#[test]
fn test_conflict_detection() {
    let temp_dir = tempdir().unwrap();
    // Create existing file
    // Try to inject same file
    // Verify error
    // Try with --force
    // Verify overwrite
}
```

### Integration Tests
```bash
# Test 1: Single file with metadata
kain MyActor.kn -t ue5
# Verify: Uses EngineKnowledge, Oracle validation runs

# Test 2: Inject into existing plugin
cd testing/Phase3/SlateTest4
kain inject --ue5 NewActor.kn
# Verify: File created, master header updated, no overwrites

# Test 3: Build without KAIN.toml
cd testing/Phase3/NoConfig
kain build --ue5
# Verify: Auto-detects .kn files, generates plugin
```

---

## Conclusion

This design gives you:
1. **Full feature parity** across all compilation modes
2. **Non-destructive injection** for surgical updates
3. **Portable mode** without KAIN.toml requirement
4. **Backward compatibility** (existing workflows still work)

**Implementation time:** 7-10 hours total

**Impact:** MASSIVE - This makes KAIN truly flexible and production-ready for all workflows.

---

**Ready to implement?** This would be a game-changer for your workflow! 🔥
