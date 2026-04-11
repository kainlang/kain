# KAIN Native Runtime Validation Commands

**Spec:** `.kiro/specs/kain-native-runtime-completion`  
**Task:** 0.2 Establish native runtime validation commands  
**Last Updated:** 2026-03-17

---

## Purpose

This document defines the canonical commands used to validate the native runtime and its compiler/driver dependencies. These commands serve as the baseline validation suite for all phases of the native runtime completion work.

**Critical Rule:** All runtime-facing changes MUST pass these validation commands before being considered complete.

Validation is split into three distinct proof layers:

- Backend IR-generation tests prove LLVM lowering shape and strict codegen behavior.
- Runtime-native conformance harnesses prove the C runtime surface and subsystem behavior.
- Executable LLVM fixtures prove generated Kain programs compile, link, and run successfully against the native runtime.

---

## Quick Reference

```bash
# Full validation suite (run all commands in order)
./runtime/validate_native_runtime.sh

# Backend/codegen proof
cargo test -p kain-sys-codegen --test llvm_codegen_test -- --nocapture

# LLVM/native executable proof
./runtime/fixtures/validate_all.sh

# Runtime-native harness proof
./runtime/conformance/run_all.sh

# Native runtime compilation
./runtime/compile_native_runtime.sh
```

---

## 1. Compiler Frontend Validation

### kain-core Tests

**Command:**
```bash
cargo test --package kain-core
```

**Purpose:** Validates the language frontend including:
- Lexer and parser correctness
- Type system and inference
- Comptime evaluation
- Runtime contract emission
- Low-level memory lowering
- Interpreter/runtime execution

**Expected Result:** All tests pass

**Failure Impact:** Critical - indicates core language semantics are broken

---

## 2. Driver and Bundle Materialization Validation

### kain-driver Tests

**Command:**
```bash
cargo test --package kain-driver
```

**Purpose:** Validates the compiler orchestration layer including:
- Multi-target compilation coordination
- Native app bundle materialization
- Runtime contract packaging
- Artifact output and layout
- Build configuration handling

**Expected Result:** All tests pass

**Failure Impact:** Critical - indicates bundle generation or compilation orchestration is broken

---

## 3. Backend Code Generation Validation

### kain-sys-codegen Tests

**Command:**
```bash
cargo test --package kain-sys-codegen
```

**Purpose:** Validates system backends including:
- LLVM IR generation
- C++ code generation
- Rust code generation
- Actor bootstrap emission
- Low-level helper binding
- Runtime ABI conformance

**Expected Result:** All tests pass

**Failure Impact:** Critical - indicates backend/runtime contract mismatch

---

## 4. Native Runtime Compilation Validation

### Direct Native Runtime Compilation

**Command:**
```bash
./runtime/compile_native_runtime.sh
```

**Purpose:** Validates that the native C runtime compiles successfully with:
- Current manifest (`runtime/native_runtime.toml`)
- All source files under `runtime/native/src/`
- All headers under `runtime/native/include/`
- Platform-specific link dependencies

**Expected Result:** Compilation succeeds with no errors

**Failure Impact:** Critical - indicates native runtime source is broken

**Implementation:** See `runtime/compile_native_runtime.sh` for the canonical compilation command

---

## 5. Full Workspace Validation (Optional)

### All Crates

**Command:**
```bash
cargo test --workspace
```

**Purpose:** Validates the entire Rust workspace including all crates

**Expected Result:** All tests pass

**Failure Impact:** Indicates broader ecosystem breakage beyond core runtime

**Note:** This is more comprehensive but slower. Use for final validation before major milestones.

---

## Native Runtime Compilation Details

### Manifest Location

**File:** `runtime/native_runtime.toml`

**Contents:**
- `sources`: List of 13 C source files
- `include_dirs`: Header directories
- `link.windows`: Windows-specific link libraries
- `link.linux`: Linux-specific link libraries (currently empty)
- `link.macos`: macOS-specific link libraries (currently empty)

### Source Files (13 files)

**Core Runtime:**
1. `native/src/core/kain_runtime_core.c` - Allocation, RC, arrays, maps, strings, file I/O, sockets, threads, queues
2. `native/src/core/kain_runtime_contract.c` - Runtime contract validation
3. `native/src/core/kain_runtime_realtime.c` - Realtime bundle ingestion

**Asset Loading:**
4. `native/src/asset/kain_asset_gltf.c` - glTF asset loading

**Graphics:**
5. `native/src/gfx/opengl/kain_gl_win32_host.c` - OpenGL Win32 host

**Platform (Win32):**
6. `native/src/platform/win32/kain_win32_app_host.c` - Win32 app host
7. `native/src/platform/win32/kain_win32_input_host.c` - Win32 input capture
8. `native/src/platform/win32/kain_runtime_win32_shared.c` - Win32 shared utilities
9. `native/src/platform/win32/kain_runtime_viewport_win32.c` - Win32 viewport host
10. `native/src/platform/win32/kain_runtime_sculpt_win32.c` - Win32 sculpt host

