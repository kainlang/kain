# LLVM_REFACTOR.md — Stripped, Modernized, JIT-First LLVM/Clang for Kain

**Date:** 2026-06-28
**Status:** Architecture proposal — clean folder layout + scope definition
**Sources:** `LLVM-NEXT-GEN-PART1-6.md`, `SYNTHESIS_PART_1.md`, `MAP.MD`, `map-1-11.md`, `codegen_analysis.md`
**Cross-ref:** `X:/research/llvm/codegen_analysis.md`, `X:/crates/sys-codegen/src/codegen_llvm/`

---

## 1. The Premise

Current vendor: `X:/llvm-project/` — **8.4 GB, 2,321 directories, 350K+ files.** Kain uses ~3% of it.

Target: a **~300 MB** self-contained LLVM/Clang with:
- X86 + AArch64 targets only
- C-only Clang (C11/C17/C23)
- ORC JIT as primary output (no object files, no linker)
- Static linkage into Kain's Rust compiler
- Clean folder layout (no 90s-style nested `lib/CodeGen/GlobalISel/Legalizer/` hell)

---

## 2. Current vs Proposed Size

| Component | Current | Proposed | Savings |
|-----------|---------|----------|---------|
| Targets (25 → 2) | 170 MB | 37 MB | 78% |
| LLVM libraries | 377 MB | ~138 MB | 63% |
| Clang | 173 MB | ~42 MB | 76% |
| flang/mlir/lldb/libcxx/polly/bolt | ~1.8 GB | 0 | 100% |
| **Total** | **~8.4 GB** | **~300 MB** | **96%** |

---

## 3. Clean Folder Layout

### Current (Painful 90s Layout)

```
llvm-project/
├── llvm/
│   ├── lib/
│   │   ├── CodeGen/
│   │   │   ├── GlobalISel/
│   │   │   │   ├── Legalizer/
│   │   │   │   │   └── LegalizerInfo.cpp
│   │   │   ├── SelectionDAG/
│   │   │   │   ├── DAGCombiner.cpp
│   │   ├── Target/
│   │   │   ├── X86/
│   │   │   │   ├── X86ISelLowering.cpp
│   │   │   ├── AArch64/
│   │   │   │   ├── AArch64ISelLowering.cpp
│   │   │   ├── AMDGPU/  ← dead
│   │   │   ├── ARM/      ← dead
│   │   │   ├── ...23 more dead targets...
│   │   ├── Transforms/
│   │   │   ├── Instrumentation/  ← sanitizers, dead
│   │   │   ├── ObjCARC/         ← dead
│   │   │   ├── Coroutines/      ← dead
│   │   ├── ExecutionEngine/
│   │   │   ├── Orc/
│   │   │   ├── JITLink/
│   │   │   ├── MCJIT/           ← dead
│   │   │   ├── RuntimeDyld/     ← dead
│   ├── include/
│   │   ├── llvm/
│   │   │   ├── CodeGen/
│   │   │   ├── Target/
│   │   │   ├── ...
├── clang/
│   ├── lib/
│   │   ├── CodeGen/    ← 166K lines, 88K essential
│   │   ├── Sema/       ← 330K lines, 117K essential
│   │   ├── Parse/      ← 50K lines, 25K essential
│   │   ├── AST/        ← 170K lines, 57K essential
│   │   ├── Driver/     ← dead (Kain IS the driver)
│   │   ├── StaticAnalyzer/  ← dead
│   │   ├── Format/     ← dead
│   │   ├── Tooling/    ← dead
├── flang/      ← dead
├── mlir/       ← dead
├── lldb/       ← dead
├── libcxx/     ← dead
├── polly/      ← dead
├── bolt/       ← dead
├── compiler-rt/ (non-builtins)  ← dead
└── __REFACTOR__/  ← our docs, keep
```

### Proposed (Clean, Flat, Kain-Style)

