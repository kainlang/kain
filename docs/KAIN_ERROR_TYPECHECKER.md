# Kain Typechecker Error Reference

**Source:** `KAIN_ERROR_SPECS.md` (2,655 lines, 184 errors) + compiler source audit (`crates/error/src/code.rs`, `crates/core/src/types.rs`)
**Generated:** 2026-06-27
**Scope:** All non-parse error categories — typechecker, semantic, lowering, and runtime errors
**Total errors:** 169 (164 from spec + 5 undocumented from compiler source)

> **Note:** Parse errors (KAIN-PARSE-0001 through KAIN-PARSE-0020, 20 errors) are handled in a separate document. This reference covers all typechecker and semantic errors.

---

## Category Index

| Category | Error Count | First Code | Last Code | Description |
|----------|:-----------:|------------|-----------|-------------|
| borrow | 10 | KAIN-BORROW-0001 | KAIN-BORROW-0010 | Ownership, single_writer, use-after-move, weak refs |
| codegen | 11 | KAIN-CODEGEN-0001 | KAIN-CODEGEN-0011 | LLVM lowering, backend linking, ABI mismatch |
| component | 8 | KAIN-ACTOR-0001 | KAIN-ACTOR-0008 | Actor spawn/send/receive, component props/state |
| comptime | 10 | KAIN-COMPTIME-0001 | KAIN-COMPTIME-0010 | Macro expansion, comptime eval, patch/law/axiom |
| config | 6 | KAIN-CONFIG-0001 | KAIN-CONFIG-0006 | Toolchain, manifest, project setup |
| converge | 8 | KAIN-CONVERGE-0001 | KAIN-CONVERGE-0008 | Fast lanes, spec contract, verifier sampling |
| effect | 12 | KAIN-EFFECT-0001 | KAIN-EFFECT-0012 | Effect system (Pure, IO, Async, GPU, Reactive, Unsafe) |
| entangle | 7 | KAIN-ENTANGLE-0001 | KAIN-ENTANGLE-0007 | World coupling, single_writer, cycle detection |
| internal | 1 | KAIN-INTERNAL-0001 | KAIN-INTERNAL-0001 | Compiler bugs, impossible states |
| io | 6 | KAIN-IO-0001 | KAIN-IO-0006 | I/O operations |
| memory | 8 | KAIN-MEM-0001 | KAIN-MEM-0008 | Memory layout, alignment, address spaces |
| patch | 7 | KAIN-PATCH-0001 | KAIN-PATCH-0007 | World mutation, law pre/postconditions |
| pulse (undocumented) | 3 | KAIN-PULSE-BUDGET-0001 | KAIN-PULSE-BUDGET-0003 | **Undocumented** — compile-time pulse budget constraints |
| runtime | 8 | KAIN-RUNTIME-0001 | KAIN-RUNTIME-0008 | Runtime errors, actor panic, deadlock |
| shader | 14 | KAIN-SHADER-0001 | KAIN-SHADER-0043 | Shader compilation, GPU pipeline (+2 undocumented: subgroup) |
| state | 8 | KAIN-STATE-0001 | KAIN-STATE-0008 | State machines, pulse, guarantee |
| test | 7 | KAIN-TEST-0001 | KAIN-TEST-0007 | Test framework, assertions, verify |
| type | 26 | KAIN-TYPE-0001 | KAIN-TYPE-0026 | Type system (the BIG one) |
| validation | 1 | KAIN-VALIDATE-0001 | KAIN-VALIDATE-0001 | Cross-pass structural validation |
| world | 8 | KAIN-WORLD-0001 | KAIN-WORLD-0008 | World declarations, surfaces, teleport |

---

## Error Details By Category

### borrow

Ownership, borrowing, single_writer, weak references, shared state. Kain's ownership system enforces explicit `collapse`/`observe`/`decay` semantics — see `OWNERSHIP.MD`.

#### KAIN-BORROW-0001 — Borrow Error
- **Severity:** error
- **Category:** borrow/general
- **Help:** A borrow-checking rule has been violated. Kain's ownership system enforces single-writer or multiple-reader semantics on shared state.
- **See Also:** KAIN-BORROW-0002, KAIN-BORROW-0003

#### KAIN-BORROW-0002 — Multiple Mutable Borrows
- **Severity:** error
- **Category:** borrow/multiple-mutable
- **Help:** A mutable reference to shared state overlaps with another active mutable or immutable reference. Kain enforces `single_writer` semantics — only one writer OR multiple readers may be active at a time. Fix: restructure the code so borrows do not overlap, or use an explicit scope to shorten one of the borrow lifetimes.
- **Example Bad:** `let mut x = 0\nlet a = &mut x\nlet b = &mut x`
- **Example Good:** `let mut x = 0\n{\n    let a = &mut x\n}\nlet b = &mut x`

#### KAIN-BORROW-0003 — Borrow And Mutation Conflict
- **Severity:** error
- **Category:** borrow/borrow-mutation-conflict
- **Help:** A value is borrowed (either mutably or immutably) while a mutation occurs through another path. This violates the single-writer invariant. Fix: complete all borrows before mutating, or clone the value.
- **See Also:** KAIN-BORROW-0002

#### KAIN-BORROW-0004 — Use After Move
- **Severity:** error
- **Category:** borrow/use-after-move
- **Help:** A value has been moved (ownership transferred) and is used afterwards. By default, Kain moves values into function arguments, assignments, and return positions. Fix: clone the value before the move, or restructure to borrow instead.
- **Example Bad:** `let x = [1, 2, 3]\nlet y = x\nprint(x)`
- **Example Good:** `let x = [1, 2, 3]\nlet y = x.clone()\nprint(x)`

#### KAIN-BORROW-0005 — Shared State Without Annotation
- **Severity:** error
- **Category:** borrow/missing-shared-annotation
- **Help:** State that is accessed from multiple actors, components, or threads must be explicitly annotated with `shared` or `single_writer`. The compiler detected cross-actor access without the required annotation. Fix: add the `shared` keyword to the state declaration, or use appropriate synchronization primitives.
- **See Also:** KAIN-BORROW-0006

#### KAIN-BORROW-0006 — Single Writer Violation
- **Severity:** error
- **Category:** borrow/single-writer-violation
- **Help:** State annotated `single_writer` is being mutated from multiple locations simultaneously. Only one writer may exist at any time for single_writer-protected state. Fix: serialize writes through a single owner, or upgrade to a more permissive sharing model.
- **See Also:** KAIN-BORROW-0005

#### KAIN-BORROW-0007 — Weak Reference Upgraded Unsafely
- **Severity:** error
- **Category:** borrow/weak-upgrade
- **Help:** A `weak` reference was upgraded to a strong reference, but the target has already been dropped. Weak references must check for liveness before upgrading. Fix: use the `?` operator or an `if let` pattern when upgrading weak references.
- **Example Bad:** `let strong = weak_ref.upgrade()`
- **Example Good:** `if let Some(strong) = weak_ref.upgrade(): ...`

#### KAIN-BORROW-0008 — Lifetime Mismatch
- **Severity:** error
- **Category:** borrow/lifetime-mismatch
- **Help:** A reference outlives the value it points to. Kain tracks lifetimes implicitly for most references and detected that the borrow outlasts the owned value. Fix: restructure to keep the owned value alive longer than its borrows, or clone the value so the borrow is not needed.
- **Example Bad:** `let ref: &i32\n{\n    let x = 5\n    ref = &x\n}\nprint(ref)`
- **Example Good:** `let x = 5\nlet ref = &x\nprint(ref)`

#### KAIN-BORROW-0009 — Send Constraint Violation
- **Severity:** error
- **Category:** borrow/send-violation
- **Help:** A value is being `send` to another actor or component but its type does not satisfy the `Send` trait (it contains non-sendable state such as raw pointers or actor-local references). Fix: ensure all fields of the sent type satisfy `Send`, or use a serialization boundary.
- **See Also:** KAIN-ACTOR-0003

#### KAIN-BORROW-0010 — Implicit Clone On Large Value
- **Severity:** warning
- **Category:** borrow/large-clone
- **Help:** A potentially large value is being implicitly cloned. Consider passing by reference or using `shared` to avoid the copy. Fix: use `&value` to borrow, or add an explicit `.clone()` if the copy is intentional.

---

### codegen

Code generation, lowering, backend compilation, linking. These fire when the compiler cannot produce valid output for the target backend.

#### KAIN-CODEGEN-0001 — Codegen Error
- **Severity:** error
- **Category:** codegen/general
- **Help:** A code generation or lowering pass failed.
- **See Also:** KAIN-CODEGEN-0002

#### KAIN-CODEGEN-0002 — Unknown Codegen Variable
- **Severity:** error
- **Category:** codegen/unknown-variable
- **Help:** The lowered backend could not find a value for this symbol. The frontend may have accepted invalid code, or a lowering pass lost a binding. Fix: check whether the frontend should have rejected this earlier or whether the lowering pass dropped the binding.

