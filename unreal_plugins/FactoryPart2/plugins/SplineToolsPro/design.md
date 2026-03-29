# SplineToolsPro - Design Document

## Architecture Overview

SplineToolsPro follows a layered architecture with clear separation between data structures, math utilities, runtime systems, editor UI, and Blueprint integration.

```
┌─────────────────────────────────────────────────────────────┐
│                     Editor Layer                             │
│  (Slate Widgets, Details Panels, Viewport, Toolbar)         │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                   Blueprint Layer                            │
│     (Function Library, Blueprint Events, Async Actions)      │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                    Runtime Layer                             │
│  (Actors, Components, Subsystem, Async Tasks)                │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                   Math Utilities Layer                       │
│  (Interpolation, Arc-Length, Curvature, Intersection)        │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                  Data Structures Layer                       │
│     (SplinePoint, SplineSegment, SplineMetadata)             │
└─────────────────────────────────────────────────────────────┘
```

## Module Structure

### 1. Data Structures (`spline_data_structures.kn`)
Core data types for spline representation.

**Structs:**
- `SplinePoint` - Control point with position, tangent in/out, rotation, scale
- `SplineSegment` - Segment between two points with cached arc-length data
- `SplineMetadata` - Per-point metadata (width, color, custom float values)
- `ArcLengthTable` - Cached arc-length lookup table for fast distance queries
- `SplineMeshParams` - Parameters for mesh deformation (scale, twist, offset)

**Enums:**
- `SplineInterpolationType` - Linear, Bezier, CatmullRom, BSpline
- `SplineCoordinateSpace` - Local, World
- `SplineTangentMode` - Auto, Manual, Smooth, Sharp

### 2. Math Utilities (`spline_math_utilities.kn`)
Pure mathematical functions for spline calculations.

**Functions:**
- `evaluate_bezier(p0, p1, p2, p3, t)` - Cubic Bezier evaluation using De Casteljau
- `evaluate_catmull_rom(p0, p1, p2, p3, t)` - Catmull-Rom spline evaluation
- `evaluate_bspline(points, knots, degree, t)` - B-spline evaluation with Cox-de Boor
- `calculate_arc_length(points, segments)` - Numerical integration for arc length
- `build_arc_length_table(points, resolution)` - Build lookup table for distance queries
- `find_parameter_at_distance(table, distance)` - Binary search in arc-length table
- `calculate_tangent(p0, p1, p2, mode)` - Tangent calculation based on mode
- `calculate_curvature(p0, p1, p2, t)` - Curvature (kappa) at parameter t
- `closest_point_on_segment(segment_start, segment_end, point)` - Closest point projection
- `intersect_splines(spline_a, spline_b, tolerance)` - Spline-spline intersection

### 3. Spline Component (`spline_component.kn`)
Component for attaching splines to actors.

**Component: SplineComponent**
- State: `points: Array<SplinePoint>`, `interpolation_type: SplineInterpolationType`, `is_closed_loop: Bool`, `default_up_vector: Vec3`
- Methods: `add_point()`, `remove_point()`, `update_point()`, `get_position_at_distance()`, `get_tangent_at_time()`
- Replication: `@replicated` for points array
- Lifecycle: `@beginplay` for arc-length table initialization, `@tick` for dynamic updates

### 4. Spline Actors (`spline_actors.kn`)
Actor implementations for spline-based objects.

**Actor: SplineActor**
- Base spline actor with SplineComponent
- Editor-visible control points
- Blueprint events for point modification

**Actor: SplineMeshActor**
- Extends SplineActor with mesh deformation
- State: `source_mesh: StaticMesh`, `deform_params: SplineMeshParams`, `instances: Array<InstancedStaticMeshComponent>`
- Methods: `deform_mesh_along_spline()`, `update_mesh_instances()`
- Async task integration for heavy deformation

**Actor: SplinePathActor**
- Spline for AI/vehicle pathfinding
- State: `path_width: Float`, `speed_limit: Float`, `one_way: Bool`
- Methods: `get_path_direction()`, `is_point_on_path()`

### 5. Spline Subsystem (`spline_subsystem.kn`)
World subsystem for spline management and caching.

**Subsystem: SplineSubsystem**
- State: `active_splines: Map<Int, SplineComponent>`, `arc_length_cache: Map<Int, ArcLengthTable>`, `dirty_splines: Set<Int>`
- Methods: `register_spline()`, `unregister_spline()`, `invalidate_cache()`, `get_cached_arc_length()`, `update_dynamic_splines()`
- Lifecycle: `@tick` for cache updates and dynamic spline processing