```
llvm-kain/
├── README.md                     ← This document
├── build.kn                      ← Kain build authority (or CMakeLists.txt for bootstrap)
│
├── src/
│   ├── core/                     ← LLVM IR, passes, analysis (~138 MB → ~50 MB stripped)
│   │   ├── ir/                   ←   Type, Value, Instruction, Module, Constants
│   │   ├── passes/               ←   InstCombine, SimplifyCFG, Inline, Mem2Reg, DCE, GVN, LICM
│   │   ├── analysis/             ←   AliasAnalysis, LoopInfo, ScalarEvolution, Dominators
│   │   └── support/              ←   ADT (SmallVector, StringMap, etc.), Math, FileSystem
│   │
│   ├── target/
│   │   ├── x86/                  ←   ISel, InstPrinter, AsmParser, MCTargetDesc, Subtarget
│   │   ├── aarch64/              ←   Same structure
│   │   └── shared/               ←   GlobalISel, common codegen (MachineFunction, RegAlloc, etc.)
│   │
│   ├── jit/                      ← ORC JIT + JITLink
│   │   ├── orc/                  ←   CompileLayer, ObjectLinkingLayer, LazyCompile
│   │   └── jitlink/              ←   Memory manager, symbol resolution, relocation
│   │
│   └── support/                  ← Shared infrastructure
│       ├── adt/                  ←   SmallVector, DenseMap, StringRef, ArrayRef
│       ├── math/                 ←   APInt, APFloat
│       ├── debug/                ←   DWARF emission (Kain uses textual DIBuilder, but keep)
│       └── target/               ←   Triple, DataLayout (cross-compilation info)
│
├── include/                      ← Public headers (mirrors src/ structure)
│   ├── core/
│   ├── target/x86/
│   ├── target/aarch64/
│   ├── jit/
│   └── support/
│
├── clang/                        ← C-only Clang (~42 MB → ~25 MB stripped)
│   ├── src/
│   │   ├── parse/                ←   C parser (25K lines essential)
│   │   ├── sema/                 ←   C semantic analysis (117K lines essential)
│   │   ├── codegen/              ←   C → LLVM IR codegen (88K lines essential)
│   │   ├── ast/                  ←   C AST nodes (57K lines essential)
│   │   └── lex/                  ←   C lexer (preprocessor, tokens)
│   │
│   └── include/                  ← Public headers
│       ├── parse/
│       ├── sema/
│       ├── codegen/
│       └── ast/
│
├── rt/                           ← Runtime (compiler-rt builtins only)
│   ├── builtins/                 ←   udivdi3, memcpy, float ops — what LLVM codegen needs
│   └── crt/                      ←   crtbegin.o, crtend.o stubs
│
├── tools/                        ← CLI tools (minimal)
│   ├── llc/                      ←   LLVM IR → native (kept for debugging .ll files)
│   └── opt/                      ←   LLVM optimizer standalone (kept for pass debugging)
│
├── tests/                        ← Test suite (curated)
│   ├── core/                     ←   IR tests, pass tests
│   ├── target/x86/               ←   X86 codegen tests
│   ├── target/aarch64/           ←   AArch64 codegen tests
│   └── clang/                    ←   C compilation tests
│
├── __REFACTOR__/                 ← Our planning docs (keep)
├── __ORIGINAL__/                 ← Symlink or note pointing to X:/llvm-project/ (for diffing)
│
└── vendor/                       ← Third-party (minimal)
    └── googletest/               ←   Test framework
```

### Key Differences from Current Layout

| Aspect | Current (LLVM 21) | Proposed |
|--------|------------------|----------|
| Depth | `lib/CodeGen/GlobalISel/Legalizer/LegalizerInfo.cpp` (5 levels) | `src/target/shared/globalisel/` (3 levels) |
| Naming | CamelCase directories (`GlobalISel`, `SelectionDAG`) | flat lowercase (`globalisel`, `x86`, `jit`) |
| Headers | Mixed with implementation (`include/llvm/CodeGen/`) | Mirrors `src/` structure exactly (`include/core/`) |
| Target separation | Each target is a flat dump of 100+ files | `target/x86/` contains everything X86 |
| JIT | Buried in `lib/ExecutionEngine/Orc/` | Top-level `src/jit/` — it's a first-class concern |
| Dead code | 20 dead targets, 10 dead libraries interleaved with live code | Only live code exists. Dead code deleted. |
| Build | 2,321 CMakeLists.txt files, recursive | Single `build.kn` or flat CMakeLists.txt |

