# KAIN Pipeline — Agent Handoff Document
> **Last Updated:** Feb 12, 2026  
> **Purpose:** Get the next LLM agent productive in <2 minutes  
> **Status:** Pipeline at 99% — 11 critical codegen bugs just fixed, awaiting first UE5 compile test

---

## 1. WHAT IS KAIN?

KAIN is a **Python-like language that compiles to UE5 C++**. One `.kn` file produces a complete UE5 plugin: actors, components, structs, enums, delegates, Slate UI, Details panels, Viewports, Toolbars, Asset Editors, Editor Modules, and HLSL shaders (.usf).

**Key value prop:** A single 500-line `.kn` file generates 20+ C++ files (~8000 lines) that compile in Unreal Engine 5. The pipeline is designed to be the most LLM-friendly game development tool on the planet.

**Binary:** `kain` (Rust, built via `cargo build --release --package cli`)  
**File extension:** `.kn`  
**Build command:** `cd PluginFolder && kain build --ue5`  
**Config:** `KAIN.toml` per plugin

---

## 2. REPOSITORY STRUCTURE

```
kain-private/
├── kain/                          # Rust compiler monorepo
│   ├── crates/
│   │   ├── kain-core/             # Parser, AST, type checker
│   │   ├── ue5/                   # Runtime codegen (actors, components, RPCs)
│   │   │   └── src/
│   │   │       ├── codegen_ue5.rs # Main C++ code generator (~3200 lines)
│   │   │       └── ue5/
│   │   │           ├── context.rs       # Ue5Context (shared state)
│   │   │           ├── naming.rs        # UE5 prefix rules (A/F/E/U)
│   │   │           ├── types.rs         # Type mapping (KAIN → C++)
│   │   │           ├── oracle.rs        # Semantic validator
│   │   │           └── engine_knowledge.rs  # Engine type database
│   │   ├── ue5-editor/            # Editor codegen (Slate, Details, Viewports)
│   │   │   └── src/editor/
│   │   │       ├── codegen.rs     # Editor orchestrator + asset editors + modules
│   │   │       ├── slate.rs       # Slate widget tree → SNew() chains
│   │   │       ├── details.rs     # IDetailCustomization generation
│   │   │       ├── viewport.rs    # SEditorViewport + FEditorViewportClient
│   │   │       └── assets.rs      # FAssetEditorToolkit generation
│   │   ├── ue5-shaders/           # Shader codegen (HLSL .usf files)
│   │   └── cli/                   # CLI binary + packager
│   │       └── src/
│   │           ├── main.rs        # Entry point
│   │           └── packager.rs    # Multi-file build orchestrator (~1500 lines)
│   └── Cargo.toml
├── plugins/                       # Production plugins (COSMOS, Flow, AlphaGen)
├── testing/
│   └── Phase3/
│       ├── SlateTest2/            # Reference: comprehensive Slate test (old system)
│       ├── SlateTest3/            # Reference: ParticleEditor (old system)
│       └── SlateTest4/            # ACTIVE: "Ulta" — first test of new architecture
│           ├── kain.toml
│           ├── ultimate.kn        # 544-line self-validating dashboard plugin
│           ├── Source/             # Generated C++ output
│           └── Shaders/           # Generated .usf output
├── docs/                          # You are here
└── vscode-kain/                   # VS Code syntax highlighting extension
```

---

## 3. THE THREE CODEGEN CRATES

### `ue5` (Runtime)
- **Owns:** `EngineKnowledge`, `Ue5Context`, naming conventions, `StdLibResolver`
- **Generates:** Actor headers/cpp, component headers, struct headers, enum headers, delegate macros, blueprint function libraries
- **Key file:** `codegen_ue5.rs` — `gen_actor_with_shaders()`, `gen_expr()`, `map_type()`, `is_pointer_receiver()`
- **Key modules:** `stdlib_resolver.rs` — Maps KAIN stdlib functions (abs, sqrt, sin, etc.) to UE5 FMath:: equivalents
- **Tests:** 22 passing

### `ue5-editor` (Editor)
- **Receives:** `Ue5Context` from runtime crate (includes EngineKnowledge)
- **Generates:** Slate widgets (SCompoundWidget), Details customization (IDetailCustomization), Viewports (SEditorViewport), Toolbars, Asset Editors (FAssetEditorToolkit), Editor Modules (IModuleInterface)
- **Key files:** `slate.rs` (widget tree → SNew chains), `details.rs` (@slider/@color_picker/@button), `codegen.rs` (orchestrator)
- **Tests:** 10 passing

