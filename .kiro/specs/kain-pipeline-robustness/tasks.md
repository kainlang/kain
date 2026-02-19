# Implementation Plan: KAIN Pipeline Robustness

## Overview

This implementation plan systematically hardens the KAIN UE5 codegen pipeline through 11 phases (Phase 0-10), each building on the previous. The plan prioritizes the metadata-first architecture (Phase 0) to ensure all UE5 knowledge is properly validated and loaded before proceeding with other improvements. It emphasizes non-breaking changes (Requirement 12) and uses incremental validation to ensure existing functionality continues to work. Each phase includes implementation tasks followed by testing tasks to validate the changes.

## Tasks

- [-] 0. Phase 0: Metadata System Validation & Enhancement
  - [x] 0.1 Implement metadata schema validation
    - Create JSON schemas for all 14 metadata files
    - Implement schema validation on load using serde_json + jsonschema
    - Return structured errors with file path and JSON path on validation failure
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7, 13.8, 13.9_
  
  - [x] 0.2 Add metadata completeness checks
    - Implement checks for required fields in each metadata file
    - Log warnings for missing optional fields
    - Return errors for missing required files
    - _Requirements: 13.10, 13.20_
  
  - [x] 0.3 Update metadata extraction scripts for multi-drive support
    - Add configuration file for UE5 installation paths (D:, M:, etc.)
    - Update ue5_scanner.py to read from config
    - Update all extraction scripts to support configurable paths
    - Test with UE5 5.4, 5.5, 5.6, 5.7 installations
    - _Requirements: 13.11, 13.12_
  
  - [ ] 0.4 Expand engine_knowledge.json
    - Add missing UObject-derived types
    - Add missing constructor signatures
    - Add missing include paths
    - Validate against UE5 5.4-5.7 headers
    - _Requirements: 13.13, 13.18_
  
  - [ ] 0.5 Expand module_graph.json
    - Add missing include-to-module mappings
    - Add module dependency chains
    - Validate against UE5 .Build.cs files
    - _Requirements: 13.14, 13.18_
  
  - [ ] 0.6 Expand uht_rules.json
    - Add missing UHT validation rules
    - Add attribute compatibility rules
    - Add replication rules
    - Validate against UHT source code
    - _Requirements: 13.15, 13.18_
  
  - [ ] 0.7 Expand shader_knowledge.json
    - Add missing HLSL types
    - Add HLSL keyword list
    - Add binding slot rules
    - Validate against HLSL documentation
    - _Requirements: 13.16, 13.18_
  
  - [ ] 0.8 Expand widget_registry.json
    - Add missing Slate widget types
    - Add property type mappings
    - Add widget composition rules
    - Validate against Slate source code
    - _Requirements: 13.17, 13.18_
  
  - [x] 0.9 Create metadata refresh workflow
    - Document how to run extraction scripts
    - Create batch script for full metadata refresh
    - Add version detection for UE5 installations
    - Test refresh workflow on all UE5 versions
    - _Requirements: 13.11, 13.12, 13.19_
  
  - [x] 0.10 Implement metadata hot-reload
    - Add file watching for metadata directory
    - Reload metadata on file changes
    - Validate new metadata before applying
    - _Requirements: 13.19_
  
  - [ ]* 0.11 Write unit tests for metadata validation
    - Test schema validation with valid JSON
    - Test schema validation with invalid JSON
    - Test missing file handling
    - Test malformed JSON handling
    - _Requirements: 13.1-13.10_
  
  - [ ]* 0.12 Write unit tests for metadata queries
    - Test engine_knowledge queries
    - Test module_graph queries
    - Test uht_rules queries
    - Test fallback behavior when metadata is incomplete
    - _Requirements: 13.13-13.18, 13.20_
  
  - [ ]* 0.13 Document metadata file formats
    - Create schema documentation for each file
    - Add examples for each metadata type
    - Document extraction script usage
    - Document refresh workflow
    - _Requirements: 13.11, 13.12_

