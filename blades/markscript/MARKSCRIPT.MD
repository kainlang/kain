# MARKSCRIPT 1.0 — Specification & Contract

**This document is the canonical specification.** It defines what Markscript is, what it is not, and the invariant boundaries that must hold in every version. It is not a README, tutorial, or marketing document. It does not track progress — it states truth.

---

## Identity

Markscript is a **markdown-native bytecode VM** that serves as Kain's companion language for configuration, orchestration, and executable documentation. It is implemented in Kain and compiles through Kain's LLVM backend to native code.

**Core property:** Markdown has no syntax errors. Every `#`, `>`, `|`, and ` ``` ` is valid. The only errors are runtime errors — name not found, arity mismatch, bounds violation, import failure.

---

## The Markdown → Semantics Mapping

| Markdown Construct | Markscript Semantics | Bytecode |
|-------------------|---------------------|----------|
| `# Title` | **Domain** — named scope | `OP_ENTER_DOMAIN hash` |
| `## Title` | **Routine** — named executable block | `OP_ROUTINE_HEADER hash` |
| `> phrase` | **Intent** — natural-language command dispatched through IVT | `OP_PUSH_PARAM hash` + `OP_EXECUTE_CALL` |
| `\| col \| col \|` | **Matrix** — typed data table stored in VM | `OP_PUSH_MATRIX handle cols rows data[]` |
| ` ```lang ` | **Fenced block** — code or data stored by language tag | `OP_FENCED_CODE lang_hash content_hash` |
| `- item` / `1. item` | List markers — parsed, reserved for iteration | Lexed, no bytecode |
| `*italic*` / `**bold**` | Inline formatting — parsed, reserved | Lexed, no bytecode |
| `[text](url)` | Link — parsed, reserved | Lexed, no bytecode |
| Plain text | Documentation — consumed, not emitted | `TOK_TEXTSTR`, parser skips |

---

## Tokens (22 Types)

| Constant | Value | Trigger |
|----------|-------|---------|
| `TOK_HEADER1` | 0 | `#` at line start |
| `TOK_HEADER2` | 1 | `##` at line start |
| `TOK_BLOCKQUOTE` | 2 | `>` at line start |
| `TOK_TABLEPIPE` | 3 | `\|` anywhere |
| `TOK_TEXTSTR` | 4 | Plain text, any non-structural content |
| `TOK_EOF` | 5 | End of source |
| `TOK_HEADER3` | 6 | `###` at line start |
| `TOK_HEADER4` | 7 | `####` at line start |
| `TOK_HEADER5` | 8 | `#####` at line start |
| `TOK_HEADER6` | 9 | `######` at line start |
| `TOK_FENCE` | 10 | ` ``` ` (opening/closing) |
| `TOK_LANG_TAG` | 11 | Language identifier after ``` |
| `TOK_FENCED_CODE` | 12 | Content inside ` ```...``` ` |
| `TOK_BOLD` | 13 | `**text**` |
| `TOK_ITALIC` | 14 | `*text*` |
| `TOK_CODE_SPAN` | 15 | `` `inline code` `` |
| `TOK_LIST_UNORDERED` | 16 | `-`, `*`, `+` at line start |
| `TOK_LIST_ORDERED` | 17 | `1.`, `2.` at line start |
| `TOK_LINK_TEXT` | 18 | `[text]` |
| `TOK_LINK_URL` | 19 | `(url)` |
| `TOK_HR` | 20 | `---`, `***`, `___` |
| `TOK_NEWLINE` | 21 | Explicit newline between inline content |

---

## Bytecode Opcodes (20 Total)

### Parser-Emitted (7)

