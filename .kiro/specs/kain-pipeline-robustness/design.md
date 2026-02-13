# Design Document: KAIN-PRO Pipeline Robustness Research & Improvement

## Overview

This design document outlines a comprehensive research and improvement initiative to transform the KAIN-PRO UE5 compilation pipeline into the most robust, intelligent, and future-proof code generation system for Unreal Engine 5. The goal is to enable truly limitless agentic coding where LLMs can generate production-quality UE5 plugins with zero manual intervention.

The system currently consists of three major pipelines:
1. **UE5 C++ Codegen Pipeline**: Generates actors, components, structs, enums, RPCs, and networking code
2. **USF Shader Pipeline**: Generates compute/fragment shaders with permutations and RDG integration
3. **UE5 Editor Pipeline**: Generates editor tools, custom editors, and viewport extensions

This project will enhance all three pipelines with intelligent code generation, comprehensive error handling, deep UE5 API knowledge through an enhanced Oracle system, and integration with external tools to create a system that truly makes "Unreal Engine 5 feel limitless."

### Key Design Goals

1. **Intelligence**: Code generation that understands context and generates optimal UE5 patterns automatically
2. **Robustness**: Comprehensive error detection with actionable, LLM-friendly error messages
3. **Completeness**: Support for all UE5 patterns including advanced editor tooling
4. **Performance**: Fast compilation even for large plugins (100+ files)
5. **Maintainability**: Template-based architecture that's easy to extend
6. **Future-Proof**: Automatic UE5 API knowledge extraction that stays current with UE5 releases

## Architecture

### High-Level System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        KAIN-PRO Compiler                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐ │
│  │   Frontend   │─────▶│   Analysis   │─────▶│   Backend    │ │
│  │              │      │              │      │              │ │
│  │  • Lexer     │      │  • Type      │      │  • UE5 C++   │ │
│  │  • Parser    │      │    Checker   │      │  • USF       │ │
│  │  • AST       │      │  • Oracle    │      │  • Editor    │ │
│  │    Builder   │      │    Validator │      │    Tools     │ │
│  └──────────────┘      └──────────────┘      └──────────────┘ │
│         │                      │                      │         │
│         │                      │                      │         │
│         ▼                      ▼                      ▼         │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              Enhanced Oracle System                       │ │
│  │                                                            │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │ │
│  │  │ UE5 API DB  │  │ Pattern DB  │  │ Template DB │     │ │
│  │  │             │  │             │  │             │     │ │
│  │  │ • Classes   │  │ • RPC       │  │ • Actor     │     │ │
│  │  │ • Functions │  │   Patterns  │  │   Templates │     │ │
│  │  │ • Modules   │  │ • Network   │  │ • Component │     │ │
│  │  │ • Shaders   │  │   Patterns  │  │   Templates │     │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘     │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              External Tool Integration                    │ │
│  │                                                            │ │
│  │  • UE5 Header Parser (libclang/tree-sitter)              │ │
│  │  • C++ Validator (clang-tidy)                            │ │
│  │  • Shader Validator (DXC/glslang)                        │ │
│  │  • Documentation Generator (custom)                      │ │
│  └──────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Pipeline Architecture

Each pipeline follows a consistent architecture with enhanced validation and intelligence:

```
.kn Source Files
      │
      ▼
┌─────────────────┐
│  Multi-File     │
│  Parser         │
│  • Per-file     │
│    validation   │
│  • AST merging  │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│  Type Checker   │
│  • Type         │
│    inference    │
│  • Cross-file   │
│    validation   │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│  Oracle         │
│  Validation     │
│  • UE5 API      │
│    checks       │
│  • Pattern      │
│    validation   │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│  Intelligent    │
│  Codegen        │
│  • Template     │
│    selection    │
│  • Context-     │
│    aware gen    │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│  Post-          │
│  Processing     │
│  • Include      │
│    ordering     │
│  • Forward      │
│    decls        │
└─────────────────┘
      │
      ▼
  UE5 Plugin
```

## Components and Interfaces

### 1. Enhanced Oracle System

The Oracle is the intelligence layer that provides UE5 API knowledge and validates code generation decisions.

#### Oracle Core Interface

```rust
pub trait Oracle {
    // UE5 API Validation
    fn validate_class(&self, class_name: &str, ue5_version: &str) -> Result<ClassInfo>;
    fn validate_function(&self, class: &str, func: &str) -> Result<FunctionSignature>;
    fn validate_property_specifier(&self, specifier: &str, context: &PropertyContext) -> Result<()>;
    fn validate_module_dependency(&self, module: &str) -> Result<ModuleInfo>;
    
    // Pattern Validation
    fn validate_rpc_pattern(&self, func: &FunctionDef) -> Result<RPCValidation>;
    fn validate_replication_pattern(&self, prop: &PropertyDef) -> Result<ReplicationInfo>;
    fn validate_blueprint_pattern(&self, func: &FunctionDef) -> Result<BlueprintInfo>;
    
    // Intelligent Suggestions
    fn suggest_includes(&self, types_used: &[String]) -> Vec<String>;
    fn suggest_forward_decls(&self, types_used: &[String]) -> Vec<String>;
    fn suggest_module_dependencies(&self, apis_used: &[String]) -> Vec<String>;
    fn suggest_rpc_implementation(&self, func: &FunctionDef) -> String;
    
    // API Knowledge Extraction
    fn extract_from_ue5_headers(&mut self, ue5_path: &Path) -> Result<()>;
    fn update_api_database(&mut self, version: &str) -> Result<()>;
}

pub struct ClassInfo {
    pub name: String,
    pub module: String,
    pub base_class: Option<String>,
    pub is_blueprintable: bool,
    pub is_abstract: bool,
    pub required_includes: Vec<String>,
}

pub struct FunctionSignature {
    pub name: String,
    pub return_type: String,
    pub parameters: Vec<Parameter>,
    pub specifiers: Vec<String>,
    pub is_const: bool,
    pub is_virtual: bool,
}

pub struct RPCValidation {
    pub is_valid: bool,
    pub needs_implementation: bool,
    pub needs_validation: bool,
    pub parameter_issues: Vec<String>,
}
```

