# ModularBuilder - Requirements Document

## 1. Overview

ModularBuilder is a modular building system for Unreal Engine 5 that enables designers to create complex structures using snap-together building pieces with variants, categories, and custom building logic.

**Category:** Level Design Tools  
**Target LOC:** 7,000-10,000 KAIN  
**Expected Output:** 14,000-20,000 C++

## 2. Functional Requirements (EARS Pattern)

### 2.1 Core Building System

**REQ-2.1.1:** WHEN a designer places a building piece in the editor, THEN the system SHALL detect nearby snap points within a configurable radius and display visual indicators for valid attachment locations.

**REQ-2.1.2:** WHEN a building piece is snapped to another piece, THEN the system SHALL automatically align the piece's transform to match the target snap point's position and rotation with configurable offset support.

**REQ-2.1.3:** WHEN multiple snap points are within range, THEN the system SHALL prioritize the closest snap point and allow cycling through alternatives using keyboard shortcuts.

**REQ-2.1.4:** WHEN a building piece is selected, THEN the system SHALL display all available snap points with color-coded indicators based on compatibility (green=compatible, red=incompatible, yellow=occupied).

**REQ-2.1.5:** WHEN a building piece is removed, THEN the system SHALL optionally remove all connected pieces based on structural dependency rules.

### 2.2 Piece Management

**REQ-2.2.1:** WHEN the plugin initializes, THEN the system SHALL load all building piece definitions from DataTables including mesh references, snap point configurations, and metadata.

**REQ-2.2.2:** WHEN a building piece is spawned, THEN the system SHALL support multiple mesh variants per piece type with random or manual selection.

**REQ-2.2.3:** WHEN pieces are organized, THEN the system SHALL support hierarchical categories (e.g., Walls/Exterior/Stone, Floors/Wood, Roofs/Tile) with up to 5 levels of nesting.

**REQ-2.2.4:** WHEN a piece is placed, THEN the system SHALL validate placement rules including ground clearance, overlap detection, and structural support requirements.

**REQ-2.2.5:** WHEN pieces are filtered, THEN the system SHALL support tag-based filtering (e.g., "medieval", "industrial", "modular") with AND/OR logic.

### 2.3 Editor UI

**REQ-2.3.1:** WHEN the editor mode is activated, THEN the system SHALL display a Slate-based piece browser with thumbnail previews, search functionality, and category tree navigation.

**REQ-2.3.2:** WHEN a piece is selected in the browser, THEN the system SHALL display a 3D preview with rotation controls and variant selection dropdown.

**REQ-2.3.3:** WHEN building, THEN the system SHALL provide a toolbar with buttons for: Place Mode, Select Mode, Delete Mode, Rotate, Snap Toggle, Grid Snap, and Undo/Redo.

**REQ-2.3.4:** WHEN a piece is hovered in the viewport, THEN the system SHALL display a details panel showing piece name, category, variant count, snap point count, and custom properties.

**REQ-2.3.5:** WHEN multiple pieces are selected, THEN the system SHALL support bulk operations including rotation, deletion, material override, and group creation.

### 2.4 Snap Point System

**REQ-2.4.1:** WHEN snap points are defined, THEN the system SHALL support typed snap points (e.g., Wall-to-Wall, Floor-to-Ceiling, Corner, Edge) with compatibility rules.

**REQ-2.4.2:** WHEN snap points are configured, THEN the system SHALL allow per-point settings including snap radius, rotation constraint, offset, and tag filters.

**REQ-2.4.3:** WHEN snapping occurs, THEN the system SHALL support alignment modes: Exact (0° rotation), 90° increments, 45° increments, and Free rotation.

**REQ-2.4.4:** WHEN snap points overlap, THEN the system SHALL prevent duplicate connections and mark occupied snap points as unavailable.

**REQ-2.4.5:** WHEN snap validation occurs, THEN the system SHALL check compatibility based on snap point types, tags, and custom validation functions.

### 2.5 Blueprint Integration

**REQ-2.5.1:** WHEN Blueprint logic is needed, THEN the system SHALL provide 30+ Blueprint-callable functions for piece spawning, snapping, querying, and manipulation.

**REQ-2.5.2:** WHEN custom building logic is required, THEN the system SHALL support Blueprint events for: OnPiecePlaced, OnPieceRemoved, OnPieceSnapped, OnValidationFailed.

**REQ-2.5.3:** WHEN runtime building is needed, THEN the system SHALL support player-controlled building with input handling, preview ghosts, and placement validation.

**REQ-2.5.4:** WHEN building state is queried, THEN the system SHALL provide functions to get connected pieces, find pieces by tag, calculate structure bounds, and count pieces by category.

**REQ-2.5.5:** WHEN procedural generation is needed, THEN the system SHALL provide functions for automatic structure generation based on templates and rules.

### 2.6 Asset Management

**REQ-2.6.1:** WHEN assets are organized, THEN the system SHALL support asset collections with metadata including author, version, thumbnail, description, and tags.

**REQ-2.6.2:** WHEN pieces are imported, THEN the system SHALL auto-detect snap points from socket names (e.g., "Snap_Wall_Top", "Snap_Floor_Bottom") with configurable naming conventions.

**REQ-2.6.3:** WHEN materials are managed, THEN the system SHALL support material overrides per piece instance with material collection support.

**REQ-2.6.4:** WHEN assets are validated, THEN the system SHALL check for missing meshes, invalid snap points, circular dependencies, and naming conflicts.

**REQ-2.6.5:** WHEN assets are exported, THEN the system SHALL support exporting building definitions to JSON for sharing and version control.

### 2.7 Structural System

**REQ-2.7.1:** WHEN structural integrity is enabled, THEN the system SHALL calculate support chains from foundation pieces to dependent pieces.

