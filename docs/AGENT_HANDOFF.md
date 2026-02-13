# KAIN Pipeline — Agent Handoff Document
> **Last Updated:** Feb 13, 2026  
> **Purpose:** Get the next LLM agent productive in <2 minutes  
> **Status:** Data-driven pipeline fully wired — 61 tests passing, corpus-powered codegen operational

---

## 1. WHAT IS KAIN?

KAIN is a **Python-like language that compiles to UE5 C++**. One `.kn` file produces a complete UE5 plugin: actors, components, structs, enums, delegates, Slate UI, Details panels, Viewports, Toolbars, Asset Editors, Editor Modules, and HLSL shaders (.usf).

**Key value prop:** A single 500-line `.kn` file generates 30+ C++ files (~8000 lines) that compile in Unreal Engine 5. The pipeline is data-driven — the compiler loads metadata extracted from the entire UE5 engine source code at startup, so it *knows* 21,134 types, 2,346 Slate widgets, and 7,271 shader functions.

**Binary:** `kain` (Rust, built via `cargo build --release --package cli`)  
**Install:** `cargo install --path crates/cli --force`  
**File extension:** `.kn`  
**Build command:** `cd PluginFolder && kain build --ue5`  
**Config:** `kain.toml` per plugin

---

## 2. REPOSITORY STRUCTURE

```
kain/                                # Rust compiler monorepo
├── crates/
│   ├── kain-core/                   # Parser, AST, type checker
│   ├── ue5/                         # Runtime codegen (actors, components, RPCs)
│   │   └── src/ue5/
│   │       ├── context.rs           # Ue5Context — loads all metadata, shared state
│   │       ├── engine_knowledge.rs  # 21,134-type database (classes, structs, enums)
│   │       ├── widget_registry.rs   # 2,346-widget database (properties, delegates)
│   │       ├── naming.rs            # UE5 prefix rules (A/F/E/U)
│   │       ├── types.rs             # Type mapping (KAIN → C++)
│   │       └── oracle.rs            # Semantic validator
│   ├── ue5-editor/                  # Editor codegen (Slate, Details, Viewports)
│   │   └── src/editor/
│   │       ├── codegen.rs           # Editor orchestrator + asset editors + modules
│   │       ├── slate.rs             # Slate widget tree → SNew() chains
│   │       ├── details.rs           # IDetailCustomization generation
│   │       ├── viewport.rs          # SEditorViewport + FEditorViewportClient
│   │       └── assets.rs            # FAssetEditorToolkit generation
│   ├── ue5-shaders/                 # Shader codegen (HLSL .usf files)
│   │   └── src/
│   │       ├── codegen_usf.rs       # USF code generation (~1970 lines)
│   │       └── shader_knowledge.rs  # 7,271-function shader database
│   └── cli/                         # CLI binary + packager
│       └── src/
│           ├── main.rs              # Entry point
│           └── packager/            # Multi-file build orchestrator
├── unreal/
│   ├── metadata/                    # JSON databases loaded at compile time
│   │   ├── engine_knowledge_expanded.json  (6.6 MB — 21,134 types)
│   │   ├── widget_registry.json            (1.2 MB — 2,346 widgets)
│   │   ├── codegen_rules.json              (15 KB  — 209 patterns)
│   │   └── shader_knowledge.json           (3.7 MB — 7,271 functions)
│   └── scripts/
│       ├── corpus_extractor.py      # 3-pass type/widget/rule extractor
│       └── shader_extractor.py      # 4-pass shader metadata extractor
├── testing/
│   ├── Phase3/SlateTest4/           # ACTIVE: "Ulta" — comprehensive system health plugin
│   ├── Phase4/                      # Shader data-driven tests
│   └── BestExample/ULTIMATE_DEMO.kn # Feature reference
├── unreal/plugins/                  # Production plugins (COSMOS, Flow)
└── docs/                            # You are here
```

---

## 3. THE DATA-DRIVEN PIPELINE (THE BIG WIN)

**See `docs/DATA_DRIVEN_PIPELINE.md` for the full deep-dive.**

