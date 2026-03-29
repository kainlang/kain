# Design Doc: KAIN Stdlib Full Expansion
**Agent Task:** Populate the entire KAIN UE5 standard library with 200+ files across 24+ domains  
**Goal:** Make every common UE5 pattern a one-liner in KAIN  
**Approach:** Loose, iterative, data-driven — write KAIN, verify it compiles, move on

**Parallel Context:** This task runs alongside the plugin compilation agent. Your stdlib additions may temporarily break their plugins. Coordinate when needed. Mark tasks complete immediately.

---

## Current Status (Feb 23, 2026)

### What's Live in `Kain/stdlib/ue5/` Right Now

| File | Lines | Status |
|---|---|---|
| `shaders.kn` | 2,763 | ✅ Complete — 100+ functions: PBR, noise, color grading, UV, volumetric, SSS, post-processing, ray marching, SDF, procedural gen |
| `actor.kn` | 500 | ✅ Complete — 30+ functions: lifecycle, transform, attachment, velocity, component access |
| `gameplay.kn` | 430 | ✅ Complete — 20+ functions: damage, health, XP, inventory, cooldowns, buffs, loot, quests |
| `utilities.kn` | 156 | ✅ Complete — 20+ functions: math helpers, remap, interpolation, random, string formatting |
| `world.kn` | ~80 | ✅ Complete — 20+ functions: world queries, spawning, traces, debug drawing |
| `skeletal_mesh.kn` | 79 | ✅ Complete — 20+ functions: montages, bone manipulation, sockets, morph targets |
| `materials.kn` | 53 | ✅ Complete — 15+ functions: material parameter control, dynamic materials |
| `particles.kn` | 57 | ✅ Complete — 15+ functions: Niagara variable binding, system control |
| `components.kn` | 78 | ✅ Complete — 10+ structs: Health, Inventory, Movement, Combat, Interaction |
| `patterns.kn` | 106 | ✅ Complete — 12+ types: LootRarity, BuffType, DamageType, WeaponStats, etc. |
| `math.kn` | 73 | ✅ Complete — 11+ functions: vector math, rotation, interpolation, type aliases |
| `common.kn` | 17 | ✅ Complete — core engine bindings |

**Total: ~4,400 lines, 200+ functions across 12 files. Backend loading is live.**

### Backend Wiring — DONE
All three backend fixes from Phase 0 are complete:
1. ✅ `kain-core/src/stdlib.rs` — `load_stdlib()` reads from disk with `find_stdlib_search_roots()` + `load_kn_files_from_dir()`
2. ✅ `cli/src/packager/ue5_pipeline.rs` — Stdlib auto-discovery restored (env var → exe walk → CWD walk)
3. ✅ `stdlib/ue5/gameplay.kn` — `var` → `let`, `&&` → `and` syntax bugs fixed

### Validation
- ✅ All 12 stdlib files parse without errors
- ✅ Factory/Example plugin uses functions from all 12 categories
- ✅ Compression ratio of 1:20 documented in `Factory/_Docs/COMPRESSION_RATIO_ANALYSIS.md`
- ✅ Validation report at `Factory/Example/_Docs/STDLIB_VALIDATION_REPORT.md`

---

## Context

The KAIN stdlib is a set of `.kn` files that get prepended to user source before compilation. Functions defined here are globally available to every plugin with zero imports.

