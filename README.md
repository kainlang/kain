# KAIN — Canonical LLM Bootstrap

> **KAIN is a compiled multi-paradigm systems language for universal compilation. One `.kn` source can target web, native, GPU, scripting, and Unreal Engine 5 codegen pipelines.**

```
.kn source
  -> kain build / run / import-*
  -> WASM | JS | TS | KS | LLVM/native | Rust | C++ | SPIR-V | HLSL | USF | UE5 plugin | UE5 editor | run | test
```

This file is intentionally optimized for agent and LLM onboarding, not human marketing. It is the top-level operational brief: language model, command surface, target model, importer model, backend capabilities, repository layout, and current project status in one pass.

---

## Read This First

- Treat this README as the top-level source of truth for KAIN concepts and CLI usage.
- Prefer the modern subcommand CLI: `kain build`, `kain run`, `kain import-c`, `kain import-rust`, `kain import-ts`, `kain import-asm`, `kain inject`, `kain gpu-artifacts`, `kain doctor`, `kain lsp`.
- Legacy positional invocation still exists, but it is compatibility behavior, not the primary interface.
- KAIN is designed around universal compilation, data-driven stdlib injection, effect tracking, foreign-source import, and strong UE5 codegen support.
- When reasoning about KAIN output, distinguish between language features, importer transforms, and backend codegen. They are related but not identical layers.

---

## Fast Operational Snapshot

| Area | Current State | Notes |
|------|---------------|-------|
| Core language | Active | Systems language with effect tracking, actors, shaders, comptime, generics, traits, pattern matching |
| CLI | Active | Subcommand-based interface is canonical |
| Web targets | Available | WASM, JS, TS, KS, hybrid |
| System targets | Available | LLVM/native, Rust, C++ |
| GPU targets | Available | SPIR-V, HLSL, USF |
| UE5 backend | Advanced | Runtime, editor, graphs, shaders, materials, blueprints |
| C import | Production-ready | Full C11-oriented pipeline with preprocessor support |
| Rust import | Active development | Project Ouroboros self-hosting path |
| TypeScript import | Production-ready | TS/TSX support via SWC |
| Assembly import | Production-ready | Game Boy LR35902, 6502, Z80 |
| C++ import | Planned | Stub exists |
| Python import | Planned | Not yet active as a source importer |

---

## Build Commands You Should Know

These commands matter because they define the actual operational model of the language.

```bash
# Diagnose the compiler and enabled capabilities
kain doctor

# Build from project config (KAIN.toml)
kain build

# Build a file to a specific target
kain build src/main.kn --target wasm
kain build src/main.kn --target rust
kain build src/shader.kn --target spirv
kain build src/shader.kn --target hlsl
kain build src/shader.kn --target usf

# Build UE5 output from project config
kain build --ue5

# Build multiple targets
kain build --targets wasm,js,rust

# Immediate execution / interpreter
kain run examples/hello.kn

# Source import pipelines
kain import-c src/main.c --output main.kn
kain import-rust crates/kain-core/src --output kain-core.kn --flat
kain import-ts src/app.ts --output app.kn
kain import-asm firmware.asm --format gameboy --out game.kn

# UE5 incremental injection into existing plugin
kain inject src/new_actor.kn --ue5

# Generate paired GPU artifacts
kain gpu-artifacts src/shader.kn --output dist/
```

---

## What KAIN Is

KAIN is a multi-paradigm systems language designed to unify:

- Rust-style safety and low-level control
- Python-style readability and significant whitespace
- Lisp-style metaprogramming and DSL orientation
- Zig-style compile-time execution
- Erlang-style actor concurrency
- Data-driven codegen across many backends

The core value proposition is not just syntax. It is that one KAIN source model can be compiled into multiple execution environments and code ecosystems, including browser/web, native, GPU shading pipelines, scripting output, and Unreal Engine 5 plugin generation.

---

## Language Surface

### Core Syntax

**Functions with effect tracking:**
```kain
fn factorial(n: Int) -> Int with Pure:
    match n:
        0 => 1
        _ => n * factorial(n - 1)

fn read_config(path: String) -> String with IO:
    return read_file(path)

async fn fetch_data(url: String) -> Result<String, Error> with Async, IO:
    let response = await http_get(url)
    return response
```

**Structs and enums:**
```kain
struct Player:
    name: String
    health: Float
    max_health: Float
    position: Vec3

enum GameState:
    MainMenu
    Playing(Level)
    Paused
    GameOver { score: Int, time: Float }
```

**Pattern matching:**
```kain
fn handle_input(key: Key) -> Action:
    match key:
        Key::W | Key::Up => Action::MoveForward
        Key::S | Key::Down => Action::MoveBackward
        Key::Space => Action::Jump
        _ => Action::None
```

**Traits and generics:**
```kain
trait Drawable:
    fn draw(self) -> Unit

impl Drawable for Player:
    fn draw(self):
        println("Drawing player at {self.position}")

fn map<T, U>(arr: Array<T>, f: fn(T) -> U) -> Array<U>:
    let result: Array<U> = []
    for item in arr:
        push(result, f(item))
    return result
```

**Async/await:**
```kain
async fn process_batch(items: Array<String>) -> Array<Result<Data, Error>> with Async, IO:
    let results: Array<Result<Data, Error>> = []
    for item in items:
        let data = await fetch_data(item)
        push(results, data)
    return results
```

### First-Class Domains

**Actor concurrency:**
```kain
actor ChatRoom:
    var messages: Array<String> = []
    var users: Array<String> = []

    on Join(name: String):
        push(users, name)
        broadcast("{name} joined")

    on Message(from: String, text: String):
        push(messages, "{from}: {text}")
        broadcast("{from}: {text}")
```

**Reactive components:**
```kain
component Counter(initial: Int) -> UI with Reactive:
    state count: Int = initial

    fn increment():
        count = count + 1

    return <div>
        <p>Count: {count}</p>
        <button onClick={increment}>Increment</button>
    </div>
```

**GPU shaders:**
```kain
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform base_color: Vec3 @0
    uniform albedo_map: Sampler2D @1

    let tex_color = sample(albedo_map, uv).rgb
    return vec4(tex_color * base_color, 1.0)

shader compute VoxelGenerator(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform noise_scale: Float @1
    buffer output: RWBuffer<Float> @2

    let noise = perlin_noise(thread_id * noise_scale)
    output[thread_id.x] = noise
```