**UI Runtime:**
11. `native/src/ui/kain_ui_compiled_bundle.c` - Compiled UI bundle loading
12. `native/src/ui/kain_ui_compiled_overlay.c` - Compiled UI overlay rendering
13. `native/src/ui/kain_ui_overlay.c` - UI overlay base

### Include Directories

**Location:** `runtime/native/include/`

**Headers:**
- `kain_runtime_base.h` - Base types, RC, thread args, arrays, maps, queues
- `kain_runtime_contract.h` - Runtime contract structures
- `kain_runtime_realtime.h` - Realtime bundle structures
- `kain_runtime_asset.h` - Asset loading structures
- `kain_runtime_ui.h` - UI bundle structures
- `kain_runtime_win32.h` - Win32 platform declarations

### Platform-Specific Link Dependencies

**Windows:**
- `legacy_stdio_definitions`
- `user32`
- `gdi32`
- `opengl32`
- `ws2_32`

**Linux:** (not yet implemented)

**macOS:** (not yet implemented)

### Compilation Command Structure

The canonical compilation command follows this pattern:

```bash
# Windows (MSVC)
cl.exe /nologo /W3 /O2 \
  /I runtime/native/include \
  /I runtime/native/third_party/cgltf \
  /D WIN32 /D _WINDOWS \
  /c runtime/kain_runtime.c \
  /Fo:generated/kain_runtime.obj

# Windows (Clang)
clang -O2 \
  -I runtime/native/include \
  -I runtime/native/third_party/cgltf \
  -D WIN32 -D _WINDOWS \
  -c runtime/kain_runtime.c \
  -o generated/kain_runtime.obj

# Linux (GCC/Clang) - when implemented
gcc -O2 \
  -I runtime/native/include \
  -I runtime/native/third_party/cgltf \
  -c runtime/kain_runtime.c \
  -o generated/kain_runtime.o

# macOS (Clang) - when implemented
clang -O2 \
  -I runtime/native/include \
  -I runtime/native/third_party/cgltf \
  -c runtime/kain_runtime.c \
  -o generated/kain_runtime.o
```

**Note:** The actual script detects the platform and available compiler automatically.

---

## Validation Workflow

### Pre-Implementation Validation

Before starting any runtime-facing work:

1. Run all validation commands to establish baseline
2. Record any existing failures
3. Ensure your changes don't introduce new failures

### During Implementation

After each significant change:

1. Run affected crate tests (`cargo test --package <crate>`)
2. Run native runtime compilation if C sources changed
3. Fix failures immediately - don't accumulate technical debt

### Pre-Commit Validation

Before committing runtime-facing changes:

1. Run full validation suite (`./runtime/validate_native_runtime.sh`)
2. Ensure all tests pass
3. Update this document if validation commands change

### Phase Completion Validation

At the end of each phase:

1. Run full workspace validation (`cargo test --workspace`)
2. Run native runtime compilation
3. Run any phase-specific smoke tests
4. Update `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md`

---

## Troubleshooting

### Cargo Test Failures

**Symptom:** `cargo test --package <crate>` fails

**Common Causes:**
- Type system changes broke existing tests
- Runtime contract schema changed
- ABI mismatch between compiler and runtime expectations
- Missing test fixtures or data files

**Resolution:**
1. Read test failure output carefully
2. Check if changes intentionally broke old behavior
3. Update tests to match new contracts
4. Add new tests for new behavior

### Native Runtime Compilation Failures

**Symptom:** `./runtime/compile_native_runtime.sh` fails

**Common Causes:**
- Syntax errors in C source files
- Missing header includes
- Undefined symbols or functions
- Platform-specific code on wrong platform
- Missing third-party dependencies

**Resolution:**
1. Check compiler error messages
2. Verify all source files are listed in `native_runtime.toml`
3. Verify all headers are in `runtime/native/include/`
4. Check for platform-specific `#ifdef` guards
5. Ensure third-party dependencies are present

### Link Failures

**Symptom:** Compilation succeeds but linking fails

**Common Causes:**
- Missing platform libraries in `native_runtime.toml`
- Undefined external symbols
- Incorrect calling conventions
- Missing runtime helper implementations

**Resolution:**
1. Check linker error messages for missing symbols
2. Add missing libraries to `[link]` section in manifest
3. Implement missing runtime helpers
4. Verify function signatures match declarations

---

## Future Validation Additions

As the native runtime completion work progresses, this document will be extended with:

### Phase 1+: ABI and Service Table Validation
- ABI version compatibility tests
- Service registry validation
- Startup validation tests

### Phase 2+: Diagnostics Validation
- Structured diagnostics tests
- Error code stability tests
- Failure model conformance tests

### Phase 3+: Reflection Validation
- Reflection payload emission tests
- Schema validation tests
- Runtime reflection loader tests

### Phase 4+: Low-Level Helper Validation
- Helper ABI conformance tests
- Memory operation parity tests
- Layout and alignment tests

### Phase 5+: Actor Runtime Validation
- Actor bootstrap tests
- Mailbox and lifecycle tests
- Supervision and monitoring tests

