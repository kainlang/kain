# KAIN Packager Architecture
> **Last Updated:** Feb 19, 2026  
> **Purpose:** Complete guide to the build pipeline and packager system  
> **Audience:** LLMs extending the compiler, debugging build issues, adding new codegen targets

---

## Overview

The KAIN packager is the orchestration layer that transforms `.kn` source files into complete UE5 plugins. It coordinates parsing, type checking, validation, and dispatches to specialized codegen crates.

**Location:** `crates/cli/src/packager/`

**Key Files:**
- `ue5_pipeline.rs` - Main build orchestration
- `codegen.rs` - Code generation dispatch
- `plugin_layout.rs` - Directory structure management
- `material_gen.rs` - Material factory generation
- `config.rs` - KAIN.toml configuration
- `build_cs_gen.rs` - .Build.cs generation
- `uplugin_gen.rs` - .uplugin generation

---

## Build Pipeline Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    kain build --ue5                             │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 1: Load & Parse Sources (ue5_pipeline.rs)                │
│  ─────────────────────────────────────────────────────────────  │
│  • Load stdlib files (optional, disabled by default)           │
│  • Load user source files from KAIN.toml                        │
│  • Parse EACH file independently (LLM-optimized)                │
│  • Extract shader names from each file                          │
│  • Merge all ASTs into single program                           │
│  • Extract material graphs BEFORE type checking                 │
│  • Type-check merged program                                    │
│  • Run Oracle validation (semantic checks)                      │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 2: Setup Plugin Directory (plugin_layout.rs)             │
│  ─────────────────────────────────────────────────────────────  │
│  • Detect editor items (@slate, @details, @viewport, etc.)      │
│  • Detect runtime items (actors, components, shaders, etc.)     │
│  • Decide: Single module OR Two-module split                    │
│  • Create directory structure:                                  │
│    - Single: Source/Public, Source/Private                      │
│    - Split:  Source/Plugin/Public, Source/PluginEditor/Public   │
│  • Clean stale files from old layouts                           │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 3: Compile Shaders (codegen.rs)                          │
│  ─────────────────────────────────────────────────────────────  │
│  • Generate shared POD types header (PluginShaderTypes.h)       │
│  • For each shader:                                             │
│    - Generate .usf (HLSL code)                                  │
│    - Generate .h (FGlobalShader class)                          │
│    - Generate .cpp (IMPLEMENT_GLOBAL_SHADER)                    │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 3.5: Generate Material Graphs (material_gen.rs)          │
│  ─────────────────────────────────────────────────────────────  │
│  • Convert AST MaterialGraphDef to IR MaterialGraph            │
│  • Generate MaterialFactories.h/cpp in Generated/ directory    │
│  • Factory provides runtime material creation API               │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 4: Generate Plugin Files (codegen.rs)                    │
│  ─────────────────────────────────────────────────────────────  │
│  MODULAR MODE (default):                                        │
│  ├─ 4.1: Generate Headers                                       │
│  │   • Master header (forward declarations)                     │
│  │   • Delegate header (PluginDelegates.h)                      │
│  │   • EditorTypes header (runtime types for editor)            │
│  │   • Build type registry (item_name → header_file)            │
│  │                                                               │
│  ├─ 4.2: Generate Runtime Items (per-item .h/.cpp)              │
│  │   • For each actor, struct, enum, component:                 │
│  │     - Generate filtered output (single item)                 │
│  │     - Write .h to Public/                                    │
│  │     - Write .cpp to Private/ (if has implementation)         │
│  │     - Append include to master header                        │
│  │                                                               │
│  ├─ 4.3: Generate Stdlib Functions                              │
│  │   • KainStdlib.h (static inline utility functions)           │
│  │   • Insert before module includes in master header           │
│  │                                                               │
│  ├─ 4.4: Generate Blueprint Library                             │
│  │   • PluginBlueprintLibrary.h/cpp                             │
│  │   • All @blueprint functions in one UBlueprintFunctionLibrary│
│  │   • Append include to master header                          │
│  │                                                               │
│  ├─ 4.5: Generate Editor Items                                  │
│  │   • Slate widgets (SCompoundWidget)                          │
│  │   • Details customizations (IDetailCustomization)            │
│  │   • Viewports (SEditorViewport + FEditorViewportClient)      │
│  │   • Toolbars (FToolBarBuilder extensions)                    │
│  │   • Asset editors (FAssetEditorToolkit)                      │
│  │   • Editor modules (IModuleInterface)                        │
│  │   • Files go to Editor/ subdir OR separate module            │
│  │                                                               │
│  └─ 4.6: Generate Module Registration                           │
│      • Plugin.cpp (IMPLEMENT_MODULE)                            │
│      • Shader path mapping (if has_shaders)                     │
│      • Material factory initialization                          │
│      • Split mode: separate runtime + editor modules            │
│                                                                  │
│  MONOLITHIC MODE (legacy):                                      │
│  └─ Generate single Plugin.h/cpp with all types merged          │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 5: Write Plugin Metadata (codegen.rs)                    │
│  ─────────────────────────────────────────────────────────────  │
│  • Load module_graph.json (data-driven dependency resolution)   │
│  • Extract referenced UE5 types from program                    │
│  • Resolve module dependencies via graph                        │
│  • Generate .uplugin file:                                      │
│    - Module list (runtime + editor if split)                    │
│    - CanContainContent: true (if has_shaders)                   │
│  • Generate .Build.cs file(s):                                  │
│    - Single mode: One .Build.cs                                 │
│    - Split mode: Runtime.Build.cs + Editor.Build.cs             │
│    - Auto-resolved PublicDependencyModuleNames                  │
│    - Feature-based fallback if no module graph                  │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  COMPLETE: Plugin ready for UE5 compilation                    │
│  ─────────────────────────────────────────────────────────────  │
│  Output structure:                                              │
│  PluginName/                                                    │
│  ├── PluginName.uplugin                                         │
│  ├── Source/                                                    │
│  │   ├── PluginName/                (runtime module)            │
│  │   │   ├── Public/                                            │
│  │   │   │   ├── PluginName.h       (master header)             │
│  │   │   │   ├── PluginNameDelegates.h                          │
│  │   │   │   ├── PluginNameEditorTypes.h                        │
│  │   │   │   ├── Actor1.h, Struct1.h, Enum1.h, ...              │
│  │   │   │   └── ShaderName.h, PluginNameShaderTypes.h          │
│  │   │   ├── Private/                                           │
│  │   │   │   ├── PluginName.cpp     (module registration)       │
│  │   │   │   ├── Actor1.cpp, ...                                │
│  │   │   │   ├── ShaderName.cpp                                 │
│  │   │   │   └── Generated/                                     │
│  │   │   │       └── MaterialFactories.h/cpp                    │
│  │   │   └── PluginName.Build.cs                                │
│  │   └── PluginNameEditor/          (editor module, if split)   │
│  │       ├── Public/                                            │
│  │       │   ├── PluginNameEditor.h (editor master header)      │
│  │       │   └── SWidget1.h, FDetails1.h, ...                   │
│  │       ├── Private/                                           │
│  │       │   ├── PluginNameEditor.cpp (editor module reg)       │
│  │       │   └── SWidget1.cpp, FDetails1.cpp, ...               │
│  │       └── PluginNameEditor.Build.cs                          │
│  └── Shaders/                                                   │
│      └── ShaderName.usf                                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## Key Components

