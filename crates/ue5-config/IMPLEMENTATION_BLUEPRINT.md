# UE5-CONFIG Crate Implementation Blueprint

> **Purpose:** Complete implementation guide for subagents to build the ue5-config crate independently  
> **Status:** Ready for parallel implementation  
> **Estimated Time:** 8-13 days across 3-5 subagents  
> **Constraint:** DO NOT touch any other crates - work only within `Kain/crates/ue5-config/`

---

## Overview

The `ue5-config` crate generates UE5 configuration systems from KAIN `@config` structs:
1. **UDeveloperSettings subclasses** — Runtime-accessible settings
2. **Config .ini files** — DefaultGame.ini, DefaultEngine.ini sections
3. **Console variables (CVars)** — Auto-registered with callbacks
4. **Project Settings UI** — Automatic Details panel
5. **Blueprint accessors** — Get/Set functions for BP access

**Compression Ratio:** 1:10+ (4 lines KAIN → 41+ lines C++/.ini)

---

## Reference Code Locations

### UE5 Patterns (DO NOT MODIFY - READ ONLY)
- `Research/ReferencePatterns/28_VoxelSystems/VoxelPluginPro/Source/Voxel/Public/VoxelSettings.h`
- `Research/ReferencePatterns/28_VoxelSystems/VoxelPluginPro/Source/Voxel/Private/VoxelSettings.cpp`
- `Research/ReferencePatterns/07_EditorExtensions/WidgetLauncher/Source/WidgetLauncher/Public/WidgetLauncherSettings.h`
- `Research/ReferencePatterns/07_EditorExtensions/WidgetLauncher/Source/WidgetLauncher/Private/WidgetLauncherSettings.cpp`

### Existing Crate Patterns (READ ONLY)
- `Kain/crates/ue5/` — Runtime codegen patterns
- `Kain/crates/ue5-editor/` — Editor codegen patterns
- `Kain/crates/kain-core/src/ast.rs` — AST types

### Console Variable Examples (READ ONLY)
- Search for `TAutoConsoleVariable` in `Research/ReferencePatterns/28_VoxelSystems/VoxelPluginPro/`

---

## KAIN Syntax Design

```kain
@config(category: "Game", file: "DefaultGame.ini", section: "MyPlugin")
struct VoxelSettings:
    @setting(
        display_name: "Chunk Size",
        tooltip: "Size of voxel chunks in world units",
        cvar: "voxel.ChunkSize",
        blueprint: true,
        min: 10.0,
        max: 1000.0
    )
    chunk_size: Float = 100.0
    
    @setting(
        display_name: "Max LOD Levels",
        cvar: "voxel.MaxLOD",
        blueprint: true,
        min: 1,
        max: 8
    )
    max_lod: Int = 4
    
    @setting(
        display_name: "Enable Debug Visualization",
        cvar: "voxel.DebugVis",
        blueprint: true
    )
    debug_vis: Bool = false
```

### Attribute Parameters

**@config attributes:**
- `category`: String — Config category ("Game", "Engine", "Editor", "EditorPerProjectUserSettings")
- `file`: String — Config file name (optional, defaults based on category)
- `section`: String — .ini section name (optional, defaults to plugin name)
- `display_name`: String — Display name in Project Settings (optional)

**@setting attributes:**
- `display_name`: String — UI display name
- `tooltip`: String — Tooltip text
- `cvar`: String — Console variable name (e.g., "voxel.ChunkSize")
- `blueprint`: Bool — Generate Blueprint accessor functions
- `min`: Float/Int — Minimum value (generates ClampMin meta)
- `max`: Float/Int — Maximum value (generates ClampMax meta)
- `writable`: Bool — Generate setter functions (default: false, read-only)

---

## Generated Code Examples

### Input KAIN
```kain
@config(category: "Game")
struct VoxelSettings:
    @setting(cvar: "voxel.ChunkSize", blueprint: true, min: 10.0, max: 1000.0)
    chunk_size: Float = 100.0
```

