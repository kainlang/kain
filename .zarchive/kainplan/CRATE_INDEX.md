# KAIN Crate Documentation Index

> **Last Updated:** 2026-02-20  
> **Purpose:** Master index of all KAIN crate documentation  
> **Status:** Complete - 8 crates fully documented

---

## Overview

This document provides a comprehensive index of all KAIN compiler crates with links to their detailed documentation. Each crate has a `CRATE_REFERENCE.md` file in its directory with complete API documentation, examples, and usage guides.

---

## Core Compiler Crates

### 1. kain-core
**Path:** `crates/kain-core/CRATE_REFERENCE.md`  
**Purpose:** Frontend compiler - lexer, parser, AST, type system, effects  
**Status:** Production-ready  
**Key Features:**
- Python-style indentation (INDENT/DEDENT tokens)
- Rust-like type system with generics
- Effect tracking (Pure, IO, Async, GPU, Reactive, Unsafe)
- Compile-time evaluation (comptime blocks)
- Monomorphization (generic instantiation)
- 60+ built-in functions

**When to use:** Building language features, parsing KAIN source, type checking

---

### 2. cli
**Path:** `crates/cli/CRATE_REFERENCE.md`  
**Purpose:** Command-line interface and build orchestrator  
**Status:** Production-ready  
**Key Features:**
- Multi-file build orchestration
- KAIN.toml configuration
- Modular output generation
- Plugin packaging
- .uplugin and .Build.cs generation

**When to use:** Building plugins, running the compiler, packaging projects

---

## UE5 Codegen Crates

### 3. ue5
**Path:** `crates/ue5/CRATE_REFERENCE.md`  
**Purpose:** Runtime codegen - actors, components, structs, enums, delegates  
**Status:** Production-ready - 22 tests passing  
**Key Features:**
- Actor codegen (RPCs, replication, lifecycle)
- Component codegen (@component)
- Struct codegen (regular, @datatable)
- Enum codegen (UENUM)
- Delegate system (multicast, single, dynamic)
- Blueprint functions (@blueprint)
- EngineKnowledge system (500+ UE5 types)
- Oracle validation (22+ semantic rules)

**When to use:** Generating runtime C++ code, actors, components, gameplay logic

---

### 4. ue5-editor
**Path:** `crates/ue5-editor/CRATE_REFERENCE.md`  
**Purpose:** Editor UI codegen - Slate, Details, Viewports, Toolbars, Asset Editors  
**Status:** Production-ready - 10 tests passing  
**Key Features:**
- Slate widget generation (30+ widget types)
- Details panel customization (@slider, @color_picker, @button)
- Viewport generation (SEditorViewport + FEditorViewportClient)
- Toolbar generation (buttons, toggles, separators)
- Asset editor generation (FAssetEditorToolkit)
- Editor module generation (IModuleInterface)
- Smart slot awareness (VBox/HBox/Overlay)
- Delegate bridging (CreateSP, CreateLambda)

**When to use:** Creating editor UI, custom viewports, detail panels, asset editors

---

### 5. ue5-shaders
**Path:** `crates/ue5-shaders/CRATE_REFERENCE.md`  
**Purpose:** Shader codegen - HLSL .usf files, compute/fragment/vertex shaders  
**Status:** Production-ready  
**Key Features:**
- Fragment shader generation
- Compute shader generation
- Vertex shader generation
- Surface shader generation
- Uniform system (types, bindings, @N syntax)
- Permutation system (CFG_*, ENABLE_* prefixes)
- Shader parameter struct generation (FGlobalShader)
- IMPLEMENT_GLOBAL_SHADER registration
- Integration with actors (@dispatch attribute)

**When to use:** Creating GPU shaders, compute kernels, custom rendering

---

### 6. ue5-materials
**Path:** `crates/ue5-materials/CRATE_REFERENCE.md`  
**Purpose:** Material graph codegen - UMaterial assets with node networks  
**Status:** Phase 2 complete  
**Key Features:**
- Node-based material graphs
- 30+ material node types
- Automatic node layout
- PBR support (base_color, metallic, roughness, emissive)
- Material functions (reusable subgraphs)
- Binary .uasset generation
- C++ factory fallback
- Type-safe material expressions

**When to use:** Creating materials, PBR shaders, material functions

---

### 7. ue5-blueprints
**Path:** `crates/ue5-blueprints/CRATE_REFERENCE.md`  
**Purpose:** Blueprint codegen - .uasset generation with Kismet bytecode  
**Status:** Phase 2 complete - 15 tests passing  
**Key Features:**
- Binary .uasset generation (no editor required)
- C++ factory fallback
- Component hierarchy support
- Event graph compilation (BeginPlay, Tick, Custom)
- Kismet bytecode emission
- Property default values
- Data-driven IR architecture
- JSON serialization

**When to use:** Generating Blueprint assets, visual scripting, rapid prototyping

---

### 8. ue5-asset-utils
**Path:** `crates/ue5-asset-utils/CRATE_REFERENCE.md`  
**Purpose:** Asset management utilities  
**Status:** Reference template (implementation in progress)  
**Key Features:**
- Asset path resolution
- Asset loading utilities
- Asset registry integration
- Content browser operations
- Import/export utilities

**When to use:** Managing assets, loading resources, content pipeline

---

## Quick Reference

### By Use Case

**Building a UE5 Plugin:**
1. Start with `cli` - orchestrates the build
2. Use `ue5` for runtime code (actors, components)
3. Use `ue5-editor` for editor UI
4. Use `ue5-shaders` for GPU code
5. Use `ue5-materials` for materials
6. Use `ue5-blueprints` for visual scripting

**Extending the Language:**
1. Start with `kain-core` - add syntax, AST nodes
2. Update `ue5` - add codegen for new features
3. Update `cli` - wire new features into pipeline

**Creating Custom Tools:**
1. Use `kain-core` as a library
2. Parse KAIN source
3. Generate custom output

---

## Documentation Standards

Each `CRATE_REFERENCE.md` follows this structure:

1. **Overview** - What the crate does, key features
2. **Architecture** - Entry points, output structure, core components
3. **Feature Sections** - Detailed documentation for each major feature
4. **File Structure** - Directory layout, key files
5. **Examples** - Real-world usage examples
6. **Testing** - Test coverage, how to run tests
7. **Integration** - How it integrates with other crates
8. **Future Enhancements** - Planned features

---

## Contributing

When adding new features:

1. Update the relevant `CRATE_REFERENCE.md`
2. Add examples showing the new feature
3. Update this index if adding a new crate
4. Keep documentation in sync with code

---

## Additional Resources

- **AGENT_HANDOFF.md** - Architecture overview, bug fixes, priorities
- **llm-first-development.md** - LLM-first development philosophy
- **kain-patterns.md** - KAIN language patterns and conventions
- **automation-hooks.md** - Hook system and automated quality assurance

---

## Summary

The KAIN compiler consists of 8 specialized crates working together to transform KAIN source code into production-ready UE5 plugins. Each crate is fully documented with comprehensive examples, test coverage, and integration guides. The system is designed for LLM-first development with clear error messages, data-driven architecture, and zero manual intervention required.