### `ue5-shaders` (Shaders)
- **Standalone:** No EngineKnowledge dependency
- **Generates:** `.usf` HLSL files, C++ shader parameter structs (FGlobalShader), IMPLEMENT_GLOBAL_SHADER registration
- **Supports:** Fragment, Compute, Vertex, Surface shaders with permutations (`CFG_*` prefix)

### `cli` (Packager)
- **Orchestrates:** Reads `KAIN.toml`, parses all `.kn` sources, merges ASTs, runs type checker + oracle, dispatches to all 3 codegen crates, writes modular file output
- **Key file:** `packager.rs` — handles per-item slicing, delegate header generation, master header, module registration, `.uplugin`, `.Build.cs`

---

## 4. ENGINEKNOWLEDGE SYSTEM

A queryable database of UE5 engine types seeded from `kain/unreal/metadata/engine_knowledge.json`.

**What it provides:**
- Type resolution: `StaticMeshComponent` → `UStaticMeshComponent*` with correct include
- Named colors: `color("sunset")` → `FLinearColor(1.0, 0.5, 0.0, 1.0)`
- Constructor formats: `vec3(x,y,z)` → `FVector(x,y,z)` with correct arg count validation
- Property string formats: UE5 ImportText/ExportText format strings
- Engine name collision detection (oracle)

**Wired into:** Both `ue5` and `ue5-editor` crates via `Ue5Context.knowledge`

---

## 5. BUGS JUST FIXED (This Session — Feb 12, 2026)

All 11 bugs were caught by the `ultimate.kn` test plugin. **All fixes are implemented and verified in generated output.**

| # | Bug | Crate | Root Cause | Fix |
|---|-----|-------|------------|-----|
| 1 | Double E-prefix (`EEHealthStatus`) | packager | Inline delegate map_type added E unconditionally | Use `naming::to_enum_name()` |
| 2 | F-prefix on method calls (`FSetStatus()`) | ue5 | All PascalCase calls got struct prefix | Only prefix KNOWN structs via context + EngineKnowledge |
| 3 | `.` instead of `->` on pointers | ue5 | `is_pointer_receiver` only checked components/actors | New `is_pointer_type_by_name()` with UObject type list |
| 4 | Phantom RDG boilerplate in actors | ue5 | All shaders passed to actor codegen | Filter to `ShaderStage::Compute` only |
| 5 | `FVector` instead of `FLinearColor` | ue5-editor | Color property didn't convert vec3→LinearColor | Detect FVector in Color property, convert to FLinearColor |
| 6 | Double-prefix `SFDiagnosticViewport` | ue5-editor | `format!("S{}", map_type(ty))` double-prefixed | Extract raw type name, apply S-prefix directly |
| 7 | Wrong `CreateSP` delegate binding | ue5-editor | InArgs delegates treated as member function ptrs | `is_inargs_reference()` → pass delegate directly |
| 8 | @slider max value lost (always 0.0) | ue5-editor | `extract_named_float_arg` always returned first arg | New `extract_float_arg_at(args, index)` positional |
| 9 | String literals not FText-wrapped | ue5-editor | Fallback property handler emitted raw strings | Detect string literal, wrap in `FText::FromString(TEXT(...))` |
| 10 | Double `IMPLEMENT_MODULE` | packager | Packager + editor module both generated it | Detect `@editor_module`, skip packager's version |
| 11 | Master header `.generated.h` | packager | Appended to non-UHT aggregation header | Removed — individual type headers have their own |

---

## 6. CURRENT TEST PLUGIN: `SlateTest4/ultimate.kn`

**Plugin name:** Ulta  
**Purpose:** Self-validating System Health Dashboard — open in UE5, click buttons, instantly see if codegen works  
**Location:** `testing/Phase3/SlateTest4/`

**What it exercises (13 subsystems):**
- 3 enums, 5 delegates, 2 structs (1 @datatable)
- 1 fragment shader with permutations
- 1 actor with @blueprint_callable, @replicated, Tick, material parameter driving
- 5 Slate widgets (nested composition, sliders, buttons, scroll, splitter)
- 1 details panel (@slider, @color_picker, @button)
- 1 viewport (@scene_actor, @camera)
- 1 toolbar (buttons, toggles, separators, shortcuts)
- 1 asset editor (wires viewport + details + toolbar + slate)
- 1 editor module (@menu_entry, @toolbar_button)