- [ ] 0.14 Checkpoint - Verify metadata system
  - Run metadata validation on all files
  - Test metadata queries in Ue5Context
  - Verify extraction scripts work with multi-drive installations
  - Ask user if questions arise

- [ ] 1. Phase 1: Error Handling Foundation
  - [x] 1.1 Enhance KainError struct with location tracking
    - Add file: Option<PathBuf>, location: Option<(usize, usize)>, context: String, suggestion: Option<String> fields
    - Implement Display trait for user-friendly error messages
    - _Requirements: 1.1, 1.2_
  
  - [x] 1.2 Implement ErrorContext trait for error chaining
    - Create trait with with_file(), with_location(), with_context(), with_suggestion() methods
    - Implement for Result<T, KainError>
    - Add usage examples in documentation
    - _Requirements: 1.1, 1.2_
  
  - [x] 1.3 Replace unwrap() calls in packager/codegen.rs
    - Audit all unwrap() calls (20+ identified)
    - Replace with proper error handling using ErrorContext
    - Add file:line:col context to all errors
    - _Requirements: 1.1, 1.7, 1.8_
  
  - [ ]* 1.4 Write unit tests for error handling
    - Test error message formatting
    - Test error context chaining
    - Test file:line:col tracking
    - _Requirements: 1.1, 1.2_
  
  - [ ]* 1.5 Write property test for graceful error handling
    - **Property 1: Graceful Error Handling**
    - **Validates: Requirements 1.1, 1.2, 1.7**

- [ ] 2. Phase 2: Type Mapping Centralization
  - [x] 2.1 Create TypeMapper struct in types.rs
    - Define TypeMapper with config, knowledge, registry fields
    - Implement new(), register_enum(), register_struct(), register_actor(), register_component(), register_delegate()
    - Add MappedType struct with cpp_type, is_pointer, needs_forward_decl, include_path, prefix fields
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_
  
  - [x] 2.2 Implement centralized map_type() method
    - Move all type mapping logic from packager/codegen into TypeMapper
    - Handle primitives, engine types, user types, generics
    - Implement prefix detection to prevent double-prefixing
    - _Requirements: 2.2, 2.3, 2.4, 2.5, 2.10_
  
  - [x] 2.3 Implement pointer type detection
    - Create is_pointer_type() method
    - Query EngineKnowledge for UObject-derived types
    - Handle actor, component, and object references
    - _Requirements: 2.6_
  
  - [x] 2.4 Migrate packager to use TypeMapper
    - Update packager/codegen.rs to use TypeMapper::map_type()
    - Remove duplicate type mapping code
    - Update delegate header generation to use TypeMapper
    - _Requirements: 2.1_
  
  - [x] 2.5 Migrate runtime codegen to use TypeMapper
    - Update ue5/src/codegen_ue5.rs to use TypeMapper
    - Remove inline type mapping logic
    - Ensure all type references go through TypeMapper
    - _Requirements: 2.1_
  
  - [x] 2.6 Migrate editor codegen to use TypeMapper
    - Update ue5-editor/src/editor/codegen.rs to use TypeMapper
    - Update slate.rs, details.rs to use TypeMapper
    - Remove duplicate type mapping code
    - _Requirements: 2.1_
  
  - [ ]* 2.7 Write unit tests for type mapping edge cases
    - Test already-prefixed names (EHealthStatus → EHealthStatus)
    - Test names with numbers (Player2 → APlayer2)
    - Test names with underscores (health_component → UHealthComponent)
    - _Requirements: 2.2, 2.3, 2.4, 2.5, 2.10_
  
  - [ ]* 2.8 Write property test for no double-prefixing
    - **Property 4: No Double-Prefixing**
    - **Validates: Requirements 2.2, 2.3, 2.4, 2.5, 2.10, 7.1**
  
  - [ ]* 2.9 Write property test for pointer type consistency
    - **Property 5: Pointer Type Consistency**
    - **Validates: Requirements 2.6**
  
  - [ ]* 2.10 Write property test for recursive type mapping
    - **Property 6: Recursive Type Mapping**
    - **Validates: Requirements 2.7, 2.8**