#### Oracle Implementation Strategy

The Oracle will be implemented as a multi-layered knowledge system:

**Layer 1: Static API Database**
- Pre-extracted UE5 API knowledge for common versions (5.3, 5.4, 5.5)
- Stored as efficient binary format for fast lookup
- Includes all UCLASS, UFUNCTION, UPROPERTY definitions

**Layer 2: Dynamic Header Parser**
- Uses libclang or tree-sitter to parse UE5 headers on-demand
- Extracts API knowledge for custom UE5 builds or plugins
- Caches results for performance

**Layer 3: Pattern Database**
- Common UE5 patterns (RPC, replication, Blueprint integration)
- Best practices and anti-patterns
- Template selection rules

**Layer 4: Validation Rules**
- Type compatibility rules
- Specifier validation rules
- Module dependency rules
- Platform-specific rules

### 2. Intelligent Code Generation System

The code generation system uses templates and context-aware logic to generate optimal UE5 code.

#### Template System Interface

```rust
pub trait TemplateSystem {
    // Template Management
    fn register_template(&mut self, name: &str, template: Template);
    fn get_template(&self, name: &str) -> Option<&Template>;
    
    // Template Rendering
    fn render(&self, template_name: &str, context: &Context) -> Result<String>;
    fn render_multi_file(&self, template_name: &str, context: &Context) -> Result<Vec<GeneratedFile>>;
    
    // Template Composition
    fn compose(&self, templates: &[&str], context: &Context) -> Result<String>;
}

pub struct Template {
    pub name: String,
    pub description: String,
    pub parameters: Vec<TemplateParameter>,
    pub header_template: Option<String>,
    pub source_template: Option<String>,
    pub dependencies: Vec<String>,
    pub validation_rules: Vec<ValidationRule>,
}

pub struct Context {
    pub variables: HashMap<String, Value>,
    pub ue5_version: String,
    pub platform: String,
    pub oracle: Arc<dyn Oracle>,
}

pub struct GeneratedFile {
    pub path: PathBuf,
    pub content: String,
    pub file_type: FileType,
}

pub enum FileType {
    Header,
    Source,
    Shader,
    Config,
}
```

#### Code Generation Strategy

**Phase 1: Analysis**
1. Analyze KAIN AST to identify required UE5 patterns
2. Query Oracle for API validation and suggestions
3. Select appropriate templates based on context
4. Build generation context with all necessary information

**Phase 2: Template Rendering**
1. Render header templates with type definitions
2. Render source templates with implementations
3. Apply intelligent include ordering
4. Generate forward declarations where needed

**Phase 3: Post-Processing**
1. Resolve circular dependencies
2. Optimize include statements
3. Format code consistently
4. Validate generated code structure

**Phase 4: Validation**
1. Run C++ syntax validation (optional clang-tidy integration)
2. Validate UE5 macro usage
3. Check for common mistakes
4. Generate validation report

### 3. UE5 Editor Pipeline Templates

The editor pipeline needs comprehensive templates for all UE5 editor extension types.

#### Editor Template Categories

**Asset System Templates**
- Custom Asset Factory
- Custom Asset Editor
- Asset Actions
- Asset Thumbnail Renderer
- Asset Import/Export

**Detail Customization Templates**
- Property Type Customization
- Detail Panel Customization
- Category Customization
- Property Row Customization

**Viewport Tool Templates**
- Editor Mode
- Editor Mode Toolkit
- Editor Mode Tool
- Viewport Gizmo
- Viewport Widget

**UI Extension Templates**
- Custom Editor Tab
- Dockable Window
- Toolbar Extension
- Menu Extension
- Context Menu Extension

**Settings Templates**
- Project Settings
- Editor Settings
- Developer Settings
- Config File Integration

#### Example: Asset Editor Template

```rust
pub struct AssetEditorTemplate {
    asset_type: String,
    base_class: String,
    editor_features: Vec<EditorFeature>,
}

pub enum EditorFeature {
    Viewport3D,
    PropertyPanel,
    Toolbar,
    MenuBar,
    AssetPreview,
    CustomWidgets(Vec<WidgetDef>),
}

impl Template for AssetEditorTemplate {
    fn render(&self, context: &Context) -> Result<Vec<GeneratedFile>> {
        let mut files = vec![];
        
        // Generate asset factory
        files.push(self.generate_factory(context)?);
        
        // Generate asset editor class
        files.push(self.generate_editor_class(context)?);
        
        // Generate editor toolkit
        files.push(self.generate_toolkit(context)?);
        
        // Generate asset actions
        files.push(self.generate_actions(context)?);
        
        // Generate module registration
        files.push(self.generate_module_registration(context)?);
        
        Ok(files)
    }
}
```

### 4. Error Detection and Recovery System

The error system provides clear, actionable error messages optimized for LLM consumption.

#### Error Message Interface

