# Requirements Document: KAIN Workspace Refactor

## Introduction

The KAIN compiler currently suffers from a monolithic "God Module" architecture where all components (frontend, backends, CLI, LSP) reside in a single `kain/src/` directory. This results in slow compile times (80% slower than necessary), tight coupling between unrelated components, and difficulty in parallel development. This refactor will transform the monolithic structure into a Cargo workspace with 5 separate crates, enabling modular compilation, parallel development by multiple AI agents, and an 80% reduction in compile times.

## Glossary

- **KAIN_Compiler**: The complete KAIN-PRO compiler system
- **Workspace**: A Cargo workspace containing multiple related crates
- **Frontend**: The lexer, parser, AST, and type system (language-agnostic)
- **Backend**: Code generators for specific targets (UE5, GPU)
- **TypedProgram**: The exchange currency between frontend and backends
- **Oracle**: The UE5 metadata system that provides type information
- **Agent**: An AI assistant responsible for a specific portion of the refactor
- **Surgical_Migration**: A staged, incremental refactor strategy with verification at each step
- **Shadow_Structure**: Creating new crate directories while keeping old code for reference

## Requirements

### Requirement 1: Workspace Structure Creation

**User Story:** As a compiler developer, I want a Cargo workspace structure with 5 separate crates, so that I can compile and test components independently.

#### Acceptance Criteria

1. WHEN the workspace is created, THE System SHALL have a root `kain/Cargo.toml` configured as a workspace manifest
2. WHEN the workspace is created, THE System SHALL have 5 crate directories under `kain/crates/`: `kain-core`, `kain-codegen-ue5`, `kain-codegen-gpu`, `kain-cli`, and `kain-stdlib`
3. WHEN each crate is initialized, THE System SHALL have a valid `Cargo.toml` with appropriate dependencies
4. WHEN the workspace is built, THE System SHALL compile all crates without errors
5. WHEN a single crate is modified, THE System SHALL only recompile that crate and its dependents (not the entire workspace)

### Requirement 2: Frontend Migration (kain-core)

**User Story:** As a compiler developer, I want the frontend (lexer, parser, AST, type system) isolated in `kain-core`, so that backend changes don't trigger frontend recompilation.

#### Acceptance Criteria

1. WHEN `kain-core` is created, THE System SHALL contain `ast.rs`, `lexer.rs`, `parser.rs`, `types.rs`, `error.rs`, `span.rs`, and `diagnostics.rs`
2. WHEN `kain-core` is compiled, THE System SHALL have zero dependencies on UE5-specific or GPU-specific code
3. WHEN the parser processes attributes, THE System SHALL treat `@component`, `@replicated`, and other attributes as generic metadata (not backend-specific)
4. WHEN `kain-core` exports its API, THE System SHALL expose a `pub struct TypedProgram` as the exchange currency for backends
5. WHEN `kain-core` is tested, THE System SHALL pass all existing frontend unit tests
6. WHEN visibility is updated, THE System SHALL change necessary `pub(crate)` items to `pub` for cross-crate access

### Requirement 3: UE5 Backend Migration (kain-codegen-ue5)

**User Story:** As a UE5 plugin developer, I want the UE5 code generator isolated in `kain-codegen-ue5`, so that GPU backend changes don't affect UE5 compilation.

#### Acceptance Criteria

1. WHEN `kain-codegen-ue5` is created, THE System SHALL contain all files from `src/ue5/` and `src/codegen/ue5.rs`
2. WHEN the UE5 backend is compiled, THE System SHALL depend on `kain-core` but not on `kain-codegen-gpu`
3. WHEN the Oracle is initialized, THE System SHALL load UE5 metadata from JSON files without requiring full crate recompilation
4. WHEN shader virtual paths are generated, THE System SHALL maintain consistency regardless of the crate's physical location
5. WHEN the UE5 backend processes a `TypedProgram`, THE System SHALL generate valid C++ header and source files
6. WHEN the UE5 backend is tested, THE System SHALL pass all existing UE5 codegen tests

### Requirement 4: GPU Backend Migration (kain-codegen-gpu)

**User Story:** As a shader developer, I want the GPU code generators isolated in `kain-codegen-gpu`, so that UE5 backend changes don't affect shader compilation.

#### Acceptance Criteria

1. WHEN `kain-codegen-gpu` is created, THE System SHALL contain `spirv.rs`, `hlsl.rs`, `usf.rs`, and `shader_analysis.rs`
2. WHEN the GPU backend is compiled, THE System SHALL depend on `kain-core` but not on `kain-codegen-ue5`
3. WHEN shader code is generated, THE System SHALL support SPIR-V, HLSL, and USF targets
4. WHEN permutation analysis is performed, THE System SHALL correctly identify `CFG_*` and `ENABLE_*` uniforms
5. WHEN the GPU backend processes a `TypedProgram`, THE System SHALL generate valid shader code
6. WHEN the GPU backend is tested, THE System SHALL pass all existing shader codegen tests

### Requirement 5: CLI and LSP Migration (kain-cli)

**User Story:** As a KAIN user, I want the CLI and LSP server in `kain-cli`, so that I can use the compiler and editor integration without compiling backends I don't need.

#### Acceptance Criteria

1. WHEN `kain-cli` is created, THE System SHALL contain `main.rs`, `lsp.rs`, `packager.rs`, and files from `bootstrap/` and `editor/`
2. WHEN `kain-cli` is compiled with default features, THE System SHALL include both UE5 and GPU backends
3. WHEN `kain-cli` is compiled with `--no-default-features`, THE System SHALL compile without any backends (LSP-only mode)
4. WHEN `kain-cli` is compiled with `--features ue5`, THE System SHALL include only the UE5 backend
5. WHEN `kain-cli` is compiled with `--features gpu`, THE System SHALL include only the GPU backend
6. WHEN the CLI searches for stdlib, THE System SHALL correctly locate `../../stdlib/` relative to the binary
7. WHEN build scripts are executed, THE System SHALL use `cargo run -p kain-cli` instead of `cargo run`

