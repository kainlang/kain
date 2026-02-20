# KAIN Compiler Post-Mortem: FluidFlow Plugin Generation
**Version:** 1.0
**Date:** 2026-02-17
**Project:** FluidFlow Unreal Engine Plugin

This document serves as a comprehensive technical analysis of the KAIN compiler's behavior, bugs, and edge cases encountered during the generation of the `FluidFlow` plugin. It is intended to guide the refinement of the codegen backend and bootstrap loader.

---

## 1. Critical Architecture: The "Actor Value-Type" Bug
### Issue
KAIN currently treats `actor` types referenced within other `actor` or `struct` definitions as **value types** (stack-allocated instances) rather than **reference types** (pointers). This is catastrophic for Unreal Engine generation.

### Symptom
**KAIN Source:**
```kain
actor HyperFluidController:
    state world: HyperFluidWorld
```
**Generated C++ (Faulty):**
```cpp
UPROPERTY()
HyperFluidWorld world; // Error: UObject/AActor cannot be member variable unless it's a pointer (UObject*) or TSubclassOf
```
**UHT Error:** `Unable to find 'class', 'delegate', 'enum', or 'struct' with name 'HyperFluidWorld'`

### Root Cause
The type system does not distinguish between `struct` (value) and `actor` (reference) when generating member variables. It defaults to value semantics.

### Proposed Fix (Backend)
Modify the codegen logic for `state` and `var` declarations:
1.  **Check Type Kind:** If the type is an `actor` or `@uclass` object:
    *   **Always generate as Pointer:** `AHyperFluidWorld* world;`
    *   **Forward Declare:** Ensure `class AHyperFluidWorld;` is generated at the top of the header.
2.  **Initialization:** Initialize to `nullptr` in the constructor, not a default constructor call `HyperFluidWorld()`.

---

## 2. Parser Strictness: Syntax Divergence (Actor vs. Struct)
### Issue
The parser enforces different keywords for `actor` vs. `component` (struct) contexts but provides opaque error messages when they are mixed.

### Symptom A: Variable Declaration
*   **Context:** Inside a `struct` or `@component`.
*   **Faulty KAIN:** `state my_var: Int` OR `var my_var: Int`
*   **Error:** `Expected identifier, got State` / `Expected identifier, got Var`
*   **Correct KAIN:** `my_var: Int` (No keyword).

### Symptom B: Method Declaration
*   **Context:** Inside a `struct` or `@component`.
*   **Faulty KAIN:** `on Initialize():`
*   **Error:** `Expected Colon, got Ident("Initialize")`
*   **Correct KAIN:** `fn Initialize():`

### Proposed Fix (Backend)
1.  **Unify Syntax:** Allow `state` and `var` interchangeably in both contexts to reduce cognitive load, OR providing a clear "Use 'fn' for structs, 'on' for actors" error message.
2.  **Smart Error Reporting:** "You are inside a Struct. Did you mean to use 'fn' instead of 'on'?"

---

## 3. Name Collisions & Prefixing
### Issue
KAIN generates C++ class names directly from the KAIN type names without safeguarding against engine-level collisions.

### Symptom
**KAIN Source:**
```kain
struct ParticleSystemComponent: ...
```
**Generated C++:** `class UParticleSystemComponent : public UActorComponent`
**Unreal Error:** "Class 'UParticleSystemComponent' shares engine name..."

### Proposed Fix (Backend)
1.  **Auto-Prefixing:** Implement a project-level prefix config (e.g., `prefix = "Hyper"`) in `kain.toml`.
2.  **Sanitization:** Internally check against a list of reserved Unreal Engine class names (`Actor`, `Pawn`, `ParticleSystem`, `StaticMesh`, etc.) and append a suffix or prefix if a collision is detected.

---

## 4. Initialization & Constructors
### Issue
KAIN currently attempts to initialize struct members inline in the header, which works for primitive types but fails for complex UObjects/Components if not handled carefully.