**Compile-time execution:**
```kain
comptime:
    let build_config = read_file("config.json")
    let optimizations = parse_json(build_config)

fn optimized_function() -> Int:
    comptime:
        if optimizations.level > 2:
            return inline_version()
        else:
            return standard_version()
```

**Low-level memory operations:**
```kain
@c_packed
struct PackedData:
    @c_bitfield(3, true)
    flags: Int
    @c_bitfield(5, false)
    count: Int
    value: Float

fn manipulate_memory(data: Ptr<PackedData>):
    let flags = mem_load(data).flags
    mem_store(data, PackedData { flags: flags | 0x4, count: 10, value: 3.14 })
```

**Python FFI:**
```kain
fn calculate_advanced_math(x: Float) -> Float with IO:
    let result = py_call("scipy.special.gamma", [x])
    return result
```

---

## Semantic Model

### Effect System

KAIN tracks side effects in the type system.

| Effect | Meaning | Example |
|--------|---------|---------|
| `Pure` | No side effects | `fn factorial(n: Int) -> Int with Pure` |
| `IO` | File, network, console | `fn read_file(path: String) -> String with IO` |
| `Async` | Can `await` | `async fn fetch() -> Data with Async, IO` |
| `GPU` | Runs on graphics hardware | `shader compute(...) with GPU` |
| `Reactive` | Triggers UI updates | `component Counter() -> UI with Reactive` |
| `Unsafe` | Breaks safety guarantees | `fn raw_ptr_access() with Unsafe` |
| `Alloc` | Performs allocation | `fn create_buffer() with Alloc` |
| `Panic` | Can abort | `fn assert_valid(x: Int) with Panic` |

Effects form a lattice: `Pure` is the most restrictive bottom, `Unsafe` is the permissive top.

### Type System

- Primitives: `Int`, `Float`, `Bool`, `String`, `Char`
- Sized integers: `i8`, `i16`, `i32`, `i64`, `i128`, `u8`, `u16`, `u32`, `u64`, `u128`, `isize`, `usize`
- Sized floats: `f32`, `f64`
- Collections: `Array<T>`, `Slice<T>`, `Tuple(T, U, ...)`, `Map<K, V>`, `Set<T>`
- Sum/product helpers: `Option<T>`, `Result<T, E>`, `Unit`, `Never`
- References and pointers: `&T`, `&mut T`, `Ptr<T>`, `PtrMut<T>`
- Function types: `fn(T, U) -> V`, `fn(T) -> U with Effects`
- GPU types: `Vec2`, `Vec3`, `Vec4`, `Mat2`, `Mat3`, `Mat4`, `Sampler2D`, `Sampler3D`

### Standard Library

KAIN prepends a data-driven stdlib to every compilation.

- I/O: `print`, `println`, `read_line`, `read_file`, `write_file`
- Collections: `push`, `pop`, `len`, `map`, `filter`, `reduce`, `sort`, `reverse`
- Math: `abs`, `min`, `max`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`
- String: `split`, `join`, `trim`, `replace`, `substring`, `to_upper`, `to_lower`
- JSON: `json_parse`, `json_stringify`
- HTTP: `http_get`, `http_post`
- Shaders: `fresnel_schlick`, `ggx_distribution`, `smith_geometry`, `perlin_noise`, `simplex_noise`, `worley_noise`, `rgb_to_hsv`, `hsv_to_rgb`, `color_grade`, `uv_scroll`, `uv_scale`, `uv_rotate`
- Gameplay: `apply_damage`, `calculate_xp`, `check_cooldown`, `roll_loot`

Compression model: 1 line KAIN -> 5-8 lines C++ base output -> often 1:20+ when stdlib expansion is included.

### Influences

| Influence | What KAIN Takes | How It Shows Up |
|-----------|----------------|-----------------|
| Rust | Ownership, borrowing, no null, no data races | Memory-safe by default, low-level systems orientation |
| Python | Significant whitespace, minimal ceremony | Readable syntax, rapid authoring |
| Lisp | Code as data, hygienic macros | DSL-friendly metaprogramming |
| Zig | Compile-time execution | `comptime` blocks without separate macro language |
| Erlang | Actor concurrency | Built-in actor/message model |
| Effect systems | Typed side effects | `with Pure`, `with IO`, `with Async` |

### Differentiators

1. One language, many targets.
2. Universal source import from foreign languages into KAIN IR.
3. Actor-native concurrency.
4. Effect tracking in the type system.
5. Compile-time execution without a separate macro language.
6. Self-hosting path via Project Ouroboros.
7. Python FFI.
8. Production-grade low-level memory layer and C ABI compatibility.
9. UE5 as a first-class backend target.
10. Data-driven stdlib automatically available during compilation.

---

## Compilation Targets (15+)

| Target | Primary Flag | Output | Use Case |
|--------|--------------|--------|----------|
| WebAssembly | `-t wasm` | `.wasm` | Web apps, edge runtimes |
| JavaScript | `-t js` | `.js` | Node.js, browser execution |
| TypeScript | `-t ts` | `.ts` | Typed web development |
| KainScript | `-t ks` | `.ks` | JavaScript with JSDoc types, runs without TS compilation |
| Hybrid | `-t hybrid` | WASM + JS | Full-stack web applications |
| LLVM / Native | `-t llvm` | executable / LLVM IR | Native binaries, systems work |
| Rust | `-t rust` | `.rs` | Interop, bootstrap, inspection |
| C++ | `-t cpp` | `.cpp/.h` | C++ interop |
| SPIR-V | `-t spirv` | `.spv` | Cross-platform GPU shaders |
| HLSL | `-t hlsl` | `.hlsl` | DirectX shaders |
| USF | `-t usf` | `.usf` | Unreal Engine 5 shader generation |
| UE5 C++ | `--ue5` | full plugin | Runtime plugin generation |
| UE5 Editor | `-t ue5editor` | editor C++ | Slate, details, viewports, toolbars |
| Interpret | `-t run` | stdout | REPL/immediate execution |
| Test | `-t test` | stdout | Unit test runner |

---

## Universal Source Import — MAJOR FEATURE

KAIN can **import foreign language source code** and transform it into KAIN IR, which can then be compiled to any KAIN backend target.

**Supported Languages:**
- ✅ **C** (Production-Ready) — Full C11 support with preprocessor
- ✅ **Rust** (Active Development) — Project Ouroboros self-hosting
- ✅ **TypeScript** (Production-Ready) — Full TS/TSX support via SWC
- ✅ **Assembly** (Production-Ready) — 3 CPU dialects (Game Boy, 6502, Z80)
- 🔜 **C++** (Planned) — Stub exists
- 🔜 **Python** (Planned)

### C Import (Production-Ready)

Import C source code directly into KAIN:

```bash
# Single file
kain import-c ./physics.c --output ./physics.kn

