# Using Generics in KAIN Plugins

**Audience:** Plugin developers  
**Level:** Intermediate  
**Prerequisites:** Basic KAIN syntax, function definitions  
**Estimated Reading Time:** 15 minutes

---

## Table of Contents

1. [Introduction](#introduction)
2. [Generic Functions](#generic-functions)
3. [Generic Structs](#generic-structs)
4. [Generic Methods](#generic-methods)
5. [Using Stdlib Functions](#using-stdlib-functions)
6. [Best Practices](#best-practices)
7. [Common Patterns](#common-patterns)
8. [Limitations and Edge Cases](#limitations-and-edge-cases)
9. [Examples from GenericIntegrationTest](#examples-from-genericintegrationtest)

---

## Introduction

**Generics** allow you to write code that works with multiple types without duplication. Instead of writing separate functions for `Int`, `Float`, and `String`, you write one generic function that works with all of them.

### Why Use Generics?

**Without Generics (Repetitive):**
```kain
fn abs_int(x: Int) -> Int:
    if x < 0:
        return -x
    return x

fn abs_float(x: Float) -> Float:
    if x < 0.0:
        return -x
    return x

// Need to write abs_double, abs_i64, abs_i32, etc.
```

**With Generics (Reusable):**
```kain
fn abs<T>(x: T) -> T:
    if x < 0:
        return -x
    return x

// Works with Int, Float, and any comparable numeric type
let a = abs(-42)        // T = Int
let b = abs(-3.14)      // T = Float
```

### Key Benefits

- ✅ **Code Reuse:** Write once, use with many types
- ✅ **Type Safety:** Compiler ensures type correctness
- ✅ **Zero Runtime Cost:** Monomorphization generates optimized code
- ✅ **Better Organization:** Group related functionality
- ✅ **Stdlib Access:** 47 generic functions available

---

## Generic Functions

### Basic Syntax

```kain
fn function_name<TypeParam>(param: TypeParam) -> TypeParam:
    // Function body
    return param
```

**Components:**
- `<TypeParam>` - Type parameter declaration (can be any name: `T`, `U`, `V`, `Item`, etc.)
- `param: TypeParam` - Parameter using the type parameter
- `-> TypeParam` - Return type using the type parameter

### Simple Example: Identity Function

```kain
fn identity<T>(x: T) -> T:
    return x

// Usage
let int_val = identity(42)           // T = Int
let float_val = identity(3.14)       // T = Float
let string_val = identity("hello")   // T = String
```

**How It Works:**
1. Compiler sees `identity(42)`
2. Infers `T = Int` from argument type
3. Generates `identity_Int(x: Int) -> Int`
4. Replaces call with `identity_Int(42)`

### Multiple Type Parameters

```kain
fn pair<T, U>(first: T, second: U) -> U:
    return second

// Usage
let result = pair(42, "hello")       // T = Int, U = String
let another = pair(3.14, true)       // T = Float, U = Bool
```

### Generic Functions with Operations

```kain
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b

// Usage
let max_int = max(10, 20)            // T = Int, returns 20
let max_float = max(1.5, 2.5)        // T = Float, returns 2.5
```

**Note:** The type `T` must support the `>` operator. The compiler will validate this.

### Nested Generic Calls

```kain
fn abs<T>(x: T) -> T:
    if x < 0:
        return -x
    return x

fn double<T>(x: T) -> T:
    return x + x

// Nested usage
let result = double(abs(-5))         // abs(-5) = 5, double(5) = 10
```

---

## Generic Structs

### Basic Syntax

```kain
struct StructName<TypeParam>:
    field: TypeParam
```

### Example: Generic Container

```kain
struct Box<T>:
    value: T

// Usage
let int_box = Box { value: 42 }              // Box<Int>
let string_box = Box { value: "hello" }      // Box<String>
```

### Multiple Type Parameters

```kain
struct Pair<T, U>:
    first: T
    second: U

// Usage
let pair = Pair { first: 42, second: "hello" }  // Pair<Int, String>
```

### Generic Structs with Methods

```kain
struct Box<T>:
    value: T

impl<T> Box<T>:
    fn get(self) -> T:
        return self.value
    
    fn set(self, new_value: T):
        self.value = new_value

// Usage
let box = Box { value: 42 }
let val = box.get()                  // Returns 42
box.set(100)                         // Sets value to 100
```

---

## Generic Methods

### Syntax

```kain
impl<T> StructName<T>:
    fn method_name(self, param: T) -> T:
        // Method body
        return param
```

### Example: Generic Stack

```kain
struct Stack<T>:
    items: Array<T>
    count: Int

impl<T> Stack<T>:
    fn new() -> Stack<T>:
        return Stack { items: [], count: 0 }
    
    fn push(self, item: T):
        self.items.append(item)
        self.count = self.count + 1
    
    fn pop(self) -> T:
        if self.count > 0:
            self.count = self.count - 1
            return self.items[self.count]
        return None
    
    fn is_empty(self) -> Bool:
        return self.count == 0

// Usage
let int_stack = Stack<Int>::new()
int_stack.push(1)
int_stack.push(2)
let val = int_stack.pop()            // Returns 2
```

### Generic Methods with Additional Type Parameters

```kain
impl<T> Box<T>:
    fn map<U>(self, f: fn(T) -> U) -> Box<U>:
        return Box { value: f(self.value) }

// Usage
let int_box = Box { value: 42 }
let string_box = int_box.map(fn(x: Int) -> String:
    return f"Value: {x}"
)
```

---

## Using Stdlib Functions

KAIN provides **47 generic stdlib functions** across 4 categories:

### Math Functions (20 functions)

```kain
// Absolute value
let a = abs(-42)                     // 42
let b = abs(-3.14)                   // 3.14

// Min/Max
let min_val = min(10, 20)            // 10
let max_val = max(10, 20)            // 20

// Clamping
let clamped = clamp(150, 0, 100)     // 100

// Rounding (Float only)
let rounded = round(3.7)             // 4.0
let floored = floor(3.7)             // 3.0
let ceiled = ceil(3.2)               // 4.0

// Power and roots
let squared = pow(5.0, 2.0)          // 25.0
let cubed = pow(2.0, 3.0)            // 8.0
let root = sqrt(16.0)                // 4.0

// Trigonometry
let sine = sin(1.57)                 // ~1.0
let cosine = cos(0.0)                // 1.0
let tangent = tan(0.785)             // ~1.0

// Exponential
let exp_val = exp(1.0)               // ~2.718
let log_val = log(2.718)             // ~1.0
let log10_val = log10(100.0)         // 2.0
```

### Vector Functions (10 functions)

```kain
// Vector creation
let v = vec3(1.0, 2.0, 3.0)

// Vector operations
let length = vec3_length(v)          // Magnitude
let normalized = vec3_normalize(v)   // Unit vector
let dot_prod = vec3_dot(v, v)        // Dot product
let cross_prod = vec3_cross(v, vec3(0, 1, 0))  // Cross product

// Distance
let dist = vec3_distance(v, vec3(0, 0, 0))

// Lerp (linear interpolation)
let lerped = vec3_lerp(vec3(0, 0, 0), vec3(10, 10, 10), 0.5)  // (5, 5, 5)
```

### Collection Functions (12 functions)

```kain
// Array operations
let arr = [1, 2, 3, 4, 5]

let length = array_length(arr)       // 5
let first = array_first(arr)         // 1
let last = array_last(arr)           // 5
let contains = array_contains(arr, 3)  // true

// Slicing
let slice = array_slice(arr, 1, 3)   // [2, 3]

// Mapping
let doubled = array_map(arr, fn(x: Int) -> Int:
    return x * 2
)  // [2, 4, 6, 8, 10]

// Filtering
let evens = array_filter(arr, fn(x: Int) -> Bool:
    return x % 2 == 0
)  // [2, 4]

// Reducing
let sum = array_reduce(arr, 0, fn(acc: Int, x: Int) -> Int:
    return acc + x
)  // 15

// Sorting
let sorted = array_sort(arr)         // [1, 2, 3, 4, 5]
let reversed = array_reverse(arr)    // [5, 4, 3, 2, 1]
```

### String Functions (15 functions)

```kain
let text = "Hello, World!"

// Length and access
let len = string_length(text)        // 13
let char = string_char_at(text, 0)   // "H"

// Substrings
let sub = string_substring(text, 0, 5)  // "Hello"
let slice = string_slice(text, 7, 12)   // "World"

// Search
let index = string_index_of(text, "World")  // 7
let contains = string_contains(text, "Hello")  // true
let starts = string_starts_with(text, "Hello")  // true
let ends = string_ends_with(text, "!")  // true

// Transformation
let upper = string_to_upper(text)    // "HELLO, WORLD!"
let lower = string_to_lower(text)    // "hello, world!"
let trimmed = string_trim("  hello  ")  // "hello"

// Splitting and joining
let parts = string_split(text, ", ")  // ["Hello", "World!"]
let joined = string_join(parts, " - ")  // "Hello - World!"

// Replacement
let replaced = string_replace(text, "World", "KAIN")  // "Hello, KAIN!"
```

---

## Best Practices

### 1. Use Descriptive Type Parameter Names

```kain
// ❌ Not descriptive
fn process<T, U, V>(a: T, b: U) -> V:
    // What are T, U, V?

// ✅ Descriptive
fn convert<Input, Output>(value: Input) -> Output:
    // Clear what each type represents
```

### 2. Keep Generic Functions Simple

```kain
// ❌ Too complex
fn complex<T>(a: T, b: T, c: T, d: T) -> T:
    // 50 lines of logic
    // Hard to understand and maintain

// ✅ Simple and focused
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b
```

### 3. Use Stdlib Functions When Possible

```kain
// ❌ Reinventing the wheel
fn my_abs<T>(x: T) -> T:
    if x < 0:
        return -x
    return x

// ✅ Use stdlib
let result = abs(x)
```

### 4. Document Type Requirements

```kain
// ✅ Clear documentation
// Compares two values and returns the larger one.
// Type T must support the > operator.
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b
```

### 5. Avoid Over-Generalization

```kain
// ❌ Too generic (loses type safety)
fn process<T>(x: T) -> T:
    // Works with ANY type, even when it shouldn't

// ✅ Specific when needed
fn process_number(x: Float) -> Float:
    // Only works with Float, as intended
```

---

## Common Patterns

### Pattern 1: Generic Utility Functions

```kain
// Swap two values
fn swap<T>(a: T, b: T) -> (T, T):
    return (b, a)

// Identity function
fn identity<T>(x: T) -> T:
    return x

// Constant function
fn constant<T, U>(x: T, y: U) -> T:
    return x
```

### Pattern 2: Generic Containers

```kain
struct Container<T>:
    value: T
    metadata: String

impl<T> Container<T>:
    fn new(val: T) -> Container<T>:
        return Container { value: val, metadata: "" }
    
    fn get(self) -> T:
        return self.value
    
    fn set(self, new_val: T):
        self.value = new_val
```

### Pattern 3: Generic Algorithms

```kain
// Map function
fn map<T, U>(arr: Array<T>, f: fn(T) -> U) -> Array<U>:
    var result: Array<U> = []
    for item in arr:
        result.append(f(item))
    return result

// Filter function
fn filter<T>(arr: Array<T>, predicate: fn(T) -> Bool) -> Array<T>:
    var result: Array<T> = []
    for item in arr:
        if predicate(item):
            result.append(item)
    return result
```

### Pattern 4: Generic Wrappers for UE5

```kain
// Blueprint-callable wrappers for generic functions
@blueprint
fn blueprint_max_int(a: Int, b: Int) -> Int:
    return max(a, b)

@blueprint
fn blueprint_max_float(a: Float, b: Float) -> Float:
    return max(a, b)

@blueprint
fn blueprint_clamp_int(x: Int, lo: Int, hi: Int) -> Int:
    return clamp(x, lo, hi)
```

### Pattern 5: Generic State Management

```kain
actor GenericManager<T>:
    state items: Array<T> = []
    state count: Int = 0
    
    on BeginPlay():
        println("Manager initialized")
    
    on Server_AddItem(item: T):
        items.append(item)
        count = count + 1
        Multicast_OnItemAdded(item)
    
    on Multicast_OnItemAdded(item: T):
        println(f"Item added: {item}")
```

---

## Limitations and Edge Cases

### Limitation 1: No Explicit Type Arguments (Yet)

```kain
// ❌ Not yet supported
let result = identity<Int>(42)

// ✅ Use type inference
let result: Int = identity(42)
```

**Workaround:** Use type annotations on variables.

### Limitation 2: No Trait Bounds (Yet)

```kain
// ❌ Not yet supported
fn compare<T: Comparable>(a: T, b: T) -> Bool:
    return a > b

// ✅ Compiler infers requirements
fn compare<T>(a: T, b: T) -> Bool:
    return a > b  // Compiler checks if T supports >
```

**Workaround:** Compiler validates operations automatically.

### Limitation 3: Generic Actors Need Concrete Types

```kain
// ❌ Cannot instantiate generic actor directly in UE5
actor GenericActor<T>:
    state value: T

// ✅ Create concrete versions
actor IntActor:
    state value: Int

actor FloatActor:
    state value: Float
```

**Reason:** UE5 reflection system requires concrete types.

### Edge Case 1: Type Inference with None

```kain
// ❌ Ambiguous
let x = identity(None)  // What type is None?

// ✅ Explicit type
let x: Int = identity(None)  // Now T = Int
```

### Edge Case 2: Nested Generics

```kain
// ✅ Works
fn process<T>(arr: Array<T>) -> T:
    return arr[0]

let result = process([1, 2, 3])  // T = Int

// ✅ Also works
fn nested<T>(arr: Array<Array<T>>) -> T:
    return arr[0][0]

let result = nested([[1, 2], [3, 4]])  // T = Int
```

### Edge Case 3: Generic Methods with Self

```kain
struct Box<T>:
    value: T

impl<T> Box<T>:
    // ✅ Self refers to Box<T>
    fn clone(self) -> Self:
        return Box { value: self.value }
```

---

## Examples from GenericIntegrationTest

### Example 1: Math Utilities

```kain
// From testing/generics/GenericMath.kn

fn abs<T>(x: T) -> T:
    if x < 0:
        return -x
    return x

fn clamp<T>(x: T, lo: T, hi: T) -> T:
    return min(max(x, lo), hi)

// Usage in actor
actor MathTester:
    on BeginPlay():
        let a = abs(-42)              // 42
        let b = clamp(150, 0, 100)    // 100
        println(f"abs(-42) = {a}")
        println(f"clamp(150, 0, 100) = {b}")
```

### Example 2: Generic Container

```kain
struct Box<T>:
    value: T

impl<T> Box<T>:
    fn get(self) -> T:
        return self.value
    
    fn set(self, new_value: T):
        self.value = new_value

// Usage
let int_box = Box { value: 42 }
let val = int_box.get()
int_box.set(100)
```

### Example 3: Blueprint Integration

```kain
// Generic function
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b

// Blueprint-callable wrappers
@blueprint
fn blueprint_max_int(a: Int, b: Int) -> Int:
    return max(a, b)

@blueprint
fn blueprint_max_float(a: Float, b: Float) -> Float:
    return max(a, b)
```

### Example 4: Nested Generic Calls

```kain
fn abs<T>(x: T) -> T:
    if x < 0:
        return -x
    return x

fn double<T>(x: T) -> T:
    return x + x

// Nested usage
actor Tester:
    on BeginPlay():
        let result = double(abs(-5))  // abs(-5) = 5, double(5) = 10
        println(f"double(abs(-5)) = {result}")
```

---

## Quick Tips

1. **Start Simple:** Begin with basic generic functions before moving to structs and methods
2. **Use Stdlib:** Leverage the 47 built-in generic functions
3. **Type Annotations:** Add type annotations when inference is ambiguous
4. **Blueprint Wrappers:** Create concrete wrappers for Blueprint integration
5. **Test Incrementally:** Test each generic function with multiple types
6. **Document Requirements:** Clearly document what operations types must support
7. **Avoid Over-Engineering:** Don't make everything generic; use concrete types when appropriate

---

## Next Steps

1. **Read:** `docs/guides/GENERICS_QUICK_REFERENCE.md` for syntax cheat sheet
2. **Explore:** `testing/generics/GenericMath.kn` for complete examples
3. **Experiment:** Try writing your own generic functions
4. **Build:** Create a plugin using generic utilities
5. **Share:** Contribute generic utilities to the stdlib

---

## Additional Resources

- **Monomorphization Details:** `docs/recent/MONOMORPHIZATION_INTEGRATION.md`
- **Stdlib Functions:** `docs/stdlib/STDLIB_MATH_FUNCTIONS.md`
- **Test Results:** `testing/generics/TEST_RESULTS.md`
- **KAIN Patterns:** `.kiro/kain-patterns.md`

---

**Happy coding with generics! 🚀**