### 6. Mesh Deformation (`spline_mesh_deformation.kn`)
Mesh deformation algorithms and utilities.

**Functions:**
- `deform_vertex_along_spline(vertex, spline, params)` - Transform vertex to follow spline
- `calculate_cross_section_transform(spline_pos, spline_tangent, up_vector)` - Local coordinate frame
- `apply_twist(vertex, twist_angle, distance)` - Apply twist deformation
- `apply_scale(vertex, scale_curve, distance)` - Apply scale along spline
- `batch_deform_mesh(vertices, spline, params)` - Batch process for performance

**Async Task: MeshDeformationTask**
- Input: `vertices: Array<Vec3>`, `spline_data: SplineComponent`, `params: SplineMeshParams`
- Output: `deformed_vertices: Array<Vec3>`
- Callback: `on_deformation_complete()`

### 7. Blueprint Integration (`spline_blueprint_library.kn`)
Blueprint function library for spline operations.

**Blueprint Functions:**
- `GetPointAtDistance(spline, distance) -> Vec3` - World position at arc-length distance
- `GetTangentAtPoint(spline, t) -> Vec3` - Normalized tangent at parameter t
- `GetRotationAtPoint(spline, t) -> Rotator` - Rotation aligned with spline
- `GetScaleAtPoint(spline, t) -> Vec3` - Scale interpolated along spline
- `GetClosestPointOnSpline(spline, world_pos) -> (Float, Float)` - Returns (parameter, distance)
- `SampleSplineAtTime(spline, t) -> (Vec3, Rotator, Vec3)` - Returns (position, rotation, scale)
- `GetSplineLength(spline) -> Float` - Total arc length
- `GetNumberOfPoints(spline) -> Int` - Control point count
- `FindDirectionAtDistance(spline, distance) -> Vec3` - Direction vector at distance
- `GetUpVectorAtDistance(spline, distance) -> Vec3` - Up vector at distance
- `SplitSplineAtDistance(spline, distance) -> (SplineComponent, SplineComponent)` - Split into two
- `MergeSplines(spline_a, spline_b) -> SplineComponent` - Merge end-to-end

### 8. Editor UI (`spline_editor_ui.kn`)
Slate-based editor interface for spline manipulation.

**Slate Widget: SplineEditorPanel**
- Control point list with add/remove buttons
- Per-point property editing (position, tangent mode, rotation, scale)
- Interpolation type dropdown
- Closed loop checkbox
- Visualization mode selector

**Details Panel: SplineComponentDetails**
- Custom property layout for SplineComponent
- Interactive tangent handle controls
- Real-time preview updates
- Metadata editing (width, color per point)

**Viewport: SplineEditorViewport**
- 3D spline visualization with color-coded segments
- Interactive control point manipulation (translate, rotate)
- Tangent handle gizmos
- Hover tooltips with point information
- Debug visualization modes (wireframe, arrows, curvature heatmap)

**Toolbar: SplineEditorToolbar**
- Add Point button
- Delete Point button
- Interpolation mode selector
- Visualization toggle buttons
- Snap to grid toggle

### 9. Advanced Features (`spline_advanced_features.kn`)
Advanced spline operations and utilities.

**Functions:**
- `create_closed_loop(spline)` - Convert open spline to closed with C1 continuity
- `smooth_spline(spline, iterations, strength)` - Laplacian smoothing
- `subdivide_spline(spline, segments_per_point)` - Increase point density
- `simplify_spline(spline, tolerance)` - Reduce point count (Ramer-Douglas-Peucker)
- `offset_spline(spline, distance)` - Create parallel offset spline
- `extrude_spline_to_mesh(spline, cross_section)` - Generate mesh from spline + profile
- `calculate_spline_bounds(spline)` - Axis-aligned bounding box
- `raycast_against_spline(ray_origin, ray_dir, spline, tolerance)` - Ray-spline intersection

### 10. Performance Optimization (`spline_optimization.kn`)
Performance-critical code and optimizations.

**Spatial Partitioning:**
- `SplineOctree` - Octree for fast closest-point queries on large splines
- `build_octree(spline, max_depth)` - Construct octree from spline segments
- `query_octree(octree, point, radius)` - Find nearby segments

