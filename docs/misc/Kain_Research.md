# Kain Language and Toolchain in ephemara/godkain  
*(A repository-grounded academic-style report based on primary source code and in-repo documentation)*

## Executive summary

This report documents the **Kain** programming language and its toolchain **as implemented in the `ephemara/godkain` repository**, emphasizing *direct evidence* from the repository’s source code, CLI entrypoints, and repository maps. The analysis is anchored to the repository’s then-latest `master` commit retrieved during research (e.g., commit `d7f32c6…`, dated **2026‑03‑28**). fileciteturn5file0L1-L1  

Kain—at least in the parts directly inspectable here—exhibits a distinctive design: a **Python-style indentation-sensitive syntax** paired with **Rust-like surface constructs**, and a runtime story that includes an **interpreter with a built-in actor system**, a **JSX-like UI AST**, and a **minimal async “future polling” executor**. These are not merely aspirational: the lexer inserts `INDENT/DEDENT` tokens; the parser recognizes first-class constructs such as `component`, `actor`, `shader`, `macro`, `comptime`, and `test`; and the runtime evaluates a typed program with native functions, closures, message-passing actors, and async polling. fileciteturn14file0L1-L1 fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  

A particularly important finding is that the implementation appears to be **mid-evolution**: some features are explicitly *feature-gated* (e.g., struct literal parsing), and there are **internal inconsistencies** between default capability settings and tests in the same module, suggesting rapid iteration or incomplete integration. fileciteturn12file0L1-L1  

Finally, while the repository contains extensive surface area (many crates and commands), this report’s *deepest* coverage is for the **core frontend and interpreter/runtime**, because those are the parts directly examined at source level in this session (lexer, parser, effects, type checker, runtime, CLI root). The repository maps indicate additional modules and crates beyond those inspected; where relevant, this report calls them out explicitly as *present in repository structure but not exhaustively analyzed here*. fileciteturn6file0L1-L1 fileciteturn7file0L1-L1  

## Repository structure and key files

### Repository entry points and maps

The repository provides **explicit “repomap” files** that function as a curated index of the filesystem layout. The top-level structure is summarized in `repomap.md`, and crate-level structure is enumerated in `crates/repomap.md`. These are the principal “table of contents” artifacts used for this report’s first-pass inventory. fileciteturn6file0L1-L1 fileciteturn7file0L1-L1  

Key root artifacts and their roles (as evidenced directly in-file or by their use as canonical maps):

- `repomap.md`: repository-wide file/directory inventory scaffold. fileciteturn6file0L1-L1  
- `crates/repomap.md`: crate-by-crate inventory scaffold (Rust workspace submodules). fileciteturn7file0L1-L1  
- `Cargo.toml`: workspace configuration indicating Rust-first implementation and dependency footprint. fileciteturn9file0L1-L1  
- `crates/cli/src/main.rs`: authoritative definition of the `kain` CLI (options + subcommands). fileciteturn10file0L1-L1  
- `crates/kain-core/src/{lexer,parser,types,effects,runtime}.rs`: the most direct evidence for Kain’s syntax, semantic checks, and interpreter/runtime behavior. fileciteturn14file0L1-L1 fileciteturn15file0L1-L1 fileciteturn16file0L1-L1 fileciteturn13file0L1-L1 fileciteturn17file0L1-L1  

### High-level architecture inferred from inspected entrypoints

From the inspected core, the minimal “observed architecture” is:

- **Frontend**: `Lexer` → `Parser` → AST `Program` (indentation-sensitive). fileciteturn14file0L1-L1 fileciteturn15file0L1-L1  
- **Type phase (partial)**: `types::check` produces a `TypedProgram` and typed wrappers, but many constructs remain only lightly checked or marked “not supported.” fileciteturn16file0L1-L1  
- **Runtime**: `runtime::interpret` registers program items into an `Env` and (if present) calls `main`. This interpreter includes standard library functions, module loading for `.kn` files, actors, JSX evaluation hooks, and async polling. fileciteturn17file0L1-L1  

