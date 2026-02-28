# KAIN Error Message Examples

This document provides before/after examples for the most common KAIN errors, focusing on effect system violations and shader operation errors.

---

## Effect System Errors

### Example 1: Pure Function Calling IO Function

**Before (Error):**
```
Effect violation: {Pure} cannot call {IO}
```

**After (Improved):**
```
Effect violation: Pure function 'calculate_score' cannot call IO function 'load_config'.

Effect System Rules:
  • Pure functions: No side effects, can only call Pure functions
  • IO functions: Can perform I/O (file/network/console), can call Pure or IO functions
  • Async functions: Can perform async operations, can call Pure, IO, or Async functions
  • GPU functions: Run on graphics hardware, can call Pure or GPU functions
  • Unsafe functions: Can break safety guarantees, can call any function

Current situation:
  • Caller 'calculate_score' is marked as Pure
  • Callee 'load_config' is marked as IO

How to fix:
  1. Add effect annotation to caller: fn calculate_score() -> RetType with IO
  2. OR mark callee as Pure if it has no side effects
  3. OR change your call chain to avoid mixing incompatible effects
```

**Code Example:**
```kain
# ❌ INVALID
fn load_config() -> String with IO:
    return read_file("config.txt")

fn calculate_score(base: Int) -> Int with Pure:
    let config = load_config()  # ERROR: Pure cannot call IO
    return base * 2

# ✅ FIXED
fn calculate_score(base: Int) -> Int with IO:  # Changed to IO
    let config = load_config()  # OK: IO can call IO
    return base * 2
```

---

### Example 2: IO Function Calling Async Function

**Code Example:**
```kain
# ❌ INVALID
async fn fetch_data() -> String with Async:
    return await http_get("https://api.example.com")

fn process_data() -> String with IO:
    let data = fetch_data()  # ERROR: IO cannot call Async
    return data

# ✅ FIXED
async fn process_data() -> String with Async:  # Changed to Async
    let data = await fetch_data()  # OK: Async can call Async
    return data
```

---

### Example 3: GPU Shader Calling IO Function

**Code Example:**
```kain
# ❌ INVALID
fn load_texture() -> Vec4 with IO:
    return read_texture("albedo.png")

shader fragment MyShader(uv: Vec2) -> Vec4:
    let color = load_texture()  # ERROR: GPU cannot call IO
    return color

# ✅ FIXED
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform albedo: Sampler2D @0  # Pass texture as uniform
    let color = sample(albedo, uv)
    return color
```

---

### Example 4: Pure Calling Pure (Valid)

**Code Example:**
```kain
# ✅ VALID
fn square(x: Int) -> Int with Pure:
    return x * x

fn sum_of_squares(a: Int, b: Int) -> Int with Pure:
    return square(a) + square(b)  # OK: Pure can call Pure
```

---

### Example 5: Unsafe Calling Anything (Valid)

**Code Example:**
```kain
# ✅ VALID
fn dangerous_operation() -> Int with Unsafe:
    let data = read_file("data.bin")  # OK: Unsafe can call IO
    let result = await process(data)   # OK: Unsafe can call Async
    return result
```

---

## Shader Error Examples

### Example 1: Invalid Cast - Dimension Mismatch (Vec2 → Vec3)

**Before (Error):**
```
Invalid cast from 'Vec2' to 'Vec3' in shader. Casts must be within same dimension...
```

**After (Improved):**
```
Invalid cast from 'Vec2' to 'Vec3' in shader.

Cast Rules:
  • Casts must be within same dimension category
  • Scalars: Int, UInt, Float, Bool (can cast between these)
  • 2D vectors: Vec2, IVec2, UVec2 (can cast between these)
  • 3D vectors: Vec3, IVec3, UVec3 (can cast between these)
  • 4D vectors: Vec4, IVec4, UVec4 (can cast between these)

What you tried: Vec2 as Vec3
Problem: Dimension mismatch - cannot cast between different dimensions

How to fix: Use swizzling: my_vec.xy or my_vec.xyz, or constructor: Vec3(my_vec.x, my_vec.y, ...)

Valid cast examples:
  • let x: Float = 3.14
    let y = x as Int              # OK: scalar to scalar
  
  • let v: Vec3 = vec3(1.0, 2.0, 3.0)
    let u = v as IVec3            # OK: Vec3 to IVec3 (same dimension)
  
  • let v2: Vec2 = vec2(1.0, 2.0)
    let v3 = Vec3(v2.x, v2.y, 0.0) # OK: use constructor for dimension change
    # NOT: let v3 = v2 as Vec3    # ERROR: dimension mismatch
```

