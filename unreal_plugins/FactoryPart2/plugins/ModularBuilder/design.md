# ModularBuilder - Design Document

## 1. Architecture Overview

ModularBuilder is a modular building system with snap-together pieces, editor UI, and Blueprint integration. The architecture consists of 7 core subsystems:

1. **Data Layer** - Enums, structs, DataTables for piece definitions
2. **Snap System** - Snap point detection, validation, and alignment
3. **Actor System** - Building pieces, managers, and structural components
4. **Editor UI** - Slate widgets for piece browser, toolbar, and details
5. **Subsystem Layer** - World subsystems for building management and performance
6. **Blueprint Integration** - 30+ Blueprint-callable functions
7. **Persistence** - Save/load system with JSON serialization

### 1.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    ModularBuilder Plugin                     │
├─────────────────────────────────────────────────────────────┤
│  Editor UI Layer (Slate)                                    │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │ PieceBrowser │ │   Toolbar    │ │DetailsPanel  │       │
│  └──────────────┘ └──────────────┘ └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  Subsystem Layer                                            │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │BuildManager  │ │SnapManager   │ │Performance   │       │
│  └──────────────┘ └──────────────┘ └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  Actor Layer                                                │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │BuildingPiece │ │BuildManager  │ │SnapPoint     │       │
│  └──────────────┘ └──────────────┘ └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  Data Layer                                                 │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │PieceData     │ │SnapPointData │ │BuildingState │       │
│  └──────────────┘ └──────────────┘ └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

## 2. File Structure

### 2.1 Source Files (7 files, ~8,500 LOC target)

1. **building_data_structures.kn** (~1,200 LOC)
   - 12 enums, 25 structs, 4 DataTables
   - Core data types for pieces, snap points, categories

2. **building_snap_system.kn** (~1,400 LOC)
   - Snap point detection and validation
   - Alignment algorithms, compatibility checking
   - 20+ Blueprint functions

3. **building_actors.kn** (~1,800 LOC)
   - 5 actors: BuildingPieceActor, BuildManagerActor, SnapPointActor, StructuralSupportActor, BuildingGhostActor
   - Full replication support
   - 25+ Blueprint functions

4. **building_editor_ui.kn** (~1,500 LOC)
   - 6 Slate widgets: PieceBrowserWidget, ToolbarWidget, DetailsPanelWidget, PreviewWidget, SnapIndicatorWidget, CategoryTreeWidget
   - Editor mode integration

5. **building_subsystems.kn** (~1,200 LOC)
   - 3 subsystems: BuildManagerSubsystem, SnapManagerSubsystem, PerformanceSubsystem
   - All with @tick support

6. **building_blueprint_library.kn** (~900 LOC)
   - 40+ Blueprint-callable functions
   - Utility functions for building operations

7. **building_persistence.kn** (~500 LOC)
   - Save/load system with JSON serialization
   - Undo/redo command history
   - Export to static mesh



## 3. Data Structures Design

### 3.1 Enums (12)

```kain
enum SnapPointType:
    Wall, Floor, Ceiling, Corner, Edge, Custom

enum AlignmentMode:
    Exact, Rotate90, Rotate45, Free

enum BuildMode:
    Place, Select, Delete, Rotate, Paint

enum PieceCategory:
    Walls, Floors, Roofs, Stairs, Doors, Windows, Decorative, Structural, Custom

enum StructuralType:
    Foundation, Support, Dependent, Decorative

enum ValidationResult:
    Valid, InvalidSnap, Overlapping, NoSupport, OutOfBounds, Custom

enum PlacementRule:
    RequireGround, RequireSupport, AllowOverlap, RequireSnap, Custom

enum SnapIndicatorColor:
    Green, Yellow, Red, Blue, White

enum BuildPermission:
    Owner, Team, Public, Admin

enum SerializationFormat:
    JSON, Binary

enum ExportFormat:
    StaticMesh, JSON, FBX

enum PerformanceMode:
    Quality, Balanced, Performance
```

### 3.2 Core Structs (25)

