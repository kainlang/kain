# Design Doc: Get 5 Complex Plugins to Full Compilation
**Agent Task:** Fix codegen issues and KAIN source until all 5 target plugins pass `FULLBUILD.bat`  
**Goal:** Clean `kain build --ue5` + clean UE5 `RunUAT BuildPlugin` for each plugin  
**Approach:** Iterative — build, read errors, fix, rebuild. No compromises on plugin scope.

**Parallel Context:** This task runs alongside the stdlib expansion agent. Unexplained build errors may be stdlib-related. Coordinate when needed. Mark tasks complete immediately upon finishing them.

---

## Current Status (Feb 23, 2026)

| Plugin | KAIN Build | UE5 Build | Notes |
|---|---|---|---|
| **Materialize** | ✅ Clean | ✅ Clean | Reference implementation — run as regression after every backend change |
| **VoxelForgePro** | ✅ Clean | ✅ Clean | Completed Phase 2 |
| **Cinema4DMograph** | ✅ Clean | ⚠️ File lock | KAIN compiles; UE5 build blocked by file lock during agent run |
| **TemporalBlueprint** | ✅ Clean | ⚠️ In progress | UE5 build hitting name collisions (ETransitionType, InventorySlot, function names) |
| **MetaFitter** | ✅ Clean | ✅ Clean | Completed Phase 5 |

**Remaining work:** Resolve TemporalBlueprint UE5 name collisions, re-verify Cinema4DMograph UE5 build, full regression suite.

---

## Target Plugins

| Plugin | .kn Files | Domain | Complexity |
|---|---|---|---|
| **Cinema4DMograph** | 6 (actors, components, editor, modifiers, types, utilities) | MoGraph-style procedural animation | Editor UI + compute shaders + procedural mesh |
| **Materialize** | 14 | PBR material authoring suite | ✅ Already compiles — reference implementation |
| **MetaFitter** | 15 (actors, algorithms, batch, components, details, editor_module, editor_toolbar, editor_ui, editor_viewport, materials, metahuman_integration, physics, presets, subsystems, types) | MetaHuman fitting/customization | Full editor suite + physics + MetaHuman API |
| **TemporalBlueprint** | 9 (actors, algorithms, components, details, editor_toolbar, editor_ui, editor, subsystems, types) | Time-travel debugging for Blueprints | Editor-heavy + temporal state management |
| **VoxelForgePro** | 1 (voxelforge.kn at root) | Voxel terrain engine | Compute shaders + meshing + physics + LOD |

---

## The Build Pipeline (What FULLBUILD.bat Does)

Each plugin's `FULLBUILD.bat` runs a 3-step pipeline:

1. **`cargo install --path crates/cli --force`** — Rebuilds the KAIN compiler from source
2. **`kain build --ue5`** — Compiles `.kn` → C++, shaders, blueprints, materials, .uplugin, Build.cs
3. **`RunUAT BuildPlugin`** — UE5 compiles the generated C++ with Unreal Build Tool

Step 2 failures are KAIN-side (syntax errors, type errors, Oracle warnings, codegen bugs).  
Step 3 failures are C++-side (invalid generated code, missing includes, UHT errors, linker errors).

Both must pass for the plugin to be considered done.

---

## Backend Fixes Applied (Completed)

These were fixed during the overnight agent run and benefit all plugins:

### Phase 1 — Foundation Fixes
- **SpanMapper** (`kain-core/src/diagnostics.rs`) — All errors now report `file:line:col` instead of raw byte offsets
- **Parser error quality** (`kain-core/src/parser.rs`) — Reserved keyword detection, struct literal detection, `::` vs `.` guidance
- **TypeMapper** (`ue5-shaders/src/type_mapping.rs`) — Single source of truth for KAIN→HLSL type mappings, used by both validator and codegen
- **USF array literal support** (`ue5-shaders/src/codegen_usf.rs`) — `[a, b, c]` in shaders now emits `static const float arr[] = {a, b, c}` — no more manual if/else chains required
- **USF cast expression support** (`ue5-shaders/src/codegen_usf.rs`) — `expr as Float` now emits `(float)expr` with type compatibility validation
- **@N binding semantics** (`ue5-shaders/src/validation.rs`) — Removed incorrect cbuffer slot limit; `@N` is an ordering index, not a D3D register
- **Vector operation codegen** (`ue5/src/codegen_ue5.rs`) — `floor(v)` on Vec2/Vec3/Vec4 emits component-wise `FVector(FMath::FloorToFloat(v.X), ...)` 
- **UObject pointer detection** (`ue5/src/codegen_ue5.rs`) — Member access on UObject-derived types correctly uses `->` vs `.` for value types
- **Array method translation** (`ue5/src/codegen_ue5.rs`) — `.len()→.Num()`, `.push()→.Add()`, `.pop()→.Pop()`, `.clear()→.Empty()`
- **Delegate codegen** (`ue5/src/codegen_ue5.rs`) — `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` macros instead of `TFunction`
- **Stdlib auto-discovery** (`cli/src/packager/ue5_pipeline.rs`) — Stdlib loading restored; auto-discovers `stdlib/ue5/` via env var → exe walk → CWD walk