### 1. ue5_pipeline.rs - Main Orchestration

**Entry point:** `build_ue5_plugin()`

**Responsibilities:**
- Load KAIN.toml configuration
- Load and parse source files (stdlib + user)
- Merge ASTs into single program
- Type-check and validate
- Coordinate all codegen steps
- Print build summary

**Key Functions:**

```rust
// Main entry point
pub fn build_ue5_plugin() -> KainResult<()>

// Load stdlib + user sources, parse, validate, type-check
fn load_and_parse_sources(
    ue5_config: &Ue5Config,
    manifest: &PackageManifest,
    cwd: &PathBuf,
) -> KainResult<(TypedProgram, Vec<String>, Vec<PathBuf>, Vec<PathBuf>, Vec<MaterialGraphDef>)>

// Convert AST MaterialGraphDef to IR MaterialGraph
#[cfg(feature = "ue5")]
fn convert_material_graph(def: &MaterialGraphDef) -> KainResult<MaterialGraph>
```

**LLM-Optimized Pipeline:**
- Parses EACH source file independently (clear error context)
- Reports errors with file:line:col immediately
- Merges ASTs only after all files validate
- Extracts material graphs BEFORE type checking (not yet in TypedItem)

**Stdlib Handling:**
- Disabled by default (empty search paths)
- Only loads if `stdlib_path` explicitly set in KAIN.toml
- Loads stdlib files FIRST (type definitions)
- Skips README files

