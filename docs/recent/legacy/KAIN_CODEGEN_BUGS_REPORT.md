# KAIN Codegen Bug Report & Fix Tracking
**Project:** ToonShaderz Plugin
**Date:** February 18, 2026

This document tracks the persistent codegen bugs encountered during the KAIN -> UE5 build process. These should be used as reference for updating the Rust-based KAIN compiler and the Godmode v3 codegen engine.

---

## 🟢 1. Invalid Identifier Sanitization (Pipes in Names)
**Bug:** The compiler uses raw `@category` strings (e.g., `"ToonShaderz|Colors"`) to generate C++ local variable names in Details Customizations.
**Example:**
```cpp
// GENERATED (INVALID)
IDetailCategoryBuilder& ToonShaderz|ColorsCat = DetailBuilder.EditCategory(TEXT("ToonShaderz|Colors"));
```
**Fix:** Variable names must be sanitized (e.g., replace `|` with `_` or remove the prefix). 
**Manual Patch:** Renamed variables to `ColorsCat`, `ShadingCat`, etc. in `FToonDirectorDetailsCustomization.cpp`.

---

## 🟢 2. Slate Template Argument Omission
**Bug:** KAIN generates `SListView` widgets without the required C++ template argument.
**Example:**
```cpp
// GENERATED (INVALID)
SNew(SListView) 

// EXPECTED
SNew(SListView<TSharedPtr<FSomeType>>)
```
**Fix:** The compiler needs to track the item type for list/collection bindings in Slate.
**Manual Patch:** Replaced with `SBox` in `SSToonStylePanel.cpp` to bypass compilation failure.

---

## 🟢 3. Missing Virtual Shader Path Mapping
**Bug:** Shaders are registered via `IMPLEMENT_GLOBAL_SHADER` using a virtual path (e.g., `"/Plugin/ToonShaderz/..."`), but the module never initializes the mapping. This results in a fatal crash at engine startup.
**Error:** `Couldn't find source file of virtual shader path '/Plugin/ToonShaderz/ToonWatercolor.usf'`
**Fix:** The `StartupModule()` of the runtime module must include the mapping:
```cpp
FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ToonShaderz"))->GetBaseDir(), TEXT("Shaders"));
AddShaderSourceDirectoryMapping(TEXT("/Plugin/ToonShaderz"), ShaderDir);
```
**Manual Patch:** Added mapping and required includes (`Interfaces/IPluginManager.h`, `ShaderCore.h`, `Misc/Paths.h`) to `ToonShaderz.cpp`.

---

## 🟢 4. Include Pollution (Non-Existent Headers)
**Bug:** The generated Blueprint Library header (`ToonShaderzBlueprintLibrary.h`) includes headers for every possible panel/component, even if they weren't generated or don't exist.
**Example:** Includes like `FSToonColorPanel.h` and `FToonDirectorDetails.h` were requested but never found in the file system.
**Fix:** Include logic should be strictly based on generated file manifest.
**Manual Patch:** Commented out/removed missing includes in `ToonShaderzBlueprintLibrary.h`.

---

## 🟢 5. Shader Pass Argument Mismatch
**Bug:** The helper function `AddPass_ShaderName` declaration in `.h` often mismatches the call site in `AToonDirector.cpp`.
**Example:** The header expected 6 arguments (including `texture_size` and `gamma`), but the codegen in the Director was only passing 4 or 5, or misordering Texture vs. Float parameters.
**Fix:** Ensure the Director's calling logic matches the exact signature of the reflected shader parameter struct.
**Manual Patch:** Aligned arguments in `AToonDirector.cpp` (e.g., manually passing `512.0f` for `texture_size`).

---

## 🟢 6. Class Name Disrepancy
**Bug:** Director calls use a "shortened" or "alternative" class name for libraries.
**Example:** Call site used `UToonShaderzFunctionLibrary::ToonShadowSteps`, but the generated class was `UToonShaderzBlueprintLibraryFunctionLibrary`.
**Fix:** Unified naming convention across the compiler phases.

---