#### KAIN-CODEGEN-0003 — Lowering Pass Failed
- **Severity:** error
- **Category:** codegen/lowering-failed
- **Help:** An IR lowering pass could not transform the intermediate representation. This typically occurs when a Kain construct has no direct lowering path to the target backend. Fix: simplify the construct or add a lowering rule for the target.

#### KAIN-CODEGEN-0004 — Backend Compilation Failed
- **Severity:** error
- **Category:** codegen/backend-failed
- **Help:** The native code backend (LLVM, C, or target-specific compiler) could not compile the generated code. Fix: check target compatibility, reduce optimization level, or simplify the generated code.

#### KAIN-CODEGEN-0005 — Linking Failed
- **Severity:** error
- **Category:** codegen/linking-failed
- **Help:** The linker could not resolve all symbols or encountered a conflict. This may indicate missing libraries, duplicate symbols, or target-incompatible object files. Fix: check library paths, symbol names, and target architecture.

#### KAIN-CODEGEN-0006 — Unsupported Target Architecture
- **Severity:** error
- **Category:** codegen/unsupported-target
- **Help:** The specified compilation target (CPU architecture, OS, GPU, or runtime) is not supported by the current Kain backend configuration. Fix: choose a supported target or enable the required backend feature.
- **See Also:** KAIN-CODEGEN-0007

#### KAIN-CODEGEN-0007 — Target Capability Missing
- **Severity:** error
- **Category:** codegen/capability-missing
- **Help:** The code uses a `target` or `capability` that the current compilation target does not provide. Capabilities are feature-gated by the target specification. Fix: guard the capability-dependent code with `if capability(...)` or change the compilation target.

#### KAIN-CODEGEN-0008 — Foreign ABI Mismatch
- **Severity:** error
- **Category:** codegen/foreign-abi-mismatch
- **Help:** A foreign function interface (FFI) declaration does not match the actual symbol in the linked library. Type layouts, calling conventions, or symbol names may differ. Fix: align the Kain FFI declaration with the native library's ABI.

#### KAIN-CODEGEN-0009 — Codegen Intrinsic Not Found
- **Severity:** error
- **Category:** codegen/intrinsic-not-found
- **Help:** A compiler intrinsic required by the generated code is not available for the target platform. Some intrinsics are platform-specific. Fix: use a portable alternative or add a platform-specific implementation.

#### KAIN-CODEGEN-0010 — Optimization Pass Failed
- **Severity:** warning
- **Category:** codegen/optimization-failed
- **Help:** An optimization pass encountered an unexpected IR state and was skipped. The generated code is correct but may be slower than optimal. Fix: this is typically a compiler bug — report the issue with a reproduction.

#### KAIN-CODEGEN-0011 — Codegen Budget Exceeded
- **Severity:** error
- **Category:** codegen/budget-exceeded
- **Help:** Code generation exceeded a resource budget (time, memory, or output size limit). The generated code is too large or complex for the target. Fix: split the compilation unit, reduce inlining, or increase the budget via compiler flags.

---

### component

Components, actors, spawn, send, receive, emit, observe, decay, on. See `COMPONENT.MD` for component typechecking rules.

#### KAIN-ACTOR-0001 — Component/Actor Error
- **Severity:** error
- **Category:** actor/general
- **Help:** A component or actor system invariant has been violated.

#### KAIN-ACTOR-0002 — Actor Spawn Failed
- **Severity:** error
- **Category:** actor/spawn-failed
- **Help:** `spawn` could not create the actor or component. Common causes: the target world does not exist or has been torn down; the actor type is not registered with the world; resource limits have been exceeded. Fix: ensure the world is alive and the actor type is registered.

#### KAIN-ACTOR-0003 — Send To Invalid Actor
- **Severity:** error
- **Category:** actor/send-invalid
- **Help:** `send` targets an actor that does not exist, has been destroyed, or does not accept messages of the given type. Fix: check that the target actor is alive and handles the message type.
- **See Also:** KAIN-BORROW-0009

#### KAIN-ACTOR-0004 — Receive Handler Missing
- **Severity:** error
- **Category:** actor/receive-missing
- **Help:** An actor or component receives messages but does not have a `receive` handler for one or more of the message types sent to it. Fix: add a `receive` block for the unhandled message type.

#### KAIN-ACTOR-0005 — Emit Without Listener
- **Severity:** warning
- **Category:** actor/emit-no-listener
- **Help:** `emit` sends an event, but no component or actor is `observe`-ing the event. The emission has no effect. Fix: add an observer for the event, or remove the dead emit.

#### KAIN-ACTOR-0006 — Decay Transition Invalid
- **Severity:** error
- **Category:** actor/decay-invalid
- **Help:** A `decay` state transition targets an invalid state or occurs in a context where decay is not meaningful. Fix: ensure the decay target state exists in the state machine.
- **See Also:** KAIN-STATE-0004

#### KAIN-ACTOR-0007 — Component Lifecycle Violation
- **Severity:** error
- **Category:** actor/lifecycle-violation
- **Help:** An operation on a component is invalid for its current lifecycle stage. Components have distinct phases: created, active, decaying, destroyed. Operations must align with the current phase. Fix: check the component lifecycle phase before the operation.

#### KAIN-ACTOR-0008 — On Handler Duplicate
- **Severity:** error
- **Category:** actor/on-duplicate
- **Help:** Multiple `on` handlers are registered for the same event on the same actor or component. Only one handler per event type is allowed per component. Fix: consolidate the handlers or use different event types.

---

### comptime

Compile-time evaluation, macro expansion, patch, law, axiom, orchestrate, converge, shatter.

#### KAIN-COMPTIME-0001 — Comptime Error
- **Severity:** error
- **Category:** comptime/general
- **Help:** A compile-time evaluation failed. This is the generic comptime fallback.

#### KAIN-COMPTIME-0002 — Comptime Evaluation Exceeded Recursion Limit
- **Severity:** error
- **Category:** comptime/recursion-limit
- **Help:** A `comptime` expression recursed beyond the compile-time evaluation limit (default: 1000 steps). This prevents infinite loops during compilation. Fix: increase the recursion depth or restructure the comptime logic to terminate earlier.

#### KAIN-COMPTIME-0003 — Comptime Access To Runtime Value
- **Severity:** error
- **Category:** comptime/runtime-value
- **Help:** A `comptime` block attempts to access a value that is only available at runtime (e.g., user input, file I/O result, actor state). Compile-time evaluation can only use constants, type information, and comptime-known values. Fix: move the runtime-dependent logic out of the comptime block.

#### KAIN-COMPTIME-0004 — Macro Expansion Error
- **Severity:** error
- **Category:** comptime/macro-expansion
- **Help:** A `macro` failed to expand — either the macro body produced invalid syntax, the argument count mismatched, or an expansion recursion limit was hit. Fix: check the macro definition and call site for consistency.
- **See Also:** KAIN-COMPTIME-0002

#### KAIN-COMPTIME-0005 — Patch Target Not Found
- **Severity:** error
- **Category:** comptime/patch-target
- **Help:** A `patch` declaration targets a function, type, or module that does not exist or is not patchable. Only items explicitly marked as patchable can be modified by patches. Fix: ensure the target exists and is declared in a patchable context.

#### KAIN-COMPTIME-0006 — Law Violation
- **Severity:** error
- **Category:** comptime/law-violation
- **Help:** A `law` invariant has been violated. Laws are compile-time assertions about types, values, or structural properties that must hold for the program to be valid. Fix: adjust the code to satisfy the law, or modify the law if it is too restrictive.

#### KAIN-COMPTIME-0007 — Axiom Contradiction
- **Severity:** error
- **Category:** comptime/axiom-contradiction
- **Help:** An `axiom` (assumed truth) contradicts another axiom or a proven law. The axiom set is inconsistent — the compiler cannot proceed. Fix: remove or weaken one of the conflicting axioms.
- **See Also:** KAIN-COMPTIME-0006

#### KAIN-COMPTIME-0008 — Orchestrate Dependency Cycle
- **Severity:** error
- **Category:** comptime/orchestrate-cycle
- **Help:** `orchestrate` defines a compile-time pipeline of transformations, but the pipeline contains a dependency cycle (a step depends on its own output, directly or transitively). Fix: break the cycle by reordering or splitting pipeline steps.

#### KAIN-COMPTIME-0009 — Converge Failed
- **Severity:** error
- **Category:** comptime/converge-failed
- **Help:** `converge` attempts to unify multiple code variants but the variants are not structurally compatible — they produce different types, effect sets, or control flow. Fix: align the variants so they converge to a consistent interface.

#### KAIN-COMPTIME-0010 — Shatter Pattern Incomplete
- **Severity:** error
- **Category:** comptime/shatter-incomplete
- **Help:** `shatter` splits a generic function into specialized variants, but the pattern does not cover all concrete type combinations used at call sites. Fix: add the missing specialization pattern or provide a fallback generic implementation.