Mermaid diagram (observed-from-code “happy path”):

```mermaid
flowchart LR
  A[.kn source text] --> B[Lexer: tokens + INDENT/DEDENT]
  B --> C[Parser: AST Program]
  C --> D[Type check: TypedProgram (partial)]
  D --> E[Runtime Env: register items + stdlib]
  E --> F[Call main() if defined]
```

(Architecture grounded in the existence of these concrete modules and their call patterns.) fileciteturn14file0L1-L1 fileciteturn15file0L1-L1 fileciteturn16file0L1-L1 fileciteturn17file0L1-L1  

## Kain language specification from the frontend implementation

This section treats the repository’s lexer/parser/runtime as a **de facto language reference**: what the code accepts and how it desugars/evaluates defines the real feature set exposed by this build.

### Lexical structure, tokens, and indentation semantics

**Indentation is significant**. The lexer emits `Newline(String)` tokens that include the trailing newline plus any leading spaces/tabs on the next line, and then post-processes these into synthetic `Indent` and `Dedent` tokens based on indentation depth (tabs count as 4 spaces). This means Kain’s block structure is primarily indentation-driven (Python-style), not brace-driven, even though braces exist for other constructs such as `{}` in JSX props or struct literals. fileciteturn14file0L1-L1  

Kain tokens reflect a **hybrid language**:

- Rust-like declaration keywords: `fn`, `let`, `mut`, `struct`, `enum`, `trait`, `impl`, `pub`, `mod`, `use`, plus `type` for type aliases and `const`. fileciteturn14file0L1-L1  
- Control-flow keywords: `if`, `else`, `elif`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`. fileciteturn14file0L1-L1  
- Concurrency/async keywords: `await` and a lowercase `async` keyword token (`AsyncKw`) used both for `async fn` and for `async` expression blocks in the parser. fileciteturn14file0L1-L1 fileciteturn15file0L1-L1  
- “First-class citizen” domain keywords: `component`, `shader`, `actor`, plus message keywords like `spawn`, `send`, `receive`, `emit` and compile-time/macro tokens like `comptime` and `macro`. fileciteturn14file0L1-L1  

The lexer includes literal forms beyond basic strings:

- Integer and float literals accept `_` separators (underscores are stripped before parsing). fileciteturn14file0L1-L1  
- Strings have both normal quoted and raw-string-like forms (`r"..."`). fileciteturn14file0L1-L1  
- There is an `FString` token form `f"..."`, and the parser implements brace-delimited expression interpolation by lexing/parsing the embedded expression substrings. fileciteturn14file0L1-L1 fileciteturn15file0L1-L1  

Comments exist in two forms in the lexer: `// ...` and `# ...`, both skipped during tokenization. fileciteturn14file0L1-L1  

### Reserved keyword policy and multi-domain hygiene

The parser enforces an unusually extensive **reserved keyword list**, explicitly including *Kain keywords*, but also **HLSL keywords**, **C++ keywords**, and **Unreal Engine macro identifiers** (e.g., `UCLASS`, `UPROPERTY`, etc.). This is an explicit design choice to prevent code generation and host integration collisions. fileciteturn15file0L1-L1  

This reserved keyword system is enforced in `parse_ident` and used in pattern parsing (bindings are validated unless the name is syntactically part of a qualified enum variant path). fileciteturn15file0L1-L1  

### Top-level declarations and module system

The parser recognizes the following **top-level items** (non-exhaustively ordered here as “language surface categories”):

- **Functions**: `fn name(params) -> Ret with Effect1, Effect2: <block>` and `async fn name(...) ...` (the parser injects the `Async` effect for `async fn`). fileciteturn15file0L1-L1  
- **Data types**: `struct`, `enum`, and `type` aliases (`type Name<T> = ...`). fileciteturn15file0L1-L1  
- **Traits and impl blocks**: `trait` and `impl`, including trait impl syntax `impl<T> Trait for Type:` and inherent impl `impl<T> Type:`. fileciteturn15file0L1-L1  
- **Visibility**: `pub` toggles `Visibility::Public` (private otherwise). fileciteturn15file0L1-L1  
- **Imports and modules**:
  - `mod Name` supports both declaration-only and inline module bodies (`mod Name:` followed by indented items). fileciteturn15file0L1-L1  
  - `use` statements support paths with `::` as well as `/` separators, plus `as` aliasing and `*` glob imports. fileciteturn15file0L1-L1  