**Caching:**
- `SplineCache` - Cache for frequently accessed spline data
- `cache_arc_length_table()` - Store arc-length tables
- `cache_tangent_vectors()` - Store pre-calculated tangents
- `invalidate_cache_region(start, end)` - Partial cache invalidation

**Async Processing:**
- `AsyncSplineEvaluator` - Evaluate multiple splines in parallel
- `AsyncMeshDeformer` - Deform multiple meshes concurrently
- Thread pool management (max 4 concurrent tasks)

## Data Flow

### Spline Modification Flow
```
User Input (Editor/Blueprint)
    ↓
SplineComponent.update_point()
    ↓
Invalidate arc-length cache
    ↓
SplineSubsystem.invalidate_cache()
    ↓
Rebuild arc-length table (on next query)
    ↓
Update mesh deformations (if attached)
    ↓
AsyncMeshDeformationTask
    ↓
Update visual representation
```

### Mesh Deformation Flow
```
SplineMeshActor.deform_mesh_along_spline()
    ↓
Check vertex count (>10k?)
    ↓ Yes
AsyncMeshDeformationTask.start()
    ↓
Batch process vertices in chunks
    ↓
For each vertex:
    - Find closest spline parameter
    - Calculate local coordinate frame
    - Apply deformation (twist, scale, offset)
    - Transform to world space
    ↓
on_deformation_complete() callback
    ↓
Update InstancedStaticMeshComponent
```

### Blueprint Query Flow
```
Blueprint calls GetPointAtDistance(spline, 500.0)
    ↓
SplineSubsystem.get_cached_arc_length(spline)
    ↓
Cache hit? → Return cached table
Cache miss? → Rebuild table, cache, return
    ↓
find_parameter_at_distance(table, 500.0)
    ↓
Binary search in arc-length table
    ↓
Interpolate between table entries
    ↓
evaluate_spline_at_parameter(spline, t)
    ↓
Return world position
```

## Correctness Properties

### Property 1: Spline Continuity
**Property:** For any spline with interpolation type Bezier or CatmullRom, the curve SHALL be C1 continuous (continuous position and tangent) at all control points.

**Verification:** Unit test that samples tangent vectors at t=0.999 and t=1.001 around each control point and verifies angle difference <0.1 degrees.

### Property 2: Arc-Length Accuracy
**Property:** For any spline, the calculated arc length SHALL be within 0.1% of the true geometric length.

