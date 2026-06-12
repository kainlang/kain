# Task: context.kn — LLVM Context Management

## Agent: kain-writer
## Wave: 1
## File to write: X:/blades/kain/src/context.kn
## Target lines: ~100
## Dependencies: None
## Parallel: Yes

---

## What to Build

Lifecycle manager for LLVM context, module, and builder objects. These are opaque `ptr<Byte>` handles that flow through the codegen pipeline. This file ensures they're created and destroyed correctly — no leaks, no use-after-free.

All LLVM-C types become `ptr<Byte>` in Kain.

## Public API Contract

```kain
/// Create a new LLVM context (the top-level compilation unit)
pub fn create_context() -> ptr<Byte> with Unsafe

/// Create a new LLVM module inside a context
pub fn create_module(name: String, ctx: ptr<Byte>) -> ptr<Byte> with Unsafe

/// Create an LLVM IR builder for a context
pub fn create_builder(ctx: ptr<Byte>) -> ptr<Byte> with Unsafe

/// Destroy the builder (call when codegen is done)
pub fn dispose_builder(builder: ptr<Byte>) with Unsafe

/// Destroy the module (call after emitting)
pub fn dispose_module(mod: ptr<Byte>) with Unsafe

/// Destroy the context (call at compiler shutdown)
pub fn dispose_context(ctx: ptr<Byte>) with Unsafe

/// Verify a module — returns empty string if valid, error message if invalid
pub fn verify_module(mod: ptr<Byte>) -> String with Unsafe

/// Write a module to a .ll file on disk
pub fn write_module_to_file(mod: ptr<Byte>, path: String) -> Int with Unsafe, IO

/// Write a module to a .bc (bitcode) file on disk
pub fn write_bitcode_to_file(mod: ptr<Byte>, path: String) -> Int with Unsafe, IO
```

## LLVM-C FFI

```kain
include <llvm-c/Core.h> as llvm
include <llvm-c/BitWriter.h> as llvm_bw
```

Key FFI calls used internally:

| Kain call | LLVM-C function |
|-----------|----------------|
| create_context | LLVMContextCreate() |
| create_module | LLVMModuleCreateWithNameInContext(name, ctx) |
| create_builder | LLVMCreateBuilderInContext(ctx) |
| dispose_context | LLVMContextDispose(ctx) |
| dispose_module | LLVMModuleDispose(mod) |
| dispose_builder | LLVMDisposeBuilder(builder) |
| verify_module | LLVMVerifyModule(mod, action, error_msg) |
| write_module_to_file | LLVMPrintModuleToFile(mod, path, error) |
| write_bitcode_to_file | LLVMWriteBitcodeToFile(mod, path) |

## Research to Read

- X:/blades/kain/research/03-llvm-codegen-jit.md — Section on LLVM-C API surface
- X:/blades/kain/research/SELFHOST-KN.MD — Section 9 (LLVM-C FFI Contract)

## Neighboring Files

| File | What it needs from context.kn |
|------|------------------------------|
| codegen.kn | `create_module()`, `create_builder()`, `verify_module()`, `dispose_*()` |
| jit.kn | `create_module()` for OrcJIT |
| driver.kn | `dispose_context()` at shutdown |

## Test Expectations

- `kain check src/context.kn` passes
- LLVM-C header must be findable by libclang on the build machine
- `create_context()` returns non-null pointer
- `dispose_context(ctx)` is safe to call
