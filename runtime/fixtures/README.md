# Native Runtime Smoke Fixtures

**Spec:** `.kiro/specs/kain-native-runtime-completion`  
**Task:** 0.3 Create native runtime smoke fixtures  
**Created:** 2026-03-17

---

## Purpose

This directory contains minimal smoke programs and artifacts for validating the LLVM/native runtime lane.

There are two fixture classes:

- Startup fixtures validate bundle loading, startup diagnostics, and native host initialization.
- Executable LLVM fixtures compile Kain source to LLVM IR, link against the native runtime, execute the produced binary, and assert deterministic exit behavior.

**Critical Rule:** Later phases MUST reuse these fixtures instead of inventing new ones. If a fixture is insufficient, extend it rather than creating a duplicate.

---

## Fixture Organization

### 1. Contract Startup (`contract_startup/`)

**Purpose:** Validates minimal runtime contract loading and validation

**Artifacts:**
- `main.kn` - Minimal Kain program with no runtime features
- `kain_runtime_contract.json` - Minimal contract bundle
- `README.md` - Usage instructions

**What it tests:**
- Runtime contract JSON parsing
- Schema version validation
- Required capabilities validation
- Service bindings resolution
- Startup diagnostics on success

**Usage:**
```bash
cd runtime/fixtures/contract_startup
# Compile and validate contract loading
kain build main.kn --target llvm --output generated/contract_startup.ll
```

---

### 2. Realtime Bundle Startup (`realtime_startup/`)

**Purpose:** Validates realtime bundle ingestion and scene metadata loading

**Artifacts:**
- `main.kn` - Minimal program with scene reference
- `kain_runtime_contract.json` - Contract with realtime requirements
- `kain_realtime_app_bundle.json` - Minimal realtime bundle
- `README.md` - Usage instructions

**What it tests:**
- Realtime bundle JSON parsing
- Scene metadata validation
- Asset reference resolution (empty)
- Shader bundle reference resolution (empty)
- Material reference resolution (empty)
- Startup diagnostics on success

**Usage:**
```bash
cd runtime/fixtures/realtime_startup
# Compile and validate realtime bundle loading
kain build main.kn --target llvm --output generated/realtime_startup.ll
```

---

### 3. UI Bundle Startup (`ui_startup/`)

**Purpose:** Validates compiled UI bundle loading and component metadata

**Artifacts:**
- `main.kn` - Minimal UI component program
- `kain_runtime_contract.json` - Contract with UI requirements
- `README.md` - Usage instructions

**What it tests:**
- UI component compilation
- Compiled UI bundle structure
- Component metadata validation
- UI runtime initialization
- Startup diagnostics on success

**Usage:**
```bash
cd runtime/fixtures/ui_startup
# Compile and validate UI bundle loading
kain build main.kn --target rust
```

---

### 4. Native Viewport Startup (`viewport_startup/`)

**Purpose:** Validates native viewport host startup with Win32 platform services

**Artifacts:**
- `main.kn` - Minimal viewport program
- `kain_runtime_contract.json` - Contract with viewport requirements
- `kain_realtime_app_bundle.json` - Realtime bundle with viewport scene
- `README.md` - Usage instructions

**What it tests:**
- Win32 app host initialization
- Win32 input host initialization
- OpenGL context creation
- Viewport scene loading
- Platform service availability
- Startup diagnostics on success

**Usage:**
```bash
cd runtime/fixtures/viewport_startup
# Compile and validate viewport startup
kain build main.kn --target rust
# Run the native viewport (Windows only)
./run.ps1
```

---

### 5. LLVM Heap Memory (`llvm_heap_memory/`)

**Purpose:** Validates canonical heap helper lowering and helper-owned realloc correctness

**What it tests:**
- Canonical `__kain_alloc(size, stride, zeroed)` lowering
- Canonical `__kain_realloc(ptr, size, stride, zeroed_new)` lowering
- End-to-end execution of `mem_store`, `realloc_mem`, and `mem_load`
- Helper-owned realloc growth zero-filling of newly exposed bytes

**Usage:**
```bash
cd runtime/fixtures/llvm_heap_memory
../../fixtures/validate_all.sh
```