# Single file → KAIN → TypeScript
kain import-c ./physics.c --output ./physics.kn --target ts

# Directory import with failure reporting
kain import-c ./src --output ./game.kn --report-json ./game.import_report.json

# With include paths and defines
kain import-c ./src/main.c --output ./main.kn -I ./include -D DEBUG
```

**Supported C Features:**
- ✅ Structs, unions, enums, typedefs
- ✅ Function definitions with full parameter/return types
- ✅ Pointers, arrays, function pointers
- ✅ Bitfields with ABI-aware packing
- ✅ `#pragma pack`, `__attribute__((packed))`, `__attribute__((aligned(N)))`
- ✅ Memory operations: `&x`, `*ptr`, `ptr[i]`, `sizeof`, `_Alignof`
- ✅ Dynamic allocation: `malloc`, `calloc`, `realloc`
- ✅ Designated initializers
- ✅ Preprocessor: `-I` include paths, `-D` defines

**Example:** Import the entire Super Mario 64 decompilation project:
```bash
kain import-c ./sm64-master/src --output sm64.kn --flat
```

### Rust Import (Active Development — Project Ouroboros)

Import Rust source code for **self-hosting the KAIN compiler**:

```bash
# Single file
kain import-rust ./crates/kain-core/src/lib.rs --output kain-core.kn

# Directory import (flat mode for self-hosting)
kain import-rust ./crates/kain-core/src --output kain-core.kn --flat

# Import + compile
kain import-rust ./src/main.rs --output main.kn --target wasm
```

**Supported Rust Features:**
- ✅ Functions (with `unsafe` → `Effect::Unsafe`, `async` → `Effect::Async`)
- ✅ Structs (named, tuple, unit)
- ✅ Enums (unit, tuple, struct variants)
- ✅ Impl blocks (methods, trait impls)
- ✅ Constants, statics, type aliases
- ✅ Modules (inline and external)
- ✅ Pattern matching
- ✅ Smart pointers (`Box`, `Arc`, `Rc` → transparent unwrapping)
- ✅ Collections (`Vec` → `Array`, `HashMap` → `Map`)
- ✅ References (`&T`, `&mut T` → `Type::Ref`)
- ✅ Raw pointers (`*const T`, `*mut T` → `Type::Ptr`)

**Project Ouroboros Pipeline:**
```
Rust Compiler Source (kain-core/*.rs)
    ↓
kain import-rust --flat
    ↓
KAIN Source (kain-core.kn)
    ↓
kain build -t rust
    ↓
Generated Rust (kain-core-generated.rs)
    ↓
cargo build + tests pass = ✅ Self-hosted
```

### TypeScript Import (Production-Ready)

Import TypeScript source code directly into KAIN:

```bash
# Single file
kain import-ts ./src/app.ts --output src/app.kn

# Single file → KAIN → WASM
kain import-ts ./src/app.ts --output src/app.kn --target wasm

# Directory import with failure reporting
kain import-ts ./src --output app.kn --report-json app.import_report.json

# Flat mode (merge all files into top-level scope)
kain import-ts ./src --output app.kn --flat
```

**Supported TypeScript Features:**
- ✅ Interfaces → KAIN structs
- ✅ Type aliases
- ✅ Enums (unit, tuple, struct variants)
- ✅ Classes → KAIN struct + impl
- ✅ Functions (with `async` → `Effect::Async`)
- ✅ Arrow functions → KAIN lambdas
- ✅ Generics
- ✅ Union types (`T | U`) → KAIN enum
- ✅ Intersection types (`T & U`) → KAIN struct
- ✅ `Promise<T>` → async function
- ✅ `Array<T>`, `ReadonlyArray<T>` → KAIN `Array<T>`

**Type Mappings:**
- `number` → `Float` or `Int` (context-dependent)
- `string` → `String`
- `boolean` → `Bool`
- `bigint` → `Int`
- `void` → `Unit`
- `never` → `Never`
- `null | undefined` → `Option<T>`

**Round-Trip Pipeline:**
```
app.ts → kain import-ts → app.kn → kain build -t ts → app-generated.ts
```

### Assembly Import (Production-Ready — 3 CPU Dialects)

Import assembly source files for retro/embedded processors via `kain-asm` crate:

```bash
# Game Boy LR35902
kain import-asm ./game.asm --format gameboy --output game.kn

# MOS 6502 (Furby, NES, C64, Apple II)
kain import-asm ./furby.asm --format 6502 --output furby.kn

# Zilog Z80 (ZX Spectrum, MSX, arcade boards)
kain import-asm ./arcade.asm --format z80 --output arcade.kn

# Validate only (no output)
kain import-asm ./game.asm --format gameboy --validate-only
```

**Supported Dialects:**

| Dialect | Aliases | CPU | Use Cases |
|---------|---------|-----|-----------|
| `lr35902-gameboy` | `gameboy`, `gb-lr35902`, `lr35902` | Game Boy CPU | Game Boy games, homebrew |
| `6502-furby` | `6502`, `furby`, `furby-6502` | MOS 6502 | Furby, NES, C64, Apple II |
| `z80` | `z80-arcade`, `z80-spectrum`, `z80-msx` | Zilog Z80 | ZX Spectrum, MSX, arcade |

**Game Boy Dialect Features (Most Complete):**
- Full LR35902 CPU emulator with cycle-accurate timing
- Memory Bank Controller (MBC1, MBC5) support
- PPU (pixel processing unit) emulation
- APU (audio processing unit) emulation
- DMA, Timer, Joypad, Serial peripherals
- Control flow graph analysis via `petgraph`
- Parallel section processing via `rayon`
- Parity tracing for round-trip validation

**Output:**
```rust
ImportAsmOutput {
    kain_source: String,           // Generated .kn source
    recovery_report: RecoveryReport, // Line-by-line recovery stats
    parity_schema: ParityTraceFrame, // Round-trip validation schema
}
```

### Coming Soon
- **C++ Import** — Stub exists, planned via tree-sitter-cpp
- **Python Import** — Planned
- **x86_64 Assembly** — Planned
- **ARM64 Assembly** — Planned
- **RISC-V Assembly** — Planned

---

## Low-Level Memory Layer — 90% Complete

KAIN has a **production-grade low-level memory layer** (2078 lines) with C ABI compatibility, enabling zero-cost C FFI, embedded systems programming, and direct hardware access.

