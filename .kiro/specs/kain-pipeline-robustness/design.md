# Design Document: KAIN Pipeline Robustness

## Overview

This design systematically hardens the KAIN UE5 codegen pipeline from a working prototype to production-quality compiler infrastructure. The pipeline currently generates functional UE5 plugins but contains fragile areas that cause crashes on invalid input, produce incorrect code for edge cases, and lack comprehensive validation. This design addresses these issues through centralized error handling, unified type mapping, complete validation coverage, and data-driven rule systems.

The core philosophy is: **If `kain build --ue5` succeeds, the plugin MUST compile in UE5 on the first try.** All errors must be caught during KAIN compilation with clear, actionable messages that LLMs can fix immediately.

### Current State

The pipeline consists of four Rust crates:
- **cli/packager**: Orchestrates builds, parses sources, dispatches to codegen crates
- **ue5 (runtime)**: Generates actors, components, structs, enums, delegates
- **ue5-editor**: Generates Slate UI, Details panels, Viewports, Asset Editors
- **ue5-shaders**: Generates HLSL .usf files and C++ shader bindings

Shared state flows through `Ue5Context` which contains `EngineKnowledge` (database of UE5 types), type registries, and naming conventions.

### Metadata-First Architecture (CRITICAL)

**The KAIN compiler is built on a metadata-first foundation.** All UE5 knowledge is loaded from JSON files in `unreal/metadata/` rather than hardcoded. This enables:
- Zero-recompilation updates for new UE5 versions
- Data-driven validation rules
- Multi-version UE5 support (5.4, 5.5, 5.6, 5.7)
- LLM-friendly knowledge base (JSON is easier to query than Rust code)

**Existing Metadata Infrastructure:**

The system already has 14 metadata files fully wired into `Ue5Context`:

1. **engine_knowledge.json** - Core UE5 types, constructors, includes, named colors
2. **engine_knowledge_expanded.json** - Extended type information
3. **engine_5.4_scanned.json** - UE5 5.4 specific types
4. **engine_5.5_scanned.json** - UE5 5.5 specific types
5. **engine_5.6_scanned.json** - UE5 5.6 specific types
6. **engine_5.7_scanned.json** - UE5 5.7 specific types
7. **module_graph.json** - Module dependencies and include-to-module mappings
8. **uht_rules.json** - Unreal Header Tool validation rules
9. **shader_knowledge.json** - HLSL types, keywords, binding rules
10. **widget_registry.json** - Slate widget types and property mappings
11. **editor_attributes.json** - Editor attribute definitions (@slider, @color_picker, etc.)
12. **virtual_obligations.json** - Virtual function requirements for UE5 classes
13. **codegen_rules.json** - Code generation rules and patterns
14. **5.4.json** - Version-specific configuration

**Existing Extraction Scripts:**

Python scripts in `unreal/scripts/` generate/update metadata:

1. **ue5_scanner.py** - Scans UE5 installations for types (multi-drive support: D:, M:, etc.)
2. **module_graph_extractor.py** - Extracts module dependency graph
3. **uht_extractor.py** - Extracts UHT validation rules
4. **shader_extractor.py** - Extracts shader type information
5. **editor_attributes_extractor.py** - Extracts editor attribute definitions
6. **virtual_obligations_extractor.py** - Extracts virtual function requirements
7. **corpus_extractor.py** - Extracts code corpus for analysis
8. **verify_scan.py** - Validates metadata completeness

**Design Principle: Query Metadata First, Hardcode Never**

Every codegen decision should follow this pattern:
```rust
// ❌ BAD: Hardcoded knowledge
fn is_uobject_type(ty: &str) -> bool {
    matches!(ty, "AActor" | "UActorComponent" | "UObject")
}

// ✅ GOOD: Query metadata
fn is_uobject_type(&self, ty: &str) -> bool {
    self.knowledge.is_uobject_derived(ty)
}
```

This robustness spec must emphasize expanding and validating the metadata system, not replacing it with hardcoded logic.

### Problem Areas

1. **Error Handling**: 20+ `.unwrap()` calls in packager that crash instead of returning structured errors
2. **Type Mapping**: Logic split across 3 locations causing double-prefixing bugs (EEHealthStatus)
3. **Validation**: Oracle has incomplete rules, no shader validation, no cross-file checks
4. **Shader Pipeline**: POD struct redefinitions, permutation bugs, HLSL keyword collisions
5. **Editor Codegen**: Multiple TODO stubs in asset editors, viewports, details generation
6. **Module Dependencies**: Relies on optional JSON, no validation of .Build.cs correctness
7. **Naming Edge Cases**: Doesn't handle already-prefixed names, numbers, special characters
8. **Post-Processing**: Only fixes basic issues, doesn't handle replication/shaders/forward decls
9. **Metadata Validation**: No schema validation on load, incomplete coverage in metadata files
10. **Metadata Extraction**: Scripts don't handle multi-drive UE5 installations consistently

### Design Goals

1. Zero crashes on invalid input - all errors are graceful with file:line:column context
2. Zero double-prefixing bugs through centralized type mapping
3. Complete validation coverage catching all UE5 compilation errors before codegen
4. Production-ready shader pipeline with comprehensive validation
5. Complete editor codegen with no TODO stubs
6. Automatic module dependency resolution with validation
7. Robust naming handling all edge cases
8. Enhanced post-processing for replication, shaders, forward declarations
9. 100+ tests covering edge cases and error paths
10. Data-driven validation rules loaded from JSON

