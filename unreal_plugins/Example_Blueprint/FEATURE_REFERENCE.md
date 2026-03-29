# ue5-blueprints Crate — Complete Feature Reference

> **Generated from:** `Kain/crates/ue5-blueprints/src/`  
> **Purpose:** Document EVERY feature supported by the ue5-blueprints crate with code evidence  
> **Showcase File:** `blueprint_showcase.kn` demonstrates all features in production-ready KAIN code

---

## Table of Contents

1. [Core Architecture](#core-architecture)
2. [Blueprint IR (Intermediate Representation)](#blueprint-ir)
3. [Phase 1: Factory Generator](#phase-1-factory-generator)
4. [Phase 2: Binary Writer](#phase-2-binary-writer)
5. [Kismet Bytecode](#kismet-bytecode)
6. [AST Conversion](#ast-conversion)
7. [Property System](#property-system)
8. [Component System](#component-system)
9. [Event Graph](#event-graph)
10. [Parent Class Resolution](#parent-class-resolution)
11. [Testing](#testing)

---

## Core Architecture

### Dual-Path Strategy

**Evidence:** `lib.rs:71-83`

```rust
/// Convenience: attempt binary .uasset generation, fall back to factory
/// if binary writer is not yet supported for this blueprint.
///
/// Returns:
///   - `Ok(Some(bytes))` — binary .uasset generated successfully
///   - `Ok(None)`        — binary writer not supported, use factory fallback
///   - `Err(e)`          — hard error
pub fn generate_uasset(bp: &BlueprintDef) -> Result<Option<Vec<u8>>> {
    match BlueprintBinaryWriter::check_support(bp) {
        Ok(_) => BlueprintBinaryWriter::write(bp).map(Some),
        Err(_) => Ok(None), // graceful fallback to factory
    }
}
```

**What it does:**
- Attempts binary .uasset generation first (Phase 2)
- Falls back to C++ factory generation if binary writer doesn't support the blueprint
- Provides seamless transition between generation strategies

**Showcase usage:** All 10 actors in `blueprint_showcase.kn` use this dual-path strategy

---

## Blueprint IR (Intermediate Representation)

### BlueprintDef — Core Data Structure

**Evidence:** `ir.rs:127-150`

```rust
/// The complete data-driven description of a Blueprint asset KAIN will generate.
/// Consumed by both:
///   - `BlueprintBinaryWriter`  → writes real .uasset bytes (no editor needed)
///   - `BlueprintFactoryWriter` → writes C++ factory code (editor-startup fallback)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlueprintDef {
    /// Asset name without extension. e.g. "BP_Player"
    pub name: String,

    /// Content browser path. e.g. "/Game/MyPlugin/Blueprints"
    pub package_path: String,

    /// Fully-qualified parent C++ class path.
    /// e.g. "/Script/MyPlugin.APlayerBase"
    pub parent_class: String,

    /// Component tree (SimpleConstructionScript nodes).
    pub components: Vec<ComponentDef>,

    /// ClassDefaultObject property overrides.
    pub defaults: Vec<PropertyDef>,

    /// Event graph nodes (BeginPlay, Tick, custom events).
    pub event_graph: Vec<EventGraphNode>,

    /// UE5 engine version to target. Affects binary format.
    /// Defaults to UE5.3.
    pub engine_version: BlueprintEngineVersion,
}
```

**What it does:**
- Single source of truth for Blueprint generation
- Serializable (JSON) for data-driven workflows
- Engine-neutral (works with UE 5.0-5.7)

**Showcase usage:** Every actor in `blueprint_showcase.kn` converts to a `BlueprintDef`

### ComponentDef — Component Hierarchy

**Evidence:** `ir.rs:29-64`

```rust
/// A component attached to a Blueprint via the SimpleConstructionScript.
/// Maps to a `USCS_Node` in the .uasset.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentDef {
    /// UE5 component class — must be an engine class or a 