### Core Features

**C ABI Policy System:**
- ✅ Per-target ABI policies (x86_64 System V, x86_64 Windows, ARM64 AAPCS64, WASM32/64)
- ✅ Type size/alignment rules matching C compilers
- ✅ Bitfield packing (LSB-first vs MSB-first)
- ✅ Integer promotion and usual arithmetic conversions

**Struct Layout Engine:**
- ✅ Field offset calculation with alignment padding
- ✅ Packed structs (`@c_packed`)
- ✅ Custom pack alignment (`@c_pack_align(N)`)
- ✅ Custom type alignment (`@c_type_align(N)`)
- ✅ Bitfield packing with unit size detection (8/16/32/64 bits)
- ✅ Union support with type-safe access (`@c_union`)

**Automatic Lowering Pipeline:**
- ✅ Address-taken analysis (detects `addr_of(x)` usage)
- ✅ Pointer binding injection (auto-generates shadow pointers)
- ✅ Bitfield access lowering (read/write with bit manipulation)
- ✅ Union access lowering (type-safe field access)
- ✅ Arithmetic promotion (C-compatible integer promotion)

---

### Memory Operations (11 Expressions)

KAIN provides 11 low-level memory operations that lower to runtime helper functions:

| Operation | Syntax | Purpose | Lowered To |
|-----------|--------|---------|------------|
| **addr_of** | `addr_of(x)` | Take address of variable/field | `__kain_addr_of(x)` or pointer binding |
| **ptr_offset** | `ptr_offset(ptr, i)` | Pointer arithmetic | `__kain_ptr_offset(ptr, i, stride)` |
| **mem_load** | `mem_load(ptr)` | Read from pointer | `__kain_mem_load(ptr)` |
| **mem_store** | `mem_store(ptr, val)` | Write to pointer | `__kain_mem_store(ptr, val)` |
| **sizeof_type** | `sizeof_type(Int)` | Get type size | Compile-time constant |
| **alignof_type** | `alignof_type(Float)` | Get type alignment | Compile-time constant |
| **alloca** | `alloca(MyStruct)` | Stack allocation | Zero-initialized struct literal |
| **uninit** | `uninit(MyStruct)` | Uninitialized storage | None-initialized struct literal |
| **alloc** | `alloc(1024, u8, zeroed: true)` | Heap allocation | `__kain_alloc(size, stride, zeroed, seed)` |
| **realloc** | `realloc(ptr, 2048, u8)` | Resize allocation | `__kain_realloc(ptr, size, stride, seed)` |
| **aggregate_init** | `aggregate_init(Point, {x: 1.0}, zero_fill: true)` | Partial struct init | Struct literal with zero-fill |

---

### Runtime Helper Functions

The lowering pipeline generates calls to these runtime functions (implemented per backend):

```rust
// Pointer operations
fn __kain_bind_local<T>(x: T) -> *mut T
fn __kain_addr_of<T>(x: T) -> *const T
fn __kain_ptr_offset<T>(ptr: *const T, offset: isize, stride: isize) -> *const T
fn __kain_field_ptr<T>(ptr: *const T, field: &str, offset: usize) -> *const u8
fn __kain_index_ptr<T>(ptr: *const T, index: isize, stride: isize) -> *const T

// Memory access
fn __kain_mem_load<T>(ptr: *const T) -> T
fn __kain_mem_store<T>(ptr: *mut T, value: T)

// Bitfield access
fn __kain_bitfield_get(object, field: &str, offset: usize, bit_offset: usize, 
                       bit_width: usize, signed: bool, promoted_bits: usize) -> i64
fn __kain_bitfield_set(object, field: &str, offset: usize, bit_offset: usize,
                       bit_width: usize, signed: bool, promoted_bits: usize, value: i64)

// Union access
fn __kain_union_get(object, field: &str, type_key: &str, stride: i64, 
                    union_size: i64, fallback) -> T
fn __kain_union_set(object, field: &str, type_key: &str, stride: i64,
                    union_size: i64, value)
fn __kain_union_wrap(object, active_field: &str, type_key: &str, stride: i64,
                     union_size: i64, value) -> Union

// Heap allocation
fn __kain_alloc(size: usize, stride: usize, zeroed: bool, seed) -> *mut u8
fn __kain_realloc(ptr: *mut u8, size: usize, stride: usize, seed) -> *mut u8
```

---

### Struct/Field Attributes

Control memory layout with attributes:

| Attribute | Level | Purpose | Example |
|-----------|-------|---------|---------|
| `@c_packed` | Struct | Pack fields with no padding | `@c_packed struct Data: ...` |
| `@c_pack_align(N)` | Struct | Pack with max alignment N bits | `@c_pack_align(16) struct Data: ...` |
| `@c_type_align(N)` | Struct | Override struct alignment to N bits | `@c_type_align(64) struct Data: ...` |
| `@c_union` | Struct | Union type (overlapping fields) | `@c_union struct Data: ...` |
| `@c_bitfield(width, signed)` | Field | Bitfield with width and signedness | `@c_bitfield(3, true) flags: Int` |
| `@c_storage_bits(N)` | Field | Override field size to N bits | `@c_storage_bits(128) big_value: Int` |
| `@c_storage_align(N)` | Field | Override field alignment to N bits | `@c_storage_align(32) aligned_field: Int` |

---

### Example 1: Packed Struct with Bitfields

```kain
@c_packed
struct PackedData:
    @c_bitfield(3, true)   // 3-bit signed bitfield
    flags: Int
    @c_bitfield(5, false)  // 5-bit unsigned bitfield
    count: Int
    @c_storage_bits(128)   // 128-bit (16 byte) field
    big_value: Int
```

**Generated Layout:**
```
Offset 0: Bitfield unit (1 byte)
  - flags: bits 0-2 (signed, 3 bits)
  - count: bits 3-7 (unsigned, 5 bits)
Offset 1: big_value (16 bytes, packed alignment = 1)
Total size: 17 bytes
Alignment: 1 byte (packed)
```

---

### Example 2: Union with Type-Safe Access

```kain
@c_union
struct Data:
    int_val: Int      // 8 bytes
    float_val: Float  // 8 bytes
    bytes: [u8; 8]    // 8 bytes

fn example():
    let d = Data { int_val: 42 }
    let f = d.float_val  // Type-safe union read
    
    // Lowered to:
    // let f = __kain_union_get(d, "float_val", "Float", 8, 8, 0.0)
```