---

## 4. What Dies — Complete Kill List

### 4.1 Dead Targets (20 dropped, 2 kept)

| Target | Size | Fate | Reason |
|--------|------|------|--------|
| **X86** | 19 MB | KEEP | Primary Kain target |
| **AArch64** | 18 MB | KEEP | macOS ARM64, Linux ARM64 |
| AMDGPU | 23 MB | DROP | Kain has own GPU backend |
| RISCV | 14 MB | DROP | Not yet needed |
| ARM | 13 MB | DROP | ARM32 — not a Kain target |
| Hexagon | 13 MB | DROP | Qualcomm DSP |
| PowerPC | 10 MB | DROP | Legacy |
| Mips | 8.4 MB | DROP | Dead architecture |
| WebAssembly | 6.5 MB | DROP | Kain has own WASM emitter |
| SystemZ | 5.8 MB | DROP | IBM mainframe |
| NVPTX | 4.6 MB | DROP | Kain has own PTX backend |
| SPIRV | 4.7 MB | DROP | Kain uses rspirv crate |
| LoongArch | 4.0 MB | DROP | China-only |
| M68k | 3.2 MB | DROP | Dead since 1990s |
| Sparc | 3.0 MB | DROP | Dead |
| VE | 3.0 MB | DROP | NEC vector engine |
| DirectX | 2.7 MB | DROP | Kain has own HLSL emitter |
| CSKY | 2.8 MB | DROP | Chinese embedded |
| BPF | 2.5 MB | DROP | Kernel bytecode |
| AVR | 2.5 MB | DROP | Arduino |
| Xtensa | 2.3 MB | DROP | ESP32 |
| Lanai | 2.1 MB | DROP | Google abandoned |
| MSP430 | 1.8 MB | DROP | TI microcontroller |
| XCore | 1.6 MB | DROP | XMOS |
| ARC | 1.5 MB | DROP | ARC embedded |

### 4.2 Dead LLVM Libraries

| Library | Size | Reason |
|---------|------|--------|
| `DebugInfo/` all | 11 MB | Kain emits DWARF via DIBuilder textual IR |
| `Transforms/Instrumentation/` | 2.8 MB | Sanitizer passes |
| `Transforms/ObjCARC/` | 0.9 MB | Apple ObjC |
| `Transforms/Coroutines/` | 0.9 MB | C++20 coroutines |
| `ExecutionEngine/MCJIT/` | ~2 MB | Legacy JIT |
| `ExecutionEngine/RuntimeDyld/` | ~1.5 MB | Legacy dynamic linker |
| `LTO/` + `DTLTO/` | 0.6 MB | Link-time optimization |
| `ObjCopy/` | ~3 MB | Object file manipulation |
| `ObjectYAML/` | ~2 MB | YAML object serialization |
| `MCA/` | ~3 MB | Machine code analyzer |
| `XRay/` | ~1 MB | Instrumentation |
| `DWARFLinker/` | ~1 MB | DWARF linking |
| `Frontend/` | 1.4 MB | OpenMP/OpenACC/HLSL |
| `SandboxIR/` | ~1 MB | Sandboxed IR |
| `CAS/` | ~1 MB | Content-addressed storage |
| `CGData/` | ~0.5 MB | CodeGen data |
| `FuzzMutate/` | ~0.5 MB | IR fuzzer |
| All dead target includes | 1.4 MB | Dead target headers |
| Misc small dirs | ~5 MB | Various |

### 4.3 Dead Clang Libraries