```kain
struct SnapPointData:
    point_type: SnapPointType
    local_position: Vec3
    local_rotation: Vec3
    snap_radius: Float
    alignment_mode: AlignmentMode
    compatible_types: Array<SnapPointType>
    required_tags: Array<String>
    is_occupied: Bool
    connected_piece_id: Int

struct BuildingPieceDefinition:
    piece_id: String
    display_name: String
    category: PieceCategory
    mesh_variants: Array<String>
    snap_points: Array<SnapPointData>
    tags: Array<String>
    structural_type: StructuralType
    placement_rules: Array<PlacementRule>
    bounds_size: Vec3
    cost: Int

struct PieceInstance:
    instance_id: Int
    piece_id: String
    variant_index: Int
    world_transform: Transform
    snap_connections: Array<Int>
    material_overrides: Array<String>
    custom_properties: Map<String, String>
    owner_id: Int
    is_valid: Bool

struct SnapQueryResult:
    found_snap: Bool
    snap_point_index: Int
    target_piece_id: Int
    target_snap_index: Int
    alignment_transform: Transform
    distance: Float
    validation_result: ValidationResult

struct BuildingState:
    building_id: String
    piece_instances: Array<PieceInstance>
    total_piece_count: Int
    bounds_min: Vec3
    bounds_max: Vec3
    creation_time: Float
    last_modified_time: Float
    version: Int

struct StructuralChain:
    foundation_piece_id: Int
    supported_piece_ids: Array<Int>
    chain_depth: Int
    is_stable: Bool
    support_strength: Float

struct CategoryNode:
    category_name: String
    parent_category: String
    child_categories: Array<String>
    piece_ids: Array<String>
    icon_path: String
    is_expanded: Bool

struct PlacementValidation:
    is_valid: Bool
    validation_result: ValidationResult
    error_message: String
    suggested_fix: String
    conflicting_pieces: Array<Int>

struct BuildCommand:
    command_type: String
    piece_id: String
    transform: Transform
    timestamp: Float
    can_undo: Bool
    can_redo: Bool

struct PerformanceMetrics:
    total_pieces: Int
    snap_query_time_ms: Float
    render_time_ms: Float
    memory_usage_mb: Float
    frame_time_ms: Float
    instanced_mesh_count: Int
```

### 3.3 DataTable Structs (4)

```kain
@datatable
struct PieceDataTable:
    piece_id: String
    display_name: String
    category: PieceCategory
    mesh_path: String
    thumbnail_path: String
    snap_point_count: Int
    tags: String

@datatable
struct CategoryDataTable:
    category_name: String
    parent_category: String
    display_order: Int
    icon_path: String
    description: String

@datatable
struct SnapCompatibilityTable:
    source_type: SnapPointType
    target_type: SnapPointType
    is_compatible: Bool
    alignment_mode: AlignmentMode

@datatable
struct MaterialCollectionTable:
    collection_name: String
    material_paths: String
    preview_texture: String
    tags: String
```

## 4. Actor Design

### 4.1 BuildingPieceActor

**Purpose:** Represents a single placed building piece with snap points and structural data.

**State (15 fields):**
- piece_definition: BuildingPieceDefinition
- instance_id: Int
- variant_index: Int
- snap_connections: Array<Int>
- structural_chain: StructuralChain
- is_foundation: Bool
- is_supported: Bool
- material_overrides: Array<String>
- owner_player_id: Int
- placement_time: Float
- custom_properties: Map<String, String>
- mesh_component: StaticMeshComponent
- snap_point_components: Array<SceneComponent>
- collision_enabled: Bool
- is_ghost: Bool

**RPCs (8):**
- Server_SetVariant(variant_index: Int)
- Server_SetMaterialOverride(slot: Int, material_path: String)
- Server_UpdateSnapConnection(snap_index: Int, target_piece_id: Int)
- Server_SetCustomProperty(key: String, value: String)
- Server_ValidatePlacement() -> ValidationResult
- Multicast_OnPiecePlaced()
- Multicast_OnPieceRemoved()
- Multicast_OnStructuralStateChanged(is_supported: Bool)