The compiler no longer guesses about UE5 types, widgets, or shader functions. It loads metadata extracted from the actual engine source code:

### What Gets Loaded at Startup

| Database | File | Size | Entries |
|----------|------|------|---------|
| `EngineKnowledge` | `engine_knowledge_expanded.json` | 6.6 MB | 9,896 classes + 8,323 structs + 2,915 enums |
| `WidgetRegistry` | `widget_registry.json` | 1.2 MB | 2,346 widgets + 470 delegates + 3,839 properties |
| `ShaderKnowledge` | `shader_knowledge.json` | 3.7 MB | 7,271 intrinsics + 612 permutations + 97 material getters |

### How It's Extracted

```bash
# Corpus extraction (types + widgets) — scans 785 plugin Source/ folders
python unreal/scripts/corpus_extractor.py "M:\Utility\Unreal-Corpus" --output unreal/metadata

# Shader extraction — scans 1,151 Engine Shader files
python unreal/scripts/shader_extractor.py "D:\Unreal\UE_5.7\Engine\Shaders" --output unreal/metadata
```

### How It's Used

All three databases are fields on `Ue5Context`, which is the shared compilation context:

- **`ctx.knowledge`** (`EngineKnowledge`) — Type resolution, class hierarchy, includes, module deps
- **`ctx.widget_registry`** (`WidgetRegistry`) — Slate delegate resolution, property types, slot detection
- **`ctx.shader_knowledge`** (`ShaderKnowledge`) — Intrinsic return types, include resolution, permutation validation

`Ue5Context::new()` auto-loads all JSON from `unreal/metadata/` by filename routing.

---

## 4. THE FOUR CODEGEN CRATES

### `ue5` (Runtime)
- **Owns:** `EngineKnowledge`, `WidgetRegistry`, `Ue5Context`, naming conventions
- **Generates:** Actor headers/cpp, component headers, struct headers, enum headers, delegate macros, blueprint function libraries
- **Key file:** `codegen_ue5.rs` — `gen_actor_with_shaders()`, `gen_expr()`, `map_type()`, `is_pointer_receiver()`
- **Tests:** 28 passing

### `ue5-editor` (Editor)
- **Receives:** `Ue5Context` from runtime crate (includes all databases)
- **Generates:** Slate widgets (`SCompoundWidget`), Details customization (`IDetailCustomization`), Viewports (`SEditorViewport`), Toolbars, Asset Editors (`FAssetEditorToolkit`), Editor Modules (`IModuleInterface`)
- **Data-driven:** `slate.rs` queries `WidgetRegistry` for delegate types before falling back to hardcoded values
- **Tests:** 10 passing

### `ue5-shaders` (Shaders)
- **Owns:** `ShaderKnowledge` module
- **Generates:** `.usf` HLSL files, C++ shader parameter structs (`FGlobalShader`), `IMPLEMENT_GLOBAL_SHADER` registration
- **Data-driven:** `emit_function_call()` fallback uses `ShaderKnowledge.infer_return_type()` — 7,271 functions get proper type inference instead of defaulting to `float4`
- **Supports:** Fragment, Compute, Vertex shaders with permutations (`CFG_*` / `ENABLE_*` prefix)
- **Tests:** 18 passing (11 codegen + 7 shader_knowledge)

### `cli` (Packager)
- **Orchestrates:** Reads `kain.toml`, parses all `.kn` sources, merges ASTs, runs type checker + oracle, dispatches to all 3 codegen crates, writes modular file output
- **Auto-splits:** Detects runtime vs editor items, generates two-module split when both exist
- **Key file:** `packager.rs` — handles per-item slicing, delegate header, master header, `.uplugin`, `.Build.cs`

---

## 5. CURRENT TEST STATUS

### Test Suites: 61/61 Passing

```bash
cargo test
# 28 ue5 + 10 ue5-editor + 18 ue5-shaders + 6 widget_registry (in ue5-shaders) = 61
```

### Build Validation