**Generated Layout:**
```
All fields at offset 0 (overlapping)
Total size: 8 bytes (max field size)
Alignment: 8 bytes
```

---

### Example 3: Address-Taken Analysis

```kain
fn example():
    let x = 42
    let ptr = addr_of(x)  // x is address-taken
    let val = x           // x must be loaded from pointer
    mem_store(ptr, 100)
    let new_val = x       // x reloaded from pointer
```

**Lowered Code:**
```kain
fn example():
    let x = 42
    let __kain_ptr_x = __kain_bind_local(x)  // Auto-injected shadow pointer
    let ptr = __kain_ptr_x
    let val = __kain_mem_load(__kain_ptr_x)  // Auto-rewritten load
    __kain_mem_store(__kain_ptr_x, 100)
    let new_val = __kain_mem_load(__kain_ptr_x)
```

---

### Example 4: Pointer Chain Walking

```kain
struct Point { x: Float, y: Float }
struct Line { start: Point, end: Point }

fn example():
    let line = Line { ... }
    let ptr = addr_of(line.start.x)  // Complex address-of
    
    // Lowered to:
    // let __kain_ptr_line = __kain_bind_local(line)
    // let ptr = __kain_field_ptr(
    //     __kain_field_ptr(__kain_ptr_line, "start", offset_of_start),
    //     "x", offset_of_x
    // )
```

---

### Example 5: Heap Allocation

```kain
fn create_buffer():
    // Allocate 1024 bytes, zero-initialized
    let buffer = alloc(1024, u8, zeroed: true)
    
    // Resize to 2048 bytes
    let bigger = realloc(buffer, 2048, u8, zeroed_new: true)
    
    // Lowered to:
    // let buffer = __kain_alloc(1024, 1, true, 0)
    // let bigger = __kain_realloc(buffer, 2048, 1, 0)
```

---

### Example 6: Aggregate Initialization

```kain
struct Point { x: Float, y: Float, z: Float }

fn example():
    // Partial initialization with zero-fill
    let p = aggregate_init(Point, { x: 1.0, y: 2.0 }, zero_fill: true)
    
    // Lowered to:
    // let p = Point { x: 1.0, y: 2.0, z: 0.0 }  // z auto-filled
```

---

### Backend Support

| Backend | Raw Pointers | Memory Ops | Status |
|---------|--------------|------------|--------|
| **Rust** | ✅ Yes | ✅ Yes | Ready (runtime functions implemented) |
| **C++** | ✅ Yes | ✅ Yes | Ready (runtime functions implemented) |
| **LLVM** | ✅ Yes | ✅ Yes | Ready (direct IR generation) |
| **TypeScript** | ❌ No | ❌ No | Validation only (rejects with error) |
| **JavaScript** | ❌ No | ❌ No | Validation only (rejects with error) |
| **WASM** | ❌ No | ❌ No | Validation only (rejects with error) |
| **UE5** | ❌ No | ❌ No | Validation only (rejects with error) |

**Validation:** Backends without support reject programs using memory operations with diagnostic code `KAIN-MEM-0002`.

---

### What's Missing (10% to MVP)

1. **Parser Integration** (2-3 days)
   - Add `addr_of`, `mem_load`, `mem_store`, etc. as built-in functions
   - Parse type arguments: `sizeof_type(Int)`, `alloca(MyStruct)`

2. **Runtime Function Implementation** (3-4 days)
   - Implement `__kain_*` functions in Rust backend
   - Implement `__kain_*` functions in C++ backend
   - Implement `__kain_*` functions in LLVM backend

3. **C Import Integration** (5-7 days)
   - Parse C headers with libclang
   - Generate KAIN bindings with `@c_import` attribute
   - Handle function pointers, macros, typedefs

**Total to MVP:** 10-14 days

---

### Use Cases Enabled

Once complete, KAIN will support:
- ✅ C FFI without wrappers
- ✅ Zero-copy data processing
- ✅ Embedded systems programming
- ✅ Kernel development
- ✅ Hardware drivers
- ✅ Game engine internals
- ✅ Network protocol parsing
- ✅ File format parsing
- ✅ Memory-mapped I/O
- ✅ Custom allocators

**Documentation:** See `Kain/crates/kain-core/LOW_LEVEL_MEMORY_STATUS.md` for full implementation details.

---

## KainScript — NEW Zero-Compilation JavaScript Target

**KainScript** (`.ks`) is a revolutionary target that combines the best of JavaScript and TypeScript:

- **Pure ES2022 JavaScript** — Runs natively in Node.js, Deno, Bun, browsers
- **Full type information** — JSDoc annotations understood by TypeScript LSP
- **No compilation step** — `node file.ks` just works
- **Type checking** — `// @ts-check` enables VS Code full type checking without tsc
- **Zero build wall** — Edit and run immediately

### Example
```bash
# Compile to KainScript
kain build app.kn --target ks

# Run directly (no compilation needed!)
node app.ks

# Type check (optional)
tsc --checkJs --noEmit app.ks
```

### Generated Output
```javascript
// @ts-check
// Generated by KAIN compiler — KainScript target (.ks)
// Runs natively in Node.js, Deno, Bun, and browsers. No compilation needed.
// Full type checking: tsc --checkJs --noEmit file.ks

/** @typedef {{ x: number, y: number }} Point */
class Point {
    /** @param {number} x @param {number} y */
    constructor(x, y) {
        this.x = x;
        this.y = y;
    }
}

/** @param {Point} p1 @param {Point} p2 @returns {number} */
function distance(p1, p2) {
    const dx = p2.x - p1.x;
    const dy = p2.y - p1.y;
    return Math.sqrt(dx * dx + dy * dy);
}
```

---

## GPU Shader Compilation

KAIN has **full GPU shader support** with multiple backends:

### SPIR-V Backend (Cross-Platform)
```bash
# Compile to SPIR-V binary
kain build shader.kn --target spirv

# Validate
spirv-val shader.spv

# Transpile to WGSL/GLSL/Metal via naga
naga shader.spv shader.wgsl
```

**Supported stages:** Vertex, Fragment, Compute, Surface

**Cross-platform via naga:**
- SPIR-V → WGSL (WebGPU)
- SPIR-V → GLSL (OpenGL)
- SPIR-V → Metal Shading Language (macOS/iOS)

### Rust GPU Artifact Bundling
```bash
# Generate Rust + GPU artifacts
kain build shader.kn --target rust --artifacts spirv

# Outputs:
# - lib.rs (Rust source)
# - kain_gpu.rs (GPU dispatch helpers)
# - kain_gpu.reflect.json (reflection metadata)
# - shader.spv (SPIR-V binary)
```

