# KAIN Generics Quick Reference

**Quick lookup for generic syntax, patterns, and stdlib functions**

---

## Syntax Cheat Sheet

### Generic Function
```kain
fn name<T>(param: T) -> T:
    return param
```

### Multiple Type Parameters
```kain
fn name<T, U>(a: T, b: U) -> U:
    return b
```

### Generic Struct
```kain
struct Name<T>:
    field: T
```

### Generic Impl
```kain
impl<T> Name<T>:
    fn method(self) -> T:
        return self.field
```

### Generic Method with Additional Type Param
```kain
impl<T> Name<T>:
    fn map<U>(self, f: fn(T) -> U) -> Name<U>:
        return Name { field: f(self.field) }
```

---

## Common Patterns

### Identity
```kain
fn identity<T>(x: T) -> T:
    return x
```

### Swap
```kain
fn swap<T, U>(a: T, b: U) -> (U, T):
    return (b, a)
```

### Min/Max
```kain
fn min<T>(a: T, b: T) -> T:
    if a < b:
        return a
    return b

fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b
```

### Clamp
```kain
fn clamp<T>(x: T, lo: T, hi: T) -> T:
    return min(max(x, lo), hi)
```

### Container
```kain
struct Box<T>:
    value: T

impl<T> Box<T>:
    fn get(self) -> T:
        return self.value
    
    fn set(self, val: T):
        self.value = val
```

### Map
```kain
fn map<T, U>(arr: Array<T>, f: fn(T) -> U) -> Array<U>:
    var result: Array<U> = []
    for item in arr:
        result.append(f(item))
    return result
```

### Filter
```kain
fn filter<T>(arr: Array<T>, pred: fn(T) -> Bool) -> Array<T>:
    var result: Array<T> = []
    for item in arr:
        if pred(item):
            result.append(item)
    return result
```

---

## Stdlib Functions (47 Total)

### Math Functions (20)

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs` | `fn<T>(x: T) -> T` | Absolute value |
| `min` | `fn<T>(a: T, b: T) -> T` | Minimum of two values |
| `max` | `fn<T>(a: T, b: T) -> T` | Maximum of two values |
| `clamp` | `fn<T>(x: T, lo: T, hi: T) -> T` | Clamp value between bounds |
| `pow` | `fn(base: Float, exp: Float) -> Float` | Power |
| `sqrt` | `fn(x: Float) -> Float` | Square root |
| `exp` | `fn(x: Float) -> Float` | Exponential (e^x) |
| `log` | `fn(x: Float) -> Float` | Natural logarithm |
| `log10` | `fn(x: Float) -> Float` | Base-10 logarithm |
| `sin` | `fn(x: Float) -> Float` | Sine |
| `cos` | `fn(x: Float) -> Float` | Cosine |
| `tan` | `fn(x: Float) -> Float` | Tangent |
| `asin` | `fn(x: Float) -> Float` | Arcsine |
| `acos` | `fn(x: Float) -> Float` | Arccosine |
| `atan` | `fn(x: Float) -> Float` | Arctangent |
| `atan2` | `fn(y: Float, x: Float) -> Float` | Two-argument arctangent |
| `floor` | `fn(x: Float) -> Float` | Round down |
| `ceil` | `fn(x: Float) -> Float` | Round up |
| `round` | `fn(x: Float) -> Float` | Round to nearest |
| `trunc` | `fn(x: Float) -> Float` | Truncate decimal |

### Vector Functions (10)

| Function | Signature | Description |
|----------|-----------|-------------|
| `vec3` | `fn(x: Float, y: Float, z: Float) -> Vec3` | Create vector |
| `vec3_length` | `fn(v: Vec3) -> Float` | Vector magnitude |
| `vec3_normalize` | `fn(v: Vec3) -> Vec3` | Unit vector |
| `vec3_dot` | `fn(a: Vec3, b: Vec3) -> Float` | Dot product |
| `vec3_cross` | `fn(a: Vec3, b: Vec3) -> Vec3` | Cross product |
| `vec3_distance` | `fn(a: Vec3, b: Vec3) -> Float` | Distance between points |
| `vec3_lerp` | `fn(a: Vec3, b: Vec3, t: Float) -> Vec3` | Linear interpolation |
| `vec3_add` | `fn(a: Vec3, b: Vec3) -> Vec3` | Vector addition |
| `vec3_sub` | `fn(a: Vec3, b: Vec3) -> Vec3` | Vector subtraction |
| `vec3_scale` | `fn(v: Vec3, s: Float) -> Vec3` | Scalar multiplication |

### Collection Functions (12)

| Function | Signature | Description |
|----------|-----------|-------------|
| `array_length` | `fn<T>(arr: Array<T>) -> Int` | Array length |
| `array_first` | `fn<T>(arr: Array<T>) -> T` | First element |
| `array_last` | `fn<T>(arr: Array<T>) -> T` | Last element |
| `array_contains` | `fn<T>(arr: Array<T>, item: T) -> Bool` | Check if contains |
| `array_slice` | `fn<T>(arr: Array<T>, start: Int, end: Int) -> Array<T>` | Slice array |
| `array_map` | `fn<T, U>(arr: Array<T>, f: fn(T) -> U) -> Array<U>` | Map function |
| `array_filter` | `fn<T>(arr: Array<T>, pred: fn(T) -> Bool) -> Array<T>` | Filter array |
| `array_reduce` | `fn<T, U>(arr: Array<T>, init: U, f: fn(U, T) -> U) -> U` | Reduce array |
| `array_sort` | `fn<T>(arr: Array<T>) -> Array<T>` | Sort array |
| `array_reverse` | `fn<T>(arr: Array<T>) -> Array<T>` | Reverse array |
| `array_append` | `fn<T>(arr: Array<T>, item: T) -> Array<T>` | Append element |
| `array_concat` | `fn<T>(a: Array<T>, b: Array<T>) -> Array<T>` | Concatenate arrays |

### String Functions (15)

| Function | Signature | Description |
|----------|-----------|-------------|
| `string_length` | `fn(s: String) -> Int` | String length |
| `string_char_at` | `fn(s: String, index: Int) -> String` | Character at index |
| `string_substring` | `fn(s: String, start: Int, end: Int) -> String` | Substring |
| `string_slice` | `fn(s: String, start: Int, end: Int) -> String` | Slice string |
| `string_index_of` | `fn(s: String, sub: String) -> Int` | Find substring |
| `string_contains` | `fn(s: String, sub: String) -> Bool` | Check if contains |
| `string_starts_with` | `fn(s: String, prefix: String) -> Bool` | Check prefix |
| `string_ends_with` | `fn(s: String, suffix: String) -> Bool` | Check suffix |
| `string_to_upper` | `fn(s: String) -> String` | Convert to uppercase |
| `string_to_lower` | `fn(s: String) -> String` | Convert to lowercase |
| `string_trim` | `fn(s: String) -> String` | Trim whitespace |
| `string_split` | `fn(s: String, delim: String) -> Array<String>` | Split string |
| `string_join` | `fn(arr: Array<String>, sep: String) -> String` | Join strings |
| `string_replace` | `fn(s: String, old: String, new: String) -> String` | Replace substring |
| `string_repeat` | `fn(s: String, count: Int) -> String` | Repeat string |

---

## Usage Examples

### Math
```kain
let a = abs(-42)                     // 42
let b = min(10, 20)                  // 10
let c = max(10, 20)                  // 20
let d = clamp(150, 0, 100)           // 100
let e = sqrt(16.0)                   // 4.0
let f = pow(2.0, 3.0)                // 8.0
```

### Vectors
```kain
let v = vec3(1.0, 2.0, 3.0)
let len = vec3_length(v)             // ~3.74
let norm = vec3_normalize(v)         // Unit vector
let dot = vec3_dot(v, v)             // 14.0
```

### Collections
```kain
let arr = [1, 2, 3, 4, 5]
let len = array_length(arr)          // 5
let first = array_first(arr)         // 1
let doubled = array_map(arr, fn(x: Int) -> Int:
    return x * 2
)  // [2, 4, 6, 8, 10]
```

### Strings
```kain
let text = "Hello, World!"
let len = string_length(text)        // 13
let upper = string_to_upper(text)    // "HELLO, WORLD!"
let parts = string_split(text, ", ") // ["Hello", "World!"]
```

---

## Type Inference Rules

### From Literals
```kain
identity(42)        // T = Int
identity(3.14)      // T = Float
identity("hello")   // T = String
identity(true)      // T = Bool
```

### From Variables
```kain
let x: Int = 42
identity(x)         // T = Int
```

### From Operations
```kain
fn double<T>(x: T) -> T:
    return x + x