| Plugin | Location | Status | Files Generated |
|--------|----------|--------|----------------|
| **Ulta** (SlateTest4) | `testing/Phase3/SlateTest4/` | ✅ Builds clean | 32 files, two-module split |
| **Shader test** | `testing/Phase4/shader_data_driven.kn` | ✅ Builds clean | 3 shaders compiled |
| **COSMOS** | `unreal/plugins/COSMOS/` | Builds (not re-tested this session) | Full plugin |

---

## 6. PATH FORWARD

### Immediate Next Steps
1. **Compile `SlateTest4` in UE5** — Copy to UE5 project Plugins/ folder, regenerate project files, compile. Fix any remaining C++ issues.
2. **Fix any UE5 compile errors** — Likely minor: missing includes, API differences between UE5 versions.
3. **Validate in-editor** — Open plugin, click "KAIN System Dashboard", verify UI renders.

### Known Remaining Issues (Minor)
- **Compute shader `[numthreads(32,32,1)]`** — Code still uses old hardcoded default in some paths. The `ShaderKnowledge.default_thread_group()` returns `(8,8,1)` but `emit_shader_body` doesn't use it yet for the `[numthreads]` declaration.
- **Compute `RWTexture2D` always emits `OutputTexture`** — Should use the actual uniform name from the KAIN source.
- **`sample()` in compute shaders uses `.Load()`** — Correct for compute, but the TexCoord pattern is always 2D (`ThreadId.xy`) even for 3D textures.
- **Duplicate `FPSInput`/`FPSOutput` structs** — Multiple fragment shaders in one file each emit their own copy. Should be deduplicated.
- **Missing `_MAX` enum warnings** — Cosmetic. The compiler warns but doesn't fail.

### Medium-Term
- **`.ush` header generation** — Currently only generates `.usf`. Surface shaders and shared utility functions should go in `.ush` files.
- **Wire `ShaderKnowledge` include resolution** — When KAIN code calls `CalcSceneDepth()`, auto-add `#include "SceneTexturesCommon.ush"`.
- **Wire `ShaderKnowledge` permutation validation** — Warn when a `CFG_*` name isn't in the corpus.
- **Improve `is_pointer_type_by_name()`** — Query EngineKnowledge for all classes instead of hardcoded list.
- **Run full test suite on COSMOS and Flow** — Verify existing plugins rebuild cleanly.

### Long-Term
- **UE5 version compatibility** — Test against 5.3, 5.4, 5.5 (some APIs changed: EditorStyle → AppStyle)
- **Hot reload** — `kain watch --ue5` for live recompilation
- **Marketplace packaging** — Automated .uplugin versioning
- **Embedded intelligence** — Bake the metadata JSON into the compiler binary via `include_bytes!()` so it doesn't need the `unreal/metadata/` folder at runtime

---

## 7. HOW TO BUILD & TEST

```bash
# Build the compiler
cd kain
cargo build --release

# Install to PATH
cargo install --path crates/cli --force

# Build a plugin
cd testing/Phase3/SlateTest4
kain build --ue5

# Run all tests
cd kain
cargo test
# Expected: 61 tests, all passing

# Re-extract metadata (if engine version changes)
python unreal/scripts/corpus_extractor.py "M:\Utility\Unreal-Corpus" --output unreal/metadata
python unreal/scripts/shader_extractor.py "D:\Unreal\UE_5.7\Engine\Shaders" --output unreal/metadata
cargo build --release  # Compiler auto-loads new JSON
```

---

## 8. KEY PATTERNS TO KNOW

### UE5 Naming Conventions (naming.rs)
- Actors: `Player` → `APlayer` (A-prefix)
- Structs: `Transform` → `FTransform` (F-prefix)
- Enums: `Direction` → `EDirection` (E-prefix)
- Components: `Health` → `UHealthComponent` (U-prefix)
- **Critical:** If the KAIN source already has the prefix (e.g., `EHealthStatus`), the naming functions detect it and don't double-prefix.