**Module Graph Resolution:**
- Searches for `unreal/metadata/module_graph.json` using data-driven order:
  1. `KAIN_ROOT` env var (explicit override)
  2. Walk up from CWD (finds kain/ root from any plugin subdir)
  3. CWD-relative fallback
- Loads module graph for dependency resolution
- Falls back to feature-based detection if not found

---

### 2. codegen.rs - Code Generation Dispatch

**Responsibilities:**
- Dispatch to specialized codegen crates (ue5, ue5-editor, ue5-shaders)
- Generate headers (master, delegates, EditorTypes)
- Generate per-item runtime files
- Generate editor tools
- Generate module registration
- Write plugin metadata files

**Key Functions:**

```rust
// Compile shaders (dispatch to ue5-shaders crate)
pub fn compile_shaders(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
    shader_names: &[String],
) -> KainResult<()>

// Generate master header, delegate header, EditorTypes header
pub fn generate_headers(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
) -> KainResult<(PathBuf, usize, HashMap<String, String>)>

// Generate per-item runtime files (actors, structs, enums, components)
pub fn generate_runtime_items(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
    shader_names: &[String],
    type_headers: &HashMap<String, String>,
    master_header_path: &PathBuf,
) -> KainResult<()>

// Generate stdlib functions header
pub fn generate_stdlib_functions(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
    type_headers: &HashMap<String, String>,
    master_header_path: &PathBuf,
) -> KainResult<()>

// Generate blueprint function library
pub fn generate_blueprint_library(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
    type_headers: &HashMap<String, String>,
    master_header_path: &PathBuf,
) -> KainResult<()>

// Generate editor tools (Slate, Details, Viewport, Toolbar, Asset Editors, Modules)
pub fn generate_editor_items(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
    master_header_path: &PathBuf,
) -> KainResult<()>

// Generate module registration (IMPLEMENT_MODULE)
pub fn generate_module_registration(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
    has_shaders: bool,
) -> KainResult<()>

// Generate monolithic output (legacy single .h/.cpp)
pub fn generate_monolithic(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &TypedProgram,
) -> KainResult<()>

// Write .uplugin and .Build.cs files
pub fn write_plugin_files(
    layout: &PluginLayout,
    config: &Ue5Config,
    description: &Option<String>,
    has_shaders: bool,
    module_graph: &ModuleGraph,
    program: &TypedProgram,
) -> KainResult<()>
```

**Header Generation Strategy:**

1. **Master Header** (`Plugin.h`):
   - Forward declarations for all types
   - Includes delegate header (if delegates exist)
   - Includes individual type headers (appended during item generation)
   - NO .generated.h (not a UHT-processed type)