### HLSL & USF Backends
```bash
# DirectX shaders
kain build shader.kn --target hlsl

# Unreal Engine 5 shaders
kain build shader.kn --target usf
```

---

## KAIN CLI Commands — Complete Reference

### Installation & Setup

```bash
# Build from source (requires Rust)
cargo install --path crates/cli --force

# Verify installation
kain --version

# Check system health
kain doctor
```

**Binary Location:** `M:\Code\Kain\target\release\kain.exe`

---

### Core Commands

#### `kain doctor` — System Diagnostics
Shows binary/build diagnostics and resolved compiler capabilities.

```bash
kain doctor
```

**Output:**
- Version and build number
- Build timestamp (UTC)
- Git SHA, commit count, dirty status
- Build profile (debug/release)
- Target triple (host and target)
- Binary path
- Current directory
- Project root (if KAIN.toml found)
- Supported targets
- Feature flags (ue5, web, gpu, sys)

---

#### `kain init` — Initialize New Project
Creates a new KAIN project with KAIN.toml, src/, and .gitignore.

```bash
kain init                              # Current directory
kain init MyProject                    # New directory
kain init MyProject --name "My Plugin" # Explicit name
```

**Generated Files:**
- `KAIN.toml` - Project configuration
- `src/` - Source directory
- `.gitignore` - Git ignore file

---

#### `kain build` — Build Project or File
Build project from KAIN.toml or compile a single file.

```bash
# Build UE5 plugin from KAIN.toml
kain build --ue5

# Build specific file to a target
kain build src/main.kn --target wasm

# Build with multiple targets
kain build --targets wasm,js,rust

# Build Rust project
kain build --rust

# Verbose output
kain build --ue5 --embed --verbose

# Dry run (preview without writing)
kain build --ue5 --dry-run

# Watch mode (auto-recompile on changes)
kain build src/main.kn --target wasm --watch
```

**Flags:**
- `input` (optional): Input file. If omitted, builds all targets from KAIN.toml
- `-o, --output`: Output file path
- `-t, --target`: Single target override for file builds
- `--targets`: Override targets (comma-separated: wasm,js,rust)
- `--ue5`: Build UE5 plugin from KAIN.toml [ue5] config
- `--rust`: Build Rust project
- `--embed`: Embed original KAIN source as comments in generated C++
- `-w, --watch`: Watch for file changes and recompile
- `--dry-run`: Print planned actions without executing
- `--strict`: Treat warnings as errors
- `--analyze`: Analyze shader complexity (USF target only)
- `-v, --verbose`: Verbose output

---

#### `kain run` — Execute Immediately
Run a file using the interpreter (immediate execution).

```bash
kain run examples/hello.kn
kain run examples/test.kn --verbose
```

---

#### `kain lsp` — Language Server Protocol
Starts the KAIN Language Server for IDE integration.

```bash
kain lsp  # Typically invoked by IDE, not manually
```

**Provides:** Autocomplete, diagnostics, hover information, go-to-definition

---

### Import Commands

#### `kain import-c` — Import C Source

```bash
# Single file
kain import-c src/main.c --output main.kn

# Directory (recursive)
kain import-c src/ --output combined.kn

# Compile directly without writing .kn
kain import-c src/main.c --target wasm

# With preprocessor flags
kain import-c src/main.c -I include/ -D DEBUG -D VERSION=1.0

# Flat mode (no per-file modules)
kain import-c src/ --flat --output flat.kn

# Filtering
kain import-c src/ --include "core,util" --exclude "test,deprecated"

# Fail fast on errors
kain import-c src/ --fail-fast

# Generate report JSON
kain import-c src/ --report-json import_report.json
```

**Flags:**
- `input` (required): Input C source file or directory
- `-o, --output`: Output .kn file
- `-t, --target`: Compilation target (compile directly without writing .kn)
- `-I, --include-paths`: Include paths for C preprocessor
- `-D, --defines`: Preprocessor defines
- `--flat`: Flatten all imported symbols into one global scope
- `--include`: Include only files matching filters (comma-separated)
- `--exclude`: Exclude files matching filters (comma-separated)
- `--fail-fast`: Stop on first failed file import
- `--report-json`: Write import failure/report JSON

---

#### `kain import-rust` — Import Rust Source

```bash
# Single file
kain import-rust src/main.rs --output main.kn

# Directory (flat mode for self-hosting)
kain import-rust src/ --flat --output combined.kn

# Compile directly
kain import-rust src/main.rs --target wasm

# With filtering
kain import-rust src/ --include "core" --exclude "test"
```

**Flags:** Same as `import-c` (except no preprocessor flags)

---

#### `kain import-ts` — Import TypeScript Source

```bash
# Single file
kain import-ts src/main.ts --output main.kn

# Directory (recursive)
kain import-ts src/ --output combined.kn

# Compile directly
kain import-ts src/main.ts --target wasm

# With filtering
kain import-ts src/ --include "components" --exclude "test"
```

**Flags:** Same as `import-rust`

---

#### `kain import-asm` — Import Assembly

```bash
# Game Boy assembly
kain import-asm firmware.asm --format gameboy --out game.kn

# 6502 assembly
kain import-asm firmware.asm --format 6502-furby --out furby.kn

# Z80 assembly
kain import-asm arcade.asm --format z80 --out arcade.kn

# Validate only (no output)
kain import-asm firmware.asm --validate-only
```

**Flags:**
- `input` (required): Input assembly source file
- `--format`: Input dialect format (default: "6502-furby")
  - Supported: `gameboy`, `lr35902`, `6502`, `furby`, `z80`, `z80-arcade`, `z80-spectrum`, `z80-msx`
- `--out`: Output .kn file
- `--validate-only`: Parse/canonicalize and report only, without writing

**Generated Files:**
- `.kn` - KAIN firmware scaffolding
- `_canonical.asm` - Canonicalized assembly
- `_map.json` - Mapping metadata
- `_report.json` - Recovery report

---

### UE5 Commands

#### `kain inject` — Inject into Existing Plugin
Inject KAIN file into existing UE5 plugin (non-destructive).

```bash
# Auto-detect plugin
kain inject src/new_actor.kn --ue5

# Multiple files
kain inject src/a.kn src/b.kn --ue5

# Explicit plugin
kain inject src/new_actor.kn --ue5 --plugin MyPlugin

# Explicit directory
kain inject src/new_actor.kn --ue5 --plugin-dir /path/to

# Preview changes
kain inject src/new_actor.kn --ue5 --dry-run

# Force overwrite
kain inject src/new_actor.kn --ue5 --force
```

