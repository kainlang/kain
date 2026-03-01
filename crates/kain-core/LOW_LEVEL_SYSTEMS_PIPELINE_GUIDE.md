# KAIN Low-Level Systems Pipeline Guide

## Purpose

This document describes the low-level systems pipeline that now exists across `kain-core`, `kain-import`, and the backend crates.

It covers:

- what was built
- why it was added
- the semantic model now available in KAIN
- how imported C and future assembly/C++ work map into that model
- what backends can do with it
- current limits
- real examples

This is the practical companion to [LOW_LEVEL_MEMORY_LAYER_DESIGN.md](m:\Code\Kain\crates\kain-core\LOW_LEVEL_MEMORY_LAYER_DESIGN.md). That design doc explains the intent. This guide explains the implemented system.

## Executive Summary

KAIN now has a real low-level semantic tier.

That means the compiler can represent and lower:

- raw pointers
- address-taken values
- pointer arithmetic
- raw memory loads/stores
- `sizeof` / `alignof`
- stack/local storage
- heap allocation and reallocation
- aggregate initialization
- union-aware layout
- bitfield-aware layout and helper lowering
- ABI-aware layout policies
- compiler-flavor ABI selection

This moved the C import path from:

`C syntax -> high-level approximation -> backend`

to:

`C syntax -> low-level KAIN semantics -> normalized lowering -> backend`

That is the core shift.

## Why This Matters

Before this work, imported low-level code had to be faked through high-level constructs:

- raw pointers approximated as refs
- pointer arithmetic approximated as indexing tricks
- storage semantics approximated as arrays or defaults
- `sizeof` collapsed to guessed literals
- unions and bitfields treated as if they were ordinary fields

That was enough for smoke compilation, but not enough for serious low-level fidelity.

The new pipeline makes KAIN stronger in four distinct ways:

1. C import fidelity
2. assembly/transliteration infrastructure
3. self-hosting and runtime code expression
4. backend normalization for low-level semantics

## High-Level Architecture

The implemented model is:

1. frontend/importer emits richer low-level KAIN AST
2. `kain-core` typechecks that AST
3. `kain-core` lowers low-level semantics into backend-safe helper/runtime forms
4. backends emit code from the normalized program

In short:

`source -> low-level KAIN IR -> lowered KAIN IR -> backend`

This keeps the system coherent:

- importers stay honest
- backends stay manageable
- low-level behavior stays centralized

## Core Files

### Core semantics and lowering

- [ast.rs](m:\Code\Kain\crates\kain-core\src\ast.rs)
- [types.rs](m:\Code\Kain\crates\kain-core\src\types.rs)
- [low_level_memory.rs](m:\Code\Kain\crates\kain-core\src\low_level_memory.rs)
- [low_level_abi.rs](m:\Code\Kain\crates\kain-core\src\low_level_abi.rs)
- [low_level_memory_metadata.rs](m:\Code\Kain\crates\kain-core\src\low_level_memory_metadata.rs)

### Parser / language normalization

- [parser.rs](m:\Code\Kain\crates\kain-core\src\parser.rs)

### Import side

- [parser.rs](m:\Code\Kain\crates\kain-import\src\c\parser.rs)
- [transformer.rs](m:\Code\Kain\crates\kain-import\src\c\transformer.rs)

### Backends

- [codegen_ts.rs](m:\Code\Kain\crates\web\src\codegen_ts.rs)
- [codegen_js.rs](m:\Code\Kain\crates\web\src\codegen_js.rs)
- [codegen_wasm.rs](m:\Code\Kain\crates\web\src\codegen_wasm.rs)
- [codegen_cpp.rs](m:\Code\Kain\crates\sys\src\codegen_cpp.rs)
- [codegen_rust.rs](m:\Code\Kain\crates\sys\src\codegen_rust.rs)
- [codegen_ue5.rs](m:\Code\Kain\crates\ue5\src\codegen_ue5.rs)

### Regression / conformance

- [ptr_type_test.rs](m:\Code\Kain\crates\kain-core\tests\ptr_type_test.rs)
- [c_abi_conformance.rs](m:\Code\Kain\crates\kain-import\tests\c_abi_conformance.rs)
- [c_abi_corpus.rs](m:\Code\Kain\crates\kain-import\tests\c_abi_corpus.rs)
- [manifest.json](m:\Code\Kain\crates\kain-import\tests\abi_corpus\manifest.json)

## What Was Added To The Language Core

