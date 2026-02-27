# kain-import

Universal import system for KAIN - import multiple source languages into KAIN IR.

## Supported Languages

| Language | Status | Parser | Features |
|----------|--------|--------|----------|
| **C** | 🚧 In Progress | `lang-c` | Functions, structs, enums, pointers, arrays |
| **Rust** | 📋 Planned | `syn` | Full Rust syntax |
| **C++** | 📋 Planned | `tree-sitter-cpp` | Classes, templates |
| **Python** | 📋 Planned | `rustpython-parser` | Functions, classes |

## Usage

### As Library

```rust
use kain_import;
use std::path::Path;

// Import a C file
let program = kain_import::import_c(Path::new("physics.c"))?;

// Import multiple C files (project)
let program = kain_import::import_c_project(&[
    Path::new("main.c"),
    Path::new("utils.c"),
])?;
```

### CLI

```bash
# Import C file to KAIN source
kain import-c physics.c --output physics.kn

# Import and compile directly to target
kain import-c mario.c --target ue5

# Import multiple files
kain import-c src/*.c --output game_logic.kn

# With preprocessor options
kain import-c -D DEBUG -I include/ main.c --output main.kn
```

## Architecture

```
kain-import/
├── src/
│   ├── lib.rs              # Public API
│   ├── c/                  # C importer
│   │   ├── mod.rs
│   │   ├── parser.rs       # lang-c integration
│   │   ├── transformer.rs  # C → KAIN AST
│   │   └── types.rs        # Type mappings
│   ├── rust/               # Rust importer (future)
│   ├── cpp/                # C++ importer (future)
│   └── common/             # Shared utilities
│       ├── preprocessor.rs
│       └── type_mapper.rs
```

## Examples

### Import SM64 Physics

```bash
# Import Mario physics from Super Mario 64
kain import-c sm64/src/game/mario.c --output mario_physics.kn

# Compile to UE5
kain build mario_physics.kn --target ue5

# Result: Mario physics as UE5 plugin!
```

### Import Doom Engine

```bash
# Import Doom game logic
kain import-c doom/src/*.c --output doom_logic.kn

# Compile to WASM for web
kain build doom_logic.kn --target wasm

# Result: Doom logic running in browser!
```

## Type Mappings

### C → KAIN

| C Type | KAIN Type | Notes |
|--------|-----------|-------|
| `int`, `long` | `Int` | 64-bit signed |
| `float`, `double` | `Float` | 64-bit float |
| `char` | `Char` | Single character |
| `char*` | `String` | Null-terminated string |
| `void` | `Unit` | Empty type |
| `bool`, `_Bool` | `Bool` | Boolean |
| `int*` | `&mut Int` | Mutable reference |
| `const int*` | `&Int` | Immutable reference |
| `int arr[10]` | `[Int; 10]` | Fixed-size array |
| `struct Foo` | `struct Foo` | Direct mapping |
| `enum Bar` | `enum Bar` | Direct mapping |

## Development Status

### Implemented ✅

- [x] Project structure
- [x] C parser integration (lang-c)
- [x] Type mapper
- [x] Preprocessor utilities
- [x] Basic transformer skeleton

### In Progress 🚧

- [ ] Function transformation
- [ ] Struct transformation
- [ ] Enum transformation
- [ ] Expression transformation
- [ ] Statement transformation
- [ ] Pointer/array handling

### Planned 📋

- [ ] Rust importer
- [ ] C++ importer
- [ ] Python importer
- [ ] CLI integration
- [ ] Test suite
- [ ] Documentation

## Contributing

See the main KAIN repository for contribution guidelines.

## License

MIT