---

### config

Configuration, manifests, toolchain, project setup.

#### KAIN-CONFIG-0001 — Config Error
- **Severity:** error
- **Category:** config/general
- **Help:** A configuration or manifest validation error occurred.

#### KAIN-CONFIG-0002 — Manifest Parse Error
- **Severity:** error
- **Category:** config/manifest-parse
- **Help:** A Kain manifest (kain.toml, fabric.toml, blade.toml) could not be parsed. The file may have invalid TOML syntax or missing required fields. Fix: validate the manifest file against the expected schema.

#### KAIN-CONFIG-0003 — Toolchain Not Found
- **Severity:** error
- **Category:** config/toolchain-not-found
- **Help:** A required external toolchain (LLVM, MSVC, GCC, GPU SDK, UE5) is not installed or not detected on the system PATH. Fix: install the required toolchain and ensure it is on the PATH.

#### KAIN-CONFIG-0004 — Target Specification Invalid
- **Severity:** error
- **Category:** config/target-invalid
- **Help:** The build target specification (target triple, platform, GPU, runtime) is invalid or contradictory. Fix: provide a valid target specification. Use `kain targets` to list available targets.

#### KAIN-CONFIG-0005 — Feature Flag Conflict
- **Severity:** error
- **Category:** config/feature-conflict
- **Help:** Two or more enabled feature flags are mutually exclusive or conflict. Check the feature flag documentation for compatibility. Fix: disable one of the conflicting features.

#### KAIN-CONFIG-0006 — Dependency Resolution Failed
- **Severity:** error
- **Category:** config/dependency-failed
- **Help:** A project dependency could not be resolved. The package may not exist, the version constraint may be unsatisfiable, or there may be a dependency cycle. Fix: check the dependency name, version, and source.

---

### converge

Fast-lane dispatch selection, spec/fast contract verification, verifier sampling, lane capability gaps, and return/effect-set divergence. `converge` computes a value using the spec lane as the oracle and one or more fast lanes as optimised alternatives.

#### KAIN-CONVERGE-0001 — Converge Error
- **Severity:** error
- **Category:** converge/general
- **Help:** A converge block has a well-formedness error. This is the generic fallback.
- **See Also:** KAIN-CONVERGE-0002, KAIN-CONVERGE-0003

#### KAIN-CONVERGE-0002 — Converge Missing Spec Lane
- **Severity:** error
- **Category:** converge/missing-spec
- **Help:** Every `converge` block must have exactly one `spec reference:` lane that acts as the ground-truth oracle for verifier sampling. None was found. Fix: add a `spec reference:` lane before any `fast` lanes.
- **Example Bad:** `converge compute(x: Int) -> Int:\n    fast avx2_lane when capability("cpu.x86.avx2"):\n        return simd_mix(x)`
- **Example Good:** `converge compute(x: Int) -> Int:\n    spec reference:\n        return scalar_mix(x)\n    fast avx2_lane when capability("cpu.x86.avx2"):\n        return simd_mix(x)`

#### KAIN-CONVERGE-0003 — Converge Fast Lane Contract Mismatch
- **Severity:** error
- **Category:** converge/fast-lane-mismatch
- **Help:** A `fast` lane's verifier samples do not agree with the `spec reference` oracle. The fast implementation is semantically inequivalent. Fix: align the fast lane's result with the spec lane for all inputs, or reduce the verifier sample count and re-check the boundary cases.
- **See Also:** KAIN-CONVERGE-0004

#### KAIN-CONVERGE-0004 — Converge Verifier Failed
- **Severity:** error
- **Category:** converge/verifier-failed
- **Help:** The `verify` clause ran the built-in random sampler against the spec and fast lanes and found a disagreement. The fast lane diverges from the spec on at least one generated input. Fix: inspect the diverging input (the compiler will emit it) and fix the fast lane logic.
- **See Also:** KAIN-CONVERGE-0003

#### KAIN-CONVERGE-0005 — Converge Capability Gap At Target
- **Severity:** error
- **Category:** converge/capability-gap
- **Help:** A `fast ... when capability(...)` lane requires a hardware capability (e.g., `cpu.x86.avx2`, `gpu.tensor_core`) that is absent on the current compilation target. No valid lane remains after filtering. Fix: add a fallback fast lane for the common-case target, or make the spec lane available unconditionally.
- **See Also:** KAIN-CONVERGE-0002

#### KAIN-CONVERGE-0006 — Converge Return Type Divergence
- **Severity:** error
- **Category:** converge/return-type-divergence
- **Help:** The spec lane and one or more fast lanes return incompatible types. All lanes in a `converge` block must agree on the return type. Fix: align the return type annotation across all lanes.

#### KAIN-CONVERGE-0007 — Converge Effect Set Divergence
- **Severity:** error
- **Category:** converge/effect-set-divergence
- **Help:** Lanes within a `converge` block declare different effect sets. The compiler cannot guarantee safe dispatch — a caller expecting a Pure lane might receive an IO lane at runtime. Fix: ensure all lanes agree on effect annotations, or mark the converge block with the union of required effects.
- **See Also:** KAIN-EFFECT-0001

#### KAIN-CONVERGE-0008 — Converge Ambiguous Lane Selection
- **Severity:** error
- **Category:** converge/ambiguous-lane
- **Help:** Multiple `fast` lanes match the current target's capability set and runtime conditions simultaneously. The compiler cannot deterministically choose one. Fix: use more specific `when` guards or add a priority ordering with `prefer` annotations.

---

### effect

Effect system: Pure, IO, Async, GPU, Reactive, Unsafe. See `EFFECTS.MD` for the full effect lattice and `can_call()` algorithm.

#### KAIN-EFFECT-0001 — Effect Violation
- **Severity:** error
- **Category:** effects/violation
- **Help:** A function or block performs an operation that is not permitted by its declared effect annotation. The effect system tracks side-effect capabilities (IO, GPU, async, etc.) and ensures they are explicitly declared. Fix: either add the required effect annotation to the enclosing function, or move the violating operation behind a compatible effect boundary.
- **See Also:** KAIN-EFFECT-0002

#### KAIN-EFFECT-0002 — Missing Effect Capability
- **Severity:** error
- **Category:** effects/missing-capability
- **Help:** A function is annotated with an effect (e.g., `GPU`) but the caller does not have that capability in its own effect set. Effect capabilities flow upward — callers must declare at least the effects of their callees. Fix: propagate the effect annotation to the caller, or gate the call behind a `capability` check.
- **See Also:** KAIN-EFFECT-0001, KAIN-EFFECT-0005

#### KAIN-EFFECT-0003 — Effect Polymorphism Mismatch
- **Severity:** error
- **Category:** effects/polymorphism-mismatch
- **Help:** A generic effect-polymorphic function was instantiated with effect parameters that conflict. Effect variables must unify consistently across all call sites. Fix: align the effect parameters or make the function more permissive.

#### KAIN-EFFECT-0004 — Pure Function With Side Effect
- **Severity:** error
- **Category:** effects/pure-side-effect
- **Help:** A function annotated `Pure` (or implicitly pure by default) contains an operation with observable side effects (IO, mutation of shared state, GPU dispatch, async scheduling). Fix: remove the side effect, or change the annotation to the appropriate effect (e.g., `IO`, `GPU`).
- **Example Bad:** `Pure fn compute: {\n    print("hello")\n}`
- **Example Good:** `IO fn compute: {\n    print("hello")\n}`

#### KAIN-EFFECT-0005 — Capability Gate Failure
- **Severity:** error
- **Category:** effects/capability-gate
- **Help:** A `capability` check guards access to an effect-gated operation, but the required capability is not available at the call site. Capabilities are intrinsic permissions granted by the runtime or platform. Fix: acquire the needed capability before the gate, or restructure the code to run in a context where the capability is present.
- **See Also:** KAIN-EFFECT-0002

#### KAIN-EFFECT-0006 — Async In Sync Context
- **Severity:** error
- **Category:** effects/async-in-sync
- **Help:** An `async` function or `await` expression appears in a non-async context that cannot suspend. Async can only be called from other async contexts or from an async runtime entry point. Fix: mark the enclosing function `async` or use a blocking adapter.
- **See Also:** KAIN-EFFECT-0007

#### KAIN-EFFECT-0007 — Await Outside Async
- **Severity:** error
- **Category:** effects/await-outside-async
- **Help:** `await` was used in a function that is not marked `async`. Only async functions can suspend at await points. Fix: add the `async` annotation to the enclosing function.
- **Example Bad:** `fn fetch: {\n    await http.get("...")\n}`
- **Example Good:** `async fn fetch: {\n    await http.get("...")\n}`

#### KAIN-EFFECT-0008 — GPU Effect In Host Context
- **Severity:** error
- **Category:** effects/gpu-in-host
- **Help:** A function annotated `GPU` (or containing GPU intrinsics) is being called from a host-only context without GPU dispatch capability. Fix: wrap the call in a `shader` or `compute` block, or use a GPU dispatch primitive.
- **See Also:** KAIN-SHADER-0001