2. **Delegate Header** (`PluginDelegates.h`):
   - ONLY delegate declarations (avoids circular dependencies)
   - Includes type dependencies (enums, structs referenced by delegates)
   - Has .generated.h (UHT-processed via dummy USTRUCT)
   - Uses TypeMapper for correct prefix detection

3. **EditorTypes Header** (`PluginEditorTypes.h`):
   - Includes ALL runtime type headers
   - Includes delegate header
   - Forward declares Slate widgets (prevents circular deps)
   - Single include for editor code (Slate, Details, Viewports)

**Type Registry:**
- HashMap<String, String> mapping item_name → header_file
- Passed to all codegen functions
- Enables correct #include generation

**Module Registration:**
- Detects @editor_module (skips default IMPLEMENT_MODULE)
- Split mode: generates separate runtime + editor modules
- Single mode: generates one module (or skips if @editor_module)
- Includes shader path mapping (if has_shaders)
- Includes material factory initialization

**Dependency Resolution:**
- Extracts referenced UE5 types from program
- Resolves via module_graph.json (data-driven)
- Falls back to feature-based detection (legacy)
- Separate resolution for runtime vs editor modules

---

### 3. plugin_layout.rs - Directory Structure

**Responsibilities:**
- Detect editor vs runtime items
- Decide single module vs two-module split
- Create directory structure
- Clean stale files from old layouts

**Key Functions:**

```rust
// Detect editor items (@slate, @details, @viewport, etc.)
pub fn detect_editor_items(program: &TypedProgram) -> bool

// Detect runtime items (actors, components, shaders, etc.)
pub fn detect_runtime_items(program: &TypedProgram, has_shaders: bool) -> bool

// Setup plugin directory structure
pub fn setup(
    config: &Ue5Config,
    cwd: &Path,
    program: &TypedProgram,
    has_shaders: bool,
) -> KainResult<PluginLayout>
```

**PluginLayout Struct:**

```rust
pub struct PluginLayout {
    pub plugin_root: PathBuf,
    pub source_dir: PathBuf,
    pub shaders_dir: PathBuf,
    pub public_dir: PathBuf,
    pub private_dir: PathBuf,
    pub editor_public_dir: Option<PathBuf>,
    pub editor_private_dir: Option<PathBuf>,
    pub needs_split: bool,
    pub has_editor_items: bool,
    pub has_runtime_items: bool,
}
```

**Split Decision Logic:**
- Two-module split: BOTH runtime AND editor items exist
- Single module: ONLY runtime OR ONLY editor items
- Split mode creates: `Source/Plugin/` + `Source/PluginEditor/`
- Single mode creates: `Source/Public/` + `Source/Private/`

**Stale File Cleanup:**
- Detects old single-module layout files in split mode
- Removes `Source/Public/`, `Source/Private/`, `Source/Plugin.Build.cs`
- Prevents conflicts between old and new layouts

---

### 4. material_gen.rs - Material Factory Generation

**Responsibilities:**
- Generate MaterialFactories.h/cpp for runtime material creation
- Provides API for creating materials from KAIN material graphs

**Key Functions:**

```rust
#[cfg(feature = "ue5")]
pub fn generate_material_factories(
    plugin_name: &str,
    graphs: &[MaterialGraph],
    output_dir: &Path,
) -> KainResult<()>
```

**Output:**
- `Source/Plugin/Private/Generated/MaterialFactories.h`
- `Source/Plugin/Private/Generated/MaterialFactories.cpp`
- Factory class: `FPluginNameMaterialFactory`
- Static method: `GenerateMaterials()` (called in module startup)

**Integration:**
- Material graphs extracted in ue5_pipeline.rs BEFORE type checking
- Converted from AST MaterialGraphDef to IR MaterialGraph
- Passed to material_gen for factory generation
- Factory initialization added to module registration

---

## Modular Compilation

### Per-File Output

**Goal:** Each KAIN source file generates separate C++ files for faster incremental compilation.