**Blueprint Functions (12):**
- get_snap_point_world_transform(snap_index: Int) -> Transform
- get_available_snap_points() -> Array<Int>
- is_snap_point_occupied(snap_index: Int) -> Bool
- get_connected_pieces() -> Array<BuildingPieceActor>
- set_ghost_mode(enabled: Bool)
- validate_placement_at_location(location: Vec3) -> ValidationResult
- get_bounds() -> Box
- has_tag(tag: String) -> Bool
- get_structural_support_chain() -> StructuralChain
- calculate_cost() -> Int
- export_to_json() -> String
- clone_piece() -> BuildingPieceActor



### 4.2 BuildManagerActor

**Purpose:** Central manager for building operations, piece spawning, and state management.

**State (12 fields):**
- active_pieces: Array<BuildingPieceActor>
- piece_definitions: Map<String, BuildingPieceDefinition>
- building_state: BuildingState
- current_build_mode: BuildMode
- selected_pieces: Array<Int>
- undo_history: Array<BuildCommand>
- redo_history: Array<BuildCommand>
- max_undo_depth: Int
- spatial_grid: Map<Vec3, Array<Int>>
- grid_cell_size: Float
- enable_structural_validation: Bool
- enable_multiplayer: Bool

**RPCs (10):**
- Server_SpawnPiece(piece_id: String, transform: Transform, variant: Int) -> Int
- Server_RemovePiece(instance_id: Int, remove_connected: Bool)
- Server_MovePiece(instance_id: Int, new_transform: Transform)
- Server_RotatePiece(instance_id: Int, rotation_delta: Vec3)
- Server_SetBuildMode(mode: BuildMode)
- Server_SelectPieces(piece_ids: Array<Int>)
- Server_UndoLastAction()
- Server_RedoLastAction()
- Server_SaveBuilding(save_name: String) -> Bool
- Server_LoadBuilding(save_name: String) -> Bool
- Multicast_OnBuildModeChanged(mode: BuildMode)
- Multicast_OnPieceSpawned(instance_id: Int)

**Blueprint Functions (15):**
- spawn_piece_at_location(piece_id: String, location: Vec3) -> BuildingPieceActor
- remove_piece_by_id(instance_id: Int)
- get_piece_by_id(instance_id: Int) -> BuildingPieceActor
- get_all_pieces() -> Array<BuildingPieceActor>
- get_pieces_by_category(category: PieceCategory) -> Array<BuildingPieceActor>
- get_pieces_by_tag(tag: String) -> Array<BuildingPieceActor>
- find_pieces_in_radius(center: Vec3, radius: Float) -> Array<BuildingPieceActor>
- calculate_building_bounds() -> Box
- get_total_piece_count() -> Int
- clear_all_pieces()
- export_building_to_json() -> String
- import_building_from_json(json_data: String) -> Bool
- validate_all_pieces() -> Array<ValidationResult>
- rebuild_spatial_grid()
- get_building_statistics() -> PerformanceMetrics

### 4.3 SnapPointActor

**Purpose:** Visual indicator for snap points during building operations.

**State (8 fields):**
- snap_data: SnapPointData
- parent_piece_id: Int
- indicator_color: SnapIndicatorColor
- is_visible: Bool
- is_highlighted: Bool
- snap_radius_sphere: SphereComponent
- indicator_mesh: StaticMeshComponent
- compatibility_tags: Array<String>

**Blueprint Functions (6):**
- set_indicator_color(color: SnapIndicatorColor)
- set_visibility(visible: Bool)
- highlight(enabled: Bool)
- check_compatibility(other_snap: SnapPointData) -> Bool
- get_world_transform() -> Transform
- draw_debug_info()

### 4.4 StructuralSupportActor

**Purpose:** Manages structural integrity calculations and visualization.

**State (7 fields):**
- foundation_pieces: Array<Int>
- support_chains: Array<StructuralChain>
- unsupported_pieces: Array<Int>
- enable_physics_collapse: Bool
- max_chain_depth: Int
- support_strength_threshold: Float
- debug_visualization_enabled: Bool