**Flags:**
- `inputs` (required): Input .kn file(s)
- `--plugin-dir`: Target plugin directory (auto-detected if omitted)
- `--plugin`: Plugin name (auto-detected if omitted)
- `--force`: Force overwrite existing files
- `--dry-run`: Dry run (show what would be generated)
- `--ue5`: Use UE5 codegen (required)

---

### GPU Commands

#### `kain gpu-artifacts` — Generate GPU Artifacts
Generate paired GPU artifacts (SPIR-V, Rust host wrappers, reflection JSON).

```bash
kain gpu-artifacts src/shader.kn
kain gpu-artifacts src/shader.kn --output dist/
```

**Generated Artifacts:**
- `.spv` - SPIR-V binary
- `.rs` - Rust host wrapper
- `.json` - Reflection metadata

---

### Compilation Targets

All targets available via `-t, --target` flag:

| Target | Aliases | Extension | Description |
|--------|---------|-----------|-------------|
| **wasm** | wasm, w | .wasm | WebAssembly binary |
| **llvm** | llvm, native, n | .ll | LLVM IR (links to executable) |
| **spirv** | spirv, gpu, shader, s | .spv | SPIR-V binary (cross-platform GPU) |
| **hlsl** | hlsl, h | .hlsl | DirectX HLSL shader |
| **usf** | usf | .usf | Unreal Engine 5 shader |
| **js** | js, javascript, j | .js | JavaScript |
| **ts** | ts, typescript | .ts | TypeScript |
| **ks** | ks, kainscript, kscript | .ks | KainScript (TS/JS hybrid) |
| **rust** | rust, rs | .rs | Rust source |
| **cpp** | cpp, c++ | .cpp | C++ source |
| **hybrid** | hybrid, web | .js | WASM + JS (full-stack web) |
| **ue5** | ue5, unreal, u | .h | UE5 C++ plugin |
| **ue5editor** | ue5editor, ue5-editor, editor, slate | .h | UE5 Editor C++ (Slate UI) |
| **run** | run, r, interpret, i | .txt | Interpret (immediate execution) |
| **test** | test, t | .txt | Test runner |

---

### Environment Variables

| Variable | Description |
|----------|-------------|
| `KAIN_METADATA_DIR` | Explicit metadata directory path |
| `KAIN_ROOT` | KAIN repository root (for metadata discovery) |
| `KAIN_STDLIB_PATH` | Explicit stdlib directory path |
| `KAIN_RUNTIME_C_PATH` | Path to kain_runtime.c for LLVM linking |
| `KAIN_BUILD_NUMBER` | Build number (set during compilation) |
| `KAIN_BUILD_UNIX_TIME` | Build timestamp (set during compilation) |
| `KAIN_BUILD_PROFILE` | Build profile (debug/release) |
| `KAIN_BUILD_TARGET_TRIPLE` | Target triple |
| `KAIN_BUILD_HOST_TRIPLE` | Host triple |
| `KAIN_GIT_SHA` | Git commit SHA |
| `KAIN_GIT_COMMIT_COUNT` | Git commit count |
| `KAIN_GIT_DIRTY` | Git dirty status |

---

### Feature Flags (Compile-Time)

KAIN compiler is built with Cargo feature flags:

| Feature | Description |
|---------|-------------|
| `ue5` | UE5 C++ codegen (actors, components, shaders, materials, blueprints) |
| `web` | Web targets (WASM, JS, TS, KS, Hybrid) |
| `gpu` | GPU targets (SPIR-V, HLSL) |
| `sys` | System targets (LLVM, Rust, C++) |

**Check enabled features:**
```bash
kain doctor  # Shows features: ue5=on, web=on, gpu=on, sys=on
```

---

### Legacy Mode (No Subcommand)

When no subcommand is provided, KAIN uses legacy positional argument mode:

```bash
# Compile to WASM (default)
kain src/main.kn

# Compile to specific target
kain src/main.kn --target rust

# Watch mode
kain src/main.kn --watch

# Run immediately
kain src/main.kn --run

# Verbose output
kain src/main.kn --verbose
```

---

### Common Workflows

**UE5 Plugin Development:**
```bash
kain init MyPlugin --name "My Awesome Plugin"
kain build --ue5
kain inject src/new_actor.kn --ue5 --plugin MyPlugin
```

**Shader Development:**
```bash
kain build src/shader.kn --target spirv
kain build src/shader.kn --target hlsl
kain build src/shader.kn --target usf --analyze
kain gpu-artifacts src/shader.kn --output dist/
```

**Import Workflows:**
```bash
kain import-c legacy/src/ --output imported.kn
kain import-rust compiler/src/ --output compiler.kn --flat
kain import-ts frontend/src/ --output frontend.kn
kain import-asm firmware.asm --format 6502-furby
```

**Development:**
```bash
kain build src/main.kn --target wasm --watch
kain run examples/hello.kn
kain build --ue5 --verbose --dry-run
```

---

## Repository Structure

```
.
├── Kain/                          # Rust compiler monorepo
│   ├── crates/
│   │   ├── kain-core/             # Parser, AST, type checker, low-level memory layer
│   │   ├── kain-import/           # C, Rust, TypeScript importers
│   │   ├── kain-asm/              # Assembly importers (Game Boy, 6502, Z80)
│   │   ├── kain-sys-codegen/      # LLVM, Rust, C++ backends
│   │   ├── web/                   # WASM, JS, TS, KS backends
│   │   ├── gpu/                   # SPIR-V, HLSL backends
│   │   ├── ue5/                   # Runtime codegen (actors, components, RPCs)
│   │   ├── ue5-editor/            # Editor codegen (Slate, Details, Viewports)
│   │   ├── ue5-graphs/            # Graph editor + runtime codegen
│   │   ├── ue5-shaders/           # Shader codegen (compute, fragment, vertex, surface)
│   │   ├── ue5-materials/         # Material graph codegen (binary .uasset)
│   │   ├── ue5-blueprints/        # Blueprint node codegen (UK2Node, Kismet bytecode)
│   │   └── cli/                   # CLI binary + packager
│   ├── stdlib/ue5/                # 200+ stdlib functions (12 categories)
│   ├── unreal/metadata/           # 14 JSON metadata files (engine types, widgets, shaders)
│   └── docs/recent/               # V1 compiler documentation
│
├── Factory/                       # 20 production UE5 plugins with compiled binaries
├── Research/                      # 29 UE5 pattern taxonomies, battle reports
└── README.md                      # This file
```

