# KAIN Language Features - Part 1
## Core Compiler Infrastructure (kain-core)

> **Comprehensive documentation of all language features, syntax constructs, and compiler capabilities**  
> **Source:** Analysis of 9 kain-core modules (ast.rs, lexer.rs, types.rs, effects.rs, comptime.rs, diagnostics.rs, monomorphize.rs, asm_ir.rs, lib.rs)

---

## Table of Contents

1. [Abstract Syntax Tree (AST)](#1-abstract-syntax-tree-ast)
2. [Type System](#2-type-system)
3. [Effect System](#3-effect-system)
4. [Compile-Time Evaluation](#4-compile-time-evaluation)
5. [Lexer & Tokenization](#5-lexer--tokenization)
6. [Error Handling & Diagnostics](#6-error-handling--diagnostics)
7. [Monomorphization (Generics)](#7-monomorphization-generics)
8. [Assembly Integration](#8-assembly-integration)
9. [Compilation Targets](#9-compilation-targets)
10. [Feature Summary](#10-feature-summary)

---

## 1. Abstract Syntax Tree (AST)

### 1.1 Top-Level Items

KAIN supports 25 top-level item types, making it one of the most feature-rich ASTs in modern language design:

| Item Type | Syntax | Purpose |
|-----------|--------|---------|
| **Function** | `fn name(args) -> Type with Effects: body` | Standard functions with effect annotations |
| **Component** | `component Name(props) -> UI with Reactive: jsx` | React-like UI components |
| **Shader** | `shader Name(inputs) -> Fragment with GPU: body` | GPU shader programs (vertex/fragment/compute/surface) |
| **Actor** | `actor Name: handlers` | Erlang-style concurrent actors with message passing |
| **Struct** | `struct Name { fields }` | Data structures with fields and methods |
| **Enum** | `enum Name { variants }` | Algebraic data types with unit/tuple/struct variants |
| **Trait** | `trait Name { methods }` | Interface definitions with default implementations |
| **Impl** | `impl Trait for Type { methods }` | Trait implementations and inherent methods |
| **TypeAlias** | `type Alias = Type` | Type aliases for complex types |
| **Use** | `use path::to::item` | Module imports with aliasing and glob support |
| **Mod** | `mod name` | Module declarations (inline or external) |
| **Const** | `const NAME: Type = value` | Compile-time constants |
| **Comptime** | `comptime { code }` | Zig-style compile-time execution blocks |
| **Macro** | `macro name!(params) { expansion }` | Hygienic macro definitions |
| **Test** | `test "name": body` | Unit test definitions |
| **MaterialGraph** | `@material_graph Name: inputs, body, outputs` | UE5 material graph definitions |
| **MaterialFunction** | `@material_function Name: inputs, body, output` | Reusable material node graphs |
| **GraphEditor** | `@graph_editor Name: node_types, schema` | UE5 graph editor definitions |
| **GraphRuntime** | `@graph_runtime Name: graph_data, node_data, instance` | Runtime graph execution system |
| **StateMachine** | `@state_machine Name: states, transitions` | Animation state machines |
| **AsyncTask** | `@async_task Name: input, output, callback` | Async worker thread tasks |
| **EditorModule** | `@editor_module Name: menu_entries, toolbar_buttons` | UE5 editor extensions |
| **GameplayTags** | `@gameplay_tags namespace Name: tag_hierarchy` | UE5 Gameplay Tag hierarchies |
| **GameplayAbility** | `@ability struct Name: policies, tags, lifecycle` | UE5 Gameplay Ability System abilities |
| **GameplayEffect** | `@gameplay_effect struct Name: duration, modifiers` | UE5 GAS effects |
| **GameplayCue** | `@gameplay_cue struct Name: tag, type, lifecycle` | UE5 GAS cues |
| **AbilityTask** | `@ability_task struct Name: delegates, state` | UE5 GAS ability tasks |
| **TargetActor** | `@target_actor struct Name: trace_type, filters` | UE5 GAS target actors |

### 1.2 Functions

```kain
fn factorial(n: Int) -> Int with Pure:
    match n:
        0 => 1
        _ => n * factorial(n - 1)

async fn fetch_data(url: String) -> Result<String, Error> with Async, IO:
    let response = await http_get(url)?
    return Ok(response)
```

**Features:**
- Generic parameters with trait bounds: `fn map<T, U: Display>(x: T) -> U`
- Effect annotations: `with Pure`, `with IO`, `with Async`, `with GPU`
- Default parameter values: `fn greet(name: String = "World")`
- Mutable parameters: `fn increment(mut x: Int)`
- Visibility modifiers: `pub fn`, `pub(crate) fn`
- Attributes: `@inline`, `@wasm`, `@blueprint_callable`


### 1.3 Components (React-like UI)

```kain
component Counter(initial: Int) -> UI with Reactive:
    state count: Int = initial
    state label: String = "Count"
    
    fn increment():
        count = count + 1
    
    <div>
        <h1>{label}: {count}</h1>
        <button onclick={increment}>Increment</button>
    </div>
```

**Features:**
- Props with type annotations
- State declarations with initial values
- Methods for state manipulation
- JSX-style syntax with embedded expressions
- Control flow in JSX: `for`, `if/else`
- Component composition: `<OtherComponent prop={value} />`

**JSX Node Types:**
- `Element`: HTML-like tags with attributes and children
- `Expression`: Embedded KAIN expressions `{expr}`
- `Text`: Plain text nodes
- `ComponentCall`: Nested component invocations
- `For`: Loop iteration `for item in list: <li>{item}</li>`
- `If`: Conditional rendering `if cond: <div>Yes</div> else: <div>No</div>`
- `Fragment`: Wrapper for multiple children

### 1.4 Shaders (GPU Programs)

```kain
shader compute VoxelGenerator(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform noise_scale: Float @1
    buffer output: RWBuffer<Float> @2
    
    let noise = perlin_noise(thread_id * noise_scale)
    output[thread_id.x] = noise

shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform base_color: Vec3 @0
    uniform albedo_map: Sampler2D @1
    
    let tex_color = sample(albedo_map, uv).rgb
    return vec4(tex_color * base_color, 1.0)
```

**Shader Stages:**
- `Vertex`: Vertex transformation shaders
- `Fragment`: Pixel/fragment shaders
- `Compute`: General-purpose GPU compute
- `Surface`: UE5 surface shaders (material shaders)

**Features:**
- Uniform bindings with explicit slots: `@0`, `@1`, `@2`
- Buffer types: `RWBuffer<T>`, `Buffer<T>`, `Sampler2D`, `Texture2D`
- Built-in shader functions: `perlin_noise`, `sample`, `vec4`, etc.
- Shader permutations via `CFG_*` / `ENABLE_*` prefixed uniforms


### 1.5 Actors (Erlang-style Concurrency)

```kain
actor ChatRoom:
    state messages: Array<String> = []
    state users: Array<String> = []
    
    on Join(name: String):
        push(users, name)
        broadcast("{name} joined")
    
    on Message(from: String, text: String):
        push(messages, "{from}: {text}")
        broadcast("{from}: {text}")
    
    fn broadcast(msg: String):
        // Send to all connected clients
        println(msg)
```

**Features:**
- State fields with initial values
- Message handlers: `on MessageType(params): body`
- Methods for internal logic
- Actor spawning: `spawn ChatRoom { messages: [], users: [] }`
- Message sending: `send actor <- Message { from: "Alice", text: "Hello" }`
- No shared mutable state (actor isolation)

### 1.6 Data Structures

#### Structs

```kain
struct Point:
    x: Float
    y: Float
    
    fn distance(self, other: Point) -> Float:
        let dx = self.x - other.x
        let dy = self.y - other.y
        return sqrt(dx * dx + dy * dy)

@component
struct HealthComponent:
    @replicated
    current: Float
    @replicated
    max: Float
    @savegame
    is_invulnerable: Bool
```

**Features:**
- Fields with types and optional defaults
- Methods (inherent and trait implementations)
- Generic structs: `struct Vec<T> { data: Array<T> }`
- Attributes: `@component`, `@datatable`, `@subsystem`
- Field attributes: `@replicated`, `@savegame`, `@transient`, `@editdefaults`
- Visibility: `pub struct`, `pub(crate) struct`


#### Enums

```kain
enum ItemRarity:
    Common
    Rare
    Epic
    Legendary

enum Result<T, E>:
    Ok(T)
    Err(E)

enum Message:
    Quit
    Move { x: Int, y: Int }
    Write(String)
    ChangeColor(Int, Int, Int)
```

**Variant Types:**
- **Unit**: `Common`, `Rare` (no associated data)
- **Tuple**: `Ok(T)`, `Err(E)` (positional fields)
- **Struct**: `Move { x: Int, y: Int }` (named fields)

**Features:**
- Generic enums with type parameters
- Pattern matching on variants
- Visibility control
- UE5 codegen: `UENUM(BlueprintType) enum class EItemRarity : uint8`

### 1.7 Traits and Implementations

```kain
trait Display:
    fn to_string(self) -> String

trait Iterator<T>:
    fn next(mut self) -> Option<T>
    
    fn collect(mut self) -> Array<T>:
        let result = []
        loop:
            match self.next():
                Some(item) => push(result, item)
                None => break
        return result

impl Display for Point:
    fn to_string(self) -> String:
        return "Point({self.x}, {self.y})"
```

**Features:**
- Generic traits with associated types
- Default method implementations
- Trait bounds on generics: `fn print<T: Display>(x: T)`
- Multiple trait bounds: `fn process<T: Display + Clone>(x: T)`
- Inherent implementations (methods without traits)


### 1.8 Expressions (50+ Expression Types)

KAIN has one of the most comprehensive expression systems in modern languages:

#### Literals
```kain
42                    // Int
3.14                  // Float
"hello"               // String
f"Hello {name}"       // FString (formatted string)
'a'                   // Char
true, false           // Bool
none                  // None (Option type)
```

#### Operators

**Binary Operators:**
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**` (power)
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&` (and), `||` (or)
- Bitwise: `&`, `|`, `^`, `<<`, `>>`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`
- Range: `..` (exclusive), `..=` (inclusive)

**Unary Operators:**
- Negation: `-x`
- Logical NOT: `!x`
- Bitwise NOT: `~x`
- Reference: `&x`, `&mut x`
- Dereference: `*ptr`

#### Control Flow

```kain
// If expression
let result = if x > 0:
    "positive"
else if x < 0:
    "negative"
else:
    "zero"

// Match expression
let description = match value:
    0 => "zero"
    1..10 => "small"
    10..100 => "medium"
    _ => "large"

// Match with guards
match point:
    Point { x, y } if x == y => "diagonal"
    Point { x: 0, y } => "on y-axis"
    Point { x, y: 0 } => "on x-axis"
    _ => "other"
```


#### Loops

```kain
// For loop
for item in items:
    println(item)

// While loop
while condition:
    do_work()

// Infinite loop
loop:
    if should_break:
        break
    if should_skip:
        continue
```

#### Function Calls

```kain
// Regular call
let result = calculate(x, y)

// Named arguments
let point = Point { x: 10, y: 20 }

// Method call
let distance = point.distance(other)

// Chained calls
let result = data.filter(|x| x > 0).map(|x| x * 2).sum()
```

#### Lambdas (Closures)

```kain
// Simple lambda
let add = |a, b| a + b

// Lambda with type annotations
let multiply: fn(Int, Int) -> Int = |a: Int, b: Int| -> Int: a * b

// Lambda with block body
let process = |x|:
    let doubled = x * 2
    let squared = doubled * doubled
    return squared
```

#### Collections

```kain
// Array literal
let numbers = [1, 2, 3, 4, 5]

// Tuple literal
let pair = (42, "answer")

// Struct literal
let point = Point { x: 10, y: 20 }

// Enum variant
let result = Ok(42)
let message = Message::Move { x: 10, y: 20 }
```

#### Advanced Expressions

```kain
// Range
for i in 0..10:        // 0 to 9
    println(i)

for i in 0..=10:       // 0 to 10 (inclusive)
    println(i)

// Try operator (error propagation)
let data = read_file(path)?

// Await (async)
let response = await fetch_data(url)

// Cast
let x = value as Float

// Reference and dereference
let ref = &value
let val = *ref

// Comptime
let result = comptime { 2 + 2 }  // Evaluated at compile time
```


### 1.9 Patterns (Advanced Pattern Matching)

```kain
// Wildcard
match value:
    _ => "anything"

// Literal
match x:
    0 => "zero"
    1 => "one"
    "hello" => "greeting"

// Binding
match value:
    x => println(x)
    mut y => y = y + 1

// Struct destructuring
match point:
    Point { x, y } => println("({x}, {y})")
    Point { x: 0, y } => println("on y-axis: {y}")
    Point { x, y: 0 } => println("on x-axis: {x}")
    Point { x, .. } => println("x is {x}, y ignored")

// Tuple destructuring
match pair:
    (0, y) => println("first is zero, second is {y}")
    (x, 0) => println("first is {x}, second is zero")
    (x, y) => println("({x}, {y})")

// Enum variant
match result:
    Ok(value) => println("Success: {value}")
    Err(error) => println("Error: {error}")

// Array/Slice
match list:
    [] => "empty"
    [x] => "single element"
    [first, rest @ ..] => "first: {first}, rest: {rest}"

// Or pattern
match value:
    1 | 2 | 3 => "small"
    4 | 5 | 6 => "medium"
    _ => "large"

// Range pattern
match age:
    0..18 => "minor"
    18..65 => "adult"
    65.. => "senior"
```


### 1.10 UE5-Specific AST Nodes

KAIN has first-class support for Unreal Engine 5 constructs:

#### Material Graphs

```kain
@material_graph PBRGround:
    input albedo: Texture2D
    input roughness_value: Float = 0.5
    
    let base = texture_sample(albedo).rgb
    let uv_scaled = uv_scale(uv, 2.0)
    
    base_color = base
    roughness = roughness_value
    metallic = 0.0
```

#### Graph Editors

```kain
@graph_editor DialogueGraph:
    @node_type
    node NPCNode:
        category: "Dialogue"
        inputs:
            InExec: Exec
        outputs:
            Next: Exec
        properties:
            SpeakerName: String = "NPC"
            DialogueText: String = ""
```

#### Graph Runtime

```kain
@graph_runtime DialogueSystem:
    @node_data
    node SpeakerNode:
        speaker_name: String = "NPC"
        @input_pin
        in_exec: Exec
        @output_pin
        next: Exec
        
        fn execute():
            println("Speaker: {speaker_name}")
```

#### State Machines

```kain
@state_machine CombatAnimations:
    @state(entry: true)
    idle:
        animation: "Idle_Anim"
        @transition(to: "attacking")
        fn can_attack() -> Bool:
            return input_pressed("Attack")
    
    @state
    attacking:
        animation: "Attack_Anim"
        @transition(to: "idle")
        fn attack_finished() -> Bool:
            return animation_complete()
```


#### Async Tasks

```kain
@async_task MeshGenerator:
    @input
    resolution: Int
    @output
    vertices: Array<Vec3>
    
    @callback(thread: "game")
    fn on_complete(result: Array<Vec3>):
        println("Mesh generated with {len(result)} vertices")
    
    fn do_work():
        // Heavy computation on worker thread
        return generate_mesh(resolution)
```

#### Gameplay Ability System

```kain
@ability struct FireballAbility:
    instancing_policy: "InstancedPerActor"
    @tag("Ability.Magic.Fireball")
    @cost_tag("Resource.Mana")
    
    fn can_activate() -> Bool:
        return has_mana(50)
    
    fn activate():
        consume_mana(50)
        spawn_projectile("Fireball")

@gameplay_effect struct BurnEffect:
    duration_policy: "HasDuration"
    duration: 5.0
    @modifier(attribute: "Health", op: "Add", magnitude: -10.0)
    @tag("Effect.Damage.Fire")
```

---

## 2. Type System

### 2.1 Primitive Types

| Type | Description | Size | Example |
|------|-------------|------|---------|
| `Int` | Signed integer | 64-bit | `42`, `-10` |
| `Float` | Floating point | 64-bit | `3.14`, `-0.5` |
| `Bool` | Boolean | 1 bit | `true`, `false` |
| `String` | UTF-8 string | Variable | `"hello"` |
| `Char` | Unicode character | 32-bit | `'a'`, '🚀' |
| `Unit` | Empty type | 0 bytes | `()` |
| `Never` | Diverging type | N/A | `!` |

### 2.2 Composite Types

```kain
// Tuple
let pair: (Int, String) = (42, "answer")

// Array (fixed size)
let numbers: [Int; 5] = [1, 2, 3, 4, 5]

// Slice (dynamic size)
let slice: [Int] = numbers[1..3]

// Option
let maybe: Option<Int> = Some(42)
let nothing: Option<Int> = None

// Result
let success: Result<Int, String> = Ok(42)
let failure: Result<Int, String> = Err("error")
```


### 2.3 Reference Types

```kain
// Immutable reference
let x = 42
let ref: &Int = &x

// Mutable reference
let mut y = 10
let mut_ref: &mut Int = &mut y
*mut_ref = 20

// Lifetime annotations (explicit)
fn longest<'a>(x: &'a String, y: &'a String) -> &'a String:
    if len(x) > len(y):
        return x
    else:
        return y
```

### 2.4 Function Types

```kain
// Function pointer
let add: fn(Int, Int) -> Int = |a, b| a + b

// Function with effects
let read: fn(String) -> String with IO = read_file

// Generic function type
let map: fn<T, U>(T, fn(T) -> U) -> U
```

### 2.5 Generic Types

```kain
// Generic struct
struct Vec<T>:
    data: Array<T>
    len: Int

// Generic enum
enum Option<T>:
    Some(T)
    None

// Generic function
fn map<T, U>(x: T, f: fn(T) -> U) -> U:
    return f(x)

// Multiple type parameters
fn zip<A, B>(a: Array<A>, b: Array<B>) -> Array<(A, B)>

// Type bounds
fn print_all<T: Display>(items: Array<T>):
    for item in items:
        println(item.to_string())
```

### 2.6 Type Inference

```kain
// Inferred from literal
let x = 42              // x: Int
let y = 3.14            // y: Float
let s = "hello"         // s: String

// Inferred from context
let numbers = [1, 2, 3] // numbers: Array<Int>
let result = Ok(42)     // result: Result<Int, _>

// Explicit type annotation
let x: Float = 42       // Cast to Float
let y: _ = compute()    // Infer from compute() return type
```


### 2.7 UE5 Type Mappings

KAIN automatically maps types to UE5 equivalents:

| KAIN Type | UE5 Type | Notes |
|-----------|----------|-------|
| `Int` | `int32` / `int64` | Configurable size |
| `Float` | `float` / `double` | Configurable precision |
| `Bool` | `bool` | Native bool |
| `String` | `FString` | UE5 string type |
| `Array<T>` | `TArray<T>` | Dynamic array |
| `Option<T>` | `TOptional<T>` | Optional value |
| `Vec2` | `FVector2D` | 2D vector |
| `Vec3` | `FVector` | 3D vector |
| `Vec4` | `FVector4` | 4D vector |
| `Quat` | `FQuat` | Quaternion |
| `Transform` | `FTransform` | Transform matrix |

---

## 3. Effect System

KAIN tracks side effects at compile time, preventing unsafe operations:

### 3.1 Effect Types

| Effect | Description | Example |
|--------|-------------|---------|
| `Pure` | No side effects | `fn factorial(n: Int) -> Int with Pure` |
| `IO` | File/Network/Console I/O | `fn read_file(path: String) -> String with IO` |
| `Async` | Can await futures | `async fn fetch() -> Data with Async` |
| `GPU` | Runs on GPU | `shader compute process() with GPU` |
| `Reactive` | Triggers UI updates | `component Counter() with Reactive` |
| `Unsafe` | Breaks safety guarantees | `fn raw_ptr() with Unsafe` |
| `Alloc` | Memory allocation | `fn create_vec() with Alloc` |
| `Panic` | Can abort execution | `fn assert(cond: Bool) with Panic` |

### 3.2 Effect Checking

```kain
// Pure function cannot call IO function
fn pure_func() -> Int with Pure:
    let x = read_file("data.txt")  // ERROR: Effect violation
    return len(x)

// IO function can call pure function
fn io_func() -> Int with IO:
    let x = read_file("data.txt")  // OK
    return factorial(len(x))       // OK: Pure is subset of IO

// Unsafe can call anything
fn unsafe_func() with Unsafe:
    let x = read_file("data.txt")  // OK
    let y = gpu_compute()          // OK
    // Unsafe bypasses all effect checks
```


### 3.3 Effect Inference

```kain
// Effects are inferred from function body
fn process_data(path: String):
    let data = read_file(path)     // Infers IO effect
    println(data)                  // Infers IO effect
    // Inferred signature: fn process_data(String) with IO

// Explicit effects override inference
fn safe_read(path: String) -> String with Pure:
    return "cached data"  // OK: no actual IO performed
```

---

## 4. Compile-Time Evaluation

KAIN supports Zig-style compile-time execution:

### 4.1 Comptime Blocks

```kain
// Compile-time constant
const SIZE: Int = comptime { 2 * 1024 * 1024 }

// Compile-time function call
const FACTORIAL_10: Int = comptime { factorial(10) }

// Comptime in expressions
let array_size = comptime { if DEBUG: 100 else: 10 }
```

### 4.2 Comptime Functions

```kain
fn fibonacci(n: Int) -> Int with Pure:
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

// Evaluated at compile time
const FIB_20: Int = comptime { fibonacci(20) }
```

### 4.3 Comptime Limitations

- Only `Pure` functions can be evaluated at compile time
- No IO, no GPU, no Async operations
- Must terminate (no infinite loops)
- Available when `runtime` feature is enabled (not in browser builds)

---

## 5. Lexer & Tokenization

### 5.1 Token Types (60+ Tokens)

#### Keywords (40+)
```
fn, let, mut, var, const, if, else, elif, match, for, while, loop,
break, continue, return, await, in, with, as, type, struct, enum,
trait, impl, pub, mod, use, self, Self, true, false, none,
component, shader, actor, state, spawn, send, receive, emit,
comptime, macro, vertex, fragment, test, async, Pure, IO, Async,
GPU, Reactive, Unsafe
```


#### Operators (30+)
```
+, -, *, /, %, **, ==, !=, <, >, <=, >=, &&, ||, !, &, |, ^, ~,
<<, >>, =, +=, -=, *=, /=, .., ..., ->, =>
```

#### Punctuation
```
(, ), [, ], {, }, ,, ., :, ::, ;, @, ?, </, 
```

### 5.2 Significant Whitespace

KAIN uses Python-style indentation with INDENT/DEDENT tokens:

```kain
fn example():
    let x = 1        # INDENT after colon
    if x > 0:
        println(x)   # INDENT for nested block
                     # DEDENT when returning to outer level
    println("done")  # DEDENT to function level
                     # DEDENT at end of function
```

**Indentation Rules:**
- Tabs = 4 spaces
- Consistent indentation required
- Blank lines ignored
- Comments don't affect indentation

### 5.3 String Literals

```kain
// Regular string with escape sequences
let s1 = "Hello\nWorld"

// Formatted string (f-string)
let name = "Alice"
let s2 = f"Hello {name}"

// Character literal
let c = 'a'

// Escape sequences: \n, \r, \t, \0, \\, \", \'
```

### 5.4 Comments

```kain
// Single-line comment (C++ style)

# Single-line comment (Python style)

// Multi-line comments not yet supported
```

---

## 6. Error Handling & Diagnostics

### 6.1 Error Types

| Error Type | Description | Example |
|------------|-------------|---------|
| `Lexer` | Tokenization errors | Unexpected character |
| `Parser` | Syntax errors | Missing closing brace |
| `Type` | Type mismatch | Expected Int, got String |
| `Effect` | Effect violation | Pure function calling IO |
| `Borrow` | Ownership/borrowing | Multiple mutable references |
| `Codegen` | Code generation | Invalid UE5 type |
| `Runtime` | Execution errors | Division by zero |
| `Io` | File system errors | File not found |


### 6.2 Diagnostic Output

KAIN provides rich error messages with source context:

```
error[Parser]: Expected closing brace
  --> example.kn:5:10
   |
 5 |     let x = {
   |             ^
   |

error[Type]: Type mismatch
  --> example.kn:12:15
   |
12 |     let y: Int = "hello"
   |                  ^^^^^^^
   |
   Expected: Int
   Found: String
```

### 6.3 SpanMapper (file:line:col Reporting)

The `SpanMapper` converts byte offsets to human-readable locations:

```rust
let mapper = SpanMapper::new(source);
let loc = mapper.span_to_location(span, "example.kn");
// Returns: SourceLocation { file: "example.kn", line: 12, col: 15 }
```

**Features:**
- Handles multi-byte UTF-8 characters correctly
- Supports empty files and single-line files
- Binary search for efficient line lookup
- Accurate column reporting (byte-based, 1-indexed)

### 6.4 Error Context Chaining

```rust
// Add context to errors
read_file(path)
    .with_file(PathBuf::from("config.toml"))
    .with_context("Loading configuration")
    .with_suggestion("Check that the file exists and is readable")?
```

---

## 7. Monomorphization (Generics)

KAIN implements full generic instantiation with type inference:

### 7.1 Generic Function Instantiation

```kain
// Generic function definition
fn map<T, U>(x: T, f: fn(T) -> U) -> U:
    return f(x)

// Explicit type arguments
let result = map<Int, String>(42, to_string)

// Inferred type arguments
let result = map(42, to_string)  // T=Int, U=String inferred
```


### 7.2 Type Inference Algorithm

The monomorphizer uses **unification** to infer type arguments:

```kain
fn process<T>(items: Array<T>, f: fn(T) -> Bool) -> Array<T>

// Call site
let numbers = [1, 2, 3, 4, 5]
let evens = process(numbers, |x| x % 2 == 0)

// Inference steps:
// 1. items: Array<T> unifies with [1,2,3,4,5]: Array<Int> → T = Int
// 2. f: fn(T) -> Bool unifies with |x| x % 2 == 0: fn(Int) -> Bool → T = Int
// 3. Result: process<Int> instantiated
```

### 7.3 Generic Struct Instantiation

```kain
struct Vec<T>:
    data: Array<T>
    len: Int
    
    fn push(mut self, item: T):
        // ...

// Instantiation
let v1 = Vec<Int> { data: [], len: 0 }  // Explicit
let v2: Vec<String> = Vec { data: [], len: 0 }  // Inferred from type annotation
```

### 7.4 Trait Bounds Checking

```kain
fn print_all<T: Display>(items: Array<T>):
    for item in items:
        println(item.to_string())

// Monomorphizer checks:
// 1. T is bound by Display trait
// 2. At call site, verify Display is implemented for concrete type
// 3. If not, emit error: "Type 'Foo' does not satisfy bound 'Display'"
```

### 7.5 Name Mangling

Generic instantiations are mangled to unique names:

```kain
fn identity<T>(x: T) -> T:
    return x

// Generates:
// identity_Int(x: Int) -> Int
// identity_String(x: String) -> String
// identity_Vec_Int(x: Vec<Int>) -> Vec<Int>
```

### 7.6 Async Function Lowering

The monomorphizer transforms async functions into state machines:

```kain
async fn fetch_data(url: String) -> String with Async, IO:
    let response = await http_get(url)
    let data = await response.text()
    return data

// Lowered to:
struct fetch_data_Future:
    state: Int
    url: String
    _await_0: Future<Response>
    _await_0_result: Response
    _await_1: Future<String>
    _await_1_result: String

fn fetch_data_Future_poll(mut self: fetch_data_Future) -> Poll<String>:
    match self.state:
        0 => // Start http_get
        1 => // Poll http_get, start response.text()
        2 => // Poll response.text(), return result
```


---

## 8. Assembly Integration

KAIN can import and transpile assembly code from multiple architectures:

### 8.1 Assembly IR

```rust
pub struct AsmProgram {
    pub blocks: Vec<AsmBlock>,
    pub directives: Vec<AsmDirective>,
    pub data_tables: Vec<AsmDataTable>,
}

pub struct AsmBlock {
    pub label: String,
    pub instructions: Vec<AsmInstr>,
    pub source_line_start: usize,
    pub source_line_end: usize,
}

pub struct AsmInstr {
    pub opcode: String,
    pub operand: Option<String>,
    pub source_line: usize,
}
```

### 8.2 Supported Architectures

KAIN has assembly dialect support for:

- **6502** (Furby firmware)
- **Z80** (Game Boy, retro systems)
- **LR35902** (Game Boy CPU variant)
- **x86/x64** (planned)
- **ARM** (planned)

### 8.3 Assembly Import

```bash
# Import Furby firmware assembly
kain import-asm furby_firmware.asm --dialect 6502 --output furby_firmware.kn

# Import Game Boy ROM
kain import-asm pokemon_red.asm --dialect lr35902 --output pokered_firmware.kn
```

### 8.4 Parity Trace Frames

For debugging and verification:

```rust
pub struct ParityTraceFrame {
    pub tick: u64,
    pub pc: u32,
    pub opcode: String,
    pub registers: BTreeMap<String, i64>,
    pub flags: BTreeMap<String, bool>,
    pub notes: Vec<String>,
}
```

---

## 9. Compilation Targets

KAIN supports 15+ compilation targets:

| Target | Flag | Output | Backend |
|--------|------|--------|---------|
| **WebAssembly** | `-t wasm` | `.wasm` | 95KB backend with component system |
| **JavaScript** | `-t js` | `.js` | ES6+ with async/await |
| **TypeScript** | `-t ts` | `.ts` | Full type annotations |
| **Hybrid** | `-t hybrid` | `.wasm + .js` | WASM core + JS glue |
| **LLVM Native** | `-t llvm` | executable | 66KB backend with RAII |
| **Rust** | `-t rust` | `.rs` | 28KB transpiler with Cargo.toml |
| **C++** | `-t cpp` | `.cpp/.h` | Full C++17 codegen |
| **UE5 Runtime** | `--ue5` | Full plugin | 7 specialized crates |
| **UE5 Editor** | `-t ue5editor` | Editor C++ | Slate, Details, Viewports |
| **SPIR-V** | `-t spirv` | `.spv` | 14KB backend, cross-platform GPU |
| **HLSL** | `-t hlsl` | `.hlsl` | 25KB backend, DirectX shaders |
| **USF** | `-t usf` | `.usf` | 20KB backend, UE5 shaders |
| **Interpret** | `-t run` | stdout | Instant execution, REPL |
| **Test** | `-t test` | stdout | Unit test runner |


### 9.1 Target Selection

```bash
# Single target
kain build src/main.kn --target wasm

# Multiple targets
kain build --targets wasm,js,rust

# UE5 plugin (reads KAIN.toml)
kain build --ue5

# Immediate execution
kain run examples/hello.kn
```

### 9.2 Target-Specific Features

**WASM:**
- Struct memory layout with padding
- Component system for modular code
- Enum discriminants
- Lambda/closure collection
- String pooling
- Bump allocator

**LLVM:**
- Struct/actor compilation
- Reference counting
- Scope-based cleanup (RAII)
- External C runtime linkage
- Debug info generation

**UE5:**
- AActor/UActorComponent generation
- UCLASS/UPROPERTY/UFUNCTION macros
- Replication (GetLifetimeReplicatedProps)
- RPC validation (_Validate methods)
- Blueprint integration
- Material graphs (binary .uasset)
- Graph editors (UEdGraph)
- Slate UI (SCompoundWidget)

---

## 10. Feature Summary

### 10.1 Language Features by Category

#### Core Language (Rust-inspired)
- ✅ Functions with generics and trait bounds
- ✅ Structs with methods and fields
- ✅ Enums with unit/tuple/struct variants
- ✅ Traits with default implementations
- ✅ Pattern matching with guards
- ✅ Ownership and borrowing (references)
- ✅ Type inference
- ✅ Effect system (Pure, IO, Async, GPU, etc.)
- ✅ Significant whitespace (Python-style)

#### Unique Features
- ✅ Components (React-like UI with JSX)
- ✅ Shaders (first-class GPU programs)
- ✅ Actors (Erlang-style concurrency)
- ✅ Comptime (Zig-style compile-time execution)
- ✅ Macros (Lisp-style hygienic macros)


#### UE5 Integration (25+ specialized constructs)
- ✅ Material graphs with 30+ node types
- ✅ Material functions (reusable node graphs)
- ✅ Graph editors (UEdGraph + schema + factory)
- ✅ Graph runtime (NodeData + GraphInstance)
- ✅ State machines (animation states + transitions)
- ✅ Async tasks (worker threads + callbacks)
- ✅ Editor modules (menu + toolbar extensions)
- ✅ Gameplay Ability System (abilities, effects, cues, tasks, target actors)
- ✅ Network replication (interpolation, extrapolation, compression)
- ✅ Slate widgets (full SCompoundWidget generation)
- ✅ Details panels (IDetailCustomization)
- ✅ Viewports (SEditorViewport)
- ✅ Asset editors (FAssetEditorToolkit)
- ✅ Blueprint nodes (UK2Node + Kismet bytecode)
- ✅ Binary .uasset generation (materials, blueprints)

#### Type System
- ✅ Primitives: Int, Float, Bool, String, Char, Unit, Never
- ✅ Composites: Tuple, Array, Slice, Option, Result
- ✅ References: &T, &mut T with lifetimes
- ✅ Generics with trait bounds
- ✅ Type inference with unification
- ✅ Function types with effects
- ✅ UE5 type mappings (FString, TArray, FVector, etc.)

#### Expressions (50+ types)
- ✅ Literals: Int, Float, String, FString, Char, Bool, None
- ✅ Binary ops: Arithmetic, Comparison, Logical, Bitwise, Assignment, Range
- ✅ Unary ops: Neg, Not, BitNot, Ref, RefMut, Deref
- ✅ Control flow: If, Match, For, While, Loop
- ✅ Functions: Call, MethodCall, Lambda
- ✅ Collections: Array, Tuple, Struct, EnumVariant
- ✅ Advanced: Cast, Try, Await, Spawn, SendMsg, Comptime, JSX

#### Patterns (10+ types)
- ✅ Wildcard, Literal, Binding
- ✅ Struct destructuring with rest
- ✅ Tuple destructuring
- ✅ Enum variant matching
- ✅ Array/Slice with rest binding
- ✅ Or patterns
- ✅ Range patterns
- ✅ Guards

#### Effects
- ✅ Pure, IO, Async, GPU, Reactive, Unsafe, Alloc, Panic
- ✅ Effect inference from function body
- ✅ Effect checking at call sites
- ✅ Effect subtyping (Pure ⊆ IO ⊆ Unsafe)


#### Compilation
- ✅ Lexer with 60+ token types
- ✅ Parser with error recovery
- ✅ Type checker with inference
- ✅ Effect checker
- ✅ Monomorphizer (generic instantiation)
- ✅ Comptime evaluator
- ✅ 15+ codegen backends
- ✅ Stdlib auto-discovery (200+ functions)

#### Diagnostics
- ✅ Rich error messages with source context
- ✅ SpanMapper (byte offset → file:line:col)
- ✅ Error context chaining
- ✅ Multi-error reporting
- ✅ Colored terminal output

#### Assembly Integration
- ✅ 6502, Z80, LR35902 dialects
- ✅ Assembly IR (blocks, instructions, directives)
- ✅ Parity trace frames for debugging
- ✅ Transliteration units

### 10.2 Quick Reference Table

| Feature | Syntax | Example |
|---------|--------|---------|
| Function | `fn name(args) -> Type with Effects: body` | `fn add(a: Int, b: Int) -> Int: a + b` |
| Generic | `fn name<T>(x: T) -> T` | `fn identity<T>(x: T) -> T: x` |
| Struct | `struct Name { fields }` | `struct Point { x: Float, y: Float }` |
| Enum | `enum Name { variants }` | `enum Option<T> { Some(T), None }` |
| Trait | `trait Name { methods }` | `trait Display { fn to_string(self) -> String }` |
| Impl | `impl Trait for Type { methods }` | `impl Display for Point { ... }` |
| Component | `component Name(props) -> UI: jsx` | `component Counter(initial: Int): <div>{count}</div>` |
| Shader | `shader stage Name(inputs) -> Output: body` | `shader fragment Tint(uv: Vec2) -> Vec4: ...` |
| Actor | `actor Name: state, handlers` | `actor ChatRoom: on Join(name: String): ...` |
| Match | `match expr: pattern => body` | `match x: 0 => "zero", _ => "other"` |
| Lambda | `\|args\| body` | `\|x\| x * 2` |
| Comptime | `comptime { expr }` | `const SIZE = comptime { 1024 * 1024 }` |
| Effect | `with Effect` | `fn read() -> String with IO` |
| Attribute | `@name` or `@name(args)` | `@component`, `@replicated` |


### 10.3 Comparison with Other Languages

| Feature | KAIN | Rust | Python | TypeScript | Zig | Erlang |
|---------|------|------|--------|------------|-----|--------|
| **Ownership** | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ |
| **Effect System** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Comptime** | ✅ | ❌ | ❌ | ❌ | ✅ | ❌ |
| **Actors** | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **Components (UI)** | ✅ | ❌ | ❌ | ✅ (React) | ❌ | ❌ |
| **Shaders** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Pattern Matching** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Generics** | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| **Traits** | ✅ | ✅ | ❌ | ✅ (interfaces) | ❌ | ❌ |
| **Macros** | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| **Significant Whitespace** | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ |
| **UE5 Integration** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Multi-Target** | ✅ (15+) | ✅ (LLVM) | ❌ | ✅ (JS) | ✅ (LLVM) | ✅ (BEAM) |

### 10.4 Statistics

**Language Complexity:**
- 25 top-level item types
- 50+ expression types
- 10+ pattern types
- 60+ token types
- 8 effect types
- 15+ compilation targets

**Codebase Size:**
- `kain-core`: ~15,000 lines (parser, AST, types, effects, comptime, monomorphize)
- `ue5`: ~20,000 lines (runtime codegen)
- `ue5-editor`: ~15,000 lines (Slate, Details, Viewports, Toolbars)
- `ue5-shaders`: ~8,000 lines (USF, HLSL, SPIR-V)
- `ue5-materials`: ~6,000 lines (material graphs, binary .uasset)
- `ue5-blueprints`: ~5,000 lines (UK2Node, Kismet bytecode)
- `ue5-graphs`: ~7,000 lines (graph editors + runtime)
- `cli`: ~10,000 lines (packager, module system)
- **Total**: ~100,000+ lines of Rust

**Test Coverage:**
- 386 tests passing
- 148 tests in `ue5`
- 85 tests in `ue5-shaders`
- 58 tests in `ue5-graphs`
- 38 tests in `ue5-editor`
- 36 tests in `ue5-materials`
- 21 tests in `ue5-blueprints`


**Production Plugins:**
- 20 UE5 plugins compiled successfully
- VoxelForgePro: 1,943 KAIN lines → 15,000 C++ lines
- TitanGraph: 1,692 KAIN lines → 10,000 C++ lines
- AeroTunnel: 1,620 KAIN lines → 12,000 C++ lines
- Average compression: 1:5 (base) → 1:20 (with stdlib)

**Stdlib:**
- 200+ functions across 12 categories
- Auto-discovery (no configuration needed)
- 1:20 compression ratio
- Categories: shaders, actor, gameplay, utilities, world, skeletal_mesh, materials, particles, components, patterns, math, common

---

## Appendix A: Complete Token List

### Keywords (40)
```
fn, let, mut, var, const, if, else, elif, match, for, while, loop,
break, continue, return, await, in, with, as, type, struct, enum,
trait, impl, pub, mod, use, self, Self, true, false, none,
component, shader, actor, state, spawn, send, receive, emit,
comptime, macro, vertex, fragment, test, async, Pure, IO, Async,
GPU, Reactive, Unsafe
```

### Operators (30)
```
+, -, *, /, %, **, ==, !=, <, >, <=, >=, &&, ||, !, &, |, ^, ~,
<<, >>, =, +=, -=, *=, /=, .., ..., ->, =>
```

### Punctuation (15)
```
(, ), [, ], {, }, ,, ., :, ::, ;, @, ?, </, 
```

### Literals (5)
```
Int(i64), Float(f64), String(String), FString(String), Char(String)
```

### Special (4)
```
Ident(String), Newline(String), Indent, Dedent, Eof
```

---

## Appendix B: Complete Effect List

| Effect | Symbol | Description | Subset Of |
|--------|--------|-------------|-----------|
| Pure | `Pure` | No side effects | All |
| IO | `IO` | File/Network/Console I/O | Unsafe |
| Async | `Async` | Can await futures | Unsafe |
| GPU | `GPU` | Runs on graphics hardware | Unsafe |
| Reactive | `Reactive` | Triggers UI updates | Unsafe |
| Unsafe | `Unsafe` | Breaks safety guarantees | None |
| Alloc | `Alloc` | Memory allocation | Unsafe |
| Panic | `Panic` | Can abort execution | Unsafe |

**Effect Hierarchy:**
```
Pure ⊆ IO ⊆ Unsafe
Pure ⊆ Async ⊆ Unsafe
Pure ⊆ GPU ⊆ Unsafe
Pure ⊆ Reactive ⊆ Unsafe
Pure ⊆ Alloc ⊆ Unsafe
Pure ⊆ Panic ⊆ Unsafe
```


---

## Appendix C: Complete Type List

### Primitive Types
- `Int` (i64)
- `Float` (f64)
- `Bool`
- `String`
- `Char`
- `Unit` (())
- `Never` (!)

### Composite Types
- `Tuple(Vec<Type>)` - `(A, B, C)`
- `Array(Box<Type>, usize)` - `[T; N]`
- `Slice(Box<Type>)` - `[T]`
- `Option(Box<Type>)` - `T?`
- `Result(Box<Type>, Box<Type>)` - `T!E`

### Reference Types
- `Ref { mutable: bool, inner: Box<Type>, lifetime: Option<String> }` - `&T`, `&mut T`

### Function Types
- `Function { params: Vec<Type>, return_type: Box<Type>, effects: Vec<Effect> }` - `fn(A, B) -> C with Effects`

### Named Types
- `Named { name: String, generics: Vec<Type> }` - `Vec<T>`, `Option<Int>`

### Special Types
- `Infer` - `_` (type inference placeholder)
- `Impl { trait_name: String, generics: Vec<Type> }` - `impl Trait`

### UE5 Types (mapped from KAIN)
- `Vec2` → `FVector2D`
- `Vec3` → `FVector`
- `Vec4` → `FVector4`
- `Quat` → `FQuat`
- `Transform` → `FTransform`
- `Color` → `FLinearColor`
- `Rotator` → `FRotator`

---

## Appendix D: Attribute Reference

### Struct-Level Attributes
- `@component` - UActorComponent generation
- `@datatable` - FTableRowBase for CSV import
- `@subsystem` - UWorldSubsystem generation
- `@tick` - TickComponent() override
- `@beginplay` - BeginPlay() override
- `@slate` - SCompoundWidget generation
- `@details` - IDetailCustomization generation
- `@viewport` - SEditorViewport generation
- `@toolbar` - FToolBarBuilder generation
- `@asset_editor` - FAssetEditorToolkit generation
- `@editor_module` - IModuleInterface generation
- `@async_task` - FRunnable task generation
- `@state_machine` - State machine runtime
- `@graph_runtime` - Graph runtime system
- `@graph_editor` - UEdGraph editor
- `@material_graph` - Material graph definition
- `@material_function` - Material function definition
- `@ability` - Gameplay Ability
- `@gameplay_effect` - Gameplay Effect
- `@gameplay_cue` - Gameplay Cue
- `@ability_task` - Ability Task
- `@target_actor` - Target Actor


### Field-Level Attributes
- `@replicated` - Network replication
- `@replicated(mode: "interpolated", back_time: 0.1)` - Advanced replication
- `@savegame` - Saved to disk
- `@transient` - Not serialized
- `@editdefaults` - Editable in defaults
- `@visibleonly` - Visible but not editable
- `@category("X")` - Blueprint category
- `@slider(min, max)` - Slider widget in details panel
- `@color_picker` - Color picker widget
- `@property` - Generic property marker
- `@scene_actor` - Scene actor in viewport
- `@camera` - Camera setup
- `@input` - Input field (async task)
- `@output` - Output field (async task)
- `@input_pin` - Input pin (graph)
- `@output_pin` - Output pin (graph)

### Function-Level Attributes
- `@blueprint` - UBlueprintFunctionLibrary
- `@blueprint_callable` - UFUNCTION(BlueprintCallable)
- `@blueprint_pure` - UFUNCTION(BlueprintPure)
- `@blueprint_event` - UFUNCTION(BlueprintNativeEvent)
- `@blueprint_implementable_event` - UFUNCTION(BlueprintImplementableEvent)
- `@inline` - Inline function body in header
- `@button(label)` - Button in details panel
- `@toggle(label)` - Toggle in toolbar
- `@dropdown(label)` - Dropdown in toolbar
- `@menu_entry(path, label)` - Menu entry in editor
- `@toolbar_button(section, icon)` - Toolbar button
- `@callback(thread)` - Async task callback

### Actor-Level Attributes
- `@base("ACharacter")` - Custom base class
- `@uclass("Blueprintable", "Abstract")` - UCLASS specifiers

### Shader Attributes
- `shader compute` - Compute shader
- `shader fragment` - Fragment shader
- `shader surface` - Surface shader (UE5)
- `shader vertex` - Vertex shader
- `uniform X: Type @N` - Uniform with binding slot
- `CFG_*` / `ENABLE_*` - Shader permutations

---

## Appendix E: Standard Library Categories

### 1. Shaders (100+ functions)
- PBR: `fresnel_schlick`, `ggx_distribution`, `schlick_ggx`
- Noise: `perlin_noise`, `simplex_noise`, `voronoi`
- Color: `rgb_to_hsv`, `hsv_to_rgb`, `color_grade`
- UV: `uv_scroll`, `uv_scale`, `uv_rotate`, `uv_tile`
- Volumetric: `ray_march`, `volumetric_fog`
- SSS: `subsurface_scattering`
- Post-processing: `bloom`, `tone_map`, `vignette`
- SDF: `sdf_sphere`, `sdf_box`, `sdf_union`

### 2. Actor (30+ functions)
- Lifecycle: `BeginPlay`, `Tick`, `EndPlay`
- Transform: `GetActorLocation`, `SetActorLocation`, `GetActorRotation`
- Attachment: `AttachToActor`, `DetachFromActor`
- Velocity: `GetVelocity`, `SetVelocity`
- Component: `GetComponentByClass`, `AddComponent`


### 3. Gameplay (20+ functions)
- Damage: `apply_damage`, `calculate_damage`, `damage_with_falloff`
- Health: `heal`, `is_alive`, `get_health_percentage`
- XP: `add_experience`, `level_up`, `get_level`
- Inventory: `add_item`, `remove_item`, `has_item`
- Cooldowns: `start_cooldown`, `is_on_cooldown`
- Buffs: `apply_buff`, `remove_buff`, `has_buff`
- Loot: `roll_loot`, `spawn_loot`
- Quests: `start_quest`, `complete_quest`, `update_objective`

### 4. Utilities (20+ functions)
- Math: `remap`, `lerp`, `clamp`, `smoothstep`
- Interpolation: `ease_in`, `ease_out`, `ease_in_out`
- Random: `random_float`, `random_int`, `random_range`
- String: `format`, `concat`, `split`, `trim`

### 5. World (20+ functions)
- Queries: `line_trace`, `sphere_trace`, `box_trace`
- Spawning: `spawn_actor`, `spawn_emitter`, `spawn_sound`
- Debug: `draw_debug_line`, `draw_debug_sphere`, `draw_debug_box`
- Game Mode: `get_game_mode`, `get_game_state`, `get_player_controller`

### 6. Skeletal Mesh (20+ functions)
- Montages: `play_montage`, `stop_montage`, `get_montage_position`
- Bones: `get_bone_location`, `get_bone_rotation`, `get_socket_location`
- Morph Targets: `set_morph_target`, `get_morph_target`

### 7. Materials (15+ functions)
- Parameters: `set_scalar_parameter`, `set_vector_parameter`, `set_texture_parameter`
- Dynamic: `create_dynamic_material`, `get_material`
- Collections: `set_collection_scalar`, `get_collection_scalar`

### 8. Particles (15+ functions)
- Niagara: `set_niagara_variable`, `get_niagara_variable`, `spawn_niagara_system`
- Control: `activate_system`, `deactivate_system`, `reset_system`
- Pooling: `get_pooled_system`, `return_to_pool`

### 9. Components (10+ functions)
- Common structs: `HealthComponent`, `InventoryComponent`, `MovementComponent`, `CombatComponent`

### 10. Patterns (12+ functions)
- Shared types: `LootRarity`, `BuffType`, `DamageType`, `WeaponStats`

### 11. Math (11+ functions)
- Vector: `dot`, `cross`, `normalize`, `length`, `distance`
- Rotation: `rotate_vector`, `look_at_rotation`
- Interpolation: `vector_lerp`, `quat_slerp`
- Type aliases: `Vec2`, `Vec3`, `Vec4`, `Quat`

### 12. Common (3+ functions)
- Core: `GetWorldDeltaSeconds`, `GetActorLocation`, `GetGameTimeSinceCreation`

---

## Appendix F: Compilation Pipeline

```
Source Code (.kn files)
    ↓
[1] Stdlib Discovery & Prepending
    ↓ (200+ functions auto-discovered)
Full Source (stdlib + user code)
    ↓
[2] Lexer (lexer.rs)
    ↓ (60+ token types, INDENT/DEDENT)
Token Stream
    ↓
[3] Parser (parser.rs)
    ↓ (25 item types, 50+ expr types)
Abstract Syntax Tree (AST)
    ↓
[4] Comptime Evaluator (comptime.rs)
    ↓ (Zig-style compile-time execution)
AST with Evaluated Comptime
    ↓
[5] Type Checker (types.rs)
    ↓ (Type inference, unification)
Typed AST (TypedProgram)
    ↓
[6] Effect Checker (effects.rs)
    ↓ (Effect inference, validation)
Typed AST with Effects
    ↓
[7] Monomorphizer (monomorphize.rs)
    ↓ (Generic instantiation, async lowering)
Monomorphized Program
    ↓
[8] Codegen Backend Selection
    ├─→ WASM (95KB backend)
    ├─→ JavaScript (ES6+)
    ├─→ TypeScript (full types)
    ├─→ LLVM (66KB backend)
    ├─→ Rust (28KB transpiler)
    ├─→ C++ (full C++17)
    ├─→ UE5 Runtime (7 crates)
    ├─→ UE5 Editor (Slate, Details, etc.)
    ├─→ SPIR-V (14KB backend)
    ├─→ HLSL (25KB backend)
    ├─→ USF (20KB backend)
    └─→ Interpret (runtime)
    ↓
Output Files
```


---

## Appendix G: Module Structure

```
kain-core/
├── src/
│   ├── lib.rs              # Main entry point, re-exports
│   ├── lexer.rs            # Tokenization (1,200 lines)
│   ├── ast.rs              # Abstract Syntax Tree (2,300 lines)
│   ├── parser.rs           # Recursive descent parser (8,000+ lines)
│   ├── types.rs            # Type system, inference (3,000+ lines)
│   ├── effects.rs          # Effect system (200 lines)
│   ├── comptime.rs         # Compile-time evaluation (150 lines)
│   ├── diagnostics.rs      # Error reporting, SpanMapper (300 lines)
│   ├── monomorphize.rs     # Generic instantiation (1,746 lines)
│   ├── runtime.rs          # Interpreter (when runtime feature enabled)
│   ├── stdlib.rs           # Stdlib discovery & loading (200 lines)
│   ├── stdlib_tests.rs     # Stdlib unit tests
│   ├── shader_analysis.rs  # Shader complexity analysis
│   ├── asm_ir.rs           # Assembly IR (100 lines)
│   ├── span.rs             # Source location tracking
│   └── error.rs            # Error types (300 lines)
└── tests/
    └── (integration tests)
```

---

## Appendix H: Key Design Decisions

### 1. Significant Whitespace
**Decision:** Python-style indentation with INDENT/DEDENT tokens  
**Rationale:** Reduces visual noise, enforces consistent formatting, familiar to Python/YAML users  
**Trade-off:** Requires careful handling of mixed tabs/spaces

### 2. Effect System
**Decision:** Compile-time effect tracking with explicit annotations  
**Rationale:** Prevents accidental side effects, enables optimization, documents function behavior  
**Trade-off:** Requires effect annotations on function signatures

### 3. First-Class Constructs
**Decision:** Components, Shaders, Actors as top-level items (not libraries)  
**Rationale:** Makes common patterns ergonomic, enables specialized codegen, reduces boilerplate  
**Trade-off:** Increases language complexity

### 4. Multi-Target Compilation
**Decision:** 15+ compilation targets from single source  
**Rationale:** Write once, run anywhere (web, native, GPU, UE5)  
**Trade-off:** Complex backend maintenance

### 5. UE5 Integration Depth
**Decision:** 25+ specialized UE5 constructs in the language  
**Rationale:** Achieves 1:20 compression ratio, eliminates boilerplate, type-safe UE5 code  
**Trade-off:** Tight coupling to UE5 API

### 6. Stdlib Auto-Discovery
**Decision:** Automatically prepend 200+ stdlib functions to every compilation  
**Rationale:** Zero configuration, consistent API, massive compression  
**Trade-off:** Longer compilation times (mitigated by caching)

### 7. Monomorphization Strategy
**Decision:** Eager instantiation with type inference  
**Rationale:** Predictable performance, no runtime overhead, full type checking  
**Trade-off:** Code bloat for heavily generic code

### 8. Async Lowering
**Decision:** Transform async functions into state machines at compile time  
**Rationale:** Zero-cost async, no runtime dependency, compatible with all targets  
**Trade-off:** Complex codegen, larger binary size


---

## Appendix I: Future Roadmap

### Planned Features
- [ ] Lifetime elision (reduce explicit lifetime annotations)
- [ ] Async/await syntax sugar improvements
- [ ] Pattern guards in let bindings
- [ ] Destructuring in function parameters
- [ ] Multi-line string literals
- [ ] Raw string literals (r"...")
- [ ] Byte string literals (b"...")
- [ ] Const generics (const N: usize)
- [ ] Associated types in traits
- [ ] Higher-kinded types
- [ ] Linear types (affine types)
- [ ] Dependent types (experimental)

### UE5 Roadmap
- [ ] GAS integration (in progress)
- [ ] Timeline/Sequencer integration
- [ ] Mesh manipulation (procedural geometry)
- [ ] AI integration (behavior trees, EQS)
- [ ] Animation blueprints
- [ ] Control Rig integration
- [ ] Niagara graph editor
- [ ] PCG (Procedural Content Generation)
- [ ] MetaHuman integration (extension available)

### Backend Roadmap
- [ ] C backend (for embedded systems)
- [ ] Swift backend (for iOS/macOS)
- [ ] Kotlin backend (for Android)
- [ ] Go backend (for microservices)
- [ ] Python backend (for scripting)
- [ ] CUDA backend (for GPU compute)
- [ ] Metal backend (for Apple GPUs)
- [ ] Vulkan backend (for cross-platform graphics)

---

## Appendix J: Learning Resources

### Official Documentation
- `Kain/docs/recent/README.MD` - V1 compiler documentation
- `Kain/docs/recent/PARSER_AST_GUIDE.md` - Parser deep-dive (27KB)
- `Kain/docs/recent/AI_PLUGIN_CREATION_GUIDE.md` - LLM guide (21KB)
- `Kain/crates/*/CRATE_REFERENCE.md` - Per-crate documentation
- `Kain/docs/CRATE_INDEX.md` - Master index

### Language Patterns
- `.kiro/steering/kain-patterns.md` - Best practices
- `.kiro/steering/llm-first-development.md` - LLM-first philosophy

### Stdlib
- `Kain/stdlib/USAGE_GUIDE.md` - How to use stdlib
- `Kain/stdlib/PATTERN_EXTRACTION_GUIDE.md` - Extract patterns
- `Factory/_Docs/STDLIB_BACKEND_RUNDOWN.md` - Backend wiring

### Examples
- `Factory/Example/` - Basic plugin example
- `Factory/Example_Blueprint/` - Blueprint integration
- `Factory/Example_Graph/` - Graph editor example
- `Factory/Example_Material/` - Material graph example
- `Factory/Example_Slate/` - Slate UI example

### Research
- `Research/ReferencePatterns/` - 29 UE5 pattern taxonomies
- `Research/_docs/CODEGEN_ARCHITECTURE_ANALYSIS.md` - Architecture (25KB)
- `Research/_docs/FINALSUMMARY.md` - Marketplace battle analysis (33KB)


---

## Appendix K: FAQ

### Q: Is KAIN production-ready?
**A:** Yes. 20 UE5 plugins have been successfully compiled and tested. The compiler has 386 passing tests and has been battle-tested against 9 marketplace plugins.

### Q: What's the learning curve?
**A:** If you know Rust + Python, you'll feel at home. The syntax is familiar, and the effect system is intuitive. UE5-specific features require UE5 knowledge.

### Q: How does KAIN compare to C++ for UE5?
**A:** KAIN achieves 1:5 compression (base) to 1:20 (with stdlib). It eliminates boilerplate, provides type safety, and generates production-quality C++.

### Q: Can I mix KAIN and C++ in the same project?
**A:** Yes. KAIN generates standard UE5 C++ that interoperates seamlessly with existing C++ code.

### Q: Does KAIN support hot reload?
**A:** Yes. The metadata system supports hot-reload, and generated C++ works with UE5's hot reload.

### Q: What's the compilation speed?
**A:** Fast. The compiler is written in Rust and uses parallel compilation. Stdlib auto-discovery adds minimal overhead.

### Q: Can I use KAIN for non-UE5 projects?
**A:** Absolutely. KAIN targets WASM, JS, TS, LLVM, Rust, C++, and more. UE5 is just one of 15+ targets.

### Q: Is there IDE support?
**A:** Yes. KAIN has an LSP (Language Server Protocol) implementation for autocomplete, diagnostics, hover, and go-to-definition.

### Q: How stable is the language?
**A:** The core language is stable. UE5 features are production-ready. New features are added regularly but don't break existing code.

### Q: What's the license?
**A:** Check the repository for the current license. The compiler is open-source.

---

## Appendix L: Glossary

**AST** - Abstract Syntax Tree. The tree representation of source code after parsing.

**Comptime** - Compile-time. Code that executes during compilation, not at runtime.

**Effect** - A side effect tracked by the type system (IO, Async, GPU, etc.).

**Monomorphization** - The process of instantiating generic functions/types with concrete types.

**Span** - A byte range in source code (start, end). Used for error reporting.

**SpanMapper** - Converts byte offsets (spans) to human-readable file:line:col locations.

**Stdlib** - Standard library. 200+ functions automatically available in every KAIN program.

**Trait** - An interface definition. Similar to Rust traits or TypeScript interfaces.

**Unification** - The algorithm for inferring type arguments by matching parameter types with argument types.

**UE5** - Unreal Engine 5. A game engine by Epic Games.

**UCLASS** - Unreal Engine class macro. Marks a C++ class as a UE5 type.

**UPROPERTY** - Unreal Engine property macro. Marks a field for reflection/serialization.

**UFUNCTION** - Unreal Engine function macro. Marks a method for Blueprint exposure.

**USF** - Unreal Shader File. UE5's shader format.

**HLSL** - High-Level Shading Language. DirectX shader language.

**SPIR-V** - Standard Portable Intermediate Representation. Cross-platform shader IR.

**WASM** - WebAssembly. Binary instruction format for web browsers.

**LLVM** - Low-Level Virtual Machine. Compiler infrastructure for native code generation.

---

## Conclusion

KAIN is a **multi-paradigm systems language** that combines the best features of Rust, Python, Lisp, Zig, and Erlang, with first-class support for Unreal Engine 5. It achieves unprecedented compression ratios (1:20 with stdlib) while maintaining type safety, memory safety, and zero-cost abstractions.

**Key Strengths:**
- 25 top-level item types (most comprehensive AST in modern languages)
- 50+ expression types with full pattern matching
- Effect system for compile-time side effect tracking
- 15+ compilation targets from single source
- 200+ stdlib functions with auto-discovery
- 25+ UE5-specific constructs for game development
- Production-ready (20 plugins, 386 tests passing)

**Use Cases:**
- Game development (UE5 plugins)
- Web applications (WASM, JS, TS)
- Systems programming (LLVM, Rust, C++)
- GPU computing (SPIR-V, HLSL, USF)
- Embedded systems (assembly import)
- Concurrent systems (actors)
- UI development (components, Slate)

**Next Steps:**
1. Read the examples in `Factory/Example*/`
2. Try the REPL: `kain run examples/hello.kn`
3. Build a UE5 plugin: `kain init MyPlugin --ue5`
4. Explore the stdlib: `Kain/stdlib/ue5/`
5. Join the community (check repository for links)

---

**Document Version:** 1.0  
**Last Updated:** February 2026  
**Compiler Version:** Production (100,000+ LOC)  
**Test Coverage:** 386 tests passing  
**Production Plugins:** 20 compiled successfully