```rust
pub struct CompilationError {
    pub error_type: ErrorType,
    pub severity: Severity,
    pub location: SourceLocation,
    pub message: String,
    pub help: Option<String>,
    pub suggestions: Vec<Suggestion>,
    pub related_errors: Vec<RelatedError>,
}

pub enum ErrorType {
    ParseError,
    TypeError,
    SemanticError,
    OracleValidationError,
    CodegenError,
}

pub enum Severity {
    Error,
    Warning,
    Info,
}

pub struct SourceLocation {
    pub file: PathBuf,
    pub line: usize,
    pub column: usize,
    pub length: usize,
}

pub struct Suggestion {
    pub description: String,
    pub replacement: Option<String>,
    pub example: Option<String>,
}

pub struct RelatedError {
    pub location: SourceLocation,
    pub message: String,
}
```

#### Error Message Format

Errors should follow this format for maximum LLM comprehension:

```
❌ [ERROR_TYPE] in [FILE]:[LINE]:[COLUMN]

   [LINE] | [SOURCE CODE]
          | [HIGHLIGHT]
          |
   [DETAILED MESSAGE]
   
   Help: [ACTIONABLE SUGGESTION]
   
   Example:
   [CORRECT CODE EXAMPLE]
   
   Note: [ADDITIONAL CONTEXT]
```

Example:

```
❌ Type Error in actors.kn:45:23

   45 |     state health: HealthComponent
      |                   ^^^^^^^^^^^^^^^
      |
   Expected initializer for actor state field.
   Actor state fields must have default values.
   
   Help: Add an initializer:
         state health: HealthComponent = HealthComponent()
   
   Example:
         actor Player:
             state health: HealthComponent = HealthComponent()
             state score: Int = 0
   
   Note: Components should typically be created in BeginPlay()
         for proper initialization order. Consider using a
         component reference instead of direct instantiation.
```

### 5. USF Shader Pipeline Enhancements

The shader pipeline needs robustness improvements for all UE5 shader patterns.

#### Shader Generation Architecture

```rust
pub struct ShaderGenerator {
    oracle: Arc<dyn Oracle>,
    template_system: Arc<dyn TemplateSystem>,
}

impl ShaderGenerator {
    pub fn generate_compute_shader(&self, shader_def: &ShaderDef) -> Result<ShaderOutput> {
        // Validate shader definition
        self.validate_shader_def(shader_def)?;
        
        // Generate USF file
        let usf_content = self.generate_usf(shader_def)?;
        
        // Generate C++ binding header
        let binding_header = self.generate_binding_header(shader_def)?;
        
        // Generate C++ implementation
        let binding_impl = self.generate_binding_impl(shader_def)?;
        
        // Generate RDG pass setup
        let rdg_pass = self.generate_rdg_pass(shader_def)?;
        
        Ok(ShaderOutput {
            usf_file: usf_content,
            binding_header,
            binding_impl,
            rdg_pass,
        })
    }
    
    fn validate_shader_def(&self, shader_def: &ShaderDef) -> Result<()> {
        // Validate shader stage
        self.validate_shader_stage(shader_def.stage)?;
        
        // Validate parameters
        for param in &shader_def.parameters {
            self.validate_shader_parameter(param)?;
        }
        
        // Validate permutations
        for perm in &shader_def.permutations {
            self.validate_permutation(perm)?;
        }
        
        // Validate output type
        self.validate_shader_output(shader_def.output_type)?;
        
        Ok(())
    }
}

pub struct ShaderOutput {
    pub usf_file: GeneratedFile,
    pub binding_header: GeneratedFile,
    pub binding_impl: GeneratedFile,
    pub rdg_pass: GeneratedFile,
}
```

#### Shader Template System

The shader pipeline will use templates for common shader patterns:

**Compute Shader Template**
- RDG pass setup
- Parameter struct definition
- Shader permutation domain
- Dispatch logic

**Material Shader Template**
- Material domain setup
- Blend mode configuration
- Shader parameter bindings
- Material expression integration

**Global Shader Template**
- Shader registration
- Virtual path mapping
- Shader compilation environment
- Shader parameter struct

**Post-Process Shader Template**
- Post-process material setup
- Render target configuration
- Blendable interface
- Shader parameter bindings

### 6. Multi-File Build System Enhancements

The build system needs better validation and dependency resolution.

#### Build System Architecture

```rust
pub struct BuildSystem {
    oracle: Arc<dyn Oracle>,
    file_graph: DependencyGraph,
    cache: BuildCache,
}

impl BuildSystem {
    pub fn build_multi_file(&mut self, config: &BuildConfig) -> Result<PluginOutput> {
        // Phase 1: Parse all files
        let asts = self.parse_all_files(&config.source_files)?;
        
        // Phase 2: Build dependency graph
        self.build_dependency_graph(&asts)?;
        
        // Phase 3: Validate dependencies
        self.validate_dependencies()?;
        
        // Phase 4: Topological sort
        let build_order = self.topological_sort()?;
        
        // Phase 5: Type check in order
        let typed_asts = self.type_check_in_order(&asts, &build_order)?;
        
        // Phase 6: Merge ASTs
        let merged_ast = self.merge_asts(&typed_asts)?;
        
        // Phase 7: Oracle validation
        self.oracle_validate(&merged_ast)?;
        
        // Phase 8: Code generation
        let generated_files = self.generate_code(&merged_ast)?;
        
        // Phase 9: Post-processing
        let final_files = self.post_process(generated_files)?;
        
        Ok(PluginOutput {
            files: final_files,
            metadata: self.generate_metadata(&merged_ast)?,
        })
    }
    
    fn validate_dependencies(&self) -> Result<()> {
        // Check for circular dependencies
        if let Some(cycle) = self.file_graph.find_cycle() {
            return Err(Error::CircularDependency(cycle));
        }
        
        // Check for missing dependencies
        for node in self.file_graph.nodes() {
            for dep in node.dependencies() {
                if !self.file_graph.has_node(dep) {
                    return Err(Error::MissingDependency {
                        file: node.file.clone(),
                        missing: dep.clone(),
                    });
                }
            }
        }
        
        Ok(())
    }
}

pub struct DependencyGraph {
    nodes: HashMap<PathBuf, FileNode>,
    edges: Vec<(PathBuf, PathBuf)>,
}

impl DependencyGraph {
    pub fn find_cycle(&self) -> Option<Vec<PathBuf>> {
        // Tarjan's algorithm for cycle detection
        // Returns the cycle if found
        todo!()
    }
    
    pub fn topological_sort(&self) -> Result<Vec<PathBuf>> {
        // Kahn's algorithm for topological sorting
        // Returns build order
        todo!()
    }
}
```