### Requirement 6: Standard Library Migration (kain-stdlib)

**User Story:** As a KAIN developer, I want the standard library in `kain-stdlib`, so that stdlib code is versioned and distributed with the compiler.

#### Acceptance Criteria

1. WHEN `kain-stdlib` is created, THE System SHALL contain all `.kn` files from `stdlib/`
2. WHEN the CLI loads stdlib, THE System SHALL discover stdlib files relative to the `kain-stdlib` crate location
3. WHEN stdlib is updated, THE System SHALL not require recompiling the entire compiler
4. WHEN stdlib is distributed, THE System SHALL include stdlib files in the packaged binary or adjacent directory

### Requirement 7: Path and Include Resolution

**User Story:** As a compiler developer, I want all file paths and includes to work correctly after the refactor, so that no runtime errors occur due to missing files.

#### Acceptance Criteria

1. WHEN `include_str!` or `include_bytes!` macros are used, THE System SHALL resolve paths correctly from the new crate locations
2. WHEN the Oracle loads metadata, THE System SHALL find JSON files in `unreal/metadata/` relative to the workspace root
3. WHEN the CLI loads stdlib, THE System SHALL find `.kn` files in `stdlib/` relative to the workspace root
4. WHEN shader virtual paths are constructed, THE System SHALL use consistent paths regardless of crate location
5. WHEN build scripts reference files, THE System SHALL use workspace-relative paths

### Requirement 8: Staged Migration with Verification

**User Story:** As a project manager, I want the refactor to proceed in verified stages, so that we can roll back if issues are discovered.

#### Acceptance Criteria

1. WHEN Stage 1 (Shadow Structure) completes, THE System SHALL have all crate directories and `Cargo.toml` files created while `src/` still exists
2. WHEN Stage 2 (Dependency Linkage) completes, THE System SHALL have the workspace configured to reference new crates
3. WHEN Stage 3 (Incremental Move) completes, THE System SHALL have all source files moved to new crates and verified with `cargo check`
4. WHEN Stage 4 (Path Patching) completes, THE System SHALL have all visibility modifiers and path references updated
5. WHEN Stage 5 (Final Verification) completes, THE System SHALL pass 100% of the integration test suite
6. WHEN Stage 6 (The Clean) completes, THE System SHALL have the root `src/` directory deleted

### Requirement 9: Agent Coordination Protocol

**User Story:** As an orchestrator, I want clear coordination between the three AI agents, so that they work in parallel without conflicts.

#### Acceptance Criteria

1. WHEN the refactor begins, THE System SHALL assign Agent Alpha to `kain-core` migration
2. WHEN the refactor begins, THE System SHALL assign Agent Beta to `kain-codegen-ue5` and `kain-codegen-gpu` migration
3. WHEN the refactor begins, THE System SHALL assign Agent Charlie to `kain-cli` and workspace integration
4. WHEN an agent completes a task, THE System SHALL signal completion before other agents proceed with dependent tasks
5. WHEN Agent Alpha completes `kain-core`, THE System SHALL allow Agent Beta to begin backend migrations
6. WHEN Agents Alpha and Beta complete their crates, THE System SHALL allow Agent Charlie to delete the root `src/` directory

### Requirement 10: Compilation Performance Verification

**User Story:** As a compiler developer, I want to verify the 80% compile time reduction, so that I can confirm the refactor achieved its performance goals.

#### Acceptance Criteria

1. WHEN a full clean build is performed, THE System SHALL complete in 20% or less of the original monolithic build time
2. WHEN only `kain-core` is modified, THE System SHALL not recompile any backend crates
3. WHEN only `kain-codegen-ue5` is modified, THE System SHALL not recompile `kain-codegen-gpu` or `kain-core`
4. WHEN only `kain-codegen-gpu` is modified, THE System SHALL not recompile `kain-codegen-ue5` or `kain-core`
5. WHEN `kain-cli` is compiled with `--no-default-features`, THE System SHALL compile in less than 10% of full build time

### Requirement 11: Backward Compatibility

**User Story:** As a KAIN user, I want existing plugins to compile without changes, so that the refactor doesn't break my projects.

#### Acceptance Criteria

1. WHEN an existing `.kn` plugin is compiled, THE System SHALL produce identical output to the pre-refactor compiler
2. WHEN the CLI is invoked with existing commands, THE System SHALL behave identically to the pre-refactor version
3. WHEN the LSP is used in an editor, THE System SHALL provide identical functionality to the pre-refactor version
4. WHEN build scripts are executed, THE System SHALL produce identical results to the pre-refactor version
5. WHEN the packager creates a plugin, THE System SHALL generate identical directory structures to the pre-refactor version

### Requirement 12: Risk Mitigation Verification

**User Story:** As a quality assurance engineer, I want explicit verification of all identified risk points, so that no critical issues are missed.

#### Acceptance Criteria

1. WHEN macro paths are verified, THE System SHALL confirm all `include_str!` and `include_bytes!` resolve correctly
2. WHEN stdlib discovery is verified, THE System SHALL confirm the CLI finds stdlib files in all deployment scenarios
3. WHEN cross-crate visibility is verified, THE System SHALL confirm all necessary types and functions are accessible
4. WHEN shader paths are verified, THE System SHALL confirm virtual paths remain consistent
5. WHEN build scripts are verified, THE System SHALL confirm `cb.ps1`, `build.bat`, and other scripts work correctly