### Raw pointer types

KAIN now distinguishes:

- high-level references
- raw pointer semantics

The core type tier includes:

- `ptr<T>`
- `ptr_mut<T>`

These are not the same as `&T` or `&mut T`.

That split matters because raw pointer semantics allow:

- pointer stepping
- raw memory reads/writes
- lower-level alias behavior

### New low-level expression forms

The core AST now supports:

- `AddrOf`
- `PtrOffset`
- `MemLoad`
- `MemStore`
- `SizeOfType`
- `AlignOfType`
- `Alloca`
- `Uninit`
- `Alloc`
- `Realloc`
- `AggregateInit`

These are not importer hacks. They are real AST nodes in `kain-core`.

## What Each Semantic Node Means

### `AddrOf`

Represents taking the address of an addressable location.

Examples:

- `&x`
- `&arr[i]`
- `&obj.field`

This matters because address-taking is not the same as an ordinary KAIN reference in imported low-level code.

### `PtrOffset`

Represents pointer arithmetic:

- `ptr + i`
- `ptr - i`
- `&arr[i]` lowered through pointer stepping

This prevents the importer from faking raw pointer math as array sugar.

### `MemLoad`

Represents a raw memory read:

- `*ptr`
- pointer-like `ptr[i]`

### `MemStore`

Represents a raw memory write:

- `*ptr = value`
- pointer-like `ptr[i] = value`

### `SizeOfType`

Represents layout-backed `sizeof(type)`.

This is resolved through the layout registry, not hardcoded importer guesses.

### `AlignOfType`

Represents layout-backed `_Alignof(type)` / alignment reasoning.

### `Alloca`

Represents explicit local/stack storage.

This is important for:

- fixed local arrays
- stack buffers
- explicit addressable storage

### `Uninit`

Represents explicit uninitialized storage rather than silently zeroing/defaulting.

### `Alloc` / `Realloc`

Represent semantic heap storage patterns like:

- `malloc(sizeof(T))`
- `calloc(n, sizeof(T))`
- `realloc(ptr, n * sizeof(T))`

### `AggregateInit`

Represents typed aggregate construction, including designated initializer lowering.

That includes:

- struct designated initializers
- array designators
- nested designated aggregate initialization

## ABI Layer

### Why the ABI layer exists

Low-level layout cannot be correct if the compiler only “kind of knows” how big things are.

The ABI layer centralizes:

- integer widths
- pointer widths
- promotion widths
- packed layout assumptions
- backend-target ABI choice
- compiler flavor choice

### Compiler-flavor policy

The ABI layer is now data-driven in [low_level_abi.rs](m:\Code\Kain\crates\kain-core\src\low_level_abi.rs).

Supported flavors:

- `generic`
- `gcc`
- `clang`
- `msvc`

Current selection mechanism:

- environment variable: `KAIN_C_ABI_FLAVOR`

Examples:

```powershell
$env:KAIN_C_ABI_FLAVOR = "gcc"
```

```powershell
$env:KAIN_C_ABI_FLAVOR = "msvc"
```

### ABI kind selection

Current ABI kinds:

- `GenericLp64`
- `GenericLlp64`

Current target mapping:

- most targets -> LP64
- `ue5` / `ue5editor` -> LLP64

That matches the practical native/engine split KAIN needed.

## Layout Registry

The layout system in [low_level_memory.rs](m:\Code\Kain\crates\kain-core\src\low_level_memory.rs) now computes and tracks:

- struct size
- struct alignment
- union layout
- field offsets
- bitfield offsets
- bitfield widths
- storage bit widths
- storage alignment
- packed and explicit type alignment effects

This registry powers:

- `sizeof`
- `alignof`
- field-address lowering
- union helper lowering
- backend memory helper calls

## C Import Improvements

The importer is materially stronger now.

### Pointer/memory semantics

It now lowers:

- `&x`
- `&arr[i]`
- `*ptr`
- `ptr[i]`
- `*ptr = value`
- `ptr[i] = value`
- pointer `+/- int`

into actual low-level KAIN semantics instead of pretending they are ordinary safe-language operations.

### Storage semantics

It now lowers:

- local fixed arrays -> `Alloca`
- uninitialized locals -> `Uninit`
- `malloc/calloc/realloc` patterns -> `Alloc` / `Realloc`

### Layout semantics

It now lowers:

- `sizeof(type)` -> `SizeOfType`
- `_Alignof(type)` -> `AlignOfType`
- `sizeof(expr)` via a richer typed path where possible