## Data Models

### Oracle Data Models

```rust
// UE5 API Database Schema
pub struct UE5ApiDatabase {
    pub version: String,
    pub classes: HashMap<String, ClassDefinition>,
    pub functions: HashMap<String, FunctionDefinition>,
    pub modules: HashMap<String, ModuleDefinition>,
    pub shaders: HashMap<String, ShaderDefinition>,
}

pub struct ClassDefinition {
    pub name: String,
    pub module: String,
    pub header_path: String,
    pub base_class: Option<String>,
    pub interfaces: Vec<String>,
    pub metadata: HashMap<String, String>,
    pub properties: Vec<PropertyDefinition>,
    pub functions: Vec<FunctionDefinition>,
    pub is_blueprintable: bool,
    pub is_abstract: bool,
}

pub struct PropertyDefinition {
    pub name: String,
    pub type_name: String,
    pub specifiers: Vec<String>,
    pub metadata: HashMap<String, String>,
    pub default_value: Option<String>,
}

pub struct FunctionDefinition {
    pub name: String,
    pub return_type: String,
    pub parameters: Vec<ParameterDefinition>,
    pub specifiers: Vec<String>,
    pub metadata: HashMap<String, String>,
    pub is_const: bool,
    pub is_virtual: bool,
    pub is_static: bool,
}

pub struct ModuleDefinition {
    pub name: String,
    pub type_: ModuleType,
    pub dependencies: Vec<String>,
    pub public_headers: Vec<String>,
    pub private_headers: Vec<String>,
}

pub enum ModuleType {
    Runtime,
    Editor,
    Developer,
    ThirdParty,
}
```

### Template Data Models

```rust
pub struct TemplateLibrary {
    pub actor_templates: Vec<ActorTemplate>,
    pub component_templates: Vec<ComponentTemplate>,
    pub editor_templates: Vec<EditorTemplate>,
    pub shader_templates: Vec<ShaderTemplate>,
}

pub struct ActorTemplate {
    pub name: String,
    pub base_class: String,
    pub features: Vec<ActorFeature>,
    pub header_template: String,
    pub source_template: String,
}

pub enum ActorFeature {
    Replication,
    RPCs,
    Components,
    Networking,
    SaveGame,
    BlueprintCallable,
}

pub struct EditorTemplate {
    pub name: String,
    pub editor_type: EditorType,
    pub required_modules: Vec<String>,
    pub files: Vec<TemplateFile>,
}

pub enum EditorType {
    AssetEditor,
    DetailCustomization,
    ViewportTool,
    EditorTab,
    PropertyEditor,
    ThumbnailRenderer,
}

pub struct TemplateFile {
    pub path: String,
    pub template: String,
    pub file_type: FileType,
}
```

### Build System Data Models