**Code Example:**
```kain
# ❌ INVALID
shader compute Process(thread_id: Vec3):
    let uv: Vec2 = vec2(0.5, 0.5)
    let pos: Vec3 = uv as Vec3  # ERROR: dimension mismatch

# ✅ FIXED
shader compute Process(thread_id: Vec3):
    let uv: Vec2 = vec2(0.5, 0.5)
    let pos: Vec3 = vec3(uv.x, uv.y, 0.0)  # Use constructor
```

---

### Example 2: Empty Array Literal

**Before (Error):**
```
Empty array literals not supported in shaders
```

**After (Improved):**
```
Empty array literals not supported in shaders.

Problem: HLSL requires arrays to have known size and initial values at compile time.

How to fix:
  1. Provide at least one element: [0.0] or [vec3(0.0, 0.0, 0.0)]
  2. Use a buffer instead: uniform my_data: RWBuffer<Float> @0
  3. Use a fixed-size array with explicit initialization

Examples:
  ❌ let arr = []                           # ERROR: empty array
  ✅ let arr = [0.0]                        # OK: single element
  ✅ let arr = [1.0, 2.0, 3.0]              # OK: multiple elements
  ✅ uniform data: RWBuffer<Float> @0       # OK: dynamic buffer
```

**Code Example:**
```kain
# ❌ INVALID
shader fragment ColorGrade(uv: Vec2) -> Vec4:
    let lut = []  # ERROR: empty array not supported
    return vec4(1.0, 1.0, 1.0, 1.0)

# ✅ FIXED - Option 1: Provide values
shader fragment ColorGrade(uv: Vec2) -> Vec4:
    let lut = [0.0, 0.5, 1.0]  # Provide initial values
    return vec4(lut[0], lut[1], lut[2], 1.0)

# ✅ FIXED - Option 2: Use buffer
shader fragment ColorGrade(uv: Vec2) -> Vec4:
    uniform lut: RWBuffer<Float> @0  # Dynamic buffer
    return vec4(lut[0], lut[1], lut[2], 1.0)
```

---

### Example 3: String Type in Shader Uniform

**Before (Error):**
```
Shader 'TextRender': Uniform 'text' has String type. String types are not supported in shaders.
```

**After (Improved):**
```
Shader 'TextRender': Uniform 'text' has String type.

Problem: String types cannot be passed to GPU shaders.
  • Shaders run on GPU hardware which doesn't support dynamic strings
  • HLSL has no string type - only numeric types, vectors, matrices, textures, buffers

How to fix:
  1. Use numeric indices instead of strings
  2. Use texture/sampler types directly
  3. For text rendering, use texture atlases with character indices
  4. For debug output, use numeric codes

Valid shader uniform types:
  • Scalars: Int, UInt, Float, Bool
  • Vectors: Vec2, Vec3, Vec4 (and IVec*, UVec* variants)
  • Matrices: Mat2, Mat3, Mat4
  • Textures: Texture2D, Texture3D, TextureCube, Sampler2D
  • Buffers: Buffer<T>, RWBuffer<T>, StructuredBuffer<T>
  • User-defined POD structs (no strings inside)
```

**Code Example:**
```kain
# ❌ INVALID
shader fragment TextRender(uv: Vec2) -> Vec4:
    uniform text: String @0  # ERROR: strings not supported in shaders
    return vec4(1.0, 1.0, 1.0, 1.0)

# ✅ FIXED - Use texture atlas + numeric index
shader fragment TextRender(uv: Vec2) -> Vec4:
    uniform font_atlas: Texture2D @0  # Texture atlas
    uniform char_index: Int @1        # Numeric index
    let char_uv = get_char_uv(char_index)
    return sample(font_atlas, char_uv)
```

