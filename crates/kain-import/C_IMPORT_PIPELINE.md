# C Import Pipeline

## Purpose

The C import pipeline turns C source into KAIN AST and then into generated `.kn` source that can be compiled through KAIN backends such as `ts`, `js`, `wasm`, `rust`, `cpp`, and `ue5`.

This is not a text converter. The pipeline parses C, lowers it into KAIN structures, then serializes valid KAIN source.

## Pipeline Stages

1. Source discovery
   - Accepts a single `.c` file or a directory of `.c` files.
   - Directory import can recurse and optionally flatten all output into one scope.
2. Preprocessor configuration
   - Supports `-I` include paths and `-D` defines passed through the CLI.
   - This helps with normal C project layouts, but it is not a full replacement for a project-specific build system.
3. C parse
   - Uses `lang-c` to parse C into a C AST.
4. AST transform
   - Lowers C declarations, expressions, statements, structs, enums, arrays, refs, derefs, and control flow into KAIN AST.
5. KAIN source emission
   - Emits `.kn` source from the lowered KAIN AST.
6. Optional backend compile
   - If `--target` is supplied, the generated KAIN is compiled immediately into the requested backend artifact.

## CLI Usage

The primary workflow is through `kain.exe`.

### Single file

```bash
kain.exe import-c .\runtime.c --output .\runtime.kn
```

### Single file with direct backend compile

```bash
kain.exe import-c .\runtime.c --output .\runtime.kn --target ts
```

This writes:

- `runtime.kn`
- `runtime.ts`

If `--output` is omitted and `--target` is present, the compiled artifact is still written next to the input file using the target extension.

### Directory import

```bash
kain.exe import-c .\src --output .\game.kn
```

Default directory behavior:

- recursively finds `.c` files
- imports each file independently
- wraps each imported file in a KAIN `mod <name>:` block
- merges them into one output program

### Flat merge mode

```bash
kain.exe import-c .\src --output .\game.kn --flat
```

`--flat` disables per-file module wrapping and merges all imported symbols into a single top-level scope.

### Include and exclude filters

```bash
kain.exe import-c .\src --output .\game.kn --include game --exclude sound
```

Filters match on normalized relative paths. This is useful for large source trees where you want a subset first.

### Include paths and defines

```bash
kain.exe import-c .\src\main.c --output .\main.kn -I .\include -I .\third_party -D DEBUG -D VERSION=3
```

### Failure reporting

```bash
kain.exe import-c .\src --output .\game.kn --report-json .\game.import_report.json
```

For directory imports:

- if some files fail and others succeed, import still continues by default
- the JSON report records discovered/imported/skipped/failed counts
- if `--report-json` is omitted, a default report is written automatically when directory import has failures

### Fail fast

```bash
kain.exe import-c .\src --output .\game.kn --fail-fast
```

This stops on the first failed file instead of continuing.

## Development Workflow

If you are working inside the Rust workspace instead of using the installed binary:

```bash
cargo run -p cli -- import-c .\runtime.c --output .\runtime.kn
```

## Output Model

The importer emits KAIN source, not just AST in memory.

For a single file:

- input: `runtime.c`
- output: `runtime.kn`
- optional compiled output: `runtime.ts`, `runtime.cpp`, `runtime.js`, `runtime.h`, etc. depending on target

For a directory:

- input: `src\`
- output: one merged `.kn` file
- optional JSON import report

## What Works Well Right Now

These areas are materially functional today:

- single-file C import to `.kn`
- directory import into one merged `.kn`
- per-file module wrapping and flat merge mode
- failure-report JSON for partial imports
- identifier sanitization for reserved KAIN keywords
- address-of and dereference lowering
- fixed-size local array lowering
- sequence-correct lowering for `++` and `--` in common statement contexts
- anonymous `typedef struct { ... } Name;` recovery into named KAIN structs
- generated KAIN compiling to downstream backends, including large outputs such as `ts` and `ue5`

## Current Readiness

The importer is past the "prototype only" stage, but it is not yet full-semantic-parity C.

A practical way to describe it:

- good for substantial C subsets
- good for large exploratory imports and transliteration workflows
- good enough to push imported code through KAIN backends for inspection and smoke compilation
- not yet strong enough to claim full self-hosting-grade C semantics

## Known Limits

The current limits are structural, not cosmetic.

### Memory model fidelity is still incomplete

Remaining hard problems include:

- pointer arithmetic edge cases
- aliasing-sensitive behavior
- precise storage-duration and lifetime modeling in all cases
- full `sizeof` and layout fidelity across every imported construct

### Preprocessor/build-system fidelity is incomplete

The importer supports defines and include paths, but not full build-system emulation.

Typical gaps:

- project-specific generated includes
- unusual macro-heavy pipelines
- nonstandard decompilation include conventions
- configuration that depends on external build scripts

### Large legacy codebases still need staged import

For codebases like SM64-style decomp trees, the correct workflow is:

1. import a subset
2. inspect failure report JSON
3. fix systemic importer gaps
4. re-import

That is the intended path. The goal is not a one-off hack for one codebase.

## What The Current State Means

If a large imported C aggregate compiles through KAIN into `ts` or `ue5`, that is meaningful. It proves:

- parser and lowering are broadly working
- generated KAIN is increasingly valid
- backend integration is usable on imported code

It does not, by itself, prove perfect semantic equivalence with the original C program.

The real success bar is:

1. source ingestion works broadly
2. generated KAIN parses and typechecks
3. backend artifacts generate cleanly
4. runtime behavior remains faithful enough for the target class of code

The pipeline is partway through step 4.

## Recommended Usage Pattern

For serious projects, use this order:

1. Import to `.kn`
2. Compile imported `.kn` to `ts` first
3. Read the failure-report JSON for directory imports
4. Only then push to `ue5` or other heavier backends

Example:

```bash
kain.exe import-c .\runtime\kain_runtime_clean.c --output .\clean.kn
kain.exe -t ts .\clean.kn -o .\clean.ts
```

## Current Validation Snapshot

Representative validations already exercised on this pipeline:

- self-hosting runtime sample imports to KAIN and compiles to TypeScript
- large aggregate C imports can be emitted as `.kn`
- imported mega-files can be pushed through `ts` and `ue5` codegen after importer/compiler fixes

That means the system is useful now, but still in an active semantics-hardening phase.

## File Map

Main implementation surfaces:

- `src/c/parser.rs`
- `src/c/transformer.rs`
- `src/common/identifier_registry.rs`
- `../cli/src/import_c.rs`

## Near-Term Priorities

The next high-value work is:

1. richer pointer arithmetic lowering
2. better storage/layout fidelity
3. stronger preprocessor/project import handling
4. more importer diagnostics by failure class
5. more self-hosting and large-codebase regression tests