### Output: VoxelSettings.h
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

    UPROPERTY(Config, EditAnywhere, Category="Voxel", meta=(
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

### Output: VoxelSettings.cpp
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
    CategoryName = "Plugins";
    SectionName = "Voxel Settings";
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
    return "Project";
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

### Output: Config/DefaultGame.ini
```ini
[/Script/MyPlugin.VoxelSettings]
ChunkSize=100.0
```

---

## Crate Structure

```
ue5-config/
├── Cargo.toml
├── CRATE_REFERENCE.md
├── IMPLEMENTATION_BLUEPRINT.md (this file)
├── src/
│   ├── lib.rs                          # Public API
│   ├── config_ir.rs                    # IR types (ConfigStruct, ConfigField, CVar)
│   ├── parser.rs                       # Parse @config and @setting attributes
│   ├── developer_settings_codegen.rs   # UDeveloperSettings .h/.cpp generation
│   ├── ini_file_generator.rs           # .ini file generation
│   ├── cvar_codegen.rs                 # Console variable registration
│   ├── blueprint_accessor_codegen.rs   # Blueprint Get/Set functions
│   └── templates/                      # Minijinja templates
│       ├── developer_settings.h.jinja
│       ├── developer_settings.cpp.jinja
│       └── ini_section.jinja
└── tests/
    ├── config_ir_tests.rs              # IR type tests
    ├── parser_tests.rs                 # Attribute parsing tests
    ├── developer_settings_tests.rs     # UDeveloperSettings generation tests
    ├── ini_file_tests.rs               # .ini file format tests
    ├── cvar_tests.rs                   # CVar registration tests
    ├── blueprint_accessor_tests.rs     # Blueprint accessor tests
    └── integration_tests.rs            # End-to-end config tests
```

---

## Implementation Phases

### Phase 1: Core IR & Parser (Agent 1)
**Files:** `config_ir.rs`, `parser.rs`, `lib.rs`, `Cargo.toml`

**Tasks:**
1. Create `Cargo.toml` with dependencies (kain-core, ue5, heck, minijinja, serde, serde_json)
2. Define IR types in `config_ir.rs`:
   - `ConfigStruct` — Represents a @config struct
   - `ConfigField` — Represents a @setting field
   - `CVar` — Console variable metadata
   - `ConfigCategory` enum — Game, Engine, Editor, EditorPerProjectUserSettings
3. Implement attribute parser in `parser.rs`:
   - `parse_config_attribute()` — Extract @config params
   - `parse_setting_attribute()` — Extract @setting params
4. Create `lib.rs` with public API:
   - `pub fn generate_config_code(program: &Program, ctx: &Ue5Context) -> Result<Vec<GeneratedFile>>`
5. Write unit tests in `tests/config_ir_tests.rs` and `tests/parser_tests.rs`

**Acceptance Criteria:**
- [ ] IR types compile and have proper Debug/Clone/PartialEq derives
- [ ] Parser extracts all @config and @setting parameters correctly
- [ ] 10+ unit tests passing
- [ ] No dependencies on other crates except kain-core

---

### Phase 2: UDeveloperSettings Codegen (Agent 2)
**Files:** `developer_settings_codegen.rs`, `templates/developer_settings.h.jinja`, `templates/developer_settings.cpp.jinja`

**Tasks:**
1. Create `developer_settings_codegen.rs`:
   - `generate_developer_settings_header()` — Generate .h file
   - `generate_developer_settings_cpp()` — Generate .cpp file
   - Type mapping: KAIN → UE5 (Float → float, Int → int32, Bool → bool, String → FString)
   - UPROPERTY generation with Config, EditAnywhere, Category, meta specifiers
   - Constructor with default values
   - Singleton Get() method
   - GetContainerName() override
   - PostInitProperties() override
   - PostEditChangeProperty() override (WITH_EDITOR)
2. Create Minijinja templates:
   - `developer_settings.h.jinja` — Header template
   - `developer_settings.cpp.jinja` — Implementation template
3. Write tests in `tests/developer_settings_tests.rs`:
   - Test header generation
   - Test cpp generation
   - Test UPROPERTY meta specifiers (ClampMin, ClampMax, DisplayName, ToolTip)
   - Test constructor initialization
   - Test singleton accessor

**Acceptance Criteria:**
- [ ] Generates valid UDeveloperSettings .h/.cpp files
- [ ] UCLASS specifiers correct (Config=X, DefaultConfig, meta=(DisplayName=...))
- [ ] UPROPERTY specifiers correct (Config, EditAnywhere, Category, meta)
- [ ] Constructor initializes all fields with defaults
- [ ] Singleton Get() method works
- [ ] 15+ unit tests passing

**Reference Pattern:**
```cpp
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="My Settings"))
class MYPLUGIN_API UMySettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UMySettings();
    
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="10.0", ClampMax="1000.0"))
    float ChunkSize;
    
    static const UMySettings* Get();
    virtual FName GetContainerName() const override;
    virtual void PostInitProperties() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
```

---

### Phase 3: Console Variables & .ini Files (Agent 3)
**Files:** `cvar_codegen.rs`, `ini_file_generator.rs`, `templates/ini_section.jinja`

**Tasks:**
1. Create `cvar_codegen.rs`:
   - `generate_cvar_declarations()` — Generate TAutoConsoleVariable<T> declarations
   - `generate_cvar_callbacks()` — Generate OnXChanged() callback methods
   - Type mapping: Float → TAutoConsoleVariable<float>, Int → TAutoConsoleVariable<int32>, Bool → TAutoConsoleVariable<bool>
   - CVar naming: "plugin.FieldName" format
   - ECVF_Default flags
2. Create `ini_file_generator.rs`:
   - `generate_ini_section()` — Generate .ini file section
   - Section naming: [/Script/PluginName.ClassName]
   - Value formatting: Float → "100.0", Int → "4", Bool → "True"/"False"
3. Create template `ini_section.jinja`
4. Write tests:
   - `tests/cvar_tests.rs` — CVar generation tests
   - `tests/ini_file_tests.rs` — .ini format tests

**Acceptance Criteria:**
- [ ] Generates valid TAutoConsoleVariable declarations
- [ ] CVar names follow "plugin.FieldName" convention
- [ ] Callback methods sync CVar → UPROPERTY
- [ ] .ini sections have correct format
- [ ] Bool values use "True"/"False" (not "true"/"false")
- [ ] 10+ unit tests passing

**Reference Pattern:**
```cpp
static TAutoConsoleVariable<float> CVarChunkSize(
    TEXT("voxel.ChunkSize"),
    100.0f,
    TEXT("Size of voxel chunks"),
    ECVF_Default
);

void UVoxelSettings::OnChunkSizeChanged()
{
    ChunkSize = CVarChunkSize.GetValueOnGameThread();
}
```

---

### Phase 4: Blueprint Integration (Agent 4)
**Files:** `blueprint_accessor_codegen.rs`

**Tasks:**
1. Create `blueprint_accessor_codegen.rs`:
   - `generate_blueprint_getters()` — Generate UFUNCTION(BlueprintCallable) static getters
   - `generate_blueprint_setters()` — Generate setters (if @setting(writable: true))
   - Category: "{StructName} Settings"
   - Return types: float, int32, bool, FString
2. Write tests in `tests/blueprint_accessor_tests.rs`:
   - Test getter generation
   - Test setter generation (when writable: true)
   - Test UFUNCTION specifiers
   - Test category naming

**Acceptance Criteria:**
- [ ] Generates valid UFUNCTION(BlueprintCallable) methods
- [ ] Getters are static and call Get()->FieldName
- [ ] Setters modify the CDO (if writable: true)
- [ ] Category matches struct name
- [ ] 8+ unit tests passing

**Reference Pattern:**
```cpp
UFUNCTION(BlueprintCallable, Category="Voxel Settings")
static float GetChunkSize()
{
    return Get()->ChunkSize;
}

UFUNCTION(BlueprintCallable, Category="Voxel Settings")
static void SetChunkSize(float NewValue)
{
    UVoxelSettings* Settings = GetMutableDefault<UVoxelSettings>();
    Settings->ChunkSize = NewValue;
    Settings->SaveConfig();
}
```

---

### Phase 5: Integration & Testing (Agent 5)
**Files:** `tests/integration_tests.rs`, `CRATE_REFERENCE.md`

**Tasks:**
1. Write integration tests in `tests/integration_tests.rs`:
   - End-to-end test: KAIN → .h/.cpp/.ini
   - Test multiple settings in one struct
   - Test different config categories (Game, Engine, Editor)
   - Test all attribute combinations
   - Test type mapping (Float, Int, Bool, String)
2. Create `CRATE_REFERENCE.md`:
   - Overview
   - KAIN syntax reference
   - Attribute reference
   - Generated code examples
   - Integration with other crates
   - Usage examples
3. Run all tests across all phases
4. Fix any integration issues

**Acceptance Criteria:**
- [ ] 10+ integration tests passing
- [ ] All unit tests passing (50+ total)
- [ ] CRATE_REFERENCE.md complete
- [ ] No compilation errors
- [ ] No clippy warnings

---

## Type Mapping Reference

| KAIN Type | UE5 C++ Type | CVar Type | .ini Format |
|-----------|--------------|-----------|-------------|
| Float | float | TAutoConsoleVariable<float> | "100.0" |
| Int | int32 | TAutoConsoleVariable<int32> | "4" |
| Bool | bool | TAutoConsoleVariable<bool> | "True"/"False" |
| String | FString | TAutoConsoleVariable<FString> | "MyString" |

---

## Config Category Mapping

| KAIN Category | UCLASS Config | Default .ini File |
|---------------|---------------|-------------------|
| "Game" | Config=Game | DefaultGame.ini |
| "Engine" | Config=Engine | DefaultEngine.ini |
| "Editor" | Config=Editor | DefaultEditor.ini |
| "EditorPerProjectUserSettings" | Config=EditorPerProjectUserSettings | DefaultEditorPerProjectUserSettings.ini |

---

## Naming Conventions

### UE5 Class Names
- KAIN: `@config struct VoxelSettings`
- UE5: `UVoxelSettings` (U prefix auto-added)

### Console Variable Names
- Pattern: `{plugin_name}.{FieldName}`
- Example: `voxel.ChunkSize`, `narrative.DialogueSpeed`
- Use PascalCase for field names

### .ini Section Names
- Pattern: `[/Script/{PluginName}.{ClassName}]`
- Example: `[/Script/MyPlugin.VoxelSettings]`

### Blueprint Category Names
- Pattern: `{StructName} Settings`
- Example: `Voxel Settings`, `Narrative Settings`

---

## Dependencies (Cargo.toml)

```toml
[package]
name = "ue5-config"
version = "0.1.0"
edition = "2021"

[dependencies]
kain-core = { path = "../kain-core" }
ue5 = { path = "../ue5" }
heck = { workspace = true }
minijinja = { workspace = true }
serde = { workspace = true }
serde_json = { workspace = true }
anyhow = "1.0"
thiserror = "1.0"

[dev-dependencies]
tempfile = "3.8"
```

---

## Testing Strategy

### Unit Tests (Per Phase)
- Test IR types (config_ir_tests.rs)
- Test attribute parsing (parser_tests.rs)
- Test UDeveloperSettings generation (developer_settings_tests.rs)
- Test .ini generation (ini_file_tests.rs)
- Test CVar generation (cvar_tests.rs)
- Test Blueprint accessor generation (blueprint_accessor_tests.rs)

### Integration Tests (Phase 5)
- End-to-end KAIN → C++/.ini
- Multiple settings per struct
- All config categories
- All attribute combinations
- Type mapping validation

### Test Coverage Goals
- 50+ total tests
- 100% of public API covered
- All edge cases covered (empty structs, no CVars, no Blueprint, etc.)

---

## Common Pitfalls to Avoid

1. **Bool .ini format:** Use "True"/"False" (capital T/F), not "true"/"false"
2. **UPROPERTY order:** Config must come before EditAnywhere
3. **CVar naming:** Use PascalCase for field names (ChunkSize, not chunk_size)
4. **Singleton pattern:** Use GetDefault<T>(), not new instances
5. **WITH_EDITOR guards:** PostEditChangeProperty must be guarded
6. **ImportConsoleVariableValues():** Call in PostInitProperties if IsTemplate()
7. **ExportValuesToConsoleVariables():** Call in PostEditChangeProperty
8. **GetContainerName():** Return "Project" for project settings, "Editor" for editor settings

---

## Success Metrics

### Quantitative
- [ ] 50+ unit tests passing
- [ ] 10+ integration tests passing
- [ ] 0 compilation errors
- [ ] 0 clippy warnings
- [ ] 1:10+ compression ratio maintained

### Qualitative
- [ ] Generates valid UDeveloperSettings classes
- [ ] Console commands work (`voxel.ChunkSize 200`)
- [ ] .ini files have correct format
- [ ] Blueprint nodes would appear (can't test without UE5)
- [ ] Code matches reference patterns
- [ ] CRATE_REFERENCE.md is complete

---

## Agent Coordination

### Agent 1 (Phase 1): Core IR & Parser
- **Blocks:** None
- **Blocked by:** None
- **Output:** IR types, parser, lib.rs skeleton

### Agent 2 (Phase 2): UDeveloperSettings Codegen
- **Blocks:** Agent 5 (integration tests need this)
- **Blocked by:** Agent 1 (needs IR types)
- **Output:** .h/.cpp generation, templates

### Agent 3 (Phase 3): CVars & .ini Files
- **Blocks:** Agent 5 (integration tests need this)
- **Blocked by:** Agent 1 (needs IR types)
- **Output:** CVar generation, .ini generation

### Agent 4 (Phase 4): Blueprint Integration
- **Blocks:** Agent 5 (integration tests need this)
- **Blocked by:** Agent 1 (needs IR types)
- **Output:** Blueprint accessor generation

### Agent 5 (Phase 5): Integration & Testing
- **Blocks:** None (final phase)
- **Blocked by:** Agents 2, 3, 4 (needs all codegen complete)
- **Output:** Integration tests, CRATE_REFERENCE.md

### Parallel Execution Strategy
1. **Start:** Agent 1 (Phase 1)
2. **After Agent 1 completes:** Start Agents 2, 3, 4 in parallel (Phases 2, 3, 4)
3. **After Agents 2, 3, 4 complete:** Start Agent 5 (Phase 5)

**Total Time:** ~8-13 days (2-3 days Phase 1, 4-6 days Phases 2-4 parallel, 2-4 days Phase 5)

---

## Final Checklist

Before marking complete:
- [ ] All 50+ tests passing
- [ ] Cargo build succeeds
- [ ] Cargo clippy passes with no warnings
- [ ] CRATE_REFERENCE.md complete
- [ ] All generated code matches reference patterns
- [ ] No dependencies on crates other than kain-core and ue5
- [ ] No modifications to any other crates
- [ ] All files have proper module documentation
- [ ] All public functions have doc comments

---

## Questions for Main Developer

If you encounter any ambiguities:
1. Check reference code in `Research/ReferencePatterns/`
2. Check existing crate patterns in `Kain/crates/ue5/` and `Kain/crates/ue5-editor/`
3. Document the question in a `QUESTIONS.md` file in the crate root
4. Make a reasonable decision based on existing patterns
5. Add a TODO comment with the question

**DO NOT:**
- Modify any other crates
- Add dependencies outside the approved list
- Change the public API without documenting why
- Skip tests to save time

---

## Status Tracking

Each agent should update this section when completing their phase:

- [ ] Phase 1: Core IR & Parser (Agent 1) — NOT STARTED
- [ ] Phase 2: UDeveloperSettings Codegen (Agent 2) — NOT STARTED
- [ ] Phase 3: CVars & .ini Files (Agent 3) — NOT STARTED
- [ ] Phase 4: Blueprint Integration (Agent 4) — NOT STARTED
- [ ] Phase 5: Integration & Testing (Agent 5) — NOT STARTED

---

**Ready for implementation. Good luck, agents!**