**REQ-2.7.2:** WHEN a support piece is removed, THEN the system SHALL optionally collapse unsupported pieces with physics simulation or instant removal.

**REQ-2.7.3:** WHEN pieces are placed, THEN the system SHALL validate structural rules including maximum height, cantilever limits, and load-bearing requirements.

**REQ-2.7.4:** WHEN structural analysis is performed, THEN the system SHALL visualize support chains with color-coded indicators (green=supported, yellow=weak, red=unsupported).

**REQ-2.7.5:** WHEN foundation pieces are defined, THEN the system SHALL mark pieces as foundations that provide infinite support and require ground contact.

### 2.8 Performance & Optimization

**REQ-2.8.1:** WHEN many pieces are placed, THEN the system SHALL use spatial hashing for O(1) snap point queries within configurable grid cell size.

**REQ-2.8.2:** WHEN rendering occurs, THEN the system SHALL support instanced static mesh rendering for identical pieces to reduce draw calls.

**REQ-2.8.3:** WHEN pieces are numerous, THEN the system SHALL support hierarchical LOD (HLOD) generation for distant building clusters.

**REQ-2.8.4:** WHEN memory is managed, THEN the system SHALL pool piece actors to avoid allocation overhead during rapid placement/removal.

**REQ-2.8.5:** WHEN performance is monitored, THEN the system SHALL track metrics including piece count, snap query time, render time, and memory usage.

### 2.9 Persistence & Serialization

**REQ-2.9.1:** WHEN buildings are saved, THEN the system SHALL serialize all piece data including transforms, variants, materials, and connections to JSON or binary format.

**REQ-2.9.2:** WHEN buildings are loaded, THEN the system SHALL restore all pieces with correct transforms, snap connections, and custom properties.

**REQ-2.9.3:** WHEN save data is versioned, THEN the system SHALL support migration from older save formats with backward compatibility.

**REQ-2.9.4:** WHEN buildings are exported, THEN the system SHALL support exporting to static mesh for final level geometry with merged meshes and optimized materials.

**REQ-2.9.5:** WHEN undo/redo is used, THEN the system SHALL maintain a command history with configurable max depth (default 50 actions).

### 2.10 Multiplayer Support

**REQ-2.10.1:** WHEN multiplayer is enabled, THEN the system SHALL replicate piece placement, removal, and modification across all clients.

**REQ-2.10.2:** WHEN multiple players build, THEN the system SHALL prevent conflicting placements with server-authoritative validation.

**REQ-2.10.3:** WHEN network bandwidth is limited, THEN the system SHALL batch piece updates and use delta compression for efficient replication.

**REQ-2.10.4:** WHEN ownership is tracked, THEN the system SHALL record which player placed each piece for permissions and attribution.

**REQ-2.10.5:** WHEN building permissions are enforced, THEN the system SHALL support per-player or per-team building zones with configurable rules.

## 3. Non-Functional Requirements

### 3.1 Performance

**NFR-3.1.1:** The system SHALL support 10,000+ placed pieces with <16ms frame time on mid-range hardware.

**NFR-3.1.2:** Snap point queries SHALL complete in <1ms for typical building scenarios (100 nearby pieces).

**NFR-3.1.3:** Piece placement SHALL have <50ms latency from input to visual feedback.

**NFR-3.1.4:** Editor UI SHALL remain responsive with <100ms interaction latency.

### 3.2 Usability

**NFR-3.2.1:** The piece browser SHALL support keyboard navigation and search with <500ms response time.

**NFR-3.2.2:** Snap indicators SHALL be clearly visible with configurable colors and sizes.

**NFR-3.2.3:** Error messages SHALL be descriptive and provide actionable suggestions.

**NFR-3.2.4:** The system SHALL provide comprehensive tooltips for all UI elements.

### 3.3 Compatibility

**NFR-3.3.1:** The plugin SHALL be compatible with Unreal Engine 5.4+.

**NFR-3.3.2:** The system SHALL work with standard UE5 input systems (Enhanced Input, legacy input).

**NFR-3.3.3:** The plugin SHALL integrate with UE5 editor modes and toolbars.

**NFR-3.3.4:** The system SHALL support both editor and runtime building workflows.

### 3.4 Maintainability

**NFR-3.4.1:** All configuration SHALL be data-driven using DataTables and JSON.

**NFR-3.4.2:** The codebase SHALL follow UE5 coding standards and KAIN best practices.

**NFR-3.4.3:** The system SHALL provide debug visualization for all major systems.

**NFR-3.4.4:** The plugin SHALL include comprehensive Blueprint documentation.

## 4. Constraints

**CON-4.1:** The plugin SHALL be implemented entirely in KAIN targeting UE5 C++.

**CON-4.2:** The system SHALL NOT use third-party libraries beyond UE5 engine modules.

**CON-4.3:** The implementation SHALL contain zero TODOs, shortcuts, or simplifications.

**CON-4.4:** The codebase SHALL target 7,000-10,000 LOC of KAIN code.

## 5. Acceptance Criteria

**AC-5.1:** All 50 functional requirements are implemented and testable.

**AC-5.2:** The plugin compiles successfully with `kain build --ue5`.

**AC-5.3:** The editor UI is fully functional with piece browser, toolbar, and details panel.

**AC-5.4:** Snap point system works with all alignment modes and validation rules.

**AC-5.5:** Blueprint integration provides 30+ callable functions with full documentation.

**AC-5.6:** Performance meets all NFR targets with 10,000+ pieces.

**AC-5.7:** Multiplayer replication works correctly with server validation.

**AC-5.8:** Save/load system preserves all building data accurately.

**AC-5.9:** The codebase contains no TODOs, placeholders, or incomplete implementations.

**AC-5.10:** Generated C++ code compiles in UE5 without errors or warnings.
