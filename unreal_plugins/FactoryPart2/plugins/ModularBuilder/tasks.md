# ModularBuilder - Implementation Tasks

## Task Breakdown

### Phase 1: Project Setup
- [x] 1.1 Create requirements.md with EARS patterns
- [x] 1.2 Create design.md with architecture
- [ ] 1.3 Create tasks.md (this file)
- [ ] 1.4 Create feature_checklist.md
- [ ] 1.5 Create KAIN.toml configuration

### Phase 2: Data Structures (building_data_structures.kn)
- [ ] 2.1 Implement 12 enums
  - [ ] 2.1.1 SnapPointType, AlignmentMode, BuildMode, PieceCategory
  - [ ] 2.1.2 StructuralType, ValidationResult, PlacementRule, SnapIndicatorColor
  - [ ] 2.1.3 BuildPermission, SerializationFormat, ExportFormat, PerformanceMode
- [ ] 2.2 Implement 25 core structs
  - [ ] 2.2.1 SnapPointData, BuildingPieceDefinition, PieceInstance
  - [ ] 2.2.2 SnapQueryResult, BuildingState, StructuralChain
  - [ ] 2.2.3 CategoryNode, PlacementValidation, BuildCommand
  - [ ] 2.2.4 PerformanceMetrics and remaining structs
- [ ] 2.3 Implement 4 DataTable structs
  - [ ] 2.3.1 PieceDataTable, CategoryDataTable
  - [ ] 2.3.2 SnapCompatibilityTable, MaterialCollectionTable
- [ ] 2.4 Implement helper functions (20+)
  - [ ] 2.4.1 Struct creation functions
  - [ ] 2.4.2 Validation functions
  - [ ] 2.4.3 Query functions

### Phase 3: Snap System (building_snap_system.kn)
- [ ] 3.1 Implement snap point detection
  - [ ] 3.1.1 Spatial grid query algorithm
  - [ ] 3.1.2 Radius-based search
  - [ ] 3.1.3 Distance calculation and sorting
- [ ] 3.2 Implement snap validation
  - [ ] 3.2.1 Type compatibility checking
  - [ ] 3.2.2 Tag filtering
  - [ ] 3.2.3 Occupation checking
- [ ] 3.3 Implement snap alignment
  - [ ] 3.3.1 Transform calculation for Exact mode
  - [ ] 3.3.2 Transform calculation for Rotate90 mode
  - [ ] 3.3.3 Transform calculation for Rotate45 and Free modes
- [ ] 3.4 Implement Blueprint functions (20+)
  - [ ] 3.4.1 find_nearest_snap_point, find_all_snap_points_in_radius
  - [ ] 3.4.2 validate_snap_compatibility, calculate_snap_alignment
  - [ ] 3.4.3 Query and utility functions

### Phase 4: Actors (building_actors.kn)
- [ ] 4.1 Implement BuildingPieceActor
  - [ ] 4.1.1 State fields (15 fields)
  - [ ] 4.1.2 RPCs (8 RPCs with validation)
  - [ ] 4.1.3 Blueprint functions (12 functions)
  - [ ] 4.1.4 Snap point component management
- [ ] 4.2 Implement BuildManagerActor
  - [ ] 4.2.1 State fields (12 fields)
  - [ ] 4.2.2 RPCs (10 RPCs with validation)
  - [ ] 4.2.3 Blueprint functions (15 functions)
  - [ ] 4.2.4 Spatial grid management
- [ ] 4.3 Implement SnapPointActor
  - [ ] 4.3.1 State fields (8 fields)
  - [ ] 4.3.2 Blueprint functions (6 functions)
  - [ ] 4.3.3 Visual indicator rendering
- [ ] 4.4 Implement StructuralSupportActor
  - [ ] 4.4.1 State fields (7 fields)
  - [ ] 4.4.2 Blueprint functions (8 functions)
  - [ ] 4.4.3 Support chain calculation
- [ ] 4.5 Implement BuildingGhostActor
  - [ ] 4.5.1 State fields (6 fields)
  - [ ] 4.5.2 Blueprint functions (5 functions)
  - [ ] 4.5.3 Ghost material and validation display

### Phase 5: Editor UI (building_editor_ui.kn)
- [ ] 5.1 Implement PieceBrowserWidget (@slate)
  - [ ] 5.1.1 Category tree navigation
  - [ ] 5.1.2 Thumbnail grid view
  - [ ] 5.1.3 Search and filtering
  - [ ] 5.1.4 Methods (8 methods)