## Architecture

### Metadata Loading Architecture

All metadata files are loaded during `Ue5Context` initialization:

```rust
pub struct Ue5Context {
    pub knowledge: Arc<EngineKnowledge>,
    pub module_graph: ModuleGraph,
    pub uht_rules: UhtRules,
    pub shader_knowledge: ShaderKnowledge,
    pub widget_registry: WidgetRegistry,
    pub editor_attributes: EditorAttributes,
    pub virtual_obligations: VirtualObligations,
    // ... other fields
}

impl Ue5Context {
    pub fn new() -> KainResult<Self> {
        // Load and validate all metadata files
        let knowledge = EngineKnowledge::load("unreal/metadata/engine_knowledge.json")
            .with_context("Loading engine knowledge")?;
        
        let module_graph = ModuleGraph::load("unreal/metadata/module_graph.json")
            .with_context("Loading module graph")?;
        
        let uht_rules = UhtRules::load("unreal/metadata/uht_rules.json")
            .with_context("Loading UHT rules")?;
        
        // ... load other metadata files
        
        Ok(Self {
            knowledge: Arc::new(knowledge),
            module_graph,
            uht_rules,
            // ... other fields
        })
    }
}
```

Each metadata loader validates the JSON schema before use:

```rust
impl EngineKnowledge {
    pub fn load(path: &str) -> KainResult<Self> {
        let content = fs::read_to_string(path)
            .map_err(|e| KainError::io(e))
            .with_file(path.into())
            .with_context("Reading engine knowledge file")?;
        
        let schema = Self::get_schema();
        let instance: serde_json::Value = serde_json::from_str(&content)
            .map_err(|e| KainError::config(e.to_string()))
            .with_file(path.into())
            .with_context("Parsing engine knowledge JSON")?;
        
        // Validate against schema
        schema.validate(&instance)
            .map_err(|e| KainError::config(format!("Schema validation failed: {}", e)))
            .with_file(path.into())?;
        
        // Deserialize to struct
        let knowledge: Self = serde_json::from_value(instance)
            .map_err(|e| KainError::config(e.to_string()))
            .with_file(path.into())?;
        
        Ok(knowledge)
    }
}
```

### High-Level Flow

```
.kn sources → Parser (kain-core) → AST → Type Checker → Oracle Validator
    ↓
Packager (cli):
  - Reads KAIN.toml
  - Merges ASTs into TypedProgram
  - Runs Oracle validation
  - Dispatches to codegen crates
    ↓
Runtime Codegen (ue5):
  - Generates actors, components, structs, enums
  - Uses Ue5Context for type mapping
  - Returns Result<Output, KainError>
    ↓
Editor Codegen (ue5-editor):
  - Generates Slate, Details, Viewports, Asset Editors
  - Receives Ue5Context from runtime
  - Returns Result<Output, KainError>
    ↓
Shader Codegen (ue5-shaders):
  - Generates .usf HLSL + C++ bindings
  - Standalone validation
  - Returns Result<Artifacts, KainError>
    ↓
Packager:
  - Writes all files
  - Generates .uplugin, .Build.cs
  - Runs post-processor
  - Returns Result<(), KainError>
```

### Error Handling Architecture

All functions return `KainResult<T>` which is `Result<T, KainError>`. Errors contain:
- **File path**: Which .kn file caused the error
- **Location**: Line and column numbers
- **Context**: What operation was being performed
- **Message**: Clear description of the problem
- **Suggestion**: How to fix it (when possible)

```rust
pub struct KainError {
    pub kind: ErrorKind,
    pub file: Option<PathBuf>,
    pub location: Option<(usize, usize)>, // (line, column)
    pub context: String,
    pub message: String,
    pub suggestion: Option<String>,
}

pub enum ErrorKind {
    Parse,
    Type,
    Validation,
    Codegen,
    Io,
    Config,
}
```

### Type Mapping Architecture

All type mapping goes through `types.rs` as the single source of truth:

```rust
pub struct TypeMapper {
    config: TypeMapConfig,
    knowledge: Arc<EngineKnowledge>,
    type_registry: TypeRegistry,
}

impl TypeMapper {
    pub fn map_type(&self, ty: &Type) -> KainResult<String> {
        // 1. Check primitives (Int, Float, Bool, String)
        // 2. Check EngineKnowledge for engine types
        // 3. Check TypeRegistry for user types
        // 4. Apply correct prefix based on type kind
        // 5. Handle generics recursively
        // 6. Return fully qualified C++ type
    }
    
    pub fn is_pointer_type(&self, ty: &Type) -> bool {
        // Centralized logic for when to append *
    }
    
    pub fn needs_forward_decl(&self, ty: &Type) -> bool {
        // Determines if type needs forward declaration
    }
}
```

### Validation Architecture

Three-phase validation system:

**Phase 1: Syntax Validation** (kain-core parser)
- Catches malformed KAIN syntax
- Returns parse errors with file:line:col

**Phase 2: Semantic Validation** (Oracle)
- Validates UE5-specific rules
- Checks type compatibility
- Verifies attribute combinations
- Returns structured validation errors

**Phase 3: Codegen Validation** (per-crate)
- Validates generated code structure
- Checks for name collisions
- Verifies include dependencies
- Returns codegen errors

