# Requirements Document: KAIN Pipeline Robustness

## Introduction

The KAIN compiler transforms .kn source files into production-ready UE5 C++ plugins through a multi-stage pipeline involving four specialized codegen crates. While the pipeline successfully generates functional code for basic cases, it contains numerous fragile areas that prevent it from being production-ready. This specification addresses systematic hardening of error handling, validation, type mapping, shader generation, editor codegen, and naming conventions to achieve zero-crash, first-compile-success reliability.

## Glossary

- **KAIN**: Python-like language that compiles to UE5 C++
- **Packager**: CLI orchestrator that coordinates the build pipeline
- **Runtime_Codegen**: The ue5 crate that generates actors, components, structs, enums
- **Editor_Codegen**: The ue5-editor crate that generates Slate UI, Details panels, Viewports
- **Shader_Codegen**: The ue5-shaders crate that generates HLSL .usf files
- **Oracle**: Semantic validator that checks for UE5-specific correctness issues
- **Ue5Context**: Shared state object passed between codegen crates
- **EngineKnowledge**: Database of UE5 engine types, constructors, and conventions
- **Type_Mapping**: Process of converting KAIN types to UE5 C++ types with correct prefixes
- **Post_Processor**: Python script that cleans up generated C++ code
- **POD_Struct**: Plain Old Data structure used in shader parameter passing
- **Permutation**: Shader variant controlled by compile-time flags (CFG_*, ENABLE_*)

## Requirements

### Requirement 1: Graceful Error Handling

**User Story:** As a KAIN developer, I want all compilation errors to provide clear file:line:column context and actionable messages, so that I can fix issues immediately without the compiler crashing.

#### Acceptance Criteria

1. WHEN the Packager encounters any error condition, THEN the System SHALL return a Result type with structured error context including file path, line number, column number, and error description
2. WHEN the Runtime_Codegen encounters invalid AST nodes, THEN the System SHALL return descriptive errors rather than calling unwrap() or panic()
3. WHEN the Editor_Codegen encounters unsupported widget configurations, THEN the System SHALL return errors with suggestions for valid alternatives
4. WHEN the Shader_Codegen encounters HLSL keyword collisions, THEN the System SHALL return errors identifying the conflicting identifier and suggesting a rename
5. WHEN type mapping fails to resolve a type, THEN the System SHALL return an error listing available types and suggesting closest matches
6. WHEN the Oracle detects semantic violations, THEN the System SHALL return errors with references to the specific EARS requirement violated
7. WHEN file I/O operations fail, THEN the System SHALL return errors with the file path and OS error details
8. WHEN JSON parsing fails for KAIN.toml or engine_knowledge.json, THEN the System SHALL return errors with the JSON path and expected schema
9. WHEN the Post_Processor encounters malformed C++ code, THEN the System SHALL log warnings but continue processing rather than crashing
10. WHEN multiple errors occur in a single build, THEN the System SHALL collect and report all errors rather than stopping at the first failure

### Requirement 2: Centralized Type Mapping

**User Story:** As a KAIN compiler maintainer, I want all type mapping logic centralized in a single module, so that double-prefixing bugs and inconsistent type conversions are eliminated.

#### Acceptance Criteria

1. WHEN any codegen crate needs to map a KAIN type to UE5 C++, THEN the System SHALL use types.rs as the single source of truth
2. WHEN mapping an actor type, THEN the System SHALL apply A-prefix only if not already present
3. WHEN mapping a struct type, THEN the System SHALL apply F-prefix only if not already present
4. WHEN mapping an enum type, THEN the System SHALL apply E-prefix only if not already present
5. WHEN mapping a component type, THEN the System SHALL apply U-prefix and Component suffix only if not already present
6. WHEN mapping a pointer type, THEN the System SHALL append * only for UObject-derived types
7. WHEN mapping array types, THEN the System SHALL use TArray<T> with correctly mapped inner type
8. WHEN mapping delegate types, THEN the System SHALL use the delegate registry to resolve the correct UE5 macro name
9. WHEN mapping engine types, THEN the System SHALL query EngineKnowledge for the canonical name and include path
10. WHEN a type is already prefixed in the KAIN source, THEN the System SHALL detect the prefix and not double-apply it

