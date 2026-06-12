# Task: target.kn — LLVM Target Machine Initialization

## Agent: kain-writer
## Wave: 1
## File to write: X:/blades/kain/src/target.kn
## Target lines: ~100
## Dependencies: None
## Parallel: Yes

---

## What to Build

Initializes LLVM's native target support — the target machine that the codegen backend uses to emit correct machine code for the current CPU. Must be called once at compiler startup before any codegen runs.

## Public API Contract

```kain
include <llvm-c/Target.h> as llvm_target
include <llvm-c/TargetMachine.h> as llvm_tm

/// Initialize all native targets (call once at startup)
pub fn init_native_target() -> Int with Unsafe

/// Initialize native assembly printer (needed for .ll output)
pub fn init_native_asm_printer() -> Int with Unsafe

/// Initialize native assembly parser (needed for .s input)
pub fn init_native_asm_parser() -> Int with Unsafe

/// Get the host CPU name string (e.g., "skylake", "znver3")
pub fn get_host_cpu_name() -> String with Unsafe

/// Get the host CPU features string (e.g., "+avx2,+sse4.2")
pub fn get_host_cpu_features() -> String with Unsafe

/// Create a target machine for the given triple, CPU, and features.
/// Returns ptr<Byte> handle to the target machine.
pub fn create_target_machine(triple: String, cpu: String, features: String) -> ptr<Byte> with Unsafe

/// Set the target machine on a module (for codegen)
pub fn set_module_target(mod: ptr<Byte>, triple: String) with Unsafe

/// Dispose a target machine
pub fn dispose_target_machine(tm: ptr<Byte>) with Unsafe
```

## LLVM-C FFI

| Kain call | LLVM-C function |
|-----------|----------------|
| init_native_target | LLVMInitializeNativeTarget() |
| init_native_asm_printer | LLVMInitializeNativeAsmPrinter() |
| init_native_asm_parser | LLVMInitializeNativeAsmParser() |
| get_host_cpu_name | LLVMGetHostCPUName() |
| get_host_cpu_features | LLVMGetHostCPUFeatures() (returns a map) |
| create_target_machine | LLVMCreateTargetMachine() |
| set_module_target | LLVMSetTarget(mod, triple) / LLVMSetModuleDataLayout |
| dispose_target_machine | LLVMDisposeTargetMachine(tm) |

## Research to Read

- X:/blades/kain/research/03-llvm-codegen-jit.md — Section on target initialization
- X:/blades/kain/research/SELFHOST-KN.MD — Section 9 (LLVM-C FFI Contract)

## Neighboring Files

| File | What it needs from target.kn |
|------|------------------------------|
| driver.kn | `init_native_target()` called at startup |
| codegen.kn | `create_target_machine()`, `set_module_target()` |
| platform.kn | `get_target_triple()` used to pass triple to `create_target_machine()` |

## Test Expectations

- `kain check src/target.kn` passes
- LLVM-C Target headers findable on build machine
- `init_native_target()` returns 0 on success
- `get_host_cpu_name()` returns non-empty string