```rust
pub struct Oracle {
    knowledge: Arc<EngineKnowledge>,
    uht_rules: UhtRules,
    shader_rules: ShaderRules,
    custom_rules: Vec<ValidationRule>,
}

impl Oracle {
    pub fn validate(&self, program: &TypedProgram) -> KainResult<ValidationReport> {
        let mut report = ValidationReport::new();
        
        // Run all validation passes
        self.validate_types(&mut report, program)?;
        self.validate_attributes(&mut report, program)?;
        self.validate_rpcs(&mut report, program)?;
        self.validate_replication(&mut report, program)?;
        self.validate_shaders(&mut report, program)?;
        self.validate_editor_features(&mut report, program)?;
        self.validate_cross_file(&mut report, program)?;
        
        if report.has_errors() {
            Err(KainError::validation(report))
        } else {
            Ok(report)
        }
    }
}
```

## Components and Interfaces

### 1. Error Handling System

**Location**: `crates/cli/src/error.rs` (enhanced)

**Interface**:
```rust
pub trait ErrorContext {
    fn with_file(self, path: PathBuf) -> Self;
    fn with_location(self, line: usize, col: usize) -> Self;
    fn with_context(self, ctx: String) -> Self;
    fn with_suggestion(self, suggestion: String) -> Self;
}

impl<T> ErrorContext for Result<T, KainError> {
    // Implementation for chaining context
}
```

**Usage Pattern**:
```rust
// Before (crashes):
let config = toml::from_str(&content).unwrap();

// After (graceful):
let config = toml::from_str(&content)
    .map_err(|e| KainError::config(e.to_string()))
    .with_file(config_path.clone())
    .with_context("Parsing KAIN.toml")
    .with_suggestion("Check TOML syntax at the reported line")?;
```

### 2. Centralized Type Mapper

**Location**: `crates/ue5/src/ue5/types.rs` (refactored)

**Interface**:
```rust
pub struct TypeMapper {
    config: TypeMapConfig,
    knowledge: Arc<EngineKnowledge>,
    registry: TypeRegistry,
}

impl TypeMapper {
    pub fn new(knowledge: Arc<EngineKnowledge>) -> Self;
    
    pub fn register_enum(&mut self, name: String);
    pub fn register_struct(&mut self, name: String);
    pub fn register_actor(&mut self, name: String);
    pub fn register_component(&mut self, name: String);
    pub fn register_delegate(&mut self, name: String);
    
    pub fn map_type(&self, ty: &Type) -> KainResult<MappedType>;
    pub fn is_pointer_type(&self, ty: &Type) -> bool;
    pub fn needs_forward_decl(&self, ty: &Type) -> bool;
    pub fn get_include_path(&self, ty: &Type) -> Option<String>;
}

pub struct MappedType {
    pub cpp_type: String,
    pub is_pointer: bool,
    pub needs_forward_decl: bool,
    pub include_path: Option<String>,
    pub prefix: Option<String>, // A, F, E, U, S
}
```

**Migration Strategy**:
1. Create new `TypeMapper` struct
2. Move all type mapping logic from packager/codegen into it
3. Update all call sites to use `TypeMapper::map_type()`
4. Remove duplicate type mapping code
5. Add tests for all edge cases

### 3. Enhanced Oracle Validator

**Location**: `crates/ue5/src/ue5/oracle.rs` (enhanced)

**New Validation Rules**:

```rust
impl Oracle {
    // Requirement 3: Complete Oracle Validation
    
    fn validate_replication(&self, ctx: &mut ValidationContext, program: &TypedProgram) {
        // Check all replicated properties have GetLifetimeReplicatedProps
        // Verify RPC naming conventions
        // Validate replicated types are serializable
    }
    
    fn validate_rpcs(&self, ctx: &mut ValidationContext, program: &TypedProgram) {
        // Server_* must be on server
        // Client_* must be on client
        // Multicast_* broadcasts to all
        // No delegate parameters in RPCs
    }
    
    fn validate_datatables(&self, ctx: &mut ValidationContext, program: &TypedProgram) {
        // All fields must be UE5-serializable
        // No pointers in datatable structs
        // Must inherit from FTableRowBase
    }
    
    fn validate_components(&self, ctx: &mut ValidationContext, program: &TypedProgram) {
        // No actor-only features
        // Proper component lifecycle
    }
    
    fn validate_name_collisions(&self, ctx: &mut ValidationContext, program: &TypedProgram) {
        // Check against EngineKnowledge
        // Check for C++ keywords
        // Check for UE5 macro names
    }
    
    fn validate_circular_dependencies(&self, ctx: &mut ValidationContext, program: &TypedProgram) {
        // Build dependency graph
        // Detect cycles
        // Suggest forward declarations
    }
}
```

### 4. Shader Validation System

**Location**: `crates/ue5-shaders/src/validation.rs` (new)

**Interface**:
```rust
pub struct ShaderValidator {
    hlsl_keywords: HashSet<String>,
    reserved_bindings: HashMap<String, usize>,
}

impl ShaderValidator {
    pub fn validate_shader(&self, shader: &TypedShader) -> KainResult<()> {
        self.validate_uniforms(shader)?;
        self.validate_permutations(shader)?;
        self.validate_pod_structs(shader)?;
        self.validate_hlsl_syntax(shader)?;
        self.validate_bindings(shader)?;
        Ok(())
    }
    
    fn validate_uniforms(&self, shader: &TypedShader) -> KainResult<()> {
        // Check unique binding slots
        // Verify types are HLSL-compatible
        // Validate permutation naming (CFG_*, ENABLE_*)
    }
    
    fn validate_pod_structs(&self, shader: &TypedShader) -> KainResult<()> {
        // Check for redefinitions
        // Verify field types
        // Validate alignment
    }
    
    fn validate_hlsl_syntax(&self, shader: &TypedShader) -> KainResult<()> {
        // Check for C++ keywords used as HLSL identifiers
        // Validate function signatures
        // Check return types
    }
}
```