- **Compile-time and meta**: `const`, `comptime:` blocks as items, `macro` definitions, and `test` blocks. fileciteturn15file0L1-L1  

A distinctive behavior: if there are **top-level statements** that are not items, the parser wraps them into an implicit `main` function and appends it to the program’s item list. This is an explicit “script mode” feature in the parser. fileciteturn15file0L1-L1  

### Expression language and syntactic desugarings

The parser implements a conventional expression grammar with significant extensions and explicit desugarings.

**Assignment** supports `=`, compound operators (`+=`, `-=`, etc.), and composes compound assignment into a binary expression followed by assignment at AST level. fileciteturn15file0L1-L1  

**Binary operator precedence** is defined, including boolean ops (`and`/`or` tokens map to logical ops), arithmetic, comparisons, and power (`**`). Bitwise and shift operators are present in the token set and precedence table, but are routed through **capability checks** (feature gating). fileciteturn14file0L1-L1 fileciteturn15file0L1-L1 fileciteturn12file0L1-L1  

**Null coalescing** (`??`) and **safe navigation** (`?.`) are first-class tokens and are explicitly desugared into `match` expressions using generated temporary bindings (e.g., `__kain_coalesceN`, `__kain_safeN`). fileciteturn14file0L1-L1 fileciteturn15file0L1-L1  

**Ternary conditional** `cond ? then : else` is parsed, but is also desugared into a `match` on boolean literals (`true`/`false`). fileciteturn15file0L1-L1  

**Increment/decrement** (`++x`, `x++`, `--x`, `x--`) is supported and is translated into a match + temporary-binding + sequencing trick (implemented using array indexing on a synthetic `[assign, result]` array to force evaluation order). This transformation is performed directly in the parser. fileciteturn15file0L1-L1  

**Lambdas** exist in two syntactic forms:
- Pipe-lambdas: `|x, y| expr` (parameters inferred). fileciteturn15file0L1-L1  
- `fn`-lambdas as expressions: `fn(x: Int) -> Int: <body>` where `<body>` can be an expression or a block. fileciteturn15file0L1-L1  

**Match expressions** use indentation-based bodies and support multi-line arm bodies (parsed as statement blocks when indented). fileciteturn15file0L1-L1  

**If expressions** support `if`, `elif`, `else`, including an “else if” form without a nested colon keyword boundary. The parser supports both block form and inline single-statement form depending on whether a newline/indent follows. fileciteturn15file0L1-L1  

**Collections and indexing**: arrays are `[...]` with multiline support, indexing uses `obj[idx]`, and the index expression itself can be a range (`..`, `..=`, `start..end`, `start..=end`). fileciteturn15file0L1-L1  

**Casting** exists via `as Type`. fileciteturn15file0L1-L1  

**Error propagation** uses postfix `?` (Try), which the runtime interprets as “unwrap Ok or return Err” semantics over a runtime `Result` representation. fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  

### Types and the effect system

Kain’s type syntax includes references (`&` with optional `mut`), arrays (`[T; N]`), slices (`[T]`), tuples (`(A, B)`), `impl Trait`, function types `fn(T)->R`, and named generic types `Type<T, U>`. The parser also provides special-case named pointer types (`ptr<T>` and `ptr_mut<T>`) that lower to a `Ptr` type AST node. fileciteturn15file0L1-L1  

The type checker defines a `ResolvedType` universe including primitives, arrays/slices/tuples, options/results, refs/ptrs, functions with effect sets, structs/enums, generics, and `Never`. It also defines built-in types in the environment (e.g., `Int`, `UInt`, `Float`, `Bool`, `Char`, `String`, and even `Vec2`/`Vec3` as tuple types of `f32`). fileciteturn16file0L1-L1  

