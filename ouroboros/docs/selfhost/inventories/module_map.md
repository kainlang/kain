# Initial Self-Host Module / Crate Map

Initial slice: `kain-core, kain-import`

## kain-core

- **Root:** `crates/kain-core/src/lib.rs`
- **Initial slice candidate:** yes

### Root modules

- `lexer`
- `ast`
- `parser`
- `types`
- `effects`
- `stdlib`
- `error`
- `span`
- `comptime`
- `diagnostics`
- `diagnostic_registry`
- `low_level_abi`
- `low_level_memory`
- `low_level_memory_metadata`
- `monomorphize`
- `runtime`
- `ui`
- `shader_analysis`
- `asm_ir`
- `language_features`
- `stdlib_tests`
## kain-import

- **Root:** `crates/kain-import/src/lib.rs`
- **Initial slice candidate:** yes

### Root modules

- `c`
- `common`
- `rust`
- `typescript`

### Nested module declarations

- `c/mod.rs` -> `parser`, `transformer`, `types`
- `common/mod.rs` -> `preprocessor`, `c_registry`, `identifier_registry`, `language_schema`, `type_mapper`
- `rust/mod.rs` -> `parser`, `selfhost`, `transformer`, `types`
- `typescript/mod.rs` -> `parser`, `transformer`, `types`
## kain-sys-codegen

- **Root:** `crates/kain-sys-codegen/src/lib.rs`
- **Initial slice candidate:** no

### Root modules

- `codegen_llvm`
- `codegen_rust`
- `codegen_cpp`

### Nested module declarations

- `codegen_rust/mod.rs` -> `artifact_bundle`, `gpu_artifacts`, `gpu_host`
## cli

- **Root:** `crates/cli/src/lib.rs`
- **Initial slice candidate:** no

### Root modules

- `error`
- `lsp`
- `packager`
- `omni`
- `import_asm`
- `import_c`
- `import_rust`
- `import_typescript`
- `rust_build`
- `gpu_artifacts`

### Nested module declarations

- `packager/mod.rs` -> `config`, `registry`, `build`, `ue5_pipeline`, `plugin_layout`, `codegen`, `material_gen`, `uplugin_gen`, `build_cs_gen`, `post_process`, `dependencies`, `inject`, `registry_writer`, `cpp_validator`
