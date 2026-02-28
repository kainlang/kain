# KAIN Effect System and Shader Error Improvements

This document details comprehensive error message improvements for the two most cryptic error categories in KAIN: effect system violations and shader operation errors.

---

## Part 1: Effect System Error Improvements

### Location
`Kain/crates/kain-core/src/effects.rs` - Line 60-66

### Current Error (Cryptic)
```
Effect violation: {Pure} cannot call {IO}
```

### Improved Error (Comprehensive)

```rust
pub fn check_effect_call(
    caller: &EffectSet, 
    callee: &EffectSet, 
    caller_name: &str,
    callee_name: &str,
    span: Span
) -> KainResult<()> {
    if !caller.can_call(callee) {
        let caller_effect_str = if caller.is_pure() { 
            "Pure".to_string() 
        } else { 
            format!("{:?}", caller.effects) 
        };
        
        let callee_effect_str = if callee.is_pure() { 
            "Pure".to_string() 
        } else { 
            format!("{:?}", callee.effects) 
        };
        
        return Err(KainError::effect_error(
            format!(
                "Effect violation: {} function '{}' cannot call {} function '{}'.

Effect System Rules:
  • Pure functions: No side effects, can only call Pure functions
  • IO functions: Can perform I/O (file/network/console), can call Pure or IO functions
  • Async functions: Can perform async operations, can call Pure, IO, or Async functions
  • GPU functions: Run on graphics hardware, can call Pure or GPU functions
  • Unsafe functions: Can break safety guarantees, can call any function

Current situation:
  • Caller '{}' is marked as {}
  • Callee '{}' is marked as {}

How to fix:
  1. Add effect annotation to caller: fn {}() -> RetType with {}
  2. OR mark callee as Pure if it has no side effects
  3. OR change your call chain to avoid mixing incompatible effects

Example (Pure calling IO - INVALID):
  fn read_config() -> String with IO:
      let data = load_from_disk()  # OK: IO can call IO
      return data
  
  fn calculate_score() -> Int with Pure:
      let config = read_config()   # ERROR: Pure cannot call IO
      return 42

Example (Fixed):
  fn calculate_score() -> Int with IO:  # Changed to IO
      let config = read_config()   # OK: IO can call IO
      return 42

Documentation: https://kain.dev/docs/effects",
                caller_effect_str, caller_name, callee_effect_str, callee_name,
                caller_name, caller_effect_str, callee_name, callee_effect_str,
                caller_name, callee_effect_str
            ),
            span,
        ));
    }
    Ok(())
}
```

### Changes Required
1. Update function signature to accept `caller_name` and `callee_name` strings
2. Replace cryptic debug format with human-readable effect names
3. Add comprehensive explanation with rules, examples, and fix suggestions
4. Update all call sites to pass function names

---

## Part 2: Shader Operation Error Improvements

### 2.1 Invalid Cast Errors

**Location:** `Kain/crates/ue5-shaders/src/codegen_usf.rs` - Line 1015-1026

**Current Error:**
```
Invalid cast from 'Vec2' to 'Vec3' in shader. Casts must be within same dimension...
```

**Improved Error:**
```rust
if !is_valid {
    let suggestion = match (source_type.as_str(), target_type_name) {
        // Scalar to vector
        (s, t) if scalar_types.contains(&s) && (vec2_types.contains(&t) || vec3_types.contains(&t) || vec4_types.contains(&t)) => {
            format!("Use constructor syntax: {}({}, {}, ...) to build vector from scalars", t, s, s)
        },
        // Vector to different dimension
        (s, t) if (vec2_types.contains(&s) || vec3_types.contains(&s) || vec4_types.contains(&s)) 
               && (vec2_types.contains(&t) || vec3_types.contains(&t) || vec4_types.contains(&t)) => {
            format!("Use swizzling: my_vec.xy or my_vec.xyz, or constructor: {}(my_vec.x, my_vec.y, ...)", t)
        },
        // Vector to scalar
        (s, t) if (vec2_types.contains(&s) || vec3_types.contains(&s) || vec4_types.contains(&s)) 
               && scalar_types.contains(&t) => {
            "Use component access: my_vec.x or my_vec.y".to_string()
        },
        _ => "Check HLSL type compatibility rules".to_string()
    };
    
    return Err(KainError::codegen(
        &format!(
            "Invalid cast from '{}' to '{}' in shader.

Cast Rules:
  • Casts must be within same dimension category
  • Scalars: Int, UInt, Float, Bool (can cast between these)
  • 2D vectors: Vec2, IVec2, UVec2 (can cast between these)
  • 3D vectors: Vec3, IVec3, UVec3 (can cast between these)
  • 4D vectors: Vec4, IVec4, UVec4 (can cast between these)

What you tried: {} as {}
Problem: Dimension mismatch - cannot cast between different dimensions

How to fix: {}

Valid cast examples:
  • let x: Float = 3.14
    let y = x as Int              # OK: scalar to scalar
  
  • let v: Vec3 = vec3(1.0, 2.0, 3.0)
    let u = v as IVec3            # OK: Vec3 to IVec3 (same dimension)
  
  • let v2: Vec2 = vec2(1.0, 2.0)
    let v3 = Vec3(v2.x, v2.y, 0.0) # OK: use constructor for dimension change
    # NOT: let v3 = v2 as Vec3    # ERROR: dimension mismatch

Documentation: https://kain.dev/docs/shaders/types",
            source_type, target_type_name,
            source_type, target_type_name,
            suggestion
        ),
        source_expr.span()
    ));
}
```