| Opcode | Value | Format |
|--------|-------|--------|
| `OP_HALT` | 0 | Single opcode — terminate execution |
| `OP_ENTER_DOMAIN` | 1 | `OP, domain_name_hash` |
| `OP_ROUTINE_HEADER` | 2 | `OP, routine_name_hash` |
| `OP_PUSH_PARAM` | 3 | `OP, intent_phrase_hash` |
| `OP_EXECUTE_CALL` | 4 | Single opcode — dispatch accumulated params |
| `OP_PUSH_MATRIX` | 5 | `OP, handle_id, cols, rows, data_count, d0..dN` |
| `OP_FENCED_CODE` | 6 | `OP, lang_hash, content_hash` |

### VM-Only (10)

| Opcode | Value | Operation |
|--------|-------|-----------|
| `OP_PUSH_STACK` | 7 | Push immediate operand to stack |
| `OP_POP_STACK` | 8 | Pop stack → accumulator |
| `OP_DUP` | 9 | Duplicate top of stack |
| `OP_CALL` | 10 | IVT lookup + subroutine call with return address |
| `OP_RET` | 11 | Pop return address, resume execution |
| `OP_JMP` | 12 | Unconditional jump to IP |
| `OP_JZ` | 13 | Jump if zero (pop stack, compare) |
| `OP_ADD` | 14 | Pops two values, pushes their sum |
| `OP_SUB` | 15 | Pops two values, pushes the difference (a - b) |
| `OP_MUL` | 16 | Pops two values, pushes their product |
| `OP_DIV` | 17 | Pops two values, pushes the quotient (a / b). Traps if zero. |
| `OP_LOAD_VAR` | 18 | Takes a name_hash, looks up the variable, and pushes its value |
| `OP_STORE_VAR` | 19 | Pops the top stack value and writes/updates it under name_hash |
| `OP_JN` | 20 | Pops a value. Jumps to instruction pointer if value < 0 |

---

## The IVT (Intent Vector Table)

The IVT is the bridge between Markscript intents and Kain execution. It maps a hashed natural-language phrase to a registered Kain function handler.

### Registration

```kain
fn register_handler(vm: MarkScriptVM, phrase: String, handler: HandlerFn) -> MarkScriptVM
```

### Handler Signature

```kain
type HandlerFn = fn(vm: MarkScriptVM, args: Array<MarkValue>) -> HandlerResult
```

### MarkValue — Typed Runtime Data

```kain
struct MarkValue:
    kind:       Int     // MARK_INT, MARK_FLOAT, MARK_STRING, MARK_TABLE, MARK_CODE
    int_val:    Int
    float_val:  Float
    str_val:    String
```

### HandlerResult — Value-Semantic Return

```kain
struct HandlerResult:
    vm:     MarkScriptVM   // updated VM state
    value:  MarkValue      // result value
    err:    String         // empty = success, non-empty = error message
```

### Dispatch Flow

```
.md source → lex → parse → OP_PUSH_PARAM(phrase_hash) → OP_EXECUTE_CALL
                                                                │
                                                                ▼
                                                    IVT lookup by phrase_hash
                                                                │
                                                    ┌───────────┴───────────┐
                                                    │ found                  │ not found
                                                    ▼                        ▼
                                          call handler(args)         NAME_ERROR
                                          return value to VM
```

### Built-in Intent Handlers

| Intent Pattern | Kain Handler | Maps To |
|---------------|-------------|---------|
| `> read file "path"` | `handler_fs_read` | `std::fs::fs_read_text(path)` |
| `> write file "path" "content"` | `handler_fs_write` | `std::fs::fs_write_text(path, content)` |
| `> run "command"` | `handler_process_run` | `std::process::process_spawn(...)` |
| `> import kain "module"` | `handler_import_kain` | Kain module loader |
| `> assert value expected` | `handler_assert` | Equality check, error on mismatch |
| `> print value` | `handler_print` | `println(str(value))` |

---

## Table Type System

Markscript tables are parsed into flat, contiguous memory matrices. The compiler automatically infers types row-by-row to construct zero-overhead Kain schemas.

### Type Detection