### Codegen Flow
```
.kn source → Parser (kain-core) → AST → Type Checker → Oracle Validator
    ↓
Packager reads kain.toml, dispatches to:
    ├── ue5 crate        → Actor/Struct/Enum/Delegate .h/.cpp
    ├── ue5-editor crate → Slate/Details/Viewport/Toolbar/AssetEditor/Module .h/.cpp
    └── ue5-shaders crate → .usf + shader binding .h/.cpp
    ↓
Packager writes: master header, .uplugin, .Build.cs, delegate header
    ↓
Python post-processor cleans up empty lines
```

### Data Loading Flow
```
Ue5Context::new()
    ├── Scans unreal/metadata/*.json
    ├── widget_registry.json     → WidgetRegistry.load()
    ├── shader_knowledge.json    → ShaderKnowledge.load()
    └── *.json (all others)      → EngineKnowledge.load_metadata()
                                   + StdLibResolver.load_from_metadata()
```

### KAIN Attribute → UE5 Feature Mapping
| KAIN | UE5 Output |
|------|------------|
| `@datatable struct` | `FTableRowBase` subclass |
| `@component struct` | `UActorComponent` subclass |
| `actor Name` | `AActor` subclass with RPCs |
| `@slate struct` | `SCompoundWidget` with SLATE_BEGIN_ARGS |
| `@details struct` | `IDetailCustomization` subclass |
| `@viewport struct` | `SEditorViewport` + `FEditorViewportClient` |
| `@toolbar struct` | `FToolBarBuilder` extension |
| `@asset_editor struct` | `FAssetEditorToolkit` subclass |
| `@editor_module struct` | `IModuleInterface` with IMPLEMENT_MODULE |
| `shader fragment/compute` | `.usf` + `FGlobalShader` + `IMPLEMENT_GLOBAL_SHADER` |
| `type X = delegate(...)` | `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` |

---

## 9. FILES YOU'LL EDIT MOST

| File | What It Does | When To Edit |
|------|-------------|--------------|
| `crates/ue5/src/codegen_ue5.rs` | Actor/struct/enum C++ generation | Runtime codegen bugs |
| `crates/ue5/src/ue5/engine_knowledge.rs` | Engine type database (21K types) | Adding new engine types |
| `crates/ue5/src/ue5/widget_registry.rs` | Widget database (2.3K widgets) | Widget delegate resolution |
| `crates/ue5-editor/src/editor/slate.rs` | Slate widget tree → SNew() | UI generation bugs |
| `crates/ue5-editor/src/editor/details.rs` | Details panel generation | Property panel bugs |
| `crates/ue5-shaders/src/codegen_usf.rs` | USF shader generation | Shader codegen bugs |
| `crates/ue5-shaders/src/shader_knowledge.rs` | Shader database (7.2K functions) | Intrinsic handling |
| `crates/cli/src/packager.rs` | Build orchestration | File output/structure bugs |
| `unreal/metadata/*.json` | Metadata databases | Re-extract when engine updates |
| `testing/Phase3/SlateTest4/ultimate.kn` | Comprehensive test plugin | Adding test coverage |

---

## 10. CRITICAL HISTORY (FOR CONTEXT)

### Feb 12, 2026 — 11 Critical Codegen Bugs Fixed
All caught by `ultimate.kn` test. Key fixes: double E-prefix on enums, F-prefix on method calls, `.` vs `->` on pointers, phantom RDG boilerplate, FVector→FLinearColor, double S-prefix on viewports, wrong delegate binding, lost slider max values, missing FText wrapping, double IMPLEMENT_MODULE, and master header `.generated.h` removal.

### Feb 13, 2026 — Data-Driven Pipeline Built
Extracted 21,134 types + 2,346 widgets + 7,271 shader functions from UE5 engine source. Created `widget_registry.rs` and `shader_knowledge.rs` modules. Wired all three databases into `Ue5Context`. Replaced hardcoded intrinsic matching with corpus-powered queries. Test count: 32 → 61.

### Feb 13, 2026 — Auto Two-Module Split
Packager auto-detects runtime vs editor items and generates `Plugin/` (Runtime) + `PluginEditor/` (Editor) module split with correct Build.cs dependencies and `.uplugin` entries.
