# TemporalBlueprint — Plugin Concept & Design Document

## Elevator Pitch

**TemporalBlueprint** is a non-linear, time-state level design system for Unreal Engine 5.

It lets you author a single level that simultaneously exists across **multiple time eras** — Ancient, Past, Present, Future, Apocalyptic, Alternate, and Void — with a visual editor, runtime state machine, causality system, Blueprint API, and full multiplayer replication.

> *"Design the world as it was, as it is, and as it will be — all at once."*

---

## Market Gap

No plugin on the Fab marketplace or anywhere else provides a **general-purpose temporal level design system**. Games like Dishonored (time-stop), Outer Wilds (time loops), Prince of Persia (rewind), and Deathloop (timeline manipulation) all implement bespoke, game-specific solutions from scratch. Every team reinvents the wheel.

**TemporalBlueprint solves this permanently.** It is the first authoring tool that treats time-state as a first-class level design primitive — not a gameplay hack bolted on after the fact.

**Target price: $499–$699 on Fab**

---

## Core Concept

### The Problem with Time in Games

When a designer wants an environment to change over time (a castle in ruins vs. intact, a city before and after a disaster, a forest in spring vs. winter), they currently have two options:

1. **Duplicate the level** — maintain two separate maps, manually keep them in sync, double the asset count
2. **Hack it with Blueprint** — show/hide actors, swap meshes, write custom transition logic per-project

Both approaches are painful, error-prone, and don't scale.

### The TemporalBlueprint Solution

Every actor in the world gets a `TemporalActorComponent`. This component stores:
- Which eras the actor is **visible** in
- What **mesh variant** to show per era
- Whether to render as a **ghost** in non-native eras
- **Custom per-era data** (float slots for material parameters, gameplay state, etc.)

The `TemporalManagerActor` (one per level) orchestrates global era transitions with configurable transition types (dissolve, ripple, shatter, rewind, fold, bleed).

The `TemporalSubsystem` (world subsystem) is the runtime engine — it tracks all registered actors, manages the causality graph, handles snapshots, and drives the timeline fork system.

---

## Feature Set

### Runtime Features

| Feature | Description |
|---------|-------------|
| **7 Time Eras** | Ancient, Past, Present, Future, Apocalyptic, Alternate, Void |
| **7 Transition Types** | Instant, Dissolve, Ripple, Shatter, Rewind, Fold, Bleed |
| **5 Causality Rules** | None, Linear, Branching, Convergent, Paradox |
| **6 Actor Behaviors** | Static, Unique, Interpolated, Conditional, Ghosted, Destroyed |
| **Era Zones** | Spatial volumes that force a specific era inside their bounds |
| **Temporal Anchors** | Points of temporal significance — transition triggers, story beats |
| **Temporal Portals** | Windows into another era — render the other era through a viewport |
| **Snapshot System** | Save/restore world state at any point in time |
| **Timeline Forking** | Branch the timeline based on player actions |
| **Full Replication** | Server-authoritative era state with proper RPCs |

### Editor Features

| Feature | Description |
|---------|-------------|
| **Era Picker Toolbar** | One-click era switching in the level editor |
| **Ghost Mode** | See all era states simultaneously as translucent overlays |
| **Era Config Panel** | Per-era atmosphere, audio, post-process, fog, time-of-day |
| **Snapshot Manager** | Browse, compare, and restore temporal snapshots |
| **Causality Inspector** | Visualize and edit causality links between actors |
| **Actor Inspector** | Per-actor era visibility matrix and custom data editor |
| **Viewport Overlay** | Era zone bounds, anchor icons, causality links in viewport |
| **Details Customization** | Rich Details panels for all Temporal actors |
| **Toolbar Extension** | Temporal Blueprint toolbar in the Level Editor |

### Blueprint API

50+ Blueprint-callable functions including:

```
// Era management
request_era_transition(target_era)
request_era_transition_with_type(target_era, transition_type)
get_current_era() -> TemporalEra
get_transition_alpha() -> Float
is_transitioning() -> Bool
lock_era() / unlock_era()

// Snapshots
take_snapshot()
restore_snapshot(id)

// Timeline
fork_timeline(branch_condition)
collapse_timeline(node_id)

// Utilities
era_to_string(era) -> String
era_distance(from, to) -> Int
validate_causality(rule, from, to) -> Bool
get_era_default_color(era) -> Vec3
lerp_era_colors(from, to, alpha) -> Vec3
ripple_alpha_at_distance(center, point, radius, alpha) -> Float
```