```rust
pub struct BuildConfig {
    pub source_files: Vec<PathBuf>,
    pub output_dir: PathBuf,
    pub plugin_name: String,
    pub ue5_version: String,
    pub targets: Vec<BuildTarget>,
}

pub enum BuildTarget {
    UE5Cpp,
    USF,
    Editor,
}

pub struct PluginOutput {
    pub files: Vec<GeneratedFile>,
    pub metadata: PluginMetadata,
}

pub struct PluginMetadata {
    pub name: String,
    pub version: String,
    pub description: String,
    pub modules: Vec<String>,
    pub dependencies: Vec<String>,
}
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

Before defining the correctness properties, I need to analyze the acceptance criteria from the requirements document to determine which are testable as properties, examples, or edge cases.



## Property Reflection

After analyzing all acceptance criteria, I've identified several areas where properties can be consolidated to avoid redundancy:

**Consolidation 1: Code Generation Properties**
- Properties 1.1-1.8 all test code generation behavior for different UE5 patterns
- These can be consolidated into fewer comprehensive properties that test code generation correctness across all patterns

**Consolidation 2: Oracle Validation Properties**
- Properties 2.1-2.8 all test Oracle validation behavior
- These can be consolidated into properties that test validation correctness across all API types

**Consolidation 3: Editor Template Properties**
- Properties 3.1-3.8 all test editor template generation
- These can be consolidated into properties that test template generation correctness across all editor types

**Consolidation 4: Error Message Properties**
- Properties 4.1-4.8 all test error message format and content
- These can be consolidated into properties that test error message quality across all error types

**Consolidation 5: Shader Pipeline Properties**
- Properties 5.1-5.8 all test shader generation
- These can be consolidated into properties that test shader generation correctness across all shader types

**Consolidation 6: Multi-File Build Properties**
- Properties 6.1-6.8 all test multi-file validation
- These can be consolidated into properties that test dependency resolution across all scenarios

**Consolidation 7: Template System Properties**
- Properties 8.1-8.7 all test template system features
- These can be consolidated into properties that test template functionality comprehensively

**Consolidation 8: Type Inference Properties**
- Properties 11.1-11.4, 11.6-11.8 all test type inference
- These can be consolidated into properties that test inference correctness across all contexts

**Consolidation 9: Parser Round-Trip Properties**
- Properties 16.1-16.6 and 17.1-17.5 test parsing and serialization
- These are already well-structured as round-trip properties

After reflection, I'll write consolidated properties that provide unique validation value without redundancy.

### Property 1: Replicated Property Code Generation

*For any* actor definition with replicated properties, the generated C++ code should include a GetLifetimeReplicatedProps implementation that registers all replicated properties with the replication system.

**Validates: Requirements 1.1**

### Property 2: RPC Function Generation and Validation

*For any* function definition with RPC naming convention (Server_*, Client_*, Multicast_*), the system should validate parameter types are RPC-compatible, generate _Implementation and _Validate functions, and include proper UFUNCTION specifiers.

**Validates: Requirements 1.2**

### Property 3: Intelligent Include Resolution

*For any* type reference in generated code, the system should determine whether a forward declaration or full include is needed based on usage context, and generate the minimal necessary includes to avoid circular dependencies.

**Validates: Requirements 1.3, 1.5**

### Property 4: Constructor Initialization Completeness

*For any* generated class with UPROPERTY fields, the constructor should initialize all properties with appropriate default values matching their KAIN definitions.

**Validates: Requirements 1.4**

### Property 5: Blueprint Function Validation

*For any* function marked as Blueprint-callable, the system should validate that return types and parameters are Blueprint-compatible types (no raw pointers, no non-UCLASS/USTRUCT types).

**Validates: Requirements 1.6**

### Property 6: Networking Code Completeness

*For any* actor with replicated properties or RPC functions, the generated code should include proper replication setup, GetLifetimeReplicatedProps implementation, and ownership validation.

**Validates: Requirements 1.8**

### Property 7: Oracle UE5 API Validation

*For any* UE5 class, function, or property reference in KAIN code, the Oracle should validate it exists in the target UE5 version and has the correct signature/specifiers.

**Validates: Requirements 2.1, 2.2, 2.3**

### Property 8: Oracle Module Dependency Validation

*For any* UE5 API usage, the Oracle should identify required module dependencies and suggest Build.cs additions if modules are missing.

**Validates: Requirements 2.4**

### Property 9: Oracle Shader Validation

*For any* shader definition, the Oracle should validate shader stage compatibility, uniform binding uniqueness, and parameter type compatibility.

**Validates: Requirements 2.5, 5.7, 5.8**

### Property 10: Oracle Deprecation Detection

*For any* UE5 API usage, if the API is deprecated in the target UE5 version, the Oracle should warn and suggest modern alternatives.

**Validates: Requirements 2.6**

### Property 11: Editor Code Guard Validation

*For any* editor-only API usage, the generated code should include proper WITH_EDITOR preprocessor guards to prevent compilation errors in non-editor builds.

**Validates: Requirements 2.7**

### Property 12: Editor Template Generation Completeness

*For any* editor tool definition (asset editor, detail customization, viewport tool, etc.), the system should generate all required classes, registration code, and module dependencies.

**Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8**

### Property 13: Error Message Format Consistency

*For any* compilation error (parse, type, semantic, validation), the error message should include file path, line number, column number, source code context, detailed explanation, and actionable suggestions.

**Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5, 4.8**

### Property 14: Shader Error Source Mapping

*For any* USF shader compilation error, the system should map the error location back to the original KAIN source file, line, and column.

**Validates: Requirements 4.6**

### Property 15: Circular Dependency Resolution

*For any* circular dependency detected in multi-file builds, the system should either automatically resolve it using forward declarations or provide clear suggestions for refactoring.

**Validates: Requirements 4.7, 6.2**

### Property 16: Compute Shader RDG Generation

*For any* compute shader definition, the generated code should include proper RDG pass setup, parameter struct definition, shader permutation domain, and dispatch logic.

**Validates: Requirements 5.1, 5.2, 5.3**

### Property 17: Global Shader Implementation

*For any* global shader definition, the generated code should include IMPLEMENT_GLOBAL_SHADER macro with correct virtual shader paths and shader compilation environment setup.

**Validates: Requirements 5.4, 5.6**

### Property 18: Material Shader Setup

*For any* material shader definition, the generated code should include proper material domain configuration, blend mode setup, and shader parameter bindings.

**Validates: Requirements 5.5**

### Property 19: Multi-File Type Resolution

*For any* multi-file KAIN project, all type references should resolve correctly across files, with proper dependency ordering and no undefined type errors.

**Validates: Requirements 6.1, 6.3, 6.4, 6.6, 6.7**

### Property 20: Delegate Signature Validation

*For any* delegate usage, the system should validate that the delegate signature at the usage site matches the delegate definition.

**Validates: Requirements 6.5**

### Property 21: Include Order Correctness

*For any* generated C++ code, the include statements should be topologically sorted based on type dependencies to avoid forward declaration issues and compilation errors.

**Validates: Requirements 6.8**

### Property 22: Template Parameterization

*For any* template definition with parameters, the template system should correctly substitute parameter values during code generation.

**Validates: Requirements 8.1**

### Property 23: Template Composition

*For any* template that references other templates, the template system should correctly compose them and generate coherent output.

**Validates: Requirements 8.2**

### Property 24: Template Conditional Logic

*For any* template with conditional logic (if/else, pattern matching), the template system should evaluate conditions correctly and generate appropriate code branches.

**Validates: Requirements 8.3**

### Property 25: Multi-File Template Coordination

*For any* template that generates multiple files (header/source pairs), the template system should coordinate generation to ensure consistency between files.

**Validates: Requirements 8.4**

### Property 26: Template Version Conditionals

*For any* template with UE5 version conditionals, the template system should generate code appropriate for the target UE5 version.

**Validates: Requirements 8.5**

### Property 27: Template Platform Conditionals

*For any* template with platform conditionals, the template system should generate code appropriate for the target platform.

**Validates: Requirements 8.6**

### Property 28: Template Validation Rules

*For any* template with validation rules, the template system should enforce those rules during code generation and report violations.

**Validates: Requirements 8.7**

### Property 29: UE5 Header Parsing Completeness

*For any* valid UE5 header file, the parser should extract all UCLASS, UFUNCTION, UPROPERTY, USTRUCT, and UENUM definitions with complete metadata.

**Validates: Requirements 10.1, 10.2, 10.3, 10.4**

### Property 30: UE5 Module Analysis

*For any* UE5 module, the parser should extract module dependencies, public/private API boundaries, and header file lists.

**Validates: Requirements 10.5**

### Property 31: UE5 Shader Analysis

*For any* UE5 shader file, the parser should extract shader parameter structures, bindings, and permutation definitions.

**Validates: Requirements 10.6**

### Property 32: UE5 Editor API Extraction

*For any* UE5 editor header, the parser should identify editor-only APIs and extract their signatures and usage patterns.

**Validates: Requirements 10.7**

### Property 33: UE5 Version Change Detection

*For any* two UE5 versions, the system should detect API changes (additions, removals, signature changes) and update Oracle knowledge accordingly.

**Validates: Requirements 10.8**

### Property 34: Variable Type Inference

*For any* variable initialization with an explicit initializer, the system should correctly infer the variable's type from the initializer expression.

**Validates: Requirements 11.1**

### Property 35: Function Return Type Inference

*For any* function with omitted return type, the system should infer the return type from all return statements in the function body.

**Validates: Requirements 11.2**

### Property 36: Generic Type Parameter Inference

*For any* generic type usage, the system should infer type parameters from the usage context.

**Validates: Requirements 11.3**

### Property 37: Lambda Type Inference

*For any* lambda expression, the system should infer parameter types and return type from the usage context.

**Validates: Requirements 11.4**

### Property 38: Replication Condition Inference

*For any* replicated property, the system should infer appropriate replication conditions based on usage patterns (e.g., COND_OwnerOnly for player-specific data).

**Validates: Requirements 11.6**

### Property 39: Component Type Inference

*For any* component reference in an actor, the system should infer the component type from the actor's composition and usage.

**Validates: Requirements 11.7**

### Property 40: Delegate Signature Inference

*For any* delegate binding, the system should infer the delegate signature from the bound function's signature.

**Validates: Requirements 11.8**

### Property 41: Incremental Compilation

*For any* file change in watch mode, the system should only recompile the changed file and its dependents, not the entire project.

**Validates: Requirements 12.7**

### Property 42: UE5 Header Parser Round-Trip

*For any* valid UE5 header file, parsing the header, pretty-printing the extracted API definitions, and parsing again should produce equivalent API knowledge.

**Validates: Requirements 16.6**

### Property 43: AST Serialization Round-Trip

*For any* valid KAIN AST, serializing the AST, deserializing it, and comparing should produce an equivalent AST structure.

**Validates: Requirements 17.5**

### Property 44: Corrupted Cache Detection

*For any* corrupted AST cache file, the deserializer should detect the corruption and trigger a re-parse of the source file rather than using invalid cached data.

**Validates: Requirements 17.3**

## Error Handling

### Error Categories

The system defines four main error categories, each with specific handling strategies:

**1. Parse Errors**
- Syntax errors in KAIN source code
- Invalid token sequences
- Malformed expressions or statements
- **Handling**: Report exact location with source context, suggest correct syntax

**2. Type Errors**
- Type mismatches in expressions
- Invalid type conversions
- Undefined type references
- **Handling**: Explain type mismatch, suggest valid alternatives, show expected vs actual types

**3. Semantic Errors**
- Invalid language constructs (e.g., component with default values)
- Circular dependencies
- Missing required elements (e.g., RPC _Implementation)
- **Handling**: Explain semantic rule violation, provide example correct code

**4. Oracle Validation Errors**
- Invalid UE5 API usage
- Missing module dependencies
- Deprecated API usage
- Platform/version incompatibilities
- **Handling**: Suggest correct API usage, provide documentation links, suggest alternatives

### Error Recovery Strategies

**Strategy 1: Partial Compilation**
- Continue compilation after non-fatal errors
- Generate code for valid portions
- Mark invalid portions with clear error comments
- Allow developers to see partial results

**Strategy 2: Suggestion-Based Recovery**
- Provide multiple fix suggestions for each error
- Rank suggestions by likelihood of correctness
- Include code examples for each suggestion
- Enable LLMs to automatically apply fixes

**Strategy 3: Cascading Error Prevention**
- Detect when one error causes multiple downstream errors
- Report only the root cause error
- Suppress cascading errors to reduce noise
- Re-validate after fixing root cause

**Strategy 4: Context-Aware Errors**
- Include relevant context in error messages
- Show related code locations
- Explain why the error occurred
- Provide links to documentation

### Error Message Template

All error messages follow this template for consistency:

```
❌ [ERROR_TYPE] in [FILE]:[LINE]:[COLUMN]

   [LINE] | [SOURCE CODE]
          | [HIGHLIGHT WITH ^]
          |
   [DETAILED EXPLANATION]
   
   Help: [ACTIONABLE SUGGESTION]
   
   Example:
   [CORRECT CODE EXAMPLE]
   
   Note: [ADDITIONAL CONTEXT OR BEST PRACTICES]
   
   [OPTIONAL: Related errors or documentation links]