### 2.2 Empty Array Literal Errors

**Location:** `Kain/crates/ue5-shaders/src/codegen_usf.rs` - Line 1976-1978

**Current Error:**
```
Empty array literals not supported in shaders
```

**Improved Error:**
```rust
if elements.is_empty() {
    return Err(KainError::codegen(
        "Empty array literals not supported in shaders.

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

Note: Shader arrays are compiled to static const arrays in HLSL.
For dynamic data, use StructuredBuffer or RWBuffer uniforms.

Documentation: https://kain.dev/docs/shaders/arrays",
        *span
    ));
}
```

### 2.3 Unsupported Binary Operation Errors

**Location:** `Kain/crates/ue5-shaders/src/codegen_usf.rs` - Line 1897-1899

**Current Error:**
```
Unsupported binary op in USF
```

**Improved Error:**
```rust
_ => {
    let op_name = match op {
        BinaryOp::Add => "+",
        BinaryOp::Sub => "-",
        BinaryOp::Mul => "*",
        BinaryOp::Div => "/",
        BinaryOp::Mod => "%",
        BinaryOp::Eq => "==",
        BinaryOp::Ne => "!=",
        BinaryOp::Lt => "<",
        BinaryOp::Le => "<=",
        BinaryOp::Gt => ">",
        BinaryOp::Ge => ">=",
        BinaryOp::And => "&&",
        BinaryOp::Or => "||",
        BinaryOp::BitAnd => "&",
        BinaryOp::BitOr => "|",
        BinaryOp::BitXor => "^",
        BinaryOp::Shl => "<<",
        BinaryOp::Shr => ">>",
        _ => "unknown"
    };
    
    return Err(KainError::codegen(
        &format!(
            "Unsupported operation '{}' in shader.

Supported shader operations:
  Arithmetic: +, -, *, / (all types), % (integers only)
  Comparison: ==, !=, <, >, <=, >= (scalars and vectors)
  Logical: &&, ||, ! (booleans only)
  Bitwise: &, |, ^, <<, >> (integers only)
  
Vector operations:
  • Component-wise: +, -, *, / work on Vec2/Vec3/Vec4
    Example: vec3(1,2,3) + vec3(4,5,6) = vec3(5,7,9)
  
  • Dot product: dot(a, b) → scalar
  • Cross product: cross(a, b) → Vec3 (3D only)
  • Length: length(v) → scalar
  • Normalize: normalize(v) → same type as v
  
Matrix operations:
  • Matrix multiply: mat * mat, mat * vec
  • Component-wise: use mul() function
  
Common restrictions:
  • Modulo (%) requires integer types (Int, UInt, IVec2, etc.)
  • Bitwise ops require integer types
  • Logical ops require Bool type
  • Some ops may require specific HLSL shader model (SM 5.0+)

What you tried: <expr> {} <expr>
Problem: This operation is not supported in HLSL shaders

Suggestions:
  • For modulo on floats: use fmod(a, b) function
  • For integer division: ensure both operands are Int/UInt
  • For vector operations: use built-in functions (dot, cross, length)
  • For custom ops: write a helper function

Documentation: https://kain.dev/docs/shaders/operations",
            op_name, op_name
        ),
        expr.span()
    ))
}
```

### 2.4 String Type in Shader Errors

**Location:** `Kain/crates/ue5-shaders/src/validation.rs` - Line 329-334

**Current Error:**
```
Shader 'X': Uniform 'Y' has String type. String types are not supported in shaders.
```