---

## UE5 Backend — Complete Feature Reference

The UE5 backend is the most advanced target, spanning **7 specialized codegen crates**:

### Actors (AActor)
```kain
actor Player:
    state health: Float = 100.0
    state max_health: Float = 100.0
    on Server_TakeDamage(amount: Float):
        health = health - amount
        if health <= 0.0:
            Server_Die()
```

**Generates:** `APlayer : public AActor` with `UCLASS`, `UPROPERTY`, `UFUNCTION`, replication, RPCs

### Components (UActorComponent)
```kain
@component
struct HealthComponent:
    @replicated
    current: Float
    @replicated
    max: Float
```

### Subsystems (UWorldSubsystem)
```kain
@subsystem
struct NarrativeManager:
    active_dialogues: Array<DialogueInstance>
    fn start_dialogue(npc_id: Int) -> Bool:
        return true
```

### Shaders (.usf)
```kain
shader compute VoxelGenerator(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform noise_scale: Float @1
    buffer output: RWBuffer<Float> @2
    let noise = perlin_noise(thread_id * noise_scale)
    output[thread_id.x] = noise
```

### Material Graphs (Binary .uasset)
```kain
material PBRGround:
    input albedo: Texture2D
    input roughness_value: Float = 0.5
    base_color = texture_sample(albedo).rgb
    roughness = roughness_value
```

**30+ material node types**, custom HLSL, texture sampling, UV manipulation, time-based effects, **direct binary .uasset serialization**

### Graph Editors (UEdGraph)
```kain
@graph_runtime
graph DialogueSystem:
    @node_data
    node SpeakerNode:
        speaker_name: String = "NPC"
        @input_pin
        in_exec: Exec
        @output_pin
        next: Exec
```

### Editor UI
- **Slate Widgets** (`@slate`) → `SCompoundWidget`
- **Details Panels** (`@details`) → `IDetailCustomization`
- **Viewports** (`@viewport`) → `SEditorViewport`
- **Toolbars** (`@toolbar`) → `FToolBarBuilder`
- **Asset Editors** (`@asset_editor`) → `FAssetEditorToolkit`

---

## Test Coverage — 386 Tests Passing

| Crate | Tests | Coverage |
|-------|-------|----------|
| `ue5` | 148 | Actors, components, RPCs, networking, animation, subsystems |
| `ue5-shaders` | 85 | Compute, fragment, vertex, surface, permutations |
| `ue5-graphs` | 58 | Graph runtime, NodeData, GraphInstance, Asset |
| `ue5-editor` | 38 | Slate, Details, Viewports, Toolbars, Asset Editors |
| `ue5-materials` | 36 | Material graphs, binary .uasset, expressions |
| `ue5-blueprints` | 21 | UK2Node, Kismet bytecode, async nodes |
| `cli` | 13+ | Packager, multi-file builds, module validation |
| `kain-import` | 15+ | C/Rust/TypeScript import, ABI conformance |
| `kain-asm` | 8+ | Assembly import (Game Boy, 6502, Z80) |
| `gpu` | 85+ | SPIR-V validation, Vulkan execution |

---

## Proven Results — 20 Production UE5 Plugins

| Plugin | KAIN Lines | C++ Lines | Features |
|--------|-----------|-----------|----------|
| **VoxelForgePro** | 1,943 | 15,000 | 19 GPU compute shaders, terrain generation |
| **TitanGraph** | 1,692 | 10,000 | Quest/dialogue graph editor with UEdGraph |
| **AeroTunnel** | 1,620 | 12,000 | Flight physics + wind tunnel simulation |
| **KainFlow** | 966 | 8,000 | Soft-body physics engine |
| **NarrativeGraph** | 464 | 2,321 | Dialogue/quest runtime with graph editors |
| **Cinema4DMograph** | 1,000+ | 5,000+ | Mograph system with 20+ modifiers |
| +14 more... | | | |

**Average Compression:** 1 line KAIN → 5-8 lines C++ (base) → **1:20+ with stdlib**

---

## Documentation

### Crate-Level References
- `Kain/crates/cli/CRATE_REFERENCE.md` — Full CLI command reference
- `Kain/crates/ue5/CRATE_REFERENCE.md` — Runtime codegen
- `Kain/crates/ue5-editor/CRATE_REFERENCE.md` — Editor codegen
- `Kain/crates/ue5-shaders/CRATE_REFERENCE.md` — Shader codegen
- `Kain/crates/kain-import/CRATE_REFERENCE.md` — C/Rust import
- `Kain/crates/kain-core/LOW_LEVEL_MEMORY_STATUS.md` — Low-level memory layer (90% complete)
- `Kain/docs/CRATE_INDEX.md` — Master index

### Language & Patterns
- `Kain/docs/recent/README.MD` — V1 compiler documentation
- `.kiro/steering/kain-patterns.md` — Language patterns and best practices
- `Kain/docs/recent/PARSER_AST_GUIDE.md` — Parser and AST deep-dive

### Stdlib
- `Kain/stdlib/ue5/` — 12 stdlib files (200+ functions)
- `Kain/stdlib/USAGE_GUIDE.md` — How to use stdlib in plugins
- `Factory/_Docs/COMPRESSION_RATIO_ANALYSIS.md` — 1:20 compression methodology

---

## Roadmap

**Recently Completed (Feb-Mar 2026):**
- ✅ C importer (production-grade, 131KB transformer)
- ✅ Rust importer (active development, Project Ouroboros)
- ✅ TypeScript importer (production-ready, SWC-based)
- ✅ Assembly importer (3 CPU dialects: Game Boy, 6502, Z80)
- ✅ Low-level memory layer (90% complete, 2078 lines)
- ✅ KainScript target (zero-compilation JavaScript)
- ✅ SPIR-V backend with cross-platform support
- ✅ Rust GPU artifact bundling
- ✅ Data-driven stdlib system (200+ functions, 1:20 compression)
- ✅ Vector operation codegen (component-wise floor/frac/abs)
- ✅ UObject pointer detection (`.` vs `->`)
- ✅ Array method translation (`.len()→.Num()`)
- ✅ Delegate codegen (DECLARE_DYNAMIC_MULTICAST_DELEGATE)
- ✅ USF array literal support
- ✅ USF cast expression support
- ✅ TypeMapper (unified KAIN→HLSL)
