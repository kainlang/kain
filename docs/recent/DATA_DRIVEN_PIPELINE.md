# KAIN Data-Driven Pipeline — From Hardcoded to Corpus-Powered

> **Date:** February 13, 2026 (updated)  
> **Status:** Fully implemented, tested, and wired into compiler  
> **Impact:** Eliminated thousands of hardcoded values across the entire codegen stack  
> **Latest:** Module Dependency Graph + Virtual Method Obligations systems added

---

## The Problem

KAIN's codegen relied on hardcoded lists everywhere:

- **~50 shader intrinsics** hardcoded in `emit_function_call()` — missed thousands of HLSL/UE5 functions
- **~15 Slate widget delegate types** hardcoded in `native_delegate_for_property()` — missed 2,300+ widgets
- **~40 engine type mappings** hardcoded in `StdLibResolver` — missed 21,000+ types
- **Thread group size `[32,32,1]`** hardcoded — not even in Epic's top 5 patterns
- **Return type `float4`** as universal fallback — wrong for scalar intrinsics like `dot()`, `length()`

Every unknown function returned `float4`. Every unknown widget delegate was guessed. Every unknown type was a compile error.

---

## The Solution: Three Extraction Passes

We built Python extractors that scan real UE5 engine source code and shader files, then output structured JSON metadata that the Rust compiler loads at startup. No more guessing — the compiler now *knows* what Epic's code actually looks like.

### Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│  UE5 Engine Source Code (M:\Utility\Unreal-Corpus)       │
│  785 plugin Source/ folders + Engine Runtime/Slate/Core   │
└───────────────────────┬─────────────────────────────────┘
                        │  corpus_extractor.py (3-pass)
                        ▼
┌─────────────────────────────────────────────────────────┐
│  engine_knowledge_expanded.json  (6.6 MB)                │
│  widget_registry.json            (1.2 MB)                │
│  codegen_rules.json              (15 KB)                 │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────┼─────────────────────────────────┐
│  UE5 Engine Shaders (D:\Unreal\UE_5.7\Engine\Shaders)   │
│  1,151 files: 556 .usf + 545 .ush + 43 .h               │
└───────────────────────┬─────────────────────────────────┘
                        │  shader_extractor.py (4-pass)
                        ▼
┌─────────────────────────────────────────────────────────┐
│  shader_knowledge.json           (3.7 MB)                │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────┼─────────────────────────────────┐
│  UE5 Engine Source (D:\Unreal\UE_5.7\Engine\Source)      │
│  711 .Build.cs files + 16,412 C++ headers                │
└───────────────────────┬─────────────────────────────────┘
                        │  module_graph_extractor.py (5-pass)
                        │  virtual_obligations_extractor.py (3-pass)
                        │  uht_extractor.py (5-pass)
                        ▼