### Requirement 3: Complete Oracle Validation

**User Story:** As a KAIN developer, I want the compiler to catch all UE5-specific semantic errors before codegen, so that generated code compiles in UE5 on the first try.

#### Acceptance Criteria

1. WHEN an actor contains replicated properties, THEN the Oracle SHALL verify GetLifetimeReplicatedProps will be generated
2. WHEN an RPC function is declared, THEN the Oracle SHALL verify the naming convention (Server_*, Client_*, Multicast_*) is followed
3. WHEN a @datatable struct is declared, THEN the Oracle SHALL verify it contains only UE5-serializable field types
4. WHEN a @component struct is declared, THEN the Oracle SHALL verify it does not contain actor-only features
5. WHEN a shader uniform is declared, THEN the Oracle SHALL verify it has a binding slot (@N) and the slot is unique
6. WHEN a shader permutation uniform is declared, THEN the Oracle SHALL verify it uses the CFG_* or ENABLE_* naming convention
7. WHEN a Slate widget references a delegate, THEN the Oracle SHALL verify the delegate signature matches the expected InArgs type
8. WHEN a Details panel uses @slider, THEN the Oracle SHALL verify min and max values are provided
9. WHEN an asset editor is declared, THEN the Oracle SHALL verify it specifies at least one supported asset type
10. WHEN a type name collides with a UE5 engine type, THEN the Oracle SHALL return an error suggesting a different name
11. WHEN a function name collides with a UE5 reserved keyword, THEN the Oracle SHALL return an error suggesting a different name
12. WHEN circular dependencies exist between types, THEN the Oracle SHALL detect and report the dependency cycle

### Requirement 4: Shader Pipeline Validation

**User Story:** As a KAIN developer writing shaders, I want comprehensive validation of shader code before and after codegen, so that HLSL compilation errors are caught early with clear messages.

#### Acceptance Criteria

1. WHEN a shader is parsed, THEN the System SHALL validate all uniform bindings are unique within the shader
2. WHEN a shader uses a permutation uniform, THEN the System SHALL validate it is used in conditional branches
3. WHEN a shader declares a POD struct, THEN the System SHALL validate field types are HLSL-compatible
4. WHEN shader codegen generates a POD struct, THEN the System SHALL validate it is not redefined if already declared in the .kn source
5. WHEN shader codegen generates HLSL code, THEN the System SHALL validate no C++ keywords are used as HLSL identifiers
6. WHEN a shader references a texture sampler, THEN the System SHALL validate the binding slot does not conflict with uniforms
7. WHEN a compute shader is declared, THEN the System SHALL validate thread group dimensions are specified
8. WHEN a surface shader is declared, THEN the System SHALL validate it returns a SurfaceOutput struct
9. WHEN shader permutations are declared, THEN the System SHALL validate permutation combinations do not exceed UE5 limits
10. WHEN generated .usf files are written, THEN the System SHALL validate they contain no syntax errors detectable by regex patterns

### Requirement 5: Complete Editor Codegen

**User Story:** As a KAIN developer creating editor tools, I want all editor features fully implemented without TODO stubs, so that generated plugins provide complete functionality.

#### Acceptance Criteria

1. WHEN an asset editor is declared, THEN the System SHALL generate complete FAssetEditorToolkit implementation with no TODO comments
2. WHEN a viewport is declared with @scene_actor, THEN the System SHALL generate actor spawning and management code
3. WHEN a viewport is declared with @camera, THEN the System SHALL generate camera setup and control code
4. WHEN a toolbar is declared, THEN the System SHALL generate all button handlers, toggle state management, and shortcut registration
5. WHEN a Details panel uses @color_picker, THEN the System SHALL generate FLinearColor property handling with color picker widget
6. WHEN a Details panel uses @button, THEN the System SHALL generate button click handler delegation
7. WHEN a Slate widget uses nested composition, THEN the System SHALL generate correct SNew() chain with proper indentation
8. WHEN an editor module is declared with @menu_entry, THEN the System SHALL generate menu extension registration code
9. WHEN an editor module is declared with @toolbar_button, THEN the System SHALL generate toolbar extension registration code
10. WHEN multiple editor features are combined, THEN the System SHALL generate correct initialization order and dependency wiring