#### KAIN-EFFECT-0009 — Reactive Cycle Detected
- **Severity:** error
- **Category:** effects/reactive-cycle
- **Help:** Reactive state dependencies form a cycle. The reactive runtime cannot resolve circular update chains. Fix: break the cycle by introducing an explicit delay, a `decay` transition, or restructuring to a directed acyclic graph.
- **See Also:** KAIN-STATE-0003

#### KAIN-EFFECT-0010 — Unsafe Block Not Allowed
- **Severity:** error
- **Category:** effects/unsafe-disallowed
- **Help:** An `Unsafe` block or function is prohibited by the current compilation profile or security policy. Some Kain targets (e.g., web, sandboxed runtimes) disallow unsafe operations entirely. Fix: remove the unsafe block, or change the compilation target.

#### KAIN-EFFECT-0011 — Effect Leakage Through Public API
- **Severity:** warning
- **Category:** effects/public-leakage
- **Help:** A `pub` function exposes an effect annotation that may be an implementation detail. Public APIs should use effect polymorphism or explicitly document the required capabilities. Fix: consider making the function effect-polymorphic or hiding the effect behind a trait abstraction.

#### KAIN-EFFECT-0012 — Conflicting Effect Annotations
- **Severity:** error
- **Category:** effects/conflicting
- **Help:** Multiple effect annotations on the same item conflict (e.g., marking a function both `Pure` and `IO`). Fix: choose the single correct annotation. If the function does both pure computation and IO, it is `IO` (IO subsumes pure computation).

---

### entangle

Bidirectional world-state coupling: entangle syntax, single_writer policy, cycle detection, dangling references, cross-world scoping, type and direction mismatches. `entangle A.field <-> B.field_copy with single_writer` creates a compiler-owned observer graph edge.

#### KAIN-ENTANGLE-0001 — Entangle Error
- **Severity:** error
- **Category:** entangle/general
- **Help:** An `entangle` declaration has a well-formedness error. This is the generic fallback.
- **See Also:** KAIN-ENTANGLE-0002, KAIN-ENTANGLE-0003

#### KAIN-ENTANGLE-0002 — Entangle Cycle Detected
- **Severity:** error
- **Category:** entangle/cycle
- **Help:** The set of `entangle` declarations forms a directed cycle. The compiler cannot establish a stable propagation order when state changes would propagate infinitely between worlds. Fix: break the cycle by making at least one edge directional (one-way update) or by introducing an explicit intermediary.
- **See Also:** KAIN-WORLD-0007

#### KAIN-ENTANGLE-0003 — Entangle Single Writer Violation
- **Severity:** error
- **Category:** entangle/single-writer
- **Help:** An `entangle` link declared `with single_writer` has more than one potential writer. Only the designated writer world may mutate the coupled state. Fix: identify which world owns the write and remove or guard the other write site. Use `patch` to make the mutation explicit and transactional.
- **Example Bad:** `entangle Authority.count <-> Mirror.count_copy with single_writer\n// ... later: Mirror.count_copy = 42  // second writer — error`
- **Example Good:** `entangle Authority.count <-> Mirror.count_copy with single_writer\n// Mirror.count_copy is read-only; only Authority writes count`
- **See Also:** KAIN-BORROW-0006

#### KAIN-ENTANGLE-0004 — Entangle Dangling Reference
- **Severity:** error
- **Category:** entangle/dangling
- **Help:** One or both worlds referenced by an `entangle` declaration do not exist or have been removed. Entangle links must point to live world state slots. Fix: ensure both worlds are declared and their state fields exist.
- **See Also:** KAIN-WORLD-0005

#### KAIN-ENTANGLE-0005 — Entangle Cross-World Scope Error
- **Severity:** error
- **Category:** entangle/cross-world-scope
- **Help:** An `entangle` declaration references a state field from a world that is not in scope at the entangle site. Top-level entangle declarations must be at module scope and both worlds must be visible. Fix: move the entangle declaration to module scope, or import both worlds.

#### KAIN-ENTANGLE-0006 — Entangle Type Mismatch
- **Severity:** error
- **Category:** entangle/type-mismatch
- **Help:** The types of the two coupled state fields are not compatible for synchronization. Entangle requires the fields to have the same type or an explicit coercion annotation. Fix: ensure both fields have the same type, or add a conversion function to the entangle declaration.
- **See Also:** KAIN-TYPE-0025

#### KAIN-ENTANGLE-0007 — Entangle Direction Conflict
- **Severity:** error
- **Category:** entangle/direction-conflict
- **Help:** Two `entangle` declarations couple the same pair of fields in conflicting directions. Only one coupling direction may be declared per field pair. Fix: use a single bidirectional `<->` link, or split into two distinct one-way links with explicit direction annotations.

---

### internal

Compiler bugs, impossible states, and unclassified hard failures.

#### KAIN-INTERNAL-0001 — Internal Error
- **Severity:** error
- **Category:** internal/general
- **Help:** The compiler or runtime hit an internal invariant failure. This is not a normal user-authored language error; it usually means the toolchain hit an unsupported state, stale assumption, or outright bug. Fix: capture the repro, the active pass, and the surrounding diagnostic context so the owning subsystem can patch the invariant.

---

### io

File I/O, network, asset loading, filesystem operations.

#### KAIN-IO-0001 — IO Error
- **Severity:** error
- **Category:** io/general
- **Help:** A filesystem or network I/O operation failed.

#### KAIN-IO-0002 — File Not Found
- **Severity:** error
- **Category:** io/file-not-found
- **Help:** The specified file does not exist or is not accessible. Check the file path, permissions, and working directory. Fix: verify the file path and ensure the file exists.

#### KAIN-IO-0003 — File Read Error
- **Severity:** error
- **Category:** io/read-error
- **Help:** A file could not be read. Possible causes: permission denied, file locked by another process, or filesystem corruption. Fix: check file permissions and that the file is not locked.

#### KAIN-IO-0004 — File Write Error
- **Severity:** error
- **Category:** io/write-error
- **Help:** A file could not be written. Possible causes: disk full, permission denied, or the parent directory does not exist. Fix: check disk space, permissions, and parent directory existence.

#### KAIN-IO-0005 — Network Request Failed
- **Severity:** error
- **Category:** io/network-failed
- **Help:** A network operation (HTTP, WebSocket, etc.) failed. The remote host may be unreachable, the request may have timed out, or the response may be malformed. Fix: check network connectivity and the request configuration.

#### KAIN-IO-0006 — Asset Import Failed
- **Severity:** error
- **Category:** io/asset-import
- **Help:** An asset (texture, mesh, sound, data file) could not be imported. The file format may be unsupported, the asset may be corrupt, or the importer may have encountered an error. Fix: check the asset file format and integrity.

---

### memory

Memory layout, pointers, bitfields, alignment, address spaces.

#### KAIN-MEM-0001 — Memory Lowering Required
- **Severity:** error
- **Category:** memory/lowering
- **Help:** The code uses raw memory semantics (pointers, bitfields, etc.) but no lowering policy has been selected for the target backend. Fix: add a lowering policy or select a backend with native memory support.

#### KAIN-MEM-0002 — Memory Semantics Unsupported By Backend
- **Severity:** error
- **Category:** memory/backend-capabilities
- **Help:** The backend does not support the memory operations used in this code (e.g., raw pointers on a GPU target, or bitfield addressing on WASM). Fix: choose a compatible backend or lower the memory operations.
- **See Also:** KAIN-MEM-0001

#### KAIN-MEM-0003 — Illegal Bitfield Address
- **Severity:** error
- **Category:** memory/bitfields
- **Help:** Taking the address of a C-compatible bitfield is not allowed. Bitfields may not occupy whole bytes and do not have stable addresses. Fix: lower the bitfield access into load/store/mask operations.

#### KAIN-MEM-0004 — Memory Layout Overflow
- **Severity:** error
- **Category:** memory/layout-overflow
- **Help:** The computed memory layout for a type exceeds the maximum representable size or alignment for the target address space (32-bit vs 64-bit). Fix: reduce the aggregate size, split the layout, or use a 64-bit target.

#### KAIN-MEM-0005 — Alignment Requirement Not Satisfied
- **Severity:** error
- **Category:** memory/alignment
- **Help:** A value requires a stricter alignment than the allocated memory provides. This can cause undefined behavior on some platforms. Fix: increase the allocation alignment or use an unaligned access primitive if the platform supports it.

#### KAIN-MEM-0006 — Null Pointer Dereference
- **Severity:** error
- **Category:** memory/null-deref
- **Help:** A pointer that may be null is being dereferenced without a null check. Kain requires explicit null guards for nullable pointers. Fix: wrap the dereference in an `if ptr != none:` block or use the `?.` safe-access operator.
- **Example Bad:** `let value = *ptr`
- **Example Good:** `let value = ptr?.*value`