However, the current `resolve_type` implementation is explicitly incomplete (many variants fall back to `Unknown`), and item checking returns “not yet supported” for certain constructs. This strongly suggests the type checker is a partial implementation relative to the parser’s accepted surface. fileciteturn16file0L1-L1  

Kain includes an explicit **effect system** intended to enforce “what kinds of side effects a function may perform.” The enum includes `Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`, and also `Alloc` and `Panic`, and effect checking is implemented as a “caller must be at least as effectful as callee” policy (with `Unsafe` as an override). fileciteturn13file0L1-L1  

### First-class domain constructs

#### Components and JSX-like UI syntax

The language includes a `component` top-level construct with parameter-like props, optional state declarations, methods, and a `render` body that is parsed as a JSX-like element tree. JSX syntax uses `<Tag ...>children</Tag>` or self-closing `<Tag .../>`, with attribute values as either strings or `{expr}` blocks; it supports fragments via `<Fragment>...</Fragment>`. fileciteturn15file0L1-L1  

JSX children support embedded control blocks inside `{ ... }` including `{if cond: <A/> else: <B/>}` and `{for x in xs: <Row/>}` parsing into dedicated JSX AST nodes. fileciteturn15file0L1-L1  

The runtime evaluates JSX nodes via a `ui::eval_jsx` hook and represents JSX values as `Value::JSX(VNode)`, indicating this is not just syntax—there is a runtime representation prepared for it. fileciteturn17file0L1-L1  

#### Actors, message passing, and concurrency

The parser recognizes an `actor` top-level item with state variables (`state` or `var`), `fn` methods, and message handlers introduced by the identifier `on`. It also recognizes expressions `spawn ActorName(...)` and `send <call>`, where `send` desugars a method-like call into a `SendMsg` expression requiring named arguments. fileciteturn15file0L1-L1  

The runtime implements actors concretely using channels (`flume`) and OS threads: `spawn` creates a new actor instance with its own `Env`, initializes state, then loops receiving messages and dispatching to matching handlers. fileciteturn17file0L1-L1  

#### Shaders and uniforms

The parser recognizes a `shader` item with an optional stage marker (`vertex`, `fragment`, or `compute` by contextual identifier), name, input params, output type, and an indented body. Inside shader bodies, it specifically parses `uniform name: Type @<binding-int>` declarations. For compute shaders, it validates explicit compute metadata through a method `explicit_compute_metadata()` on the shader AST. fileciteturn15file0L1-L1  

Even absent full backend inspection, this provides direct evidence that Kain treats shader authoring as a first-class language domain, with binding layout integrated into syntax. fileciteturn15file0L1-L1  

#### Attribute-driven DSLs

The parser’s item parsing path checks for specific attributes (e.g., `@material_graph`, `@graph_editor`, `@state_machine`, `@editor_module`, `@gameplay_tags`, `@ability`, `@gameplay_effect`, `@gameplay_cue`, `@ability_task`, `@target_actor`) and routes to specialized parsers for each, creating distinct AST item variants rather than treating them as normal structs/functions. fileciteturn15file0L1-L1  

This design pattern indicates Kain is not “only” a general-purpose language; it embeds **domain-specific schema languages** (e.g., hierarchical gameplay tags, structured ability specs, effect specs, state machine graphs) into the core parser, with indentation-based blocks acting as the serialization format. fileciteturn15file0L1-L1  

## Runtime, module loading, and standard library surface

### Interpreter value model and environment

The runtime defines a rich `Value` model including primitives, arrays (mutable via `Arc<RwLock<Vec<Value>>>`), tuples, structs (maps), host objects for embedding (`Arc<dyn Any + Send + Sync>`), function references, native functions, actor references, option-like `None`, and control-flow markers (`Return`, `Break`, `Continue`). It also includes representations for enum variants, polling (`Poll`), and “future state machines.” fileciteturn17file0L1-L1  