| Library | Lines | Reason |
|---------|-------|--------|
| C++ Sema/CodeGen/Parse/AST | ~212K | No C++ support |
| ObjC Sema/CodeGen | ~18K | No ObjC support |
| OpenMP Sema/CodeGen | ~40K | No OpenMP support |
| HLSL Sema/CodeGen | ~11K | No HLSL support |
| CUDA CodeGen | ~2K | No CUDA support |
| Coroutines | ~1.2K | No C++20 coroutines |
| Driver/ | 2.6 MB | Kain IS the driver |
| StaticAnalyzer/ | 3.7 MB | Kain has own semantic diagnostics |
| CIR/ | 3.3 MB | MLIR dialect |
| Format/ | 1.4 MB | clang-format |
| Tooling/ | 0.8 MB | LibTooling |
| Headers intrinsic bloat | ~7 MB | arm_neon.h, altivec.h, etc. |
| All tools/ | 10 MB | clang-format, clangd, clang-tidy |

### 4.4 Dead Entire Projects

| Project | Size | Reason |
|---------|------|--------|
| flang/ | 173 MB | Fortran frontend |
| mlir/ | 297 MB | MLIR framework |
| lldb/ | 287 MB | Debugger |
| libcxx/ | 386 MB | C++ standard library |
| libc/ | 100 MB | C standard library (we use platform libc) |
| clang-tools-extra/ | 134 MB | clangd, clang-tidy, etc. |
| polly/ | 83 MB | Polyhedral optimizer |
| compiler-rt non-builtins | ~200 MB | Sanitizers, profilers, fuzzers |
| bolt/ | 40 MB | Binary optimizer |
| openmp/ | 28 MB | OpenMP runtime |
| offload/ | 21 MB | GPU offloading |
| third-party/ | 41 MB | Vendored deps |
| orc-rt/ | 5.4 MB | ORC runtime (keep only what JIT needs) |
| libunwind/ | 3.2 MB | Unwinder |
| libsycl/ | 3.3 MB | SYCL runtime |

---

## 5. What Stays — Complete Keep List

### 5.1 LLVM Core (~138 MB → ~50 MB after dead code removal)

| Module | What | Why |
|--------|------|-----|
| `IR/` | Module, Function, BasicBlock, Instruction, Constant, Type, DataLayout | Foundation |
| `Passes/` | InstCombine, SimplifyCFG, Inline, Mem2Reg, DCE, GVN, LICM, LoopUnroll, SROA, EarlyCSE | Optimization pipeline |
| `Analysis/` | AliasAnalysis, LoopInfo, ScalarEvolution, Dominators, MemorySSA, AssumptionCache, TargetTransformInfo | Pass dependencies |
| `CodeGen/` (shared) | MachineFunction, RegisterAllocation, GlobalISel, AsmPrinter, MC layer | Backend infrastructure |
| `Target/X86/` | ISel, Subtarget, InstPrinter, AsmParser, MCTargetDesc | Primary target |
| `Target/AArch64/` | Same as X86 | Secondary target |
| `ExecutionEngine/Orc/` | CompileLayer, ObjectLinkingLayer, LazyCompile, EPC, IndirectionUtils | JIT engine |
| `ExecutionEngine/JITLink/` | Memory manager, ELF/MachO/COFF linkers, relocation | JIT memory |
| `Support/` | ADT, FileSystem, Math, Allocator, Error, CommandLine, raw_ostream | Infrastructure |
| `MC/` | Assembler, Disassembler, MCObjectWriter | Object file emission |
| `Object/` | ELF, MachO, COFF readers (for JIT) | Object file parsing |
| `ProfileData/` | Coverage mapping | Minimal |
| `Bitcode/` | Reader + Writer (for pre-compiled runtime .bc) | Keep |
| `DebugInfo/` | DWARF → textual DIBuilder only, not the full library | Kain does its own DWARF |

### 5.2 Clang C-Only (~42 MB → ~25 MB)