### Symptom
**KAIN Source:**
```kain
state simulation: HyperFluidSimulationCore = HyperFluidSimulationCore()
```
**Generated C++:**
```cpp
UPROPERTY()
UHyperFluidSimulationCore* simulation = CreateDefaultSubobject<UHyperFluidSimulationCore>(TEXT("simulation")); // Only valid in Constructor!
```
*   **Bug:** Assignments with complex constructors (`= Ty()`) are sometimes generated as member initializers in the header (illegal for UObjects) instead of inside the `.cpp` constructor.

### Proposed Fix (Backend)
1.  **Move to Constructor:** Ensure ALL object creation logic (`CreateDefaultSubobject`) is moved strictly to the `.cpp` constructor body.
2.  **Header Cleanliness:** Headers should only contain `UPROPERTY()` declarations, mostly uninitialized or initialized to `nullptr`/primitive defaults.

---

## 5. Replication Validation (`@replicated`)
### Issue
The `@replicated` decorator is permitted by the parser on `struct` members, but the backend generates `DOREPLIFETIME` macros that fail because structs don't inherit from `AActor` or `UActorComponent` in the way the macro expects (unless the struct is a Component).

### Symptom
**KAIN Source:**
```kain
struct MyStruct:
    @replicated
    val: Int
```
**Generated C++:** `DOREPLIFETIME(FMyStruct, val);` fails compilation if `FMyStruct` isn't a valid replication owner.

### Proposed Fix (Backend)
1.  **Context Check:** Only generate `GetLifetimeReplicatedProps` for classes that inherit from `AActor` or `UActorComponent`.
2.  **Struct Replication:** For plain structs, `@replicated` should probably trigger a warning "Struct member replication is not supported directly; replicate the Struct property in the Actor instead."

---

## 6. Shader Management & Auto-Discovery
### Issue
The loop of adding shaders to `kain.toml` manually is prone to user error.

### Symptom
*   User adds `shader compute MyKernel...` in `.kn`.
*   User compiles.
*   C++ code references `MyKernel`.
*   **Linker Error:** The shader wasn't built because it wasn't in `kain.toml`.

### Proposed Fix (Bootstrap)
1.  **Scanner Pass:** The compiler/bootstrap should scan all input `.kn` files for `shader [type] [Name]` blocks.
2.  **Auto-Populate:** Automatically internally add these found shaders to the build manifest, overriding or merging with `kain.toml`.

---

## 7. Header File Hygiene
### Issue
Renaming a type in KAIN leaves the old generated header file in the `Source/Public` directory. Unreal's UHT scans *all* headers in that folder, causing redefinition errors or "class not found" ghosts.

### Symptom
*   Rename `ParticleSystemComponent` -> `HyperFluidParticleSystemComponent`.
*   File `FParticleSystemComponent.h` remains on disk.
*   UHT finds both. Chaos ensues.

### Proposed Fix (Bootstrap)
1.  **Clean Build Option:** A flag `--clean` that wipes the `Generated/` folder before emission.
2.  **Manifest Tracking:** track exactly which files were emitted in the previous run and delete any that are not in the current emission set.

---

## 8. Unreal Header Tool (UHT) Compliance
### Generated Macros
Ensure the following macros are strictly ordered:
1.  `#include "CoreMinimal.h"`
2.  `#include "GameFramework/Actor.h"` (or relevant base)
3.  `#include "MyClass.generated.h"` (**Must be the last include**)

### Class Declaration
*   `UCLASS()` macros must have correct specifiers (`BlueprintType`, `Blueprintable`).
*   `GENERATED_BODY()` must be the very first line of the class.

---

## Summary
The KAIN compiler has successfully implemented 95% of the logic required to build complex fluid simulation plugins. The remaining 5% lies in stricter handling of **C++ Pointers vs Values**, **Lifecycle Management (Constructors)**, and **Namespace Hygiene**. Fixing these in the backend will remove the need for manual C++ intervention.