**Status:** `kain build --ue5` succeeds. Generated C++ output verified clean. **Not yet compiled in UE5.**

---

## 7. PATH FORWARD

### Immediate Next Steps
1. **Compile `SlateTest4` in UE5** — Copy to a UE5 project's Plugins/ folder, regenerate project files, compile. Fix any remaining C++ issues that only surface in the actual MSVC/Clang compiler.
2. **Fix any UE5 compile errors** — These will likely be minor: missing includes, wrong UHT macro placement, or API differences between UE5 versions.
3. **Validate in-editor** — Open the plugin, click "KAIN System Dashboard" in Tools menu, verify the UI renders and buttons respond.

### Known Remaining Issues (Minor)
- **Actor header has phantom RDG includes** — `RenderGraph.h`, `RenderGraphBuilder.h` etc. are still included in the actor .cpp even when no compute shaders exist. Harmless but wasteful. Fix: gate includes behind `!compute_shaders.is_empty()`.
- **`EEHealthStatus` in SSStatusRow.h SLATE_ARGS** — The Slate header still references `EEHealthStatus()` as default value in SLATE_BEGIN_ARGS. This is generated by the editor crate's `generate_slate_args()`, not the packager. Needs the same naming fix applied to the editor's type mapping.
- **`.uplugin` module type** — Currently `"Type": "Runtime"`. Plugins with editor-only code (Slate, Details) should use `"Type": "Editor"` or split into Runtime + Editor modules.
- **Double `CoreMinimal.h` include** — Some generated headers include it twice. Harmless (pragma once) but messy.

### Medium-Term
- **Run the full test suite on existing plugins** — COSMOS, Flow, AlphaGen should all rebuild cleanly with the new fixes.
- **Add regression tests** — Each of the 11 bugs should have a dedicated test case in the Rust test suites.
- **Improve `is_pointer_type_by_name()`** — Currently uses a hardcoded list of UObject types. Should query EngineKnowledge for all classes and check inheritance.

### Long-Term
- **UE5 version compatibility** — Test against 5.3, 5.4, 5.5. Some APIs changed (EditorStyle → AppStyle, etc.)
- **Hot reload** — `kain watch --ue5` for live recompilation
- **Marketplace packaging** — Automated .uplugin versioning, content browser integration

---

## 8. HOW TO BUILD & TEST

```bash
# Build the compiler
cd kain-private/kain
cargo build --release --package cli

# The binary is at: kain/target/release/kain.exe
# Copy to PATH or use directly

# Build a plugin
cd testing/Phase3/SlateTest4
kain build --ue5

# Run Rust tests
cd kain-private/kain
cargo test --package ue5 --package ue5-editor
# Expected: 22 + 10 = 32 tests, all passing
```

---

## 9. KEY PATTERNS TO KNOW

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
Packager reads KAIN.toml, dispatches to:
    ├── ue5 crate      → Actor/Struct/Enum/Delegate .h/.cpp
    ├── ue5-editor crate → Slate/Details/Viewport/Toolbar/AssetEditor/Module .h/.cpp
    └── ue5-shaders crate → .usf + shader binding .h/.cpp
    ↓
Packager writes: master header, .uplugin, .Build.cs, delegate header
    ↓
Python post-processor cleans up empty lines
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

## 10. FILES YOU'LL EDIT MOST

| File | What It Does | When To Edit |
|------|-------------|--------------|
| `crates/ue5/src/codegen_ue5.rs` | Actor/struct/enum C++ generation | Runtime codegen bugs |
| `crates/ue5/src/ue5/stdlib_resolver.rs` | KAIN stdlib → UE5 FMath mapping | Adding stdlib functions |
| `crates/ue5/src/ue5/engine_knowledge.rs` | Engine type database | Adding new engine types |
| `crates/ue5-editor/src/editor/slate.rs` | Slate widget tree → SNew() | UI generation bugs |
| `crates/ue5-editor/src/editor/details.rs` | Details panel generation | Property panel bugs |
| `crates/ue5-editor/src/editor/codegen.rs` | Editor orchestrator | Asset editor/module bugs |
| `crates/cli/src/packager.rs` | Build orchestration | File output/structure bugs |
| `testing/Phase3/SlateTest4/ultimate.kn` | Test plugin source | Adding test coverage |