┌─────────────────────────────────────────────────────────┐
│  module_graph.json               (1.7 MB)                │
│  virtual_obligations.json        (4.3 MB)                │
│  uht_rules.json                  (361 KB)                │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│  KAIN Compiler (Rust)                                    │
│                                                          │
│  Ue5Context::new() auto-loads all JSON from              │
│  unreal/metadata/*.json at startup:                      │
│                                                          │
│  ├── engine_knowledge.rs     → EngineKnowledge           │
│  │   (21,134 types, class hierarchy, includes, modules)  │
│  ├── widget_registry.rs      → WidgetRegistry            │
│  │   (2,346 widgets, 3,839 properties, 470 delegates)    │
│  ├── shader_knowledge.rs     → ShaderKnowledge           │
│  │   (7,271 intrinsics, 612 permutations, 97 getters)    │
│  ├── uht_rules.rs            → UhtRules                  │
│  │   (337 rules, 154 specifiers, 25 incompatible combos) │
│  ├── module_graph.rs         → ModuleGraph               │
│  │   (711 modules, 6,272 types, 16,208 headers, 61 APIs) │
│  └── virtual_obligations.rs  → VirtualObligations        │
│      (3,541 classes, 11 KAIN-focus, auto-stub generation) │
│                                                          │
│  All seven are fields on Ue5Context, accessible by       │
│  every codegen crate during compilation.                 │
└─────────────────────────────────────────────────────────┘
```

---

## Pass 1: UE5 Corpus Extraction (engine + widgets)

### Script: `unreal/scripts/corpus_extractor.py`

A 3-pass Python extractor that scans the entire UE5 engine source corpus.

**Source:** `M:\Utility\Unreal-Corpus` — gathered via `scripts/gather_source.py` which walks all UE5 plugin `Source/` directories and copies them into one flat corpus folder. Includes 785 plugin folders plus Engine core modules (Slate, SlateCore, Engine, CoreUObject).

### Pass 1A: Type Registry

Scans every `.h` file for `UCLASS`, `USTRUCT`, `UENUM` declarations using regex patterns derived from `Ue5Scanner.scan_file()`.

**Output: `engine_knowledge_expanded.json` (6.6 MB)**

| Category | Count |
|----------|-------|
| Classes | 9,896 |
| Structs | 8,323 |
| Enums | 2,915 |
| Include mappings | 21,134 |
| Module mappings | 21,134 |

Each entry includes: name, parent class, header file, module name, specifiers, functions (name + return type + params), properties (name + type + specifiers), and whether abstract.

**Merge strategy (engine_knowledge.rs `ingest_metadata`):** When a new entry arrives, it only overwrites an existing one if it has MORE detail (more functions or properties). This prevents the expanded corpus data from clobbering hand-seeded entries that have richer type information.

### Pass 1B: Widget Registry

Scans Slate/SlateCore source for `SNew(SWidget)` patterns, `.Property()` calls, `SLATE_ARGUMENT`, `SLATE_ATTRIBUTE`, `SLATE_EVENT` macros, and delegate type declarations.

**Output: `widget_registry.json` (1.2 MB)**

| Category | Count |
|----------|-------|
| Widgets | 2,346 |
| Properties | 3,839 |
| Events | 1,530 |
| Slots | 140 |
| Delegate types | 470 |

Each widget entry includes: class name, header, parent, properties (name + type + is_event), events (name + delegate_type), slots (default/multi).

### Pass 1C: Codegen Rules

Extracts patterns for constructor calls, replication usage, and Build.cs module dependencies.

**Output: `codegen_rules.json` (15 KB)**

| Category | Count |
|----------|-------|
| Constructor patterns | 209 |
| Replication uses | 65 |
| Build.cs modules | 1,683 |

---

## Pass 2: Shader Corpus Extraction

### Script: `unreal/scripts/shader_extractor.py`

A 4-pass Python extractor that scans the entire UE5 Engine Shaders directory — every `.usf`, `.ush`, and `.h` file that Epic ships.

**Source:** `D:\Unreal\UE_5.7\Engine\Shaders` (1,151 files)

### Pass 2A: Intrinsic Catalog

Scans for function calls, function definitions, and macro definitions. Starts with a baseline of 95 known HLSL intrinsics (barrier functions, wave intrinsics, math, texture ops) and discovers everything else.

**Results:**

| Category | Count |
|----------|-------|
| HLSL intrinsics confirmed | 95 |
| UE5 function definitions | 8,069 |
| UE5 macro definitions | 1,469 |
| **Total known functions** | **7,271** (deduplicated with call counts) |

Each function entry includes: name, category (`hlsl`/`ue5`/`macro`), call count across the corpus, parameter types and names (when parseable), source `.ush` file, and whether it's a macro.

**Example entries:**
```json
"CalcSceneDepth": {
    "name": "CalcSceneDepth",
    "category": "ue5",
    "call_count": 61,
    "params": [{"type": "float2", "name": "ScreenUV"}],
    "param_count": 1,
    "source": "SceneTexturesCommon.ush"
}
```

### Pass 2B: Include Graph

Maps every `#include` directive to build a dependency graph and frequency table.

**Results:**

| Most Included File | Count |
|-------------------|-------|
| `Common.ush` | 551x (combined path variants) |
| `/Engine/Generated/Material.ush` | 64x |
| `ShaderPrint.ush` | 64x |
| `DeferredShadingCommon.ush` | 111x |
| `VertexFactory.ush` | 44x |
| `MonteCarlo.ush` | 38x |

Also builds a `file_provides` map: which `.ush` file defines which functions. This enables automatic `#include` resolution — when KAIN code calls `CalcSceneDepth()`, the compiler knows to include `SceneTexturesCommon.ush`.

### Pass 2C: Permutations & Bindings

Scans for `#ifdef`, `SHADER_PERMUTATION_BOOL`, `SHADER_PERMUTATION_INT`, `SHADER_PERMUTATION_ENUM`, `[numthreads()]`, `groupshared`, and `cbuffer` declarations.

**Results:**

| Category | Count |
|----------|-------|
| Unique permutations | 612 |
| Thread group patterns | 14 |
| Groupshared variables | 309 |
| cbuffers | 5 |

**Top permutations:**

| Permutation | Usage |
|-------------|-------|
| `SUBSTRATE_GBUFFER_FORMAT` | 160x |
| `SUBSTRATE_ENABLED` | 124x |
| `FEATURE_LEVEL` | 72x |
| `SUBSTRATE_INLINE_SHADING` | 53x |
| `VIRTUAL_TEXTURE_TARGET` | 52x |
| `USE_INSTANCING` | 47x |
| `ALLOW_STATIC_LIGHTING` | 44x |

**Thread group sizes (what Epic actually uses):**

| Size | Usage | Pattern |
|------|-------|---------|
| `[1,1,1]` | 90x | Per-pixel ops |
| `[8,8,1]` | 60x | Tile-based (Lumen, shadows) — **new default** |
| `[64,1,1]` | 50x | Linear workloads |
| `[1024,1,1]` | 10x | Large linear |
| `[4,4,4]` | 4x | 3D volumetrics |

Our previous hardcoded default `[32,32,1]` wasn't even in Epic's corpus.

### Pass 2D: Material & Surface Patterns

Scans for material output assignments, `Get*` material getters, `MaterialFloat*` type aliases, and material parameter access patterns.

**Results:**

| Category | Count |
|----------|-------|
| Material getter functions | 97 |
| Material parameters | 350 |
| MaterialFloat type aliases | 4 |

**Top material getters (what surface shaders actually use):**

| Getter | Usage |
|--------|-------|
| `GetPixelParameters` | 66x |
| `GetOpacity` | 57x |
| `GetVertexParameters` | 46x |
| `GetBaseColor` | 24x |
| `GetWorldPositionOffset` | 23x |
| `GetEmissive` | 22x |
| `GetSpecular` | 19x |
| `GetMetallic` | 17x |
| `GetRoughness` | 15x |
| `GetAmbientOcclusion` | 12x |
| `GetSubsurfaceData` | 10x |
| `GetRefraction` | 8x |
| `GetAnisotropy` | 6x |
| `GetDisplacement` | 2x |

**MaterialFloat types:** `MaterialFloat4` (111x), `MaterialFloat` (79x), `MaterialFloat3` (56x), `MaterialFloat2` (54x) — UE5's precision-agnostic float aliases used in all material shaders.

**Output: `shader_knowledge.json` (3.7 MB, 152,274 lines)**

---

## Rust Integration

### 1. `engine_knowledge.rs` (crate: `ue5`)

The original `EngineKnowledge` struct, now fed by the expanded corpus data. Provides:

- **Type resolution:** `StaticMeshComponent` → `UStaticMeshComponent*` with correct `#include`
- **Class hierarchy:** Knows that `ACharacter` inherits `APawn` inherits `AActor`
- **Include mapping:** Every type → its header file
- **Module mapping:** Every type → its Build.cs module dependency
- **Constructor validation:** Knows arg counts and types for engine constructors
- **Named colors:** `color("sunset")` → `FLinearColor(1.0, 0.5, 0.0, 1.0)`

**Wired into:** `Ue5Context.knowledge` — accessible by all codegen crates.

### 2. `widget_registry.rs` (crate: `ue5`) — NEW

A queryable database of Slate widget metadata. Created from scratch for this pipeline.

**Query API:**
- `get_event_delegate(widget, event)` → e.g., `("SSlider", "OnValueChanged")` → `"FOnFloatValueChanged"`
- `get_event_delegate_any(event)` → global delegate lookup across all widgets
- `get_property_type(widget, prop)` → property type for a specific widget
- `get_widget_header(widget)` → which header to include
- `has_default_slot(widget)` / `has_multi_slot(widget)` → slot type detection

**Integration point:** `slate.rs` functions `native_delegate_for_property()` and `map_event_delegate_type()` now query the widget registry FIRST, then fall back to hardcoded values. This means 2,346 widgets get correct delegate types automatically instead of relying on ~15 hardcoded entries.

**Tests:** 6 unit tests.

### 3. `shader_knowledge.rs` (crate: `ue5-shaders`) — NEW

A queryable database of HLSL/UE5 shader function signatures, permutations, thread groups, and material properties. Created from scratch for this pipeline.

**Query API:**
- `is_known_function(name)` → true for any of the 7,271 known functions
- `is_hlsl_intrinsic(name)` → true for HLSL builtins (`lerp`, `saturate`, etc.)
- `is_ue5_function(name)` → true for UE5-defined helpers and macros
- `infer_return_type(name)` → `"passthrough"` (match first arg type), concrete type, or `"unknown"`
- `get_function_include(name)` → which `.ush` file defines it
- `is_known_permutation(name)` → validates permutation names against corpus
- `default_thread_group()` → `(8, 8, 1)` based on corpus data
- `is_material_getter(name)` → validates material property accessors
- `get_param_count(name)` → expected parameter count for validation

**Integration point:** `codegen_usf.rs` `emit_function_call()` fallback now uses `infer_return_type()` instead of blindly returning `float4` for unknown functions. The `USFContext` carries an `Option<ShaderKnowledge>` loaded from `shader_knowledge.json` at shader compilation time.

**Return type inference strategy:**
- **`"passthrough"`** — functions like `lerp`, `clamp`, `normalize`, `dot` that preserve their first argument's type
- **`"bool"`** — functions like `all`, `any`, `isfinite`
- **`"void"`** — functions like `clip`, `InterlockedAdd`, barrier functions
- **`"float4"`** — texture operations like `Sample`, `Load`, `GatherRed`
- **Concrete types** — `asfloat` → `"float"`, `asint` → `"int"`, etc.
- **`"unknown"`** — still falls back, but first checks if it's a known function (uses first arg type) vs truly unknown (uses `float4`)

**Tests:** 7 unit tests.

### 4. `context.rs` (crate: `ue5`)

`Ue5Context::new()` auto-loads all JSON from `unreal/metadata/` at startup, routing by filename:

```
widget_registry.json        → WidgetRegistry.load()
editor_attributes.json      → EditorAttributesRegistry.load()
shader_knowledge.json       → ShaderKnowledge.load()
uht_rules.json              → UhtRules.load()
module_graph.json            → ModuleGraph.load()
virtual_obligations.json     → VirtualObligations.load()
*.json (everything else)     → EngineKnowledge.load_metadata() + StdLibResolver.load_from_metadata()
```

All seven databases are public fields on `Ue5Context`:
- `ctx.knowledge` — `EngineKnowledge`
- `ctx.widget_registry` — `WidgetRegistry`
- `ctx.shader_knowledge` — `ShaderKnowledge`
- `ctx.uht_rules` — `UhtRules`
- `ctx.module_graph` — `ModuleGraph`
- `ctx.virtual_obligations` — `VirtualObligations`
- `ctx.editor_attributes` — `EditorAttributesRegistry`

---

## Before vs After

| What | Before (Hardcoded) | After (Data-Driven) |
|------|--------------------|--------------------|
| Known UE5 types | ~40 | **21,134** |
| Known Slate widgets | ~15 | **2,346** |
| Known shader functions | ~50 | **7,271** |
| Known permutations | 7 prefix checks | **612** from corpus |
| Material properties | 4 | **97** getters cataloged |
| Thread group default | `[32,32,1]` (wrong) | **`[8,8,1]`** (corpus-validated) |
| Unknown function return type | Always `float4` | Type-inferred from category |
| Widget delegate resolution | Guessed from name | Queried from 470 known delegates |
| Build.cs module deps | ~10 hardcoded | **711 modules**, auto-resolved from type/API usage |
| Pure virtual overrides | Manually remembered | **3,541 classes** with obligation sets, auto-stub generation |
| UHT validation rules | ~20 hardcoded checks | **337 rules**, 154 specifiers, 25 incompatible combos |

---

## File Locations

### Extraction Scripts
| File | Purpose |
|------|---------|
| `unreal/scripts/corpus_extractor.py` | 3-pass type/widget/rule extractor |
| `unreal/scripts/shader_extractor.py` | 4-pass shader metadata extractor |
| `unreal/scripts/ue5_scanner.py` | Engine header scanner (UCLASS/USTRUCT/UENUM) |
| `unreal/scripts/uht_extractor.py` | 5-pass UHT validation rule extractor |
| `unreal/scripts/module_graph_extractor.py` | 5-pass .Build.cs module dependency extractor |
| `unreal/scripts/virtual_obligations_extractor.py` | 3-pass pure virtual method obligation extractor |
| `scripts/gather_source.py` | Copies all Source/ folders into corpus |

### JSON Metadata (loaded at compile time)
| File | Size | Contents |
|------|------|----------|
| `unreal/metadata/engine_knowledge_expanded.json` | 6.6 MB | 9,896 classes + 8,323 structs + 2,915 enums |
| `unreal/metadata/engine_5.7_scanned.json` | — | 2,688 classes with functions/properties from UE5.7 headers |
| `unreal/metadata/widget_registry.json` | 1.2 MB | 2,346 widgets + 470 delegates |
| `unreal/metadata/codegen_rules.json` | 15 KB | 209 constructor patterns + 1,683 modules |
| `unreal/metadata/shader_knowledge.json` | 3.7 MB | 7,271 intrinsics + 612 permutations |
| `unreal/metadata/uht_rules.json` | 361 KB | 337 validation rules + 154 specifiers + 25 incompatible combos |
| `unreal/metadata/module_graph.json` | 1.7 MB | 711 modules + 6,272 type→module + 16,208 header→module + 61 API→module |
| `unreal/metadata/virtual_obligations.json` | 4.3 MB | 3,541 classes with pure virtual obligations + 11 KAIN-focus classes |
| `unreal/metadata/editor_attributes.json` | — | Editor attribute metadata (descriptions, base classes, required modules) |

### Rust Modules
| File | Crate | Tests | Purpose |
|------|-------|-------|---------|
| `crates/ue5/src/ue5/engine_knowledge.rs` | ue5 | 4 | Type database + hierarchy |
| `crates/ue5/src/ue5/widget_registry.rs` | ue5 | 6 | Widget property/delegate queries |
| `crates/ue5-shaders/src/shader_knowledge.rs` | ue5-shaders | 7 | Intrinsic/permutation/material queries |
| `crates/ue5/src/ue5/uht_rules.rs` | ue5 | 8 | UHT specifier validation + incompatible combos |
| `crates/ue5/src/ue5/module_graph.rs` | ue5 | 11 | Module dependency graph + Build.cs dep resolution |
| `crates/ue5/src/ue5/virtual_obligations.rs` | ue5 | 11 | Pure virtual obligation queries + auto-stub generation |
| `crates/ue5/src/ue5/editor_attributes.rs` | ue5 | — | Editor attribute registry |
| `crates/ue5/src/ue5/context.rs` | ue5 | — | Auto-loading + shared context (routes all 7 systems) |

---

## How To Re-Extract

If the engine version changes or the corpus grows:

```bash
# 1. Gather source (if corpus folder doesn't exist yet)
python scripts/gather_source.py "D:\Unreal\UE_5.7" "M:\Utility\Unreal-Corpus"

# 2. Run corpus extractor (types + widgets + rules)
python unreal/scripts/corpus_extractor.py "M:\Utility\Unreal-Corpus" --output unreal/metadata

# 3. Run shader extractor
python unreal/scripts/shader_extractor.py "D:\Unreal\UE_5.7\Engine\Shaders" --output unreal/metadata

# 4. Run engine header scanner (UCLASS/USTRUCT/UENUM with functions/properties)
python unreal/scripts/ue5_scanner.py "D:\Unreal\UE_5.7\Engine\Source\Runtime" "D:\Unreal\UE_5.7\Engine\Source\Editor" unreal/metadata/engine_5.7_scanned.json

# 5. Run UHT rule extractor (validation rules from EpicGames.UHT C# source)
python unreal/scripts/uht_extractor.py "D:\Unreal\UE_5.7\Engine\Source\Programs\Shared\EpicGames.UHT" unreal/metadata/uht_rules.json

# 6. Run module dependency graph extractor (.Build.cs → module deps + type→module)
python unreal/scripts/module_graph_extractor.py "D:\Unreal\UE_5.7\Engine\Source" --engine-scan unreal/metadata/engine_5.7_scanned.json

# 7. Run virtual obligations extractor (pure virtual methods from C++ headers)
python unreal/scripts/virtual_obligations_extractor.py "D:\Unreal\UE_5.7\Engine\Source"

# 8. Rebuild compiler
cargo build --release

# The compiler auto-loads all JSON from unreal/metadata/ at startup.
# No code changes needed — just new data.
```

---

## Test Results

**99 tests passing across all crates:**
- 66 `ue5` tests (type mapping, naming, oracle, engine knowledge, UHT rules, module graph, virtual obligations)
- 10 `ue5-editor` tests (slate, details, viewport generation)
- 18 `ue5-shaders` tests (shader codegen, intrinsic queries, includes, permutations)
- 3 `kain-core` tests (parser, type checker)
- 2 `cli` tests (packaging, build orchestration)

**Build validation:**
- `testing/Phase3/SlateTest4/` — 32 files generated, two-module split, all delegate types correct
- `testing/Phase4/shader_data_driven.kn` — 3 shaders (compute + 2 fragment), corpus intrinsics resolved
- `testing/CorpusTest/` — 40+ C++ files, 2 .usf shaders, auto-resolved Build.cs deps (RenderCore, RHI, Renderer)

---

## Pass 3: Module Dependency Graph Extraction

### Script: `unreal/scripts/module_graph_extractor.py`

A 5-pass Python extractor that scans all `.Build.cs` files in the UE5 engine source tree and cross-references with engine scan metadata.

**Source:** `D:\Unreal\UE_5.7\Engine\Source` (711 `.Build.cs` files)

### Pass 3A: Parse .Build.cs Files
Extracts module names, categories (Runtime/Editor/Developer), `PublicDependencyModuleNames`, `PrivateDependencyModuleNames`, `DynamicallyLoadedModuleNames`, and `PrivateIncludePathModuleNames` using regex on C# source.

### Pass 3B: Cross-Reference Engine Scan
Maps 6,272 UE5 types to their modules using `engine_5.7_scanned.json`.

### Pass 3C: Transitive Closure
Computes the transitive public dependency closure for every module — if module A depends on B which depends on C, then A transitively depends on C.

### Pass 3D: Header→Module Mapping
Scans `Public/` directories to build 16,208 header→module mappings.

### Pass 3E: API Symbol→Module Map
Maps 61 known API symbols (like `AddShaderSourceDirectoryMapping`) to their owning modules.

**Output: `module_graph.json` (1.7 MB)**

| Category | Count |
|----------|-------|
| Modules | 711 |
| Type→module mappings | 6,272 |
| Header→module mappings | 16,208 |
| API→module mappings | 61 |

**Integration:** `build_cs_gen.rs` generates Build.cs files with data-driven dependency lists. `codegen.rs` queries the module graph to compute runtime/editor deps based on what the plugin actually references. No more hardcoded module lists.

**Result:** `✓ KainToolkit.Build.cs + auto-resolved: RenderCore, RHI, Renderer`

---

## Pass 4: Virtual Method Obligations Extraction

### Script: `unreal/scripts/virtual_obligations_extractor.py`

A 3-pass Python extractor that scans all C++ headers for pure virtual methods and computes obligation sets via inheritance chain traversal.

**Source:** `D:\Unreal\UE_5.7\Engine\Source` (16,412 C++ headers)

### Pass 4A: Scan Pure Virtuals
Scans every header for class declarations and pure virtual methods (`= 0` and `PURE_VIRTUAL()` macro). Detects both raw C++ classes and UCLASS types.

### Pass 4B: Compute Obligation Sets
Walks inheritance chains to compute what pure virtuals a concrete subclass must implement. For each class: collect all pure virtuals from ancestors, subtract any that are implemented (non-pure virtual override) by the class or its ancestors.

### Pass 4C: Generate Default Stubs
For each obligation, generates a sensible default C++ stub body based on return type (`void` → `{ }`, `bool` → `{ return false; }`, `FName` → `{ return FName(); }`, etc.).

**Output: `virtual_obligations.json` (4.3 MB)**

| Category | Count |
|----------|-------|
| Classes scanned | 36,838 |
| Classes with pure virtuals | 2,334 |
| Classes with unresolved obligations | 3,541 |
| KAIN-focus classes (rich detail) | 11 |

**Key KAIN-relevant classes:**

| Base Class | Obligations | Methods |
|-----------|-------------|---------|
| `FAssetEditorToolkit` | 5 | GetToolkitFName, GetBaseToolkitName, GetWorldCentricTabPrefix, GetEditorModeManager, GetWorldCentricTabColorScale |
| `IDetailCustomization` | 1 | CustomizeDetails |
| `IPropertyTypeCustomization` | 2 | CustomizeHeader, CustomizeChildren |
| `FGCObject` | 2 | AddReferencedObjects, GetReferencerName |
| `FTickableGameObject` | 2 | Tick, GetStatId |
| `SLeafWidget` | 2 | OnPaint, ComputeDesiredSize |
| `SPanel` | 3 | OnArrangeChildren, ComputeDesiredSize, GetChildren |
| `IAssetEditorInstance` | 7 | GetEditorName, FocusWindow, IsPrimaryEditor, InvokeTab, ... |
| `IToolkitHost` | 10 | GetParentWidget, BringToFront, GetTabManager, ... |

**Integration:** `gen_asset_editor()` in `codegen.rs` uses `VirtualObligations` to auto-generate override declarations and definitions. Custom methods (GetToolkitFName, etc.) keep editor-name-specific implementations; remaining obligations get auto-generated default stubs. Falls back to hardcoded behavior when data not loaded.

---

## High-Value Extraction Targets (ranked)

### ✅ DONE — 1. Module Dependency Graph
**Status:** Fully implemented and verified. See Pass 3 above.

### ✅ DONE — 2. Virtual Method Obligations
**Status:** Fully implemented and verified. See Pass 4 above.

### 🟡 3. Delegate Signatures Registry — Prevents the Broadcast() bug class
**What:** Extract all `DECLARE_DELEGATE*` / `DECLARE_MULTICAST_DELEGATE*` / `DECLARE_DYNAMIC_*` signatures from engine headers.
**Source:** `D:\Unreal\UE_5.7\Engine\Source\**\*.h` — regex for `DECLARE_*DELEGATE*` macros.
**Why:** We fixed the `Broadcast()` args bug by tracking user-defined delegate params. But when KAIN code uses engine delegates (like `FOnClicked`, `FOnFloatValueChanged`, `FSimpleDelegate`), we're still guessing. A registry of engine delegate signatures would let us validate and bridge any delegate correctly.
**Output:** `delegate_registry.json` — `{ "FOnClicked": { "macro": "DECLARE_DELEGATE_RetVal", "return": "FReply", "params": [] }, ... }`

### 🟡 4. UPROPERTY/UFUNCTION Meta Specifier Defaults — Correctness
**What:** Extract common UPROPERTY() and UFUNCTION() specifier combinations and their valid contexts.
**Source:** Already partially in `uht_rules.json`, but missing the frequency data — which combos are most common in practice.
**Why:** The oracle validates specifiers but doesn't suggest the right ones. Knowing that `UPROPERTY(EditAnywhere, BlueprintReadWrite)` appears 12,000 times vs `UPROPERTY(EditDefaultsOnly)` 800 times would let us auto-select the best specifier combo based on context.
**Output:** Extend `uht_rules.json` with frequency data, or new `specifier_patterns.json`.

### 🟡 5. Constructor Pattern Templates — Quality of life
**What:** Extract full constructor bodies for common component setups (not just `CreateDefaultSubobject` patterns, but the full init sequence).
**Source:** `*.cpp` files — match constructor bodies for common actor/component patterns.
**Why:** `codegen_rules.json` only has 15KB of pattern frequencies. A richer extraction would capture things like "when you have a `USpringArmComponent`, you always set `bUsePawnControlRotation = true`" — letting codegen emit production-quality constructors.
**Output:** Extend `codegen_rules.json` with full init sequences per component type.