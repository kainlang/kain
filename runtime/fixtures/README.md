# Native Runtime Smoke Fixtures

**Spec:** `.kiro/specs/kain-native-runtime-completion`  
**Task:** 0.3 Create native runtime smoke fixtures  
**Created:** 2026-03-17

---

## Purpose

This directory contains minimal smoke programs and artifacts for validating native runtime startup paths. These fixtures are designed to be reused across all phases of the native runtime completion work instead of creating new test programs for each phase.

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
kain build main.kn --target rust
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
kain build main.kn --target rust
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
# Validate all fixtures compile successfully
./runtime/fixtures/validate_all.sh
```

### Individual Fixture Validation

```bash
# Contract startup
cd runtime/fixtures/contract_startup && kain build main.kn --target rust

# Realtime startup
cd runtime/fixtures/realtime_startup && kain build main.kn --target rust

# UI startup
cd runtime/fixtures/ui_startup && kain build main.kn --target rust

# Viewport startup (Windows only)
cd runtime/fixtures/viewport_startup && kain build main.kn --target rust
```

---

## Integration with Validation Suite

These fixtures are referenced by `runtime/NATIVE_RUNTIME_VALIDATION.md` and integrated into the full validation suite at `runtime/validate_native_runtime.sh`.

As the native runtime completion work progresses, these fixtures will be used in:
- Phase 1: ABI and service table validation
- Phase 2: Structured diagnostics validation
- Phase 3: Reflection payload validation
- Phase 4: Low-level helper parity validation
- Phase 5+: Actor, async, UI, graphics, hot reload validation

---

## Related Documentation

- **Spec Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md`
- **Spec Design:** `.kiro/specs/kain-native-runtime-completion/design.md`
- **Spec Tasks:** `.kiro/specs/kain-native-runtime-completion/tasks.md`
- **Validation Commands:** `runtime/NATIVE_RUNTIME_VALIDATION.md`
- **Progress Tracker:** `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md`

---

## Notes

- These fixtures are intentionally minimal - resist the urge to make them "realistic"
- Platform-specific fixtures (viewport) should fail gracefully on unsupported platforms
- All fixtures should compile and validate successfully on their target platform
- Fixtures are not meant to be run as full applications (except viewport)
- Focus on startup validation, not runtime behavior