- **Compiler stdlib:** `m:\Code\Kain\stdlib\ue5\` — loaded by the backend during `kain build`
- **Factory testing ground:** `m:\Code\Factory\Stdlib\` — organized working copy for development/testing
- Sync verified files from `Factory/Stdlib/` to `Kain/stdlib/ue5/` for the compiler to pick them up

Two function patterns:
- **`@extern fn`** — declares a function that exists in UE5 C++. No body. Tells the type-checker it's valid.
- **`@blueprint fn`** — pure KAIN function with a body. Compiles to `UFUNCTION(BlueprintCallable)` C++.

---

## Phase 5 — Advanced Shaders & GPU Math (Next Priority)

The shader stdlib is the highest-leverage area (1:30 compression). The base `shaders.kn` is done. These are the next-level additions.

### `shaders/advanced/` — Complex Math & Signal Processing

| File | What It Covers |
|---|---|
| `complex_math.kn` | Complex number arithmetic, FFT helpers, Fourier series evaluation, polynomial roots, Chebyshev approximations |
| `quaternion_math.kn` | Quaternion slerp, squad, double cover, log/exp, quaternion fields for rotation interpolation |
| `matrix_ops.kn` | 2x2/3x3/4x4 determinant, inverse, transpose, eigenvalue approximation, SVD decomposition helpers |
| `numerical.kn` | Newton-Raphson, bisection, Runge-Kutta ODE integration, gradient descent step, finite differences |
| `splines.kn` | Bezier evaluation, Catmull-Rom, B-spline, NURBS weight evaluation, arc-length parameterization |
| `sdf_advanced.kn` | sdf_torus, sdf_capsule, sdf_cylinder, sdf_cone, sdf_helix, sdf_mandelbulb, sdf_menger_sponge, domain_repetition, domain_twist, domain_bend |
| `raymarching.kn` | raymarch_scene, soft_shadow, ambient_occlusion_march, normal_from_sdf, cone_step_mapping |
| `geometry_shaders.kn` | barycentric_coords, triangle_area, point_in_triangle, mesh_normal_smooth, tangent_frame |

### `shaders/dcc/` — DCC-Style Shader Nodes

Houdini, Substance, and Cinema 4D artists expect these. Make KAIN shaders feel like a node graph you can write as code.

| File | What It Covers |
|---|---|
| `substance_nodes.kn` | histogram_scan, histogram_range, levels, curves, warp, slope_blur, non_uniform_blur, bevel, ambient_occlusion_bake |
| `houdini_vex.kn` | fit, efit, chramp, smooth, bias, gain, xyzdist, primuv, volumesample, pointcloud_open style patterns |
| `color_science.kn` | oklab_to_rgb, rgb_to_oklab, aces_transform, rec709_to_rec2020, hsl_to_rgb, hsv_to_rgb, lab_to_xyz, color_temperature_to_rgb, planckian_locus |
| `texture_ops.kn` | normal_blend_reoriented, normal_from_height, height_from_normal, cavity_from_normal, curvature_from_normal, ao_from_normal, thickness_from_normal |
| `procedural_patterns.kn` | brick, hexagon, truchet, voronoi_cells, reaction_diffusion_step, turing_pattern, lissajous, rose_curve, superformula |
| `displacement.kn` | parallax_occlusion, steep_parallax, relief_mapping, tessellation_displacement, micro_displacement |
| `atmosphere.kn` | rayleigh_scattering, mie_scattering, single_scattering, aerial_perspective, volumetric_fog_density, cloud_density |

### `shaders/brushes/` — Brush & Sculpt Math

For plugins like UESculpt, Materialize, and any tool that does texture/mesh painting.

| File | What It Covers |
|---|---|
| `brush_shapes.kn` | circle_brush, square_brush, diamond_brush, star_brush, custom_alpha_brush, brush_falloff_curves (linear, smooth, sphere, root, sharp) |
| `brush_ops.kn` | brush_add, brush_subtract, brush_smooth, brush_flatten, brush_pinch, brush_inflate, brush_crease, brush_relax |
| `brush_masking.kn` | cavity_mask, curvature_mask, normal_mask, height_mask, random_mask, texture_mask, vertex_color_mask |
| `stroke_interpolation.kn` | stroke_spacing, stroke_jitter, stroke_rotation, stroke_size_curve, stroke_opacity_curve, lazy_mouse |
| `projection.kn` | planar_project, triplanar_project, cylindrical_project, spherical_project, camera_project, uv_project |

---

## Phase 6 — Complex Editor Tooling

The editor stdlib is where KAIN becomes a DCC tool authoring language.

### `editor/nodes/` — Node Graph System

| File | What It Covers |
|---|---|
| `node_graph.kn` | NodeGraph struct, add_node, remove_node, connect_pins, disconnect_pins, evaluate_graph, serialize_graph |
| `node_pins.kn` | PinType enum (Float, Vec2, Vec3, Vec4, Bool, Int, String, Texture, Material, Mesh, Any), pin_compatible, auto_convert |
| `node_layout.kn` | auto_layout, force_directed_layout, hierarchical_layout, align_nodes, distribute_nodes |
| `node_search.kn` | node_search_panel, fuzzy_search_nodes, category_browser, recent_nodes, favorite_nodes |
| `node_comments.kn` | comment_box, reroute_node, group_nodes, collapse_group, expand_group |
| `node_execution.kn` | topological_sort, cycle_detection, lazy_evaluation, dirty_propagation, cache_node_output |
| `material_nodes.kn` | Material-specific node types — texture_sample, constant, lerp, multiply, add, fresnel, normal_map, vertex_color |
| `blueprint_nodes.kn` | UK2Node patterns — custom_k2_node, create_pin, allocate_default_pins, expand_node, get_tooltip |

### `editor/tools/` — Editor Tool Patterns

| File | What It Covers |
|---|---|
| `interactive_tool.kn` | UInteractiveTool base patterns — begin_tool, end_tool, on_tick, on_click, on_drag, on_hover |
| `gizmos.kn` | translation_gizmo, rotation_gizmo, scale_gizmo, custom_gizmo, gizmo_hit_test, gizmo_drag |
| `selection.kn` | selection_set, add_to_selection, remove_from_selection, select_all, invert_selection, selection_bounds |
| `undo_redo.kn` | transaction_begin, transaction_end, add_to_transaction, undo, redo, mark_dirty |
| `mode_toolkit.kn` | editor_mode_register, mode_toolbar, mode_palette, mode_settings, mode_enter, mode_exit |
| `paint_tools.kn` | mesh_paint_begin, vertex_color_paint, texture_paint, weight_paint, paint_bucket_fill |
| `placement_tools.kn` | snap_to_grid, snap_to_surface, align_to_normal, random_rotation_on_place, scatter_placement |
| `measurement.kn` | measure_distance, measure_angle, measure_area, measure_volume, ruler_tool |

### `editor/panels/` — Advanced Panel Patterns

| File | What It Covers |
|---|---|
| `curve_editor.kn` | CurveEditor widget, add_key, remove_key, set_tangent, evaluate_curve, import_curve, export_curve |
| `gradient_editor.kn` | GradientEditor widget, add_stop, remove_stop, set_stop_color, evaluate_gradient, preset_gradients |
| `color_picker.kn` | HSV picker, RGB sliders, hex input, eyedropper, palette, recent colors, color harmony |
| `timeline.kn` | Timeline widget, add_track, add_keyframe, scrub, play, loop, set_range, snap_to_frame |
| `spreadsheet.kn` | Spreadsheet view, column_definition, row_filter, row_sort, multi_select, inline_edit |
| `tree_view.kn` | Hierarchical tree, expand_collapse, drag_reorder, multi_select, search_filter, context_menu |
| `property_matrix.kn` | Multi-object property editing, mixed value display, bulk edit, per-object override |
| `data_table_editor.kn` | DataTable row editor, add_row, delete_row, duplicate_row, import_csv, export_csv |

### `editor/viewport_tools/` — Viewport Extensions

| File | What It Covers |
|---|---|
| `viewport_overlay.kn` | 2D overlay drawing, text overlay, icon overlay, progress overlay, debug overlay |
| `viewport_manipulation.kn` | Custom drag behavior, axis constraints, pivot override, coordinate space toggle |
| `preview_scene.kn` | Preview scene setup, lighting rig, turntable, background, floor grid, reference mesh |
| `hit_proxy.kn` | Custom hit proxies for viewport clicking, hit_proxy_register, hit_proxy_click, hit_proxy_hover |
| `scene_outliner.kn` | Custom outliner columns, outliner filter, outliner group, outliner drag_drop |
| `level_editor_ext.kn` | Level editor menu extension, actor context menu, folder operations, layer management |

### `editor/asset_pipeline/` — Asset Creation & Import

| File | What It Covers |
|---|---|
| `asset_factory.kn` | Custom asset factory patterns — create_asset, reimport_asset, can_reimport, get_supported_class |
| `import_settings.kn` | Import option structs, import_dialog, apply_import_settings, import_from_file |
| `thumbnail.kn` | Custom thumbnail renderer — render_thumbnail, thumbnail_scene, thumbnail_camera |
| `asset_actions.kn` | AssetTypeActions — get_actions, execute_action, can_filter, get_filter_name |
| `content_browser.kn` | Content browser extension — add_filter, add_column, add_drag_drop_handler |
| `cook_rules.kn` | Asset cook rules, platform_specific_cook, cook_dependency, neverCook, alwaysCook |

---

## Phase 7 — Rust StdLib Expansion (Backend Work)

The Rust-side `StdLib` struct in `kain-core/src/stdlib.rs` needs to grow in parallel. Every function declared as `@extern` in `.kn` files needs a matching `lib.add_fn()` entry or the type-checker rejects calls to it.

### Missing Categories to Register

| Category | Functions to Register |
|---|---|
| **Extended Math** | `atan2`, `asin`, `acos`, `exp`, `log`, `log2`, `frac`, `sign`, `step`, `saturate`, `degrees`, `radians` |
| **Vector Extended** | `vec2i`, `vec3i`, `vec4i`, `uvec2`, `uvec3`, `uvec4`, `reflect`, `refract`, `faceforward`, `project`, `reject` |
| **Matrix** | `mat2`, `mat3`, `mat4`, `mat_mul`, `mat_transpose`, `mat_inverse`, `mat_determinant` |
| **Texture Sampling** | `sample`, `sample_lod`, `sample_grad`, `sample_bias`, `sample_level`, `gather` |
| **Atomic** | `atomic_add`, `atomic_min`, `atomic_max`, `atomic_and`, `atomic_or`, `atomic_exchange` |
| **Wave Intrinsics** | `wave_active_sum`, `wave_active_max`, `wave_prefix_sum`, `wave_read_lane_at`, `wave_get_lane_index` |
| **UE5 Actor** | `get_actor_location`, `set_actor_location`, `get_actor_rotation`, `destroy_actor`, `is_valid` |
| **UE5 World** | `get_world_delta_seconds`, `get_world_time_seconds`, `is_server`, `is_client`, `spawn_actor`, `line_trace` |
| **UE5 Materials** | `set_scalar_parameter`, `set_vector_parameter`, `set_texture_parameter`, `create_dynamic_material` |
| **UE5 Particles** | `spawn_particle_at_location`, `spawn_particle_attached`, `set_particle_float`, `set_particle_vector` |
| **UE5 Audio** | `play_sound_at_location`, `play_sound_attached`, `play_music`, `set_volume` |
| **UE5 Animation** | `play_montage`, `stop_montage`, `set_morph_target`, `get_bone_location` |
| **UE5 Collision** | `sphere_trace`, `box_trace`, `capsule_trace`, `multi_line_trace` |
| **UE5 Rendering** | `spawn_decal`, `add_instance`, `set_lod_distance`, `draw_debug_line`, `draw_debug_sphere` |

Pattern for adding:
```rust
lib.add_fn("atan2", &[("y", "Float"), ("x", "Float")], "Float", "Arc tangent of y/x");
lib.add_fn("reflect", &[("v", "Vec3"), ("n", "Vec3")], "Vec3", "Reflect vector around normal");
lib.add_fn("sample", &[("tex", "Texture2D"), ("uv", "Vec2")], "Vec4", "Sample texture");
```

---

## Phase 8 — Codegen Additions

Some stdlib patterns require new codegen support.

### `@shader_fn` — Shader-Inlined Functions
Functions marked `@shader_fn` should be inlined into USF shader bodies as HLSL function definitions rather than compiled to C++ Blueprint functions.

**What needs to happen:**
1. `parser.rs` — recognize `@shader_fn` as valid attribute on `fn` declarations
2. `ast.rs` — store the attribute on `FnDef`
3. `codegen_usf.rs` — when generating a shader body, emit `@shader_fn` functions as HLSL function definitions at the top of the `.usf` file
4. `codegen_ue5.rs` — skip `@shader_fn` functions entirely (shader-only)

### Node Graph Codegen
- `@node_graph` annotation on a struct → generates `UEdGraph` subclass
- `@graph_node` annotation on a struct → generates `UEdGraphNode` subclass
- `@k2_node` annotation → generates `UK2Node` subclass with Blueprint integration
- Pin definitions in the struct → `CreatePin()` calls in `AllocateDefaultPins()`

### Interactive Tool Codegen
- `@interactive_tool` annotation → generates `UInteractiveTool` subclass
- `@tool_builder` annotation → generates `UInteractiveToolBuilder`
- `on_begin`, `on_end`, `on_tick`, `on_click` handlers → override methods

### Asset Factory Codegen
- `@asset_factory(type: "MyAsset")` → generates `UFactory` subclass
- `@asset_actions(type: "MyAsset")` → generates `IAssetTypeActions` implementation

---

## KAIN Syntax Rules (Critical)

- No `var` keyword — use `let`
- No `&&` / `||` — use `and` / `or`
- No `for..in` — use `while` loops
- No `as Type` casts in shaders — now supported via USF cast codegen (use freely)
- No struct literal syntax `Foo { x: 1 }` — use field assignment
- No bitwise operators
- All enums need `_MAX` variant
- `@extern` functions have no body
- `@blueprint` functions have a full body
- Array literals `[a, b, c]` in shaders — now supported via USF array literal codegen (use freely)

---

## Folder Structure

```
m:\Code\Factory\Stdlib\          ← testing ground (not loaded by compiler)
m:\Code\Kain\stdlib\ue5\         ← compiler loads from here
├── common.kn          ✅
├── math.kn            ✅
├── actor.kn           ✅
├── world.kn           ✅
├── components.kn      ✅
├── gameplay.kn        ✅
├── materials.kn       ✅
├── particles.kn       ✅
├── patterns.kn        ✅
├── shaders.kn         ✅ (2,763 lines, 100+ functions)
├── skeletal_mesh.kn   ✅
└── utilities.kn       ✅