Runtime execution occurs inside an `Env` environment holding lexical scopes, functions, components, actor definitions, and method tables for `impl`-style dispatch. The interpreter registers a program, then calls `main()` if it exists. fileciteturn17file0L1-L1  

The runtime also supports an extension mechanism: external code can register an environment extension registrar in a global registry, and the environment applies these registrars during initialization. This is explicit evidence that Kain is designed for embedding/hosting. fileciteturn17file0L1-L1  

### Module loading behavior (`use`)

Runtime module loading converts a `use` path into a file search strategy:

- Inline module bodies are supported: the runtime stores inline module item lists under a path key and resolves `use` against them, including `glob` and `alias` forms. fileciteturn17file0L1-L1  
- For “stdlib” modules, it searches `stdlib` roots discovered from:
  - `KAIN_STDLIB_PATH` env var,
  - parent directories of the executable,
  - parent directories of the current working directory,
  looking for `stdlib/<module>.kn`. fileciteturn17file0L1-L1  
- For non-stdlib modules, it tries several relative paths such as `./path.kn`, `./src/path.kn`, and a legacy `.god` extension. fileciteturn17file0L1-L1  

This is a concrete, code-specified module resolution scheme that is more akin to scripting languages than Rust/C++ module systems. fileciteturn17file0L1-L1  

### Standard library and built-in operations

The runtime registers a broad set of native functions. Because this report is grounded in source-level evidence, the list below is limited to functions explicitly registered in the inspected runtime module (not claims about uninspected stdlib `.kn` files).

Selected groups (examples, not an exhaustive list of every helper duplicated by copy/paste in `runtime.rs`):

- **I/O and debugging**: `print`, `println`, `eprint`, `eprintln`, `dbg`, `read_line`, `stdout_write`, `stdin_read_exact`. fileciteturn17file0L1-L1  
- **Math/time**: `min`, `max`, `abs`, `sqrt`, `sin`, `cos`, `tan`, `now`, `time`. fileciteturn17file0L1-L1  
- **Collections and strings**: `len`, `push`, `first`, `last`, `reverse`, `sum`, `split`, `join`, `trim`, `upper/to_upper`, `lower/to_lower`, `contains`, `starts_with`, `ends_with`, `replace`, `char_at`, `substring`. fileciteturn17file0L1-L1  
- **Conversions and reflection**: `type_of`, `variant_of`, `variant_field`, `str`, `int`, `float`, `bool`, `to_string`. fileciteturn17file0L1-L1  
- **Error handling**: `assert`, `panic`, plus constructors `ok` and `err` for the runtime’s `Result` representation. fileciteturn17file0L1-L1  
- **Networking and JSON**: `http_get`, `http_post_json`, `json_parse`, `json_string`. fileciteturn17file0L1-L1  

Macro support at runtime is **mostly built-in**: the interpreter recognizes macro calls and implements only a small set (`vec!`, `format!`, `type_name!`, and formatting helpers such as `__kain_write_fmt`/`__kain_writeln_fmt`). User-defined `macro` items are parsed, but the interpreter does not implement general macro expansion using those definitions. fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  

### Async runtime semantics (as implemented)

Async support is implemented in the interpreter using a polling model:

- `await <expr>` evaluates the expression to a “future value,” then repeatedly polls it to completion (with a hard iteration cap to avoid infinite loops). fileciteturn17file0L1-L1  
- The runtime defines native functions `block_on`, `spawn_task`, `poll_once`, `is_ready`, `is_pending`, and `unwrap_ready`. fileciteturn17file0L1-L1  
- A “future” may be represented as `Value::Future(name, state_map)` or as a struct with an associated `Type_poll` function name convention. Poll results are normalized into a `Value::Poll(ready, value)` representation, and there is special treatment for an enum named `Poll` with variants `Ready` and `Pending`. fileciteturn17file0L1-L1  

This is best characterized as a **minimal embedded executor** suitable for deterministic or single-threaded “tick style” futures, rather than a full async runtime. fileciteturn17file0L1-L1  