**Blueprint Functions (8):**
- calculate_support_chains()
- validate_structural_integrity() -> Bool
- get_unsupported_pieces() -> Array<Int>
- mark_as_foundation(piece_id: Int)
- remove_foundation(piece_id: Int)
- collapse_unsupported_pieces()
- visualize_support_chains()
- get_support_chain_for_piece(piece_id: Int) -> StructuralChain

### 4.5 BuildingGhostActor

**Purpose:** Preview ghost for piece placement before confirmation.

**State (6 fields):**
- ghost_piece_id: String
- ghost_transform: Transform
- is_valid_placement: Bool
- validation_result: ValidationResult
- ghost_material: Material
- snap_target: SnapQueryResult

**Blueprint Functions (5):**
- update_ghost_transform(transform: Transform)
- update_validation_state(result: ValidationResult)
- set_snap_target(snap_result: SnapQueryResult)
- confirm_placement() -> BuildingPieceActor
- cancel_placement()

## 5. Subsystem Design

### 5.1 BuildManagerSubsystem (@subsystem, @tick)

**Purpose:** World-level building management with tick updates.

**State (10 fields):**
- registered_managers: Array<BuildManagerActor>
- global_piece_registry: Map<String, BuildingPieceDefinition>
- active_building_sessions: Array<BuildingState>
- performance_metrics: PerformanceMetrics
- spatial_hash_grid: Map<Vec3, Array<Int>>
- piece_pool: Array<BuildingPieceActor>
- max_pooled_pieces: Int
- enable_instancing: Bool
- enable_hlod: Bool
- tick_rate: Float

**Tick Functions:**
- update_spatial_grid()
- update_performance_metrics()
- process_pooled_pieces()
- update_instanced_meshes()

**Blueprint Functions (12):**
- register_build_manager(manager: BuildManagerActor)
- unregister_build_manager(manager: BuildManagerActor)
- load_piece_definitions_from_datatable(table_path: String)
- get_piece_definition(piece_id: String) -> BuildingPieceDefinition
- query_pieces_in_sphere(center: Vec3, radius: Float) -> Array<Int>
- allocate_piece_from_pool() -> BuildingPieceActor
- return_piece_to_pool(piece: BuildingPieceActor)
- get_global_performance_metrics() -> PerformanceMetrics
- set_performance_mode(mode: PerformanceMode)
- rebuild_all_spatial_grids()
- clear_all_buildings()
- get_total_active_pieces() -> Int

### 5.2 SnapManagerSubsystem (@subsystem, @tick)

**Purpose:** Centralized snap point detection and validation.

**State (8 fields):**
- snap_query_cache: Map<Int, Array<SnapQueryResult>>
- compatibility_rules: Map<SnapPointType, Array<SnapPointType>>
- snap_radius_multiplier: Float
- enable_snap_preview: Bool
- snap_indicators: Array<SnapPointActor>
- active_snap_queries: Int
- query_time_budget_ms: Float
- cache_expiry_time: Float

**Tick Functions:**
- update_snap_indicators()
- expire_cached_queries()
- process_pending_snap_queries()

**Blueprint Functions (10):**
- find_nearest_snap_point(piece: BuildingPieceActor, search_radius: Float) -> SnapQueryResult
- find_all_snap_points_in_radius(center: Vec3, radius: Float) -> Array<SnapQueryResult>
- validate_snap_compatibility(source: SnapPointData, target: SnapPointData) -> Bool
- calculate_snap_alignment(source: SnapPointData, target: SnapPointData) -> Transform
- register_snap_compatibility_rule(source: SnapPointType, target: SnapPointType)
- show_snap_indicators(piece: BuildingPieceActor)
- hide_all_snap_indicators()
- get_snap_query_statistics() -> PerformanceMetrics
- clear_snap_cache()
- set_snap_radius_multiplier(multiplier: Float)

### 5.3 PerformanceSubsystem (@subsystem, @tick)

**Purpose:** Performance monitoring and optimization.

