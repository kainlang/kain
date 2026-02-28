# KAIN Low-Level Memory Layer Design

## Purpose

This document proposes a low-level memory layer for `kain-core` that improves semantic fidelity for:

- C import
- future C++ import
- assembly import / transliteration
- self-hosting runtime code
- binary parsing, serializers, allocators, VM cores, and engine interop

The goal is not to turn all of KAIN into C. The goal is to give KAIN a principled low-level tier that can be lowered into existing backends without forcing every backend to become a raw systems compiler.

## Why This Fits KAIN

KAIN already has the right foundation:

- first-class refs/derefs
- arrays and slices
- effect tracking with `Unsafe`
- assembly import infrastructure
- a clear parser -> typecheck -> monomorphize -> backend pipeline
- multiple targets with different capability levels

From the existing feature docs:

- refs/derefs are already core language features
- assembly import is already a first-class workflow
- LLVM/native targets already care about memory layout and ABI edges

The missing piece is that imported low-level code currently gets approximated through `&T`, arrays, casts, and identity-mode deref/runtime behavior. That is enough for transliteration and smoke compilation, but not enough for faithful low-level semantics.

## Design Goals

1. Preserve current high-level KAIN ergonomics.
2. Add low-level memory semantics as a first-class internal model.
3. Keep backend complexity bounded through normalization/lowering.
4. Avoid one-off importer hacks and file-specific rewrites.
5. Drive behavior from data tables and capability profiles, not ad hoc branching.
6. Preserve backward compatibility for existing KAIN code where possible.

## Non-Goals

1. Do not expose every internal low-level primitive as surface syntax immediately.
2. Do not make TS/JS/UE5 codegen understand raw pointer aliasing directly.
3. Do not merge C semantics into existing `&T` reference semantics unless they are truly equivalent.

## Current Problem

Today, imported low-level code is forced into existing high-level shapes:

- raw C pointers become `&T` / `&mut T`
- pointer arithmetic gets approximated as ref-index patterns
- deref/runtime behavior is treated as identity in non-native execution
- stack/raw storage often becomes arrays or default-initialized placeholders
- `sizeof` and layout reasoning are estimated, not modeled

That creates three classes of failure:

1. semantic drift
   - code compiles but does not mean the same thing
2. importer complexity
   - more and more bespoke lowering rules
3. backend leakage
   - importer changes get constrained by backend quirks instead of source semantics

## Core Approach

Add a low-level memory layer in `kain-core` as an internal semantic tier.

Recommended pipeline:

1. Source importer or low-level frontend emits extended KAIN IR.
2. A normalization/lowering pass transforms that IR into backend-safe KAIN IR.
3. Existing backends continue targeting normalized IR.

In short:

`source -> rich low-level KAIN IR -> lowered KAIN IR -> backend codegen`

This preserves:

- clean importer semantics
- stable backend contracts
- room for future low-level targets

## Data-Driven Requirement

This feature should be implemented data-first, not hardcoded-first.

The following must be represented as declarative data:

1. backend capability matrix
2. memory lowering policy per backend
3. layout/alignment policy
4. importer-to-core semantic mapping tables
5. unsafe-feature enablement policy

Recommended configuration surfaces:

- `LowLevelCapabilities`
- `MemoryLoweringPolicy`
- `LayoutPolicy`
- `PointerModel`

These should live in `kain-core` as typed structs/enums, not scattered booleans across importers and codegen crates.

## Proposed Semantic Additions

### 1. New Type Tier

Add low-level pointer/view types distinct from high-level references.

Recommended additions:

```rust
Type::Ptr {
    mutable: bool,
    inner: Box<Type>,
    provenance: PointerProvenance,
    span: Span,
}

Type::Buffer {
    element: Box<Type>,
    layout: BufferLayout,
    span: Span,
}

Type::OpaqueBytes {
    size: Option<usize>,
    alignment: Option<usize>,
    span: Span,
}
```

Rationale:

- `&T` stays a high-level borrow/reference
- `Ptr<T>` becomes raw-address semantics
- `Buffer<T>` represents addressable contiguous storage
- `OpaqueBytes` covers untyped raw memory, packed storage, and importer fallback cases

### 2. New Expr Tier

Recommended additions:

```rust
Expr::AddrOf {
    value: Box<Expr>,
    span: Span,
}

Expr::PtrOffset {
    pointer: Box<Expr>,
    offset: Box<Expr>,
    element_ty: Type,
    span: Span,
}

Expr::MemLoad {
    pointer: Box<Expr>,
    ty: Type,
    volatile: bool,
    span: Span,
}

Expr::MemStore {
    pointer: Box<Expr>,
    value: Box<Expr>,
    volatile: bool,
    span: Span,
}

Expr::SizeOfType {
    target: Type,
    span: Span,
}

Expr::AlignOfType {
    target: Type,
    span: Span,
}
```

Rationale:

- explicit semantics beat importer-side illusions
- assembly and C import can share these nodes
- backends can lower them systematically

### 3. Storage Model

Recommended additions:

```rust
Expr::Alloca {
    ty: Type,
    count: Option<Box<Expr>>,
    init: MemoryInitPolicy,
    span: Span,
}

Expr::Alloc {
    ty: Type,
    count: Option<Box<Expr>>,
    init: MemoryInitPolicy,
    span: Span,
}
```

With:

```rust
enum MemoryInitPolicy {
    Uninitialized,
    Zeroed,
    DefaultValue,
}
```

Rationale:

- distinguishes C-style raw storage from normal initialized KAIN values
- avoids pretending every imported stack buffer is a regular array literal

## Surface Syntax Strategy

Do not rush surface syntax.

Recommended phases:

### Phase A

Internal AST/type additions only.

- importer can construct nodes directly
- parser unchanged except where already needed

### Phase B

Selective syntax exposure for low-level authoring.

Possible future syntax:

```kain
let p: ptr<Int>
let x = load(p)
store(p, 42)
let q = ptr_offset(p, i)
let n = sizeof(Int)
```

This should only happen after the internal semantics and lowering passes are stable.

## Type System Rules

### Reference vs Pointer Split

High-level references:

- `&T`
- `&mut T`
- borrow-style semantics
- safe where possible

Low-level pointers:

- `ptr<T>`
- `ptr_mut<T>`
- raw address semantics
- arithmetic allowed
- deref/load/store explicitly unsafe or capability-gated

### Safety Rules

All raw pointer operations should require explicit low-level capability and likely `Unsafe`.

Examples:

- `PtrOffset`
- `MemLoad`
- `MemStore`
- `Alloca` with `Uninitialized`
- raw casts between integer and pointer forms

This integrates naturally with KAIN’s existing effect system.

## Layout Model

Layout cannot stay implicit if `1:1` import is the goal.

Recommended data model:

```rust
struct LayoutInfo {
    size_bytes: Option<usize>,
    align_bytes: Option<usize>,
    field_offsets: HashMap<String, usize>,
    packed: bool,
}
```

Recommended registry:

```rust
struct LayoutRegistry {
    named_types: HashMap<String, LayoutInfo>,
}
```

Uses:

- `sizeof`
- `alignof`
- field offset reasoning
- backend lowering
- importer diagnostics

This should be data-driven and queryable, not recomputed ad hoc inside importers.

## Lowering Strategy

### Backend Classes

Backends should not all receive raw low-level nodes unchanged.

Recommended policy classes:

1. native-like
   - LLVM
   - C++
   - maybe Rust
2. emulated-low-level
   - WASM
   - JS
   - TS
3. engine-bridge
   - UE5
   - UE5 Editor

### Policy Table

Recommended data-driven lowering table:

```rust
struct MemoryLoweringPolicy {
    backend: CompileTarget,
    supports_raw_ptr: bool,
    supports_stack_alloca: bool,
    supports_uninitialized_storage: bool,
    supports_volatile_access: bool,
    lower_ptr_to_indexed_buffer: bool,
    lower_mem_ops_to_runtime_intrinsics: bool,
}
```

Examples:

- LLVM/C++:
  - preserve most low-level nodes
- TS/JS:
  - lower raw memory into runtime-managed buffers/views
- UE5:
  - lower into generated helper layer or runtime bridge types

## Importer Benefits

### C Import

This is the immediate win.

The C importer becomes simpler and more honest:

- `char*` and `int64_t*` stop pretending to be normal refs
- pointer arithmetic becomes `PtrOffset`
- dereference becomes `MemLoad` / assignment through deref becomes `MemStore`
- stack arrays can become `Alloca`
- `sizeof` becomes a first-class semantic node instead of an importer estimate when desired

