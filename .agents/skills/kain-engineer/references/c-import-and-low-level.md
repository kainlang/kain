# C Import and Low-Level Guidance

## What `import-c` Is For

The C importer is one of Kain's highest-value workflows:

- SDK ingestion
- firmware and embedded code translation
- game decompilation and transliteration
- large native codebase exploration
- turning C into Kain IR so it can flow into multiple backends

This is not a text converter. It parses C, lowers it into Kain AST, emits `.kn`, and can optionally compile the result further.

## Current State

Current repo docs describe the C importer as production-grade. That is directionally true for many real workflows, but do not oversell it as "perfect semantic parity with all of C".

Best current description:

- very strong for broad C11-style transliteration
- very useful for large exploratory imports
- good enough to feed meaningful downstream Kain backend tests
- still not identical to full build-system-accurate C semantics in every edge case

## Canonical Commands

### Single file

```powershell
kain import-c src\main.c --output main.kn
```

### Single file plus direct compile

```powershell
kain import-c src\main.c --output main.kn --target ts
```

### Directory import

```powershell
kain import-c src --output game.kn
```

### Flat merge mode

```powershell
kain import-c src --output game.kn --flat
```

### Include paths and defines

```powershell
kain import-c src\main.c --output main.kn -I include -I third_party -D DEBUG -D VERSION=3
```

### Filters and failure reporting

```powershell
kain import-c src --output game.kn --include core --exclude sound --report-json game.import_report.json
```

### Stop on first failure

```powershell
kain import-c src --output game.kn --fail-fast
```

## Default Directory Behavior

When importing a directory, the CLI:

- recursively discovers `.c` files
- imports each file independently
- wraps each imported file in `mod <name>:` by default
- merges them into one output program

Use `--flat` only if you really want every imported symbol merged into one top-level scope.

## C Constructs That Map Well Today

Current pipeline and docs indicate support for:

- structs
- unions
- enums
- typedefs
- pointers
- arrays
- function pointers
- bitfields
- `#pragma pack`
- packed and aligned attributes
- address-of and dereference
- pointer offset expressions
- local fixed arrays
- `malloc`, `calloc`, `realloc`
- designated initializers
- aggregate initialization

The importer also uses data-driven registries for many C type and operator mappings. If you add support, prefer extending those registries rather than embedding ad hoc logic in multiple places.

## Low-Level and Backend Reality

Imported C often drags low-level memory behavior with it. That matters because backend support is not uniform.

Native-oriented targets are the safest fit for serious low-level C-derived code:

- `rust`
- `cpp`
- `llvm`
- `ue5`

Cheaper targets such as `ts`, `js`, `ks`, and `wasm` are still useful, but use them carefully:

- they are good smoke targets for generated Kain structure
- they are not the best final target for pointer-heavy, ABI-sensitive logic
- low-level memory diagnostics may reject constructs that native-oriented targets can model better

This is why "compile to `ts` first" is still a good practical workflow for importer validation, even though not every low-level pattern is equally portable to web targets.

## Recommended Workflow

For serious C imports:

1. Import to a real `.kn` file.
2. Inspect the emitted Kain for module boundaries, layout attributes, and obvious identifier cleanup.
3. Capture failure-report JSON for any directory import with partial failures.
4. Smoke compile to a cheap target such as `ts` or `ks` when appropriate.
5. Move to `rust`, `cpp`, `llvm`, or `ue5` once the imported Kain shape is healthy.

## Known Limits to Be Honest About

Still treat these as active hard areas:

- pointer arithmetic edge cases
- aliasing-sensitive behavior
- exact storage duration and lifetime fidelity
- full build-system and preprocessor emulation
- unusual macro-heavy codebases
- very large legacy trees that require staged import

If a user asks for "full fidelity C import", do not imply it is solved everywhere. Be concrete about what works and what remains approximate.

## Where to Edit and Validate

Important paths:

- `M:\Code\Kain\crates\cli\src\import_c.rs`
- `M:\Code\Kain\crates\kain-import\src\c\parser.rs`
- `M:\Code\Kain\crates\kain-import\src\c\transformer.rs`
- `M:\Code\Kain\crates\kain-import\src\common\c_registry.rs`
- `M:\Code\Kain\crates\kain-import\C_IMPORT_PIPELINE.md`

If you modify importer semantics, also inspect and extend:

- `M:\Code\Kain\crates\kain-import\tests\abi_corpus\`
- `M:\Code\Kain\crates\kain-import\tests\c_abi_conformance.rs`
- `M:\Code\Kain\crates\kain-import\tests\c_abi_corpus.rs`

## Practical Agent Guidance

- Preserve modular import mode unless flattening is clearly the better outcome.
- Reach for include paths and defines before assuming the parser is wrong.
- If many files fail, classify the failures before patching random cases.
- If the target codebase is massive, stage it with filters and reports instead of importing everything at once and hoping for the best.
