# KAIN Traits and Interfaces — Technical Specification

> **Document Version:** 1.0  
> **Last Updated:** Feb 19, 2026  
> **Status:** Research & Design Phase  
> **Primary Target:** UE5 UInterface System

---

## Executive Summary

KAIN's trait system is **parsed but not yet codegen-implemented**. The AST supports trait definitions, trait methods, and impl blocks, but no backend currently generates interface code. This document analyzes the current state, defines backend strategies (with UE5 as primary focus), and outlines implementation requirements for production-ready trait support.

**Key Finding:** UE5's UInterface system requires significant boilerplate (dual I/U classes, BlueprintNativeEvent functions, reflection macros) making it the most complex backend target. WASM/LLVM/Rust backends are comparatively straightforward.

---

## 1. Current State Analysis

### 1.1 AST Structure

**Location:** `crates/kain-core/src/ast.rs:304-332`

```rust
pub struct Trait {
    pub name: String,
    pub generics: Vec<Generic>,
    pub methods: Vec<TraitMethod>,
    pub visibility: Visibility,
    pub span: Span,
}

pub struct TraitMethod {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub effects: Vec<Effect>,
    pub default_impl: Option<Block>,  // Optional default implementation
    pub span: Span,
}

pub struct Impl {
    pub generics: Vec<Generic>,
    pub trait_name: Option<String>,  // None = inherent impl, Some = trait impl
    pub target_type: Type,
    pub methods: Vec<Function>,
    pub span: Span,
}
```

**Capabilities:**
- ✅ Trait definitions with methods
- ✅ Generic trait parameters
- ✅ Default method implementations
- ✅ Impl blocks (both inherent and trait impls)
- ❌ Associated types (not in AST)
- ❌ Trait bounds on generics (TypeBound exists but unused)
- ❌ Multiple trait inheritance


### 1.2 Parser Implementation

**Location:** `crates/kain-core/src/parser.rs:174-216`

**Current Syntax:**
```kain
impl MyType:
    fn method(self, x: Int) -> String:
        return "hello"
```