- [x] 3. Checkpoint - Verify type mapping refactor
  - Ensure all existing tests pass
  - Run snapshot tests to verify output equivalence
  - Ask user if questions arise

- [ ] 4. Phase 3: Oracle Validation Enhancement
  - [x] 4.1 Implement replication validation
    - Add validate_replication() method to Oracle
    - Check all replicated properties have GetLifetimeReplicatedProps
    - Verify RPC naming conventions
    - Validate replicated types are serializable
    - _Requirements: 3.1, 3.2_
  
  - [ ] 4.2 Implement RPC validation
    - Add validate_rpcs() method to Oracle
    - Verify Server_*, Client_*, Multicast_* naming
    - Check no delegate parameters in RPCs
    - Validate RPC parameter types are serializable
    - _Requirements: 3.2_
  
  - [ ] 4.3 Implement datatable validation
    - Add validate_datatables() method to Oracle
    - Verify all fields are UE5-serializable
    - Check no pointers in datatable structs
    - Validate inheritance from FTableRowBase
    - _Requirements: 3.3_
  
  - [ ] 4.4 Implement component validation
    - Add validate_components() method to Oracle
    - Verify no actor-only features
    - Check proper component lifecycle
    - _Requirements: 3.4_
  
  - [ ] 4.5 Implement name collision detection
    - Add validate_name_collisions() method to Oracle
    - Check against EngineKnowledge for engine types
    - Check for C++ keywords
    - Check for UE5 macro names
    - _Requirements: 3.10, 3.11_
  
  - [ ] 4.6 Implement circular dependency detection
    - Add validate_circular_dependencies() method to Oracle
    - Build dependency graph from type references
    - Detect cycles using depth-first search
    - Suggest forward declarations in error messages
    - _Requirements: 3.12_
  
  - [ ]* 4.7 Write unit tests for Oracle validation rules
    - Test replication validation with known cases
    - Test RPC naming validation
    - Test datatable field validation
    - Test name collision detection
    - _Requirements: 3.1, 3.2, 3.3, 3.10, 3.11_
  
  - [ ]* 4.8 Write property test for replication validation
    - **Property 14: Replication Validation**
    - **Validates: Requirements 3.1**
  
  - [ ]* 4.9 Write property test for RPC naming validation
    - **Property 15: RPC Naming Validation**
    - **Validates: Requirements 3.2**
  
  - [ ]* 4.10 Write property test for name collision detection
    - **Property 18: Name Collision Detection**
    - **Validates: Requirements 3.10, 3.11**