#### KAIN-MEM-0007 — Out Of Bounds Access
- **Severity:** error
- **Category:** memory/out-of-bounds
- **Help:** An array or buffer access uses an index that is not provably within bounds. Kain requires bounds checks or proofs for all indexed access. Fix: add a bounds check before the access or use an iterator.
- **Example Bad:** `let x = arr[100]`
- **Example Good:** `if arr.len() > 100: let x = arr[100]`

#### KAIN-MEM-0008 — Address Space Mismatch
- **Severity:** error
- **Category:** memory/address-space
- **Help:** A pointer in one address space (e.g., GPU global memory) is being used in a context that expects a different address space (e.g., GPU shared memory). Fix: use the correct address space qualifier or copy data to the expected space.

---

### patch

Transactional world mutation: patch target validation, law precondition/postcondition checking, out-of-scope application, conflicting mutations, and return type mismatches. `patch update(target: World, v: T) -> T` applies a validated mutation to a world's state. `law` predicates guard every patch site.

#### KAIN-PATCH-0001 — Patch Error
- **Severity:** error
- **Category:** patch/general
- **Help:** A `patch` or `law` declaration has a well-formedness error. This is the generic fallback.
- **See Also:** KAIN-PATCH-0002, KAIN-PATCH-0003

#### KAIN-PATCH-0002 — Patch Target Is Not A World
- **Severity:** error
- **Category:** patch/target-not-world
- **Help:** The first parameter of a `patch` block must be a `world` type. `patch` is designed for transactional mutation of compiler-owned world state and cannot be applied to arbitrary structs or scalars. Fix: change the patch target to a declared `world`, or use a regular function for non-world mutation.
- **Example Bad:** `patch update(target: MyStruct, v: Int) -> Int:\n    target.count = v\n    return target.count`
- **Example Good:** `patch update(target: Authority, v: Int) -> Int:\n    target.count = v\n    return target.count`

#### KAIN-PATCH-0003 — Patch Law Precondition Failed
- **Severity:** error
- **Category:** patch/law-precondition
- **Help:** A `law` predicate guarding this `patch` site failed its precondition check. The proposed mutation is not valid given the current world state. Fix: ensure the value being patched satisfies all law predicates, or adjust the law to match the intended invariant.
- **Example Bad:** `law value_in_range(v: Int) -> Bool:\n    return v >= 0 and v < 1000000007\n\npatch update(target: Authority, v: Int) -> Int:\n    target.count = v  // v = -1 violates law`
- **Example Good:** `patch update(target: Authority, v: Int) -> Int:\n    assert value_in_range(v)\n    target.count = v\n    return target.count`
- **See Also:** KAIN-PATCH-0004, KAIN-COMPTIME-0006

#### KAIN-PATCH-0004 — Patch Law Postcondition Failed
- **Severity:** error
- **Category:** patch/law-postcondition
- **Help:** After applying the `patch`, a `law` postcondition is not satisfied by the resulting world state. The patch body must leave the world in a state that satisfies all invariants. Fix: adjust the patch body to restore invariant compliance before returning, or strengthen the postcondition law.
- **See Also:** KAIN-PATCH-0003

#### KAIN-PATCH-0005 — Patch Applied Outside World Scope
- **Severity:** error
- **Category:** patch/outside-scope
- **Help:** A `patch` block was invoked in a context where the target world is not in scope or where world mutation is not permitted (e.g., inside a pure function, a shader, or a GPU compute kernel). Fix: invoke the patch from a world-aware context (a `pulse` handler, a top-level function with IO/Reactive effect, or an actor `on` handler).
- **See Also:** KAIN-EFFECT-0004, KAIN-SHADER-0001

#### KAIN-PATCH-0006 — Conflicting Patch Mutations
- **Severity:** error
- **Category:** patch/conflicting-mutations
- **Help:** Two or more `patch` applications target the same world state field concurrently and their mutations are not ordered or serialized. The resulting world state is non-deterministic. Fix: serialize patches through a single actor or use a transactional ordering annotation.
- **See Also:** KAIN-BORROW-0006, KAIN-ENTANGLE-0003

#### KAIN-PATCH-0007 — Patch Law Return Type Mismatch
- **Severity:** error
- **Category:** patch/law-return-type
- **Help:** A `law` predicate used with a `patch` must return `Bool`. The declared or inferred return type is not `Bool`. Fix: change the law body to evaluate to a boolean expression and update the return type annotation.
- **Example Bad:** `law value_ok(v: Int) -> Int:\n    return v + 1`
- **Example Good:** `law value_ok(v: Int) -> Bool:\n    return v > 0`
- **See Also:** KAIN-TYPE-0025

---

### runtime

Runtime errors: dispatch, actor messaging, resource exhaustion.

#### KAIN-RUNTIME-0001 — Runtime Error
- **Severity:** error
- **Category:** runtime/general
- **Help:** A runtime invariant has been violated.

#### KAIN-RUNTIME-0002 — Actor Panic
- **Severity:** error
- **Category:** runtime/actor-panic
- **Help:** An actor or component panicked at runtime. The panic may be caused by an assertion failure, an unhandled message, or a resource error. Fix: check the panic message and trace to identify the root cause.

#### KAIN-RUNTIME-0003 — Message Delivery Failed
- **Severity:** error
- **Category:** runtime/message-delivery
- **Help:** A message could not be delivered to its target actor. The target may have been destroyed, its mailbox may be full, or the message type may not be accepted. Fix: check the target actor's lifecycle and message handling.
- **See Also:** KAIN-ACTOR-0003

#### KAIN-RUNTIME-0004 — Resource Exhausted
- **Severity:** error
- **Category:** runtime/resource-exhausted
- **Help:** A runtime resource (memory, file handles, GPU memory, actor capacity) has been exhausted. The program cannot continue. Fix: reduce resource consumption, increase limits, or add backpressure.

#### KAIN-RUNTIME-0005 — Deadlock Detected
- **Severity:** error
- **Category:** runtime/deadlock
- **Help:** The runtime detected a deadlock — two or more actors are waiting on each other in a cycle, and no progress can be made. Fix: restructure the message flow to avoid circular waits, or use timeouts.

#### KAIN-RUNTIME-0006 — World Initialization Failed
- **Severity:** error
- **Category:** runtime/world-init
- **Help:** A `world` could not be initialized. Surface creation, component registration, or resource allocation may have failed. Fix: check the world configuration and surface backend availability.

#### KAIN-RUNTIME-0007 — Shader Dispatch Failed
- **Severity:** error
- **Category:** runtime/shader-dispatch
- **Help:** A GPU shader dispatch command failed. The GPU may be unavailable, the shader may have crashed, or dispatch parameters may be invalid. Fix: check GPU availability, shader validity, and dispatch dimensions.

#### KAIN-RUNTIME-0008 — Timeout Exceeded
- **Severity:** error
- **Category:** runtime/timeout
- **Help:** An operation exceeded its time budget and was cancelled. This applies to async operations, actor message waits, and compute dispatches. Fix: increase the timeout budget or optimize the operation.

---

### shader

Shader blocks, compute kernels, vertex/fragment stages, GPU intrinsics.

#### KAIN-SHADER-0001 — Unsupported Shader Call
- **Severity:** error
- **Category:** shader/unsupported-call
- **Help:** A function or intrinsic is not available in the current shader stage. Different shader stages (vertex, fragment, compute) expose different intrinsic sets. Fix: replace the call with a supported shader intrinsic or move the computation to a compatible stage.
- **See Also:** KAIN-SHADER-0002

#### KAIN-SHADER-0002 — Shader Stage Mismatch
- **Severity:** error
- **Category:** shader/stage-mismatch
- **Help:** A value or operation that is only valid in one shader stage is being used in a different stage. For example, vertex inputs cannot be accessed from a fragment shader directly. Fix: pass data through the appropriate stage interface (varying parameters, uniform buffers, or shared memory).

#### KAIN-SHADER-0003 — Uniform Binding Error
- **Severity:** error
- **Category:** shader/uniform-binding
- **Help:** A `uniform` declaration cannot be bound — the name conflicts with an existing binding, the type is not GPU-compatible, or the binding slot is already occupied. Fix: use a unique binding name, ensure the type is GPU-compatible, and check for slot conflicts.

#### KAIN-SHADER-0004 — Compute Dispatch Dimension Error
- **Severity:** error
- **Category:** shader/compute-dispatch
- **Help:** A `compute` block specifies invalid dispatch dimensions. Compute shaders require explicit thread group dimensions that must be positive and within platform limits. Fix: provide valid thread group counts within the platform's maxComputeWorkGroupSize limits.

#### KAIN-SHADER-0005 — Shader Resource Not GPU-Compatible
- **Severity:** error
- **Category:** shader/resource-compat
- **Help:** A resource (texture, buffer, sampler) used in a shader has a type or format that is not supported by the GPU backend. Fix: use a GPU-compatible format or convert the resource before binding.

