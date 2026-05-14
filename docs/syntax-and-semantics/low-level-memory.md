# Low-Level Memory And Provenance

Snapshot: April 12, 2026.

This page covers the part of Kain that is closest to ABI reality: raw pointers,
imported pointers, layout-sensitive lowering, and the memory helpers that the
backend ultimately consumes.

## Why This Page Exists

Kain does not treat memory as one generic bucket. The compiler tracks where a
pointer came from, what kind of layout it points at, and which backend rules
apply when that pointer is lowered.

That is why the low-level memory model lives beside the type system, the ABI
policy, and the native helper surface.

## Pointer Provenance

The AST distinguishes pointer provenance explicitly:

| Provenance | Meaning |
| --- | --- |
| `Raw` | A compiler-owned raw pointer with no imported origin tag |
| `ImportedC` | A pointer that came from imported C source |
| `ImportedAsm` | A pointer that came from imported assembly source |
| `LoweredRef` | A reference that was lowered into a raw pointer form |

That distinction matters because imported pointers and lowered references are
not the same thing as an untyped backend-local raw pointer.

## Type Forms That Matter

The important low-level type forms are:

- `&T` and `&mut T`
- `ptr<T>` and `ptr_mut<T>`
- arrays, slices, tuples, options, and results that may contain pointers
- imported host-facing types that preserve provenance through lowering

The `Type::Ptr` node carries the provenance tag directly, and
`Type::contains_raw_ptr()` is how the compiler checks whether a type family
still contains raw-pointer material somewhere inside it.

## Memory Expressions

These expression forms are the core of the memory surface:

| Expression | Purpose |
| --- | --- |
| `AddrOf` | Produce a pointer to an addressable value |
| `Deref` | Read or project through a pointer |
| `PtrOffset` | Perform pointer arithmetic over an element type |
| `MemLoad` | Load from a raw memory address |
| `MemStore` | Store through a raw memory address |
| `SizeOfType` | Ask the layout engine for a type size |
| `AlignOfType` | Ask the layout engine for a type alignment |
| `Alloca` | Reserve stack/local storage |
| `Uninit` | Reserve uninitialized storage |
| `Alloc` | Allocate heap storage |
| `Realloc` | Resize an existing allocation |

These forms are not just syntax. They are the expressions the backend lowers to
helper calls, layout queries, or target-specific memory operations.

## Ownership-State Kernel

`crates/kain-ownership` is the shared semantic home for the future
`collapse`, `observe`, and `decay` memory model.

The current kernel does not add surface syntax by itself. It defines the
portable ownership lattice that parser, typechecker, interpreter, native
runtime, and backend work should consume:

- `Idle` is the only state that can enter exclusive mutation or terminal decay
- `Observed(n)` allows nested read access but rejects collapse and decay
- `Collapsed` represents scoped exclusive mutation and must end before observe or decay
- `Decayed` is terminal and cannot be made live again

The policy table is conservative by design:

- stack/local allocation can map toward readonly/noalias/lifetime-end lowering
- heap allocation can map toward readonly/noalias/free-style lowering
- RC objects can map toward readonly/exclusive-token/release-style lowering
- world and entangle-backed regions observe through snapshots first
- entangled mirrors and imported pointers do not claim ownership powers they cannot prove

Any future syntax for `collapse`, `observe`, or `decay` should attach to this
kernel before claiming LLVM attributes, runtime frees, or entangle/world
concurrency semantics.

## Layout Rules

`crates/kain-core/src/low_level_memory.rs` is the layout engine for Kain
memory lowering. It builds struct layout information by walking typed items,
then uses that registry to decide:

- field offsets
- union layout
- packed vs unpacked layout
- bitfield packing
- size and alignment fallbacks
- how to treat nested modules that contain layout-sensitive types

The ABI policy in `crates/kain-core/src/low_level_abi.rs` supplies the
platform-facing rules for things like:

- LP64 vs LLP64 behavior
- integer promotion width
- pointer width
- bitfield ordering
- packed-struct alignment

## Backend Memory Capabilities

The backend capability table is target-sensitive.

That means some forms are legal in the language core but only lower on targets
that advertise the necessary memory support. The docs should always say which
layer owns the limitation:

1. parser support in `crates/kain-core/src/ast.rs`
2. layout and lowering support in `crates/kain-core/src/low_level_memory.rs`
3. ABI policy in `crates/kain-core/src/low_level_abi.rs`
4. target support in `crates/kain-driver/src/lib.rs`

## Canonical Helper Boundary

When low-level memory turns into backend work, it should lower through the
native helper ABI documented in `guides/native-c-runtime/helper-abi.md`.

The key rule is simple:

- layout-aware work should use the layout registry
- raw loads and stores should use the memory helpers
- imported C or ASM provenance should stay visible until the backend is done

## Source Files To Read Next

- `crates/kain-core/src/ast.rs`
- `crates/kain-core/src/low_level_memory.rs`
- `crates/kain-core/src/low_level_abi.rs`
- `guides/native-c-runtime/helper-abi.md`
- `guides/reference/feature-matrix.md`

## Practical Rule

If you are documenting a pointer or an allocation form, always say:

- whether it is a raw pointer or a lowered/imported pointer
- whether layout comes from the type system, the ABI policy, or target codegen
- which backend family is allowed to consume it