---

### Example 4: Modulo Operation on Float

**Before (Error):**
```
Unsupported binary op in USF
```

**After (Improved):**
```
Unsupported operation '%' in shader.

Supported shader operations:
  Arithmetic: +, -, *, / (all types), % (integers only)
  Comparison: ==, !=, <, >, <=, >= (scalars and vectors)
  Logical: &&, ||, ! (booleans only)
  Bitwise: &, |, ^, <<, >> (integers only)

Common restrictions:
  • Modulo (%) requires integer types (Int, UInt, IVec2, etc.)
  • Bitwise ops require integer types
  • Logical ops require Bool type

Suggestions:
  • For modulo on floats: use fmod(a, b) function
```

**Code Example:**
```kain
# ❌ INVALID
shader fragment Pattern(uv: Vec2) -> Vec4:
    let x = uv.x % 0.5  # ERROR: modulo requires integers
    return vec4(x, x, x, 1.0)

# ✅ FIXED
shader fragment Pattern(uv: Vec2) -> Vec4:
    let x = fmod(uv.x, 0.5)  # Use fmod() for floats
    return vec4(x, x, x, 1.0)
```

---

### Example 5: Complex Function Call (Function Pointers)

**Before (Error):**
```
Complex callee not supported in USF
```

**After (Improved):**
```
Complex function call expression not supported in shaders.

Problem: Shaders only support direct function calls, not computed function pointers or closures.

Supported:
  ✅ my_function(arg1, arg2)           # Direct function call
  ✅ dot(vec_a, vec_b)                 # Built-in function
  ✅ MyStruct::static_method(arg)      # Static method call

Not supported:
  ❌ let func = my_function            # Function pointers
  ❌ func(arg1, arg2)                  # Indirect call
  ❌ callbacks[index](arg)             # Function arrays
  ❌ obj.method_ptr()(arg)             # Method pointers

How to fix:
  • Use direct function calls with explicit names
  • Replace function pointers with if/else or match statements
```

**Code Example:**
```kain
# ❌ INVALID
shader compute Process(thread_id: Vec3):
    let operations = [sqrt, abs, floor]  # ERROR: function arrays not supported
    let result = operations[0](5.0)

# ✅ FIXED - Use match statement
shader compute Process(thread_id: Vec3):
    let op_index = 0
    let result = match op_index:
        0 => sqrt(5.0)
        1 => abs(5.0)
        2 => floor(5.0)
        _ => 0.0
```

---

## Summary

### Effect System Error Pattern
```
Effect violation: <caller_effect> function '<caller_name>' cannot call <callee_effect> function '<callee_name>'.

Effect System Rules:
  • [List of all effect types and their rules]

Current situation:
  • Caller '<name>' is marked as <effect>
  • Callee '<name>' is marked as <effect>

How to fix:
  1. [Specific fix option 1]
  2. [Specific fix option 2]
  3. [Specific fix option 3]

Example (before):
  [Code showing the error]

Example (after):
  [Code showing the fix]
```

### Shader Error Pattern
```
<Error type> in shader.

Problem: [Clear explanation of what went wrong]

[Rules/Restrictions section]

What you tried: [User's code pattern]
Problem: [Specific issue]

How to fix: [Concrete suggestions]

Examples:
  ❌ [Invalid code]
  ✅ [Valid code]
```

---

## Testing Checklist

- [ ] Effect system errors show function names
- [ ] Effect system errors list all effect types
- [ ] Effect system errors provide 3+ fix options
- [ ] Shader cast errors show dimension rules
- [ ] Shader cast errors suggest constructors/swizzling
- [ ] Empty array errors suggest buffers
- [ ] String type errors list valid alternatives
- [ ] Operation errors list all supported ops
- [ ] Complex callee errors show refactoring patterns
- [ ] All errors include before/after code examples
- [ ] All errors use ✅/❌ emoji for clarity
- [ ] All errors follow consistent format