- [ ] 5. Phase 4: Shader Pipeline Validation
  - [ ] 5.1 Create ShaderValidator struct in ue5-shaders/src/validation.rs
    - Define ShaderValidator with hlsl_keywords, reserved_bindings fields
    - Implement new() with HLSL keyword list
    - Add validate_shader() orchestrator method
    - _Requirements: 4.1, 4.5_
  
  - [ ] 5.2 Implement uniform validation
    - Add validate_uniforms() method
    - Check unique binding slots within shader
    - Verify types are HLSL-compatible
    - Validate permutation naming (CFG_*, ENABLE_*)
    - _Requirements: 3.5, 3.6, 4.1, 4.2_
  
  - [ ] 5.3 Implement POD struct validation
    - Add validate_pod_structs() method
    - Check for redefinitions
    - Verify field types are HLSL-compatible
    - Validate alignment requirements
    - _Requirements: 4.3, 4.4_
  
  - [ ] 5.4 Implement HLSL syntax validation
    - Add validate_hlsl_syntax() method
    - Check for C++ keywords used as HLSL identifiers
    - Validate function signatures
    - Check return types
    - _Requirements: 4.5_
  
  - [ ] 5.5 Implement binding conflict detection
    - Add validate_bindings() method
    - Check texture sampler slots don't conflict with uniforms
    - Verify all slots are within UE5 limits
    - _Requirements: 4.6_
  
  - [ ] 5.6 Integrate ShaderValidator into shader codegen
    - Call validate_shader() before generating .usf files
    - Return validation errors with file:line:col context
    - Add suggestions for common errors
    - _Requirements: 4.1, 4.5_
  
  - [ ] 5.7 Implement shader virtual path resolution
    - Add FShaderSourceFilePathMapping registration to module init
    - Use /Plugin/PluginName as virtual path prefix
    - Map to physical Shaders/ directory
    - Ensure registration happens before shader usage
    - _Requirements: 11.4, 11.5, 11.6, 11.9_
  
  - [ ] 5.8 Update .uplugin generation for shaders
    - Add CanContainContent: true when shaders are present
    - Ensure Shaders/ directory is created
    - Write .usf files to correct location
    - _Requirements: 11.1, 11.2, 11.3_
  
  - [ ]* 5.9 Write unit tests for shader validation
    - Test duplicate binding slots
    - Test POD struct redefinitions
    - Test HLSL keyword collisions
    - Test invalid permutation names
    - _Requirements: 4.1, 4.3, 4.4, 4.5_
  
  - [ ]* 5.10 Write property test for unique binding slots
    - **Property 20: Unique Binding Slots**
    - **Validates: Requirements 3.5, 4.1**
  
  - [ ]* 5.11 Write property test for POD struct HLSL compatibility
    - **Property 22: POD Struct HLSL Compatibility**
    - **Validates: Requirements 4.3**
  
  - [ ]* 5.12 Write property test for virtual path consistency
    - **Property 41: Virtual Path Consistency**
    - **Validates: Requirements 11.5, 11.6, 11.7**

- [ ] 6. Checkpoint - Verify shader validation
  - Build ultimate.kn test plugin
  - Verify shader compilation succeeds
  - Check for "could not find virtual shader path" errors
  - Ask user if questions arise

- [ ] 7. Phase 5: Editor Codegen Completion
  - [ ] 7.1 Complete asset editor generation (remove TODOs)
    - Implement complete FAssetEditorToolkit generation
    - Add asset type registration
    - Generate toolbar, menu, and docking layout
    - _Requirements: 5.1_
  
  - [ ] 7.2 Complete viewport generation (remove TODOs)
    - Implement @scene_actor actor spawning and management
    - Implement @camera setup and control code
    - Add viewport client initialization
    - _Requirements: 5.2, 5.3_
  
  - [ ] 7.3 Complete toolbar generation (remove TODOs)
    - Implement all button handlers
    - Add toggle state management
    - Generate shortcut registration
    - _Requirements: 5.4_
  
  - [ ] 7.4 Complete Details panel generation (remove TODOs)
    - Implement @color_picker with FLinearColor handling
    - Implement @button with click handler delegation
    - Add property change notifications
    - _Requirements: 5.5, 5.6_
  
  - [ ] 7.5 Implement editor feature integration
    - Generate correct initialization order
    - Wire dependencies between features
    - Add proper cleanup in shutdown
    - _Requirements: 5.10_
  
  - [ ]* 7.6 Write unit tests for editor codegen
    - Test asset editor generation completeness
    - Test viewport generation with @scene_actor and @camera
    - Test Details panel property types
    - _Requirements: 5.1, 5.2, 5.3, 5.5, 5.6_
  
  - [ ]* 7.7 Write property test for no TODO stubs
    - **Property 27: No TODO Stubs**
    - **Validates: Requirements 5.1**
  
  - [ ]* 7.8 Write property test for Slate composition correctness
    - **Property 28: Slate Composition Correctness**
    - **Validates: Requirements 5.7**