## CLI commands, flags, and usage surface

### CLI command inventory (root + subcommands)

The file `crates/cli/src/main.rs` defines the `kain` CLI entry point and is the primary authoritative source for command names, top-level flags, and subcommand existence. fileciteturn10file0L1-L1  

From the inspected definitions, the CLI includes (at minimum) the following subcommands:

- `init`, `lsp`, `doctor`
- `selfhost`, `omni`, `fabric`
- `build`, `run`
- `gpu-artifacts`, `inject`
- import pipeline commands: `import-asm`, `import-c`, `import-rust`, `import-crate`, `import-ts` fileciteturn10file0L1-L1  

The CLI also supports a “legacy/script” mode where code can be provided either as an input path or as a snippet, and the command includes flags that refer to emitting AST/typed output and analysis/verbosity controls (as indicated by option names in the CLI struct). fileciteturn10file0L1-L1  

Because the report environment here does not execute the binary to capture `--help` output, this section focuses on **structural extraction** (subcommand names and conceptual roles) and ties commands to the repository’s CLI module structure (which is explicitly mapped in `crates/repomap.md`). fileciteturn7file0L1-L1 fileciteturn10file0L1-L1  

### Command-to-module mapping

The `crates/cli` directory contains dedicated Rust modules corresponding to major subcommands (e.g., `fabric.rs`, `gpu_artifacts.rs`, `import_asm.rs`, etc.), indicating a “one module per command” architecture. This mapping is directly evidenced by `crates/repomap.md` listing those files and by the existence of the subcommands in `main.rs`. fileciteturn7file0L1-L1 fileciteturn10file0L1-L1  

A compact comparison table (command family vs likely implementation locus):

| CLI surface | Evidence of existence | Primary implementation locus (file path evidence) |
|---|---|---|
| `kain` root options + dispatch | `crates/cli/src/main.rs` | `crates/cli/src/main.rs` fileciteturn10file0L1-L1 |
| Language/toolchain “fabric” workflow | Subcommand present | `crates/cli/src/fabric.rs` listed in repo map fileciteturn7file0L1-L1 |
| GPU artifact tooling | Subcommand present | `crates/cli/src/gpu_artifacts.rs` listed in repo map fileciteturn7file0L1-L1 |
| Injection tooling | Subcommand present | `crates/cli/src/inject.rs` listed in repo map fileciteturn7file0L1-L1 |
| Import pipelines (ASM/C/Rust/TS) | Subcommands present | `crates/cli/src/import_*.rs` listed in repo map fileciteturn7file0L1-L1 |
| IDE support | `lsp` present | `crates/cli/src/lsp.rs` listed in repo map fileciteturn7file0L1-L1 |

This table is intentionally conservative: it maps **what exists** to **where it lives** rather than speculating behavior without reading each module file in depth. fileciteturn7file0L1-L1  

### Usage examples (repository-grounded, non-executed)

The parser’s “implicit main” and the runtime’s “interpret typed program and call main” imply a basic workflow such as:

```bash
kain path/to/program.kn
```

or a snippet-based mode (e.g., `-c "<code>"`) if exposed by CLI flags (as suggested by the CLI struct fields). fileciteturn10file0L1-L1 fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  

Similarly, the presence of `import-asm`, `import-c`, `import-rust`, `import-crate`, and `import-ts` commands indicates the toolchain surface includes cross-language import pipelines, though the *formats, flags, and transformations* require per-module inspection beyond the core files examined here. fileciteturn10file0L1-L1 fileciteturn7file0L1-L1  

## Compiler pipeline, transformations, and feature mapping

### Observed core pipeline and “IR-like” transformations

The observable compilation/execution pipeline in the inspected set is:

1. **Lexing** with indentation processing → token stream. fileciteturn14file0L1-L1  
2. **Parsing** into an AST `Program`, including specialized item grammars for components/actors/shaders and attribute-driven DSLs. fileciteturn15file0L1-L1  
3. **Type checking** into `TypedProgram` (partial). fileciteturn16file0L1-L1  
4. **Runtime interpretation** of the typed program. fileciteturn17file0L1-L1  