**Implementation:**
1. Parse each source file independently
2. Merge ASTs into single program (shared type context)
3. For each item in program:
   - Call `ue5::generate_filtered()` with item name
   - Write item.h to Public/
   - Write item.cpp to Private/ (if has implementation)
   - Append include to master header

**Benefits:**
- Faster incremental builds (only changed items recompile)
- Clear error messages (file:line:col)
- Better IDE integration (jump to definition)
- Scales to 100+ files

### Header Strategy

**Master Header:**
- Forward declarations only
- Includes delegate header
- Includes individual type headers (appended during generation)
- Single include point for all plugin types

**Individual Headers:**
- One per actor, struct, enum, component
- Includes own .generated.h (if UHT-processed)
- Includes dependencies via type registry
- Self-contained (can be included independently)

**Delegate Header:**
- Separate file to avoid circular dependencies
- Included FIRST in master header
- All delegates in one place

**EditorTypes Header:**
- Aggregates all runtime types for editor code
- Single include for Slate, Details, Viewports
- Prevents circular dependencies via forward declarations

---

## Common Patterns

### Adding New Codegen Targets

**Example: Adding @custom_widget support**

1. **Define attribute in kain-core:**
   ```rust
   // In kain-core/src/ast.rs
   // Add to Attribute enum if needed
   ```

2. **Add detection in plugin_layout.rs:**
   ```rust
   pub fn detect_custom_widget_items(program: &TypedProgram) -> bool {
       program.items.iter().any(|item| {
           if let TypedItem::Struct(s) = item {
               s.ast.attributes.iter().any(|a| a.name == "custom_widget")
           } else {
               false
           }
       })
   }
   ```

3. **Add codegen in ue5-editor crate:**
   ```rust
   // In ue5-editor/src/editor/custom_widget.rs
   pub fn generate_custom_widget(
       s: &TypedStruct,
       plugin_name: &str,
   ) -> Result<(String, String), String> {
       // Generate .h and .cpp
   }
   ```

4. **Add dispatch in codegen.rs:**
   ```rust
   pub fn generate_custom_widgets(
       layout: &PluginLayout,
       config: &Ue5Config,
       program: &TypedProgram,
   ) -> KainResult<()> {
       for item in &program.items {
           if let TypedItem::Struct(s) = item {
               if s.ast.attributes.iter().any(|a| a.name == "custom_widget") {
                   let (header, cpp) = ue5_editor::generate_custom_widget(s, &config.plugin_name)?;
                   // Write files
               }
           }
       }
       Ok(())
   }
   ```

5. **Call in ue5_pipeline.rs:**
   ```rust
   // In build_ue5_plugin()
   super::codegen::generate_custom_widgets(&layout, ue5_config, &typed_program)?;
   ```

### Adding New File Types

**Example: Adding .ini configuration files**

1. **Add field to Ue5Config:**
   ```rust
   // In config.rs
   pub struct Ue5Config {
       // ...
       pub config_files: Vec<PathBuf>,
   }
   ```

2. **Add generation function:**
   ```rust
   // In codegen.rs
   pub fn generate_config_files(
       layout: &PluginLayout,
       config: &Ue5Config,
       program: &TypedProgram,
   ) -> KainResult<()> {
       let config_dir = layout.plugin_root.join("Config");
       fs::create_dir_all(&config_dir)?;
       // Generate .ini files
       Ok(())
   }
   ```

3. **Call in pipeline:**
   ```rust
   // In ue5_pipeline.rs
   super::codegen::generate_config_files(&layout, ue5_config, &typed_program)?;
   ```

### Integrating New Systems

**Example: Integrating animation system**

1. **Add detection:**
   ```rust
   let has_animations = program.items.iter().any(|item| {
       // Detect animation items
   });
   ```

2. **Add to PluginLayout:**
   ```rust
   pub struct PluginLayout {
       // ...
       pub has_animations: bool,
   }
   ```

