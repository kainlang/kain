# Kain Error Specification — Full Reference

## borrow
```toml
# ── Kain Borrow Error Codes ────────────────────────────────────────────
# Ownership, borrowing, single_writer, weak references, shared state.

[[diagnostics]]
code = "KAIN-BORROW-0001"
title = "Borrow Error"
severity = "error"
docs_key = "borrow/general"
help = """
A borrow-checking rule has been violated. Kain's ownership system
enforces single-writer or multiple-reader semantics on shared state.
"""
see_also = ["KAIN-BORROW-0002", "KAIN-BORROW-0003"]

[[diagnostics]]
code = "KAIN-BORROW-0002"
title = "Multiple Mutable Borrows"
severity = "error"
docs_key = "borrow/multiple-mutable"
help = """
A mutable reference to shared state overlaps with another active mutable
or immutable reference. Kain enforces `single_writer` semantics — only
one writer OR multiple readers may be active at a time.

Fix: restructure the code so borrows do not overlap, or use an explicit
scope to shorten one of the borrow lifetimes.
"""
example_bad = "let mut x = 0\nlet a = &mut x\nlet b = &mut x"
example_good = "let mut x = 0\n{\n    let a = &mut x\n}\nlet b = &mut x"

[[diagnostics]]
code = "KAIN-BORROW-0003"
title = "Borrow And Mutation Conflict"
severity = "error"
docs_key = "borrow/borrow-mutation-conflict"
help = """
A value is borrowed (either mutably or immutably) while a mutation
occurs through another path. This violates the single-writer invariant.

Fix: complete all borrows before mutating, or clone the value.
"""
see_also = ["KAIN-BORROW-0002"]

[[diagnostics]]
code = "KAIN-BORROW-0004"
title = "Use After Move"
severity = "error"
docs_key = "borrow/use-after-move"
help = """
A value has been moved (ownership transferred) and is used afterwards.
By default, Kain moves values into function arguments, assignments, and
return positions.

Fix: clone the value before the move, or restructure to borrow instead.
"""
example_bad = "let x = [1, 2, 3]\nlet y = x\nprint(x)"
example_good = "let x = [1, 2, 3]\nlet y = x.clone()\nprint(x)"

[[diagnostics]]
code = "KAIN-BORROW-0005"
title = "Shared State Without Annotation"
severity = "error"
docs_key = "borrow/missing-shared-annotation"
help = """
State that is accessed from multiple actors, components, or threads must
be explicitly annotated with `shared` or `single_writer`. The compiler
detected cross-actor access without the required annotation.

Fix: add the `shared` keyword to the state declaration, or use
appropriate synchronization primitives.
"""
see_also = ["KAIN-BORROW-0006"]

[[diagnostics]]
code = "KAIN-BORROW-0006"
title = "Single Writer Violation"
severity = "error"
docs_key = "borrow/single-writer-violation"
help = """
State annotated `single_writer` is being mutated from multiple locations
simultaneously. Only one writer may exist at any time for
single_writer-protected state.

Fix: serialize writes through a single owner, or upgrade to a more
permissive sharing model.
"""
see_also = ["KAIN-BORROW-0005"]

[[diagnostics]]
code = "KAIN-BORROW-0007"
title = "Weak Reference Upgraded Unsafely"
severity = "error"
docs_key = "borrow/weak-upgrade"
help = """
A `weak` reference was upgraded to a strong reference, but the target
has already been dropped. Weak references must check for liveness before
upgrading.

Fix: use the `?` operator or an `if let` pattern when upgrading weak
references.
"""
example_bad = "let strong = weak_ref.upgrade()"
example_good = "if let Some(strong) = weak_ref.upgrade(): ..."

[[diagnostics]]
code = "KAIN-BORROW-0008"
title = "Lifetime Mismatch"
severity = "error"
docs_key = "borrow/lifetime-mismatch"
help = """
A reference outlives the value it points to. Kain tracks lifetimes
implicitly for most references and detected that the borrow outlasts
the owned value.

Fix: restructure to keep the owned value alive longer than its borrows,
or clone the value so the borrow is not needed.
"""
example_bad = """
let ref: &i32
{
    let x = 5
    ref = &x
}
print(ref)
"""
example_good = """
let x = 5
let ref = &x
print(ref)
"""

[[diagnostics]]
code = "KAIN-BORROW-0009"
title = "Send Constraint Violation"
severity = "error"
docs_key = "borrow/send-violation"
help = """
A value is being `send` to another actor or component but its type does
not satisfy the `Send` trait (it contains non-sendable state such as
raw pointers or actor-local references).

Fix: ensure all fields of the sent type satisfy `Send`, or use a
serialization boundary.
"""
see_also = ["KAIN-ACTOR-0003"]

[[diagnostics]]
code = "KAIN-BORROW-0010"
title = "Implicit Clone On Large Value"
severity = "warning"
docs_key = "borrow/large-clone"
help = """
A potentially large value is being implicitly cloned. Consider passing
by reference or using `shared` to avoid the copy.

Fix: use `&value` to borrow, or add an explicit `.clone()` if the copy
is intentional.
"""
```

## codegen
```toml
# ── Kain Codegen Error Codes ───────────────────────────────────────────
# Code generation, lowering, backend compilation, linking.

[[diagnostics]]
code = "KAIN-CODEGEN-0001"
title = "Codegen Error"
severity = "error"
docs_key = "codegen/general"
help = "A code generation or lowering pass failed."
see_also = ["KAIN-CODEGEN-0002"]

[[diagnostics]]
code = "KAIN-CODEGEN-0002"
title = "Unknown Codegen Variable"
severity = "error"
docs_key = "codegen/unknown-variable"
help = """
The lowered backend could not find a value for this symbol. The frontend
may have accepted invalid code, or a lowering pass lost a binding.

Fix: check whether the frontend should have rejected this earlier or
whether the lowering pass dropped the binding.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0003"
title = "Lowering Pass Failed"
severity = "error"
docs_key = "codegen/lowering-failed"
help = """
An IR lowering pass could not transform the intermediate representation.
This typically occurs when a Kain construct has no direct lowering path
to the target backend.

Fix: simplify the construct or add a lowering rule for the target.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0004"
title = "Backend Compilation Failed"
severity = "error"
docs_key = "codegen/backend-failed"
help = """
The native code backend (LLVM, C, or target-specific compiler) could not
compile the generated code. Check the backend compiler output for
specifics.

Fix: check target compatibility, reduce optimization level, or simplify
the generated code.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0005"
title = "Linking Failed"
severity = "error"
docs_key = "codegen/linking-failed"
help = """
The linker could not resolve all symbols or encountered a conflict.
This may indicate missing libraries, duplicate symbols, or
target-incompatible object files.

Fix: check library paths, symbol names, and target architecture.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0006"
title = "Unsupported Target Architecture"
severity = "error"
docs_key = "codegen/unsupported-target"
help = """
The specified compilation target (CPU architecture, OS, GPU, or runtime)
is not supported by the current Kain backend configuration.

Fix: choose a supported target or enable the required backend feature.
"""
see_also = ["KAIN-CODEGEN-0007"]

[[diagnostics]]
code = "KAIN-CODEGEN-0007"
title = "Target Capability Missing"
severity = "error"
docs_key = "codegen/capability-missing"
help = """
The code uses a `target` or `capability` that the current compilation
target does not provide. Capabilities are feature-gated by the target
specification.

Fix: guard the capability-dependent code with `if capability(...)` or
change the compilation target.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0008"
title = "Foreign ABI Mismatch"
severity = "error"
docs_key = "codegen/foreign-abi-mismatch"
help = """
A foreign function interface (FFI) declaration does not match the actual
symbol in the linked library. Type layouts, calling conventions, or
symbol names may differ.

Fix: align the Kain FFI declaration with the native library's ABI.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0009"
title = "Codegen Intrinsic Not Found"
severity = "error"
docs_key = "codegen/intrinsic-not-found"
help = """
A compiler intrinsic required by the generated code is not available
for the target platform. Some intrinsics are platform-specific.

Fix: use a portable alternative or add a platform-specific
implementation.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0010"
title = "Optimization Pass Failed"
severity = "warning"
docs_key = "codegen/optimization-failed"
help = """
An optimization pass encountered an unexpected IR state and was skipped.
The generated code is correct but may be slower than optimal.

Fix: this is typically a compiler bug — report the issue with a
reproduction.
"""

[[diagnostics]]
code = "KAIN-CODEGEN-0011"
title = "Codegen Budget Exceeded"
severity = "error"
docs_key = "codegen/budget-exceeded"
help = """
Code generation exceeded a resource budget (time, memory, or output
size limit). The generated code is too large or complex for the target.

Fix: split the compilation unit, reduce inlining, or increase the budget
via compiler flags.
"""
```