**Limitations:**
- ❌ No `impl Trait for Type` syntax (trait_name always None)
- ❌ No trait definition parsing (Trait AST node exists but parser doesn't create it)
- ✅ Impl blocks parse correctly
- ✅ Methods in impl blocks work

**Parser TODO:**
```kain
# Trait definition (NOT YET PARSED)
trait Damageable:
    fn take_damage(self, amount: Float) -> Bool
    fn is_alive(self) -> Bool

# Trait implementation (PARTIALLY PARSED)
impl Damageable for Player:
    fn take_damage(self, amount: Float) -> Bool:
        self.health = self.health - amount
        return self.health > 0.0
    
    fn is_alive(self) -> Bool:
        return self.health > 0.0
```

### 1.3 Type System Integration

**Location:** `crates/kain-core/src/types.rs:62-65`

```rust
pub struct TypedImpl {
    pub ast: Impl,
}
```

**Current Behavior:**
- Impl blocks are type-checked as `TypedItem::Impl`
- Methods are registered in runtime environment's method table
- No trait resolution or constraint checking
- No monomorphization of trait-generic functions

**Runtime Registration (interpreter only):**
```rust
// From runtime.rs:1791-1803
crate::types::TypedItem::Impl(i) => {
    let type_name = match &i.ast.target_type {
        Type::Named { name, .. } => name.clone(),
        _ => continue,
    };
    let type_methods = env.methods.entry(type_name).or_insert_with(HashMap::new);
    for method in &i.ast.methods {
        type_methods.insert(method.name.clone(), method.clone());
    }
}
```

**Type System TODO:**
- Trait constraint validation
- Trait bound checking on generics
- Associated type resolution
- Trait object type representation


### 1.4 Codegen Backend Status

| Backend | Trait Support | Status | Notes |
|---------|--------------|--------|-------|
| **UE5** | ❌ None | Not implemented | No UInterface generation |
| **UE5-Editor** | ❌ None | Not implemented | No editor interface support |
| **UE5-Shaders** | N/A | N/A | Shaders don't use traits |
| **WASM** | ❌ None | Not implemented | No vtable generation |
| **Interpreter** | ✅ Partial | Working | Method dispatch via HashMap |

**Critical Gap:** No production backend supports trait codegen. All trait/impl code is currently dead AST nodes.

---

## 2. UE5 UInterface System (Primary Target)

### 2.1 UInterface Architecture

UE5 uses a **dual-class system** for interfaces:

```cpp
// Interface class (I-prefix) - Pure abstract base
class IMyInterface {
public:
    virtual void MyMethod() = 0;
};

// UObject wrapper (U-prefix) - Reflection container
UINTERFACE(MinimalAPI, Blueprintable)
class UMyInterface : public UInterface {
    GENERATED_BODY()
};
```

**Why Two Classes?**
- `IMyInterface` = C++ interface (pure virtual methods)
- `UMyInterface` = UObject for reflection/Blueprint integration
- Classes implement `IMyInterface`, Blueprint system uses `UMyInterface`

### 2.2 Implementation Requirements

**For a class to implement an interface:**

```cpp
// Actor implementing interface
UCLASS()
class AMyActor : public AActor, public IMyInterface {
    GENERATED_BODY()
public:
    // Must implement all pure virtuals
    virtual void MyMethod() override;
};
```

**Blueprint-Callable Interfaces:**

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class UDamageable : public UInterface {
    GENERATED_BODY()
};

class IDamageable {
    GENERATED_BODY()
public:
    // BlueprintNativeEvent = C++ default + Blueprint override
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat")
    bool TakeDamage(float Amount);
    
    // Auto-generated by UHT: virtual bool TakeDamage_Implementation(float Amount) { return false; }
};
```

**UHT Magic:**
- `BlueprintNativeEvent` generates `_Implementation` suffix method
- C++ provides default in `_Implementation`, Blueprint can override
- Calling `TakeDamage()` dispatches to Blueprint if overridden, else C++ default


### 2.3 KAIN → UInterface Mapping Strategy

**KAIN Trait Definition:**
```kain
@blueprint
trait Damageable:
    fn take_damage(self, amount: Float) -> Bool
    fn is_alive(self) -> Bool
    fn get_max_health(self) -> Float:
        return 100.0  # Default implementation
```

**Generated UE5 Code:**

**File: `IDamageable.h`**
```cpp
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IDamageable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDamageable : public UInterface {
    GENERATED_BODY()
};

class IDamageable {
    GENERATED_BODY()
public:
    // Pure virtual (no default)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damageable")
    bool TakeDamage(float Amount);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damageable")
    bool IsAlive();
    
    // Has default implementation
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damageable")
    float GetMaxHealth();
    virtual float GetMaxHealth_Implementation() { return 100.0f; }
};
```

**KAIN Trait Implementation:**
```kain
actor Player:
    state health: Float = 100.0

impl Damageable for Player:
    fn take_damage(self, amount: Float) -> Bool:
        self.health = self.health - amount
        return self.health > 0.0
    
    fn is_alive(self) -> Bool:
        return self.health > 0.0
```

**Generated Actor Code:**

**File: `APlayer.h`**
```cpp
#pragma once
#include "GameFramework/Actor.h"
#include "IDamageable.h"  // Include interface
#include "APlayer.generated.h"

UCLASS()
class APlayer : public AActor, public IDamageable {  // Inherit interface
    GENERATED_BODY()
public:
    UPROPERTY(Replicated)
    float Health = 100.0f;
    
    // Implement interface methods
    virtual bool TakeDamage_Implementation(float Amount) override;
    virtual bool IsAlive_Implementation() override;
    // GetMaxHealth uses default from IDamageable
};
```

**File: `APlayer.cpp`**
```cpp
#include "APlayer.h"

bool APlayer::TakeDamage_Implementation(float Amount) {
    Health = Health - Amount;
    return Health > 0.0f;
}

bool APlayer::IsAlive_Implementation() {
    return Health > 0.0f;
}
```


### 2.4 Attribute Mapping

| KAIN Attribute | UE5 Specifier | Effect |
|----------------|---------------|--------|
| `@blueprint` | `Blueprintable` | Interface visible in Blueprint |
| (default) | `MinimalAPI` | Standard interface export |
| `@category("Name")` | `Category="Name"` | Blueprint category |
| (method default impl) | `virtual Method_Implementation()` | C++ default, Blueprint can override |
| (no default impl) | Pure `BlueprintNativeEvent` | Must be implemented |

### 2.5 Implementation Complexity

**Boilerplate per trait:**
- 2 class definitions (I-prefix + U-prefix)
- UINTERFACE macro with specifiers
- GENERATED_BODY() in both classes
- Per-method UFUNCTION macros
- `_Implementation` suffix methods
- Include guards, .generated.h includes

**Estimated LOC:** 15-30 lines per trait + 5-10 lines per method

**Codegen Challenges:**
1. **Dual-class generation** - Must emit both I and U classes
2. **Method suffix handling** - `_Implementation` for BlueprintNativeEvent
3. **Default implementations** - Inline in header vs separate .cpp
4. **Include dependencies** - Interface headers must be included by implementers
5. **Reflection ordering** - .generated.h must be last include

---

## 3. Alternative Backend Strategies

### 3.1 WASM Backend (Function Pointers)

**Strategy:** Compile traits to vtable structs with function pointers.

**KAIN Trait:**
```kain
trait Drawable:
    fn draw(self, x: Int, y: Int)
```

**Generated WASM (Conceptual):**
```wat
;; Vtable struct
(type $Drawable_vtable (struct
    (field $draw (ref $func_draw_signature))
))

;; Function signature
(type $func_draw_signature (func (param i32 i32 i32)))

;; Trait object = (data_ptr, vtable_ptr)
(type $Drawable_object (struct
    (field $data (ref any))
    (field $vtable (ref $Drawable_vtable))
))

;; Dynamic dispatch
(func $call_draw (param $obj (ref $Drawable_object)) (param $x i32) (param $y i32)
    (call_ref $func_draw_signature
        (struct.get $Drawable_object $data (local.get $obj))
        (local.get $x)
        (local.get $y)
        (struct.get $Drawable_vtable $draw
            (struct.get $Drawable_object $vtable (local.get $obj)))))
```

**Complexity:** Medium (vtable generation, function pointer management)

### 3.2 LLVM Backend (Native Vtables)

**Strategy:** Use LLVM's native vtable support (similar to C++ virtual methods).

**Generated LLVM IR:**
```llvm
; Trait vtable type
%Drawable.vtable = type { void (i8*, i32, i32)* }

; Trait object type
%Drawable = type { i8*, %Drawable.vtable* }

; Dynamic dispatch
define void @call_draw(%Drawable* %obj, i32 %x, i32 %y) {
    %vtable_ptr = getelementptr %Drawable, %Drawable* %obj, i32 0, i32 1
    %vtable = load %Drawable.vtable*, %Drawable.vtable** %vtable_ptr
    %draw_fn_ptr = getelementptr %Drawable.vtable, %Drawable.vtable* %vtable, i32 0, i32 0
    %draw_fn = load void (i8*, i32, i32)*, void (i8*, i32, i32)** %draw_fn_ptr
    %data_ptr = getelementptr %Drawable, %Drawable* %obj, i32 0, i32 0
    %data = load i8*, i8** %data_ptr
    call void %draw_fn(i8* %data, i32 %x, i32 %y)
    ret void
}
```

**Complexity:** Low (LLVM handles most vtable mechanics)


### 3.3 Rust Backend (Direct Translation)

**Strategy:** KAIN traits map 1:1 to Rust traits.

**KAIN Trait:**
```kain
trait Drawable:
    fn draw(self, x: Int, y: Int)

struct Circle:
    radius: Float

impl Drawable for Circle:
    fn draw(self, x: Int, y: Int):
        println("Drawing circle at ({x}, {y})")
```

**Generated Rust:**
```rust
pub trait Drawable {
    fn draw(&self, x: i64, y: i64);
}

pub struct Circle {
    pub radius: f64,
}

impl Drawable for Circle {
    fn draw(&self, x: i64, y: i64) {
        println!("Drawing circle at ({}, {})", x, y);
    }
}
```

**Complexity:** Trivial (nearly 1:1 mapping)

**Challenges:**
- Lifetime annotations (KAIN doesn't have explicit lifetimes)
- Trait bounds on generics
- Associated types

---

## 4. Method Dispatch Analysis

### 4.1 Static Dispatch (Monomorphization)

**When:** Trait used with concrete type known at compile time.

**KAIN Example:**
```kain
fn render_circle(c: Circle, x: Int, y: Int):
    c.draw(x, y)  # Concrete type = static dispatch
```

**UE5 Generated:**
```cpp
void RenderCircle(const FCircle& C, int32 X, int32 Y) {
    // Direct call - no virtual dispatch
    C.Draw(X, Y);
}
```

**Performance:** Zero overhead (inlined)

### 4.2 Dynamic Dispatch (Trait Objects)

**When:** Trait used as abstract type (e.g., array of different implementers).

**KAIN Example:**
```kain
fn render_all(shapes: Array<dyn Drawable>):
    for shape in shapes:
        shape.draw(0, 0)  # Dynamic dispatch
```

**UE5 Generated:**
```cpp
void RenderAll(const TArray<TScriptInterface<IDamageable>>& Shapes) {
    for (const auto& Shape : Shapes) {
        if (Shape.GetObject()) {
            IDamageable::Execute_Draw(Shape.GetObject(), 0, 0);
        }
    }
}
```

**UE5 Trait Objects:**
- `TScriptInterface<IMyInterface>` = Blueprint-compatible trait object
- `Execute_MethodName()` = UE5's dynamic dispatch helper
- Checks if object implements interface at runtime

**Performance:** Virtual function call overhead (~1-2 cycles)

### 4.3 Blueprint Integration

**Blueprint Dispatch:**
```cpp
// Blueprint calls interface method
if (Actor->Implements<UDamageable>()) {
    IDamageable::Execute_TakeDamage(Actor, 50.0f);
}
```

**How it works:**
1. `Implements<UDamageable>()` checks if Actor's class implements interface
2. `Execute_TakeDamage()` dispatches to Blueprint if overridden, else C++ `_Implementation`
3. Blueprint VM handles the call if Blueprint overrides exist


---

## 5. Implementation Roadmap

### 5.1 Phase 1: Parser Enhancement (Week 1)

**Goal:** Parse trait definitions and `impl Trait for Type` syntax.

**Tasks:**
1. Add `parse_trait()` method to parser
2. Implement `impl Trait for Type` parsing (set `trait_name` field)
3. Add trait method parsing with optional default implementations
4. Update tests to cover trait syntax

**New Syntax:**
```kain
trait MyTrait:
    fn method1(self, x: Int) -> String
    fn method2(self) -> Bool:
        return true  # Default implementation

impl MyTrait for MyType:
    fn method1(self, x: Int) -> String:
        return "hello"
```

### 5.2 Phase 2: Type System Integration (Week 2)

**Goal:** Validate trait implementations and constraints.

**Tasks:**
1. Add trait registry to `TypeEnv`
2. Implement trait constraint checking
3. Validate impl blocks implement all required methods
4. Check method signatures match trait definitions
5. Support trait bounds on generics

**Type Checking:**
```rust
// In types.rs
pub struct TypeEnv {
    traits: HashMap<String, Trait>,  // NEW
    impls: HashMap<String, Vec<(String, Impl)>>,  // (Type, [(Trait, Impl)])
    // ... existing fields
}

fn check_impl(env: &mut TypeEnv, impl_block: &Impl) -> KainResult<TypedImpl> {
    if let Some(trait_name) = &impl_block.trait_name {
        // Validate trait exists
        let trait_def = env.traits.get(trait_name)
            .ok_or_else(|| KainError::type_error("Trait not found"))?;
        
        // Check all trait methods are implemented
        for trait_method in &trait_def.methods {
            let impl_method = impl_block.methods.iter()
                .find(|m| m.name == trait_method.name)
                .ok_or_else(|| KainError::type_error("Missing trait method"))?;
            
            // Validate signatures match
            check_signature_match(trait_method, impl_method)?;
        }
    }
    Ok(TypedImpl { ast: impl_block.clone() })
}
```

### 5.3 Phase 3: UE5 Codegen (Week 3-4)

**Goal:** Generate UInterface code for UE5 backend.

**Tasks:**
1. Create `ue5/interface.rs` module
2. Implement dual-class generation (I-prefix + U-prefix)
3. Generate UINTERFACE macros with correct specifiers
4. Handle BlueprintNativeEvent method generation
5. Generate `_Implementation` methods for defaults
6. Update actor/struct codegen to inherit interfaces
7. Add interface includes to implementers

**Codegen Structure:**
```rust
// In ue5/interface.rs
pub fn gen_interface(trait_def: &Trait, context: &Ue5Context) -> (String, String) {
    let interface_name = naming::to_interface_name(&trait_def.name);
    let uobject_name = naming::to_uobject_name(&trait_def.name);
    
    let mut header = String::new();
    
    // Generate U-class (UObject wrapper)
    header.push_str(&format!("UINTERFACE(MinimalAPI, Blueprintable)\n"));
    header.push_str(&format!("class {} : public UInterface {{\n", uobject_name));
    header.push_str("    GENERATED_BODY()\n");
    header.push_str("};\n\n");
    
    // Generate I-class (actual interface)
    header.push_str(&format!("class {} {{\n", interface_name));
    header.push_str("    GENERATED_BODY()\n");
    header.push_str("public:\n");
    
    for method in &trait_def.methods {
        gen_interface_method(&mut header, method, context);
    }
    
    header.push_str("};\n");
    
    (header, String::new())  // No .cpp needed for interfaces
}
```


### 5.4 Phase 4: Actor/Struct Integration (Week 5)

**Goal:** Update actor/struct codegen to implement interfaces.

**Tasks:**
1. Detect `impl Trait for Actor` blocks
2. Add interface to actor's inheritance list
3. Generate method implementations with `_Implementation` suffix
4. Include interface headers
5. Handle multiple interface inheritance

**Actor Codegen Updates:**
```rust
// In codegen_ue5.rs
fn gen_actor_header(actor: &Actor, context: &Ue5Context) -> String {
    let mut header = String::new();
    
    // Find all trait impls for this actor
    let interfaces = context.find_trait_impls(&actor.name);
    
    // Generate class declaration with interfaces
    header.push_str(&format!("UCLASS()\nclass A{} : public AActor", actor.name));
    for interface in &interfaces {
        header.push_str(&format!(", public I{}", interface));
    }
    header.push_str(" {\n");
    header.push_str("    GENERATED_BODY()\n");
    
    // ... existing actor code ...
    
    // Generate interface method declarations
    for interface in &interfaces {
        let trait_def = context.get_trait(interface).unwrap();
        for method in &trait_def.methods {
            header.push_str(&format!(
                "    virtual {} {}_Implementation({}) override;\n",
                map_return_type(&method.return_type),
                method.name,
                map_params(&method.params)
            ));
        }
    }
    
    header.push_str("};\n");
    header
}
```

### 5.5 Phase 5: Testing & Validation (Week 6)

**Goal:** Comprehensive test coverage for trait system.

**Test Cases:**
1. Simple trait with single method
2. Trait with multiple methods
3. Trait with default implementations
4. Multiple traits on single type
5. Generic traits
6. Blueprint-callable traits
7. Trait objects (TScriptInterface)
8. Cross-file trait definitions

**Test Plugin:** `testing/Phase3/TraitTest/`
```kain
// traits.kn
@blueprint
trait Damageable:
    fn take_damage(self, amount: Float) -> Bool
    fn is_alive(self) -> Bool
    fn get_max_health(self) -> Float:
        return 100.0

@blueprint
trait Healable:
    fn heal(self, amount: Float)

// actors.kn
actor Player:
    state health: Float = 100.0

impl Damageable for Player:
    fn take_damage(self, amount: Float) -> Bool:
        self.health = self.health - amount
        return self.health > 0.0
    
    fn is_alive(self) -> Bool:
        return self.health > 0.0

impl Healable for Player:
    fn heal(self, amount: Float):
        self.health = min(self.health + amount, self.get_max_health())
```

**Expected Output:**
- `IDamageable.h` with dual-class definition
- `IHealable.h` with dual-class definition
- `APlayer.h` inheriting both interfaces
- `APlayer.cpp` with all `_Implementation` methods
- Compiles in UE5 without errors
- Blueprint can call interface methods


---

## 6. Advanced Features & Challenges

### 6.1 Generic Traits

**KAIN Example:**
```kain
trait Container<T>:
    fn add(self, item: T)
    fn get(self, index: Int) -> T
    fn size(self) -> Int

struct Inventory:
    items: Array<String>

impl Container<String> for Inventory:
    fn add(self, item: String):
        self.items.push(item)
    
    fn get(self, index: Int) -> String:
        return self.items[index]
    
    fn size(self) -> Int:
        return self.items.len()
```

**UE5 Challenge:** UInterface doesn't support templates directly.

**Solution:** Monomorphize at compile time (generate separate interface per type).

**Generated:**
```cpp
// IContainer_String.h
UINTERFACE(MinimalAPI, Blueprintable)
class UContainer_String : public UInterface {
    GENERATED_BODY()
};

class IContainer_String {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void Add(const FString& Item);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FString Get(int32 Index);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int32 Size();
};
```

**Limitation:** Each `Container<T>` instantiation creates a new interface. Blueprint sees `IContainer_String`, `IContainer_Int`, etc. as separate interfaces.

### 6.2 Associated Types

**KAIN Example:**
```kain
trait Iterator:
    type Item
    fn next(self) -> Option<Item>

struct RangeIterator:
    current: Int
    end: Int

impl Iterator for RangeIterator:
    type Item = Int
    
    fn next(self) -> Option<Int>:
        if self.current < self.end:
            let val = self.current
            self.current = self.current + 1
            return Some(val)
        return None
```

**UE5 Challenge:** No direct equivalent to associated types.

**Solution 1:** Monomorphize (same as generic traits)
**Solution 2:** Use `TSubclassOf<UObject>` for type parameters in Blueprint context

**Generated (Monomorphized):**
```cpp
class IIterator_Int {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FOptionalInt Next();  // Custom optional struct
};
```

### 6.3 Multiple Trait Inheritance

**KAIN Example:**
```kain
trait Drawable:
    fn draw(self)

trait Clickable:
    fn on_click(self)

trait Widget: Drawable, Clickable:  # Inherits both
    fn get_bounds(self) -> Rect
```

**UE5 Challenge:** UInterface supports multiple inheritance, but requires careful ordering.

**Generated:**
```cpp
// IWidget.h
UINTERFACE(MinimalAPI, Blueprintable)
class UWidget : public UInterface {
    GENERATED_BODY()
};

class IWidget : public IDrawable, public IClickable {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FRect GetBounds();
};
```

**Implementation:**
```cpp
class AMyWidget : public AActor, public IWidget {
    GENERATED_BODY()
public:
    // Must implement all methods from IDrawable, IClickable, IWidget
    virtual void Draw_Implementation() override;
    virtual void OnClick_Implementation() override;
    virtual FRect GetBounds_Implementation() override;
};
```

**Challenge:** Diamond inheritance problem if two parent traits have same method name.


### 6.4 Trait Bounds on Generics

**KAIN Example:**
```kain
fn process<T: Drawable>(item: T):
    item.draw()

fn process_multiple<T: Drawable + Clickable>(items: Array<T>):
    for item in items:
        item.draw()
        item.on_click()
```

**Type System Requirements:**
1. Parse trait bounds in generic declarations
2. Validate type arguments satisfy bounds
3. Monomorphize functions per concrete type
4. Generate static dispatch calls

**UE5 Generated (Monomorphized):**
```cpp
// For process<Circle>
void Process_Circle(const FCircle& Item) {
    Item.Draw();  // Static dispatch
}

// For process<Square>
void Process_Square(const FSquare& Item) {
    Item.Draw();  // Static dispatch
}
```

**Blueprint Limitation:** Generic functions with trait bounds can't be Blueprint-callable (Blueprint doesn't support templates). Must generate Blueprint-specific wrappers per type.

### 6.5 Trait Objects & Dynamic Dispatch

**KAIN Example:**
```kain
fn render_shapes(shapes: Array<dyn Drawable>):
    for shape in shapes:
        shape.draw()  # Dynamic dispatch
```

**UE5 Generated:**
```cpp
void RenderShapes(const TArray<TScriptInterface<IDrawable>>& Shapes) {
    for (const auto& Shape : Shapes) {
        if (Shape.GetObject() && Shape.GetObject()->Implements<UDrawable>()) {
            IDrawable::Execute_Draw(Shape.GetObject());
        }
    }
}
```

**Key Points:**
- `TScriptInterface<IMyInterface>` = UE5's trait object type
- `Execute_MethodName()` = Dynamic dispatch helper
- Runtime type checking via `Implements<UMyInterface>()`
- Blueprint-compatible (can pass Blueprint objects implementing interface)

**Performance:** ~10-20% slower than static dispatch due to virtual call + type check overhead.

---

## 7. Naming Conventions

### 7.1 UE5 Interface Naming

| KAIN | UE5 I-Class | UE5 U-Class | File |
|------|-------------|-------------|------|
| `trait Damageable` | `IDamageable` | `UDamageable` | `IDamageable.h` |
| `trait MyInterface` | `IMyInterface` | `UMyInterface` | `IMyInterface.h` |
| `trait AI_Controller` | `IAI_Controller` | `UAI_Controller` | `IAI_Controller.h` |

**Rules:**
- Trait name → I-prefix for interface class
- Trait name → U-prefix for UObject wrapper
- File named after I-class
- No separate .cpp file (interfaces are header-only)

### 7.2 Method Naming

| KAIN Method | UE5 Declaration | UE5 Implementation | Blueprint Name |
|-------------|-----------------|-------------------|----------------|
| `fn take_damage` | `TakeDamage` | `TakeDamage_Implementation` | `Take Damage` |
| `fn is_alive` | `IsAlive` | `IsAlive_Implementation` | `Is Alive` |
| `fn get_max_health` | `GetMaxHealth` | `GetMaxHealth_Implementation` | `Get Max Health` |

**Rules:**
- snake_case → PascalCase
- `_Implementation` suffix for BlueprintNativeEvent methods
- Blueprint displays with spaces (auto-converted by UE5)

---

## 8. Error Handling & Diagnostics

### 8.1 Compile-Time Errors

**Missing Method Implementation:**
```kain
trait Damageable:
    fn take_damage(self, amount: Float) -> Bool
    fn is_alive(self) -> Bool

impl Damageable for Player:
    fn take_damage(self, amount: Float) -> Bool:
        return true
    # Missing: is_alive
```

**Error Message:**
```
❌ Type error in player.kn:8:1

   8 | impl Damageable for Player:
     | ^^^^^^^^^^^^^^^^^^^^^^^^^^^
     |
   Trait 'Damageable' requires method 'is_alive' but it is not implemented.
   
   Help: Add the missing method:
         fn is_alive(self) -> Bool:
             # implementation
```


**Signature Mismatch:**
```kain
trait Damageable:
    fn take_damage(self, amount: Float) -> Bool

impl Damageable for Player:
    fn take_damage(self, amount: Int) -> Bool:  # Wrong type
        return true
```

**Error Message:**
```
❌ Type error in player.kn:5:5

   5 |     fn take_damage(self, amount: Int) -> Bool:
     |                          ^^^^^^^^^^^
     |
   Method signature does not match trait definition.
   
   Expected: fn take_damage(self, amount: Float) -> Bool
   Found:    fn take_damage(self, amount: Int) -> Bool
   
   Help: Change parameter type to 'Float' to match trait.
```

**Trait Not Found:**
```kain
impl NonExistent for Player:
    fn method(self):
        pass
```

**Error Message:**
```
❌ Type error in player.kn:1:6

   1 | impl NonExistent for Player:
     |      ^^^^^^^^^^^
     |
   Trait 'NonExistent' is not defined.
   
   Help: Check trait name spelling or import the trait definition.
```

### 8.2 UE5 Compilation Errors

**Missing Interface Include:**
```cpp
// Generated APlayer.h
class APlayer : public AActor, public IDamageable {  // IDamageable not included
    GENERATED_BODY()
};
```

**UE5 Error:**
```
error C2504: 'IDamageable': base class undefined
```

**Fix:** Codegen must add `#include "IDamageable.h"` to actor header.

**Missing _Implementation Method:**
```cpp
// Generated APlayer.h declares but doesn't implement
virtual bool TakeDamage_Implementation(float Amount) override;
```

**UE5 Error:**
```
error LNK2001: unresolved external symbol "public: virtual bool __cdecl APlayer::TakeDamage_Implementation(float)"
```

**Fix:** Codegen must generate method body in .cpp file.

---

## 9. Performance Considerations

### 9.1 Static vs Dynamic Dispatch

| Dispatch Type | Use Case | Performance | Blueprint Support |
|---------------|----------|-------------|-------------------|
| **Static** | Concrete type known | Zero overhead (inlined) | ❌ No (templates) |
| **Dynamic** | Trait object (`dyn Trait`) | ~1-2 cycle overhead | ✅ Yes (TScriptInterface) |

**Recommendation:** Use static dispatch for performance-critical code, dynamic dispatch for Blueprint integration.

### 9.2 Monomorphization Cost

**Code Size Impact:**
- Each generic trait instantiation generates new interface
- `Container<String>`, `Container<Int>`, `Container<Float>` = 3 separate interfaces
- Can lead to code bloat with many type parameters

**Mitigation:**
- Limit generic trait usage
- Use trait objects for collections of different types
- Consider type erasure for Blueprint-facing APIs

### 9.3 Blueprint Call Overhead

**Blueprint → C++ Interface Call:**
1. Blueprint VM dispatch (~5-10 cycles)
2. `Execute_MethodName()` wrapper (~2-3 cycles)
3. Virtual function call (~1-2 cycles)
4. Actual method implementation

**Total:** ~8-15 cycles overhead vs direct C++ call

**Optimization:** Mark performance-critical methods as `BlueprintPure` (no execution pins) or avoid Blueprint exposure entirely.

---

## 10. Future Enhancements

### 10.1 Trait Aliases

**Syntax:**
```kain
trait GameObject = Drawable + Clickable + Serializable

impl GameObject for MyActor:
    # Implements all three traits at once
```

**Benefit:** Reduces boilerplate for common trait combinations.

### 10.2 Conditional Trait Implementation

**Syntax:**
```kain
impl<T: Drawable> Container<T> for Inventory:
    # Only implement if T is Drawable
```

**Benefit:** More flexible generic programming.

### 10.3 Trait Specialization

**Syntax:**
```kain
impl<T> Serializable for T:
    fn serialize(self) -> String:
        return default_serialize(self)

impl Serializable for Player:  # Specialized version
    fn serialize(self) -> String:
        return custom_player_serialize(self)
```

**Benefit:** Default implementations with opt-in specialization.

### 10.4 Async Traits

**Syntax:**
```kain
trait AsyncLoader:
    async fn load(self, path: String) -> Result<Data>

impl AsyncLoader for AssetManager:
    async fn load(self, path: String) -> Result<Data>:
        let data = await fetch_from_disk(path)
        return Ok(data)
```

**UE5 Challenge:** No native async interface support. Would require custom Future/Promise wrapper types.


---

## 11. Implementation Checklist

### Parser (Week 1)
- [ ] Add `parse_trait()` method
- [ ] Parse trait method signatures
- [ ] Parse default method implementations
- [ ] Parse `impl Trait for Type` syntax
- [ ] Update `trait_name` field in Impl AST
- [ ] Add trait parsing tests

### Type System (Week 2)
- [ ] Add trait registry to TypeEnv
- [ ] Implement trait constraint validation
- [ ] Check impl blocks implement all required methods
- [ ] Validate method signatures match trait definitions
- [ ] Support trait bounds on generics
- [ ] Add type system tests for traits

### UE5 Codegen (Week 3-4)
- [ ] Create `ue5/interface.rs` module
- [ ] Generate I-prefix interface class
- [ ] Generate U-prefix UObject wrapper
- [ ] Add UINTERFACE macro with specifiers
- [ ] Generate UFUNCTION macros for methods
- [ ] Handle BlueprintNativeEvent methods
- [ ] Generate `_Implementation` methods for defaults
- [ ] Update actor codegen to inherit interfaces
- [ ] Update struct codegen to inherit interfaces
- [ ] Add interface includes to implementers
- [ ] Handle multiple interface inheritance
- [ ] Add UE5 codegen tests

### Integration (Week 5)
- [ ] Update packager to handle trait files
- [ ] Generate separate interface header files
- [ ] Update master header to include interfaces
- [ ] Handle cross-file trait references
- [ ] Add integration tests

### Testing (Week 6)
- [ ] Create TraitTest plugin
- [ ] Test simple trait with single method
- [ ] Test trait with multiple methods
- [ ] Test trait with default implementations
- [ ] Test multiple traits on single type
- [ ] Test generic traits (monomorphization)
- [ ] Test Blueprint-callable traits
- [ ] Test trait objects (TScriptInterface)
- [ ] Test cross-file trait definitions
- [ ] Compile in UE5 and verify Blueprint integration

### Documentation
- [ ] Update KAIN language guide with trait syntax
- [ ] Add trait examples to cookbook
- [ ] Document UInterface mapping strategy
- [ ] Add performance guidelines
- [ ] Update AI_PLUGIN_CREATION_GUIDE.md

---

## 12. Open Questions

### 12.1 Trait Object Syntax

**Option 1:** Rust-style `dyn Trait`
```kain
fn process(items: Array<dyn Drawable>):
    for item in items:
        item.draw()
```

**Option 2:** Explicit trait object type
```kain
fn process(items: Array<TraitObject<Drawable>>):
    for item in items:
        item.draw()
```

**Recommendation:** Option 1 (more concise, familiar to Rust developers)

### 12.2 Default Implementation Location

**Option 1:** Inline in interface header
```cpp
class IDamageable {
    virtual float GetMaxHealth_Implementation() { return 100.0f; }
};
```

**Option 2:** Separate .cpp file
```cpp
// IDamageable.cpp
float IDamageable::GetMaxHealth_Implementation() {
    return 100.0f;
}
```

**Recommendation:** Option 1 for simple defaults, Option 2 for complex logic (to reduce header bloat)

### 12.3 Blueprint Category Naming

**Option 1:** Use trait name as category
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damageable")
bool TakeDamage(float Amount);
```

**Option 2:** Allow custom category via attribute
```kain
@blueprint
@category("Combat")
trait Damageable:
    fn take_damage(self, amount: Float) -> Bool
```

**Recommendation:** Option 2 (more flexible, better Blueprint organization)

---

## 13. Summary

### Current State
- ✅ Trait AST exists and is parsed (partially)
- ✅ Impl blocks work in interpreter
- ❌ No production codegen for any backend
- ❌ No type system validation

### Implementation Complexity (1-10)
- **Parser:** 3/10 (straightforward syntax extension)
- **Type System:** 6/10 (constraint validation, signature matching)
- **UE5 Codegen:** 9/10 (dual-class system, BlueprintNativeEvent, includes)
- **WASM Codegen:** 5/10 (vtable generation)
- **LLVM Codegen:** 3/10 (native vtable support)
- **Rust Codegen:** 2/10 (nearly 1:1 mapping)

### Estimated Timeline
- **Full Implementation:** 6 weeks (1 developer)
- **MVP (UE5 only, no generics):** 3 weeks
- **Production-Ready (all features):** 8-10 weeks

### Priority Recommendation
**HIGH** - Traits are essential for:
- Polymorphic gameplay systems (damage, interaction, AI)
- Blueprint interface integration
- Reusable component patterns
- Plugin extensibility

Without traits, KAIN users must resort to inheritance (less flexible) or duplicate code (unmaintainable).

---

## 14. References

### UE5 Documentation
- [UInterface Documentation](https://docs.unrealengine.com/5.3/en-US/interfaces-in-unreal-engine/)
- [BlueprintNativeEvent](https://docs.unrealengine.com/5.3/en-US/blueprint-native-events-in-unreal-engine/)
- [TScriptInterface](https://docs.unrealengine.com/5.3/en-US/API/Runtime/CoreUObject/UObject/TScriptInterface/)

### KAIN Codebase
- `crates/kain-core/src/ast.rs:304-332` - Trait AST definitions
- `crates/kain-core/src/parser.rs:174-216` - Impl block parsing
- `crates/kain-core/src/types.rs:62-65` - TypedImpl structure
- `crates/kain-core/src/runtime.rs:1791-1803` - Impl registration

### Related Documents
- `docs/recent/AGENT_HANDOFF.md` - KAIN pipeline overview
- `docs/recent/AI_PLUGIN_CREATION_GUIDE.md` - Plugin development patterns
- `docs/kain-patterns.md` - KAIN language patterns

---

**Document End**