### 5. Module Dependency Resolver

**Location**: `crates/cli/src/packager/dependencies.rs` (new)

**Interface**:
```rust
pub struct DependencyResolver {
    knowledge: Arc<EngineKnowledge>,
    module_map: HashMap<String, Vec<String>>, // include -> modules
}

impl DependencyResolver {
    pub fn analyze(&self, generated_files: &[GeneratedFile]) -> KainResult<Dependencies> {
        let mut deps = Dependencies::new();
        
        for file in generated_files {
            // Parse #include statements
            // Map to UE5 modules
            // Detect circular dependencies
            // Add to dependency set
        }
        
        self.validate_dependencies(&deps)?;
        Ok(deps)
    }
    
    fn validate_dependencies(&self, deps: &Dependencies) -> KainResult<()> {
        // Check for circular module dependencies
        // Verify all modules exist
        // Warn about missing optional modules
    }
}

pub struct Dependencies {
    pub public_modules: Vec<String>,
    pub private_modules: Vec<String>,
    pub circular_deps: Vec<(String, String)>,
}
```

### 6. Enhanced Post-Processor

**Location**: `crates/cli/src/packager/post_process.rs` (enhanced)

**New Capabilities**:
```rust
pub struct PostProcessor {
    fixes: Vec<Box<dyn CodeFix>>,
}

trait CodeFix {
    fn name(&self) -> &str;
    fn apply(&self, code: &str) -> KainResult<String>;
}

// New fixes:
struct ReplicationFix;      // Adds GetLifetimeReplicatedProps
struct ShaderInitFix;       // Adds shader initialization in BeginPlay
struct ForwardDeclFix;      // Adds missing forward declarations
struct IncludeOrderFix;     // Reorders includes to UE5 conventions
struct IndentationFix;      // Normalizes to tabs
struct LineEndingFix;       // Normalizes to LF
```

## Data Models

### TypeRegistry

```rust
pub struct TypeRegistry {
    enums: HashMap<String, EnumInfo>,
    structs: HashMap<String, StructInfo>,
    actors: HashMap<String, ActorInfo>,
    components: HashMap<String, ComponentInfo>,
    delegates: HashMap<String, DelegateInfo>,
}

pub struct EnumInfo {
    pub name: String,
    pub cpp_name: String,  // E-prefixed
    pub variants: Vec<String>,
    pub file: PathBuf,
}

pub struct StructInfo {
    pub name: String,
    pub cpp_name: String,  // F-prefixed
    pub fields: Vec<FieldInfo>,
    pub is_datatable: bool,
    pub is_slate: bool,
    pub file: PathBuf,
}

pub struct ActorInfo {
    pub name: String,
    pub cpp_name: String,  // A-prefixed
    pub state: Vec<FieldInfo>,
    pub rpcs: Vec<RpcInfo>,
    pub file: PathBuf,
}

pub struct ComponentInfo {
    pub name: String,
    pub cpp_name: String,  // U-prefixed + Component suffix
    pub state: Vec<FieldInfo>,
    pub file: PathBuf,
}

pub struct DelegateInfo {
    pub name: String,
    pub cpp_name: String,  // F-prefixed
    pub params: Vec<Type>,
    pub file: PathBuf,
}
```

### ValidationRule (Data-Driven)

```rust
pub struct ValidationRule {
    pub id: String,
    pub category: RuleCategory,
    pub severity: Severity,
    pub condition: RuleCondition,
    pub message: String,
    pub suggestion: Option<String>,
}

pub enum RuleCategory {
    Naming,
    TypeCompatibility,
    AttributeCombination,
    Replication,
    Blueprint,
    Shader,
    Editor,
}

pub enum Severity {
    Error,
    Warning,
    Info,
}

pub enum RuleCondition {
    TypeCollision { type_name: String },
    IncompatibleAttributes { attr1: String, attr2: String },
    InvalidRpcNaming { pattern: String },
    NestedContainer { outer: String, inner: String },
    // ... extensible
}
```

### validation_rules.json Format

```json
{
  "rules": [
    {
      "id": "no_nested_containers",
      "category": "TypeCompatibility",
      "severity": "Error",
      "condition": {
        "type": "NestedContainer",
        "outer": ["TArray", "TMap", "TSet"],
        "inner": ["TArray", "TMap", "TSet"]
      },
      "message": "Nested containers are not supported by UHT",
      "suggestion": "Use a wrapper struct instead"
    },
    {
      "id": "rpc_naming_convention",
      "category": "Replication",
      "severity": "Error",
      "condition": {
        "type": "InvalidRpcNaming",
        "pattern": "^(Server_|Client_|Multicast_)"
      },
      "message": "RPC functions must start with Server_, Client_, or Multicast_",
      "suggestion": "Rename function to follow RPC naming convention"
    }
  ]
}
```

## Testing Strategy

### Unit Tests

Focus on specific examples and edge cases:

**Type Mapping Tests** (`crates/ue5/src/ue5/types_test.rs`):
- Already-prefixed names (EHealthStatus → EHealthStatus, not EEHealthStatus)
- Names with numbers (Player2 → APlayer2)
- Names with underscores (health_component → UHealthComponent)
- Invalid names (starting with number, special chars)
- C++ keywords (class, struct, enum)
- UE5 macro names (UPROPERTY, UFUNCTION)

**Oracle Validation Tests** (`crates/ue5/src/ue5/oracle_test.rs`):
- Replicated properties without GetLifetimeReplicatedProps
- Invalid RPC naming
- Delegate parameters in RPCs
- BlueprintImplementableEvent + replication
- Nested containers
- Type collisions with engine types

**Shader Validation Tests** (`crates/ue5-shaders/src/validation_test.rs`):
- Duplicate binding slots
- POD struct redefinitions
- HLSL keyword collisions
- Invalid permutation names
- Missing uniform bindings

**Naming Tests** (`crates/ue5/src/ue5/naming_test.rs`):
- All prefix edge cases
- PascalCase conversion
- snake_case conversion
- Consecutive capitals (HTTPServer → HttpServer)

### Property-Based Tests

Verify universal properties across all inputs:

**Property Tests** (`crates/ue5/tests/property_tests.rs`):
- For any valid KAIN type, mapping never double-prefixes
- For any type name, collision detection is consistent
- For any shader, binding slots are unique
- For any RPC, naming convention is enforced
- For any replicated property, validation catches missing GetLifetimeReplicatedProps

### Integration Tests

Test end-to-end pipeline:

**Pipeline Tests** (`crates/cli/tests/integration_tests.rs`):
- Build ultimate.kn (comprehensive test plugin)
- Build with intentional errors, verify error messages
- Build with edge cases, verify correct output
- Build with all features, verify no TODO stubs

### Regression Tests

Snapshot testing for non-breaking changes:

**Snapshot Tests** (`crates/cli/tests/snapshots/`):
- Generate code for known-good .kn files
- Compare output to saved snapshots
- Fail if output changes unexpectedly
- Update snapshots when intentional changes made

### Test Coverage Goals

- Unit tests: 100+ tests covering all edge cases
- Property tests: 20+ properties covering universal rules
- Integration tests: 10+ end-to-end scenarios
- Regression tests: Snapshots for all example plugins
- Total: 150+ tests, 90%+ code coverage

## Error Handling

### Error Categories

1. **Parse Errors**: Malformed KAIN syntax
2. **Type Errors**: Type mismatches, unknown types
3. **Validation Errors**: UE5 semantic rule violations
4. **Codegen Errors**: Failed to generate valid C++
5. **IO Errors**: File system operations
6. **Config Errors**: Invalid KAIN.toml or JSON configs

### Error Message Format

```
❌ [ERROR_CATEGORY] Error in file.kn:42:15

   42 |     state health: EHealthStatus = EHealthStatus::Healthy
      |               ^^^^^^^^^^^^^^^
      |
   Type collision: 'EHealthStatus' collides with engine type 'EHealthStatus'.
   
   Help: Rename to something more specific:
         - MyHealthStatus
         - CustomHealthStatus
         - GameHealthStatus
   
   Note: UHT will reject this with "shares engine name" error.
```

### Error Recovery

