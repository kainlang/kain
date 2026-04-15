# Selfhost Direction

Snapshot: April 14, 2026.

This document is the durable execution contract for the hand-written selfhost lane.
`src/README.md` is the high-energy stream-of-consciousness vision document.
This file turns that vision into a concrete compiler direction other agents can follow.

## Core Call

Kain should become self-hosting through a manifest-first, hand-written compiler lane.

The final goal is:

- the compiler is written in Kain
- the compiler compiles its own compiler sources
- the compiler does not depend on Rust compiler logic for parsing, typechecking, lowering, or codegen once bootstrap is over

Rust is allowed only as a temporary host substrate during bootstrap.
That means Rust can assist with filesystem, process execution, reflection export, bridge loading, and validation/oracle work, but Rust must stop owning compiler passes.

## What Counts As Real Selfhost

Real selfhost:

- Kain owns manifest loading and module graph construction
- Kain owns parsing
- Kain owns name resolution
- Kain owns typechecking
- Kain owns memory lowering / ABI decisions
- Kain owns backend/codegen decisions

Acceptable temporary bootstrap assistance:

- path, env, filesystem, process helpers
- TOML / JSON parsing helpers
- reflected host prelude generation
- temporary FFI bridges
- oracle comparisons against Rust implementations

Fake selfhost:

- Kain source that still calls back into Rust parser/typechecker/lowering/codegen passes on the main compile path

## Architectural Direction

### 1. `KAIN.toml` is the canonical compiler contract

The selfhost pipeline should be package-first and manifest-first.

`KAIN.toml` should become the source of truth for:

- package identity
- compiler entrypoint
- module roots / search paths
- build targets
- bootstrap mode
- selfhost settings
- host bridge declarations
- FFI declarations
- verification and artifact settings

The compiler should stop treating project builds as "read one entry file into one source string".
The hand-written lane must own a real module/package graph.

The current owned-lane manifest now lives at `src/KAIN.toml`.
It is repo-root-relative on purpose so the CLI bootstrap path, shell wrappers, and future native `kainc` runs all resolve the same contract from the repo root.

The manifest is expected to carry, at minimum:

- package identity
- compiler entrypoint
- ordered `src/core` source set for the temporary aggregate bootstrap
- future module roots and search paths for the real multi-file frontend
- native runtime manifest and runtime build script references
- artifact and report output paths under `src/.selfhost/`
- bootstrap mode and verification mode
- optional FFI declarations with explicit allowed and forbidden roles

### 2. `src/core` is the owning compiler lane

- `src/core` is the only hand-owned compiler lane
- `src/.rustimport/phase2` is reference and mirror material
- `src/.legacy` is structural reference material

Do not let the Rust import lane become the permanent semantic owner of the compiler.

### 3. Keep the structure from `.legacy`, not the runtime model

The old lane got important things right:

- explicit staged bootstrap shape
- ordered compiler assembly
- honest artifact discipline
- honest failure / exit behavior

Those ideas should be reused.

Do not revive:

- the old NaN-boxed runtime model
- transition hacks
- compatibility shortcuts that diverge from current Kain semantics

### 4. Use a mixed bridge strategy during bootstrap

Preferred bridge policy:

- reflected host prelude / generated host module for typed bootstrap services
- `c_ffi` for bridges that must survive into LLVM/native lanes
- `rust crate FFI` only as a temporary bootstrap accelerator or oracle lane

`rust crate FFI` is useful during bootstrap, but it must not become the permanent definition of the compiler.
If the main compiler path still depends on `use rust::<crate>` for compiler logic, the hand-written lane has failed to become independent.

### 5. The native C runtime is the runtime target, not an optional side lane

The hand-written selfhost lane should target the existing native runtime contract from day one.

- `runtime/native_runtime.toml` is the canonical native runtime manifest
- `runtime/compile_native_runtime.sh` is the canonical runtime build entrypoint for bootstrap
- `src/.selfhost/` is the owned-lane artifact root for the compiler outputs that link against that runtime

The bootstrap host may orchestrate runtime discovery and linking.
It must not replace the native runtime with a Rust-defined runtime model.

## Recommended Stages

### Stage 0: Rust bootstrap host

Use Rust as a thin host substrate only.

Responsibilities:

- launch bootstrap runs
- expose typed host services into Kain
- export reflected host/module surfaces
- assist with manifest parsing if needed temporarily
- assist with process spawning and tool invocation
- delegate from `src/build_selfhost.sh` into `kain selfhost bootstrap`

Non-responsibilities:

- owning parser logic
- owning typechecker logic
- owning lowering logic
- owning backend logic

### Stage 1: Kain-owned compiler in host-backed mode

The compiler runs in a host-backed lane, but the compiler passes are implemented in Kain.

Primary goal:

- Kain compiler can compile real Kain compiler sources under a controlled selfhost subset

### Stage 2: Kain compiles Kain

Primary goal:

- the hand-written compiler compiles its own compiler sources
- Rust host services remain allowed only for generic host utilities

### Stage 3: Native selfhost lane

Primary goal:

- the hand-written compiler emits LLVM/native artifacts
- Rust is no longer part of the compiler-logic path
- remaining host/toolchain dependencies are ordinary external tools, not semantic ownership

Promotion command surface:

- `src/build_selfhost.sh`
- `kain selfhost bootstrap`

Promotion artifact root:

- `src/.selfhost/`

## Immediate Compiler Priorities

The first hand-written lane does not need every aspirational language feature before it becomes real.
It does need a disciplined subset strong enough to compile the compiler honestly.

Required early priorities:

- manifest loading
- module graph resolution
- stable span / diagnostics contract
- parser
- name resolution
- typechecking
- enough data structures and generics to express compiler internals
- enough `comptime` to support compiler metadata and configuration
- memory lowering / ABI path
- LLVM/native backend path
- honest report and artifact contracts under `src/.selfhost/`

Long-term language intent remains much larger:

- Rust-grade safety and ownership
- Python-like significant-whitespace syntax
- Lisp/Zig-style metaprogramming and `comptime`
- effect tracking
- actor-model concurrency
- expressive JSX/component UI
- GPU / 3D / native runtime power as first-class concerns

That is the north star.
But the compiler source should first target a controlled selfhost subset instead of trying to dogfood every frontier feature at once.

## Keep / Drop Summary

Keep from `.legacy`:

- stage discipline
- bootstrap contract
- honest build and verification behavior
- source-lane ownership mindset

Drop from `.legacy`:

- old runtime representation
- NaN-box assumptions
- migration hacks
- anything that drifts from current Kain semantics

Keep from the current repo:

- `KAIN.toml` as the package/build contract
- bridge systems that are already real
- reflection/export tools for typed host integration
- the existing native runtime manifest and compile script

Drop from the current repo's selfhost endgame:

- dependence on the Rust-import mirror as permanent semantic truth
- dependence on single-source-string project compilation for the hand-written lane
- dependence on Rust compiler passes for the real compile path

The temporary aggregate-source bootstrap is still allowed as a compatibility bridge.
The rule is that the public contract must already be future-proof for module-graph compilation, which is why `src/KAIN.toml` carries both `source_order` and module-root/search-path data.

## Anti-Goals

Avoid these outcomes:

- a "selfhost" lane that is really just Kain syntax wrapped around Rust compiler passes
- a permanent dependency on `rust crate FFI` for compiler semantics
- a return to the old runtime/value model purely because it bootstraps easily
- letting orchestration/manifests replace the language instead of serving it

## Practical Rule For Future Agents

When making selfhost decisions:

1. Prefer the hand-written compiler lane over the Rust mirror lane.
2. Prefer Kain-owned compiler logic over Rust-owned compiler logic.
3. Prefer bridge code for host capabilities, not for semantic ownership.
4. Prefer `c_ffi` over Rust-only bridges for anything that must survive into native/LLVM.
5. Reuse `.legacy` for stage structure and discipline, not for semantics or runtime representation.

## Concrete Bootstrap Contract

Future agents should assume these paths are now durable unless the manifest says otherwise:

- manifest: `src/KAIN.toml`
- wrapper: `src/build_selfhost.sh`
- owned source root: `src/core`
- generated artifact root: `src/.selfhost`
- native runtime manifest: `runtime/native_runtime.toml`
- native runtime compile script: `runtime/compile_native_runtime.sh`

The wrapper is intentionally thin.
The CLI remains the source of truth for bootstrap execution, while `src/KAIN.toml` remains the source of truth for owned-lane configuration.
