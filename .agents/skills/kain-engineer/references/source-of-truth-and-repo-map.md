# Source of Truth and Repo Map

## Read This First

Kain has rich docs, but they are not all equally current. Use this trust order:

1. Live CLI help from the built binary
2. Current command and target definitions in `M:\Code\Kain\crates\cli\src\main.rs` and `M:\Code\Kain\crates\cli\src\lib.rs`
3. `CompileTarget`, `compile()`, parser, runtime, and stdlib loader in `M:\Code\Kain\crates\kain-core\src\*.rs`
4. Importer and backend implementation files
5. Focused crate docs such as `CRATE_REFERENCE.md`, `AGENT_NOTES.md`, and pipeline docs
6. `M:\Code\README.md` and `M:\Code\Kain\README.md` for overview, examples, and history

The user has already noted that `M:\Code\README.md` is slightly out of date. Keep using it, but verify important claims against source.

## High-Value Repo Paths

- `M:\Code\README.md`
  - Top-level onboarding brief for agents and LLMs
- `M:\Code\Kain\README.md`
  - Example-heavy product overview
- `M:\Code\Kain\Cargo.toml`
  - Workspace membership and crate layout
- `M:\Code\Kain\crates\cli\src\main.rs`
  - Canonical command surface and flags
- `M:\Code\Kain\crates\cli\src\lib.rs`
  - Compile dispatch, target specs, and target extension mapping
- `M:\Code\Kain\crates\kain-core\src\lib.rs`
  - `CompileTarget`, `from_str()`, and `compile()`
- `M:\Code\Kain\crates\kain-core\src\parser.rs`
  - Grammar, indentation-sensitive parsing, `use` parsing, component syntax
- `M:\Code\Kain\crates\kain-core\src\runtime.rs`
  - Interpreter, module resolution, stdlib/module loading rules
- `M:\Code\Kain\crates\kain-core\src\stdlib.rs`
  - Stdlib search roots, target profile selection, env overrides
- `M:\Code\Kain\crates\kain-import\src\c\*.rs`
  - C importer implementation
- `M:\Code\Kain\crates\kain-import\src\rust\*.rs`
  - Rust importer implementation
- `M:\Code\Kain\crates\kain-import\src\typescript\*.rs`
  - TypeScript and TSX importer implementation
- `M:\Code\Kain\crates\web\src\codegen_*.rs`
  - `js`, `ts`, `ks`, `wasm`, and `hybrid` backends
- `M:\Code\Kain\crates\cli\src\selfhost.rs`
  - Advanced Ouroboros/self-host orchestration
- `M:\Code\Kain\crates\kain-core\KAIN_FEATURES_PART1.md`
  - Large language feature and target overview
- `M:\Code\Kain\crates\kain-core\KAIN_FEATURES_PART2.md`
  - Parser/runtime/test internals

## Crate Ownership Map

- `crates/kain-core`
  - Owns lexing, parsing, type checking, effects, monomorphization, interpreter, stdlib loading, and low-level memory diagnostics
- `crates/cli`
  - Owns the user-facing `kain` binary, project build flows, import command plumbing, UE5 injection, shader artifact generation, self-hosting, and omni commands
- `crates/kain-import`
  - Owns foreign-language source import into Kain AST
- `crates/web`
  - Owns `js`, `ts`, `ks`, `wasm`, and `hybrid` code generation
- `crates/gpu`
  - Owns GPU and shader-oriented codegen surfaces
- `crates/ue5*`
  - Own UE5 runtime, editor, material, graph, shader, and supporting codegen layers
- `crates/kain-asm`
  - Owns assembly import flows that feed into the same AST model
- `crates/kain-omni`
  - Owns mixed-language manifest orchestration
- `bootstrap`
  - Older self-host/bootstrap experiments and many real-world slash-style import examples
- `generated`
  - Generated outputs and smoke artifacts. Useful for inspection, not for defining semantics

## Important Mental Model

When investigating a behavior, ask which layer owns it:

- Frontend or language layer
  - Syntax, parsing, typing, effects, stdlib injection
- Importer layer
  - How C, Rust, or TypeScript source is lowered into Kain AST
- Backend layer
  - How typed Kain becomes `ks`, `ts`, `rust`, `ue5`, `spirv`, and so on

Many confusing bugs come from assuming support in one layer guarantees support in the others.

## Fast Lookup Guidance

- Syntax question
  - Open `parser.rs` and `KAIN_FEATURES_PART2.md`
- Import resolution or stdlib issue
  - Open `parser.rs`, `runtime.rs`, and `stdlib.rs`
- Backend output mismatch
  - Open the matching codegen file for that target
- Importer mismatch
  - Open the CLI import file and the corresponding transformer/type mapper in `kain-import`
- Rust import generated `.kn` contains `LOSSY LOWERING [class:unsupported_expr_lowering]`
  - Check `crates/cli/src/import_rust.rs` first. The Rust transformer in `crates/kain-import/src/rust/transformer.rs` may already have a concrete Kain AST for calls, method chains, `await`, `?`, structs, and matches; the CLI source printer is a separate choke point and can lose AST variants if its `expr_to_string` / statement emission tables lag the transformer.
  - Validate with a real command-shaped source such as GreebleFS `fs_commands.rs` plus a focused CLI test before assuming the Rust AST lowerer needs a new crate or parser.
- Self-host issue
  - Open `selfhost.rs` and inspect the inventory files it expects

## Validation Loop

Use the smallest loop that proves the behavior you changed:

- `cargo check -p kain-core --all-targets`
- `cargo test -p kain-core --tests`
- `cargo test -p cli --tests`
- `kain doctor`
- `kain build <file> --target <target>`
- `kain import-c ... --report-json ...`
- `kain import-ts ... --report-json ...`

If a claim matters and could have drifted, prefer running the command or opening the source over quoting an older doc block.