### Previously Fixed (Materialize Era)
- Oracle RPC validation rejecting user-defined structs — fixed in `oracle.rs`
- USF validator rejecting `UVec2`/`UInt`/`Mat4` types — fixed in `validation.rs`
- Constant buffer slot overflow on shaders with 14+ params — fixed in `validation.rs`
- F-prefix on method calls — fixed in `codegen_ue5.rs`
- Phantom RDG boilerplate for fragment shaders — fixed in `codegen_ue5.rs`
- Double E-prefix on enums — fixed in packager
- Double IMPLEMENT_MODULE — fixed in packager

---

## Source-Level Fix Patterns (Applied to All Plugins)

These patterns were applied systematically to every plugin's `.kn` files:

| Pattern | Fix |
|---|---|
| `var x = ...` | → `let x = ...` |
| `not expr` | → `expr == false` |
| `&&` / `\|\|` | → `and` / `or` |
| `for i in 0..n:` | → `while` loop with counter |
| `p::field` | → `p.field` (struct field access) |
| `Struct { field: val }` | → field-by-field assignment |
| `match x { arm => { ... } }` | → indented block style |
| `let mut x` | → `let x` |
| Parameter named `state` | → rename (reserved keyword) |
| Missing `_MAX` on enums | → add `EnumName_MAX` variant |
| `expr as Type` in shaders | → now supported via USF cast codegen |
| Array literals `[a, b, c]` in shaders | → now supported via USF array literal codegen |
| `actor { let field }` | → `actor { state field }` |

---

## Remaining Issues

### TemporalBlueprint — UE5 Name Collisions
Three known collisions blocking the UE5 build:
1. `ETransitionType` — conflicts with `Engine.h` engine type → rename in source
2. `InventorySlot` — type not found (likely stdlib dependency issue) → check stdlib loading
3. Function names (`henyey_greenstein`, `beer_lambert`, etc.) — conflict with stdlib shader functions → prefix with plugin namespace

**Fix strategy:** Rename conflicting types in `types.kn`, prefix plugin-specific functions, re-run FULLBUILD.bat.

### Cinema4DMograph — UE5 File Lock
KAIN compilation succeeded. UE5 build was blocked by a file lock during the agent run (likely another process holding the UE5 editor open). Re-run FULLBUILD.bat with UE5 editor closed.

---

## Recommended Completion Order

1. **TemporalBlueprint** — Fix name collisions in source, re-run FULLBUILD.bat
2. **Cinema4DMograph** — Re-run FULLBUILD.bat with editor closed
3. **Full regression suite** — Run all 5 FULLBUILD.bat scripts sequentially
4. **Pattern database export** — Document all patterns for future plugins

---

## Backend Files Reference

| File | What It Does |
|---|---|
| `crates/kain-core/src/parser.rs` | Syntax parsing — fix parse errors here |
| `crates/kain-core/src/types.rs` | Type checking — fix type errors here |
| `crates/kain-core/src/diagnostics.rs` | SpanMapper — error location reporting |
| `crates/kain-core/src/stdlib.rs` | Stdlib loading — `load_stdlib()` implementation |
| `crates/ue5/src/codegen_ue5.rs` | Runtime C++ codegen — actors, components, RPCs |
| `crates/ue5/src/oracle.rs` | UE5 semantic validation |
| `crates/ue5-editor/src/codegen.rs` | Editor C++ codegen dispatch |
| `crates/ue5-editor/src/slate.rs` | Slate widget generation |
| `crates/ue5-shaders/src/codegen_usf.rs` | USF shader codegen |
| `crates/ue5-shaders/src/validation.rs` | Shader validation |
| `crates/ue5-shaders/src/type_mapping.rs` | TypeMapper — KAIN→HLSL types |
| `crates/cli/src/packager/ue5_pipeline.rs` | Build orchestration + stdlib loading |
| `crates/cli/src/packager/codegen.rs` | Codegen dispatch |

**Critical:** After any backend change → `cargo install --path crates/cli --force` → test.

---

## Success Criteria

- `kain build --ue5` exits 0 with no errors for all 5 plugins
- `FULLBUILD.bat` completes all 3 steps for all 5 plugins
- Generated C++ compiles with UE5 5.4 BuildPlugin
- No features removed, no functions deleted, no simplification
- Materialize continues to compile as regression baseline

---

## Reference Materials

- `_Docs/MATERIALIZE_BUILD_REPORT.md` — Full fix log from Materialize (the playbook)
- `_Docs/STDLIB_BACKEND_RUNDOWN.md` — Backend stdlib wiring
- `.kiro/specs/plugin-compilation-pipeline/` — Full 32-task spec with detailed per-plugin workflows
- `Factory/Stdlib/DESIGN_DOC_STDLIB_EXPANSION.md` — What the stdlib agent is working on