**Improved Error:**
```rust
if type_name == "String" || type_name == "string" {
    errors.push(format!(
        "Shader '{}': Uniform '{}' has String type.

Problem: String types cannot be passed to GPU shaders.
  • Shaders run on GPU hardware which doesn't support dynamic strings
  • HLSL has no string type - only numeric types, vectors, matrices, textures, buffers

How to fix:
  1. Use numeric indices instead of strings
     ❌ uniform texture_name: String @0
     ✅ uniform texture_index: Int @0
  
  2. Use texture/sampler types directly
     ❌ uniform albedo_path: String @0
     ✅ uniform albedo_map: Sampler2D @1
  
  3. For text rendering, use texture atlases with character indices
     ✅ uniform font_atlas: Texture2D @0
     ✅ uniform char_index: Int @1
  
  4. For debug output, use numeric codes
     ✅ uniform debug_mode: Int @0  # 0=off, 1=normals, 2=uvs

Valid shader uniform types:
  • Scalars: Int, UInt, Float, Bool
  • Vectors: Vec2, Vec3, Vec4 (and IVec*, UVec* variants)
  • Matrices: Mat2, Mat3, Mat4
  • Textures: Texture2D, Texture3D, TextureCube, Sampler2D
  • Buffers: Buffer<T>, RWBuffer<T>, StructuredBuffer<T>
  • User-defined POD structs (no strings inside)

Documentation: https://kain.dev/docs/shaders/types",
        shader_name, uniform_name
    ));
    return;
}
```

### 2.5 Complex Callee Not Supported

**Location:** `Kain/crates/ue5-shaders/src/codegen_usf.rs` - Line 1927-1929

**Current Error:**
```
Complex callee not supported in USF
```

**Improved Error:**
```rust
} else {
    return Err(KainError::codegen(
        "Complex function call expression not supported in shaders.

Problem: Shaders only support direct function calls, not computed function pointers or closures.

What you tried: A function call where the callee is not a simple identifier.

Supported:
  ✅ my_function(arg1, arg2)           # Direct function call
  ✅ dot(vec_a, vec_b)                 # Built-in function
  ✅ MyStruct::static_method(arg)      # Static method call

Not supported:
  ❌ let func = my_function            # Function pointers
  ❌ func(arg1, arg2)                  # Indirect call
  ❌ callbacks[index](arg)             # Function arrays
  ❌ obj.method_ptr()(arg)             # Method pointers
  ❌ (condition ? func_a : func_b)(x)  # Computed function selection

How to fix:
  • Use direct function calls with explicit names
  • Replace function pointers with if/else or match statements
  • Inline lambda logic directly into shader code

Example (before):
  let operation = if use_sqrt { sqrt } else { abs }
  let result = operation(value)

Example (after):
  let result = if use_sqrt { sqrt(value) } else { abs(value) }

Documentation: https://kain.dev/docs/shaders/functions",
        expr.span()
    ))
}
```

---

## Part 3: Error Examples Reference

### Effect System Examples

#### Example 1: Pure calling IO (Invalid)
```kain
fn load_config() -> String with IO:
    return read_file("config.txt")

fn calculate_score(base: Int) -> Int with Pure:
    let config = load_config()  # ❌ ERROR: Pure cannot call IO
    return base * 2
```

**Error:**
```
Effect violation: Pure function 'calculate_score' cannot call IO function 'load_config'.
...
```

**Fix:**
```kain
fn calculate_score(base: Int) -> Int with IO:  # Changed to IO
    let config = load_config()  # ✅ OK: IO can call IO
    return base * 2
```

#### Example 2: IO calling Async (Invalid)
```kain
async fn fetch_data() -> String with Async:
    return await http_get("https://api.example.com")

fn process_data() -> String with IO:
    let data = fetch_data()  # ❌ ERROR: IO cannot call Async
    return data
```

**Fix:**
```kain
async fn process_data() -> String with Async:  # Changed to Async
    let data = await fetch_data()  # ✅ OK: Async can call Async
    return data
```

#### Example 3: GPU calling IO (Invalid)
```kain
fn load_texture() -> Vec4 with IO:
    return read_texture("albedo.png")

shader fragment MyShader(uv: Vec2) -> Vec4:
    let color = load_texture()  # ❌ ERROR: GPU cannot call IO
    return color
```

**Fix:**
```kain
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform albedo: Sampler2D @0  # ✅ Pass texture as uniform
    let color = sample(albedo, uv)
    return color
```

#### Example 4: Pure calling Pure (Valid)
```kain
fn square(x: Int) -> Int with Pure:
    return x * x

fn sum_of_squares(a: Int, b: Int) -> Int with Pure:
    return square(a) + square(b)  # ✅ OK: Pure can call Pure
```

#### Example 5: Unsafe calling anything (Valid)
```kain
fn dangerous_operation() -> Int with Unsafe:
    let data = read_file("data.bin")  # ✅ OK: Unsafe can call IO
    let result = await process(data)   # ✅ OK: Unsafe can call Async
    return result
```

### Shader Error Examples

#### Example 1: Invalid Cast (Vec2 → Vec3)
```kain
shader compute Process(thread_id: Vec3):
    let uv: Vec2 = vec2(0.5, 0.5)
    let pos: Vec3 = uv as Vec3  # ❌ ERROR: dimension mismatch
```