### Requirement 6: Module Dependency Resolution

**User Story:** As a KAIN developer, I want automatic and validated module dependency resolution, so that .Build.cs files are correct and plugins link successfully.

#### Acceptance Criteria

1. WHEN the Packager analyzes generated code, THEN the System SHALL detect all UE5 module dependencies from include statements
2. WHEN a shader is used, THEN the System SHALL automatically add RenderCore and RHI modules to dependencies
3. WHEN Slate widgets are used, THEN the System SHALL automatically add Slate and SlateCore modules to dependencies
4. WHEN Details panels are used, THEN the System SHALL automatically add PropertyEditor module to dependencies
5. WHEN asset editors are used, THEN the System SHALL automatically add UnrealEd and AssetTools modules to dependencies
6. WHEN networking features are used, THEN the System SHALL automatically add Engine and NetCore modules to dependencies
7. WHEN the System generates .Build.cs, THEN it SHALL include all detected dependencies in PublicDependencyModuleNames
8. WHEN optional engine_modules.json is missing, THEN the System SHALL use built-in default module mappings
9. WHEN engine_modules.json is present, THEN the System SHALL validate its schema before using it
10. WHEN circular module dependencies are detected, THEN the System SHALL return an error with the dependency chain

### Requirement 7: Robust Naming Conventions

**User Story:** As a KAIN developer, I want the naming system to handle all edge cases correctly, so that generated identifiers are always valid UE5 C++ names.

#### Acceptance Criteria

1. WHEN a type name already has a UE5 prefix, THEN the System SHALL detect it and not add a duplicate prefix
2. WHEN a type name contains numbers, THEN the System SHALL preserve them in the correct position
3. WHEN a type name contains underscores, THEN the System SHALL convert to PascalCase while preserving semantic boundaries
4. WHEN a type name starts with a number, THEN the System SHALL return an error indicating invalid identifier
5. WHEN a type name contains special characters, THEN the System SHALL return an error listing valid characters
6. WHEN a type name is a C++ keyword, THEN the System SHALL return an error suggesting an alternative
7. WHEN a type name is a UE5 macro name, THEN the System SHALL return an error suggesting an alternative
8. WHEN converting to snake_case for file names, THEN the System SHALL handle consecutive capitals correctly
9. WHEN generating delegate names, THEN the System SHALL follow UE5 delegate naming conventions with correct prefixes
10. WHEN generating RPC function names, THEN the System SHALL preserve the Server_/Client_/Multicast_ prefix in the implementation name

### Requirement 8: Enhanced Post-Processing

**User Story:** As a KAIN compiler maintainer, I want the post-processor to handle all code cleanup tasks, so that generated C++ is properly formatted and complete.

#### Acceptance Criteria

1. WHEN generated code contains replication macros, THEN the Post_Processor SHALL add GetLifetimeReplicatedProps implementation
2. WHEN generated code uses shaders, THEN the Post_Processor SHALL add shader initialization in BeginPlay
3. WHEN generated code has missing forward declarations, THEN the Post_Processor SHALL add them in the correct order
4. WHEN generated code has excessive blank lines, THEN the Post_Processor SHALL normalize to single blank lines
5. WHEN generated code has inconsistent indentation, THEN the Post_Processor SHALL normalize to tabs
6. WHEN generated code has missing include guards, THEN the Post_Processor SHALL add them with correct macro names
7. WHEN generated code has includes in wrong order, THEN the Post_Processor SHALL reorder to UE5 conventions
8. WHEN generated code has trailing whitespace, THEN the Post_Processor SHALL remove it
9. WHEN generated code has CRLF line endings, THEN the Post_Processor SHALL normalize to LF
10. WHEN the Post_Processor encounters unparseable C++, THEN it SHALL log a warning and skip that file rather than crashing