- [ ] 8. Phase 6: Module Dependency Resolution
  - [ ] 8.1 Create DependencyResolver struct in cli/src/packager/dependencies.rs
    - Define DependencyResolver with knowledge, module_map fields
    - Load module mappings from engine_modules.json or use defaults
    - Implement analyze() method
    - _Requirements: 6.1, 6.8_
  
  - [ ] 8.2 Implement include-based dependency detection
    - Parse #include statements from generated files
    - Map includes to UE5 modules using module_map
    - Detect circular dependencies
    - _Requirements: 6.1, 6.10_
  
  - [ ] 8.3 Implement automatic module addition
    - Add RenderCore, RHI for shaders
    - Add Slate, SlateCore for Slate widgets
    - Add PropertyEditor for Details panels
    - Add UnrealEd, AssetTools for asset editors
    - Add Engine, NetCore for networking
    - _Requirements: 6.2, 6.3, 6.4, 6.5, 6.6_
  
  - [ ] 8.4 Implement .Build.cs generation
    - Generate PublicDependencyModuleNames from detected modules
    - Add PrivateDependencyModuleNames for internal modules
    - Include all manually specified modules
    - _Requirements: 6.7_
  
  - [ ] 8.5 Implement dependency validation
    - Check for circular module dependencies
    - Verify all modules exist in UE5
    - Warn about missing optional modules
    - _Requirements: 6.10_
  
  - [ ]* 8.6 Write unit tests for dependency resolution
    - Test include parsing
    - Test module mapping
    - Test circular dependency detection
    - _Requirements: 6.1, 6.10_
  
  - [ ]* 8.7 Write property test for include-based dependency detection
    - **Property 30: Include-Based Dependency Detection**
    - **Validates: Requirements 6.1**
  
  - [ ]* 8.8 Write property test for Build.cs completeness
    - **Property 31: Build.cs Completeness**
    - **Validates: Requirements 6.7**

- [ ] 9. Phase 7: Naming & Post-Processing
  - [ ] 9.1 Harden naming edge cases
    - Handle names starting with numbers (return error)
    - Handle special characters (return error)
    - Handle consecutive capitals (HTTPServer → http_server)
    - Preserve numbers in correct position
    - _Requirements: 7.2, 7.3, 7.4, 7.5, 7.8_
  
  - [ ] 9.2 Implement ReplicationFix for post-processor
    - Detect replicated properties in generated code
    - Add GetLifetimeReplicatedProps implementation
    - Include proper DOREPLIFETIME macros
    - _Requirements: 8.1_
  
  - [ ] 9.3 Implement ShaderInitFix for post-processor
    - Detect shader usage in actors
    - Add shader initialization in BeginPlay
    - Include proper shader parameter setup
    - _Requirements: 8.2_
  
  - [ ] 9.4 Implement ForwardDeclFix for post-processor
    - Detect missing forward declarations
    - Add them in correct order (classes before structs)
    - Handle circular dependencies
    - _Requirements: 8.3_
  
  - [ ] 9.5 Implement formatting fixes for post-processor
    - Normalize blank lines to single
    - Normalize indentation to tabs
    - Normalize line endings to LF
    - Remove trailing whitespace
    - _Requirements: 8.4, 8.5, 8.8, 8.9_
  
  - [ ] 9.6 Implement IncludeOrderFix for post-processor
    - Reorder includes to UE5 conventions
    - CoreMinimal first, then engine, then project
    - Add include guards if missing
    - _Requirements: 8.6, 8.7_
  
  - [ ]* 9.7 Write unit tests for naming edge cases
    - Test number preservation
    - Test underscore conversion
    - Test consecutive capitals
    - Test invalid names (numbers, special chars)
    - _Requirements: 7.2, 7.3, 7.4, 7.5, 7.8_
  
  - [ ]* 9.8 Write property test for number preservation
    - **Property 8: Number Preservation**
    - **Validates: Requirements 7.2**
  
  - [ ]* 9.9 Write property test for underscore conversion
    - **Property 9: Underscore Conversion**
    - **Validates: Requirements 7.3**
  
  - [ ]* 9.10 Write property test for post-processing fixes
    - **Property 33: Replication Code Injection**
    - **Validates: Requirements 8.1**
  
  - [ ]* 9.11 Write property test for formatting normalization
    - **Property 36: Formatting Normalization**
    - **Validates: Requirements 8.4, 8.5, 8.8, 8.9**