### C++ Import

Future benefit:

- raw pointers
- references vs pointers
- object layout
- vtable-adjacent low-level code

### Assembly Import

This may benefit the most.

Assembly import already wants:

- addressable memory
- loads/stores
- offsets
- explicit layout

That should target the same low-level memory IR, not invent a second path.

## Self-Hosting Benefits

For self-hosting and runtime libraries, this unlocks:

- allocators
- raw runtime bridges
- parser/tokenizer internals
- binary serialization
- tagged unions stored in memory
- VM and emulator kernels

That is strategically important. It lets KAIN express more of its own low-level infrastructure without forcing that logic to stay in C.

## Backend Benefits

The key benefit to codegen is not “more complexity everywhere.”

The benefit is:

- one semantic model
- one lowering contract
- fewer importer-side distortions

If implemented correctly, backends actually get simpler because they receive normalized intent instead of increasingly strange high-level approximations.

## Diagnostics Benefits

This also improves diagnostics.

Recommended new diagnostic categories:

- pointer arithmetic lowered through approximation
- layout unavailable for `sizeof`
- volatile memory unsupported for current backend
- uninitialized storage lowered conservatively
- alias-sensitive code lowered via emulation

These should be machine-readable for importer reports.

## Compatibility Plan

### Backward Compatibility

Existing high-level KAIN code should keep working unchanged.

Key rules:

1. do not change meaning of existing `&T` and `&mut T`
2. add new low-level semantics alongside them
3. only emit low-level nodes from importers or explicit low-level syntax/capability use

### Migration Strategy

1. add new AST/types
2. add typechecking support
3. add lowering pass with no parser syntax yet
4. update C importer to emit new nodes
5. update asm importers to emit new nodes
6. expose syntax later if still justified

## Proposed Phases

### Phase 1: Internal Semantic Layer

- add `Type::Ptr`
- add `Expr::PtrOffset`
- add `Expr::MemLoad`
- add `Expr::MemStore`
- add `Expr::SizeOfType`
- add `Expr::AlignOfType`
- add `MemoryInitPolicy`
- add capability/policy structs

### Phase 2: Typechecker + Runtime Plumbing

- validate pointer operations
- route low-level ops through `Unsafe`
- provide minimal interpreter behavior
- allow non-native backends to keep identity/emulated behavior during transition

### Phase 3: Lowering Pass

- implement data-driven backend lowering
- keep codegen crates mostly untouched

### Phase 4: Importer Integration

- upgrade C importer to emit new nodes
- upgrade `kain-asm` to target same nodes

### Phase 5: Optional Surface Syntax

- only after IR and lowering are stable

## Recommended First Implementation Slice

The first slice should be small and high-value:

1. `Type::Ptr`
2. `Expr::PtrOffset`
3. `Expr::MemLoad`
4. `Expr::MemStore`
5. a `MemoryLoweringPolicy` registry
6. importer updates for:
   - pointer arithmetic
   - deref read/write
   - raw pointer casts

This is enough to materially improve C import and assembly import without forcing immediate layout/storage overhaul.

## Risks

### Risk 1: Backend churn

Mitigation:

- keep low-level nodes behind a lowering pass

### Risk 2: Semantic duplication with existing refs

Mitigation:

- keep `Ref` and `Ptr` distinct by design

### Risk 3: Importer overreach

Mitigation:

- importer emits low-level nodes only when source semantics truly require them

### Risk 4: Unbounded language complexity

Mitigation:

- phase internal IR first
- delay surface syntax
- gate through capabilities and effects

## Success Criteria

This design is successful when:

1. C importer stops relying on ref-based approximations for raw pointer behavior.
2. Assembly import and C import share the same low-level memory semantics.
3. Existing backends continue to work through normalization/lowering.
4. Self-hosting runtime samples import more faithfully with fewer special cases.
5. The low-level layer stays additive and does not degrade normal high-level KAIN workflows.

## Recommendation

Proceed with the low-level memory layer in `kain-core`.

This is the right abstraction boundary:

- richer than importer hacks
- safer than backend-specific pointer logic
- broader than just C import

It makes KAIN stronger as:

- a systems language
- a transliteration layer
- an assembly bridge
- a future self-hosting platform

The implementation should start with internal IR and lowering policy, not syntax.