double(5)           // T = Int (5 is Int)
double(2.5)         // T = Float (2.5 is Float)
```

### From Return Type
```kain
let x: Float = identity(42)  // T = Float (coerced)
```

---

## Name Mangling

| Generic Call | Inferred Types | Mangled Name |
|--------------|----------------|--------------|
| `identity(42)` | `Int` | `identity_Int` |
| `identity(3.14)` | `Float` | `identity_Float` |
| `max(10, 20)` | `Int` | `max_Int` |
| `pair(42, "hi")` | `Int, String` | `pair_Int_String` |
| `map([1,2], f)` | `Int, Float` | `map_Int_Float` |

---

## Blueprint Integration

### Pattern: Concrete Wrappers
```kain
// Generic function
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b

// Blueprint wrappers
@blueprint
fn blueprint_max_int(a: Int, b: Int) -> Int:
    return max(a, b)

@blueprint
fn blueprint_max_float(a: Float, b: Float) -> Float:
    return max(a, b)
```

---

## Common Errors

### Error: "Generic function X not found"
```kain
// ❌ Wrong
let a = unknown_func(42)

// ✅ Correct
fn my_func<T>(x: T) -> T:
    return x
let a = my_func(42)
```

### Error: "Cannot infer type arguments"
```kain
// ❌ Ambiguous
let x = identity(None)

// ✅ Explicit type
let x: Int = identity(None)
```

### Error: "Type does not support operation"
```kain
// ❌ Wrong (String doesn't support >)
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b

let x = max("hello", "world")

// ✅ Use appropriate operation
fn max_string(a: String, b: String) -> String:
    if string_length(a) > string_length(b):
        return a
    return b
```

---

## Performance Tips

1. **Inline Small Functions:** Compiler inlines monomorphized functions
2. **Avoid Over-Instantiation:** Don't use generics with 100+ types
3. **Use Concrete Types for Actors:** UE5 reflection requires concrete types
4. **Leverage Stdlib:** Built-in functions are optimized
5. **Profile Before Optimizing:** Measure actual performance impact

---

## Limitations

- ❌ No explicit type arguments yet: `identity<Int>(42)`
- ❌ No trait bounds yet: `fn compare<T: Comparable>(a: T, b: T)`
- ❌ No generic actors in UE5 (use concrete types)
- ❌ No higher-kinded types: `fn map<F<_>>(x: F<Int>)`

---

## Next Steps

1. **Read Full Guide:** `docs/guides/USING_GENERICS_IN_PLUGINS.md`
2. **Explore Examples:** `testing/generics/GenericMath.kn`
3. **Check Integration:** `docs/recent/MONOMORPHIZATION_INTEGRATION.md`
4. **Build Plugin:** Create your own generic utilities

---

**Quick Reference Version 1.0 | February 2026**