- [ ] 10. Checkpoint - Verify naming and post-processing
  - Build plugins with edge case names
  - Verify post-processor fixes are applied
  - Check generated code formatting
  - Ask user if questions arise

- [ ] 11. Phase 8: Comprehensive Testing
  - [ ] 11.1 Write remaining unit tests for error handling
    - Test JSON parsing errors
    - Test file I/O errors
    - Test error message formatting
    - Test error collection
    - _Requirements: 1.3, 1.4, 1.5, 1.6, 1.8, 1.10_
  
  - [ ] 11.2 Write remaining unit tests for Oracle validation
    - Test datatable validation
    - Test component validation
    - Test circular dependency detection
    - _Requirements: 3.3, 3.4, 3.12_
  
  - [ ] 11.3 Write remaining unit tests for editor codegen
    - Test menu entry generation
    - Test toolbar button generation
    - Test editor feature integration
    - _Requirements: 5.8, 5.9, 5.10_
  
  - [ ] 11.4 Write remaining property tests
    - **Property 7: Engine Type Resolution** (Requirements 2.9)
    - **Property 10: Consecutive Capitals Handling** (Requirements 7.8)
    - **Property 11: Keyword Collision Detection** (Requirements 7.6, 7.7)
    - **Property 12: RPC Prefix Preservation** (Requirements 7.10)
    - **Property 13: Delegate Naming Convention** (Requirements 7.9)
    - **Property 16: DataTable Field Validation** (Requirements 3.3)
    - **Property 17: Component Feature Validation** (Requirements 3.4)
    - **Property 19: Circular Dependency Detection** (Requirements 3.12)
    - **Property 21: Permutation Naming Validation** (Requirements 3.6, 4.2)
    - **Property 23: POD Struct Redefinition Detection** (Requirements 4.4)
    - **Property 24: HLSL Keyword Collision Detection** (Requirements 4.5)
    - **Property 25: Shader Binding Conflict Detection** (Requirements 4.6)
    - **Property 26: Generated USF Syntax Validation** (Requirements 4.10)
    - **Property 29: Editor Feature Integration** (Requirements 5.10)
    - **Property 32: Circular Module Dependency Detection** (Requirements 6.10)
    - **Property 34: Shader Initialization Injection** (Requirements 8.2)
    - **Property 35: Forward Declaration Injection** (Requirements 8.3)
    - **Property 37: Include Guard Injection** (Requirements 8.6)
    - **Property 38: Include Reordering** (Requirements 8.7)
    - **Property 42: Shader Include Resolution** (Requirements 11.8)
    - **Property 47: Metadata Schema Validation** (Requirements 13.1, 13.9)
    - **Property 48: Metadata Query Fallback** (Requirements 13.18, 13.20)
    - **Property 49: Metadata-First Type Resolution** (Requirements 13.13)
    - **Property 50: Metadata-First Module Resolution** (Requirements 13.14)
    - **Property 51: Metadata-First Validation** (Requirements 13.15)
    - **Property 52: Multi-Drive UE5 Support** (Requirements 13.11)
    - **Property 53: Multi-Version UE5 Support** (Requirements 13.12)
  
  - [ ] 11.5 Write integration tests for end-to-end scenarios
    - Test building ultimate.kn (comprehensive test plugin)
    - Test building with intentional errors (verify error messages)
    - Test building with edge cases (verify correct output)
    - Test building with all features (verify no TODO stubs)
    - _Requirements: All_
  
  - [ ] 11.6 Set up snapshot testing
    - Create snapshots for ultimate.kn
    - Create snapshots for simple_actor.kn
    - Create snapshots for shader_plugin.kn
    - Add snapshot update command to CI
    - _Requirements: 12.2, 12.3, 12.11_
  
  - [ ] 11.7 Verify test coverage
    - Run cargo tarpaulin to measure coverage
    - Ensure 90%+ code coverage
    - Identify untested code paths
    - Add tests for gaps
    - _Requirements: 9.1_