**State (9 fields):**
- frame_times: Array<Float>
- snap_query_times: Array<Float>
- render_times: Array<Float>
- memory_samples: Array<Float>
- max_samples: Int
- current_performance_mode: PerformanceMode
- instanced_mesh_batches: Map<String, Array<Int>>
- hlod_clusters: Array<Array<Int>>
- optimization_enabled: Bool

**Tick Functions:**
- sample_performance_metrics()
- update_instanced_batches()
- update_hlod_clusters()
- apply_performance_optimizations()

**Blueprint Functions (8):**
- get_average_frame_time() -> Float
- get_average_snap_query_time() -> Float
- get_memory_usage_mb() -> Float
- get_total_draw_calls() -> Int
- set_performance_mode(mode: PerformanceMode)
- enable_instancing(enabled: Bool)
- enable_hlod(enabled: Bool)
- get_performance_report() -> String

## 6. Editor UI Design

### 6.1 PieceBrowserWidget (@slate)

**Purpose:** Slate widget for browsing and selecting building pieces.

**Features:**
- Category tree navigation with expand/collapse
- Thumbnail grid view with configurable size
- Search bar with real-time filtering
- Tag-based filtering with AND/OR logic
- Sort options (name, category, recent, cost)
- Drag-and-drop piece selection
- Context menu for piece operations

**Methods (8):**
- set_piece_definitions(pieces: Array<BuildingPieceDefinition>)
- set_selected_category(category: String)
- set_search_filter(search_text: String)
- set_tag_filters(tags: Array<String>)
- get_selected_piece() -> BuildingPieceDefinition
- refresh_piece_list()
- show_piece_details(piece_id: String)
- export_favorites_list() -> Array<String>



### 6.2 ToolbarWidget (@slate)

**Purpose:** Toolbar with building mode controls and actions.

**Buttons (12):**
- Place Mode
- Select Mode
- Delete Mode
- Rotate (90°, 45°, Free)
- Snap Toggle
- Grid Snap
- Undo
- Redo
- Save Building
- Load Building
- Clear All
- Settings

**Methods (6):**
- set_active_mode(mode: BuildMode)
- enable_button(button_name: String, enabled: Bool)
- set_button_tooltip(button_name: String, tooltip: String)
- register_button_callback(button_name: String, callback: Function)
- update_undo_redo_state(can_undo: Bool, can_redo: Bool)
- show_settings_dialog()

### 6.3 DetailsPanelWidget (@slate)

**Purpose:** Display and edit properties of selected pieces.

**Sections:**
- Basic Info (name, category, ID)
- Transform (location, rotation, scale)
- Snap Points (list with status)
- Variants (dropdown selection)
- Materials (override list)
- Structural (support chain, foundation status)
- Custom Properties (key-value editor)
- Statistics (cost, connections, age)

**Methods (7):**
- set_selected_pieces(pieces: Array<BuildingPieceActor>)
- update_transform_display(transform: Transform)
- update_variant_list(variants: Array<String>)
- update_material_overrides(materials: Array<String>)
- update_snap_point_list(snap_points: Array<SnapPointData>)
- update_structural_info(chain: StructuralChain)
- apply_property_changes()

### 6.4 PreviewWidget (@slate)

**Purpose:** 3D preview of selected piece with rotation controls.

**Features:**
- Rotating 3D preview mesh
- Variant cycling buttons
- Zoom controls
- Lighting presets
- Background options
- Snap point visualization toggle

**Methods (6):**
- set_preview_piece(piece_id: String)
- set_preview_variant(variant_index: Int)
- rotate_preview(delta: Vec3)
- zoom_preview(delta: Float)
- set_lighting_preset(preset: String)
- toggle_snap_point_display(enabled: Bool)

### 6.5 SnapIndicatorWidget (@slate)

**Purpose:** Visual overlay showing snap point information.

**Display Elements:**
- Snap point type icon
- Distance to snap point
- Compatibility status
- Alignment mode indicator
- Keyboard shortcut hint

**Methods (4):**
- update_snap_info(snap_result: SnapQueryResult)
- set_visibility(visible: Bool)
- set_position(screen_pos: Vec2)
- cycle_snap_targets()

### 6.6 CategoryTreeWidget (@slate)