---

### 6. LLVM Actor Message (`llvm_actor_message/`)

**Purpose:** Validates actor spawn and mailbox send paths in a linked LLVM/native executable

**What it tests:**
- Actor-specific bootstrap entrypoint emission
- Mailbox allocation during actor spawn
- Message send lowering through `mq_push`
- Successful execution of the produced actor binary

---

### 7. LLVM World Pipeline (`llvm_world_pipeline/`)

**Purpose:** Validates world initialization plus patch/converge/orchestrate execution in the LLVM/native lane

**What it tests:**
- Generated world bootstrap emission and execution
- Patch and converge lowering
- Deterministic orchestrate execution in the linked binary

---

## Design Principles

### Minimal by Design

Each fixture contains the absolute minimum code and metadata required to validate its startup path. No extra features, no complex logic, no unnecessary dependencies.

### Reusable Across Phases

These fixtures are designed to be stable references that later phases can:
- Extend with new capabilities
- Use as baseline validation
- Compare against for regression testing
- Reference in conformance tests

### Self-Documenting

Each fixture includes:
- Clear README explaining what it tests
- Inline comments in source files
- Explicit artifact structure
- Usage examples

### Platform-Aware

Fixtures that require platform-specific services (like viewport startup) clearly document their platform requirements and fail gracefully on unsupported platforms.

---

## Extending Fixtures

When later phases need to extend these fixtures:

1. **Preserve the minimal baseline** - Don't add unnecessary complexity
2. **Document extensions** - Update the fixture README with new capabilities
3. **Maintain backward compatibility** - Ensure old validation still works
4. **Add new artifacts carefully** - Only add artifacts that are truly necessary
5. **Update this README** - Document what changed and why

---

## Validation Commands

### Quick Validation (All Fixtures)

```bash
# Compile all fixtures and execute the LLVM-target fixtures
./runtime/fixtures/validate_all.sh
```

### Individual Fixture Validation

```bash
# Contract startup
cd runtime/fixtures/contract_startup && ../../../target/debug/kain build main.kn --target llvm --output generated/contract_startup.ll

# Realtime startup
cd runtime/fixtures/realtime_startup && ../../../target/debug/kain build main.kn --target llvm --output generated/realtime_startup.ll

# UI startup
cd runtime/fixtures/ui_startup && ../../../target/debug/kain build main.kn --target rust --output generated/ui_startup.rs

# Viewport startup (Windows only)
cd runtime/fixtures/viewport_startup && ../../../target/debug/kain build main.kn --target rust --output generated/viewport_startup.rs
```

---

## Integration with Validation Suite

These fixtures are referenced by `runtime/changelogs/NATIVE_RUNTIME_VALIDATION.md` and integrated into the full validation suite at `runtime/validate_native_runtime.sh`.

As the native runtime completion work progresses, these fixtures will be used in:
- Phase 1: ABI and service table validation
- Phase 2: Structured diagnostics validation
- Phase 3: Reflection payload validation
- Phase 4: Low-level helper parity validation
- Phase 5+: Actor, async, UI, graphics, hot reload validation

The important distinction is:

- `runtime/fixtures/` now carries the true end-to-end LLVM executable proof lane.
- `runtime/conformance/` carries runtime-native harnesses and ABI-focused behavior checks.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs` carries backend IR-shape coverage.

---

## Related Documentation

- **Spec Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md`
- **Spec Design:** `.kiro/specs/kain-native-runtime-completion/design.md`
- **Spec Tasks:** `.kiro/specs/kain-native-runtime-completion/tasks.md`
- **Validation Commands:** `runtime/changelogs/NATIVE_RUNTIME_VALIDATION.md`
- **Progress Tracker:** `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md`

---

## Notes

- These fixtures are intentionally minimal - resist the urge to make them "realistic"
- Platform-specific fixtures (viewport) should fail gracefully on unsupported platforms
- All fixtures should compile and validate successfully on their target platform
- LLVM fixtures are meant to be executed as linked native binaries
- Startup fixtures still focus on bundle/startup validation rather than deep runtime behavior