| Cell Value | Inferred Type |
|-----------|---------------|
| `42`, `0`, `-9`, `5000000` | `Int` |
| `true`, `false`, `yes`, `no` | `Int` (natively coerced to `1` or `0`) |
| `1.05`, `2.3`, `0.001`, `-3.14` | `Float` |
| Everything else | `String` |

### Column Widening

If a column contains both `Int` and `Float` values, the column is `Float` (Ints widened). If any cell is `String`, the column is `String`.

### Separator Rows

Rows containing only `|`, `-`, `:`, and spaces are separator rows. They are not stored. The row before the separator is the header row. The row after is the first data row.

```markdown
| Object | Mass | Friction |      ← header row (column names)
|--------|------|----------|      ← separator row (skipped)
| Player | 80   | 0.45     |      ← data row
| Crate  | 200  | 0.12     |      ← data row
```

---

## `@import` — Multi-File Composition

```markdown
@import "path/to/other.md"
@import "../shared/handlers.md"
```

### Rules

1. Paths resolve relative to the importing file's directory
2. `@import` is a compile-time directive — resolved before bytecode emission
3. Imported domains and routines merge into the calling file's namespace
4. Duplicate domain names: last import wins, warning emitted
5. Circular imports: detected at compile time, hard error
6. Maximum import depth: 16

---

## Importing Kain Modules

Markscript can import Kain modules through the intent system.

```markdown
> import kain "path/to/module.kn"
```

Every `pub fn` in the imported module becomes available as an intent:

```markdown
> call module.function arg1 arg2
```

Function parameter types determine how arguments are parsed from the intent. Return values are captured in the VM accumulator.

---

## Data-Driven Intent Keyword Registry (2.0)

The intent keyword registry is **data-driven**. The single source of truth is `std/intents.md` — a markdown table with 57 single-word intent keywords. Adding a new intent requires editing ONLY that file and registering the handler in `bridge.kn`.

### Architecture

```
std/intents.md (markdown table)
    │
    ▼ load_registry()
registry.kn (IntentRegistry world)
    │ is_keyword()
    ▼
parser.kn (prose/intent disambiguation)
```

### Flow

1. **Startup:** `main.kn` calls `load_intent_registry()` before every compilation
2. **Load:** `registry.kn` reads `std/intents.md`, parses the keyword table, populates `IntentRegistry` world
3. **Lookup:** `parser.kn` calls `is_keyword(word)` for each blockquote's first word
4. **Disambiguation:** Three-way classification (keyword → intent, prose-starter → doc, unknown → doc)

### Registry Module (`src/registry.kn`)

- **`load_registry(path)`:** Reads the markdown table, extracts keyword column, populates `IntentRegistry` world
- **`is_keyword(word)`:** Hash-based lookup against the loaded keyword set
- **`is_loaded()`:** Returns whether the registry has been loaded
- **`keyword_count()`:** Returns the number of loaded keywords

### The Prose-Starter List

A closed, stable set of ~50 English words that begin sentences. When a blockquote's first word matches one of these, it is treated as prose documentation (no bytecode emitted). The list includes:

| Category | Words |
|----------|-------|
| **Articles/Determiners** | The, This, That, These, Those, A, An, Some, Any, All, Each, Every, One, No, Other |
| **Pronouns** | I, We, You, He, She, It, They |
| **Prepositions** | In, At, On, By, From, To, With, For, As, So, Not, Just, Also |
| **Conjunctions** | And, But, Or, If, Because, However |
| **Interrogatives** | Who, What, When, Where, Why, Which, How |
| **Discourse Markers** | Then, Now, Here, There, Still, Therefore, However, Meanwhile, Nevertheless, Furthermore, Moreover, Thus, Hence |

### How to Add a New Intent Keyword

1. Add a row to `std/intents.md` with: keyword, handler_fn, handler_id, description
2. Register the handler in `src/bridge.kn` with a new function and handler ID
3. Rebuild: `kain build`