**Purpose:** Hierarchical category tree for piece organization.

**Features:**
- Expand/collapse nodes
- Drag-and-drop reordering
- Context menu (add, rename, delete)
- Icon display
- Piece count badges
- Search highlighting

**Methods (7):**
- set_category_data(categories: Array<CategoryNode>)
- expand_category(category_name: String)
- collapse_category(category_name: String)
- select_category(category_name: String)
- add_category(parent: String, name: String)
- remove_category(category_name: String)
- get_selected_category() -> String

## 7. Snap System Algorithms

### 7.1 Snap Point Detection

**Algorithm:** Spatial hash grid with radius query

```
function find_nearest_snap_point(piece, search_radius):
    1. Get piece world position
    2. Query spatial grid for nearby pieces
    3. For each nearby piece:
        a. For each snap point on nearby piece:
            - Calculate world position of snap point
            - Calculate distance to piece
            - If distance < search_radius:
                - Check compatibility
                - If compatible, add to results
    4. Sort results by distance
    5. Return closest valid snap point
```

**Complexity:** O(k) where k = average pieces per grid cell

### 7.2 Snap Alignment Calculation

**Algorithm:** Transform composition with rotation constraints

```
function calculate_snap_alignment(source_snap, target_snap):
    1. Get target snap world transform
    2. Calculate rotation offset based on alignment mode:
        - Exact: 0° rotation
        - Rotate90: Round to nearest 90°
        - Rotate45: Round to nearest 45°
        - Free: No constraint
    3. Apply snap point local offset
    4. Compose final transform:
        - Position = target_position + offset
        - Rotation = target_rotation + rotation_offset
    5. Return aligned transform
```

### 7.3 Compatibility Validation

**Algorithm:** Type matching with tag filtering

```
function validate_snap_compatibility(source, target):
    1. Check if target type is in source compatible_types list
    2. Check if source type is in target compatible_types list
    3. If either check fails, return false
    4. Check required tags:
        - For each tag in source.required_tags:
            - If tag not in target.tags, return false
        - For each tag in target.required_tags:
            - If tag not in source.tags, return false
    5. Check if target snap point is occupied
    6. Return true if all checks pass
```

## 8. Structural System Algorithms

### 8.1 Support Chain Calculation

**Algorithm:** Breadth-first search from foundations

```
function calculate_support_chains():
    1. Identify all foundation pieces
    2. For each foundation:
        a. Initialize chain with foundation as root
        b. Create queue with foundation
        c. While queue not empty:
            - Dequeue piece
            - For each connected piece:
                - If not already in chain:
                    - Add to chain
                    - Enqueue piece
                    - Set chain depth
        d. Store completed chain
    3. Mark pieces not in any chain as unsupported
```

**Complexity:** O(n + e) where n = pieces, e = connections

### 8.2 Structural Validation

**Algorithm:** Recursive support checking

```
function validate_structural_integrity(piece):
    1. If piece is foundation, return true
    2. Get all snap connections
    3. For each connection:
        a. Get connected piece
        b. If connected piece is foundation, return true
        c. If connected piece has valid support chain, return true
    4. If no valid support found, return false
```

## 9. Performance Optimizations

### 9.1 Spatial Hashing

**Grid Cell Size:** Configurable (default 500 units)

**Benefits:**
- O(1) piece lookup by location
- O(k) snap point queries (k = pieces per cell)
- Reduced collision checks

### 9.2 Instanced Static Meshes

**Batching Strategy:**
- Group identical pieces by mesh and material
- Use UInstancedStaticMeshComponent
- Update transforms in batch

**Benefits:**
- Reduced draw calls (1 per batch vs 1 per piece)
- Lower CPU overhead
- Better GPU utilization

### 9.3 Actor Pooling

**Pool Size:** Configurable (default 100 pieces)

**Strategy:**
- Pre-allocate piece actors
- Reuse actors instead of spawning/destroying
- Reset state on return to pool

**Benefits:**
- Eliminates allocation overhead
- Reduces garbage collection pressure
- Faster placement/removal

### 9.4 LOD and HLOD