---

## Architecture

### Component Hierarchy

```
Level
├── TemporalManagerActor          (1 per level — global orchestrator)
│   └── TemporalSubsystem         (world subsystem — runtime engine)
│
├── TemporalActorProxy            (any world object with era states)
│   └── TemporalActorComponent    (per-actor era state manager)
│
├── TemporalZoneActor             (spatial era override volume)
│   └── TemporalZoneComponent     (zone behavior)
│
├── TemporalAnchorActor           (transition trigger / story beat)
│   └── TemporalAnchorComponent   (anchor behavior)
│
└── TemporalPortalActor           (window into another era)
```

### Data Flow

```
Player Input / Game Event
        ↓
TemporalManagerActor.request_era_transition(target_era)
        ↓
Server_RequestEraTransition RPC (server-authoritative)
        ↓
Causality validation (CausalityRule check)
        ↓
Multicast_BeginTransition → all clients
        ↓
TemporalSubsystem.request_transition()
        ↓
Per-actor: TemporalActorComponent.on_era_changed()
        ↓
Mesh variant swap + material update + visibility toggle
        ↓
Camera: TemporalCameraComponent.begin_transition_effect()
        ↓
Multicast_CompleteTransition → era committed
        ↓
Autosave snapshot (if enabled)
```

### Causality System

The causality system prevents logically impossible transitions. Five rules:

- **None** — eras are fully independent, any transition allowed
- **Linear** — can only transition ±2 eras at a time (no jumping from Ancient to Apocalyptic)
- **Branching** — any transition allowed, but creates a new timeline branch
- **Convergent** — all branches must eventually reach Present or Future
- **Paradox** — contradictions allowed and tracked (for puzzle games)

Causality links between specific actors can be defined in the editor — "if this building is destroyed in the Past, it cannot exist in the Present."

### Snapshot System

Snapshots capture the full world state (all actor era states, visibility, custom data) at a point in time. Use cases:

- **Autosave** on every era transition
- **Undo/redo** for level design
- **Save games** — restore exact world state on load
- **Debugging** — compare snapshots to find state divergence

---

## Use Cases

### Puzzle Games
Design puzzles where the player must manipulate the past to change the present. A bridge destroyed in the Past cannot exist in the Present — the player must find a way to prevent its destruction.

### Horror Games
The same location in different states of decay. The Present is a normal house; the Past shows it being built; the Future shows it as a ruin. Transition between them to reveal the story.

### Action Games
Time-rewind mechanics (Prince of Persia style). The snapshot system provides the data; the transition system provides the visual effect.

### Open World RPGs
Seasonal or historical variation. The same village in spring, summer, autumn, winter — or before and after a war. Zone actors can force specific eras in specific areas.

### Narrative Games
Environmental storytelling through time. Walk through a portal and see the room as it was 50 years ago. Causality links ensure story consistency.

### Multiplayer Games
Server-authoritative era state with full replication. All players see the same era transitions simultaneously. Era locking prevents transitions during critical gameplay moments.

---

## Technical Specifications

### Generated C++ Output

From ~900 lines of KAIN source, TemporalBlueprint generates:

| Category | Files | Approximate Lines |
|----------|-------|-------------------|
| Enums | 11 headers | ~330 |
| Structs | 18 headers | ~900 |
| Components | 4 header/cpp pairs | ~1,200 |
| Actors | 5 header/cpp pairs | ~2,500 |
| Subsystems | 2 header/cpp pairs | ~800 |
| Blueprint Library | 1 header/cpp pair | ~1,500 |
| Slate UI Panels | 5 header/cpp pairs | ~3,000 |
| Details Customizations | 5 header/cpp pairs | ~1,500 |
| Viewport Overlay | 1 header/cpp pair | ~400 |
| Toolbar Extension | 1 header/cpp pair | ~300 |
| Module Registration | 2 cpp files | ~200 |
| Build.cs | 2 files | ~120 |
| .uplugin | 1 file | ~60 |
| **Total** | **~57 files** | **~12,810 lines** |

**Compression ratio: ~14x** (900 KAIN → ~12,800 C++)

### Module Structure