- [ ] 12. Phase 9: Data-Driven Validation Rules
  - [ ] 12.1 Create validation_rules.json schema
    - Define JSON schema for validation rules
    - Include rule categories, severities, conditions
    - Add example rules for common cases
    - _Requirements: 10.1, 10.5_
  
  - [ ] 12.2 Implement rule loading system
    - Add load_rules() method to Oracle
    - Parse validation_rules.json
    - Validate schema before using
    - Fall back to built-in rules if missing
    - _Requirements: 10.1, 10.5, 10.6_
  
  - [ ] 12.3 Implement data-driven rule enforcement
    - Add enforce_custom_rules() method to Oracle
    - Check type collisions from JSON rules
    - Check naming conventions from JSON rules
    - Check semantic constraints from JSON rules
    - _Requirements: 10.2, 10.3, 10.4_
  
  - [ ] 12.4 Implement rule disabling and custom messages
    - Support disabled: true in JSON rules
    - Use custom error messages from JSON
    - Support custom suggestions
    - _Requirements: 10.7, 10.8_
  
  - [ ] 12.5 Implement rule conflict detection
    - Check for conflicting rules on load
    - Return error with conflicting rule IDs
    - Suggest resolution
    - _Requirements: 10.10_
  
  - [ ]* 12.6 Write unit tests for rule loading
    - Test valid JSON parsing
    - Test malformed JSON handling
    - Test missing file fallback
    - Test rule disabling
    - Test custom messages
    - _Requirements: 10.1, 10.5, 10.6, 10.7, 10.8_
  
  - [ ]* 12.7 Write property test for custom rule enforcement
    - **Property 39: Custom Rule Enforcement**
    - **Validates: Requirements 10.2, 10.3, 10.4**

- [ ] 13. Phase 10: Backward Compatibility Verification
  - [ ] 13.1 Run all existing tests
    - Verify all 32 existing tests pass
    - Check no regressions in functionality
    - _Requirements: 12.10_
  
  - [ ] 13.2 Build all example plugins
    - Build ultimate.kn
    - Build all plugins in testing/Phase3/
    - Build production plugins (COSMOS, Flow, AlphaGen)
    - Verify all compile successfully
    - _Requirements: 12.1, 12.5, 12.6, 12.7_
  
  - [ ] 13.3 Compare output with snapshots
    - Run snapshot tests
    - Verify output is identical or intentionally changed
    - Update snapshots if changes are intentional
    - _Requirements: 12.2, 12.3, 12.8, 12.11_
  
  - [ ] 13.4 Verify module detection completeness
    - Check .Build.cs files for all plugins
    - Verify all manually specified modules are included
    - Verify auto-detected modules are correct
    - _Requirements: 12.9_
  
  - [ ]* 13.5 Write property tests for backward compatibility
    - **Property 43: Validation Backward Compatibility** (Requirements 12.1, 12.4)
    - **Property 44: Output Equivalence** (Requirements 12.2, 12.3, 12.8)
    - **Property 45: Feature Backward Compatibility** (Requirements 12.5, 12.6, 12.7)
    - **Property 46: Module Detection Completeness** (Requirements 12.9)

- [ ] 14. Final Checkpoint - Complete verification
  - Run full test suite (160+ tests including metadata tests)
  - Build all example and production plugins
  - Verify no crashes, no double-prefixing, no TODO stubs
  - Verify metadata system is fully validated and operational
  - Measure test coverage (target: 90%+)
  - Ask user if questions arise

## Notes

- Tasks marked with `*` are optional testing tasks that can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation and allow for course correction
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- Integration tests validate end-to-end functionality
- Snapshot tests ensure non-breaking changes
- All phases build on previous phases to ensure stability
- Backward compatibility is verified throughout (Requirement 12)
