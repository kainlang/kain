<div align="center">

```text
           ▄█   ▄█▄   ▄████████   ▄█   ███▄▄▄▄
           ███ ▄███▀   ███    ███   ███   ███▀▀▀██▄
           ███▐███▀    ███    ███   ███▌  ███   ███
          ▄█████▀      ███    ███   ███▌  ███   ███
         ▀▀█████▄      ██████████   ███▌  ███   ███
           ███▐███▄    ███    ███   ███   ███   ███
           ███ ▀███▄   ███    ███   █▀    ███   ███
           ██    ▀█▀   ██     ██           ▀█   █▀

       
```
KAIN is an all new way to program. Its surface looks familiar (Python-like syntax, Rust-like safety), but its innovation is a **compiler-owned semantic stack**: 15+ constructs where the compiler -- not the programmer -- owns the truth about state, mutation, dispatch, timing, coupling, layout, and handoff.

### Paradigm Lineage

| Paradigm | Inspiration | Kain Implementation |
|----------|------------|-------------------|
| **Safety** | Rust | Explicit ownership via `collapse`/`observe`/`decay` (no borrow checker inference), no null, no data races, `Unsafe` effect gates raw operations |
| **Syntax** | Python | Significant newlines as statement terminators, minimal ceremony, `:` for blocks |
| **Metaprogramming** | Lisp | Hygienic macros (`macro name!(param: kind):`), code-as-data, DSL-friendly surface |
| **Compile-Time** | Koka/Eff, Zig | `comptime` blocks, effect system (`Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`), no separate macro language |
| **Concurrency** | Erlang | First-class `actor` with typed message contracts, `spawn`/`send`/`ask`, supervision trees, mailbox backpressure |
| **State Management** | Novel | `world` (compiler-owned state authority), `entangle` (bidirectional state sync), `patch` (journaled mutation), `law` (invariant predicates) |
| **Dispatch** | Novel | `converge` (spec + platform-gated fast lanes with `verify random(N)` fuzzing), `orchestrate` (typed multi-runtime stage graphs: CPU→GPU→law→patch→world) |
| **Temporal** | Novel | `pulse` (jitter-tolerant timed recurrence), `resonate` (compiler-owned reactive tripwires with dampening) |
| **Memory Layout** | Novel | `shatter struct` (Structure-of-Arrays layout intent), `teleport` (zero-copy cross-world handoff), `axiom` (capability assumptions with fallback) |
| **GPU** | CUDA/Vulkan/HLSL | First-class `shader` (vertex/fragment/compute), `dispatch` keyword, SPIR-V/PTX/HLSL/WGSL emission, `StorageBuffer`, `workgroup` |
| **UI** | React/JSX | Native `component` with typed props, local state, methods, JSX composition (`<Component />` dispatch by case), `for`/`if` in JSX |
| **FFI** | Novel | `include <windows.h> as win` (605 functions from real SDK via libclang), `include <vulkan/vulkan.h> as vk` (755 functions), `import` for Python, `use rust::` for Rust crates |
| **Targets** | Universal | LLVM native (.exe/.dll/.lib), WASM, SPIR-V shaders, PTX (CUDA), HLSL, WGSL, Rust/C++ transpilation, JavaScript/TypeScript, UE5 C++ |

### The Compiler-Owned Semantic Stack

Kain's core innovation is the **decision ladder** -- 8 layers of compiler-owned constructs above plain code:

```
LAYER 7: SYSTEMS     actor · collapse/observe/decay
LAYER 6: MACHINE     axiom · shatter · teleport
  STONES
LAYER 5: TEMPORAL    pulse · resonate
LAYER 4: STAGE       orchestrate
  GRAPH
LAYER 3: DISPATCH    converge
LAYER 2: STATE       patch · law
  INTEGRITY
LAYER 1: STATE       world · entangle
  AUTHORITY
LAYER 0: PLAIN       fn · struct · let · enum · trait · impl
  CODE
```

When you use `fn` and `let` for a problem that should be a `world`, `patch`, `converge`, or `pulse`, you're paying the semantic cost without getting the compiler's help. The ladder exists so the compiler can reason about, optimize, and prove properties of your program that it can't see in plain code.

### The Runtime

Underneath the language sits a **portable C11 native runtime** (`runtime/native/`) -- providing the execution substrate: arena/buddy allocators, full actor scheduler with supervision trees, async task/future runtime, GPU compute dispatch (Vulkan + CUDA/PTX), ownership state machine, machine-stones substrate (axiom/pulse/shatter/teleport), crash forensics with compiler-emitted symbol tables, and a 50-service registry. Verified with **hundreds of Z3 proof packs** and **10,000+ CBMC assertions** (arena + actor subsystems proven exhaustively).