## component
```toml
# ── Kain Component/Actor Error Codes ───────────────────────────────────
# Components, actors, spawn, send, receive, emit, observe, decay, on.

[[diagnostics]]
code = "KAIN-ACTOR-0001"
title = "Component/Actor Error"
severity = "error"
docs_key = "actor/general"
help = "A component or actor system invariant has been violated."

[[diagnostics]]
code = "KAIN-ACTOR-0002"
title = "Actor Spawn Failed"
severity = "error"
docs_key = "actor/spawn-failed"
help = """
`spawn` could not create the actor or component. Common causes:
- The target world does not exist or has been torn down.
- The actor type is not registered with the world.
- Resource limits have been exceeded.

Fix: ensure the world is alive and the actor type is registered.
"""

[[diagnostics]]
code = "KAIN-ACTOR-0003"
title = "Send To Invalid Actor"
severity = "error"
docs_key = "actor/send-invalid"
help = """
`send` targets an actor that does not exist, has been destroyed, or
does not accept messages of the given type.

Fix: check that the target actor is alive and handles the message type.
"""
see_also = ["KAIN-BORROW-0009"]

[[diagnostics]]
code = "KAIN-ACTOR-0004"
title = "Receive Handler Missing"
severity = "error"
docs_key = "actor/receive-missing"
help = """
An actor or component receives messages but does not have a `receive`
handler for one or more of the message types sent to it.

Fix: add a `receive` block for the unhandled message type.
"""

[[diagnostics]]
code = "KAIN-ACTOR-0005"
title = "Emit Without Listener"
severity = "warning"
docs_key = "actor/emit-no-listener"
help = """
`emit` sends an event, but no component or actor is `observe`-ing the
event. The emission has no effect.

Fix: add an observer for the event, or remove the dead emit.
"""

[[diagnostics]]
code = "KAIN-ACTOR-0006"
title = "Decay Transition Invalid"
severity = "error"
docs_key = "actor/decay-invalid"
help = """
A `decay` state transition targets an invalid state or occurs in a
context where decay is not meaningful.

Fix: ensure the decay target state exists in the state machine.
"""
see_also = ["KAIN-STATE-0004"]

[[diagnostics]]
code = "KAIN-ACTOR-0007"
title = "Component Lifecycle Violation"
severity = "error"
docs_key = "actor/lifecycle-violation"
help = """
An operation on a component is invalid for its current lifecycle stage.
Components have distinct phases: created, active, decaying, destroyed.
Operations must align with the current phase.

Fix: check the component lifecycle phase before the operation.
"""

[[diagnostics]]
code = "KAIN-ACTOR-0008"
title = "On Handler Duplicate"
severity = "error"
docs_key = "actor/on-duplicate"
help = """
Multiple `on` handlers are registered for the same event on the same
actor or component. Only one handler per event type is allowed per
component.

Fix: consolidate the handlers or use different event types.
"""
```

## comptime
```toml
# ── Kain Comptime/Macro Error Codes ────────────────────────────────────
# comptime evaluation, macro expansion, patch, law, axiom, orchestrate,
# converge, shatter.

[[diagnostics]]
code = "KAIN-COMPTIME-0001"
title = "Comptime Error"
severity = "error"
docs_key = "comptime/general"
help = "A compile-time evaluation failed. This is the generic comptime fallback."

[[diagnostics]]
code = "KAIN-COMPTIME-0002"
title = "Comptime Evaluation Exceeded Recursion Limit"
severity = "error"
docs_key = "comptime/recursion-limit"
help = """
A `comptime` expression recursed beyond the compile-time evaluation
limit (default: 1000 steps). This prevents infinite loops during
compilation.

Fix: increase the recursion depth or restructure the comptime logic
to terminate earlier.
"""

[[diagnostics]]
code = "KAIN-COMPTIME-0003"
title = "Comptime Access To Runtime Value"
severity = "error"
docs_key = "comptime/runtime-value"
help = """
A `comptime` block attempts to access a value that is only available at
runtime (e.g., user input, file I/O result, actor state). Compile-time
evaluation can only use constants, type information, and comptime-known
values.

Fix: move the runtime-dependent logic out of the comptime block.
"""

[[diagnostics]]
code = "KAIN-COMPTIME-0004"
title = "Macro Expansion Error"
severity = "error"
docs_key = "comptime/macro-expansion"
help = """
A `macro` failed to expand — either the macro body produced invalid
syntax, the argument count mismatched, or an expansion recursion limit
was hit.

Fix: check the macro definition and call site for consistency.
"""
see_also = ["KAIN-COMPTIME-0002"]

[[diagnostics]]
code = "KAIN-COMPTIME-0005"
title = "Patch Target Not Found"
severity = "error"
docs_key = "comptime/patch-target"
help = """
A `patch` declaration targets a function, type, or module that does not
exist or is not patchable. Only items explicitly marked as patchable can
be modified by patches.

Fix: ensure the target exists and is declared in a patchable context.
"""

[[diagnostics]]
code = "KAIN-COMPTIME-0006"
title = "Law Violation"
severity = "error"
docs_key = "comptime/law-violation"
help = """
A `law` invariant has been violated. Laws are compile-time assertions
about types, values, or structural properties that must hold for the
program to be valid.

Fix: adjust the code to satisfy the law, or modify the law if it is
too restrictive.
"""

[[diagnostics]]
code = "KAIN-COMPTIME-0007"
title = "Axiom Contradiction"
severity = "error"
docs_key = "comptime/axiom-contradiction"
help = """
An `axiom` (assumed truth) contradicts another axiom or a proven law.
The axiom set is inconsistent — the compiler cannot proceed.

Fix: remove or weaken one of the conflicting axioms.
"""
see_also = ["KAIN-COMPTIME-0006"]

[[diagnostics]]
code = "KAIN-COMPTIME-0008"
title = "Orchestrate Dependency Cycle"
severity = "error"
docs_key = "comptime/orchestrate-cycle"
help = """
`orchestrate` defines a compile-time pipeline of transformations, but
the pipeline contains a dependency cycle (a step depends on its own
output, directly or transitively).

Fix: break the cycle by reordering or splitting pipeline steps.
"""

[[diagnostics]]
code = "KAIN-COMPTIME-0009"
title = "Converge Failed"
severity = "error"
docs_key = "comptime/converge-failed"
help = """
`converge` attempts to unify multiple code variants but the variants
are not structurally compatible — they produce different types, effect
sets, or control flow.

Fix: align the variants so they converge to a consistent interface.
"""

[[diagnostics]]
code = "KAIN-COMPTIME-0010"
title = "Shatter Pattern Incomplete"
severity = "error"
docs_key = "comptime/shatter-incomplete"
help = """
`shatter` splits a generic function into specialized variants, but the
pattern does not cover all concrete type combinations used at call
sites.

Fix: add the missing specialization pattern or provide a fallback
generic implementation.
"""
```

## config
```toml
# ── Kain Config Error Codes ────────────────────────────────────────────
# Configuration, manifests, toolchain, project setup.

[[diagnostics]]
code = "KAIN-CONFIG-0001"
title = "Config Error"
severity = "error"
docs_key = "config/general"
help = "A configuration or manifest validation error occurred."

[[diagnostics]]
code = "KAIN-CONFIG-0002"
title = "Manifest Parse Error"
severity = "error"
docs_key = "config/manifest-parse"
help = """
A Kain manifest (kain.toml, fabric.toml, blade.toml) could not be
parsed. The file may have invalid TOML syntax or missing required fields.

Fix: validate the manifest file against the expected schema.
"""

[[diagnostics]]
code = "KAIN-CONFIG-0003"
title = "Toolchain Not Found"
severity = "error"
docs_key = "config/toolchain-not-found"
help = """
A required external toolchain (LLVM, MSVC, GCC, GPU SDK, UE5) is not
installed or not detected on the system PATH.

Fix: install the required toolchain and ensure it is on the PATH.
"""

[[diagnostics]]
code = "KAIN-CONFIG-0004"
title = "Target Specification Invalid"
severity = "error"
docs_key = "config/target-invalid"
help = """
The build target specification (target triple, platform, GPU, runtime)
is invalid or contradictory.

Fix: provide a valid target specification. Use `kain targets` to list
available targets.
"""

[[diagnostics]]
code = "KAIN-CONFIG-0005"
title = "Feature Flag Conflict"
severity = "error"
docs_key = "config/feature-conflict"
help = """
Two or more enabled feature flags are mutually exclusive or conflict.
Check the feature flag documentation for compatibility.

Fix: disable one of the conflicting features.
"""

[[diagnostics]]
code = "KAIN-CONFIG-0006"
title = "Dependency Resolution Failed"
severity = "error"
docs_key = "config/dependency-failed"
help = """
A project dependency could not be resolved. The package may not exist,
the version constraint may be unsatisfiable, or there may be a
dependency cycle.

Fix: check the dependency name, version, and source.
"""
```