No parser, compiler, or registry changes needed.

---

## Error Model

All errors are runtime errors with structured reporting. Markdown itself cannot produce parser errors.

### Error Structure

```kain
struct MarkError:
    kind:       Int         // error kind constant
    message:    String      // human-readable description
    line_no:    Int         // source line number
    domain:     String      // current domain name
    routine:    String      // current routine name
    suggestion: String      // did-you-mean hint (empty if none)
```

### Error Kinds

| Kind | Constant | Cause |
|------|----------|-------|
| Name Error | `ERROR_NAME` | IVT lookup miss — intent phrase not registered |
| Arity Error | `ERROR_ARITY` | Wrong number of arguments passed to handler |
| Bounds Error | `ERROR_BOUNDS` | Table row/column access out of range |
| Type Error | `ERROR_TYPE` | Type mismatch in typed table cell |
| Import Error | `ERROR_IMPORT` | `@import` path not found |
| Circular Import | `ERROR_CIRCULAR_IMPORT` | Import cycle detected |

### Error Format

```
Error: <kind>: <message>
  at line <N>, domain "<domain>", routine "<routine>"
  suggestion: <suggestion>
```

### Did-You-Mean

When an IVT lookup fails, the error engine searches the registered handler phrase registry for the closest match (edit distance ≤ 3). The closest match is included in `suggestion`.

```
Error: name error: unknown intent "apply graviti"
  at line 14, domain "PhysicsSim", routine "physics_tick"
  suggestion: did you mean "apply gravity"?
```

---

## The Markdown Stdlib

Markscript's standard library is written in Markscript itself — markdown files that use intents to dispatch to Kain stdlib functions through the IVT. Each file is both documentation and a valid program.

```
std/
├── io.md          — file read, write, list, exists
├── math.md        — arithmetic, random, clamp, min, max
├── build.md       — project check, build, run, test
├── test.md        — assert, expect, benchmark
└── process.md     — spawn, capture output, exit code
```

### Example: `io.md`

```markdown
# IO

## read
> read file "path"
> returns content as string

## write
> write file "path" "content"
> returns bytes written

## exists
> file exists "path"
> returns 1 if exists, 0 if not
```

The intent `> read file "path"` dispatches through the IVT to `handler_fs_read`, which calls `std::fs::fs_read_text("path")`. Running `io.md` through the VM verifies the intent→stdlib dispatch loop is intact.

---

## Runtime Interface

### Markscript → Kain

Intents dispatch to registered Kain handlers through the IVT. Handlers receive typed `MarkValue` arguments and return `HandlerResult`.

### Kain → Markscript (Embedding)

Any Kain program can embed the Markscript VM:

```kain
let content = fs_read_text("config.md")
let lex = create_lexer(content)
let bc = compile_source(lex)
let vm = init_vm()
let vm = register_handler(vm, "read file", handler_fs_read)
let result = execute_bytecode(vm, bc)
// result.vm.data_table contains parsed tables
// result.value contains final accumulator
```

### Shared Substrate

Both Markscript and Kain compile through the same LLVM backend to the same native binary. Both link against the same C runtime. Both access the same Kain stdlib. The separation is at the language level only.

---

## VM State

```kain
struct MarkScriptVM:
    ip:             Int                     // instruction pointer
    accumulator:    MarkValue               // primary result register
    stack:          Array<MarkValue>        // operand stack
    data_table:     Array<MatrixRecord>     // handle → typed matrix
    data_table_cnt: Int                     // next free handle
    code_blocks:    Array<CodeBlockRecord>  // fenced code blocks
    call_stack:     Array<Int>              // return addresses
    ivt:            Array<IVTEntry>         // intent vector table
    ivt_count:      Int                     // registered handler count
    variables:      Array<VarEntry>         // named variable store
    var_count:      Int                     // variable count

struct MatrixRecord:
    handle_id:  Int
    cols:       Int
    rows:       Int
    col_types:  Array<Int>   // MARK_INT | MARK_FLOAT | MARK_STRING per column
    data:       Array<MarkValue>

struct CodeBlockRecord:
    lang_hash:    Int
    content_hash: Int

struct IVTEntry:
    phrase_hash: Int
    handler_id:  Int

struct VarEntry:
    name_hash:  Int
    value:      MarkValue
```