- [ ] 5.2 Implement ToolbarWidget (@slate)
  - [ ] 5.2.1 12 toolbar buttons
  - [ ] 5.2.2 Button callbacks and state management
  - [ ] 5.2.3 Methods (6 methods)
- [ ] 5.3 Implement DetailsPanelWidget (@slate)
  - [ ] 5.3.1 8 property sections
  - [ ] 5.3.2 Property editing controls
  - [ ] 5.3.3 Methods (7 methods)
- [ ] 5.4 Implement PreviewWidget (@slate)
  - [ ] 5.4.1 3D preview rendering
  - [ ] 5.4.2 Rotation and zoom controls
  - [ ] 5.4.3 Methods (6 methods)
- [ ] 5.5 Implement SnapIndicatorWidget (@slate)
  - [ ] 5.5.1 Snap info display
  - [ ] 5.5.2 Visual indicators
  - [ ] 5.5.3 Methods (4 methods)
- [ ] 5.6 Implement CategoryTreeWidget (@slate)
  - [ ] 5.6.1 Hierarchical tree rendering
  - [ ] 5.6.2 Expand/collapse functionality
  - [ ] 5.6.3 Methods (7 methods)

### Phase 6: Subsystems (building_subsystems.kn)
- [ ] 6.1 Implement BuildManagerSubsystem (@subsystem, @tick)
  - [ ] 6.1.1 State fields (10 fields)
  - [ ] 6.1.2 Tick functions (4 functions)
  - [ ] 6.1.3 Blueprint functions (12 functions)
  - [ ] 6.1.4 Spatial grid management
- [ ] 6.2 Implement SnapManagerSubsystem (@subsystem, @tick)
  - [ ] 6.2.1 State fields (8 fields)
  - [ ] 6.2.2 Tick functions (3 functions)
  - [ ] 6.2.3 Blueprint functions (10 functions)
  - [ ] 6.2.4 Snap query caching
- [ ] 6.3 Implement PerformanceSubsystem (@subsystem, @tick)
  - [ ] 6.3.1 State fields (9 fields)
  - [ ] 6.3.2 Tick functions (4 functions)
  - [ ] 6.3.3 Blueprint functions (8 functions)
  - [ ] 6.3.4 Performance monitoring and optimization

### Phase 7: Blueprint Library (building_blueprint_library.kn)
- [ ] 7.1 Implement piece spawning functions (10 functions)
- [ ] 7.2 Implement piece query functions (10 functions)
- [ ] 7.3 Implement snap utility functions (10 functions)
- [ ] 7.4 Implement structural utility functions (10 functions)

### Phase 8: Persistence (building_persistence.kn)
- [ ] 8.1 Implement save system
  - [ ] 8.1.1 JSON serialization for BuildingState
  - [ ] 8.1.2 File I/O operations
  - [ ] 8.1.3 Version management
- [ ] 8.2 Implement load system
  - [ ] 8.2.1 JSON deserialization
  - [ ] 8.2.2 Piece reconstruction
  - [ ] 8.2.3 Connection restoration
- [ ] 8.3 Implement undo/redo system
  - [ ] 8.3.1 Command history management
  - [ ] 8.3.2 Undo operation
  - [ ] 8.3.3 Redo operation
- [ ] 8.4 Implement export system
  - [ ] 8.4.1 Export to static mesh
  - [ ] 8.4.2 Export to JSON
  - [ ] 8.4.3 Export to FBX (stub)

### Phase 9: Documentation
- [ ] 9.1 Create README.md with overview and usage
- [ ] 9.2 Create BUILD_READY.md with compilation instructions
- [ ] 9.3 Create IMPLEMENTATION_COMPLETE.md with statistics

### Phase 10: Final Review
- [ ] 10.1 Verify all requirements implemented
- [ ] 10.2 Verify no TODOs or placeholders
- [ ] 10.3 Verify LOC target met (7,000-10,000)
- [ ] 10.4 Verify KAIN.toml configuration
- [ ] 10.5 Mark task 5.9 complete in parent spec

## Progress Tracking

**Total Tasks:** 95
**Completed:** 2
**Remaining:** 93
**Estimated LOC:** 8,500 KAIN → 17,000-20,000 C++