### Requirement 9: Comprehensive Test Coverage

**User Story:** As a KAIN compiler maintainer, I want extensive test coverage of edge cases and error paths, so that regressions are caught immediately.

#### Acceptance Criteria

1. WHEN the test suite runs, THEN the System SHALL execute at least 100 test cases covering all codegen crates
2. WHEN testing type mapping, THEN the System SHALL verify all prefix edge cases (already-prefixed, numbers, underscores)
3. WHEN testing error handling, THEN the System SHALL verify all unwrap() calls have been replaced with proper error propagation
4. WHEN testing Oracle validation, THEN the System SHALL verify all semantic rules are enforced
5. WHEN testing shader codegen, THEN the System SHALL verify POD struct handling, permutations, and HLSL keyword avoidance
6. WHEN testing editor codegen, THEN the System SHALL verify all widget types, property types, and composition patterns
7. WHEN testing naming conventions, THEN the System SHALL verify all edge cases produce valid UE5 identifiers
8. WHEN testing module dependencies, THEN the System SHALL verify correct detection from various code patterns
9. WHEN testing post-processing, THEN the System SHALL verify all cleanup operations work correctly
10. WHEN testing the full pipeline, THEN the System SHALL verify end-to-end builds of complex plugins succeed

### Requirement 10: Data-Driven Validation Rules

**User Story:** As a KAIN compiler maintainer, I want validation rules loaded from JSON configuration, so that rules can be updated without recompiling the compiler.

#### Acceptance Criteria

1. WHEN the Oracle initializes, THEN the System SHALL load validation rules from validation_rules.json if present
2. WHEN validation_rules.json defines a type collision rule, THEN the Oracle SHALL enforce it during type checking
3. WHEN validation_rules.json defines a naming convention rule, THEN the Oracle SHALL enforce it during name validation
4. WHEN validation_rules.json defines a semantic constraint rule, THEN the Oracle SHALL enforce it during semantic analysis
5. WHEN validation_rules.json is malformed, THEN the System SHALL return an error with the JSON path and expected schema
6. WHEN validation_rules.json is missing, THEN the System SHALL use built-in default rules
7. WHEN a validation rule is disabled in JSON, THEN the Oracle SHALL skip that check
8. WHEN a validation rule specifies a custom error message, THEN the Oracle SHALL use that message in error output
9. WHEN validation rules are updated, THEN the System SHALL reload them on the next build without requiring recompilation
10. WHEN validation rules conflict, THEN the System SHALL return an error identifying the conflicting rules

### Requirement 11: Shader Virtual Path Resolution

**User Story:** As a KAIN developer using shaders, I want automatic and correct virtual shader path setup, so that UE5 can locate and compile generated .usf files without "could not find virtual shader path" errors.

#### Acceptance Criteria

1. WHEN the Packager generates a plugin with shaders, THEN the System SHALL create a Shaders/ directory in the plugin root
2. WHEN .usf files are generated, THEN the System SHALL write them to PluginRoot/Shaders/ with correct relative paths
3. WHEN the .uplugin file is generated, THEN the System SHALL include CanContainContent: true to enable shader loading
4. WHEN the module initialization code is generated, THEN the System SHALL add FShaderSourceFilePathMapping registration
5. WHEN registering shader paths, THEN the System SHALL use /Plugin/PluginName as the virtual path prefix
6. WHEN registering shader paths, THEN the System SHALL map the virtual path to the physical Shaders/ directory
7. WHEN multiple shaders exist, THEN the System SHALL ensure all share the same virtual path mapping
8. WHEN shader includes reference other shaders, THEN the System SHALL validate the virtual paths resolve correctly
9. WHEN the plugin is loaded in UE5, THEN the System SHALL ensure shader registration happens before any shader usage
10. WHEN shader compilation fails with path errors, THEN the System SHALL provide diagnostics showing the virtual path mapping and physical file location