### Phase 6+: Full Actor Semantics Validation
- Bounded mailbox tests
- Registry tests
- Scheduler fairness tests

### Phase 7+: Async Runtime Validation
- Task executor tests
- Timer service tests
- Wake/poll conformance tests

### Phase 8+: UI Runtime Validation
- Bundle validation tests
- Component lifecycle tests
- Event routing tests

### Phase 9+: Graphics Runtime Validation
- Shader artifact loading tests
- Material runtime tests
- Compute dispatch tests

### Phase 10+: Hot Reload Validation
- Compatibility validation tests
- Migration tests
- State transfer tests

### Phase 11+: Host Bridge Validation
- Service registration tests
- Plugin ABI tests
- Foreign bridge tests

### Phase 12+: Cross-Platform Validation
- Linux platform tests
- macOS platform tests
- Platform parity tests

### Phase 13+: End-to-End Validation
- Full bundle emission tests
- Backend/runtime parity tests
- Conformance matrix validation

---

## Smoke Fixtures

The native runtime smoke fixtures are located in `runtime/fixtures/` and now cover both startup validation and executable LLVM proof:

- **Contract Startup** (`runtime/fixtures/contract_startup/`) - Validates runtime contract loading
- **Realtime Startup** (`runtime/fixtures/realtime_startup/`) - Validates realtime bundle ingestion
- **UI Startup** (`runtime/fixtures/ui_startup/`) - Validates compiled UI bundle loading
- **Viewport Startup** (`runtime/fixtures/viewport_startup/`) - Validates native viewport host startup (Win32)
- **LLVM Heap Memory** (`runtime/fixtures/llvm_heap_memory/`) - Validates alloc/realloc/mem_store/mem_load execution in a linked LLVM/native binary
- **LLVM Actor Message** (`runtime/fixtures/llvm_actor_message/`) - Validates actor spawn and mailbox send execution in a linked LLVM/native binary
- **LLVM World Pipeline** (`runtime/fixtures/llvm_world_pipeline/`) - Validates world/patch/converge/orchestrate execution in a linked LLVM/native binary

**Validate all fixtures:**
```bash
# Bash
./runtime/fixtures/validate_all.sh

# PowerShell
./runtime/fixtures/validate_all.ps1
```

See `runtime/fixtures/README.md` for detailed fixture documentation.

Important distinction:

- The startup fixtures validate bundle/bootstrap behavior.
- The three LLVM fixtures are executed, not just compiled, and are the canonical end-to-end LLVM/native proof lane.

---

## Conformance Tests

The native runtime conformance tests are located in `runtime/conformance/` and provide runtime-specific harnesses and ABI parity tests:

- **ABI Parity** (`runtime/conformance/abi_parity/`) - Low-level memory helper ABI parity
- **Actor Runtime** (`runtime/conformance/actor_runtime/`) - Actor semantics and lifecycle
- **Async Runtime** (`runtime/conformance/async_runtime/`) - Async/await, futures, and timers
- **Reflection** (`runtime/conformance/reflection/`) - Reflection payload loading and lookup
- **Diagnostics** (`runtime/conformance/diagnostics/`) - Structured diagnostics and error codes
- **UI Runtime** (`runtime/conformance/ui_runtime/`) - UI bundle interpretation and components
- **Graphics Runtime** (`runtime/conformance/graphics_runtime/`) - Shader/material/compute artifacts
- **Hot Reload** (`runtime/conformance/hot_reload/`) - Versioning, compatibility, and migration
- **Platform Parity** (`runtime/conformance/platform_parity/`) - Cross-platform runtime behavior

**Run all conformance tests:**
```bash
# Run all tests on all backends
./runtime/conformance/run_all.sh

# Run specific category
./runtime/conformance/run_all.sh --category abi_parity

# Run with a backend label in the harness report
./runtime/conformance/run_all.sh --backend llvm

# Run quick validation
./runtime/conformance/run_all.sh --mode quick
```

See `runtime/conformance/README.md` for detailed conformance test documentation.

Important distinction:

- `runtime/conformance/` validates the native runtime harness family.
- `--backend llvm` in that runner does not replace the executable LLVM fixture proof.

---

## Related Documentation

- **Spec Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md`
- **Spec Design:** `.kiro/specs/kain-native-runtime-completion/design.md`
- **Spec Tasks:** `.kiro/specs/kain-native-runtime-completion/tasks.md`
- **Progress Tracker:** `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md`
- **Feature Matrix:** `runtime/KAIN_NATIVE_RUNTIME_FEATURE_MATRIX.md`
- **Runtime Manifest:** `runtime/native_runtime.toml`
- **Smoke Fixtures:** `runtime/fixtures/README.md`
- **Conformance Tests:** `runtime/conformance/README.md`

---

## Notes

- This document is the canonical reference for validation commands
- All validation commands should be deterministic and reproducible
- Validation commands should be fast enough to run frequently during development
- Slow or expensive validation should be clearly marked as optional
- Platform-specific validation should be clearly documented
- Validation failures should be treated as blocking issues, not warnings