```
TemporalBlueprint/           (Runtime module)
├── Source/TemporalBlueprint/
│   ├── Public/              (all headers)
│   └── Private/             (all implementations)

TemporalBlueprintEditor/     (Editor module, PostEngineInit)
├── Source/TemporalBlueprintEditor/
│   ├── Public/              (Slate, Details, Toolbar headers)
│   └── Private/             (implementations)
```

### Dependencies

**Runtime:** Core, CoreUObject, Engine, InputCore, RenderCore, RHI, GameplayTags, GeometryCore, DeveloperSettings, NetCore

**Editor:** + UnrealEd, Slate, SlateCore, EditorStyle, PropertyEditor, LevelEditor, ContentBrowser, AssetTools, ToolMenus, WorkspaceMenuStructure, SceneOutliner, EditorSubsystem, AdvancedPreviewScene

---

## DataTable Integration

Four DataTables for designer-facing configuration (no C++ required):

| DataTable | Purpose |
|-----------|---------|
| `TemporalEraPresetData` | Per-era atmosphere, color, audio, post-process presets |
| `TemporalTransitionPresetData` | Named transition presets (duration, easing, VFX, sound) |
| `TemporalActorPresetData` | Actor behavior presets (visibility masks, ghost settings) |
| `TemporalZonePresetData` | Zone configuration presets |

---

## Competitive Analysis

| Product | What It Does | Price | Gap |
|---------|-------------|-------|-----|
| No existing plugin | — | — | **TemporalBlueprint fills this gap entirely** |
| Custom game code | Per-project, not reusable | N/A | No authoring tools, no editor |
| Level streaming | Loads/unloads sublevels | Free | No time concept, no transitions, no causality |
| World Partition | Streaming cells | Free | No time concept |

**There is no direct competitor.** TemporalBlueprint is a category-defining product.

---

## Development Roadmap

### v1.0 (Current)
- ✅ 7 time eras
- ✅ 7 transition types
- ✅ 5 causality rules
- ✅ 5 actor types (Manager, Proxy, Zone, Anchor, Portal)
- ✅ 4 components (Actor, Zone, Anchor, Camera)
- ✅ 2 subsystems (Runtime, Editor)
- ✅ 50+ Blueprint functions
- ✅ Full editor suite (5 Slate panels, 5 Details customizations, toolbar, viewport overlay)
- ✅ Full replication
- ✅ Snapshot system
- ✅ Timeline forking
- ✅ DataTable integration

### v1.1 (Planned)
- Visual causality graph editor (UEdGraph-based)
- Per-actor timeline strip in Details panel
- Transition preview in viewport (without committing)
- Export/import era configurations as assets

### v1.2 (Planned)
- Shader-based transition effects (custom USF)
- Audio subsystem integration (MetaSounds)
- Niagara VFX integration for transition effects
- Sequencer integration for cinematic era transitions

### v2.0 (Planned)
- Procedural era generation (AI-assisted era variation)
- Network-optimized delta snapshots
- Mobile platform support
- Console platform support

---

## Pricing Strategy

| Tier | Price | Includes |
|------|-------|---------|
| **Standard** | $499 | Full plugin, source code, documentation |
| **Pro** | $699 | + Priority support, example project, video tutorials |
| **Studio** | $1,499 | + Unlimited seats, custom integration support |

**Comparable products:**
- MetaHuman Animator: $0 (Epic-owned, free)
- Procedural Content Generation: $0 (built-in)
- Advanced Locomotion System: $34.99 (community)
- Modular Game Features: $0 (built-in)
- **TemporalBlueprint: $499** — unique, no competition, solves a real production problem

---

## KAIN Source Summary

| File | Lines | Purpose |
|------|-------|---------|
| `types.kn` | ~280 | 14 enums, 18 structs, 4 DataTables |
| `components.kn` | ~330 | 4 components with Blueprint API |
| `actors.kn` | ~420 | 5 actors with full RPC system |
| `subsystems.kn` | ~280 | Runtime + Editor subsystems |
| `algorithms.kn` | ~310 | 40+ Blueprint utility functions |
| `editor.kn` | ~26 | Editor module registration |
| `editor_ui.kn` | ~240 | 5 Slate panels + viewport overlay |
| `editor_toolbar.kn` | ~40 | Toolbar extension |
| `details.kn` | ~200 | 5 Details panel customizations |
| **Total** | **~2,126** | **→ ~12,800 lines C++** |