Notably, there are **semantic transformations** performed *inside the parser* that behave like an early IR-lowering step:

- `x ?? y` becomes a `match` expression that branches on `none` vs a binding. fileciteturn15file0L1-L1  
- `obj?.field` becomes a `match` that produces `none` if the scrutinee is `none`, otherwise extracts the field. fileciteturn15file0L1-L1  
- `cond ? a : b` becomes a `match` on boolean literals. fileciteturn15file0L1-L1  
- `++x`/`x++` becomes a temporary-binding match with sequencing. fileciteturn15file0L1-L1  

Mermaid diagram (parser-desugaring “micro pipeline”):

```mermaid
flowchart TD
  S[Surface syntax] --> P[Parser]
  P -->|desugar: ??, ?., ?:, ++/--| A[Core AST forms:\nMatch, Assign, Binary, Ident]
  A --> T[Type checker (partial)]
  T --> R[Runtime interpreter]
```

(Desugarings are implemented by name in the parser source; runtime behavior for resulting constructs is implemented in `runtime.rs`.) fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  

### Feature gating and capability-driven parsing/runtime

Kain explicitly centralizes certain feature gates in `language_features.rs` via `LanguageCapabilities` and helper predicates such as `supports_parser_struct_literals()` and `supports_parser_binary_op(op)`. Runtime also consults a `runtime_supports_binary_op` predicate to allow/disallow bitwise/shift operators at evaluation time. fileciteturn12file0L1-L1 fileciteturn17file0L1-L1  

An important nuance: the repository contains an internal test in `language_features.rs` asserting that struct literals remain disabled, while the same module’s default capabilities appear to enable them by default—an inconsistency that should be treated as a signal of in-progress feature rollout. fileciteturn12file0L1-L1  

### Feature support matrix (frontend vs type checker vs runtime)

The table below is a **repository-grounded support matrix**, focused on observed behavior in the inspected files. “Partial” means the syntax exists and is parsed, but typing/runtime semantics are incomplete or stubbed.

| Feature | Parser | Type checker | Runtime |
|---|---|---|---|
| Indentation blocks | Yes (INDENT/DEDENT tokens) fileciteturn14file0L1-L1 | N/A | Yes (block eval executes stmt lists) fileciteturn17file0L1-L1 |
| `fn`, `async fn`, effects `with ...` | Yes fileciteturn15file0L1-L1 | Partial (effects recorded; limited type resolution) fileciteturn16file0L1-L1 | Yes (functions callable; async polling helpers exist) fileciteturn17file0L1-L1 |
| Effects policy enforcement | Utility exists (`check_effect_call`) fileciteturn13file0L1-L1 | Unclear integration | Unclear integration (not enforced in call path) |
| `if/elif/else`, `match` | Yes fileciteturn15file0L1-L1 | Minimal | Yes (interpreted) fileciteturn17file0L1-L1 |
| Ternary `?:` | Yes (desugars to match) fileciteturn15file0L1-L1 | Minimal | Yes (via match) fileciteturn17file0L1-L1 |
| Null coalescing `??`, safe-nav `?.` | Yes (desugars to match) fileciteturn15file0L1-L1 | Minimal | Yes (via match) fileciteturn17file0L1-L1 |
| Struct literals `Type { field: ... }` | Feature-gated; may error if disabled fileciteturn15file0L1-L1 | Not fully modeled | Yes (runtime constructs structs; supports update/rest) fileciteturn17file0L1-L1 |
| Macros (`macro` items, `ident!(...)`) | Yes (parsing) fileciteturn15file0L1-L1 | Stored but not expanded | Runtime: only a small built-in macro set fileciteturn17file0L1-L1 |
| Components + JSX | Yes fileciteturn15file0L1-L1 | Minimal | Partial (value exists; evaluation hooks exist) fileciteturn17file0L1-L1 |
| Actors (`actor`, `spawn`, `send`) | Yes fileciteturn15file0L1-L1 | Minimal | Yes (threads + channels implementation) fileciteturn17file0L1-L1 |
| Shaders + uniforms | Yes fileciteturn15file0L1-L1 | Minimal | Not executed as shaders in interpreter (not registered) fileciteturn17file0L1-L1 |
| Pointer/memory ops (`addr_of`, `ptr_offset`, `mem_load`, etc.) | Yes (normalized from calls) fileciteturn15file0L1-L1 | Not fully modeled | Mostly placeholder semantics fileciteturn17file0L1-L1 |