3. **Add codegen:**
   ```rust
   if layout.has_animations {
       super::codegen::generate_animations(&layout, ue5_config, &typed_program)?;
   }
   ```

4. **Update .Build.cs:**
   ```rust
   // In build_cs_gen.rs
   if has_animations {
       deps.push("AnimGraphRuntime");
   }
   ```

---

## Testing Patterns

### Unit Tests

**Test individual functions:**

```rust
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_detect_editor_items() {
        let program = /* create test program */;
        assert!(detect_editor_items(&program));
    }
}
```

### Integration Tests

**Test full pipeline:**

```rust
#[test]
fn test_build_plugin() {
    let temp_dir = tempdir().unwrap();
    // Create KAIN.toml
    // Create .kn source files
    // Run build_ue5_plugin()
    // Verify output files exist
}
```

### Snapshot Tests

**Test generated code:**

```rust
#[test]
fn test_generated_actor() {
    let program = /* parse test .kn file */;
    let output = ue5::generate(&program, Some("TestPlugin"), None).unwrap();
    insta::assert_snapshot!(output.header);
    insta::assert_snapshot!(output.source);
}
```

---

## Files LLMs Will Touch

### When to Modify ue5_pipeline.rs

- Adding new build steps
- Changing source file loading logic
- Modifying AST merging strategy
- Adding new validation passes
- Changing build summary output

### When to Modify codegen.rs

- Adding new codegen targets (widgets, tools, etc.)
- Changing header generation strategy
- Modifying module registration logic
- Adding new file types to output
- Changing dependency resolution

### When to Add New Packager Modules

- New file type generation (e.g., config_gen.rs)
- New system integration (e.g., animation_gen.rs)
- New metadata generation (e.g., localization_gen.rs)

**Pattern:**
1. Create `crates/cli/src/packager/new_system_gen.rs`
2. Add `mod new_system_gen;` to `crates/cli/src/packager/mod.rs`
3. Add public functions for generation
4. Call from ue5_pipeline.rs or codegen.rs

---

## Debugging Tips

### Build Failures

1. **Check KAIN.toml:**
   - Verify `plugin_name`, `plugin_dir`, `sources`
   - Check `modular_output = true` (default)

2. **Check source files:**
   - Run `kain build --ue5` from plugin directory
   - Look for parse errors with file:line:col
   - Check Oracle validation errors

3. **Check generated files:**
   - Verify master header includes all types
   - Check delegate header has .generated.h
   - Verify module registration has IMPLEMENT_MODULE

4. **Check module graph:**
   - Verify `unreal/metadata/module_graph.json` exists
   - Check module graph loads successfully
   - Verify dependency resolution

### Common Issues

**Issue:** Double prefixes (EEHealthStatus)
**Fix:** Use naming functions, not inline format!()

**Issue:** Missing includes
**Fix:** Check type registry, verify header generation

**Issue:** Circular dependencies
**Fix:** Use forward declarations, separate delegate header

**Issue:** Module not found
**Fix:** Check .Build.cs, verify module graph resolution

---

## Summary

The packager is the orchestration layer that:
- Loads and parses source files (LLM-optimized per-file validation)
- Merges ASTs and type-checks
- Dispatches to specialized codegen crates
- Generates modular C++ output (per-item files)
- Manages directory structure (single vs split modules)
- Resolves dependencies (data-driven via module graph)
- Writes plugin metadata (.uplugin, .Build.cs)

**Key insight:** The packager is STATELESS — all state lives in the TypedProgram and PluginLayout. This makes it easy to add new codegen targets without modifying existing code.

**For LLMs:** When adding new features, follow the pattern:
1. Detect in plugin_layout.rs
2. Generate in codegen.rs (dispatch to specialized crate)
3. Call in ue5_pipeline.rs
4. Update .Build.cs if needed

This keeps the pipeline modular, testable, and easy to extend.
