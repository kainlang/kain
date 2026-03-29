# ModularBuilder Plugin

**Version:** 1.0.0  
**Category:** Level Design Tools  
**Engine:** Unreal Engine 5.4+  
**Language:** KAIN → UE5 C++

## Overview

ModularBuilder is a comprehensive modular building system for Unreal Engine 5 that enables designers and players to create complex structures using snap-together building pieces. The plugin features a complete editor UI, multiplayer support, structural validation, and a powerful Blueprint API.

## Features

### Core Building System
- **Snap Point System** - Intelligent snap point detection with spatial grid optimization
- **12 Snap Point Types** - Wall, Floor, Ceiling, Corner, Edge, Pillar, Roof, Foundation, Decorative, Custom
- **4 Alignment Modes** - Exact, Rotate90, Rotate45, Free rotation
- **Visual Indicators** - Color-coded snap point indicators (green=valid, red=invalid, yellow=weak support)
- **Tag-Based Filtering** - Filter pieces by tags with AND/OR logic
- **Variant System** - Multiple mesh variants per piece type

### Editor UI (Slate Widgets)
- **Piece Browser** - Category tree navigation, thumbnail grid, search and filtering
- **Toolbar** - 12 building mode buttons (Place, Select, Delete, Rotate, Undo/Redo, etc.)
- **Details Panel** - 8 property sections for selected pieces
- **Preview Widget** - 3D preview with rotation controls and variant selection
- **Snap Indicator** - Real-time snap point information overlay
- **Category Tree** - Hierarchical category organization with drag-and-drop

### Structural System
- **Foundation Support** - Mark pieces as foundations that provide infinite support
- **Support Chains** - Automatic calculation of structural dependencies
- **Validation** - Real-time structural integrity checking
- **Collapse Physics** - Optional physics-based collapse for unsupported pieces
- **Visualization** - Debug visualization of support chains

### Multiplayer Support
- **Server Authority** - All placement operations validated on server
- **Full Replication** - Piece transforms, connections, and properties replicated
- **Ownership Tracking** - Track which player placed each piece
- **Building Permissions** - Per-player or per-team building zones

### Performance Optimization
- **Spatial Hashing** - O(1) snap point queries with configurable grid size
- **Instanced Rendering** - Automatic batching of identical pieces
- **Actor Pooling** - Pre-allocated piece actors to reduce allocation overhead
- **HLOD Support** - Hierarchical LOD for distant building clusters
- **Performance Modes** - Quality, Balanced, Performance presets

### Persistence
- **Save/Load System** - JSON serialization of complete building state
- **Undo/Redo** - Command history with configurable depth (default 50 actions)
- **Export Options** - Export to static mesh, JSON, or FBX
- **Auto-Save** - Automatic building snapshots
- **Version Migration** - Backward compatibility for save formats

### Blueprint Integration
- **40+ Blueprint Functions** - Comprehensive Blueprint API
- **8 Blueprint Events** - OnPiecePlaced, OnPieceRemoved, OnPieceSnapped, etc.
- **Runtime Building** - Full support for player-controlled building
- **Procedural Generation** - Template-based structure generation

## Architecture

### File Structure
```
ModularBuilder/
├── src/
│   ├── building_data_structures.kn    (~1,200 LOC)
│   ├── building_snap_system.kn        (~1,400 LOC)
│   ├── building_actors.kn             (~1,800 LOC)
│   ├── building_editor_ui.kn          (~1,500 LOC)
│   ├── building_subsystems.kn         (~1,200 LOC)
│   ├── building_blueprint_library.kn  (~900 LOC)
│   └── building_persistence.kn        (~500 LOC)
├── KAIN.toml
├── README.md
├── requirements.md
├── design.md
├── tasks.md
└── feature_checklist.md
```

### Core Components

#### Data Layer
- **12 Enums** - SnapPointType, AlignmentMode, BuildMode, PieceCategory, etc.
- **25 Structs** - SnapPointData, BuildingPieceDefinition, PieceInstance, etc.
- **4 DataTables** - PieceDataTable, CategoryDataTable, SnapCompatibilityTable, MaterialCollectionTable

#### Actor Layer
- **BuildingPieceActor** - Individual building piece with snap points and replication
- **BuildManagerActor** - Central manager for building operations and state
- **SnapPointActor** - Visual indicator for snap points
- **StructuralSupportActor** - Manages structural integrity calculations
- **BuildingGhostActor** - Preview ghost for piece placement

#### Subsystem Layer
- **BuildManagerSubsystem** - World-level building management with tick updates
- **SnapManagerSubsystem** - Centralized snap point detection and validation
- **PerformanceSubsystem** - Performance monitoring and optimization

#### Editor UI Layer
- **PieceBrowserWidget** - Slate widget for browsing and selecting pieces
- **ToolbarWidget** - Building mode toolbar with 12 buttons
- **DetailsPanelWidget** - Property editing for selected pieces
- **PreviewWidget** - 3D preview with rotation controls
- **SnapIndicatorWidget** - Snap point information overlay
- **CategoryTreeWidget** - Hierarchical category tree

## Quick Start

### 1. Compilation

```bash
cd FactoryPart2/plugins/ModularBuilder
kain build --ue5
```