## converge
```toml
# ── Kain Converge Error Codes ───────────────────────────────────────────
# Fast-lane dispatch selection, spec/fast contract verification, verifier
# sampling, lane capability gaps, and return/effect-set divergence.
#
# converge computes a value using the spec lane as the oracle and one or
# more fast lanes as optimised alternatives. These codes fire when the
# contract between spec and fast is broken, when lane selection is
# ambiguous, or when a fast lane claims a capability the target lacks.

[[diagnostics]]
code = "KAIN-CONVERGE-0001"
title = "Converge Error"
severity = "error"
docs_key = "converge/general"
help = "A converge block has a well-formedness error. This is the generic fallback."
see_also = ["KAIN-CONVERGE-0002", "KAIN-CONVERGE-0003"]

[[diagnostics]]
code = "KAIN-CONVERGE-0002"
title = "Converge Missing Spec Lane"
severity = "error"
docs_key = "converge/missing-spec"
help = """
Every `converge` block must have exactly one `spec reference:` lane that
acts as the ground-truth oracle for verifier sampling. None was found.

Fix: add a `spec reference:` lane before any `fast` lanes.
"""
example_bad = """
converge compute(x: Int) -> Int:
    fast avx2_lane when capability("cpu.x86.avx2"):
        return simd_mix(x)
"""
example_good = """
converge compute(x: Int) -> Int:
    spec reference:
        return scalar_mix(x)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return simd_mix(x)
"""

[[diagnostics]]
code = "KAIN-CONVERGE-0003"
title = "Converge Fast Lane Contract Mismatch"
severity = "error"
docs_key = "converge/fast-lane-mismatch"
help = """
A `fast` lane's verifier samples do not agree with the `spec reference`
oracle. The fast implementation is semantically inequivalent.

Fix: align the fast lane's result with the spec lane for all inputs, or
reduce the verifier sample count and re-check the boundary cases.
"""
see_also = ["KAIN-CONVERGE-0004"]

[[diagnostics]]
code = "KAIN-CONVERGE-0004"
title = "Converge Verifier Failed"
severity = "error"
docs_key = "converge/verifier-failed"
help = """
The `verify` clause ran the built-in random sampler against the spec and
fast lanes and found a disagreement. The fast lane diverges from the spec
on at least one generated input.

Fix: inspect the diverging input (the compiler will emit it) and fix the
fast lane logic.
"""
see_also = ["KAIN-CONVERGE-0003"]

[[diagnostics]]
code = "KAIN-CONVERGE-0005"
title = "Converge Capability Gap At Target"
severity = "error"
docs_key = "converge/capability-gap"
help = """
A `fast ... when capability(...)` lane requires a hardware capability
(e.g., `cpu.x86.avx2`, `gpu.tensor_core`) that is absent on the current
compilation target. No valid lane remains after filtering.

Fix: add a fallback fast lane for the common-case target, or make the
spec lane available unconditionally.
"""
see_also = ["KAIN-CONVERGE-0002"]

[[diagnostics]]
code = "KAIN-CONVERGE-0006"
title = "Converge Return Type Divergence"
severity = "error"
docs_key = "converge/return-type-divergence"
help = """
The spec lane and one or more fast lanes return incompatible types.
All lanes in a `converge` block must agree on the return type.

Fix: align the return type annotation across all lanes.
"""

[[diagnostics]]
code = "KAIN-CONVERGE-0007"
title = "Converge Effect Set Divergence"
severity = "error"
docs_key = "converge/effect-set-divergence"
help = """
Lanes within a `converge` block declare different effect sets. The
compiler cannot guarantee safe dispatch — a caller expecting a Pure lane
might receive an IO lane at runtime.

Fix: ensure all lanes agree on effect annotations, or mark the converge
block with the union of required effects.
"""
see_also = ["KAIN-EFFECT-0001"]

[[diagnostics]]
code = "KAIN-CONVERGE-0008"
title = "Converge Ambiguous Lane Selection"
severity = "error"
docs_key = "converge/ambiguous-lane"
help = """
Multiple `fast` lanes match the current target's capability set and
runtime conditions simultaneously. The compiler cannot deterministically
choose one.

Fix: use more specific `when` guards or add a priority ordering with
`prefer` annotations.
"""
```

## effect
```toml
# ── Kain Effect Error Codes ────────────────────────────────────────────
# Effect system: Pure, IO, async, Async, GPU, Reactive, Unsafe.

[[diagnostics]]
code = "KAIN-EFFECT-0001"
title = "Effect Violation"
severity = "error"
docs_key = "effects/violation"
help = """
A function or block performs an operation that is not permitted by its
declared effect annotation. The effect system tracks side-effect
capabilities (IO, GPU, async, etc.) and ensures they are explicitly
declared.

Fix: either add the required effect annotation to the enclosing function,
or move the violating operation behind a compatible effect boundary.
"""
see_also = ["KAIN-EFFECT-0002"]

[[diagnostics]]
code = "KAIN-EFFECT-0002"
title = "Missing Effect Capability"
severity = "error"
docs_key = "effects/missing-capability"
help = """
A function is annotated with an effect (e.g., `GPU`) but the caller
does not have that capability in its own effect set. Effect capabilities
flow upward — callers must declare at least the effects of their callees.

Fix: propagate the effect annotation to the caller, or gate the call
behind a `capability` check.
"""
see_also = ["KAIN-EFFECT-0001", "KAIN-EFFECT-0005"]

[[diagnostics]]
code = "KAIN-EFFECT-0003"
title = "Effect Polymorphism Mismatch"
severity = "error"
docs_key = "effects/polymorphism-mismatch"
help = """
A generic effect-polymorphic function was instantiated with effect
parameters that conflict. Effect variables must unify consistently
across all call sites.

Fix: align the effect parameters or make the function more permissive.
"""

[[diagnostics]]
code = "KAIN-EFFECT-0004"
title = "Pure Function With Side Effect"
severity = "error"
docs_key = "effects/pure-side-effect"
help = """
A function annotated `Pure` (or implicitly pure by default) contains an
operation with observable side effects (IO, mutation of shared state,
GPU dispatch, async scheduling).

Fix: remove the side effect, or change the annotation to the appropriate
effect (e.g., `IO`, `GPU`).
"""
example_bad = "Pure fn compute: {\n    print(\"hello\")\n}"
example_good = "IO fn compute: {\n    print(\"hello\")\n}"

[[diagnostics]]
code = "KAIN-EFFECT-0005"
title = "Capability Gate Failure"
severity = "error"
docs_key = "effects/capability-gate"
help = """
A `capability` check guards access to an effect-gated operation, but
the required capability is not available at the call site. Capabilities
are intrinsic permissions granted by the runtime or platform.

Fix: acquire the needed capability before the gate, or restructure the
code to run in a context where the capability is present.
"""
see_also = ["KAIN-EFFECT-0002"]

[[diagnostics]]
code = "KAIN-EFFECT-0006"
title = "Async In Sync Context"
severity = "error"
docs_key = "effects/async-in-sync"
help = """
An `async` function or `await` expression appears in a non-async context
that cannot suspend. Async can only be called from other async contexts
or from an async runtime entry point.

Fix: mark the enclosing function `async` or use a blocking adapter.
"""
see_also = ["KAIN-EFFECT-0007"]

[[diagnostics]]
code = "KAIN-EFFECT-0007"
title = "Await Outside Async"
severity = "error"
docs_key = "effects/await-outside-async"
help = """
`await` was used in a function that is not marked `async`. Only async
functions can suspend at await points.

Fix: add the `async` annotation to the enclosing function.
"""
example_bad = "fn fetch: {\n    await http.get(\"...\")\n}"
example_good = "async fn fetch: {\n    await http.get(\"...\")\n}"

[[diagnostics]]
code = "KAIN-EFFECT-0008"
title = "GPU Effect In Host Context"
severity = "error"
docs_key = "effects/gpu-in-host"
help = """
A function annotated `GPU` (or containing GPU intrinsics) is being
called from a host-only context without GPU dispatch capability.

Fix: wrap the call in a `shader` or `compute` block, or use a GPU
dispatch primitive.
"""
see_also = ["KAIN-SHADER-0001"]

[[diagnostics]]
code = "KAIN-EFFECT-0009"
title = "Reactive Cycle Detected"
severity = "error"
docs_key = "effects/reactive-cycle"
help = """
Reactive state dependencies form a cycle. The reactive runtime cannot
resolve circular update chains.

Fix: break the cycle by introducing an explicit delay, a `decay`
transition, or restructuring to a directed acyclic graph.
"""
see_also = ["KAIN-STATE-0003"]

[[diagnostics]]
code = "KAIN-EFFECT-0010"
title = "Unsafe Block Not Allowed"
severity = "error"
docs_key = "effects/unsafe-disallowed"
help = """
An `Unsafe` block or function is prohibited by the current compilation
profile or security policy. Some Kain targets (e.g., web, sandboxed
runtimes) disallow unsafe operations entirely.

Fix: remove the unsafe block, or change the compilation target.
"""

[[diagnostics]]
code = "KAIN-EFFECT-0011"
title = "Effect Leakage Through Public API"
severity = "warning"
docs_key = "effects/public-leakage"
help = """
A `pub` function exposes an effect annotation that may be an
implementation detail. Public APIs should use effect polymorphism or
explicitly document the required capabilities.

Fix: consider making the function effect-polymorphic or hiding the
effect behind a trait abstraction.
"""

[[diagnostics]]
code = "KAIN-EFFECT-0012"
title = "Conflicting Effect Annotations"
severity = "error"
docs_key = "effects/conflicting"
help = """
Multiple effect annotations on the same item conflict (e.g., marking a
function both `Pure` and `IO`).

Fix: choose the single correct annotation. If the function does both pure
computation and IO, it is `IO` (IO subsumes pure computation).
"""
```

## entangle
```toml
# ── Kain Entangle Error Codes ────────────────────────────────────────────
# Bidirectional world-state coupling: entangle syntax, single_writer policy,
# cycle detection, dangling references, cross-world scoping, type and
# direction mismatches.
#
# entangle A.field <-> B.field_copy with single_writer creates a compiler-
# owned observer graph edge. These codes fire when that graph is malformed.

[[diagnostics]]
code = "KAIN-ENTANGLE-0001"
title = "Entangle Error"
severity = "error"
docs_key = "entangle/general"
help = "An `entangle` declaration has a well-formedness error. This is the generic fallback."
see_also = ["KAIN-ENTANGLE-0002", "KAIN-ENTANGLE-0003"]

[[diagnostics]]
code = "KAIN-ENTANGLE-0002"
title = "Entangle Cycle Detected"
severity = "error"
docs_key = "entangle/cycle"
help = """
The set of `entangle` declarations forms a directed cycle. The compiler
cannot establish a stable propagation order when state changes would
propagate infinitely between worlds.

Fix: break the cycle by making at least one edge directional (one-way
update) or by introducing an explicit intermediary.
"""
see_also = ["KAIN-WORLD-0007"]

[[diagnostics]]
code = "KAIN-ENTANGLE-0003"
title = "Entangle Single Writer Violation"
severity = "error"
docs_key = "entangle/single-writer"
help = """
An `entangle` link declared `with single_writer` has more than one
potential writer. Only the designated writer world may mutate the
coupled state.

Fix: identify which world owns the write and remove or guard the other
write site. Use `patch` to make the mutation explicit and transactional.
"""
example_bad = """
entangle Authority.count <-> Mirror.count_copy with single_writer
// ... later: Mirror.count_copy = 42  // second writer — error
"""
example_good = """
entangle Authority.count <-> Mirror.count_copy with single_writer
// Mirror.count_copy is read-only; only Authority writes count
"""
see_also = ["KAIN-BORROW-0006"]

[[diagnostics]]
code = "KAIN-ENTANGLE-0004"
title = "Entangle Dangling Reference"
severity = "error"
docs_key = "entangle/dangling"
help = """
One or both worlds referenced by an `entangle` declaration do not exist
or have been removed. Entangle links must point to live world state slots.

Fix: ensure both worlds are declared and their state fields exist.
"""
see_also = ["KAIN-WORLD-0005"]

[[diagnostics]]
code = "KAIN-ENTANGLE-0005"
title = "Entangle Cross-World Scope Error"
severity = "error"
docs_key = "entangle/cross-world-scope"
help = """
An `entangle` declaration references a state field from a world that is
not in scope at the entangle site. Top-level entangle declarations must
be at module scope and both worlds must be visible.

Fix: move the entangle declaration to module scope, or import both worlds.
"""

[[diagnostics]]
code = "KAIN-ENTANGLE-0006"
title = "Entangle Type Mismatch"
severity = "error"
docs_key = "entangle/type-mismatch"
help = """
The types of the two coupled state fields are not compatible for
synchronization. Entangle requires the fields to have the same type or
an explicit coercion annotation.

Fix: ensure both fields have the same type, or add a conversion function
to the entangle declaration.
"""
see_also = ["KAIN-TYPE-0025"]

[[diagnostics]]
code = "KAIN-ENTANGLE-0007"
title = "Entangle Direction Conflict"
severity = "error"
docs_key = "entangle/direction-conflict"
help = """
Two `entangle` declarations couple the same pair of fields in conflicting
directions. Only one coupling direction may be declared per field pair.

Fix: use a single bidirectional `<->` link, or split into two distinct
one-way links with explicit direction annotations.
"""
```

