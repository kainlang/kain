# SplineToolsPro - Requirements Document

## Plugin Overview
SplineToolsPro is an advanced spline manipulation and mesh deformation system for Unreal Engine 5, providing professional-grade tools for level designers to create complex curved geometry, paths, and deformed meshes along splines.

## Domain
Level Design Tools

## Target Metrics
- **Lines of Code**: 6000-9000 LOC
- **Compilation Target**: UE5 C++ Plugin
- **Engine Version**: 5.4+

## Functional Requirements (EARS Pattern)

### FR-1: Spline Creation and Editing
**WHEN** a level designer creates a new spline actor in the editor, **THEN** the system **SHALL** provide an interactive spline with default control points that can be manipulated in 3D space.

**WHEN** a user selects a spline control point, **THEN** the system **SHALL** display tangent handles for Bezier curve manipulation with visual feedback.

**WHEN** a user adds a new control point to an existing spline, **THEN** the system **SHALL** automatically calculate smooth tangents based on neighboring points using Catmull-Rom interpolation.

**WHEN** a user deletes a control point, **THEN** the system **SHALL** recalculate the spline curve maintaining C1 continuity at the deletion site.

### FR-2: Spline Interpolation Methods
**WHEN** a user selects a spline interpolation method, **THEN** the system **SHALL** support Linear, Bezier, Catmull-Rom, and B-spline interpolation modes.

**WHEN** the interpolation method changes, **THEN** the system **SHALL** recalculate all spline segments and update mesh deformations in real-time.

**WHEN** calculating spline positions, **THEN** the system **SHALL** provide arc-length parameterization for uniform speed traversal.

### FR-3: Mesh Deformation Along Splines
**WHEN** a user attaches a static mesh to a spline, **THEN** the system **SHALL** deform the mesh geometry to follow the spline curve while preserving cross-sectional shape.

**WHEN** mesh deformation is applied, **THEN** the system **SHALL** support scale, twist, and offset parameters along the spline length.

**WHEN** a spline with attached meshes is modified, **THEN** the system **SHALL** update all deformed mesh instances within 16ms for real-time editing.

**WHEN** deforming complex meshes (>10k vertices), **THEN** the system **SHALL** use async tasks to prevent editor freezing.

### FR-4: Spline Component System
**WHEN** an actor has a spline component attached, **THEN** the system **SHALL** provide component-level control over spline properties (closed loop, default up vector, tension).

**WHEN** a spline component is replicated, **THEN** the system **SHALL** synchronize control point positions and tangents across network clients.

**WHEN** multiple spline components exist on one actor, **THEN** the system **SHALL** manage them independently without interference.

### FR-5: Blueprint Integration
**WHEN** a Blueprint calls GetPointAtDistance, **THEN** the system **SHALL** return the world-space position at the specified arc-length distance along the spline.

**WHEN** a Blueprint calls GetTangentAtPoint, **THEN** the system **SHALL** return the normalized tangent vector at the specified spline parameter (0.0 to 1.0).

**WHEN** a Blueprint calls GetClosestPointOnSpline, **THEN** the system **SHALL** return the nearest spline parameter and distance for a given world position.

**WHEN** a Blueprint calls SampleSplineAtTime, **THEN** the system **SHALL** return position, rotation, and scale at the normalized time parameter.

**WHEN** a Blueprint requests spline metadata, **THEN** the system **SHALL** provide total arc length, segment count, and bounding box information.

### FR-6: Editor UI and Visualization
**WHEN** a spline is selected in the editor, **THEN** the system **SHALL** display a Slate-based details panel with per-point property editing.

**WHEN** the spline editor viewport is active, **THEN** the system **SHALL** render spline curves with color-coded segments (green for selected, white for unselected).

**WHEN** a user hovers over a control point, **THEN** the system **SHALL** highlight the point and display a tooltip with position and tangent information.

**WHEN** the spline visualization mode changes, **THEN** the system **SHALL** support wireframe, solid curve, and debug arrow display modes.