**Fix:**
```kain
shader compute Process(thread_id: Vec3):
    let uv: Vec2 = vec2(0.5, 0.5)
    let pos: Vec3 = vec3(uv.x, uv.y, 0.0)  # ✅ Use constructor
```

#### Example 2: Empty Array Literal
```kain
shader fragment ColorGrade(uv: Vec2) -> Vec4:
    let lut = []  # ❌ ERROR: empty array not supported
    return vec4(1.0, 1.0, 1.0, 1.0)
```

**Fix:**
```kain
shader fragment ColorGrade(uv: Vec2) -> Vec4:
    let lut = [0.0, 0.5, 1.0]  # ✅ Provide initial values
    return vec4(lut[0], lut[1], lut[2], 1.0)
```

#### Example 3: String Uniform
```kain
shader fragment TextRender(uv: Vec2) -> Vec4:
    uniform text: String @0  # ❌ ERROR: strings not supported in shaders
    return vec4(1.0, 1.0, 1.0, 1.0)
```

**Fix:**
```kain
shader fragment TextRender(uv: Vec2) -> Vec4:
    uniform font_atlas: Texture2D @0  # ✅ Use texture atlas
    uniform char_index: Int @1        # ✅ Use numeric index
    let char_uv = get_char_uv(char_index)
    return sample(font_atlas, char_uv)
```

#### Example 4: Modulo on Float
```kain
shader fragment Pattern(uv: Vec2) -> Vec4:
    let x = uv.x % 0.5  # ❌ ERROR: modulo requires integers
    return vec4(x, x, x, 1.0)
```

**Fix:**
```kain
shader fragment Pattern(uv: Vec2) -> Vec4:
    let x = fmod(uv.x, 0.5)  # ✅ Use fmod() for floats
    return vec4(x, x, x, 1.0)
```

#### Example 5: Complex Function Call
```kain
shader compute Process(thread_id: Vec3):
    let operations = [sqrt, abs, floor]  # ❌ ERROR: function arrays not supported
    let result = operations[0](5.0)
```

**Fix:**
```kain
shader compute Process(thread_id: Vec3):
    let op_index = 0
    let result = match op_index:  # ✅ Use match/if for selection
        0 => sqrt(5.0)
        1 => abs(5.0)
        2 => floor(5.0)
        _ => 0.0
```

---

## Implementation Checklist

### Phase 1: Effect System (effects.rs)
- [ ] Update `check_effect_call` signature to accept function names
- [ ] Replace debug format with human-readable effect names
- [ ] Add comprehensive error message with rules, examples, fixes
- [ ] Update all call sites in type checker to pass function names
- [ ] Add unit tests for new error messages

### Phase 2: Shader Casts (codegen_usf.rs)
- [ ] Add suggestion logic based on cast type mismatch
- [ ] Expand error message with rules, examples, fixes
- [ ] Add examples for constructor syntax and swizzling
- [ ] Update existing cast tests to verify new error format

### Phase 3: Shader Arrays (codegen_usf.rs)
- [ ] Expand empty array error with workarounds
- [ ] Add examples for buffers vs static arrays
- [ ] Document HLSL array limitations

### Phase 4: Shader Strings (validation.rs)
- [ ] Expand string type error with alternatives
- [ ] Add examples for texture atlases and numeric indices
- [ ] List all valid shader uniform types

### Phase 5: Shader Operations (codegen_usf.rs)
- [ ] Create comprehensive operation support list
- [ ] Add operation-specific suggestions
- [ ] Document vector/matrix operation patterns
- [ ] Add examples for common mistakes

### Phase 6: Complex Callees (codegen_usf.rs)
- [ ] Expand error with supported vs unsupported patterns
- [ ] Add examples for refactoring function pointers
- [ ] Document shader function call limitations

### Phase 7: Testing
- [ ] Add integration tests for all new error messages
- [ ] Verify error messages appear correctly in CLI output
- [ ] Test with real-world shader code from Factory plugins
- [ ] Update documentation with error examples

---

## Success Metrics

1. **Clarity:** Users should understand what went wrong without reading source code
2. **Actionability:** Every error should suggest at least one concrete fix
3. **Examples:** Every error should show before/after code snippets
4. **Consistency:** All errors should follow the same format (Problem → Rules → Fix → Examples)
5. **Completeness:** Cover all common shader and effect system mistakes

---

## Notes

- All error messages follow the pattern: **Problem → Rules → Current Situation → How to Fix → Examples**
- Examples use ✅/❌ emoji for visual clarity
- Documentation links point to (future) comprehensive guides
- Error messages are designed for LLM consumption (structured, detailed, example-rich)
- No TODOs or simplifications - all implementations are complete and production-ready