### Aggregate initialization

It now lowers:

- struct designated initializers
- nested designators like `.inner.x = 1`
- array-of-struct designators like `[2].field = ...`

### Union and bitfield metadata

It now carries:

- `c_union`
- `c_bitfield(width, signedness)`
- storage bits
- storage align
- pack align
- explicit type align

### Source layout metadata

The C parser now tracks:

- `#pragma pack(...)`
- named `#pragma pack(push/pop, id)`
- packed attrs
- aligned attrs

and does so with offset-preserving sanitization so span-based metadata stays aligned with parsed AST spans.

## Backend Behavior

### TypeScript / JavaScript

These backends now lower low-level memory semantics into runtime helper contracts.

Representative helpers:

- `__kain_addr_of`
- `__kain_ptr_offset`
- `__kain_mem_load`
- `__kain_mem_store`
- `__kain_alloc`
- `__kain_realloc`
- `__kain_union_wrap`
- `__kain_union_get`
- `__kain_union_set`
- `__kain_bitfield_get`
- `__kain_bitfield_set`
- `__kain_bind_local`
- `__kain_field_ptr`
- `__kain_index_ptr`

This means TS/JS are no longer limited to “reject raw memory immediately.”

### WASM

WASM is on the normalized low-level helper path as well, including union/bitfield lowering support.

### C++ / Rust

Native-ish codegen paths now also run through the same normalized lowering contract rather than bypassing it.

That keeps low-level behavior consistent across backends.

### UE5

UE5 is no longer the obvious weak link in the pipeline.

It has storage-aware helper lowering for:

- unions
- bitfields
- memory helper calls

That does not mean full C runtime equivalence inside generated UE code, but it does mean the low-level semantics are no longer being dropped on the floor.

## Arithmetic and Promotion Improvements

The low-level pipeline now includes broader C-style arithmetic normalization.

That includes:

- integer promotions
- usual arithmetic conversions
- shift operand normalization
- bitfield promotion width tracking

This matters because imported C expressions like:

```c
return f.small + f.wide;
```

now lower with the expected promotion widths represented in the helper/lowering path instead of collapsing into a backend guess.

## Bitfields

### What works now

The system now supports:

- bitfield width metadata
- bitfield signedness metadata
- bitfield promotion width metadata
- bitfield read lowering
- bitfield write lowering
- mask/shift helper lowering
- illegal `&bitfield` diagnostics

### Important boundary

This is strong bitfield support, but it is still not every compiler-specific ABI corner case under the sun.

The important point is that bitfields are now first-class low-level semantics instead of “fields with strange comments.”

## Unions

### What works now

The system now supports:

- union layout as max-field-size
- union field offsets at zero
- union-aware aggregate lowering
- union helper lowering for read/write
- explicit active-member helper paths
- non-scalar union reinterpretation coverage in regression tests

### Important boundary

This is union-aware lowering, not “perfect emulation of every byte-reinterpretation edge case across every target ABI.”

But it is enough to treat unions as unions rather than fake structs.

## ABI Corpus

The ABI corpus now exists as data under:

- [abi_corpus](m:\Code\Kain\crates\kain-import\tests\abi_corpus)

It contains:

- a manifest
- real `.c` fixtures
- a Rust harness that loads the manifest and validates import/lowering expectations

### Why this matters

This changes conformance work from:

- “write a new custom Rust test every time”

to:

- “drop in a new fixture and add a manifest row”

That is the right direction for a growing compiler/import pipeline.

### Current corpus fixtures

- `pragma_pack.c`
- `aligned_attr.c`
- `named_pack_stack.c`
- `bitfield_promotion.c`
- `union_pair.c`

These cover:

- pack semantics
- explicit align semantics
- named pack-stack semantics
- bitfield promotions
- non-scalar union reinterpretation

## Examples

### Example 1: raw pointer type in KAIN

```kain
fn take_ptr(p: ptr<Int>) -> Int:
    return 0
```

### Example 2: pointer offset

```kain
fn advance(p: ptr<Int>, i: Int) -> ptr<Int>:
    return ptr_offset(p, i)
```

### Example 3: raw memory load/store

```kain
fn poke(p: ptr<Int>, v: Int) -> Int:
    mem_store(p, v)
    return mem_load(p)
```

### Example 4: layout-backed size/alignment