| Module | Lines | Why |
|--------|-------|-----|
| Sema C | 117K | `SemaDecl`, `SemaExpr`, `SemaStmt`, `SemaType`, `SemaInit`, `SemaChecking` |
| CodeGen C | 88K | `CGExpr`, `CGStmt`, `CGDecl`, `CGCall`, `CGBuiltin`, `CGAtomic` |
| Parse C | 25K | C parser (not C++) |
| AST C | 57K | C AST nodes, types, declarations |
| Lex | 20K | Preprocessor, tokens, pragmas |
| Basic | 15K | Targets, diagnostics, source locations, identifiers |
| Headers | ~5K | Platform ABI (X86, AArch64) |

### 5.3 Tools (Minimal)

| Tool | Why |
|------|-----|
| `llc` | Debug `.ll` files without Kain |
| `opt` | Debug optimization passes |

### 5.4 Runtime

| Component | Why |
|-----------|-----|
| `compiler-rt/builtins/` | Integer division, float ops, memcpy — LLVM codegen emits calls to these |
| `crt/` stubs | C runtime begin/end |

---

## 6. Build System — Flat CMakeLists.txt

Current LLVM uses recursive CMake with 2,321 `CMakeLists.txt` files. Proposed: single flat build file.

```cmake
# build.kn (Kain) or CMakeLists.txt (bootstrap)
# Target: libLLVM-Kain.a + libClang-Kain.a + llc + opt

# ── LLVM Core ──────────────────────────────────────────────────
llvm_library(llvm-core
    src/core/ir/*.cpp
    src/core/passes/*.cpp
    src/core/analysis/*.cpp
    src/core/support/*.cpp
)

# ── Shared CodeGen ─────────────────────────────────────────────
llvm_library(llvm-codegen-shared
    src/target/shared/*.cpp
)

# ── Targets ────────────────────────────────────────────────────
llvm_library(llvm-x86
    src/target/x86/*.cpp
    LINK llvm-core llvm-codegen-shared
)

llvm_library(llvm-aarch64
    src/target/aarch64/*.cpp
    LINK llvm-core llvm-codegen-shared
)

# ── JIT ────────────────────────────────────────────────────────
llvm_library(llvm-jit
    src/jit/orc/*.cpp
    src/jit/jitlink/*.cpp
    LINK llvm-core llvm-x86 llvm-aarch64
)

# ── Clang C-Only ───────────────────────────────────────────────
llvm_library(clang-c
    clang/src/lex/*.cpp
    clang/src/parse/*.cpp
    clang/src/sema/*.cpp
    clang/src/ast/*.cpp
    clang/src/codegen/*.cpp
    LINK llvm-core
)

# ── Runtime builtins ───────────────────────────────────────────
llvm_library(llvm-rt
    rt/builtins/*.c
)

# ── Tools ──────────────────────────────────────────────────────
llvm_executable(llc tools/llc/*.cpp LINK llvm-core llvm-x86 llvm-aarch64)
llvm_executable(opt tools/opt/*.cpp LINK llvm-core)
```

---

## 7. ORC JIT Integration with Kain's Codegen

The current flow:
```
LlvmGenerator::generate() → String → .ll file → clang → .exe    (2-5 seconds)
```

The proposed flow:
```
LlvmGenerator::generate() → String → ORC JIT → exec memory       (85ms)
```

### What Changes in `crates/sys-codegen/`

```rust
// mod.rs — existing
pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    // ... current textual emission ...
    Ok(generator.output.into_bytes())
}

// NEW: ORC JIT output
pub fn generate_jit(program: &TypedProgram) -> KainResult<JitModule> {
    let mut generator = LlvmGenerator::new(program)?;
    generator.compile_module(program)?;
    let llvm_text = generator.output;  // Same IR text!
    
    // Feed to in-process ORC JIT instead of writing to file
    let jit = orc_jit_compile(&llvm_text)?;
    Ok(JitModule { handle: jit })
}
```

Same `LlvmGenerator`. Same textual IR. Different output target. The ORC JIT takes the exact same `.ll` text and compiles it in-process instead of spawning `clang`.

### ORC JIT API (Rust bindings)

