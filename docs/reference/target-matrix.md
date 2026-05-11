# Target Matrix

Snapshot: May 11, 2026.

This page is the canonical view of compile targets, aliases, and output
extensions. The live target registry comes from `crates/kain-driver/src/lib.rs`.
The `CompileTarget` enum itself lives in `crates/kain-core/src/lib.rs`.

## Notes

- The CLI uses the driver registry for parsing and output naming.
- `kain-core::CompileTarget::from_str` is a smaller API and does not expose
  every CLI alias.
- `kn` remaps a bare `wasm` request to `run` when no output path is given.
- `target_extension()` falls back to `out` for unknown internal targets, but the
  public CLI rejects unknown aliases before it reaches that point.
- `run` is a CLI workflow command, not a compile target alias. `Interpret` and
  `Test` are the runtime target aliases.
- `build --ue5`, `build -t ue5`, and `build native-ui` are workflow commands
  that materialize different artifact families.

## Targets

| Target | Driver aliases | Output extension | What it emits |
| --- | --- | --- | --- |
| `Wasm` | `wasm`, `w` | `.wasm` | browser-oriented module output |
| `Llvm` | `llvm`, `native`, `n` | `.ll` | LLVM IR plus native runtime staging |
| `C` | `c` | `.c` | experimental C source plus raw-native runtime contract staging |
| `Spirv` | `spirv`, `gpu`, `shader`, `s` | `.spv` | binary SPIR-V |
| `Hlsl` | `hlsl`, `h` | `.hlsl` | HLSL shader text |
| `Usf` | `usf` | `.usf` | UE5 shader source |
| `Js` | `js`, `javascript`, `j` | `.js` | JavaScript source |
| `Ts` | `ts`, `typescript` | `.ts` | TypeScript source |
| `Rust` | `rust`, `rs` | `.rs` | Rust source |
| `Hybrid` | `hybrid`, `web` | `.js` | hybrid web output |
| `Cpp` | `cpp`, `c++` | `.cpp` | C++ source |
| `Ue5` | `ue5`, `unreal`, `u` | `.h` | UE5 runtime header and plugin lane output |
| `Ue5Editor` | `ue5editor`, `ue5-editor`, `editor`, `slate` | `.h` | UE5 editor-facing plugin output |
| `Interpret` | `run`, `r`, `interpret`, `i` | `.txt` | runtime-hosted execution lane |
| `Test` | `test`, `t` | `.txt` | runtime test lane |
| `Ks` | `ks`, `kainscript`, `kscript` | `.ks` | KainScript output |

## Grouping

### Web and scripting

- `Wasm`
- `Js`
- `Ts`
- `Hybrid`
- `Ks`

### Native and system

- `Llvm`
- `C`
- `Rust`
- `Cpp`

### GPU and shader

- `Spirv`
- `Hlsl`
- `Usf`

### UE5

- `Ue5`
- `Ue5Editor`

### Runtime lanes

- `Interpret`
- `Test`

## Alias Differences To Know

The most important alias differences are:

- The CLI accepts `native` and `n` for LLVM output.
- The CLI accepts `c` for direct experimental C output.
- The CLI accepts `gpu`, `shader`, and `s` for SPIR-V output.
- The CLI accepts `web` for the hybrid JS lane.
- The CLI accepts `slate` and `ue5editor` for the editor target, while the
  core API only documents the narrower `ue5-editor` / `editor` pair.
