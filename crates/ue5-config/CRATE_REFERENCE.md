# UE5-CONFIG Crate Reference

> **Purpose:** Generate UE5 configuration systems from KAIN `@config` structs  
> **Version:** 0.1.0  
> **Status:** Production Ready  
> **Compression Ratio:** 1:10+ (4 lines KAIN → 41+ lines C++/.ini)

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [KAIN Syntax Reference](#kain-syntax-reference)
4. [Attribute Reference](#attribute-reference)
5. [Type Mapping](#type-mapping)
6. [Generated Code Examples](#generated-code-examples)
7. [Integration with Other Crates](#integration-with-other-crates)
8. [API Reference](#api-reference)
9. [Testing](#testing)
10. [Common Patterns](#common-patterns)
11. [Troubleshooting](#troubleshooting)

---

## Overview

The `ue5-config` crate generates complete UE5 configuration systems from KAIN `@config` structs. It produces:

1. **UDeveloperSettings subclasses** — Runtime-accessible settings with automatic Project Settings UI
2. **Config .ini files** — DefaultGame.ini, DefaultEngine.ini sections
3. **Console variables (CVars)** — Auto-registered with callbacks
4. **Blueprint accessors** — Get/Set functions for Blueprint access

### What Gets Generated

From this KAIN code:

```kain
@config(category: "Game")
struct VoxelSettings:
    @setting(cvar: "voxel.ChunkSize", blueprint: true, min: 10.0, max: 1000.0)
    chunk_size: Float = 100.0
```

You get:

- `UVoxelSettings.h` (header with UCLASS, UPROPERTY, UFUNCTION declarations)
- `UVoxelSettings.cpp` (implementation with constructor, singleton, callbacks)
- Console variable registration (`TAutoConsoleVariable<float>`)
- Blueprint accessor (`GetChunkSize()`)
- DefaultGame.ini section

**Total:** 4 lines KAIN → 41+ lines C++/.ini

---

## Quick Start

### 1. Add Dependency

```toml
[dependencies]
ue5-config = { path = "../ue5-config" }
```

### 2. Write KAIN Config

```kain
@config(category: "Game", display_name: "My Plugin Settings")
struct GameSettings:
    @setting(
        display_name: "Player Speed",
        tooltip: "Movement speed in units per second",
        cvar: "game.PlayerSpeed",
        blueprint: true,
        min: 0.0,
        max: 1000.0
    )
    player_speed: Float = 300.0
    
    @setting(
        display_name: "Max Players",
        cvar: "game.MaxPlayers",
        blueprint: true,
        min: 1,
        max: 64
    )
    max_players: Int = 16
    
    @setting(
        display_name: "Enable PvP",
        cvar: "game.EnablePvP",
        blueprint: true,
        writable: true
    )
    enable_pvp: Bool = false
```

### 3. Generate Code

```rust
use ue5_config::generate_config_code;
use kain_core::ast::Program;

let program = /* parsed KAIN program */;
let files = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API")?;

for file in files {
    std::fs::write(&file.path, &file.content)?;
}
```

### 4. Use in UE5

```cpp
// C++ access
const UGameSettings* Settings = UGameSettings::Get();
float Speed = Settings->PlayerSpeed;

// Console command
voxel.ChunkSize 200

// Blueprint access
float Speed = UGameSettings::GetPlayerSpeed();
UGameSettings::SetEnablePvP(true);
```

---

## KAIN Syntax Reference

### @config Attribute

Marks a struct as a configuration settings class.

```kain
@config(
    category: "Game" | "Engine" | "Editor" | "EditorPerProjectUserSettings",
    file: "DefaultGame.ini",           // Optional: custom .ini file
    section: "MySection",               // Optional: custom .ini section
    display_name: "My Settings"         // Optional: Project Settings display name
)
struct MySettings:
    // fields...
```

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `category` | String | Yes | Config category (determines .ini file and UCLASS specifier) |
| `file` | String | No | Custom .ini file name (overrides category default) |
| `section` | String | No | Custom .ini section name (defaults to `/Script/{Plugin}.{ClassName}`) |
| `display_name` | String | No | Display name in Project Settings UI (defaults to struct name with spaces) |

**Category Mapping:**

| Category | UCLASS Config | Default .ini File |
|----------|---------------|-------------------|
| `"Game"` | `Config=Game` | `DefaultGame.ini` |
| `"Engine"` | `Config=Engine` | `DefaultEngine.ini` |
| `"Editor"` | `Config=Editor` | `DefaultEditor.ini` |
| `"EditorPerProjectUserSettings"` | `Config=EditorPerProjectUserSettings` | `DefaultEditorPerProjectUserSettings.ini` |

### @setting Attribute

Marks a field as a configuration setting.

```kain
@setting(
    display_name: "My Setting",         // Optional: UI display name
    tooltip: "Help text",               // Optional: tooltip text
    cvar: "plugin.SettingName",         // Optional: console variable name
    blueprint: true,                    // Optional: generate Blueprint accessors
    min: 0.0,                           // Optional: minimum value (numeric types)
    max: 100.0,                         // Optional: maximum value (numeric types)
    writable: false                     // Optional: generate setter (default: false)
)
field_name: Type = default_value
```

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `display_name` | String | No | Display name in Project Settings UI (defaults to field name with spaces) |
| `tooltip` | String | No | Tooltip text shown in UI |
| `cvar` | String | No | Console variable name (e.g., `"voxel.ChunkSize"`) |
| `blueprint` | Bool | No | Generate Blueprint accessor functions (default: false) |
| `min` | Float/Int | No | Minimum value (generates `ClampMin` meta) |
| `max` | Float/Int | No | Maximum value (generates `ClampMax` meta) |
| `writable` | Bool | No | Generate setter functions (default: false, read-only) |

---

## Attribute Reference

### Complete Attribute Combinations

```kain
// Minimal config
@config(category: "Game")
struct MinimalSettings:
    @setting
    value: Float = 1.0

// Full config with all options
@config(
    category: "Game",
    file: "DefaultGame.ini",
    section: "/Script/MyPlugin.MySettings",
    display_name: "My Custom Settings"
)
struct FullSettings:
    @setting(
        display_name: "Custom Name",
        tooltip: "This is a helpful tooltip",
        cvar: "my.CustomCVar",
        blueprint: true,
        min: 0.0,
        max: 100.0,
        writable: true
    )
    custom_field: Float = 50.0

// Read-only Blueprint accessor
@config(category: "Game")
struct ReadOnlySettings:
    @setting(blueprint: true)
    readonly_value: Float = 1.0

// Writable Blueprint accessor
@config(category: "Game")
struct WritableSettings:
    @setting(blueprint: true, writable: true)
    writable_value: Float = 1.0

// Console variable only (no Blueprint)
@config(category: "Game")
struct CVarSettings:
    @setting(cvar: "game.Value")
    value: Float = 1.0

// UI-only setting (no CVar, no Blueprint)
@config(category: "Game")
struct UISettings:
    @setting(display_name: "UI Value", tooltip: "Shown in Project Settings")
    value: Float = 1.0
```

---

## Type Mapping

### KAIN → UE5 Type Mapping

| KAIN Type | UE5 C++ Type | CVar Type | .ini Format | Default Value |
|-----------|--------------|-----------|-------------|---------------|
| `Float` | `float` | `TAutoConsoleVariable<float>` | `"100.0"` | `0.0f` |
| `Int` | `int32` | `TAutoConsoleVariable<int32>` | `"4"` | `0` |
| `Bool` | `bool` | `TAutoConsoleVariable<bool>` | `"True"` / `"False"` | `false` |
| `String` | `FString` | `TAutoConsoleVariable<FString>` | `"MyString"` | `TEXT("")` |

### Type-Specific Notes

**Float:**
- Literals get `f` suffix: `100.0` → `100.0f`
- Min/max constraints use `ClampMin` and `ClampMax` meta specifiers

**Int:**
- Maps to `int32` (not `int`)
- Min/max constraints supported

**Bool:**
- .ini format uses `"True"` / `"False"` (capital T/F)
- Not `"true"` / `"false"`

**String:**
- Literals wrapped in `TEXT()` macro
- .ini format is plain string (no quotes)

---

## Generated Code Examples

### Example 1: Simple Float Setting

**Input KAIN:**
```kain
@config(category: "Game")
struct VoxelSettings:
    @setting(cvar: "voxel.ChunkSize", blueprint: true, min: 10.0, max: 1000.0)
    chunk_size: Float = 100.0
```

**Generated Header (VoxelSettings.h):**
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoxelSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Voxel Settings"))
class MYPLUGIN_API UVoxelSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UVoxelSettings();

    UPROPERTY(Config, EditAnywhere, Category="Voxel Settings", meta=(
        DisplayName="Chunk Size",
        ClampMin="10.0",
        ClampMax="1000.0"
    ))
    float ChunkSize;

    // Singleton accessor
    static const UVoxelSettings* Get();

    // Blueprint accessor
    UFUNCTION(BlueprintCallable, Category="Voxel Settings")
    static float GetChunkSize();

    // Console variable callback
    void OnChunkSizeChanged();

    virtual FName GetContainerName() const override;
    virtual void PostInitProperties() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
```

**Generated Source (VoxelSettings.cpp):**
```cpp
#include "VoxelSettings.h"

// Console variable
static TAutoConsoleVariable<float> CVarChunkSize(
    TEXT("voxel.ChunkSize"),
    100.0f,
    TEXT("Chunk Size"),
    ECVF_Default
);

UVoxelSettings::UVoxelSettings()
    : ChunkSize(100.0f)
{
    CategoryName = TEXT("Plugins");
    SectionName = TEXT("Voxel Settings");
}

const UVoxelSettings* UVoxelSettings::Get()
{
    return GetDefault<UVoxelSettings>();
}

float UVoxelSettings::GetChunkSize()
{
    return Get()->ChunkSize;
}

void UVoxelSettings::OnChunkSizeChanged()
{
    ChunkSize = CVarChunkSize.GetValueOnGameThread();
}

FName UVoxelSettings::GetContainerName() const
{
    return TEXT("Project");
}

void UVoxelSettings::PostInitProperties()
{
    Super::PostInitProperties();

#if WITH_EDITOR
    if (IsTemplate())
    {
        ImportConsoleVariableValues();
    }
#endif
}

#if WITH_EDITOR
void UVoxelSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property)
    {
        ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
    }
}
#endif
```

**Generated .ini Section:**
```ini
[/Script/MyPlugin.VoxelSettings]
ChunkSize=100.0
```

### Example 2: Multiple Settings with Different Types

**Input KAIN:**
```kain
@config(category: "Engine", display_name: "Advanced Settings")
struct AdvancedSettings:
    @setting(cvar: "adv.MaxLOD", min: 1, max: 8)
    max_lod: Int = 4
    
    @setting(cvar: "adv.DebugVis", blueprint: true, writable: true)
    debug_vis: Bool = false
    
    @setting(display_name: "Material Path", tooltip: "Path to material asset")
    material_path: String = "/Game/Materials/Default"
```

**Generated Header (AdvancedSettings.h):**
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AdvancedSettings.generated.h"

UCLASS(Config=Engine, DefaultConfig, meta=(DisplayName="Advanced Settings"))
class MYPLUGIN_API UAdvancedSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UAdvancedSettings();

    UPROPERTY(Config, EditAnywhere, Category="Advanced Settings", meta=(
        DisplayName="Max Lod",
        ClampMin="1",
        ClampMax="8"
    ))
    int32 MaxLod;

    UPROPERTY(Config, EditAnywhere, Category="Advanced Settings", meta=(
        DisplayName="Debug Vis"
    ))
    bool DebugVis;

    UPROPERTY(Config, EditAnywhere, Category="Advanced Settings", meta=(
        DisplayName="Material Path",
        ToolTip="Path to material asset"
    ))
    FString MaterialPath;

    static const UAdvancedSettings* Get();

    UFUNCTION(BlueprintCallable, Category="Advanced Settings")
    static bool GetDebugVis();

    UFUNCTION(BlueprintCallable, Category="Advanced Settings")
    static void SetDebugVis(bool NewValue);

    void OnMaxLodChanged();
    void OnDebugVisChanged();

    virtual FName GetContainerName() const override;
    virtual void PostInitProperties() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
```

---

## Integration with Other Crates

### Dependencies

```toml
[dependencies]
kain-core = { path = "../kain-core" }  # AST types
ue5 = { path = "../ue5" }              # UE5 context (optional)
heck = { workspace = true }            # Case conversion
minijinja = { workspace = true }       # Template engine
serde = { workspace = true }           # Serialization
serde_json = { workspace = true }      # JSON parsing
anyhow = "1.0"                         # Error handling
thiserror = "1.0"                      # Error types
```

### Integration with CLI

The `cli` crate's packager calls `ue5-config` during UE5 plugin generation:

```rust
// In cli/src/packager/ue5_pipeline.rs
use ue5_config::generate_config_code;

let config_files = generate_config_code(&program, plugin_name, module_api)?;
for file in config_files {
    write_file(&output_dir, &file.path, &file.content)?;
}
```

### Integration with ue5 Crate

The `ue5-config` crate is independent but follows the same patterns as `ue5`:

- Uses `kain-core::ast` types
- Generates similar C++ code structure
- Follows UE5 naming conventions (U prefix, PascalCase)
- Uses Minijinja templates for code generation

---

## API Reference

### Main Entry Point

```rust
pub fn generate_config_code(
    program: &Program,
    plugin_name: &str,
    module_api: &str,
) -> Result<Vec<GeneratedFile>>
```

Generates UE5 configuration code from a KAIN program.

**Parameters:**
- `program` - The parsed KAIN program (from `kain-core`)
- `plugin_name` - Plugin name (used for .ini sections and CVar prefixes)
- `module_api` - Module API macro (e.g., `"MYPLUGIN_API"`)

**Returns:**
- `Vec<GeneratedFile>` - List of generated files with paths and content

**Example:**
```rust
let program = parse_kain_file("settings.kn")?;
let files = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API")?;

for file in files {
    println!("Generated: {}", file.path);
    std::fs::write(&file.path, &file.content)?;
}
```

### GeneratedFile

```rust
pub struct GeneratedFile {
    pub path: String,    // Relative path (e.g., "Source/Public/VoxelSettings.h")
    pub content: String, // File content
}
```

### IR Types

#### ConfigStruct

```rust
pub struct ConfigStruct {
    pub name: String,
    pub category: ConfigCategory,
    pub ini_file: Option<String>,
    pub ini_section: Option<String>,
    pub display_name: Option<String>,
    pub fields: Vec<ConfigField>,
    pub original_struct: Struct,
    pub span: Span,
}
```

**Methods:**
- `ue5_class_name() -> String` - Get UE5 class name (adds U prefix)
- `get_display_name() -> String` - Get display name (with default)
- `get_ini_file() -> String` - Get .ini file name (with default)
- `get_ini_section(plugin_name: &str) -> String` - Get .ini section name

#### ConfigField

```rust
pub struct ConfigField {
    pub name: String,
    pub ty: Type,
    pub default: Option<Expr>,
    pub display_name: Option<String>,
    pub tooltip: Option<String>,
    pub cvar: Option<String>,
    pub blueprint: bool,
    pub min: Option<f64>,
    pub max: Option<f64>,
    pub writable: bool,
    pub original_field: Field,
    pub span: Span,
}
```

**Methods:**
- `ue5_property_name() -> String` - Get UE5 property name (PascalCase)
- `get_display_name() -> String` - Get display name (with default)
- `get_cvar_name(plugin_name: &str) -> Option<String>` - Get CVar name
- `has_cvar() -> bool` - Check if field has a console variable

#### ConfigCategory

```rust
pub enum ConfigCategory {
    Game,
    Engine,
    Editor,
    EditorPerProjectUserSettings,
}
```

**Methods:**
- `uclass_specifier() -> &'static str` - Get UCLASS Config specifier
- `default_ini_file() -> &'static str` - Get default .ini file name
- `from_str(s: &str) -> Option<Self>` - Parse from string

---

## Testing

### Test Coverage

The crate has 50+ tests across 7 test files:

| Test File | Tests | Coverage |
|-----------|-------|----------|
| `config_ir_tests.rs` | 10+ | IR type construction, methods |
| `parser_tests.rs` | 15+ | Attribute parsing, error handling |
| `developer_settings_tests.rs` | 8+ | UDeveloperSettings generation |
| `ini_file_tests.rs` | 5+ | .ini file format |
| `cvar_tests.rs` | 5+ | Console variable generation |
| `blueprint_accessor_tests.rs` | 8+ | Blueprint accessor generation |
| `integration_tests.rs` | 20+ | End-to-end scenarios |

### Running Tests

```bash
# Run all tests
cargo test

# Run specific test file
cargo test --test integration_tests

# Run with output
cargo test -- --nocapture

# Run specific test
cargo test test_integration_game_config_single_float_setting
```

### Test Examples

```rust
#[test]
fn test_generate_config_code_with_config_struct() {
    let program = /* build test program */;
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    
    assert!(result.is_ok());
    let files = result.unwrap();
    assert_eq!(files.len(), 2); // Header and source
    
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("UVoxelSettings"));
}
```

---

## Common Patterns

### Pattern 1: Game Settings with Blueprint Access

```kain
@config(category: "Game", display_name: "Game Settings")
struct GameSettings:
    @setting(blueprint: true, min: 0.0, max: 1000.0)
    player_speed: Float = 300.0
    
    @setting(blueprint: true, min: 1, max: 64)
    max_players: Int = 16
    
    @setting(blueprint: true, writable: true)
    pvp_enabled: Bool = false
```

**Use Case:** Settings that need to be accessed from Blueprints.

### Pattern 2: Console Variable Configuration

```kain
@config(category: "Engine")
struct DebugSettings:
    @setting(cvar: "debug.ShowFPS")
    show_fps: Bool = false
    
    @setting(cvar: "debug.LogLevel", min: 0, max: 4)
    log_level: Int = 2
    
    @setting(cvar: "debug.WireframeOpacity", min: 0.0, max: 1.0)
    wireframe_opacity: Float = 0.5
```

**Use Case:** Settings controlled via console commands.

### Pattern 3: Editor-Only Settings

```kain
@config(category: "Editor", display_name: "Editor Preferences")
struct EditorSettings:
    @setting(display_name: "Auto Save Interval", tooltip: "Minutes between auto-saves")
    auto_save_interval: Int = 5
    
    @setting(display_name: "Show Grid")
    show_grid: Bool = true
    
    @setting(display_name: "Grid Color")
    grid_color: String = "#808080"
```

**Use Case:** Editor-specific preferences.

### Pattern 4: Per-User Editor Settings

```kain
@config(category: "EditorPerProjectUserSettings")
struct UserPreferences:
    @setting(display_name: "Last Opened Level")
    last_level: String = "/Game/Maps/Default"
    
    @setting(display_name: "Camera Speed", min: 1.0, max: 10.0)
    camera_speed: Float = 5.0
```

**Use Case:** Per-user, per-project settings (not checked into source control).

### Pattern 5: Mixed Access Patterns

```kain
@config(category: "Game")
struct MixedSettings:
    // Blueprint + CVar + UI
    @setting(
        display_name: "Quality Level",
        tooltip: "Graphics quality (0=Low, 4=Ultra)",
        cvar: "game.Quality",
        blueprint: true,
        writable: true,
        min: 0,
        max: 4
    )
    quality: Int = 2
    
    // CVar only (no Blueprint)
    @setting(cvar: "game.DebugMode")
    debug_mode: Bool = false
    
    // UI only (no CVar, no Blueprint)
    @setting(display_name: "Server URL", tooltip: "Default server address")
    server_url: String = "localhost:7777"
```

**Use Case:** Different access patterns for different settings.

---

## Troubleshooting

### Common Issues

#### Issue 1: "No field `attributes` on type `TypeAlias`"

**Symptom:** Compilation error in `ue5` crate blocking build.

**Cause:** The `ue5` crate has compilation errors unrelated to `ue5-config`.

**Solution:** Fix the `ue5` crate errors first:
```rust
// In ue5/src/codegen_ue5.rs:738
// Change: a.ast.fields.len()
// To: a.ast.state.len()

// In ue5/src/codegen_ue5.rs:740
// Remove: .attributes.len() * 4
```

#### Issue 2: Bool .ini values not working

**Symptom:** Bool settings not loading from .ini file.

**Cause:** Using lowercase `"true"/"false"` instead of `"True"/"False"`.

**Solution:** Always use capital T/F:
```ini
[/Script/MyPlugin.MySettings]
EnableFeature=True  # Correct
# EnableFeature=true  # Wrong!
```

#### Issue 3: Console variable not registering

**Symptom:** CVar command not found in console.

**Cause:** Missing `cvar` attribute or incorrect naming.

**Solution:** Ensure `cvar` attribute is set:
```kain
@setting(cvar: "plugin.SettingName")  # Correct
@setting  # No CVar generated
```

#### Issue 4: Blueprint accessor not appearing

**Symptom:** Blueprint node not found.

**Cause:** Missing `blueprint: true` attribute.

**Solution:** Add `blueprint: true`:
```kain
@setting(blueprint: true)  # Generates Blueprint accessor
@setting  # No Blueprint accessor
```

#### Issue 5: Settings not appearing in Project Settings

**Symptom:** Settings class not visible in Editor.

**Cause:** Module not loaded or incorrect category.

**Solution:**
1. Ensure module is loaded in `.uplugin`
2. Check category is correct (`"Game"`, `"Engine"`, `"Editor"`)
3. Verify `GetContainerName()` returns `"Project"`

#### Issue 6: Min/max constraints not working

**Symptom:** Can enter values outside min/max range.

**Cause:** Constraints only work in Editor UI, not at runtime.

**Solution:** Add runtime validation if needed:
```cpp
void UMySettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // Runtime validation
    MyValue = FMath::Clamp(MyValue, MinValue, MaxValue);
}
```

### Debug Tips

1. **Check generated files:** Look at the actual .h/.cpp output to verify correctness
2. **Test in isolation:** Create a minimal test case with one setting
3. **Verify .ini format:** Check that .ini file has correct section and format
4. **Console commands:** Test CVars directly in UE5 console
5. **Blueprint debugging:** Use Print String to verify Blueprint accessor values

---

## Advanced Topics

### Custom .ini Sections

```kain
@config(
    category: "Game",
    section: "/Script/MyPlugin.CustomSection"
)
struct CustomSettings:
    value: Float = 1.0
```

Generates:
```ini
[/Script/MyPlugin.CustomSection]
value=1.0
```

### Custom .ini Files

```kain
@config(
    category: "Game",
    file: "CustomConfig.ini"
)
struct CustomFileSettings:
    value: Float = 1.0
```

Generates section in `CustomConfig.ini` instead of `DefaultGame.ini`.

### Nested Structs (Not Supported)

Currently, nested structs are not supported:

```kain
// NOT SUPPORTED
@config(category: "Game")
struct OuterSettings:
    @setting
    inner: InnerStruct  # Error: nested structs not supported
```

**Workaround:** Flatten the structure or use separate config structs.

### Arrays (Not Supported)

Currently, array types are not supported:

```kain
// NOT SUPPORTED
@config(category: "Game")
struct ArraySettings:
    @setting
    values: Array<Float>  # Error: arrays not supported
```

**Workaround:** Use multiple individual fields or implement custom serialization.

---

## Performance Considerations

### Compilation Time

- Config generation is fast (~1ms per struct)
- Minijinja template rendering is cached
- No significant impact on overall build time

### Runtime Performance

- Singleton pattern (`GetDefault<T>()`) is efficient
- Console variable lookups are O(1) hash map access
- Blueprint accessors have minimal overhead (static function call)

### Memory Usage

- One UDeveloperSettings instance per config struct (singleton)
- Console variables stored in global static storage
- Minimal memory footprint

---

## Future Enhancements

Potential future features (not yet implemented):

1. **Array support** - Config arrays with TArray<T>
2. **Nested struct support** - Hierarchical config structures
3. **Enum support** - Config enums with UENUM
4. **Validation callbacks** - Custom validation functions
5. **Migration support** - Automatic config migration between versions
6. **Localization** - Localized display names and tooltips
7. **Categories** - Custom UPROPERTY categories
8. **Advanced meta specifiers** - UIMin, UIMax, Delta, etc.

---

## Contributing

### Adding New Features

1. Update IR types in `config_ir.rs`
2. Update parser in `parser.rs`
3. Update codegen in `developer_settings_codegen.rs` (or other codegen files)
4. Add tests in appropriate test file
5. Update this documentation

### Code Style

- Follow Rust standard style (rustfmt)
- Use descriptive variable names
- Add doc comments to public APIs
- Write tests for new features

### Testing Requirements

- All new features must have unit tests
- Integration tests for end-to-end scenarios
- Test coverage should remain above 80%

---

## License

Part of the KAIN compiler project.

---

## See Also

- [IMPLEMENTATION_BLUEPRINT.md](IMPLEMENTATION_BLUEPRINT.md) - Implementation guide for developers
- [QUICK_START.md](QUICK_START.md) - Quick start guide
- [AGENT_COORDINATION.md](AGENT_COORDINATION.md) - Agent coordination document
- [Kain/crates/ue5/CRATE_REFERENCE.md](../ue5/CRATE_REFERENCE.md) - UE5 crate reference
- [Kain/crates/kain-core/](../kain-core/) - Core AST and parser

---

**Questions or Issues?** Check the troubleshooting section or create an issue in the KAIN repository.
