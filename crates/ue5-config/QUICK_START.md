# UE5-CONFIG Quick Start Guide

> **For Subagents:** Read this first, then your phase in IMPLEMENTATION_BLUEPRINT.md

---

## What You're Building

A code generator that transforms KAIN `@config` structs into UE5 configuration systems:

**Input (4 lines KAIN):**
```kain
@config(category: "Game")
struct VoxelSettings:
    @setting(cvar: "voxel.ChunkSize", blueprint: true)
    chunk_size: Float = 100.0
```

**Output (41+ lines C++/.ini):**
- UDeveloperSettings .h/.cpp (30 lines)
- Console variable registration (8 lines)
- Blueprint accessor (3 lines)
- .ini file section (2 lines)

---

## Critical Rules

1. **ONLY work in `Kain/crates/ue5-config/`** — DO NOT touch other crates
2. **DO NOT modify C importer** — Main developer is working on it
3. **Read reference code ONLY** — Don't modify Research/ or other crates
4. **Follow IMPLEMENTATION_BLUEPRINT.md** — Your phase has detailed instructions
5. **Write tests** — Every phase has test requirements
6. **Update AGENT_COORDINATION.md** — Mark your status when starting/completing

---

## File Structure You're Creating

```
ue5-config/
├── Cargo.toml                          # Phase 1
├── src/
│   ├── lib.rs                          # Phase 1 (public API)
│   ├── config_ir.rs                    # Phase 1 (IR types)
│   ├── parser.rs                       # Phase 1 (attribute parsing)
│   ├── developer_settings_codegen.rs   # Phase 2 (UDeveloperSettings)
│   ├── ini_file_generator.rs           # Phase 3 (.ini files)
│   ├── cvar_codegen.rs                 # Phase 3 (console variables)
│   ├── blueprint_accessor_codegen.rs   # Phase 4 (Blueprint nodes)
│   └── templates/                      # Phases 2-3 (Minijinja)
│       ├── developer_settings.h.jinja
│       ├── developer_settings.cpp.jinja
│       └── ini_section.jinja
└── tests/                              # All phases
    ├── config_ir_tests.rs              # Phase 1
    ├── parser_tests.rs                 # Phase 1
    ├── developer_settings_tests.rs     # Phase 2
    ├── ini_file_tests.rs               # Phase 3
    ├── cvar_tests.rs                   # Phase 3
    ├── blueprint_accessor_tests.rs     # Phase 4
    └── integration_tests.rs            # Phase 5
```

---

## Reference Code Locations (READ ONLY)

### UE5 Patterns
- `Research/ReferencePatterns/28_VoxelSystems/VoxelPluginPro/Source/Voxel/Public/VoxelSettings.h`
- `Research/ReferencePatterns/28_VoxelSystems/VoxelPluginPro/Source/Voxel/Private/VoxelSettings.cpp`
- `Research/ReferencePatterns/07_EditorExtensions/WidgetLauncher/` (settings + CVars)

### Existing Crate Patterns
- `Kain/crates/ue5/` — Runtime codegen patterns
- `Kain/crates/ue5-editor/` — Editor codegen patterns
- `Kain/crates/kain-core/src/ast.rs` — AST types

---

## Type Mapping Cheat Sheet

| KAIN | UE5 C++ | CVar Type | .ini Format |
|------|---------|-----------|-------------|
| Float | float | TAutoConsoleVariable<float> | "100.0" |
| Int | int32 | TAutoConsoleVariable<int32> | "4" |
| Bool | bool | TAutoConsoleVariable<bool> | "True"/"False" |
| String | FString | TAutoConsoleVariable<FString> | "MyString" |

---

## Common Pitfalls

1. **Bool .ini format:** Use "True"/"False" (capital T/F), NOT "true"/"false"
2. **UPROPERTY order:** Config BEFORE EditAnywhere
3. **CVar naming:** Use PascalCase (ChunkSize, not chunk_size)
4. **WITH_EDITOR guards:** PostEditChangeProperty needs #if WITH_EDITOR
5. **Singleton pattern:** Use GetDefault<T>(), not new instances

---

## Testing Requirements

| Phase | Tests Required | What to Test |
|-------|----------------|--------------|
| 1 | 10+ | IR types, attribute parsing |
| 2 | 15+ | UDeveloperSettings .h/.cpp generation |
| 3 | 10+ | CVar generation, .ini format |
| 4 | 8+ | Blueprint accessor generation |
| 5 | 10+ | End-to-end integration |

**Total:** 50+ tests

---

## Execution Order

```
Phase 1 (Agent 1) → Phases 2, 3, 4 (Agents 2, 3, 4 in parallel) → Phase 5 (Agent 5)
```

1. **Agent 1** creates IR types and parser (blocks everyone)
2. **Agents 2, 3, 4** work in parallel on codegen (block Agent 5)
3. **Agent 5** integrates and tests everything

---

## Your Phase Checklist

Before marking your phase complete:
- [ ] Read IMPLEMENTATION_BLUEPRINT.md for your phase
- [ ] Implement all required files
- [ ] Write all required tests
- [ ] All tests pass (cargo test)
- [ ] No compilation errors (cargo build)
- [ ] No clippy warnings (cargo clippy)
- [ ] Update AGENT_COORDINATION.md status

---

## Questions?

1. Check IMPLEMENTATION_BLUEPRINT.md (detailed instructions)
2. Check reference code (READ ONLY)
3. Document in QUESTIONS.md
4. Make reasonable decision based on existing patterns
5. Add TODO comment with question

---

## Success Criteria

Your phase is complete when:
- All your tests pass
- Cargo build succeeds
- Cargo clippy passes
- AGENT_COORDINATION.md updated
- No modifications to other crates

---

**Ready? Read IMPLEMENTATION_BLUEPRINT.md for your phase and start coding!**