**LOD Strategy:**
- Generate simplified meshes for distant pieces
- Distance-based LOD switching
- Configurable LOD distances

**HLOD Strategy:**
- Cluster nearby pieces into single mesh
- Generate HLOD at build time or runtime
- Automatic cluster generation based on bounds

## 10. Correctness Properties

### Property 1: Snap Point Uniqueness
**Statement:** No two pieces can occupy the same snap point simultaneously.

**Verification:**
- When piece A snaps to point P on piece B, mark P as occupied
- Before snapping piece C to point P, check occupied flag
- If occupied, reject snap operation

### Property 2: Structural Transitivity
**Statement:** If piece A supports piece B, and piece B supports piece C, then A indirectly supports C.

**Verification:**
- Support chains maintain parent-child relationships
- Chain depth increases monotonically
- Removing A invalidates support for both B and C

### Property 3: Transform Consistency
**Statement:** A piece's world transform equals its local transform composed with its parent's world transform.

**Verification:**
- When snapping, calculate world transform from snap point transform
- Store world transform, not relative transform
- Verify transform matches snap point calculation

### Property 4: Replication Consistency
**Statement:** All clients see the same building state after replication.

**Verification:**
- Server is authoritative for all placement operations
- Clients replicate piece transforms and connections
- Validation occurs on server before replication

### Property 5: Undo/Redo Invertibility
**Statement:** Undo followed by Redo restores original state.

**Verification:**
- Store complete piece state in undo command
- Undo restores previous state
- Redo reapplies original command
- State hash matches before and after undo/redo cycle

## 11. Testing Strategy

### 11.1 Unit Tests
- Snap point detection with various radii
- Compatibility validation with different types
- Alignment calculation for all modes
- Structural chain calculation
- Spatial grid insertion/removal
- Transform composition

### 11.2 Integration Tests
- Piece placement and snapping
- Multi-piece structures
- Structural collapse
- Undo/redo operations
- Save/load round-trip
- Multiplayer replication

### 11.3 Performance Tests
- 10,000 piece stress test
- Snap query performance with 100 nearby pieces
- Spatial grid query performance
- Instanced mesh rendering
- Memory usage profiling

### 11.4 UI Tests
- Piece browser search and filtering
- Category tree navigation
- Toolbar button interactions
- Details panel property editing
- Preview widget rotation

## 12. Implementation Notes

### 12.1 KAIN Features Used
- Actors with @replicated state
- Subsystems with @tick
- Slate widgets with @slate
- Blueprint functions with @blueprint_callable
- Enums and structs
- DataTables with @datatable
- Array and Map collections
- Vector math (Vec2, Vec3, Transform)

### 12.2 UE5 Modules Required
- Core, CoreUObject, Engine (Runtime)
- Slate, SlateCore (Editor UI)
- UnrealEd (Editor integration)
- ProceduralMeshComponent (Dynamic meshes)
- Json, JsonUtilities (Serialization)

### 12.3 Stdlib Functions Used
- Array operations: push, pop, len, clear
- Math functions: distance, normalize, lerp
- Transform operations: compose, inverse
- String operations: format, split, join

## 13. Future Enhancements

1. **Physics Integration** - Destructible pieces with physics simulation
2. **Procedural Generation** - Template-based structure generation
3. **Material Blending** - Automatic material transitions between pieces
4. **Lighting Integration** - Automatic light placement based on piece type
5. **Audio Integration** - Ambient sounds based on building materials
6. **Weather Effects** - Dynamic weather impact on buildings
7. **Damage System** - Health and damage for pieces
8. **Upgrade System** - Piece upgrades and modifications

## 14. Conclusion

ModularBuilder provides a comprehensive modular building system with:
- 7 source files (~8,500 LOC KAIN)
- 12 enums, 25 structs, 4 DataTables
- 5 actors with full replication
- 3 subsystems with tick support
- 6 Slate widgets for editor UI
- 40+ Blueprint-callable functions
- Spatial hashing for O(1) queries
- Instanced mesh rendering
- Complete save/load system
- Multiplayer support

Expected output: 17,000-20,000 LOC C++