This matrix is intentionally limited to features that can be supported with direct code evidence from the inspected modules. fileciteturn15file0L1-L1 fileciteturn16file0L1-L1 fileciteturn17file0L1-L1  

## Limitations, inconsistencies, and potential extensions

### Limitations and inconsistencies evidenced in source

Several concrete limitations are directly visible:

- **Feature-gate/test inconsistency**: `language_features.rs` includes both a default capability configuration and a test asserting the opposite behavior for struct literals, implying either stale tests or a gated rollout mishap. fileciteturn12file0L1-L1  
- **Type checker incompleteness**: `resolve_type` returns `Unknown` for many type constructs and the checker explicitly reports some items as “not yet supported,” despite the parser accepting many advanced constructs. fileciteturn16file0L1-L1  
- **Effect enum parsing coverage gaps**: the effect enum includes `Alloc` and `Panic`, and string rendering includes them, but `Effect::from_str` only handles a subset (`Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`). This suggests incomplete integration of new effects into the “from string” path. fileciteturn13file0L1-L1  
- **Runtime stubs for low-level memory**: pointer and raw memory operations are parsed and normalized, but the interpreter implements them as pass-through or coarse approximations, clearly marking them as “parseable/executable in non-native backends” rather than truly modeled memory. fileciteturn17file0L1-L1  

### Research gaps relative to requested scope

The repository itself clearly contains much broader surface area than the inspected core. The CLI indicates additional substantial subsystems—`fabric`, `selfhost`, `omni`, multi-language import pipelines, GPU artifacts tooling, injection tooling, and LSP—whose precise semantics require focused per-module analysis. Their existence is certain (subcommand names + file-module presence in the repo map), but their behavior is not documented here at the same depth as the frontend/runtime because the relevant modules were not opened in this session. fileciteturn10file0L1-L1 fileciteturn7file0L1-L1  

Similarly, `README.md` (retrieved from a different ref than the analyzed `master` sources) asserts broad goals such as “compiled multi-target toolchain,” but this report treats those statements as informational and prioritizes the inspected `master` code as ground truth. fileciteturn3file0L1-L1 fileciteturn17file0L1-L1  

### High-confidence extension directions implied by the architecture

Based on the observed design, the most natural extensions—consistent with the repository’s declared intent and current partial implementations—would likely include:

- Completing the **type checker** to cover the full parsed type grammar (arrays/slices/options/results/refs/pointers, generics, traits/impl resolution), and aligning typed representations with runtime evaluation and/or code generation phases. fileciteturn16file0L1-L1  
- Integrating the **effect system** into semantic analysis and runtime/call validation, ensuring effect annotations are enforced consistently and that the supported effect set is complete across parsing (`with`), checking (`check_effect_call`), and execution constraints. fileciteturn13file0L1-L1 fileciteturn15file0L1-L1  
- Implementing **general macro expansion** (beyond built-ins), since the parser supports macro definitions and macro calls syntactically, and the runtime already recognizes macro call AST nodes. fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  
- Strengthening or formalizing the **async model**, potentially aligning `async fn` lowering with the runtime’s poll conventions (`Type_poll`) and integrating effect tracking (`Async`) into the type system and executor. fileciteturn15file0L1-L1 fileciteturn17file0L1-L1  

These are framed as extensions because the repository already contains the necessary “hooks” (syntax, runtime representations, and partial implementations) that make these plausible next steps. fileciteturn15file0L1-L1 fileciteturn16file0L1-L1 fileciteturn17file0L1-L1