Factory/Stdlib/ domain subfolders (for development):
├── core/               shaders/advanced/    editor/nodes/
├── gameplay/           shaders/dcc/         editor/tools/
├── combat/             shaders/brushes/     editor/panels/
├── ai/                 materials/           editor/viewport_tools/
├── physics/            particles/           editor/asset_pipeline/
├── audio/              animation/           debug/
├── ui/                 world/               data/
└── networking/         ...
```

---

## Verification Strategy

1. Write files in batches (10-20 at a time) in `Factory/Stdlib/`
2. Sync to `Kain/stdlib/ue5/` and run `kain build --ue5` on Materialize or Example plugin
3. If it parses, the files are valid
4. If you see duplicate definition errors, resolve naming conflicts
5. The plugin compilation agent is your integration tester — prioritize functions they need

---

## Success Criteria

- All `.kn` files parse without errors
- No duplicate type/function definitions
- Rust `StdLib::new()` registers all `@extern` functions
- `kain build --ue5` on Materialize with full stdlib = clean build
- Shader stdlib functions work in USF shaders
- Node graph and editor tool stdlib generate valid C++
- Any new plugin can use stdlib functions without local definitions
- The stdlib covers 500+ common UE5 patterns across all domains

---

## Reference Materials

- `_Docs/STDLIB_BACKEND_RUNDOWN.md` — Backend wiring details
- `_Docs/STDLIB_INSPECTION.md` — Current state analysis
- `_Docs/MATERIALIZE_BUILD_REPORT.md` — KAIN syntax issues to avoid
- `_Docs/COMPRESSION_RATIO_ANALYSIS.md` — 1:20 compression methodology
- `.kiro/specs/kain-stdlib-backend/` — Backend implementation spec (all tasks complete)
- `.kiro/specs/kain-stdlib-enhancement/` — Full expansion spec (all phases complete)
