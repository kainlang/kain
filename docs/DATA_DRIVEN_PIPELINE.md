# KAIN Data-Driven Pipeline — From Hardcoded to Corpus-Powered

> **Date:** February 13, 2026  
> **Status:** Fully implemented, tested, and wired into compiler  
> **Impact:** Eliminated thousands of hardcoded values across the entire codegen stack

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
                        ▼
┌─────────────────────────────────────────────────────────┐
│  KAIN Compiler (Rust)                                    │
│                                                          │
│  Ue5Context::new() auto-loads all JSON from              │
│  unreal/metadata/*.json at startup:                      │
│                                                          │
│  ├── engine_knowledge.rs  → EngineKnowledge              │
│  │   (21,134 types, class hierarchy, includes, modules)  │
│  ├── widget_registry.rs   → WidgetRegistry               │
│  │   (2,346 widgets, 3,839 properties, 470 delegates)    │
│  └── shader_knowledge.rs  → ShaderKnowledge              │
│      (7,271 intrinsics, 612 permutations, 97 getters)    │
│                                                          │
│  All three are fields on Ue5Context, accessible by       │
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
widget_registry.json    → WidgetRegistry.load()
shader_knowledge.json   → ShaderKnowledge.load()
*.json (everything else) → EngineKnowledge.load_metadata() + StdLibResolver.load_from_metadata()
```

All three databases are public fields on `Ue5Context`:
- `ctx.knowledge` — `EngineKnowledge`
- `ctx.widget_registry` — `WidgetRegistry`
- `ctx.shader_knowledge` — `ShaderKnowledge`

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

---

## File Locations

### Extraction Scripts
| File | Purpose |
|------|---------|
| `unreal/scripts/corpus_extractor.py` | 3-pass type/widget/rule extractor |
| `unreal/scripts/shader_extractor.py` | 4-pass shader metadata extractor |
| `scripts/gather_source.py` | Copies all Source/ folders into corpus |

### JSON Metadata (loaded at compile time)
| File | Size | Contents |
|------|------|----------|
| `unreal/metadata/engine_knowledge_expanded.json` | 6.6 MB | 9,896 classes + 8,323 structs + 2,915 enums |
| `unreal/metadata/widget_registry.json` | 1.2 MB | 2,346 widgets + 470 delegates |
| `unreal/metadata/codegen_rules.json` | 15 KB | 209 constructor patterns + 1,683 modules |
| `unreal/metadata/shader_knowledge.json` | 3.7 MB | 7,271 intrinsics + 612 permutations |

### Rust Modules
| File | Crate | Tests | Purpose |
|------|-------|-------|---------|
| `crates/ue5/src/ue5/engine_knowledge.rs` | ue5 | — | Type database + hierarchy |
| `crates/ue5/src/ue5/widget_registry.rs` | ue5 | 6 | Widget property/delegate queries |
| `crates/ue5-shaders/src/shader_knowledge.rs` | ue5-shaders | 7 | Intrinsic/permutation/material queries |
| `crates/ue5/src/ue5/context.rs` | ue5 | — | Auto-loading + shared context |

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

# 4. Rebuild compiler
cargo build --release

# The compiler auto-loads all JSON from unreal/metadata/ at startup.
# No code changes needed — just new data.
```

---

## Test Results

**61 tests passing across all crates:**
- 28 `ue5` tests (type mapping, naming, oracle, engine knowledge)
- 10 `ue5-editor` tests (slate, details, viewport generation)
- 7 `shader_knowledge` tests (intrinsic queries, includes, permutations, material getters)
- 6 `widget_registry` tests (delegate resolution, property types, slot detection)
- 11 `cli` tests (packaging, build orchestration)

**Build validation:**
- `testing/Phase3/SlateTest4/` — 32 files generated, two-module split, all delegate types correct
- `testing/Phase4/shader_data_driven.kn` — 3 shaders (compute + 2 fragment), corpus intrinsics resolved