#### KAIN-SHADER-0006 — Vertex Input Layout Error
- **Severity:** error
- **Category:** shader/vertex-input
- **Help:** A `vertex` shader's input layout does not match the mesh or buffer providing vertex data. Input attributes must agree in type, offset, and count. Fix: align the vertex input declaration with the mesh data layout.

#### KAIN-SHADER-0007 — Fragment Output Layout Error
- **Severity:** error
- **Category:** shader/fragment-output
- **Help:** A `fragment` shader's output does not match the render target format. Output color/depth attachments must have compatible pixel formats. Fix: match the fragment output type to the render target configuration.

#### KAIN-SHADER-0008 — Collapse Target Invalid
- **Severity:** error
- **Category:** shader/collapse-invalid
- **Help:** `collapse` reduces a parallel computation into a scalar, but the reduction target or operator is not valid for the current shader model. Fix: ensure the reduction operator is supported and the target type is scalar-compatible.

#### KAIN-SHADER-0009 — Fanout Width Exceeded
- **Severity:** error
- **Category:** shader/fanout-width
- **Help:** `fanout` distributes work across parallel lanes, but the fanout width exceeds the GPU's wavefront/warp size or thread group limit. Fix: reduce the fanout width or split across multiple waves.

#### KAIN-SHADER-0010 — Shader Compilation Failed
- **Severity:** error
- **Category:** shader/compilation
- **Help:** The shader backend (HLSL/SPIR-V/Metal) could not compile the generated shader code. This usually indicates the Kain lowering produced invalid target code — check the shader output log for details. Fix: simplify the shader code, check for target-specific restrictions, or report the issue with the generated shader source.

#### KAIN-SHADER-0011 — GPU Memory Budget Exceeded
- **Severity:** error
- **Category:** shader/memory-budget
- **Help:** The shader uses more GPU memory (registers, shared memory, constant buffers) than the target hardware allows. Fix: reduce register pressure by simplifying the shader, splitting into multiple passes, or lowering the occupancy target.

#### KAIN-SHADER-0012 — Shared Memory Bank Conflict
- **Severity:** warning
- **Category:** shader/bank-conflict
- **Help:** `share` memory access pattern may cause GPU shared memory bank conflicts, reducing throughput. Reorganize data layout to avoid concurrent access to the same bank. Fix: pad shared memory arrays or restructure access patterns.

---

### state

State machine errors: state, every, when, guarantee, fallback, pulse.

#### KAIN-STATE-0001 — State Error
- **Severity:** error
- **Category:** state/general
- **Help:** A state machine well-formedness rule has been violated.

#### KAIN-STATE-0002 — State Machine Inexhaustive
- **Severity:** error
- **Category:** state/inexhaustive
- **Help:** A `state` machine does not handle all possible transitions. Every state must have a defined transition for every possible input event, or a `fallback` handler must be present. Fix: add missing `when` clauses or a `fallback` handler.
- **See Also:** KAIN-TYPE-0013

#### KAIN-STATE-0003 — State Transition Cycle
- **Severity:** error
- **Category:** state/cycle
- **Help:** State transitions form a directed cycle without an escape path. The state machine may loop indefinitely — every cycle should have a reachable terminal state or a `decay` path. Fix: add an exit condition, a `guarantee` of termination, or a decay transition out of the cycle.
- **See Also:** KAIN-EFFECT-0009

#### KAIN-STATE-0004 — Invalid State Transition
- **Severity:** error
- **Category:** state/invalid-transition
- **Help:** A `when` clause references a target state that does not exist in the state machine definition. Fix: ensure the target state is declared.
- **See Also:** KAIN-STATE-0002

#### KAIN-STATE-0005 — Pulse Without State
- **Severity:** error
- **Category:** state/pulse-no-state
- **Help:** `pulse` triggers a state machine event, but the target component does not have an active state machine or the state machine does not handle the pulsed event. Fix: ensure the target has a state machine that handles the event.

#### KAIN-STATE-0006 — Guarantee Violation
- **Severity:** error
- **Category:** state/guarantee-violation
- **Help:** A `guarantee` clause asserts a property that does not hold. Guarantees are verified statically, and this one cannot be proven by the compiler. Fix: strengthen the precondition or weaken the guarantee, or restructure the code to make the property provable.

#### KAIN-STATE-0007 — Every Clause Unbounded
- **Severity:** warning
- **Category:** state/every-unbounded
- **Help:** An `every` clause defines a periodic behavior without an upper bound or termination condition. This may run indefinitely. Fix: add a termination guard or a bound on the iteration count.

#### KAIN-STATE-0008 — Fallback Handler Unreachable
- **Severity:** warning
- **Category:** state/fallback-unreachable
- **Help:** A `fallback` handler is declared but all possible events are already explicitly handled by `when` clauses. The fallback is dead code. Fix: remove the unnecessary fallback or ensure it covers a real case.

---

### test

Test and specification framework: test, spec, fast, verify, random, jitter.

#### KAIN-TEST-0001 — Test Error
- **Severity:** error
- **Category:** test/general
- **Help:** A test or specification framework invariant has been violated.

#### KAIN-TEST-0002 — Assertion Failed
- **Severity:** error
- **Category:** test/assertion-failed
- **Help:** An `assert` expression evaluated to `false` inside a `test` or `spec` block. The condition is not satisfied for the given inputs. Fix: correct the code or adjust the test expectation.

#### KAIN-TEST-0003 — Spec Property Violated
- **Severity:** error
- **Category:** test/spec-violated
- **Help:** A `spec` block defines a property that does not hold. The property was falsified by a counterexample found through `random` or `jitter` testing. Fix: correct the implementation or narrow the spec to match the intended behavior.

#### KAIN-TEST-0004 — Fast Test Exceeded Time Budget
- **Severity:** warning
- **Category:** test/fast-timeout
- **Help:** A `fast` test exceeded its time budget. Fast tests should complete in under 1ms — this test may be too heavy for the fast suite. Fix: move the test to the standard suite or optimize the test body.

#### KAIN-TEST-0005 — Verify Block Infallible
- **Severity:** warning
- **Category:** test/verify-infallible
- **Help:** A `verify` block can never fail — its condition is always true. This may indicate a missing check or an over-constrained spec. Fix: check that the verify condition is actually testing something meaningful.

#### KAIN-TEST-0006 — Random Seed Not Reproducible
- **Severity:** warning
- **Category:** test/seed-missing
- **Help:** `random` testing is used without an explicit seed. Failing runs may not be reproducible. Add a seed for deterministic replay. Fix: set a seed value or use `--test-seed` to capture the failing seed.

#### KAIN-TEST-0007 — Jitter Range Invalid
- **Severity:** error
- **Category:** test/jitter-range
- **Help:** `jitter` specifies an invalid timing perturbation range. Jitter bounds must be non-negative and within the test's timing tolerance. Fix: adjust the jitter range to valid values.

---

### type

Type-checking, name resolution, trait solving, and unification errors. This is the largest category.

#### KAIN-TYPE-0001 — Type Error
- **Severity:** error
- **Category:** types/general
- **Help:** A type mismatch or type-inference failure occurred. This is the generic fallback for type errors.
- **See Also:** KAIN-TYPE-0002

#### KAIN-TYPE-0002 — Unknown Identifier
- **Severity:** error
- **Category:** types/unknown-identifier
- **Help:** The name is not visible in the current scope. Common causes: misspelling of a variable, function, or type name; missing `use` import; the symbol exists only on the host/engine side and has not been bridged into Kain via the foreign-ABI layer; the symbol is defined inside a module that has not been imported. Fix: check spelling, add a `use` statement, or bridge the host symbol.
- **See Also:** KAIN-TYPE-0004, KAIN-TYPE-0010

#### KAIN-TYPE-0003 — World Requires Surface
- **Severity:** error
- **Category:** world/missing-surface
- **Help:** A `world` declaration must expose at least one surface projection so the world can map components into a live host presentation surface. Fix: add at least one surface projection such as `surface native_ui => MyPanel` inside the world body.
- **See Also:** KAIN-WORLD-0001

#### KAIN-TYPE-0004 — Duplicate Symbol
- **Severity:** error
- **Category:** types/duplicate-symbol
- **Help:** The same name has been defined more than once in the same namespace. Kain requires each visible symbol to have a unique name within its scope. Fix: rename one declaration or use an explicit alias on import.
- **See Also:** KAIN-TYPE-0005

#### KAIN-TYPE-0005 — Builtin Symbol Shadowed
- **Severity:** warning
- **Category:** types/shadowed-builtin
- **Help:** A user-defined name shadows a Kain builtin symbol. While allowed, this can cause confusion — the builtin is no longer accessible under its original name in this scope. Fix: choose a distinct local name, or import the builtin under an alias.