**Verification:** Compare numerical integration result (adaptive Simpson's rule) with cached arc-length table sum. Error must be <0.1%.

### Property 3: Closed Loop Continuity
**Property:** For any closed loop spline, the tangent at t=0.0 SHALL match the tangent at t=1.0 within 0.01 units.

**Verification:** Unit test that creates closed loop, samples tangents at endpoints, verifies vector difference magnitude <0.01.

### Property 4: Mesh Deformation Preservation
**Property:** Mesh deformation SHALL preserve the cross-sectional shape of the source mesh (no shearing or non-uniform scaling in the cross-section plane).

**Verification:** Measure distances between vertices in cross-section before and after deformation. All distances must be preserved within 1%.

### Property 5: Parameter Monotonicity
**Property:** For any spline, as the parameter t increases from 0.0 to 1.0, the arc-length distance SHALL increase monotonically.

**Verification:** Sample arc-length at t=0.0, 0.1, 0.2, ..., 1.0 and verify each value is strictly greater than the previous.

### Property 6: Tangent Normalization
**Property:** All tangent vectors returned by GetTangentAtPoint SHALL be unit vectors (length = 1.0 ± 0.001).

**Verification:** Unit test that samples tangents at 100 random parameters and verifies length is in range [0.999, 1.001].

### Property 7: Closest Point Optimality
**Property:** For any point P and spline S, GetClosestPointOnSpline SHALL return a parameter t such that the distance from P to S(t) is minimal within tolerance 0.01 units.

**Verification:** Brute-force sample spline at 1000 points, verify returned point is closer than all samples.

### Property 8: Replication Consistency
**Property:** After network replication, a spline on the client SHALL have control points within 0.1 units of the server's control points.

**Verification:** Integration test with server/client simulation, compare point positions after replication.

### Property 9: Cache Invalidation Correctness
**Property:** When a spline control point is modified, all cached data (arc-length tables, tangent vectors) SHALL be invalidated and rebuilt on next access.

**Verification:** Unit test that modifies point, queries arc-length (should trigger rebuild), verifies new table differs from old.

### Property 10: Async Task Determinism
**Property:** Mesh deformation via async task SHALL produce identical results to synchronous deformation (bit-for-bit vertex positions).

**Verification:** Deform same mesh synchronously and asynchronously, compare vertex arrays with memcmp.

## Performance Targets

| Operation | Target | Measurement Method |
|-----------|--------|-------------------|
| Evaluate position at parameter | <0.1ms | 1000 iterations, average time |
| Build arc-length table (50 points) | <5ms | Single call, wall-clock time |
| Closest point query (100 points) | <1ms | 100 queries, average time |
| Mesh deformation (10k vertices) | <16ms | Single deformation, wall-clock time |
| Async mesh deformation (50k vertices) | <50ms | End-to-end including callback |
| Spline-spline intersection | <10ms | Two 50-point splines, adaptive subdivision |
| Cache lookup (hit) | <0.01ms | 1000 lookups, average time |
| Octree construction (200 points) | <20ms | Single construction, wall-clock time |

## File Organization

```
SplineToolsPro/
├── KAIN.toml
├── requirements.md
├── design.md
├── tasks.md
├── feature_checklist.md
├── src/
│   ├── spline_data_structures.kn      (~800 LOC)
│   ├── spline_math_utilities.kn       (~1200 LOC)
│   ├── spline_component.kn            (~600 LOC)
│   ├── spline_actors.kn               (~800 LOC)
│   ├── spline_subsystem.kn            (~500 LOC)
│   ├── spline_mesh_deformation.kn     (~900 LOC)
│   ├── spline_blueprint_library.kn    (~700 LOC)
│   ├── spline_editor_ui.kn            (~1000 LOC)
│   ├── spline_advanced_features.kn    (~800 LOC)
│   └── spline_optimization.kn         (~700 LOC)
└── README.md
```

**Total Estimated LOC: 8000** (within 6000-9000 target)

## Dependencies

### UE5 Modules
- Core, CoreUObject, Engine (runtime)
- Slate, SlateCore, UnrealEd (editor)
- RenderCore (for mesh manipulation)
- PropertyEditor (for details customization)

### KAIN Stdlib Functions
- Vector math: `dot()`, `cross()`, `normalize()`, `length()`, `distance()`
- Math utilities: `lerp()`, `clamp()`, `abs()`, `sqrt()`, `pow()`
- Array operations: `push()`, `pop()`, `len()`, `map()`, `filter()`
- Actor utilities: `GetActorLocation()`, `GetActorRotation()`, `SetActorLocation()`

## Testing Strategy

### Unit Tests
- Math utilities (all interpolation methods, arc-length, curvature)
- Data structure serialization/deserialization
- Cache invalidation logic
- Tangent calculation modes

### Integration Tests
- Spline component lifecycle (add/remove points, replication)
- Mesh deformation (sync and async)
- Blueprint function library (all public functions)
- Editor UI (point manipulation, visualization modes)

### Performance Tests
- Benchmark all operations against performance targets
- Stress test with 1000-point splines
- Memory profiling for cache usage
- Thread safety verification for async tasks

### Validation Tests
- Correctness properties (all 10 properties)
- Edge cases (degenerate splines, coincident points, zero-length tangents)
- Network replication (server/client consistency)
- Cross-platform (Windows, Linux, Mac)

## Risk Mitigation

### Risk 1: Numerical Instability
**Mitigation:** Use De Casteljau algorithm for Bezier (numerically stable), clamp parameters to [0,1], handle degenerate cases explicitly.

### Risk 2: Performance Degradation
**Mitigation:** Implement spatial partitioning (octree) for large splines, use async tasks for heavy operations, profile regularly.

### Risk 3: Editor UI Responsiveness
**Mitigation:** Throttle updates to 60Hz, use dirty flags to avoid redundant calculations, defer heavy operations to next frame.

### Risk 4: Network Replication Bandwidth
**Mitigation:** Compress control point data, use delta compression for updates, replicate only changed points.

### Risk 5: Mesh Deformation Artifacts
**Mitigation:** Preserve cross-sectional distances, use smooth interpolation for scale/twist, validate deformation parameters.

## Future Enhancements (Out of Scope for V1)
- GPU-accelerated spline evaluation (compute shaders)
- Spline-based animation timeline
- Procedural spline generation (L-systems, noise-based)
- Spline collision generation
- Integration with UE5 Chaos physics
- Spline-based particle emitters