## internal
```toml
# ── Kain Internal Error Codes ───────────────────────────────────────────
# Compiler bugs, impossible states, and unclassified hard failures.

[[diagnostics]]
code = "KAIN-INTERNAL-0001"
title = "Internal Error"
severity = "error"
docs_key = "internal/general"
help = """
The compiler or runtime hit an internal invariant failure. This is not a
normal user-authored language error; it usually means the toolchain hit
an unsupported state, stale assumption, or outright bug.

Fix: capture the repro, the active pass, and the surrounding diagnostic
context so the owning subsystem can patch the invariant.
"""
```

## io
```toml
# ── Kain IO Error Codes ────────────────────────────────────────────────
# File I/O, network, asset loading, filesystem operations.

[[diagnostics]]
code = "KAIN-IO-0001"
title = "IO Error"
severity = "error"
docs_key = "io/general"
help = "A filesystem or network I/O operation failed."

[[diagnostics]]
code = "KAIN-IO-0002"
title = "File Not Found"
severity = "error"
docs_key = "io/file-not-found"
help = """
The specified file does not exist or is not accessible. Check the file
path, permissions, and working directory.

Fix: verify the file path and ensure the file exists.
"""

[[diagnostics]]
code = "KAIN-IO-0003"
title = "File Read Error"
severity = "error"
docs_key = "io/read-error"
help = """
A file could not be read. Possible causes: permission denied, file
locked by another process, or filesystem corruption.

Fix: check file permissions and that the file is not locked.
"""

[[diagnostics]]
code = "KAIN-IO-0004"
title = "File Write Error"
severity = "error"
docs_key = "io/write-error"
help = """
A file could not be written. Possible causes: disk full, permission
denied, or the parent directory does not exist.

Fix: check disk space, permissions, and parent directory existence.
"""

[[diagnostics]]
code = "KAIN-IO-0005"
title = "Network Request Failed"
severity = "error"
docs_key = "io/network-failed"
help = """
A network operation (HTTP, WebSocket, etc.) failed. The remote host may
be unreachable, the request may have timed out, or the response may be
malformed.

Fix: check network connectivity and the request configuration.
"""

[[diagnostics]]
code = "KAIN-IO-0006"
title = "Asset Import Failed"
severity = "error"
docs_key = "io/asset-import"
help = """
An asset (texture, mesh, sound, data file) could not be imported. The
file format may be unsupported, the asset may be corrupt, or the
importer may have encountered an error.

Fix: check the asset file format and integrity.
"""
```

## memory
```toml
# ── Kain Memory Error Codes ────────────────────────────────────────────
# Memory layout, pointers, bitfields, alignment, address spaces.

[[diagnostics]]
code = "KAIN-MEM-0001"
title = "Memory Lowering Required"
severity = "error"
docs_key = "memory/lowering"
help = """
The code uses raw memory semantics (pointers, bitfields, etc.) but no
lowering policy has been selected for the target backend.

Fix: add a lowering policy or select a backend with native memory
support.
"""

[[diagnostics]]
code = "KAIN-MEM-0002"
title = "Memory Semantics Unsupported By Backend"
severity = "error"
docs_key = "memory/backend-capabilities"
help = """
The backend does not support the memory operations used in this code
(e.g., raw pointers on a GPU target, or bitfield addressing on WASM).

Fix: choose a compatible backend or lower the memory operations.
"""
see_also = ["KAIN-MEM-0001"]

[[diagnostics]]
code = "KAIN-MEM-0003"
title = "Illegal Bitfield Address"
severity = "error"
docs_key = "memory/bitfields"
help = """
Taking the address of a C-compatible bitfield is not allowed. Bitfields
may not occupy whole bytes and do not have stable addresses.

Fix: lower the bitfield access into load/store/mask operations.
"""

[[diagnostics]]
code = "KAIN-MEM-0004"
title = "Memory Layout Overflow"
severity = "error"
docs_key = "memory/layout-overflow"
help = """
The computed memory layout for a type exceeds the maximum representable
size or alignment for the target address space (32-bit vs 64-bit).

Fix: reduce the aggregate size, split the layout, or use a 64-bit target.
"""

[[diagnostics]]
code = "KAIN-MEM-0005"
title = "Alignment Requirement Not Satisfied"
severity = "error"
docs_key = "memory/alignment"
help = """
A value requires a stricter alignment than the allocated memory
provides. This can cause undefined behavior on some platforms.

Fix: increase the allocation alignment or use an unaligned access
primitive if the platform supports it.
"""

[[diagnostics]]
code = "KAIN-MEM-0006"
title = "Null Pointer Dereference"
severity = "error"
docs_key = "memory/null-deref"
help = """
A pointer that may be null is being dereferenced without a null check.
Kain requires explicit null guards for nullable pointers.

Fix: wrap the dereference in an `if ptr != none:` block or use the
`?.` safe-access operator.
"""
example_bad = "let value = *ptr"
example_good = "let value = ptr?.*value"

[[diagnostics]]
code = "KAIN-MEM-0007"
title = "Out Of Bounds Access"
severity = "error"
docs_key = "memory/out-of-bounds"
help = """
An array or buffer access uses an index that is not provably within
bounds. Kain requires bounds checks or proofs for all indexed access.

Fix: add a bounds check before the access or use an iterator.
"""
example_bad = "let x = arr[100]"
example_good = "if arr.len() > 100: let x = arr[100]"

[[diagnostics]]
code = "KAIN-MEM-0008"
title = "Address Space Mismatch"
severity = "error"
docs_key = "memory/address-space"
help = """
A pointer in one address space (e.g., GPU global memory) is being used
in a context that expects a different address space (e.g., GPU shared
memory).

Fix: use the correct address space qualifier or copy data to the
expected space.
"""
```

