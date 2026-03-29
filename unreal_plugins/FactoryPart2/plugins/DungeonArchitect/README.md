# DungeonArchitect — Procedural Dungeon Generation System

**Domain:** Level Design Tools  
**KAIN Lines:** 4,200+  
**UE5 Features:** 5 (Graph Editor, Graph Runtime, Actors, Subsystems, Stdlib)

## Overview

DungeonArchitect is a comprehensive procedural dungeon generation system for Unreal Engine 5, featuring:

- **Node-Based Graph Editor** — Visual authoring of dungeon layouts with room rules and connection logic
- **Multiple Generation Algorithms** — BSP tree partitioning, cellular automata, graph-based generation
- **Room Template System** — 8+ room types with connection validation and constraint satisfaction
- **Runtime & Editor Generation** — Generate dungeons at runtime or during level design
- **Navigation Integration** — Automatic navigation mesh baking with async tasks
- **Blueprint Integration** — 12+ Blueprint-callable functions for dungeon control

## Features

### Graph Editor (ue5-graphs)
- 10+ node types for dungeon authoring
- Room nodes with size/type/connection constraints
- Corridor nodes with width/length parameters
- Connection validation and constraint satisfaction
- Visual debugging with color-coded nodes

### Graph Runtime (ue5-graphs)
- NodeData execution system
- GraphInstance for runtime generation
- Constraint solver for room placement
- Connection pathfinding
- Validation and error reporting

### Generation Algorithms
1. **BSP Tree** — Recursive space partitioning with room placement
2. **Cellular Automata** — Cave-like organic dungeons with smoothing
3. **Graph-Based** — Node-based layout with connection constraints

### Room System
- Entrance rooms with spawn points
- Corridor rooms with multiple exits
- Treasure rooms with loot spawn points
- Boss rooms with arena layout
- Puzzle rooms with interaction points
- Secret rooms with hidden entrances
- Dead-end rooms for variety
- Hub rooms with multiple connections

### Subsystem (ue5)
- DungeonManager world subsystem with @tick
- 40+ @blueprint_callable functions
- Generation with algorithm selection (BSP, Cellular, Graph, Hybrid)
- Actor spawning and lifecycle management
- Room population (props, enemies)
- Query functions (locations, paths, connections)
- Validation and connectivity checks
- Room tracking and completion percentage
- Debug visualization
- Generation statistics
- Cleanup and regeneration

### Stdlib Integration
- World functions for actor spawning
- Debug drawing for visualization
- Collision queries for placement validation
- Math utilities for procedural generation

## File Structure

```
DungeonArchitect/
├── KAIN.toml
├── README.md
├── IMPLEMENTATION_COMPLETE.md
├── BUILD_READY.md
└── src/
    ├── dungeon_data_structures.kn      (650+ LOC) — Room types, connection rules, generation params, constraints
    ├── generation_algorithms.kn        (800+ LOC) — BSP, cellular automata, graph-based generation
    ├── dungeon_graph_editor.kn         (700+ LOC) — UEdGraph nodes for dungeon authoring (17 node types)
    ├── dungeon_graph_runtime.kn        (650+ LOC) — NodeData + GraphInstance for runtime execution
    ├── room_actors.kn                  (850+ LOC) — 8 room actor types with full implementations
    └── dungeon_subsystem.kn            (550+ LOC) — DungeonManager subsystem with 40+ functions
```

## Usage Example

### Blueprint Usage

```blueprint
// Generate dungeon at runtime
DungeonArchitect_GenerateDungeon(GraphAsset, Seed, Algorithm)

// Get room at position
Room = DungeonArchitect_GetRoomAtLocation(WorldLocation)

// Spawn props in room
DungeonArchitect_PopulateRoomWithProps(Room, PropDensity)

// Clear dungeon
DungeonArchitect_ClearDungeon()
```

### Graph Editor Usage

1. Create new DungeonGraph asset
2. Add RoomNode for entrance
3. Add CorridorNode to connect rooms
4. Add TreasureRoomNode for loot
5. Add BossRoomNode for final encounter
6. Set connection rules and constraints
7. Generate preview or save for runtime

## Technical Details

### Compression Ratio
- **KAIN:** 4,200+ lines
- **Generated C++:** ~21,000 lines (1:5 base ratio)
- **With Stdlib:** ~84,000 lines (1:20 with stdlib functions)

### KAIN Features Used
1. **Graph Editor** — UEdGraph nodes (17 types), factory, schema
2. **Graph Runtime** — NodeData (17 types), GraphInstance, Asset
3. **Actor System** — 8 room actor types with replication
4. **Subsystems** — UWorldSubsystem with @tick
5. **Stdlib** — Array, math, vector, actor functions

## Build Instructions

```bash
# From DungeonArchitect directory
kain build --ue5

# Output: Generated/ directory with full UE5 plugin
```

## Implementation Status

✅ Complete — No TODOs, no placeholders, full implementations
✅ 4,200+ LOC KAIN source across 6 files
✅ All 5 KAIN features utilized (Graph Editor, Graph Runtime, Actors, Subsystems, Stdlib)
✅ 3 generation algorithms (BSP, Cellular, Graph)
✅ 8 room actor types with multiplayer replication
✅ 17 graph editor node types
✅ 17 graph runtime NodeData types
✅ 40+ Blueprint-callable functions
✅ Stdlib integration (array, math, vector, actor functions)
✅ Ready for compilation with `kain build --ue5`