```rust
// Thin wrapper around LLVM-C ORC API
struct KainJit {
    session: OrcSession,       // LLVMOrcCreateLLJITBuilder
    dylib:   OrcDylib,         // Main JIT'd library
}

impl KainJit {
    fn add_module(&self, llvm_text: &str) -> Result<*const u8> {
        // Parse text → Module
        // Add to JIT dylib
        // Look up main or entry point
        // Return function pointer
    }
    
    fn lookup(&self, symbol: &str) -> Result<*const u8> {
        // LLVMOrcLLJITLookup → function pointer
    }
}
```

---

## 8. Phase Plan

### Phase 1: Strip Dead Targets (Weekend 1)
- Delete 20 dead target directories from `llvm/lib/Target/`
- Fix `CMakeLists.txt` to not reference them
- Verify X86 + AArch64 still compile
- **Savings:** ~133 MB

### Phase 2: Strip Clang to C-Only (Weekend 2)
- Delete C++/ObjC/OpenMP/HLSL/CUDA Sema, CodeGen, Parse, AST
- Delete Driver, StaticAnalyzer, Format, Tooling
- Fix `CodeGenFunction.h` to not include dead headers
- **Savings:** ~131 MB

### Phase 3: Delete Dead LLVM Libraries (Weekend 3)
- Delete legacy pass manager, SelectionDAG dead paths, MCJIT, RuntimeDyld
- Delete Instrumentation, ObjCARC, Coroutines transforms
- Delete ObjCopy, ObjectYAML, MCA, XRay, DWARFLinker
- Delete LTO, Frontend, SandboxIR, CAS, CGData, FuzzMutate
- **Savings:** ~42 MB

### Phase 4: Delete Dead Entire Projects (Weekend 4)
- Delete flang, mlir, lldb, libcxx, libc, polly, bolt
- Delete clang-tools-extra, compiler-rt non-builtins
- Delete openmp, offload, third-party
- **Total repo size:** ~300 MB

### Phase 5: Restructure to Clean Layout (Week 5-6)
- Flatten directory hierarchy from 5 levels → 3 levels
- Rename to lowercase flat names
- Rebuild CMakeLists.txt as single flat build file
- Verify all tests pass

### Phase 6: ORC JIT Integration (Week 7-8)
- Add ORC JIT output target to `LlvmGenerator`
- Wire into `kain run` → 85ms compiles
- Add hot-swap: recompile single function, patch vtable, no restart
- Benchmark vs current clang subprocess path

### Phase 7: Pre-Compiled Runtime (Week 9)
- Compile `runtime/native/src/core/*.c` to LLVM bitcode once
- Ship `.bc` files with compiler
- Link at JIT time — no C compiler needed for runtime

---

## 9. What Does NOT Change

| Component | Fate | Reason |
|-----------|------|--------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | **UNCHANGED — THIS IS KNIR** | 21,790 lines of dep-free Rust that emit textual LLVM IR. No LLVM library linkage. Pure string builder with a monotonic SSA counter. This IS the Kain-Native IR layer — it already knows about worlds, entangles, actors, collapses, and everything else because `TypedProgram` carries them and the generator writes them as LLVM IR patterns. The Next-Gen PART3/5/6 docs envisioned a separate `crates/knir/` crate — but the codegen already IS that semantic layer, just lowered to text instead of an in-memory IR forest. |
| `crates/sys-codegen/src/codegen_llvm/component.rs` | **UNCHANGED** | Already emits vtable calls correctly. |
| `crates/core/src/` parser, typechecker, AST | **UNCHANGED** | Frontend is independent of LLVM backend. |
| Kain's SPIR-V/PTX/HLSL/WGSL backends | **UNCHANGED** | Already implemented in `crates/gpu/` and `crates/shader-text/`. |
| Kain's C FFI (`crates/c-ffi/`) | **UNCHANGED** | Already uses libclang for header parsing, independent of Clang compiler. |
|
---

|c2d|---
---

c68|*End of LLVM_REFACTOR.md. The real work is stripping dead weight and adding ORC JIT output. No new IR layer needed — the existing codegen (21K lines of dep-free Rust, aka the real KNIR) already preserves Kain semantics through TypedProgram → LLVM IR text.*