#### KAIN-TYPE-0006 — Missing Type Annotation
- **Severity:** error
- **Category:** types/missing-annotation
- **Help:** Kain requires explicit type annotations in positions where the type cannot be inferred from context (top-level declarations, function parameters without defaults, trait associated types in some positions). Fix: add an explicit type annotation (`: TypeName`).
- **Example Bad:** `let x`
- **Example Good:** `let x: i32 = 5`
- **See Also:** KAIN-TYPE-0003

#### KAIN-TYPE-0007 — Trait Not Satisfied
- **Severity:** error
- **Category:** types/trait-not-satisfied
- **Help:** A type does not implement a required trait. Traits in Kain define capability contracts that types must fulfill before they can be used in generic contexts, effect-polymorphic functions, or GPU dispatch. Fix: implement the missing trait methods for the type, or add a `derive` annotation if the trait is derivable.
- **See Also:** KAIN-TYPE-0008, KAIN-TYPE-0016

#### KAIN-TYPE-0008 — Trait Method Missing
- **Severity:** error
- **Category:** types/trait-method-missing
- **Help:** An `impl` block for a trait is missing one or more required methods. Every method declared in the trait definition must have a concrete implementation. Fix: add the missing method(s) to the impl block.
- **See Also:** KAIN-TYPE-0007

#### KAIN-TYPE-0009 — Ambiguous Trait Implementation
- **Severity:** error
- **Category:** types/ambiguous-trait
- **Help:** Multiple trait implementations could satisfy a trait bound, and the compiler cannot choose between them. This is the "coherence" problem. Fix: use a fully-qualified path or add a type annotation to disambiguate.
- **See Also:** KAIN-TYPE-0007

#### KAIN-TYPE-0010 — Unresolved Import
- **Severity:** error
- **Category:** types/unresolved-import
- **Help:** A `use` statement references a path that does not resolve to any known module or symbol. The module may not exist, or the symbol may not be `pub`. Fix: check the module path and visibility of the target symbol.
- **See Also:** KAIN-TYPE-0002

#### KAIN-TYPE-0011 — Cyclic Type Definition
- **Severity:** error
- **Category:** types/cyclic-definition
- **Help:** A type definition refers to itself in a way that would require infinite size. Kain detects cycles in struct fields, enum variants, and type aliases. Fix: break the cycle with indirection (e.g., a pointer or boxed type).

#### KAIN-TYPE-0012 — Mutable/Immutable Conflict
- **Severity:** error
- **Category:** types/mutability-conflict
- **Help:** A value declared `let` (immutable) is being used in a position that requires mutation, or vice versa. Fix: change `let` to `let mut` if mutation is required, or remove the mutation site.
- **Example Bad:** `let x = 5\nx = 10`
- **Example Good:** `let mut x = 5\nx = 10`
- **See Also:** KAIN-BORROW-0003

#### KAIN-TYPE-0013 — Pattern Match Inexhaustive
- **Severity:** error
- **Category:** types/inexhaustive-match
- **Help:** A `match` expression does not cover all possible variants of the matched enum type. Kain requires exhaustive matching unless a wildcard branch (`_`) or a `fallback` clause is present. Fix: add missing variant arms or a wildcard catch-all.
- **See Also:** KAIN-STATE-0002

#### KAIN-TYPE-0014 — Recursive Type Without Indirection
- **Severity:** error
- **Category:** types/recursive-without-indirection
- **Help:** A struct or enum contains itself as a direct field, which would require infinite memory. Use a pointer-like indirection (shared, weak, or a reference) to break the cycle. Fix: wrap the recursive field in a `shared` or heap-allocated container.

#### KAIN-TYPE-0015 — Type Alias Cycle
- **Severity:** error
- **Category:** types/alias-cycle
- **Help:** A `type` alias expands to itself, directly or through a chain of other aliases. Kain resolves aliases eagerly and detects cycles. Fix: break the alias cycle.

#### KAIN-TYPE-0016 — Impl On Foreign Type
- **Severity:** error
- **Category:** types/foreign-impl
- **Help:** An `impl` block implements a trait for a type that is not defined in the current crate. By Kain's coherence rules, you can only implement your own traits for foreign types, or foreign traits for your own types — not both foreign. Fix: create a newtype wrapper or define the trait in your crate.

#### KAIN-TYPE-0017 — Self Type In Static Context
- **Severity:** error
- **Category:** types/self-in-static
- **Help:** `Self` or `self` was used outside of a trait, impl block, or method context where it has no meaning. Fix: use a concrete type name instead of `Self`.

#### KAIN-TYPE-0018 — Invalid Type Parameter Count
- **Severity:** error
- **Category:** types/param-count
- **Help:** A generic type or function was supplied with the wrong number of type arguments. Check the definition for the expected arity. Fix: provide the correct number of type arguments.

#### KAIN-TYPE-0019 — Type Argument Kind Mismatch
- **Severity:** error
- **Category:** types/arg-kind-mismatch
- **Help:** A type argument does not satisfy the kind constraints of the generic parameter. For example, a type parameter constrained by a trait was given a type that does not implement that trait. Fix: ensure the type argument satisfies all bounds.
- **See Also:** KAIN-TYPE-0007

#### KAIN-TYPE-0020 — Return Type Mismatch
- **Severity:** error
- **Category:** types/return-mismatch
- **Help:** The body of a function produces a value whose type does not match the declared return type. Every exit path (including early returns and the implicit tail expression) must agree with the annotation. Fix: correct the return type annotation or the returned value.
- **Example Bad:** `fn answer: i32 { "forty-two" }`
- **Example Good:** `fn answer: i32 { 42 }`

#### KAIN-TYPE-0021 — Missing Return In Non-Void Function
- **Severity:** error
- **Category:** types/missing-return
- **Help:** A function with a non-void return type does not return a value on at least one control-flow path. Every branch must either return, diverge, or produce a tail expression. Fix: add a return statement or ensure the tail expression matches the declared type.

#### KAIN-TYPE-0022 — Void Value Used In Expression
- **Severity:** error
- **Category:** types/void-in-expression
- **Help:** A value of type `void` or `none` is being used in a position that expects a meaningful value (e.g., assigned to a typed variable, passed as a non-void argument). Fix: remove the void-producing expression or wrap it in a block that returns a meaningful value.

#### KAIN-TYPE-0023 — Callable Type Expected
- **Severity:** error
- **Category:** types/not-callable
- **Help:** A value was used in a function-call position but its type is not callable (not a function, closure, or object with an `invoke` method). Fix: check that the name refers to a function, not a non-callable variable.
- **Example Bad:** `let x = 5\nx()`
- **Example Good:** `let x = fn: { 5 }\nx()`

#### KAIN-TYPE-0024 — Field Not Found
- **Severity:** error
- **Category:** types/field-not-found
- **Help:** The struct or component type does not have a field with the given name. Check the type definition for the correct field names. Fix: use a field that exists on the type, or check for misspelling.
- **See Also:** KAIN-TYPE-0002

#### KAIN-TYPE-0025 — Type Mismatch
- **Severity:** error
- **Category:** types/mismatch
- **Help:** The inferred type does not match the expected type in this position. Kain uses structural typing for most constructs but enforces nominal matching for traits and effect-polymorphic boundaries. Fix: check whether a type annotation is needed, or whether the value needs an explicit conversion (via `as`).
- **Example Bad:** `let x: i32 = "hello"`
- **Example Good:** `let x: i32 = 42`
- **See Also:** KAIN-TYPE-0005, KAIN-TYPE-0012

#### KAIN-TYPE-0026 — Index Not Supported
- **Severity:** error
- **Category:** types/index-not-supported
- **Help:** The type does not support indexing with `[]`. Only arrays, maps, and types that implement the `Index` trait can be indexed. Fix: use a supported container type or implement the `Index` trait.

---

### validation

Cross-pass validation and structural certification failures.

#### KAIN-VALIDATE-0001 — Validation Error
- **Severity:** error
- **Category:** validation/general
- **Help:** A structural validation pass rejected the program. This is the generic validation fallback used when a later pass proves a construct is not well-formed even though parsing and basic typing succeeded. Fix: inspect the attached validation context and repair the violated invariant before lowering or runtime codegen continues.

---

### world

World declarations, surface projections, entanglement, teleportation.

#### KAIN-WORLD-0001 — World Missing Surface
- **Severity:** error
- **Category:** world/missing-surface
- **Help:** A `world` block must contain at least one `surface` projection that maps Kain UI components to a rendering backend (native_ui, viewport3d, web, ue5). Fix: add at least one surface projection inside the world body.
- **Example Bad:** `world MyWorld: {}`
- **Example Good:** `world MyWorld:\n    surface native_ui => MainPanel`

#### KAIN-WORLD-0002 — Duplicate Surface Kind
- **Severity:** error
- **Category:** world/duplicate-surface
- **Help:** Multiple surfaces of the same kind have been declared in one world. Each surface kind may only appear once per world. Fix: merge the surface projections or use separate worlds for different surface instances of the same kind.