```

### Error Handling in Multi-File Builds

**Per-File Error Isolation**
- Parse errors in one file don't prevent parsing other files
- Type errors in one file don't prevent type-checking other files
- Each file's errors are reported independently

**Cross-File Error Reporting**
- Circular dependency errors show all files in the cycle
- Missing type errors show where type is used and where it should be defined
- Include ordering errors show the dependency chain

**Error Aggregation**
- Group related errors together
- Show error count per file
- Provide summary of error types
- Enable filtering by error type or severity

## Testing Strategy

### Dual Testing Approach

The KAIN-PRO pipeline requires both unit testing and property-based testing for comprehensive coverage:

**Unit Tests**: Validate specific examples, edge cases, and error conditions
- Test specific UE5 patterns (e.g., "actor with 3 replicated properties")
- Test error message format for known error cases
- Test template rendering for specific inputs
- Test Oracle validation for known UE5 APIs

**Property Tests**: Validate universal properties across all inputs
- Test code generation correctness for randomly generated actors
- Test error message format for randomly generated errors
- Test template system for randomly generated templates
- Test Oracle validation for randomly generated API usage

**Balance**: Unit tests should focus on concrete examples and integration points, while property tests handle comprehensive input coverage through randomization.

### Property-Based Testing Configuration

**Library Selection**: Use `proptest` (Rust) or `hypothesis` (Python) for property-based testing

**Test Configuration**:
- Minimum 100 iterations per property test (due to randomization)
- Each property test references its design document property
- Tag format: `Feature: kain-pipeline-robustness, Property N: [property text]`

**Example Property Test**:

```rust
#[test]
fn test_replicated_property_codegen() {
    // Feature: kain-pipeline-robustness, Property 1: Replicated Property Code Generation
    proptest!(|(actor_def in arbitrary_actor_with_replicated_props())| {
        let generated_code = codegen.generate_actor(&actor_def)?;
        
        // Verify GetLifetimeReplicatedProps exists
        assert!(generated_code.contains("GetLifetimeReplicatedProps"));
        
        // Verify all replicated properties are registered
        for prop in actor_def.replicated_properties() {
            assert!(generated_code.contains(&format!("DOREPLIFETIME({}, {})", 
                actor_def.name, prop.name)));
        }
    });
}
```

### Test Categories

**1. Code Generation Tests**
- Test all UE5 patterns (actors, components, RPCs, replication)
- Test editor tool generation
- Test shader generation
- Test include ordering and forward declarations
- **Coverage**: Properties 1-6, 12, 16-18, 21

**2. Oracle Validation Tests**
- Test UE5 API validation
- Test module dependency detection
- Test deprecation warnings
- Test platform/version compatibility
- **Coverage**: Properties 7-11

**3. Error Message Tests**
- Test error message format
- Test error suggestions
- Test error recovery
- Test cascading error prevention
- **Coverage**: Properties 13-15

**4. Multi-File Build Tests**
- Test type resolution across files
- Test circular dependency detection
- Test dependency ordering
- Test incremental compilation
- **Coverage**: Properties 19-21, 41

**5. Template System Tests**
- Test template parameterization
- Test template composition
- Test conditional logic
- Test multi-file coordination
- **Coverage**: Properties 22-28

**6. Parser Tests**
- Test UE5 header parsing
- Test AST serialization
- Test round-trip properties
- Test error detection
- **Coverage**: Properties 29-33, 42-44

**7. Type Inference Tests**
- Test variable type inference
- Test function return type inference
- Test generic type inference
- Test lambda type inference
- **Coverage**: Properties 34-40

### Regression Testing

**Golden Reference Files**:
- Maintain golden reference files for generated code
- Compare generated code against references
- Update references when intentional changes are made
- Detect unintentional code generation changes

**UE5 Compilation Tests**:
- Compile generated plugins in actual UE5
- Verify no compilation errors
- Verify runtime behavior matches expectations
- Test across multiple UE5 versions (5.3, 5.4, 5.5)

**Performance Benchmarks**:
- Measure compilation time for various project sizes
- Track memory usage during compilation
- Detect performance regressions
- Ensure 100+ file projects compile in under 10 seconds

### Integration Testing

**End-to-End Plugin Tests**:
- Generate complete plugins from KAIN source
- Compile in UE5
- Load in UE5 Editor
- Test Blueprint integration
- Test networking functionality
- Test editor tool functionality

**External Tool Integration Tests**:
- Test libclang integration for header parsing
- Test clang-tidy integration for C++ validation
- Test DXC integration for shader validation
- Verify tool outputs are correctly processed

### Test Automation

**Continuous Integration**:
- Run all tests on every commit
- Test against multiple UE5 versions
- Test on multiple platforms (Windows, Linux, Mac)
- Generate test coverage reports

**Automated Regression Detection**:
- Compare test results against baseline
- Flag any new failures or performance regressions
- Automatically bisect to find regression-causing commits
- Notify developers of regressions immediately

## Implementation Phases

### Phase 1: Oracle Enhancement (Weeks 1-4)

**Goals**:
- Implement UE5 header parser using libclang
- Build UE5 API database for versions 5.3, 5.4, 5.5
- Implement Oracle validation interface
- Add pattern database for common UE5 patterns

**Deliverables**:
- Working UE5 header parser
- API database with 1000+ UE5 classes
- Oracle validation for classes, functions, properties
- Pattern validation for RPCs, replication, Blueprints

### Phase 2: Intelligent Code Generation (Weeks 5-8)

**Goals**:
- Implement template system architecture
- Create templates for all UE5 patterns
- Implement intelligent include resolution
- Add automatic forward declaration generation

**Deliverables**:
- Template system with 50+ templates
- Intelligent codegen for actors, components, RPCs
- Automatic include ordering
- Circular dependency resolution

### Phase 3: Editor Pipeline (Weeks 9-12)

**Goals**:
- Create comprehensive editor templates
- Implement asset editor generation
- Implement detail customization generation
- Implement viewport tool generation

**Deliverables**:
- Editor template library with 20+ templates
- Asset editor generation working
- Detail customization generation working
- Viewport tool generation working

### Phase 4: Error System Enhancement (Weeks 13-14)

**Goals**:
- Implement enhanced error message system
- Add error recovery strategies
- Implement suggestion generation
- Add error aggregation and filtering

**Deliverables**:
- LLM-friendly error messages
- Error recovery working
- Suggestion system generating helpful fixes
- Error aggregation reducing noise

### Phase 5: Shader Pipeline Robustness (Weeks 15-16)

**Goals**:
- Fix all known USF generation issues
- Implement shader validation
- Add RDG pass generation
- Improve shader error mapping

**Deliverables**:
- Robust USF generation
- Shader validation working
- RDG pass generation working
- Clear shader error messages

### Phase 6: Multi-File Build Enhancement (Weeks 17-18)

**Goals**:
- Improve dependency resolution
- Add incremental compilation
- Optimize build performance
- Implement build caching

**Deliverables**:
- Fast multi-file builds
- Incremental compilation working
- Build caching reducing rebuild time
- 100+ file projects building in <10s

### Phase 7: Type Inference 

**Goals**:
- Implement variable type inference
- Implement function return type inference
- Implement generic type inference
- Implement lambda type inference

**Deliverables**:
- Type inference working for all contexts
- Reduced boilerplate in KAIN code
- Better type error messages
- Improved developer experience

### Phase 8: Testing and Documentation 

**Goals**:
- Write comprehensive test suite
- Implement property-based tests
- Create regression test suite
- Write documentation and examples

**Deliverables**:
- 500+ unit tests
- 50+ property tests
- Regression test suite
- Complete documentation

### Phase 9: External Tool IntegratioN

**Goals**:
- Integrate clang-tidy for C++ validation
- Integrate DXC for shader validation
- Add documentation generation
- Implement IDE language server

**Deliverables**:
- C++ validation working
- Shader validation working
- Auto-generated documentation
- IDE support (VSCode extension)

### Phase 10: Performance Optimization 

**Goals**:
- Profile compilation pipeline
- Optimize hot paths
- Implement parallel compilation
- Add caching throughout

**Deliverables**:
- 2-5x faster compilation
- Reduced memory usage
- Parallel compilation working
- Comprehensive caching

## Success Metrics

### Robustness Metrics

**Error Detection Rate**: 95%+ of invalid KAIN code should be caught with clear error messages
**Error Recovery Rate**: 80%+ of errors should have actionable suggestions that LLMs can apply
**False Positive Rate**: <5% of valid KAIN code should be flagged as errors

### Performance Metrics

**Compilation Speed**: 100 file project should compile in <10 seconds
**Memory Usage**: Peak memory usage should be <2GB for 100 file projects
**Incremental Build**: Changing 1 file should rebuild in <1 second

### Quality Metrics

**UE5 Compilation Success**: 100% of generated plugins should compile in UE5 without errors
**Code Quality**: Generated code should pass clang-tidy with zero warnings
**Test Coverage**: 90%+ code coverage from unit and property tests

### Developer Experience Metrics

**LLM Success Rate**: LLMs should generate working plugins in <5 iterations
**Error Fix Time**: Average time to fix an error should be <2 minutes
**Documentation Quality**: 90%+ of users should find documentation helpful

## Future Enhancements

### Short-Term 
**Enhanced Oracle**:
- Support for custom UE5 plugins
- Support for third-party libraries
- Real-time API documentation lookup
- Intelligent API suggestion based on context

**Advanced Type Inference**:
- Whole-program type inference
- Type inference across file boundaries
- Inference of replication conditions
- Inference of Blueprint compatibility

**IDE Integration**:
- VSCode extension with full language support
- Real-time error highlighting
- Autocomplete with UE5 API knowledge
- Refactoring tools

### Medium-Term

**AI-Powered Code Generation**:
- LLM-based code suggestion
- Automatic bug fixing
- Code optimization suggestions
- Pattern recognition and refactoring

**Visual Programming**:
- Visual node editor for KAIN
- Blueprint-like interface
- Real-time preview of generated code
- Drag-and-drop component composition

**Marketplace Integration**:
- One-click plugin packaging
- Automated marketplace submission
- Version management
- Analytics and sales tracking

### Long-Term 

**Multi-Engine Support**:
- Unity code generation
- Godot code generation
- Custom engine support
- Cross-engine abstractions

**Cloud Compilation**:
- Cloud-based build service
- Distributed compilation
- Build caching across team
- CI/CD integration

**Advanced Verification**:
- Formal verification of generated code
- Automated testing generation
- Performance prediction
- Security analysis

## Conclusion

This design provides a comprehensive roadmap for transforming KAIN-PRO into the most robust, intelligent, and future-proof UE5 plugin generation system. By implementing enhanced Oracle knowledge, intelligent code generation, comprehensive editor support, and excellent error handling, we will create a system where "Unreal Engine 5 feels limitless" with agentic coding.

The phased implementation approach ensures steady progress with measurable milestones, while the comprehensive testing strategy ensures quality and prevents regressions. The result will be a system that enables LLMs to generate production-quality UE5 plugins with zero manual intervention, revolutionizing UE5 plugin development.