When multiple errors exist:
1. Collect all errors (don't stop at first)
2. Group by file and category
3. Sort by severity (errors first, then warnings)
4. Display with context and suggestions
5. Return non-zero exit code

### Graceful Degradation

When non-critical errors occur:
1. Log warning with context
2. Continue processing
3. Mark affected output as potentially incomplete
4. Include warnings in final report

## Implementation Plan

The implementation is broken into discrete tasks in `tasks.md`. Key phases:

**Phase 1: Error Handling Foundation** (Tasks 1-3)
- Replace all `.unwrap()` with proper error handling
- Implement `ErrorContext` trait
- Add file:line:col tracking

**Phase 2: Type Mapping Centralization** (Tasks 4-6)
- Create `TypeMapper` struct
- Migrate all type mapping logic
- Remove duplicate code

**Phase 3: Oracle Enhancement** (Tasks 7-10)
- Implement missing validation rules
- Add data-driven rule loading
- Complete cross-file validation

**Phase 4: Shader Pipeline** (Tasks 11-13)
- Add shader validation layer
- Fix POD struct handling
- Implement virtual path resolution

**Phase 5: Editor Codegen** (Tasks 14-16)
- Complete TODO implementations
- Add editor validation
- Test all editor features

**Phase 6: Module Dependencies** (Tasks 17-18)
- Implement dependency resolver
- Validate .Build.cs generation
- Add circular dependency detection

**Phase 7: Naming & Post-Processing** (Tasks 19-21)
- Harden naming edge cases
- Enhance post-processor
- Add replication/shader fixes

**Phase 8: Testing** (Tasks 22-25)
- Write unit tests (100+)
- Write property tests (20+)
- Add integration tests
- Set up snapshot testing

**Phase 9: Data-Driven Rules** (Tasks 26-27)
- Implement rule loading system
- Create validation_rules.json
- Add rule conflict detection

**Phase 10: Documentation & Polish** (Tasks 28-30)
- Update error message catalog
- Write migration guide
- Performance optimization

Each phase builds on the previous, ensuring incremental progress without breaking existing functionality (Requirement 12).

## Correctness Properties

A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.

### Error Handling Properties

Property 1: Graceful Error Handling
*For any* error condition in the Packager, Runtime_Codegen, Editor_Codegen, or Shader_Codegen, the system should return a Result::Err with structured context (file path, line, column, message) rather than panicking or calling unwrap()
**Validates: Requirements 1.1, 1.2, 1.7**

Property 2: Error Collection
*For any* build with multiple errors, the system should collect and report all errors rather than stopping at the first failure
**Validates: Requirements 1.10**

Property 3: Post-Processor Resilience
*For any* malformed C++ code encountered by the Post_Processor, the system should log warnings and continue processing rather than crashing
**Validates: Requirements 1.9, 8.10**

### Type Mapping Properties

Property 4: No Double-Prefixing
*For any* type name (actor, struct, enum, component, delegate), if the name already has the correct UE5 prefix (A, F, E, U, S), the type mapper should not add a duplicate prefix
**Validates: Requirements 2.2, 2.3, 2.4, 2.5, 2.10, 7.1**

Property 5: Pointer Type Consistency
*For any* UObject-derived type, the type mapper should append * to make it a pointer type, and for non-UObject types, it should not append *
**Validates: Requirements 2.6**

Property 6: Recursive Type Mapping
*For any* container type (TArray, TMap, TSet, TOptional) with inner types, the type mapper should correctly map all inner types recursively
**Validates: Requirements 2.7, 2.8**

Property 7: Engine Type Resolution
*For any* known engine type, the type mapper should query EngineKnowledge and return the canonical C++ name and include path
**Validates: Requirements 2.9**

### Naming Convention Properties

Property 8: Number Preservation
*For any* type name containing numbers, the naming system should preserve the numbers in the correct position (e.g., Player2 → APlayer2, not APlayer2)
**Validates: Requirements 7.2**

Property 9: Underscore Conversion
*For any* type name containing underscores, the naming system should convert to PascalCase while preserving semantic boundaries (e.g., health_component → HealthComponent)
**Validates: Requirements 7.3**

Property 10: Consecutive Capitals Handling
*For any* type name with consecutive capital letters, the snake_case converter should handle them correctly (e.g., HTTPServer → http_server)
**Validates: Requirements 7.8**

Property 11: Keyword Collision Detection
*For any* type name that is a C++ keyword or UE5 macro name, the naming system should return an error suggesting an alternative
**Validates: Requirements 7.6, 7.7**

Property 12: RPC Prefix Preservation
*For any* RPC function name (Server_*, Client_*, Multicast_*), the naming system should preserve the prefix in the implementation name
**Validates: Requirements 7.10**

Property 13: Delegate Naming Convention
*For any* delegate type, the naming system should follow UE5 delegate naming conventions with correct F-prefix
**Validates: Requirements 7.9**

### Oracle Validation Properties

Property 14: Replication Validation
*For any* actor with replicated properties, the Oracle should verify that GetLifetimeReplicatedProps will be generated
**Validates: Requirements 3.1**

Property 15: RPC Naming Validation
*For any* function declared as an RPC, the Oracle should verify it follows the naming convention (Server_*, Client_*, Multicast_*)
**Validates: Requirements 3.2**

Property 16: DataTable Field Validation
*For any* @datatable struct, the Oracle should verify all fields are UE5-serializable types
**Validates: Requirements 3.3**

Property 17: Component Feature Validation
*For any* @component struct, the Oracle should verify it does not contain actor-only features
**Validates: Requirements 3.4**

Property 18: Name Collision Detection
*For any* user-defined type name, the Oracle should check against EngineKnowledge and return an error if it collides with a known UE5 engine type or reserved keyword
**Validates: Requirements 3.10, 3.11**

Property 19: Circular Dependency Detection
*For any* set of types with dependencies, the Oracle should detect circular dependency cycles and report them
**Validates: Requirements 3.12**

### Shader Validation Properties

Property 20: Unique Binding Slots
*For any* shader with multiple uniforms, the shader validator should verify all binding slots (@N) are unique within that shader
**Validates: Requirements 3.5, 4.1**

Property 21: Permutation Naming Validation
*For any* shader permutation uniform, the validator should verify it uses the CFG_* or ENABLE_* naming convention
**Validates: Requirements 3.6, 4.2**

Property 22: POD Struct HLSL Compatibility
*For any* POD struct declared in a shader, the validator should verify all field types are HLSL-compatible
**Validates: Requirements 4.3**

Property 23: POD Struct Redefinition Detection
*For any* shader codegen that generates a POD struct, the system should validate it is not redefined if already declared in the .kn source
**Validates: Requirements 4.4**

Property 24: HLSL Keyword Collision Detection
*For any* identifier in generated HLSL code, the system should validate it is not a C++ keyword
**Validates: Requirements 4.5**

Property 25: Shader Binding Conflict Detection
*For any* shader with texture samplers and uniforms, the validator should verify binding slots do not conflict
**Validates: Requirements 4.6**

Property 26: Generated USF Syntax Validation
*For any* generated .usf file, the system should validate it contains no syntax errors detectable by regex patterns
**Validates: Requirements 4.10**

### Editor Codegen Properties

Property 27: No TODO Stubs
*For any* generated editor code (asset editors, viewports, details panels, toolbars), the output should contain no TODO comments
**Validates: Requirements 5.1**

Property 28: Slate Composition Correctness
*For any* Slate widget with nested composition, the generated code should produce correct SNew() chains with proper indentation
**Validates: Requirements 5.7**

Property 29: Editor Feature Integration
*For any* plugin with multiple editor features, the generated code should have correct initialization order and dependency wiring
**Validates: Requirements 5.10**

### Module Dependency Properties

Property 30: Include-Based Dependency Detection
*For any* generated C++ file with #include statements, the dependency resolver should correctly detect all required UE5 modules
**Validates: Requirements 6.1**

Property 31: Build.cs Completeness
*For any* generated .Build.cs file, it should include all detected module dependencies in PublicDependencyModuleNames
**Validates: Requirements 6.7**

Property 32: Circular Module Dependency Detection
*For any* set of module dependencies, the system should detect circular dependencies and return an error with the dependency chain
**Validates: Requirements 6.10**

### Post-Processing Properties

Property 33: Replication Code Injection
*For any* generated code with replicated properties, the Post_Processor should add GetLifetimeReplicatedProps implementation
**Validates: Requirements 8.1**

Property 34: Shader Initialization Injection
*For any* generated code using shaders, the Post_Processor should add shader initialization in BeginPlay
**Validates: Requirements 8.2**

Property 35: Forward Declaration Injection
*For any* generated code with missing forward declarations, the Post_Processor should add them in the correct order
**Validates: Requirements 8.3**

Property 36: Formatting Normalization
*For any* generated code, the Post_Processor should normalize blank lines (single), indentation (tabs), line endings (LF), and remove trailing whitespace
**Validates: Requirements 8.4, 8.5, 8.8, 8.9**

Property 37: Include Guard Injection
*For any* generated header file missing include guards, the Post_Processor should add them with correct macro names
**Validates: Requirements 8.6**

Property 38: Include Reordering
*For any* generated code with includes in wrong order, the Post_Processor should reorder them to UE5 conventions (CoreMinimal first, then engine, then project)
**Validates: Requirements 8.7**

### Data-Driven Validation Properties

Property 39: Custom Rule Enforcement
*For any* validation rule defined in validation_rules.json (type collision, naming convention, semantic constraint), the Oracle should enforce it during validation
**Validates: Requirements 10.2, 10.3, 10.4**

### Shader Virtual Path Properties

Property 40: USF File Placement
*For any* generated .usf file, the system should write it to PluginRoot/Shaders/ with correct relative paths
**Validates: Requirements 11.2**

Property 41: Virtual Path Consistency
*For any* plugin with multiple shaders, all shaders should share the same virtual path mapping (/Plugin/PluginName → physical Shaders/ directory)
**Validates: Requirements 11.5, 11.6, 11.7**

Property 42: Shader Include Resolution
*For any* shader that includes other shaders, the system should validate the virtual paths resolve correctly
**Validates: Requirements 11.8**

### Backward Compatibility Properties

Property 43: Validation Backward Compatibility
*For any* existing valid .kn file that previously compiled, adding new validation rules should not cause it to fail compilation
**Validates: Requirements 12.1, 12.4**

Property 44: Output Equivalence
*For any* valid input, refactoring error handling, type mapping, or post-processing should produce identical output to the previous version
**Validates: Requirements 12.2, 12.3, 12.8**

Property 45: Feature Backward Compatibility
*For any* existing shader or editor feature that previously worked, adding validation or completing codegen should not break it
**Validates: Requirements 12.5, 12.6, 12.7**

Property 46: Module Detection Completeness
*For any* plugin with manually specified module dependencies, auto-detection should include all those modules plus any additional ones found
**Validates: Requirements 12.9**

### Metadata System Properties

Property 47: Metadata Schema Validation
*For any* metadata file loaded from unreal/metadata/, the system should validate it against its JSON schema and return structured errors on validation failure
**Validates: Requirements 13.1, 13.9**

Property 48: Metadata Query Fallback
*For any* type, module, or rule query, if the metadata is incomplete, the system should log a warning and use fallback behavior rather than crashing
**Validates: Requirements 13.18, 13.20**

Property 49: Metadata-First Type Resolution
*For any* type mapping operation, the system should query engine_knowledge.json before using any hardcoded fallback logic
**Validates: Requirements 13.13**

Property 50: Metadata-First Module Resolution
*For any* module dependency resolution, the system should query module_graph.json before using any hardcoded fallback logic
**Validates: Requirements 13.14**

Property 51: Metadata-First Validation
*For any* Oracle validation rule, the system should query uht_rules.json before using any hardcoded fallback logic
**Validates: Requirements 13.15**

Property 52: Multi-Drive UE5 Support
*For any* metadata extraction script, it should support configurable UE5 installation paths on any drive (D:, M:, etc.) via configuration file
**Validates: Requirements 13.11**

Property 53: Multi-Version UE5 Support
*For any* metadata extraction script, it should support multiple UE5 versions (5.4, 5.5, 5.6, 5.7) and generate version-specific metadata files
**Validates: Requirements 13.12**

### Property Reflection Summary

After reviewing all properties, the following consolidations were made:
- Properties 2.2-2.5 and 2.10 were consolidated into Property 4 (No Double-Prefixing) as they all test the same core behavior
- Properties 7.6 and 7.7 were consolidated into Property 11 (Keyword Collision Detection)
- Properties 3.5 and 4.1 were consolidated into Property 20 (Unique Binding Slots)
- Properties 3.6 and 4.2 were consolidated into Property 21 (Permutation Naming Validation)
- Properties 8.4, 8.5, 8.8, 8.9 were consolidated into Property 36 (Formatting Normalization)
- Properties 11.5, 11.6, 11.7 were consolidated into Property 41 (Virtual Path Consistency)
- Properties 12.1 and 12.4 were consolidated into Property 43 (Validation Backward Compatibility)
- Properties 12.2, 12.3, 12.8 were consolidated into Property 44 (Output Equivalence)
- Properties 12.5, 12.6, 12.7 were consolidated into Property 45 (Feature Backward Compatibility)

This reduces the total from 70+ individual criteria to 53 comprehensive properties (46 original + 7 new metadata properties), eliminating redundancy while maintaining complete coverage including the new metadata system requirements.

## Testing Strategy

### Dual Testing Approach

The KAIN pipeline requires both unit tests and property-based tests for comprehensive coverage:

**Unit Tests**: Verify specific examples, edge cases, and error conditions
- Specific error message formats
- Known edge cases (EHealthStatus, HTTPServer)
- Integration points between crates
- Configuration file parsing
- File I/O operations

**Property Tests**: Verify universal properties across all inputs
- Type mapping never double-prefixes (Property 4)
- Error handling never panics (Property 1)
- Naming conventions handle all valid inputs (Properties 8-13)
- Validation catches all semantic errors (Properties 14-19)
- Post-processing preserves correctness (Properties 33-38)

Together, unit tests catch concrete bugs while property tests verify general correctness.

### Property-Based Testing Configuration

**Library Selection**: Use `proptest` for Rust (mature, well-integrated with cargo test)

**Test Configuration**:
- Minimum 100 iterations per property test (due to randomization)
- Each property test references its design document property
- Tag format: `// Feature: kain-pipeline-robustness, Property N: [property text]`

**Example Property Test**:
```rust
#[test]
fn property_4_no_double_prefixing() {
    // Feature: kain-pipeline-robustness, Property 4: No Double-Prefixing
    proptest!(|(name in "[A-Z][a-zA-Z0-9_]*")| {
        let mapper = TypeMapper::new(Arc::new(EngineKnowledge::new()));
        
        // Test actor names
        let actor_name = format!("A{}", name);
        let mapped = mapper.map_actor_name(&actor_name).unwrap();
        assert_eq!(mapped, actor_name); // Should not become AAName
        
        // Test struct names
        let struct_name = format!("F{}", name);
        let mapped = mapper.map_struct_name(&struct_name).unwrap();
        assert_eq!(mapped, struct_name); // Should not become FFName
        
        // Test enum names
        let enum_name = format!("E{}", name);
        let mapped = mapper.map_enum_name(&enum_name).unwrap();
        assert_eq!(mapped, enum_name); // Should not become EEName
    });
}
```

### Unit Testing Balance

Unit tests should focus on:
- Specific error message content and format
- Known edge cases from bug reports (EEHealthStatus, SFDiagnosticViewport)
- Integration between packager and codegen crates
- JSON schema validation
- File system operations
- Configuration parsing

Avoid writing too many unit tests for cases that property tests cover. For example:
- Don't write 50 unit tests for different type names - use property test
- Don't write unit tests for every possible error condition - use property test
- Do write unit tests for specific error message formats
- Do write unit tests for integration points

### Test Organization

```
crates/
├── ue5/
│   ├── src/
│   │   └── ue5/
│   │       ├── types.rs
│   │       ├── types_test.rs          # Unit tests
│   │       ├── naming.rs
│   │       ├── naming_test.rs         # Unit tests
│   │       ├── oracle.rs
│   │       └── oracle_test.rs         # Unit tests
│   └── tests/
│       ├── property_tests.rs          # Property tests
│       └── integration_tests.rs       # Integration tests
├── ue5-editor/
│   ├── src/
│   │   └── editor/
│   │       ├── codegen.rs
│   │       ├── codegen_test.rs        # Unit tests
│   │       ├── slate.rs
│   │       └── slate_test.rs          # Unit tests
│   └── tests/
│       └── property_tests.rs          # Property tests
├── ue5-shaders/
│   ├── src/
│   │   ├── validation.rs
│   │   └── validation_test.rs         # Unit tests
│   └── tests/
│       └── property_tests.rs          # Property tests
└── cli/
    ├── src/
    │   └── packager/
    │       ├── codegen.rs
    │       ├── codegen_test.rs        # Unit tests
    │       ├── dependencies.rs
    │       └── dependencies_test.rs   # Unit tests
    └── tests/
        ├── integration_tests.rs       # End-to-end tests
        └── snapshots/                 # Snapshot tests
            ├── ultimate.kn.snap
            ├── simple_actor.kn.snap
            └── shader_plugin.kn.snap
```

### Test Coverage Goals

- Unit tests: 100+ tests covering specific examples and edge cases
- Property tests: 53 properties (46 original + 7 metadata properties)
- Integration tests: 10+ end-to-end scenarios
- Snapshot tests: All example plugins in testing/Phase3/
- Total: 160+ tests, 90%+ code coverage

### Continuous Integration

All tests run on every commit:
```bash
# Run all tests
cargo test --all

# Run with coverage
cargo tarpaulin --all --out Html

# Run property tests with more iterations (CI only)
PROPTEST_CASES=1000 cargo test --all
```

### Test Maintenance

- Update snapshots when intentional changes are made: `cargo test -- --ignored`
- Review property test failures carefully - they often reveal edge cases
- Add regression tests for every bug fix
- Keep test execution time under 5 minutes for fast feedback