#### KAIN-WORLD-0003 — Surface Component Type Error
- **Severity:** error
- **Category:** world/surface-component-type
- **Help:** The type projected onto a surface must be a valid Kain `component` that implements the surface's rendering protocol. The supplied type is either not a component or does not satisfy the required trait bounds. Fix: ensure the projected type is a `component` with appropriate rendering implementations.

#### KAIN-WORLD-0004 — World Orphan
- **Severity:** error
- **Category:** world/orphan
- **Help:** A `world` declaration is not referenced by any entry point, `spawn` site, or host embedding. Unreferenced worlds are dead code. Fix: spawn the world from a `main` function or export it for host consumption.

#### KAIN-WORLD-0005 — Entanglement Target Invalid
- **Severity:** error
- **Category:** world/entanglement-invalid
- **Help:** `entangle` creates a bidirectional link between two components, but the target component does not exist in the world or does not support entanglement. Fix: ensure both components exist in the same world and implement the Entangle trait.
- **See Also:** KAIN-WORLD-0006

#### KAIN-WORLD-0006 — Teleport Destination Invalid
- **Severity:** error
- **Category:** world/teleport-invalid
- **Help:** `teleport` moves a component to a different world or surface, but the destination world/surface does not exist or does not accept the component type. Fix: ensure the destination world is declared and accepts the component type being teleported.

#### KAIN-WORLD-0007 — World Cross-Reference Cycle
- **Severity:** error
- **Category:** world/cross-reference
- **Help:** Two or more worlds reference each other in a way that creates a dependency cycle (e.g., through entanglement or teleport targets). Fix: break the cycle by introducing an intermediary or making one reference directional.

#### KAIN-WORLD-0008 — Surface Kind Platform Mismatch
- **Severity:** error
- **Category:** world/platform-mismatch
- **Help:** A surface kind (e.g., `ue5`, `native_ui`) is not supported on the current compilation target or platform. Surface kinds are target-gated — `ue5` only works on UE5 host targets, `web` only on WASM/web targets. Fix: change the surface kind or the compilation target.

---

## Severity Distribution

| Severity | Count (spec) | Count (undoc) | Total |
|----------|:-----------:|:------------:|:-----:|
| error | 151 | 5 | 156 |
| warning | 13 | 0 | 13 |

## Cross-References

| Family | Count | Related Docs |
|--------|:-----:|-------------|
| Ownership / Borrow | 10 | `OWNERSHIP.MD` |
| Effects | 12 | `EFFECTS.MD` |
| Components / Actors | 8 | `COMPONENT.MD` |
| World / Surface | 8 | `LAYER1-STATE.MD` |
| Shader / GPU | 12 | `SHADER.MD` |
| Type System | 26 | Typechecker in `crates/core/src/types.rs` |

## Undocumented Errors (from compiler source)

These errors exist in the compiler's `DiagnosticCode` enum (`crates/error/src/code.rs`) and have real checks in `crates/core/src/types.rs`, but are **not documented** in `KAIN_ERROR_SPECS.md`. They represent live compile-time validation that the error spec has not yet captured.

### shader (extended)

#### KAIN-SHADER-0042 — Subgroup Nested
- **Severity:** error
- **Category:** shader/subgroup-nested
- **Source:** `crates/core/src/types.rs:8430-8436` — `validate_subgroup_divergence()`
- **Help:** A `subgroup` block cannot be nested inside another subgroup. Each subgroup represents a warp/wavefront-level synchronization primitive, and nesting them creates undefined behavior. The compiler detects this at parse validation time.
- **Message:** `"subgroup cannot be nested inside another subgroup"`

#### KAIN-SHADER-0043 — Subgroup Divergent Escape
- **Severity:** error
- **Category:** shader/subgroup-divergent-escape
- **Source:** `crates/core/src/types.rs:8438-8452` — `validate_subgroup_divergence()`
- **Help:** `return`, `break`, or `continue` inside a `subgroup` block would cause divergent warp execution. In GPU execution models, all threads in a warp/wavefront must follow the same control flow path. Escaping from a subgroup while other threads continue would diverge execution and produce undefined results.
- **Message:** `"return inside subgroup would cause divergent warp execution"` or `"break/continue inside subgroup outside of a loop would cause divergent warp execution"`
- **Note:** `break`/`continue` are allowed inside a `subgroup` block if they are within an enclosing loop, since the loop ensures uniform convergence at the loop boundary.

### pulse (new category — compile-time budget constraints)

These three codes enforce real-time safety constraints declared as `pulse budget(alloc=N, lock=N, io=N)` on `pulse` blocks. The typechecker walks the pulse body at compile time and counts prohibited operations, emitting these errors when a budget is exceeded or when the limit is zero.

**Source:** `crates/core/src/types.rs:6418-6509` — `check_call_budget()`, `check_lock_budget()`, `check_method_call_budget()`

#### KAIN-PULSE-BUDGET-0001 — Pulse Budget Alloc Violation
- **Severity:** error
- **Category:** pulse/budget-alloc
- **Help:** An allocation operation was detected inside a `pulse` block that has `budget(alloc=0)` (or a limit that is exceeded). Pulse callbacks run on a real-time thread or at a timed recurrence — allocations can cause jitter and must be moved to initialization.
- **Message:** `"allocation in pulse with budget(alloc=0): '<name>' in pulse '<pulse_name>'\n  = note: pulse '<name>' budget(alloc=<limit>) forbids allocations\n  = help: pre-allocate buffers before the pulse starts, or increase budget"`
- **Detected operations:** `alloc`, `realloc`, `alloc_zeroed`, and any function call whose name matches forbidden allocation patterns.

#### KAIN-PULSE-BUDGET-0002 — Pulse Budget Lock Violation
- **Severity:** error
- **Category:** pulse/budget-lock
- **Help:** A locking or ownership operation was detected inside a `pulse` block with `budget(lock=0)` (or limit exceeded). Lock-acquisition can block the pulse thread — move ownership scope management or lock acquisition outside the pulse callback.
- **Message (lock):** `"locking operation in pulse with budget(lock=0): '<name>' in pulse '<pulse_name>'\n  = note: pulse '<name>' budget(lock=<limit>) forbids locking operations\n  = help: move lock acquisition outside the pulse body, or increase budget"`
- **Message (ownership):** `"ownership operation in pulse with budget(lock=0): '<name>' in pulse '<pulse_name>'\n  = note: pulse '<name>' budget(lock=<limit>) forbids ownership operations\n  = help: pre-allocate buffers on the main thread and use non-owning views"`
- **Detected operations:** `collapse`, `observe`, `decay`, mutex/semaphore operations, and any function call whose name matches locking patterns.

#### KAIN-PULSE-BUDGET-0003 — Pulse Budget IO Violation
- **Severity:** error
- **Category:** pulse/budget-io
- **Help:** An I/O operation was detected inside a `pulse` block with `budget(io=0)` (or limit exceeded). I/O operations (file reads, network calls, console output) have unpredictable latency and must be moved to initialization or an async context.
- **Message:** `"I/O operation in pulse with budget(io=0): '<name>' in pulse '<pulse_name>'\n  = note: pulse '<name>' budget(io=<limit>) forbids I/O operations\n  = help: perform I/O during initialization, not in the callback, or increase budget"`
- **Detected operations:** `print`, `println`, `fs_read*`, `fs_write*`, network calls, and any function call whose name matches I/O patterns.

### Audit Summary

| Code | Status | Defined In | Checked In |
|------|--------|-----------|-----------|
| KAIN-SHADER-0042 | Missing from spec | `crates/error/src/code.rs:107` | `crates/core/src/types.rs:8431` |
| KAIN-SHADER-0043 | Missing from spec | `crates/error/src/code.rs:108` | `crates/core/src/types.rs:8439` |
| KAIN-PULSE-BUDGET-0001 | Missing from spec | `crates/error/src/code.rs:218` | `crates/core/src/types.rs:6419` |
| KAIN-PULSE-BUDGET-0002 | Missing from spec | `crates/error/src/code.rs:219` | `crates/core/src/types.rs:6440,6505` |
| KAIN-PULSE-BUDGET-0003 | Missing from spec | `crates/error/src/code.rs:220` | `crates/core/src/types.rs:6461` |

## See Also

- `KAIN_ERROR_SPECS.md` — full error spec with all 184 errors (including parse)
- `KAIN_ERROR_PARSE.md` — parse-only error reference (separate document)
- `docs-tsv/typechecker_errors.tsv` — machine-readable TSV version of this document (with `spec=yes|no` column)
- `crates/error/src/code.rs` — canonical `DiagnosticCode` enum (all known codes)
- `crates/core/src/types.rs` — typechecker source of truth
- `crates/core/src/effects.rs` — effect system (`can_call()`)
- `crates/ownership/src/lib.rs` — ownership state machine