### 2. Setup in UE5

1. Copy generated plugin to `YourProject/Plugins/ModularBuilder/`
2. Enable plugin in UE5 Editor
3. Restart editor

### 3. Create Piece Definitions

Create a DataTable asset using `PieceDataTable` struct:

```
piece_id: "wall_stone_01"
display_name: "Stone Wall"
category: Walls
mesh_path: "/Game/Meshes/Walls/StoneWall01"
thumbnail_path: "/Game/Thumbnails/StoneWall01"
snap_point_count: 4
tags: "medieval,stone,wall"
cost: 10
structural_type: Support
```

### 4. Spawn Build Manager

Place `BuildManagerActor` in your level or spawn via Blueprint:

```cpp
// Blueprint
Spawn Actor from Class -> BuildManagerActor
```

### 5. Place Pieces

Use Blueprint functions:

```cpp
// Spawn a piece
Spawn Building Piece(Manager, "wall_stone_01", Location, Rotation)

// Find snap points
Find Nearest Snap Point(Piece, SearchRadius)

// Connect pieces
Connect Pieces At Snap Points(SourcePiece, SourceSnapIndex, TargetPiece, TargetSnapIndex)
```

## Blueprint API

### Piece Spawning (10 functions)
- `spawn_building_piece` - Spawn piece at location
- `spawn_piece_with_variant` - Spawn with specific variant
- `spawn_piece_at_snap_point` - Spawn at snap point
- `spawn_multiple_pieces` - Batch spawn
- `spawn_piece_grid` - Spawn in grid pattern
- `spawn_piece_circle` - Spawn in circle pattern
- `spawn_piece_line` - Spawn along line
- `spawn_random_pieces` - Spawn randomly in bounds
- `clone_piece` - Clone existing piece

### Piece Query (10 functions)
- `get_piece_by_instance_id` - Get piece by ID
- `get_all_pieces_in_building` - Get all pieces
- `find_pieces_by_category` - Filter by category
- `find_pieces_by_tag` - Filter by tag
- `find_pieces_in_sphere` - Spatial query (sphere)
- `find_pieces_in_box` - Spatial query (box)
- `find_connected_pieces` - Get connected pieces
- `find_foundation_pieces` - Get all foundations
- `find_unsupported_pieces` - Get unsupported pieces

### Snap Utilities (10 functions)
- `find_nearest_snap_for_piece` - Find nearest snap point
- `get_available_snap_points_for_piece` - Get available snaps
- `is_snap_point_occupied_on_piece` - Check occupation
- `calculate_snap_transform` - Calculate snap alignment
- `validate_snap_between_pieces` - Validate compatibility
- `connect_pieces_at_snap_points` - Create connection
- `disconnect_pieces` - Remove connection
- `get_all_snap_connections_for_piece` - Get all connections
- `calculate_snap_distance` - Calculate distance

### Structural Utilities (10 functions)
- `validate_structural_support` - Check support
- `calculate_support_chain_for_piece` - Get support chain
- `mark_piece_as_foundation` - Mark as foundation
- `check_if_piece_is_foundation` - Check foundation status
- `get_all_supported_pieces` - Get supported pieces
- `calculate_structural_integrity` - Calculate integrity (0-1)
- `find_weak_structural_points` - Find weak points
- `collapse_unsupported_structure` - Trigger collapse
- `rebuild_structural_chains` - Recalculate chains
- `calculate_piece_stability` - Get stability (0-1)

## Configuration

### KAIN.toml

```toml
[package]
name = "ModularBuilder"
version = "1.0.0"

[ue5]
plugin_name = "ModularBuilder"
engine_version = "5.4"
category = "Level Design"

[[ue5.modules]]
name = "ModularBuilder"
type = "Runtime"

[[ue5.modules]]
name = "ModularBuilderEditor"
type = "Editor"
depends_on = ["ModularBuilder"]
```

### Performance Settings

```cpp
// Set performance mode
Set Performance Mode(PerformanceMode::Balanced)

// Enable instancing
Enable Instancing(true)

// Enable HLOD
Enable HLOD(true)

// Set spatial grid cell size
Grid Cell Size = 500.0 (default)

// Set max undo depth
Max Undo Depth = 50 (default)
```

## Performance Targets

- **10,000+ pieces** - Supported with <16ms frame time
- **Snap queries** - <1ms for 100 nearby pieces
- **Placement latency** - <50ms from input to feedback
- **Memory usage** - Optimized with actor pooling and instancing

## Requirements

- Unreal Engine 5.4+
- KAIN compiler (latest version)
- 50+ functional requirements implemented
- Zero TODOs or placeholders
- Full multiplayer support
- Complete editor integration

## Statistics

- **Total KAIN LOC:** ~8,500
- **Expected C++ LOC:** ~17,000-20,000
- **Source Files:** 7
- **Actors:** 5
- **Subsystems:** 3
- **Slate Widgets:** 6
- **Blueprint Functions:** 40+
- **Enums:** 12
- **Structs:** 25
- **DataTables:** 4

## License

Part of the KAIN Factory Assembly Line project.

## Support

For issues, questions, or contributions, see the main Factory documentation.