---

## Source Layout

```
markscript/
├── MARKSCRIPT.MD               ← this file (the contract)
├── build.kn                    — Kain project authority
├── KAIN.toml                   — compatibility manifest
│
├── src/
│   ├── main.kn                 — CLI, disassembler, execution driver
│   ├── lexer.kn                — 22 token types, value-semantics tokenizer
│   ├── registry.kn             — data-driven intent keyword loader (std/intents.md)
│   ├── parser.kn               — single-pass token→bytecode compiler, prose/intent disambiguation
│   ├── vm.kn                   — 17-opcode stack VM, typed data, IVT dispatch
│   ├── types.kn                — MarkValue, MatrixRecord, type inference
│   ├── bridge.kn               — IVT handler registry, intent→Kain binding (81 handlers)
│   ├── error.kn                — MarkError, formatting, did-you-mean
│   ├── import.kn               — @import resolution, cycle detection, namespace merge
│   ├── jit.kn                  — x86-64 JIT compiler
│   ├── std_markscript.kn       — clean embedding API for Kain programs
│   ├── markscript_ui.kn        — UI event binding bridge
│   ├── schema.kn               — config schema validation
│   ├── gen.kn                  — config → code generator
│   └── config.kn               — layered config merging
│
├── std/                        — Markscript stdlib (written in Markscript)
│   ├── intents.md              — data-driven intent keyword registry (57 keywords)
│   ├── io.md
│   ├── math.md
│   ├── build.md
│   ├── test.md
│   └── process.md
│
└── examples/
    ├── game_engine.md
    ├── data_pipeline.md
    ├── servo_controller.md
    ├── compute_pipeline.md
    └── ...
```

---

## Invariants

These hold in every version, including post-1.0 patches.

| # | Invariant |
|---|----------|
| 1 | Markdown cannot produce syntax errors. All structural constructs are always valid |
| 2 | Value semantics throughout the compilation pipeline. No `ptr<T>` parameters in lexer, parser, compiler, or VM interface |
| 3 | Tables are zero-copy embedded in bytecode as contiguous data. No indirection |
| 4 | All data is handle-based. No raw pointers cross the bytecode boundary |
| 5 | Deterministic output — identical `.md` input always produces identical bytecode |
| 6 | The specification document itself is a valid Markscript program |
| 7 | Markscript runs on the same Kain runtime as every other Kain program. No separate process |
| 8 | IVT handlers are Kain functions. No handler logic is implemented in Markscript |

---

## What Markscript Is NOT

| Markscript Is Not | Because |
|------------------|---------|
| A general-purpose scripting language | That is Kain's domain. Markscript orchestrates Kain |
| A replacement for Kain | Markscript has no type system, memory model, ownership, or effects |
| A compiler platform | Cannot express generics, typechecking, or code generation |
| A build system | It dispatches TO Kain's build system through intents |
| A package manager | Dependency management belongs to Kain |
| Extensible through plugins | The IVT IS the extension mechanism — register a Kain function |

---

## Post-1.0 Policy

After 1.0, Markscript enters Long-Term Support.

**Permitted:**
- Bug fixes (incorrect hash, VM crash on valid input, parse failure on valid markdown)
- Security patches (path traversal in `@import`, resource exhaustion)
- Kain compiler compatibility updates (if a Kain regression breaks Markscript compilation)

**Prohibited:**
- New opcodes, token types, or markdown construct semantics
- New intent dispatch patterns
- New language features of any kind
