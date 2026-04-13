# UE5 Codegen System — Developer's Directory (Part 1)

> **Purpose:** Comprehensive guide to the UE5 codegen architecture  
> **Last Updated:** 2026-02-19  
> **Status:** Production-ready

---

## 1. Architecture Overview

### The Pipeline

```
KAIN Source (.kn)
    ↓
Parser (kain-core) → AST
    ↓
Type Checker → TypedProgram
    ↓
Oracle Validator → Semantic Validation
    ↓
Packager (cli/packager.rs) — Orchestrates multi-file modular output
    ↓
    ├─→ ue5 crate (Runtime Codegen)
    │   ├─ Actors (.h/.cpp)
    │   ├─ Components (.h/.cpp)
    │   ├─ Structs (.h)
    │   ├─ Enums (.h)
    │   └─ Delegates (.h)
    │
    ├─→ ue5-editor crate (Editor Codegen)
    │   ├─ Slate Widgets (.h/.cpp)
    │   ├─ Details Panels (.h/.cpp)
    │   ├─ Viewports (.h/.cpp)
    │   └─ Asset Editors (.h/.cpp)
    │
    └─→ ue5-shaders crate (Shader Codegen)
        ├─ HLSL .usf files
        └─ C++ shader bindings (.h/.cpp)
```

### Design Principles

1. **Data-Driven**: Type mappings, includes, validation rules from JSON metadata
2. **Single Source of Truth**: TypeMapper centralizes all type conversions
3. **Prefix Detection**: Prevents double-prefixing (EEHealthStatus bug)
4. **Context Sharing**: Ue5Context enables cross-module intelligence
5. **LLM-Friendly**: Clear error messages with file:line:col references

---

## 2. Core Components

### File Structure

```
crates/ue5/
├── src/
│   ├── codegen_ue5.rs          # Main codegen (~3200 lines)
│   └── ue5/
│       ├── context.rs          # Ue5Context — shared state
│       ├── naming.rs           # UE5 naming (A/F/E/U prefixes)
│       ├── types.rs            # TypeMapper — type mapping
│       ├── engine_knowledge.rs # EngineKnowledge database
│       ├── oracle.rs           # Semantic validator
│       └── resolver.rs         # StdLib resolver (legacy)
```