## parse
```toml
# ── Kain Parse Error Codes ─────────────────────────────────────────────
# Data-driven diagnostic specs for the lexer and parser phases.
# Every code maps to a KAIN-PARSE-NNNN identifier consumed by the
# registry, the terminal renderer, JSON output, and `kain explain`.

[[diagnostics]]
code = "KAIN-PARSE-0001"
title = "Parse Error"
severity = "error"
docs_key = "parser/general"
help = """
The parser encountered input it cannot understand. This is the generic
fallback — most parse errors produce a more specific code.
"""
see_also = ["KAIN-PARSE-0002", "KAIN-PARSE-0003"]

[[diagnostics]]
code = "KAIN-PARSE-0002"
title = "Expected Token"
severity = "error"
docs_key = "parser/expected-token"
help = """
The parser expected a specific token at this position but found something
else.  Kain grammar is whitespace-sensitive around block delimiters and
uses `:` after block headers.

Fix: insert the missing token before continuing, or restructure the
surrounding syntax so the expected token appears in a valid grammar slot.
"""
example_bad = "fn main {\n    return\n}"
example_good = "fn main: {\n    return\n}"
see_also = ["KAIN-PARSE-0005", "KAIN-PARSE-0007"]

[[diagnostics]]
code = "KAIN-PARSE-0003"
title = "Unexpected Token"
severity = "error"
docs_key = "parser/unexpected-token"
help = """
A token appeared in a position where the Kain grammar does not allow it.
This often happens when a delimiter is missing, causing the parser to
interpret the next construct incorrectly.

Fix: remove the stray token or add the missing delimiter so the token
lands in a valid grammar position.
"""
example_bad = "let x = 5\n  let y = 10"
example_good = "let x = 5\nlet y = 10"
see_also = ["KAIN-PARSE-0002"]

[[diagnostics]]
code = "KAIN-PARSE-0004"
title = "Reserved Identifier"
severity = "error"
docs_key = "parser/reserved-identifiers"
help = """
The identifier you used collides with a word reserved by Kain, HLSL,
C++, or an engine host runtime. Reserved identifiers cannot be used as
user-defined names.

Fix: rename the identifier. A common convention is to append an
underscore or choose a domain-specific synonym.
"""
see_also = ["KAIN-PARSE-0007"]

[[diagnostics]]
code = "KAIN-PARSE-0005"
title = "Missing Delimiter Before Newline"
severity = "error"
docs_key = "parser/missing-delimiter-before-newline"
help = """
Kain block headers (fn, if, match, for, while, component, world, etc.)
require a `:` before the body. A newline appeared before the expected
delimiter.

Fix: insert `:` at the end of the header line, or keep the expression
on one logical line.
"""
example_bad = "fn greet\n    return \"hi\""
example_good = "fn greet:\n    return \"hi\""
fixit = ":"
see_also = ["KAIN-PARSE-0002"]

[[diagnostics]]
code = "KAIN-PARSE-0006"
title = "Invalid World Surface Kind"
severity = "error"
docs_key = "world/surface-kind"
help = """
`surface` declarations inside a `world` block must use one of the four
built-in surface kinds: native_ui, viewport3d, web, or ue5.

Fix: replace the unknown kind with a valid surface projection kind.
"""
example_bad = "surface desktop => MyPanel"
example_good = "surface native_ui => MyPanel"
see_also = ["KAIN-WORLD-0001"]

[[diagnostics]]
code = "KAIN-PARSE-0007"
title = "Expected Contextual Keyword"
severity = "error"
docs_key = "parser/contextual-keywords"
help = """
Contextual keywords (patch, law, axiom, pulse, orchestrate, converge,
world, entangle, shatter, teleport, every, when, guarantee, fallback,
spec, fast, verify, random, jitter, target, capability, from, to, via,
surface, compute, uniform, render, on, weak, single_writer) only become
special in specific grammar slots.

Fix: check whether a nearby identifier or missing delimiter shifted the
parser out of the keyword slot.
"""
see_also = ["KAIN-PARSE-0002", "KAIN-PARSE-0004"]

[[diagnostics]]
code = "KAIN-PARSE-0008"
title = "Unclosed Delimiter"
severity = "error"
docs_key = "parser/unclosed-delimiter"
help = """
A bracket, brace, or parenthesis was opened but never closed. Kain
tracks delimiter pairs through the lexer and requires all scopes to be
explicitly terminated.

Fix: add the matching closing delimiter at the appropriate nesting level.
"""
example_bad = "let x = (a + b"
example_good = "let x = (a + b)"
see_also = ["KAIN-PARSE-0002"]

[[diagnostics]]
code = "KAIN-PARSE-0009"
title = "Mismatched Delimiter"
severity = "error"
docs_key = "parser/mismatched-delimiter"
help = """
A closing delimiter did not match its opening counterpart (e.g., `]`
closing `{`). The lexer tracks delimiter pairs and detected a mismatch.

Fix: align the closing delimiter with the expected opening pair.
"""
example_bad = "let arr = [1, 2, 3}"
example_good = "let arr = [1, 2, 3]"
see_also = ["KAIN-PARSE-0008"]

[[diagnostics]]
code = "KAIN-PARSE-0010"
title = "Invalid Numeric Literal"
severity = "error"
docs_key = "parser/numeric-literal"
help = """
A numeric literal could not be parsed. Kain supports integers (decimal,
hex 0x, binary 0b, octal 0o), floats (with `.` or exponent), and
type suffixes (u8, i32, f32, f64).

Fix: ensure the literal conforms to the supported formats.
"""
example_bad = "let x = 0xZZZ"
example_good = "let x = 0xFF"

[[diagnostics]]
code = "KAIN-PARSE-0011"
title = "Invalid String Literal"
severity = "error"
docs_key = "parser/string-literal"
help = """
A string literal is malformed. Kain strings use double quotes with
backslash escapes (\\n, \\t, \\\", \\\\, \\u{XXXX}).

Fix: ensure proper escaping and that the string is terminated.
"""
example_bad = 'let s = "hello\nworld"'
example_good = 'let s = "hello\\nworld"'

[[diagnostics]]
code = "KAIN-PARSE-0012"
title = "Invalid Character Literal"
severity = "error"
docs_key = "parser/char-literal"
help = """
A character literal must contain exactly one codepoint (or one escape
sequence) between single quotes.

Fix: use a string literal for multi-character sequences.
"""
example_bad = "let c = 'ab'"
example_good = "let c = 'a'"

[[diagnostics]]
code = "KAIN-PARSE-0013"
title = "Attribute Syntax Error"
severity = "error"
docs_key = "parser/attribute"
help = """
Kain attributes use `@name` or `@name(args...)` syntax. The attribute
could not be parsed — check the attribute name and argument list.

Fix: ensure the attribute conforms to `@identifier` or
`@identifier(arg, ...)` syntax.
"""
example_bad = "@ material_graph"
example_good = "@material_graph"

[[diagnostics]]
code = "KAIN-PARSE-0014"
title = "Effect Annotation Syntax Error"
severity = "error"
docs_key = "parser/effect-annotation"
help = """
Effect annotations (`Pure`, `IO`, `async`, `Async`, `GPU`, `Reactive`,
`Unsafe`) must appear in specific positions on function signatures or
type definitions. The parser could not interpret the annotation in this
position.

Fix: move the effect annotation to the correct position (before the
function return type or on the type definition).
"""
see_also = ["KAIN-EFFECT-0001", "KAIN-EFFECT-0002"]

[[diagnostics]]
code = "KAIN-PARSE-0015"
title = "Module Declaration Error"
severity = "error"
docs_key = "parser/module"
help = """
`mod` declarations must be followed by an identifier and optionally a
body block (`{ ... }`) or a file path string. The parser could not
complete the module declaration.

Fix: ensure the module has a valid name and a body or path.
"""
example_bad = "mod"
example_good = "mod graphics { ... }"

[[diagnostics]]
code = "KAIN-PARSE-0016"
title = "Use/Import Syntax Error"
severity = "error"
docs_key = "parser/use-import"
help = """
`use` declarations import symbols with path syntax (`use a::b::c` or
`use a::{b, c}`). The import path could not be parsed.

Fix: ensure the import path uses `::` separators and valid identifiers.
"""
example_bad = "use std..math"
example_good = "use std::math"

[[diagnostics]]
code = "KAIN-PARSE-0017"
title = "Visibility Modifier Error"
severity = "error"
docs_key = "parser/visibility"
help = """
`pub` must be followed by a declaration (fn, struct, enum, type, mod,
const, etc.). It cannot stand alone.

Fix: attach `pub` to a valid declaration.
"""
example_bad = "pub"
example_good = "pub fn greet: ..."

[[diagnostics]]
code = "KAIN-PARSE-0018"
title = "Comptime Block Syntax Error"
severity = "error"
docs_key = "parser/comptime"
help = """
`comptime` blocks must contain valid Kain expressions that can be
evaluated at compile time. The block could not be parsed.

Fix: ensure the comptime block body contains valid, evaluable
expressions.
"""
see_also = ["KAIN-COMPTIME-0001"]

[[diagnostics]]
code = "KAIN-PARSE-0019"
title = "Macro Invocation Syntax Error"
severity = "error"
docs_key = "parser/macro"
help = """
`macro` definitions and invocations use a specific syntax. The macro
could not be parsed — check the macro name, parameter list, and body.

Fix: macros take the form `macro name(params...): { body }`.
"""
see_also = ["KAIN-COMPTIME-0002"]

[[diagnostics]]
code = "KAIN-PARSE-0020"
title = "Test Declaration Syntax Error"
severity = "error"
docs_key = "parser/test"
help = """
`test` blocks must define a named test with an optional body. The test
declaration could not be parsed.

Fix: `test "name": { body }` or `test "name" { body }`.
"""
example_bad = "test: {}"
example_good = 'test "my test": { assert true }'
see_also = ["KAIN-TEST-0001"]
```

## patch
```toml
# ── Kain Patch / Law Error Codes ────────────────────────────────────────
# Transactional world mutation: patch target validation, law
# precondition/postcondition checking, out-of-scope application,
# conflicting mutations, and return type mismatches.
#
# patch update(target: World, v: T) -> T applies a validated mutation to
# a world's state. law predicates guard every patch site.

[[diagnostics]]
code = "KAIN-PATCH-0001"
title = "Patch Error"
severity = "error"
docs_key = "patch/general"
help = "A `patch` or `law` declaration has a well-formedness error. This is the generic fallback."
see_also = ["KAIN-PATCH-0002", "KAIN-PATCH-0003"]

[[diagnostics]]
code = "KAIN-PATCH-0002"
title = "Patch Target Is Not A World"
severity = "error"
docs_key = "patch/target-not-world"
help = """
The first parameter of a `patch` block must be a `world` type. `patch`
is designed for transactional mutation of compiler-owned world state
and cannot be applied to arbitrary structs or scalars.

Fix: change the patch target to a declared `world`, or use a regular
function for non-world mutation.
"""
example_bad = """
patch update(target: MyStruct, v: Int) -> Int:
    target.count = v
    return target.count
"""
example_good = """
patch update(target: Authority, v: Int) -> Int:
    target.count = v
    return target.count
"""

[[diagnostics]]
code = "KAIN-PATCH-0003"
title = "Patch Law Precondition Failed"
severity = "error"
docs_key = "patch/law-precondition"
help = """
A `law` predicate guarding this `patch` site failed its precondition
check. The proposed mutation is not valid given the current world state.

Fix: ensure the value being patched satisfies all law predicates, or
adjust the law to match the intended invariant.
"""
example_bad = """
law value_in_range(v: Int) -> Bool:
    return v >= 0 and v < 1000000007

patch update(target: Authority, v: Int) -> Int:
    target.count = v  // v = -1 violates law
"""
example_good = """
patch update(target: Authority, v: Int) -> Int:
    assert value_in_range(v)
    target.count = v
    return target.count
"""
see_also = ["KAIN-PATCH-0004", "KAIN-COMPTIME-0006"]

[[diagnostics]]
code = "KAIN-PATCH-0004"
title = "Patch Law Postcondition Failed"
severity = "error"
docs_key = "patch/law-postcondition"
help = """
After applying the `patch`, a `law` postcondition is not satisfied by the
resulting world state. The patch body must leave the world in a state
that satisfies all invariants.

Fix: adjust the patch body to restore invariant compliance before
returning, or strengthen the postcondition law.
"""
see_also = ["KAIN-PATCH-0003"]

[[diagnostics]]
code = "KAIN-PATCH-0005"
title = "Patch Applied Outside World Scope"
severity = "error"
docs_key = "patch/outside-scope"
help = """
A `patch` block was invoked in a context where the target world is not
in scope or where world mutation is not permitted (e.g., inside a pure
function, a shader, or a GPU compute kernel).

Fix: invoke the patch from a world-aware context (a `pulse` handler, a
top-level function with IO/Reactive effect, or an actor `on` handler).
"""
see_also = ["KAIN-EFFECT-0004", "KAIN-SHADER-0001"]

[[diagnostics]]
code = "KAIN-PATCH-0006"
title = "Conflicting Patch Mutations"
severity = "error"
docs_key = "patch/conflicting-mutations"
help = """
Two or more `patch` applications target the same world state field
concurrently and their mutations are not ordered or serialized. The
resulting world state is non-deterministic.

Fix: serialize patches through a single actor or use a transactional
ordering annotation.
"""
see_also = ["KAIN-BORROW-0006", "KAIN-ENTANGLE-0003"]

[[diagnostics]]
code = "KAIN-PATCH-0007"
title = "Patch Law Return Type Mismatch"
severity = "error"
docs_key = "patch/law-return-type"
help = """
A `law` predicate used with a `patch` must return `Bool`. The declared
or inferred return type is not `Bool`.

Fix: change the law body to evaluate to a boolean expression and update
the return type annotation.
"""
example_bad = """
law value_ok(v: Int) -> Int:
    return v + 1
"""
example_good = """
law value_ok(v: Int) -> Bool:
    return v > 0
"""
see_also = ["KAIN-TYPE-0025"]
```

## runtime
```toml
# ── Kain Runtime Error Codes ───────────────────────────────────────────
# Runtime errors: dispatch, actor messaging, resource exhaustion.

[[diagnostics]]
code = "KAIN-RUNTIME-0001"
title = "Runtime Error"
severity = "error"
docs_key = "runtime/general"
help = "A runtime invariant has been violated."

[[diagnostics]]
code = "KAIN-RUNTIME-0002"
title = "Actor Panic"
severity = "error"
docs_key = "runtime/actor-panic"
help = """
An actor or component panicked at runtime. The panic may be caused by
an assertion failure, an unhandled message, or a resource error.

Fix: check the panic message and trace to identify the root cause.
"""

[[diagnostics]]
code = "KAIN-RUNTIME-0003"
title = "Message Delivery Failed"
severity = "error"
docs_key = "runtime/message-delivery"
help = """
A message could not be delivered to its target actor. The target may
have been destroyed, its mailbox may be full, or the message type may
not be accepted.

Fix: check the target actor's lifecycle and message handling.
"""
see_also = ["KAIN-ACTOR-0003"]

[[diagnostics]]
code = "KAIN-RUNTIME-0004"
title = "Resource Exhausted"
severity = "error"
docs_key = "runtime/resource-exhausted"
help = """
A runtime resource (memory, file handles, GPU memory, actor capacity)
has been exhausted. The program cannot continue.

Fix: reduce resource consumption, increase limits, or add backpressure.
"""

[[diagnostics]]
code = "KAIN-RUNTIME-0005"
title = "Deadlock Detected"
severity = "error"
docs_key = "runtime/deadlock"
help = """
The runtime detected a deadlock — two or more actors are waiting on each
other in a cycle, and no progress can be made.

Fix: restructure the message flow to avoid circular waits, or use
timeouts.
"""

[[diagnostics]]
code = "KAIN-RUNTIME-0006"
title = "World Initialization Failed"
severity = "error"
docs_key = "runtime/world-init"
help = """
A `world` could not be initialized. Surface creation, component
registration, or resource allocation may have failed.

Fix: check the world configuration and surface backend availability.
"""

[[diagnostics]]
code = "KAIN-RUNTIME-0007"
title = "Shader Dispatch Failed"
severity = "error"
docs_key = "runtime/shader-dispatch"
help = """
A GPU shader dispatch command failed. The GPU may be unavailable, the
shader may have crashed, or dispatch parameters may be invalid.

Fix: check GPU availability, shader validity, and dispatch dimensions.
"""

[[diagnostics]]
code = "KAIN-RUNTIME-0008"
title = "Timeout Exceeded"
severity = "error"
docs_key = "runtime/timeout"
help = """
An operation exceeded its time budget and was cancelled. This applies
to async operations, actor message waits, and compute dispatches.

Fix: increase the timeout budget or optimize the operation.
"""
```

## shader
```toml
# ── Kain Shader/GPU Error Codes ────────────────────────────────────────
# Shader blocks, compute kernels, vertex/fragment stages, GPU intrinsics.

[[diagnostics]]
code = "KAIN-SHADER-0001"
title = "Unsupported Shader Call"
severity = "error"
docs_key = "shader/unsupported-call"
help = """
A function or intrinsic is not available in the current shader stage.
Different shader stages (vertex, fragment, compute) expose different
intrinsic sets.

Fix: replace the call with a supported shader intrinsic or move the
computation to a compatible stage.
"""
see_also = ["KAIN-SHADER-0002"]

[[diagnostics]]
code = "KAIN-SHADER-0002"
title = "Shader Stage Mismatch"
severity = "error"
docs_key = "shader/stage-mismatch"
help = """
A value or operation that is only valid in one shader stage is being
used in a different stage. For example, vertex inputs cannot be
accessed from a fragment shader directly.

Fix: pass data through the appropriate stage interface (varying
parameters, uniform buffers, or shared memory).
"""

[[diagnostics]]
code = "KAIN-SHADER-0003"
title = "Uniform Binding Error"
severity = "error"
docs_key = "shader/uniform-binding"
help = """
A `uniform` declaration cannot be bound — the name conflicts with an
existing binding, the type is not GPU-compatible, or the binding slot
is already occupied.

Fix: use a unique binding name, ensure the type is GPU-compatible, and
check for slot conflicts.
"""

[[diagnostics]]
code = "KAIN-SHADER-0004"
title = "Compute Dispatch Dimension Error"
severity = "error"
docs_key = "shader/compute-dispatch"
help = """
A `compute` block specifies invalid dispatch dimensions. Compute shaders
require explicit thread group dimensions that must be positive and
within platform limits.

Fix: provide valid thread group counts within the platform's
maxComputeWorkGroupSize limits.
"""

[[diagnostics]]
code = "KAIN-SHADER-0005"
title = "Shader Resource Not GPU-Compatible"
severity = "error"
docs_key = "shader/resource-compat"
help = """
A resource (texture, buffer, sampler) used in a shader has a type or
format that is not supported by the GPU backend.

Fix: use a GPU-compatible format or convert the resource before binding.
"""

[[diagnostics]]
code = "KAIN-SHADER-0006"
title = "Vertex Input Layout Error"
severity = "error"
docs_key = "shader/vertex-input"
help = """
A `vertex` shader's input layout does not match the mesh or buffer
providing vertex data. Input attributes must agree in type, offset,
and count.

Fix: align the vertex input declaration with the mesh data layout.
"""

[[diagnostics]]
code = "KAIN-SHADER-0007"
title = "Fragment Output Layout Error"
severity = "error"
docs_key = "shader/fragment-output"
help = """
A `fragment` shader's output does not match the render target format.
Output color/depth attachments must have compatible pixel formats.

Fix: match the fragment output type to the render target configuration.
"""

[[diagnostics]]
code = "KAIN-SHADER-0008"
title = "Collapse Target Invalid"
severity = "error"
docs_key = "shader/collapse-invalid"
help = """
`collapse` reduces a parallel computation into a scalar, but the
reduction target or operator is not valid for the current shader model.

Fix: ensure the reduction operator is supported and the target type is
scalar-compatible.
"""

[[diagnostics]]
code = "KAIN-SHADER-0009"
title = "Fanout Width Exceeded"
severity = "error"
docs_key = "shader/fanout-width"
help = """
`fanout` distributes work across parallel lanes, but the fanout width
exceeds the GPU's wavefront/warp size or thread group limit.

Fix: reduce the fanout width or split across multiple waves.
"""

[[diagnostics]]
code = "KAIN-SHADER-0010"
title = "Shader Compilation Failed"
severity = "error"
docs_key = "shader/compilation"
help = """
The shader backend (HLSL/SPIR-V/Metal) could not compile the generated
shader code. This usually indicates the Kain lowering produced invalid
target code — check the shader output log for details.

Fix: simplify the shader code, check for target-specific restrictions,
or report the issue with the generated shader source.
"""

[[diagnostics]]
code = "KAIN-SHADER-0011"
title = "GPU Memory Budget Exceeded"
severity = "error"
docs_key = "shader/memory-budget"
help = """
The shader uses more GPU memory (registers, shared memory, constant
buffers) than the target hardware allows.

Fix: reduce register pressure by simplifying the shader, splitting into
multiple passes, or lowering the occupancy target.
"""

[[diagnostics]]
code = "KAIN-SHADER-0012"
title = "Shared Memory Bank Conflict"
severity = "warning"
docs_key = "shader/bank-conflict"
help = """
`share` memory access pattern may cause GPU shared memory bank
conflicts, reducing throughput. Reorganize data layout to avoid
concurrent access to the same bank.

Fix: pad shared memory arrays or restructure access patterns.
"""
```

## state
```toml
# ── Kain State Machine Error Codes ─────────────────────────────────────
# state, every, when, guarantee, fallback, pulse.

[[diagnostics]]
code = "KAIN-STATE-0001"
title = "State Error"
severity = "error"
docs_key = "state/general"
help = "A state machine well-formedness rule has been violated."

[[diagnostics]]
code = "KAIN-STATE-0002"
title = "State Machine Inexhaustive"
severity = "error"
docs_key = "state/inexhaustive"
help = """
A `state` machine does not handle all possible transitions. Every state
must have a defined transition for every possible input event, or a
`fallback` handler must be present.

Fix: add missing `when` clauses or a `fallback` handler.
"""
see_also = ["KAIN-TYPE-0013"]

[[diagnostics]]
code = "KAIN-STATE-0003"
title = "State Transition Cycle"
severity = "error"
docs_key = "state/cycle"
help = """
State transitions form a directed cycle without an escape path. The
state machine may loop indefinitely — every cycle should have a
reachable terminal state or a `decay` path.

Fix: add an exit condition, a `guarantee` of termination, or a decay
transition out of the cycle.
"""
see_also = ["KAIN-EFFECT-0009"]

[[diagnostics]]
code = "KAIN-STATE-0004"
title = "Invalid State Transition"
severity = "error"
docs_key = "state/invalid-transition"
help = """
A `when` clause references a target state that does not exist in the
state machine definition.

Fix: ensure the target state is declared.
"""
see_also = ["KAIN-STATE-0002"]

[[diagnostics]]
code = "KAIN-STATE-0005"
title = "Pulse Without State"
severity = "error"
docs_key = "state/pulse-no-state"
help = """
`pulse` triggers a state machine event, but the target component does
not have an active state machine or the state machine does not handle
the pulsed event.

Fix: ensure the target has a state machine that handles the event.
"""

[[diagnostics]]
code = "KAIN-STATE-0006"
title = "Guarantee Violation"
severity = "error"
docs_key = "state/guarantee-violation"
help = """
A `guarantee` clause asserts a property that does not hold. Guarantees
are verified statically, and this one cannot be proven by the compiler.

Fix: strengthen the precondition or weaken the guarantee, or restructure
the code to make the property provable.
"""

[[diagnostics]]
code = "KAIN-STATE-0007"
title = "Every Clause Unbounded"
severity = "warning"
docs_key = "state/every-unbounded"
help = """
An `every` clause defines a periodic behavior without an upper bound or
termination condition. This may run indefinitely.

Fix: add a termination guard or a bound on the iteration count.
"""

[[diagnostics]]
code = "KAIN-STATE-0008"
title = "Fallback Handler Unreachable"
severity = "warning"
docs_key = "state/fallback-unreachable"
help = """
A `fallback` handler is declared but all possible events are already
explicitly handled by `when` clauses. The fallback is dead code.

Fix: remove the unnecessary fallback or ensure it covers a real case.
"""
```

## test
```toml
# ── Kain Test/Spec Error Codes ─────────────────────────────────────────
# test, spec, fast, verify, random, jitter.

[[diagnostics]]
code = "KAIN-TEST-0001"
title = "Test Error"
severity = "error"
docs_key = "test/general"
help = "A test or specification framework invariant has been violated."

[[diagnostics]]
code = "KAIN-TEST-0002"
title = "Assertion Failed"
severity = "error"
docs_key = "test/assertion-failed"
help = """
An `assert` expression evaluated to `false` inside a `test` or `spec`
block. The condition is not satisfied for the given inputs.

Fix: correct the code or adjust the test expectation.
"""

[[diagnostics]]
code = "KAIN-TEST-0003"
title = "Spec Property Violated"
severity = "error"
docs_key = "test/spec-violated"
help = """
A `spec` block defines a property that does not hold. The property was
falsified by a counterexample found through `random` or `jitter` testing.

Fix: correct the implementation or narrow the spec to match the intended
behavior.
"""

[[diagnostics]]
code = "KAIN-TEST-0004"
title = "Fast Test Exceeded Time Budget"
severity = "warning"
docs_key = "test/fast-timeout"
help = """
A `fast` test exceeded its time budget. Fast tests should complete in
under 1ms — this test may be too heavy for the fast suite.

Fix: move the test to the standard suite or optimize the test body.
"""

[[diagnostics]]
code = "KAIN-TEST-0005"
title = "Verify Block Infallible"
severity = "warning"
docs_key = "test/verify-infallible"
help = """
A `verify` block can never fail — its condition is always true. This
may indicate a missing check or an over-constrained spec.

Fix: check that the verify condition is actually testing something
meaningful.
"""

[[diagnostics]]
code = "KAIN-TEST-0006"
title = "Random Seed Not Reproducible"
severity = "warning"
docs_key = "test/seed-missing"
help = """
`random` testing is used without an explicit seed. Failing runs may not
be reproducible. Add a seed for deterministic replay.

Fix: set a seed value or use `--test-seed` to capture the failing seed.
"""

[[diagnostics]]
code = "KAIN-TEST-0007"
title = "Jitter Range Invalid"
severity = "error"
docs_key = "test/jitter-range"
help = """
`jitter` specifies an invalid timing perturbation range. Jitter bounds
must be non-negative and within the test's timing tolerance.

Fix: adjust the jitter range to valid values.
"""
```

## type
```toml
# ── Kain Type Error Codes ──────────────────────────────────────────────
# Type-checking, name resolution, trait solving, and unification errors.

[[diagnostics]]
code = "KAIN-TYPE-0001"
title = "Type Error"
severity = "error"
docs_key = "types/general"
help = """
A type mismatch or type-inference failure occurred. This is the generic
fallback for type errors.
"""
see_also = ["KAIN-TYPE-0002"]

[[diagnostics]]
code = "KAIN-TYPE-0002"
title = "Unknown Identifier"
severity = "error"
docs_key = "types/unknown-identifier"
help = """
The name is not visible in the current scope. Common causes:
- Misspelling of a variable, function, or type name.
- Missing `use` import.
- The symbol exists only on the host/engine side and has not been
  bridged into Kain via the foreign-ABI layer.
- The symbol is defined inside a module that has not been imported.

Fix: check spelling, add a `use` statement, or bridge the host symbol.
"""
see_also = ["KAIN-TYPE-0004", "KAIN-TYPE-0010"]

[[diagnostics]]
code = "KAIN-TYPE-0003"
title = "World Requires Surface"
severity = "error"
docs_key = "world/missing-surface"
help = """
A `world` declaration must expose at least one surface projection so the
world can map components into a live host presentation surface.

Fix: add at least one surface projection such as `surface native_ui => MyPanel`
inside the world body.
"""
see_also = ["KAIN-WORLD-0001"]

[[diagnostics]]
code = "KAIN-TYPE-0004"
title = "Duplicate Symbol"
severity = "error"
docs_key = "types/duplicate-symbol"
help = """
The same name has been defined more than once in the same namespace.
Kain requires each visible symbol to have a unique name within its
scope.

Fix: rename one declaration or use an explicit alias on import.
"""
see_also = ["KAIN-TYPE-0005"]

[[diagnostics]]
code = "KAIN-TYPE-0005"
title = "Builtin Symbol Shadowed"
severity = "warning"
docs_key = "types/shadowed-builtin"
help = """
A user-defined name shadows a Kain builtin symbol. While allowed, this
can cause confusion — the builtin is no longer accessible under its
original name in this scope.

Fix: choose a distinct local name, or import the builtin under an alias.
"""

[[diagnostics]]
code = "KAIN-TYPE-0006"
title = "Missing Type Annotation"
severity = "error"
docs_key = "types/missing-annotation"
help = """
Kain requires explicit type annotations in positions where the type
cannot be inferred from context (top-level declarations, function
parameters without defaults, trait associated types in some positions).

Fix: add an explicit type annotation (`: TypeName`).
"""
example_bad = "let x"
example_good = "let x: i32 = 5"
see_also = ["KAIN-TYPE-0003"]

[[diagnostics]]
code = "KAIN-TYPE-0007"
title = "Trait Not Satisfied"
severity = "error"
docs_key = "types/trait-not-satisfied"
help = """
A type does not implement a required trait. Traits in Kain define
capability contracts that types must fulfill before they can be used
in generic contexts, effect-polymorphic functions, or GPU dispatch.

Fix: implement the missing trait methods for the type, or add a
`derive` annotation if the trait is derivable.
"""
see_also = ["KAIN-TYPE-0008", "KAIN-TYPE-0016"]

[[diagnostics]]
code = "KAIN-TYPE-0008"
title = "Trait Method Missing"
severity = "error"
docs_key = "types/trait-method-missing"
help = """
An `impl` block for a trait is missing one or more required methods.
Every method declared in the trait definition must have a concrete
implementation.

Fix: add the missing method(s) to the impl block.
"""
see_also = ["KAIN-TYPE-0007"]

[[diagnostics]]
code = "KAIN-TYPE-0009"
title = "Ambiguous Trait Implementation"
severity = "error"
docs_key = "types/ambiguous-trait"
help = """
Multiple trait implementations could satisfy a trait bound, and the
compiler cannot choose between them. This is the "coherence" problem.

Fix: use a fully-qualified path or add a type annotation to
disambiguate.
"""
see_also = ["KAIN-TYPE-0007"]

[[diagnostics]]
code = "KAIN-TYPE-0010"
title = "Unresolved Import"
severity = "error"
docs_key = "types/unresolved-import"
help = """
A `use` statement references a path that does not resolve to any known
module or symbol. The module may not exist, or the symbol may not be
`pub`.

Fix: check the module path and visibility of the target symbol.
"""
see_also = ["KAIN-TYPE-0002"]

[[diagnostics]]
code = "KAIN-TYPE-0011"
title = "Cyclic Type Definition"
severity = "error"
docs_key = "types/cyclic-definition"
help = """
A type definition refers to itself in a way that would require infinite
size. Kain detects cycles in struct fields, enum variants, and type
aliases.

Fix: break the cycle with indirection (e.g., a pointer or boxed type).
"""

[[diagnostics]]
code = "KAIN-TYPE-0012"
title = "Mutable/Immutable Conflict"
severity = "error"
docs_key = "types/mutability-conflict"
help = """
A value declared `let` (immutable) is being used in a position that
requires mutation, or vice versa.

Fix: change `let` to `let mut` if mutation is required, or remove the
mutation site.
"""
example_bad = "let x = 5\nx = 10"
example_good = "let mut x = 5\nx = 10"
see_also = ["KAIN-BORROW-0003"]

[[diagnostics]]
code = "KAIN-TYPE-0013"
title = "Pattern Match Inexhaustive"
severity = "error"
docs_key = "types/inexhaustive-match"
help = """
A `match` expression does not cover all possible variants of the
matched enum type. Kain requires exhaustive matching unless a wildcard
branch (`_`) or a `fallback` clause is present.

Fix: add missing variant arms or a wildcard catch-all.
"""
see_also = ["KAIN-STATE-0002"]

[[diagnostics]]
code = "KAIN-TYPE-0014"
title = "Recursive Type Without Indirection"
severity = "error"
docs_key = "types/recursive-without-indirection"
help = """
A struct or enum contains itself as a direct field, which would require
infinite memory. Use a pointer-like indirection (shared, weak, or a
reference) to break the cycle.

Fix: wrap the recursive field in a `shared` or heap-allocated container.
"""

[[diagnostics]]
code = "KAIN-TYPE-0015"
title = "Type Alias Cycle"
severity = "error"
docs_key = "types/alias-cycle"
help = """
A `type` alias expands to itself, directly or through a chain of other
aliases. Kain resolves aliases eagerly and detects cycles.

Fix: break the alias cycle.
"""

[[diagnostics]]
code = "KAIN-TYPE-0016"
title = "Impl On Foreign Type"
severity = "error"
docs_key = "types/foreign-impl"
help = """
An `impl` block implements a trait for a type that is not defined in
the current crate. By Kain's coherence rules, you can only implement
your own traits for foreign types, or foreign traits for your own types
— not both foreign.

Fix: create a newtype wrapper or define the trait in your crate.
"""

[[diagnostics]]
code = "KAIN-TYPE-0017"
title = "Self Type In Static Context"
severity = "error"
docs_key = "types/self-in-static"
help = """
`Self` or `self` was used outside of a trait, impl block, or method
context where it has no meaning.

Fix: use a concrete type name instead of `Self`.
"""

[[diagnostics]]
code = "KAIN-TYPE-0018"
title = "Invalid Type Parameter Count"
severity = "error"
docs_key = "types/param-count"
help = """
A generic type or function was supplied with the wrong number of type
arguments. Check the definition for the expected arity.

Fix: provide the correct number of type arguments.
"""

[[diagnostics]]
code = "KAIN-TYPE-0019"
title = "Type Argument Kind Mismatch"
severity = "error"
docs_key = "types/arg-kind-mismatch"
help = """
A type argument does not satisfy the kind constraints of the generic
parameter. For example, a type parameter constrained by a trait was
given a type that does not implement that trait.

Fix: ensure the type argument satisfies all bounds.
"""
see_also = ["KAIN-TYPE-0007"]

[[diagnostics]]
code = "KAIN-TYPE-0020"
title = "Return Type Mismatch"
severity = "error"
docs_key = "types/return-mismatch"
help = """
The body of a function produces a value whose type does not match the
declared return type. Every exit path (including early returns and the
implicit tail expression) must agree with the annotation.

Fix: correct the return type annotation or the returned value.
"""
example_bad = "fn answer: i32 { \"forty-two\" }"
example_good = "fn answer: i32 { 42 }"

[[diagnostics]]
code = "KAIN-TYPE-0021"
title = "Missing Return In Non-Void Function"
severity = "error"
docs_key = "types/missing-return"
help = """
A function with a non-void return type does not return a value on at
least one control-flow path. Every branch must either return, diverge,
or produce a tail expression.

Fix: add a return statement or ensure the tail expression matches the
declared type.
"""

[[diagnostics]]
code = "KAIN-TYPE-0022"
title = "Void Value Used In Expression"
severity = "error"
docs_key = "types/void-in-expression"
help = """
A value of type `void` or `none` is being used in a position that
expects a meaningful value (e.g., assigned to a typed variable, passed
as a non-void argument).

Fix: remove the void-producing expression or wrap it in a block that
returns a meaningful value.
"""

[[diagnostics]]
code = "KAIN-TYPE-0023"
title = "Callable Type Expected"
severity = "error"
docs_key = "types/not-callable"
help = """
A value was used in a function-call position but its type is not
callable (not a function, closure, or object with an `invoke` method).

Fix: check that the name refers to a function, not a non-callable
variable.
"""
example_bad = "let x = 5\nx()"
example_good = "let x = fn: { 5 }\nx()"

[[diagnostics]]
code = "KAIN-TYPE-0024"
title = "Field Not Found"
severity = "error"
docs_key = "types/field-not-found"
help = """
The struct or component type does not have a field with the given name.
Check the type definition for the correct field names.

Fix: use a field that exists on the type, or check for misspelling.
"""
see_also = ["KAIN-TYPE-0002"]

[[diagnostics]]
code = "KAIN-TYPE-0025"
title = "Type Mismatch"
severity = "error"
docs_key = "types/mismatch"
help = """
The inferred type does not match the expected type in this position.
Kain uses structural typing for most constructs but enforces nominal
matching for traits and effect-polymorphic boundaries.

Fix: check whether a type annotation is needed, or whether the value
needs an explicit conversion (via `as`).
"""
example_bad = "let x: i32 = \"hello\""
example_good = "let x: i32 = 42"
see_also = ["KAIN-TYPE-0005", "KAIN-TYPE-0012"]

[[diagnostics]]
code = "KAIN-TYPE-0026"
title = "Index Not Supported"
severity = "error"
docs_key = "types/index-not-supported"
help = """
The type does not support indexing with `[]`. Only arrays, maps, and
types that implement the `Index` trait can be indexed.

Fix: use a supported container type or implement the `Index` trait.
"""
```

## validation
```toml
# ── Kain Validation Error Codes ─────────────────────────────────────────
# Cross-pass validation and structural certification failures.

[[diagnostics]]
code = "KAIN-VALIDATE-0001"
title = "Validation Error"
severity = "error"
docs_key = "validation/general"
help = """
A structural validation pass rejected the program. This is the generic
validation fallback used when a later pass proves a construct is not
well-formed even though parsing and basic typing succeeded.

Fix: inspect the attached validation context and repair the violated
invariant before lowering or runtime codegen continues.
"""
```

## world
```toml
# ── Kain World/Surface Error Codes ─────────────────────────────────────
# World declarations, surface projections, entanglement, teleportation.

[[diagnostics]]
code = "KAIN-WORLD-0001"
title = "World Missing Surface"
severity = "error"
docs_key = "world/missing-surface"
help = """
A `world` block must contain at least one `surface` projection that maps
Kain UI components to a rendering backend (native_ui, viewport3d, web,
ue5).

Fix: add at least one surface projection inside the world body.
"""
example_bad = "world MyWorld: {}"
example_good = "world MyWorld:\n    surface native_ui => MainPanel"

[[diagnostics]]
code = "KAIN-WORLD-0002"
title = "Duplicate Surface Kind"
severity = "error"
docs_key = "world/duplicate-surface"
help = """
Multiple surfaces of the same kind have been declared in one world. Each
surface kind may only appear once per world.

Fix: merge the surface projections or use separate worlds for different
surface instances of the same kind.
"""

[[diagnostics]]
code = "KAIN-WORLD-0003"
title = "Surface Component Type Error"
severity = "error"
docs_key = "world/surface-component-type"
help = """
The type projected onto a surface must be a valid Kain `component` that
implements the surface's rendering protocol. The supplied type is either
not a component or does not satisfy the required trait bounds.

Fix: ensure the projected type is a `component` with appropriate
rendering implementations.
"""

[[diagnostics]]
code = "KAIN-WORLD-0004"
title = "World Orphan"
severity = "error"
docs_key = "world/orphan"
help = """
A `world` declaration is not referenced by any entry point, `spawn`
site, or host embedding. Unreferenced worlds are dead code.

Fix: spawn the world from a `main` function or export it for host
consumption.
"""

[[diagnostics]]
code = "KAIN-WORLD-0005"
title = "Entanglement Target Invalid"
severity = "error"
docs_key = "world/entanglement-invalid"
help = """
`entangle` creates a bidirectional link between two components, but the
target component does not exist in the world or does not support
entanglement.

Fix: ensure both components exist in the same world and implement the
Entangle trait.
"""
see_also = ["KAIN-WORLD-0006"]

[[diagnostics]]
code = "KAIN-WORLD-0006"
title = "Teleport Destination Invalid"
severity = "error"
docs_key = "world/teleport-invalid"
help = """
`teleport` moves a component to a different world or surface, but the
destination world/surface does not exist or does not accept the
component type.

Fix: ensure the destination world is declared and accepts the component
type being teleported.
"""

[[diagnostics]]
code = "KAIN-WORLD-0007"
title = "World Cross-Reference Cycle"
severity = "error"
docs_key = "world/cross-reference"
help = """
Two or more worlds reference each other in a way that creates a
dependency cycle (e.g., through entanglement or teleport targets).

Fix: break the cycle by introducing an intermediary or making one
reference directional.
"""

[[diagnostics]]
code = "KAIN-WORLD-0008"
title = "Surface Kind Platform Mismatch"
severity = "error"
docs_key = "world/platform-mismatch"
help = """
A surface kind (e.g., `ue5`, `native_ui`) is not supported on the
current compilation target or platform. Surface kinds are
target-gated — `ue5` only works on UE5 host targets, `web` only on
WASM/web targets.

Fix: change the surface kind or the compilation target.
"""
```