## 🟢 7. Replication Macro Omisison
**Bug:** Adding `@replicated` generates `GetLifetimeReplicatedProps` but fails to include `Net/UnrealNetwork.h` and the necessary `DOREPLIFETIME` macros in the `.cpp`.
**Fix:** Inject `Net/UnrealNetwork.h` into the PCH or implementation and generate the boilerplate `DOREPLIFETIME` block.
**Manual Patch:** Removed `@replicated` from `.kn` as a temporary measure.

---

## 🟢 9. HLSL Reserved Keywords
**Bug:** Use of reserved words like `line` as variable names in shaders.
**Example:** `float line = smoothstep(...)` causes `modifiers must appear before type` error in SM6.
**Fix:** Sanitize USF variable names (e.g., prefix with `v_` or check against HLSL keyword list).
**Manual Patch:** Renamed `line` to `outline_line` in `ToonOutline.usf`.

---

## 🟢 10. SM6 Vector Initialization Rigidity
**Bug:** KAIN generates `float3(scalar_expression)` which can trigger `too few elements in vector initialization` on certain compilers.
**Example:** `color = color + float3((noise - 0.5) * strength)`
**Fix:** Rely on implicit scalar-to-vector broadcast: `color = color + ((noise - 0.5) * strength)`.
**Manual Patch:** Fixed in `ToonPostFX.usf`.

---

## 🟢 11. Missing Global Transformation Parameters
**Bug:** Vertex shaders generated for "Hull" effects often need `WorldToClip` matrices, but KAIN doesn't always include them in the `SHADER_PARAMETER_STRUCT` or the C++ `AddPass` signature if they weren't explicitly defined in the data model.
**Fix:** Automatically inject standard transformation matrices for `SF_Vertex` shaders.
**Manual Patch:** Added `WorldToClip` to `ToonHullOutline.usf`, `.h`, and `.cpp`.

---

## 🟢 12. Uninitialized UPROPERTY Members (UE5.4 Strictness)
**Bug:** Generated structs for DataTables/Settings contain `UPROPERTY` fields but no constructor or inline initialization.
**Error:** `LogClass: Error: Property ... is not initialized properly.`
**Fix:** Generate default constructors or use C++11 member initializers (e.g., `float value = 0.0f;`).
**Manual Patch:** Added member initializers (e.g., `= 0.0f`, `= EToonStyle::CelClassic`) to `FToonAtmosphereData.h`, `FToonMaterialLayer.h`, and `FToonPresetData.h`.

---

## 🟢 13. Render Graph Builder Assertions (RDG Scope Safety)
**Bug:** The `AToonDirector::Tick` method performed null checks (returns) *after* constructing the `FRDGBuilder`. If the check failed, the builder would be destroyed without `Execute()` being called, triggering `ensure(bHasExecuted)` assertion failure.
**Error:** `Assertion failed: bHasExecuted [File:RenderGraphValidation.cpp] [Line: 143] Render graph execution is required...`
**Fix:** Move all null checks and early returns *before* the `FRDGBuilder` instantiation line.
**Manual Patch:** Validated in `AToonDirector.cpp`.

---

## 🟢 14. Transient RenderTarget Initialization
**Bug:** `UPROPERTY(Transient) UTextureRenderTarget2D*` members are null by default. The KAIN simulation logic assumes they exist, leading to either early exits (no simulation) or crashes.
**Fix:** Inject initialization logic into `BeginPlay()` to create these transient resources if they are invalid.
**Manual Patch:**
```cpp
// Added to BeginPlay()
if (!PositionRT_A) {
    PositionRT_A = NewObject<UTextureRenderTarget2D>(this);
    PositionRT_A->InitAutoFormat(512, 512);
    // ...
}
```

---

## 🟢 15. Enum Name Resolution Mismatch
**Bug:** KAIN generates default values using "Display Names" (e.g., `CellShaded`) or "Internal Names" (e.g., `High`) that do not match the strict C++ `UENUM` identifiers (e.g., `CelClassic`, `Standard`).
**Error:** `C2065: 'CellShaded': undeclared identifier`
**Fix:** Strict validation of default values against the generated Enum header. Ensure identifiers match exactly (e.g., `EToonStyle::CelClassic` instead of `CellShaded`).
**Manual Patch:** Corrected 6+ enum default values in `FToonPresetData.h`.