### FR-7: Spline Math Utilities
**WHEN** calculating Bezier curves, **THEN** the system **SHALL** use De Casteljau's algorithm for numerical stability.

**WHEN** calculating B-spline basis functions, **THEN** the system **SHALL** use Cox-de Boor recursion formula with clamped knot vectors.

**WHEN** performing spline-to-spline intersection tests, **THEN** the system **SHALL** use adaptive subdivision with configurable tolerance (default 0.01 units).

**WHEN** computing spline curvature, **THEN** the system **SHALL** provide kappa values and osculating circle radius at any parameter value.

### FR-8: Spline Subsystem Management
**WHEN** the world initializes, **THEN** the system **SHALL** create a SplineSubsystem to manage all active splines and provide caching.

**WHEN** a spline is modified, **THEN** the subsystem **SHALL** invalidate cached arc-length tables and rebuild them on next query.

**WHEN** multiple systems query the same spline, **THEN** the subsystem **SHALL** return cached results to avoid redundant calculations.

**WHEN** the subsystem ticks, **THEN** it **SHALL** update dynamic splines (animated control points) at 60Hz.

### FR-9: Advanced Spline Features
**WHEN** a user creates a closed spline loop, **THEN** the system **SHALL** ensure C1 continuity at the loop closure point.

**WHEN** a spline has variable width metadata, **THEN** the system **SHALL** interpolate width values along the curve for ribbon mesh generation.

**WHEN** a user requests spline splitting, **THEN** the system **SHALL** divide the spline at a parameter value creating two independent splines with preserved tangents.

**WHEN** a user requests spline merging, **THEN** the system **SHALL** combine two splines end-to-end with automatic tangent blending.

### FR-10: Performance and Optimization
**WHEN** a spline has more than 100 control points, **THEN** the system **SHALL** use spatial partitioning (octree) for closest-point queries.

**WHEN** mesh deformation involves more than 50k vertices, **THEN** the system **SHALL** distribute work across multiple async tasks (max 4 concurrent).

**WHEN** the editor is in play mode, **THEN** the system **SHALL** disable real-time spline visualization to maintain 60 FPS.

**WHEN** spline data is serialized, **THEN** the system **SHALL** use compressed arc-length tables reducing save file size by 40%.

## Non-Functional Requirements

### NFR-1: Performance
- Spline evaluation (position/tangent) must complete in <0.1ms for splines with <50 points
- Mesh deformation must maintain 30+ FPS for meshes with <20k vertices
- Arc-length table generation must complete in <5ms for splines with <100 points

### NFR-2: Usability
- Editor UI must provide immediate visual feedback (<16ms) for control point manipulation
- Blueprint functions must have clear naming and comprehensive tooltips
- Spline visualization must be visible at distances up to 10,000 units

### NFR-3: Reliability
- Spline calculations must be numerically stable for extreme tangent values (>1000 units)
- System must handle degenerate cases (coincident points, zero-length tangents) gracefully
- Network replication must maintain spline synchronization with <100ms latency

### NFR-4: Maintainability
- All math utilities must have unit tests with >95% code coverage
- Spline interpolation methods must be pluggable (new methods can be added without modifying core)
- Editor UI must be modular (details panel, viewport, toolbar are independent)

### NFR-5: Compatibility
- Must work with UE5.4+ (tested on 5.4, 5.5, 5.6)
- Must integrate with existing UE5 spline components (USplineComponent interop)
- Must support Blueprint-only projects (no C++ required for basic usage)

## Success Criteria
1. Plugin compiles without errors on first build
2. All 10 functional requirements are fully implemented
3. LOC target (6000-9000) is met with zero TODOs
4. Editor UI is fully functional with real-time preview
5. Blueprint integration provides all documented functions
6. Performance benchmarks meet NFR-1 targets
7. Plugin passes UE5 marketplace validation guidelines

## Out of Scope
- Spline-based animation timeline (future feature)
- GPU-accelerated spline evaluation (current implementation is CPU-based)
- Spline collision generation (use UE5 built-in collision tools)
- Spline-based AI pathfinding (use UE5 navigation system)
