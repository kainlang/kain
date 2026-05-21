# KainScript and TypeScript

## Two Directions to Keep Straight

There are two different flows that people often blur together:

### Kain -> TypeScript or KainScript

This is normal compilation:

```powershell
kain build src\app.kn --target ts
kain build src\app.kn --target ks
```

### TypeScript or TSX -> Kain

This is import:

```powershell
kain import-ts src\app.ts --output app.kn
kain import-ts src\ui.tsx --output ui.kn
```

Do not conflate these. One emits TypeScript-family artifacts from Kain. The other consumes TypeScript-family source into Kain.

## KainScript (`ks`)

KainScript is a dedicated output target implemented in `M:\Code\Kain\crates\web\src\codegen_ks.rs`.

Think of it as:

- plain ES2022 JavaScript
- with embedded JSDoc types
- designed to run directly
- with enough type metadata for TypeScript-aware tooling

### What makes it different from `ts`

- `ts` emits real TypeScript syntax
- `ks` emits plain JavaScript plus JSDoc
- `ks` does not need a TypeScript compilation step
- `ks` is ideal when you want zero-build scriptability but still want editor type help

### Core `ks` workflow

```powershell
kain build src\app.kn --target ks
node src\app.ks
tsc --checkJs --noEmit src\app.ks
```

### Important generated behavior

Current codegen intentionally emits:

- `// @ts-check`
- an auto-generated stdlib bridge so the script runs in JS runtimes
- an automatic `main()` call if a `main` function exists

This is why `node file.ks` is meant to "just work".

### Best use cases for `ks`

- scripting and automation
- Node/Deno/Bun tools
- quick prototypes where a TS build wall would slow you down
- JS distribution with stronger editor support than plain `js`
- fast smoke output for Kain features that do not need a heavy backend

### When to prefer `ts` instead

Choose `ts` when:

- downstream consumers expect actual `.ts`
- you need the emitted artifact inside a strict TS build pipeline
- you want generated code that uses real TS syntax rather than JSDoc

## TypeScript Import (`import-ts`)

The TypeScript importer lives under `M:\Code\Kain\crates\kain-import\src\typescript`.

The CLI wrapper is:

- `M:\Code\Kain\crates\cli\src\import_typescript.rs`

## Accepted Inputs

Single-file importer code is focused on `.ts` and `.tsx`.

The CLI directory discovery currently includes:

- `.ts`
- `.tsx`
- `.mts`
- `.cts`

By default it skips directories named:

- `_out`
- `_single_out`
- `_batch_out`
- `dist`
- `build`
- `node_modules`

It also skips filenames containing `.generated.`.

## Supported Transform Patterns

Current high-value mappings include:

- `interface` -> `struct`
- `type` alias -> `type` alias
- `enum` -> `enum`
- `class` -> `struct` plus `impl`
- `function` -> `fn`
- some component-like functions -> `component` when the transformer recognizes them
- `async function` -> function with `Effect::Async`
- `Array<T>` and `ReadonlyArray<T>` -> slice-like Kain type
- `Promise<T>` -> `Impl { trait_name: "Async", ... }`
- `number` -> `Float`
- `string` -> `String`
- `boolean` -> `Bool`
- `bigint` -> `Int`
- `void` -> `Unit`
- `never` -> `Never`
- `null | undefined | T` style cases -> `Option<T>`

## Important Current Limitations

These are real current behaviors from the source, not just historical docs:

- TypeScript `import` declarations are skipped because module resolution is not implemented yet.
- named export lists are skipped
- default export expressions are skipped
- interface `extends` clauses are only noted, not modeled directly
- computed interface properties and methods are skipped
- computed class members may be skipped
- complex union and intersection types often degrade to `Type::Infer`
- TSX parses, but unsupported JSX forms may fall back or be skipped rather than perfectly preserved

The union and intersection point is especially important. Older docs may read as if TypeScript unions map richly across the board. The current type mapper is more conservative:

- trivial same-type unions collapse cleanly
- nullish unions become `Option<T>`
- many non-trivial unions and intersections become `Infer`

## Practical `import-ts` Commands

### Single file

```powershell
kain import-ts src\main.ts --output main.kn
kain import-ts src\main.tsx --output main.kn --target ks
```

### Directory import

```powershell
kain import-ts src --output app.kn
kain import-ts src --output app.kn --flat
```

### Filtered directory import

```powershell
kain import-ts src --output ui.kn --include components --exclude test
```

### Failure reporting

```powershell
kain import-ts src --output app.kn --report-json app.import_report.json
```

### Fail fast

```powershell
kain import-ts src --output app.kn --fail-fast
```

## What the CLI Emits

When import succeeds, the CLI can:

- write a `.kn` file
- optionally compile it immediately to a target
- summarize counts for functions, structs, enums, impls, and type aliases
- emit failure-report JSON for directory imports

Default directory behavior:

- recursively discovers TypeScript-family files
- imports each file independently
- wraps each imported file in a Kain `mod <name>:` unless `--flat` is used

## Best TypeScript Import Use Cases

1. Partial migration of a TS or TSX codebase into Kain
2. Converting TS models and utility layers into Kain AST quickly
3. Pulling React-like TSX component code into Kain for manual cleanup and re-authoring
4. TS -> Kain -> `ks`, `ts`, `js`, or `wasm` experiments
5. Large-codebase triage using include/exclude filters and failure-report JSON

## Recommended Workflow

1. Start with a single file or a filtered subset.
2. Write a real `.kn` output file and inspect it.
3. For directories, always capture or inspect the failure report if anything was skipped or failed.
4. Fix the important semantic gaps in the generated Kain code.
5. Only then push the result through heavier targets.

## Sharp Edges to Remember

- `import-ts` is not a TS bundler or module resolver.
- Cross-file TypeScript imports are not currently resolved the way a TS compiler would.
- If you need structural preservation across many files, keep modular import mode instead of `--flat`.
- If a TS type looks too rich to map directly, expect `Infer` and plan manual cleanup.