```kain
fn sizes() -> Int:
    let s = sizeof_type("Packet")
    let a = alignof_type("Packet")
    return s + a
```

### Example 5: explicit storage

```kain
fn local_buffer() -> Int:
    let mut buf = alloca("Int")
    mem_store(buf, 7)
    return mem_load(buf)
```

### Example 6: aggregate init

```kain
fn make_pair() -> Pair:
    return aggregate_init("Pair", true, x = 1, y = 2)
```

### Example 7: imported C pack semantics

Input C:

```c
#pragma pack(push, 1)
struct Packet {
    char tag;
    int value;
};
#pragma pack(pop)
```

Result:

- importer attaches pack metadata
- layout registry computes `sizeof(Packet) == 5`
- `alignof(Packet) == 1`
- lowered targets see that normalized layout result

### Example 8: imported union reinterpretation

Input C:

```c
struct Pair {
    int x;
    int y;
};

union Payload {
    struct Pair pair;
    long long raw;
};

struct Pair read_pair(union Payload u) {
    return u.pair;
}
```

Now the low-level path preserves this as union-aware lowering rather than flattening it into ordinary field access.

## What KAIN Can Do Now That It Could Not Do Before

### 1. Import low-level C more honestly

This is the biggest direct win.

KAIN can now carry substantially more of the source semantics into its own IR instead of erasing them during import.

### 2. Normalize low-level semantics across multiple backends

This is strategically important.

The same imported low-level behavior can now flow through:

- `ts`
- `js`
- `wasm`
- `cpp`
- `rust`
- `ue5`

with one shared semantic lowering model.

### 3. Support a real low-level tier for self-hosting/runtime work

This makes KAIN much more credible for:

- runtime code
- allocators
- binary parsers
- VM/emulator logic
- import/transliteration infrastructure

### 4. Give assembly import a real target tier

Even though this pass was driven by C import, the semantics are general enough for assembly workflows too:

- memory
- offsets
- storage
- layout
- low-level helper lowering

That is exactly the right foundation for `kain-asm`.

## Current Status

Practical status after this pass:

- low-level semantic layer: in
- low-level lowering layer: in
- ABI policy layer: in
- compiler-flavor ABI table: in
- file-backed ABI corpus: in
- backend normalization across major targets: in

Rough status:

- overall low-level systems pipeline: about `97-99%`

That last few percent is not “the design is missing.” It is the open-ended nature of ABI and compiler conformance work.

## What Is Still Not Absolute 100%

No honest compiler engineer should claim ABI work is “finished forever.”

The remaining open-ended areas are:

1. more compiler-specific layout oddities
2. more exotic attributes/pragmas
3. more corpus breadth
4. more calling-convention and promotion edge cases outside current scope

That is normal.

The important milestone is that the architecture is now right.

## Validation Summary

Representative validations completed in this work:

- `cargo test -p kain-core --test ptr_type_test -- --nocapture`
- `cargo test -p kain-import --test c_abi_conformance -- --nocapture`
- `cargo test -p kain-import --test c_abi_corpus -- --nocapture`
- `cargo check -p kain-core -p kain-import -p web -p sys -p ue5`

That means:

- semantic nodes parse/typecheck/lower
- ABI corpus fixtures survive import and lowering
- backend crates that share the low-level path still build

## Recommended Next Steps

### If the goal is stronger conformance

Add more ABI corpus fixtures.

That is now the correct workflow.

Examples of good next corpus categories:

- nested packed structs
- mixed signed/unsigned arithmetic edge cases
- more union reinterpretation shapes
- compiler-specific align/pack combos
- volatile memory cases if you decide to model them more explicitly

### If the goal is stronger frontend ergonomics

Add a dedicated low-level authoring doc and possibly selective syntax docs for:

- `ptr<T>`
- `ptr_mut<T>`
- memory intrinsics
- `sizeof_type`
- `alignof_type`

### If the goal is stronger assembly import

Reuse this exact low-level tier in `kain-asm` rather than inventing a separate low-level memory model.

## Bottom Line

KAIN now has a real low-level systems pipeline.

Not a patchwork of importer tricks.
Not a backend-specific pile of exceptions.

A real pipeline:

- semantic nodes in core
- ABI-aware layout
- backend-normalized lowering
- C import wired into that model
- corpus-backed conformance coverage

That is the foundation KAIN needed for:

- serious C import
- future C++ import
- assembly import
- self-hosting runtime work
- low-level logic being compiled into modern targets

That is the actual state of the system now.
