# CLI, Build, and Target Reference

## Canonical CLI Shape

The modern Kain CLI is subcommand-based. Prefer this form over legacy positional invocation:

```powershell
kain build
kain run src\main.kn
kain import-c src\main.c --output main.kn
kain import-rust crates\foo\src --output foo.kn --flat
kain import-ts src\app.tsx --output app.kn
```

From the workspace root `M:\Code\Kain`, current live help can be checked with:

```powershell
.\target\debug\kain.exe --help
.\target\debug\kain.exe build --help
.\target\debug\kain.exe import-c --help
.\target\debug\kain.exe import-rust --help
.\target\debug\kain.exe import-ts --help
```

## Core Commands

### `kain doctor`

Use first when you need to know what this binary actually supports.

It reports:

- binary and build diagnostics
- feature flags such as `ue5`, `web`, `gpu`, `sys`
- supported targets
- current repo/project detection

### `kain init [path]`

Creates a new Kain project layout. Typical usage:

```powershell
kain init
kain init MyProject
kain init MyProject --name "My Plugin"
```

### `kain build [input]`

Main build entry point.

- No input: build from `KAIN.toml`
- With input: compile a single `.kn` file

Typical usage:

```powershell
kain build
kain build src\main.kn --target ks
kain build src\main.kn --targets wasm,js,rust
kain build --ue5
kain build src\shader.kn --target spirv
```

Important flags:

- `-o, --output`
- `-t, --target`
- `--targets`
- `--ue5`
- `--rust`
- `--embed`

Widely used CLI flags across build flows:

- `--watch`
- `--run`
- `--emit-ast`
- `--emit-typed`
- `-v, --verbose`
- `--dry-run`
- `--strict`
- `--analyze`

### `kain run <input>`

Runs a `.kn` file through the interpreter path. This is the explicit execution command and maps to the `interpret` target internally.

```powershell
kain run examples\hello.kn
```

### `kain lsp`

Starts the language server. Usually launched by the editor rather than by hand.

### `kain gpu-artifacts <input>`

Generates paired shader artifacts such as SPIR-V, Rust host helpers, and reflection metadata.

```powershell
kain gpu-artifacts src\shader.kn --output dist
```

### `kain inject <inputs> --ue5`

Non-destructive injection of generated UE5 code into an existing plugin. The current CLI only supports UE5 mode for inject.

```powershell
kain inject src\new_actor.kn --ue5
kain inject src\a.kn src\b.kn --ue5 --plugin MyPlugin
```

### `kain import-c`

Imports C into Kain and optionally compiles the result.

### `kain import-rust`

Imports Rust into Kain. This is a core part of the self-host pipeline.

### `kain import-ts`

Imports TypeScript or TSX into Kain and optionally compiles the result.

### `kain import-asm`

Imports supported assembly dialects into Kain.

### `kain selfhost phase1|phase2`

Advanced inventory-driven self-host flows. See the dedicated self-host reference.

### `kain omni init|build`

Advanced mixed-language orchestration path. If you need it, verify live help before assuming details.

## Compile Targets

Current `CompileTarget` values and aliases come from `M:\Code\Kain\crates\kain-core\src\lib.rs`.

| Target | Aliases | Output | Best fit |
|---|---|---|---|
| `wasm` | `wasm` | `.wasm` | Browser or sandboxed compute |
| `js` | `js`, `javascript` | `.js` | Plain JavaScript runtime output |
| `ts` | `ts`, `typescript` | `.ts` | Typed web or TS-first downstream tooling |
| `hybrid` | `hybrid` | mixed WASM + JS | Compute-heavy web apps with glue |
| `llvm` | `llvm` | native or LLVM IR flow | Native and systems work |
| `rust` | `rust`, `rs` | `.rs` | Rust interop, round-trip, self-hosting |
| `cpp` | `cpp`, `c++` | `.cpp` | Native interop and C++ ecosystems |
| `ue5` | `ue5`, `unreal` | UE5 plugin/runtime code | Main Unreal Engine codegen |
| `ue5-editor` | `ue5-editor`, `editor` | editor-focused UE5 code | Editor-facing Unreal codegen |
| `usf` | `usf`, `shader` | `.usf` | Unreal shader output |
| `spirv` | `spirv`, `spv` | `.spv` | Vulkan or general SPIR-V workflows |
| `hlsl` | `hlsl` | `.hlsl` | DirectX or HLSL shader flows |
| `interpret` | `interpret`, `run` | interpreter execution | Quick smoke execution |
| `test` | `test` | internal/testing flow | Parser/runtime/testing flows |
| `ks` | `ks`, `kainscript`, `kscript` | `.ks` | Zero-build JavaScript with JSDoc |

## Target Selection Heuristics

- Use `ks` when you want the fastest script-like iteration and direct execution in Node, Deno, Bun, or browsers.
- Use `ts` when the output must participate in a real downstream TypeScript toolchain.
- Use `js` when you want the simplest runtime artifact and do not need emitted type syntax or JSDoc.
- Use `hybrid` when compute belongs in WASM but orchestration belongs in JS.
- Use `rust`, `cpp`, or `llvm` for native or low-level work.
- Use `ue5`, `ue5-editor`, and `usf` for Unreal-oriented flows.
- Use `spirv` or `hlsl` for shader-specific workflows.
- Use `interpret` or `kain run` for quick behavior checks without choosing a heavy backend.

## Common Build Recipes

### Project build from config

```powershell
kain build
kain build --ue5
kain build --targets wasm,js,rust
```

### Single-file build

```powershell
kain build src\main.kn --target ks
kain build src\main.kn --target rust --output dist\main.rs
```

### Fast feedback loop

```powershell
kain doctor
kain run src\main.kn
kain build src\main.kn --target ks
node src\main.ks
```

### Shader loop

```powershell
kain build src\shader.kn --target spirv
kain build src\shader.kn --target hlsl
kain build src\shader.kn --target usf
kain gpu-artifacts src\shader.kn --output dist
```

## Practical Advice

- If you are unsure what a flag does, check the live subcommand help before relying on memory.
- If a target-specific failure smells like a frontend issue, retry `kain run` or another cheap target to isolate whether the bug is in parsing, typing, or backend emission.
- If a target appears unsupported, run `kain doctor` and check the compiled feature flags on the binary you are actually using.