### Requirement 12: Non-Breaking Incremental Improvements

**User Story:** As a KAIN compiler maintainer, I want all robustness improvements to be backward-compatible, so that existing working plugins continue to build successfully.

#### Acceptance Criteria

1. WHEN new validation rules are added, THEN the System SHALL not break existing valid .kn files that previously compiled
2. WHEN error handling is improved, THEN the System SHALL maintain the same successful code generation for valid inputs
3. WHEN type mapping is centralized, THEN the System SHALL produce identical output for all existing test cases
4. WHEN Oracle validation is enhanced, THEN the System SHALL only add new checks that catch actual errors
5. WHEN shader validation is added, THEN the System SHALL not reject valid shader code that previously worked
6. WHEN editor codegen is completed, THEN the System SHALL maintain compatibility with existing editor features
7. WHEN naming conventions are hardened, THEN the System SHALL handle all previously valid names correctly
8. WHEN post-processing is enhanced, THEN the System SHALL not corrupt previously correct generated code
9. WHEN module dependencies are auto-detected, THEN the System SHALL include all modules that were previously manually specified
10. WHEN the test suite is expanded, THEN all existing 32 tests SHALL continue to pass
11. WHEN any refactoring is performed, THEN the System SHALL verify output equivalence using snapshot testing
12. WHEN new features are added, THEN the System SHALL use feature flags to allow gradual rollout without breaking existing builds

### Requirement 13: Metadata System Robustness

**User Story:** As a KAIN compiler maintainer, I want all UE5 knowledge loaded from validated JSON metadata files, so that the compiler can be updated for new UE5 versions without recompilation and all codegen decisions are data-driven.

#### Acceptance Criteria

1. WHEN the System initializes, THEN it SHALL load all metadata files from unreal/metadata/ with schema validation
2. WHEN engine_knowledge.json is loaded, THEN the System SHALL validate it contains required fields (types, constructors, includes, colors)
3. WHEN module_graph.json is loaded, THEN the System SHALL validate it contains module dependencies and include mappings
4. WHEN uht_rules.json is loaded, THEN the System SHALL validate it contains UHT validation rules and constraints
5. WHEN shader_knowledge.json is loaded, THEN the System SHALL validate it contains HLSL types, keywords, and binding rules
6. WHEN widget_registry.json is loaded, THEN the System SHALL validate it contains Slate widget types and property mappings
7. WHEN editor_attributes.json is loaded, THEN the System SHALL validate it contains editor attribute definitions and constraints
8. WHEN virtual_obligations.json is loaded, THEN the System SHALL validate it contains virtual function requirements
9. WHEN any metadata file is malformed, THEN the System SHALL return an error with the file path, JSON path, and expected schema
10. WHEN any metadata file is missing, THEN the System SHALL return an error indicating which file is required and where it should be located
11. WHEN metadata extraction scripts are run, THEN they SHALL support multi-drive UE5 installations (D:, M:, etc.) via configuration
12. WHEN metadata extraction scripts are run, THEN they SHALL support multiple UE5 versions (5.4, 5.5, 5.6, 5.7) and generate version-specific files
13. WHEN type mapping needs UE5 type information, THEN the System SHALL query engine_knowledge.json before using hardcoded fallbacks
14. WHEN module dependency resolution needs include-to-module mappings, THEN the System SHALL query module_graph.json before using hardcoded fallbacks
15. WHEN Oracle validation needs UE5 semantic rules, THEN the System SHALL query uht_rules.json before using hardcoded fallbacks
16. WHEN shader codegen needs HLSL type information, THEN the System SHALL query shader_knowledge.json before using hardcoded fallbacks
17. WHEN editor codegen needs widget information, THEN the System SHALL query widget_registry.json before using hardcoded fallbacks
18. WHEN the System encounters an unknown type, THEN it SHALL check all metadata files before returning an error
19. WHEN metadata files are updated, THEN the System SHALL reload them on the next build without requiring recompilation
20. WHEN metadata is incomplete, THEN the System SHALL log warnings indicating which entries are missing and continue with fallback behavior
