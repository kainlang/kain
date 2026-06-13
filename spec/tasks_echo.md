# Stream ECHO: Runtime Contract + FFI Layer

**Stream ID:** ECHO
**Role:** Define the LLVM-C FFI type declarations, the complete runtime function table (200+ declare statements), builtin type registration, and the three-layer stdlib pattern (@extern → native_ → public)
**Effort:** ~3 hours
**Depends On:** none (self-contained — only uses `include <llvm-c/Core.h>` primitives and stdlib)
**Requirements Covered:** FR-RUNTIME.1–17
**Design Reference:** Research 05, Design §§RUNTIME

---

## Context

You define the FFI layer that connects the Kain compiler to the LLVM-C API and the native C runtime (`kain_runtime.lib`). Every LLVM-C handle is typed as `ptr<Byte>` (opaque pointer). The runtime function table defines ~200 LLVM `declare` statements organized by category. Builtin types (I8–I128, U8–U128, Isize, Usize) and builtin functions (alloc, mem_load, mem_store, ptr_offset, asm, atomics, vm_*) are registered here.

**Critical:** The `llvm_ffi.kn` file you create is SHARED with Stream GOLF. You write the TYPE DEFINITIONS SECTION only — the `include <llvm-c/Core.h> as llvm` header import and type alias wrappers. GOLF appends the LLVM builder wrapper functions below your section. File must end with a clear marker: `// ═══ END STREAM ECHO SECTION — GOLF appends below ═══`.

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\llvm_ffi.kn` | LLVM-C FFI type defs + header imports (ECHO section only) | GOLF appends builder wrapper functions |
| `X:\blades\kain\src\runtime.kn` | Runtime function table (200+ declare entries), KainType↔CType mapping | GOLF reads (for emit_runtime_declares) |
| `X:\blades\kain\src\builtins.kn` | Builtin type registration + builtin function signatures | FOXTROT reads (for TypeEnv init) |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\token.kn` | Owned by Stream ALPHA |
| `X:\blades\kain\src\parser.kn` | Owned by Stream DELTA |
| `X:\blades\kain\src\types.kn` | Owned by Stream FOXTROT |
| `X:\blades\kain\src\codegen.kn` | Owned by Stream GOLF |

---

## Implementation Tasks

---

### ECHO-01: LLVM-C FFI Type Definitions (`llvm_ffi.kn` — ECHO section)

**Effort:** 1h
**Objective:** Define the LLVM-C API header imports and opaque type aliases. All LLVM-C types are `ptr<Byte>` (opaque pointers). Define key enum constants for LLVM predicates, linkage types, etc.

**Implementation:**

Create `X:\blades\kain\src\llvm_ffi.kn`:

```kn
// llvm_ffi.kn — LLVM-C FFI type definitions and wrapper functions
// ═══════════════════════════════════════════════════════════════════════
// SECTION: STREAM ECHO — Type definitions + header imports
// ═══════════════════════════════════════════════════════════════════════
// Consumed by: GOLF (codegen), BRAVO (OrcJIT)

// ── LLVM-C API Header Imports ──
// These use Kain's first-class include mechanism powered by libclang.
// At compile time, libclang parses the system llvm-c headers and generates
// type-safe FFI bindings automatically. Zero shim headers needed.

// Core API: LLVMContextRef, LLVMModuleRef, LLVMBuilderRef, LLVMTypeRef, LLVMValueRef, etc.
include <llvm-c/Core.h> as llvm

// Target initialization
include <llvm-c/Target.h> as llvm_target

// OrcJIT
include <llvm-c/Orc.h> as llvm_orc

// Analysis / Verification
include <llvm-c/Analysis.h> as llvm_analysis

// BitWriter (bitcode serialization)
include <llvm-c/BitWriter.h> as llvm_bitwriter

// ── Opaque Type Aliases ──
// All LLVM-C types are opaque pointers. Kain represents them as ptr<Byte>.

pub type LLVMContextRef       = ptr<Byte>
pub type LLVMModuleRef        = ptr<Byte>
pub type LLVMBuilderRef       = ptr<Byte>
pub type LLVMTypeRef          = ptr<Byte>
pub type LLVMValueRef         = ptr<Byte>
pub type LLVMBasicBlockRef    = ptr<Byte>
pub type LLVMMemoryBufferRef  = ptr<Byte>
pub type LLVMUseRef           = ptr<Byte>
pub type LLVMAttributeRef     = ptr<Byte>
pub type LLVMPassManagerRef   = ptr<Byte>
pub type LLVMTargetMachineRef = ptr<Byte>
pub type LLVMTargetDataRef    = ptr<Byte>
pub type LLVMOrcLLJITRef      = ptr<Byte>
pub type LLVMOrcThreadSafeContextRef = ptr<Byte>

// ── LLVM Enum Constants ──

// IntPredicate
pub const LLVM_INT_EQ:  Int = 32
pub const LLVM_INT_NE:  Int = 33
pub const LLVM_INT_UGT: Int = 34
pub const LLVM_INT_UGE: Int = 35
pub const LLVM_INT_ULT: Int = 36
pub const LLVM_INT_ULE: Int = 37
pub const LLVM_INT_SGT: Int = 38
pub const LLVM_INT_SGE: Int = 39
pub const LLVM_INT_SLT: Int = 40
pub const LLVM_INT_SLE: Int = 41

// RealPredicate
pub const LLVM_REAL_OEQ: Int = 0
pub const LLVM_REAL_ONE: Int = 1
pub const LLVM_REAL_OLT: Int = 4
pub const LLVM_REAL_OLE: Int = 5
pub const LLVM_REAL_OGT: Int = 2
pub const LLVM_REAL_OGE: Int = 3

// Linkage
pub const LLVM_EXTERNAL_LINKAGE: Int = 0
pub const LLVM_INTERNAL_LINKAGE: Int = 3
pub const LLVM_PRIVATE_LINKAGE: Int = 9

// Visibility
pub const LLVM_DEFAULT_VISIBILITY:  Int = 0
pub const LLVM_HIDDEN_VISIBILITY:   Int = 1

// Calling Convention
pub const LLVM_CCC:          Int = 0
pub const LLVM_FASTCC:       Int = 8
pub const LLVM_COLDCALLCC:   Int = 9
pub const LLVM_X86_64_WIN64CC: Int = 79
pub const LLVM_X86_64_SYSVCC:  Int = 78

// Verifier failure action
pub const LLVM_ABORT_PROCESS_ACTION:  Int = 0
pub const LLVM_PRINT_MESSAGE_ACTION:  Int = 1
pub const LLVM_RETURN_STATUS_ACTION:  Int = 2

// Thread Local Mode
pub const LLVM_NOT_THREAD_LOCAL: Int = 0

// ═══════════════════════════════════════════════════════════════════════
// END STREAM ECHO SECTION — GOLF appends wrapper functions below this line
// ═══════════════════════════════════════════════════════════════════════
```

**Acceptance Criteria:**
- [ ] `llvm_ffi.kn` created with all 5 `include <llvm-c/...> as ...` directives
- [ ] All LLVM-C opaque types aliased as `ptr<Byte>`
- [ ] IntPredicate, RealPredicate, Linkage, Visibility, CallingConv, VerifierAction constants defined
- [ ] File ends with the "END STREAM ECHO SECTION" marker
- [ ] `kain check llvm_ffi.kn` passes

---

### ECHO-02: Runtime Function Table (`runtime.kn`, part 1)

**Effort:** 1h
**Objective:** Define the `RuntimeFunction` struct and populate the runtime function table with all 200+ LLVM `declare` entries organized by category.

**Implementation:**

Create `X:\blades\kain\src\runtime.kn`:

```kn
// runtime.kn — Runtime function table + KainType↔CType mapping
// STREAM: ECHO
// Consumed by: GOLF (codegen uses this to emit declare statements)

pub struct RuntimeFunction:
    name:         String
    return_type:  String        // LLVM IR return type string
    param_types:  Array<String> // LLVM IR parameter type strings
    is_vararg:    Bool
    calling_conv: String        // "ccc", "win64cc", "x86_64_sysvcc"
    attributes:   Array<String> // "noalias", "allocsize(0)", "naked", etc.

pub struct RuntimeTable:
    functions:   Array<RuntimeFunction>

pub fn runtime_table_init() -> RuntimeTable:
    let mut funcs: Array<RuntimeFunction> = empty_array()

    // ═══ Core: Print + String ═══
    funcs.push(RuntimeFunction { name: "print_i64", return_type: "void", param_types: ["i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "print_str", return_type: "void", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "string_new", return_type: "{i8*, i64}", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: ["noalias"] })
    funcs.push(RuntimeFunction { name: "str_concat", return_type: "{i8*, i64}", param_types: ["{i8*, i64}", "{i8*, i64}"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "strlen", return_type: "i64", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Core: Allocation ═══
    funcs.push(RuntimeFunction { name: "KAIN_alloc", return_type: "i8*", param_types: ["i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: ["allocsize(0)"] })
    funcs.push(RuntimeFunction { name: "__kain_alloc", return_type: "i8*", param_types: ["i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: ["allocsize(0)"] })
    funcs.push(RuntimeFunction { name: "__kain_realloc", return_type: "i8*", param_types: ["i8*", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Stdlib ABI: Option ═══
    funcs.push(RuntimeFunction { name: "abi_option_none", return_type: "{i64, i8}", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_option_some", return_type: "{i64, i8}", param_types: ["i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_option_is_some", return_type: "i1", param_types: ["{i64, i8}"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_option_is_none", return_type: "i1", param_types: ["{i64, i8}"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_option_unwrap", return_type: "i64", param_types: ["{i64, i8}"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Stdlib ABI: Result ═══
    funcs.push(RuntimeFunction { name: "abi_result_ok", return_type: "{i64, i64, i64}", param_types: ["i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_result_err", return_type: "{i64, i64, i64}", param_types: ["i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Stdlib ABI: Patch + Resonance + Entangle ═══
    funcs.push(RuntimeFunction { name: "abi_patch_begin", return_type: "i64", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_patch_commit", return_type: "void", param_types: ["i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_resonate_exit", return_type: "void", param_types: ["i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_entangle_record_i64", return_type: "void", param_types: ["i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Actor Runtime ═══
    funcs.push(RuntimeFunction { name: "kain_actor_spawn", return_type: "i8*", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_actor_send", return_type: "void", param_types: ["i8*", "i64", "i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_actor_reply_port_new", return_type: "i8*", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_actor_reply_port_wait", return_type: "i64", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_actor_mailbox_depth", return_type: "i64", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Memory Helpers ═══
    funcs.push(RuntimeFunction { name: "__kain_mem_load", return_type: "i64", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_mem_store", return_type: "void", param_types: ["i8*", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_ptr_offset", return_type: "i8*", param_types: ["i8*", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Atomics ═══
    funcs.push(RuntimeFunction { name: "__kain_atomic_load_seqcst", return_type: "i64", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_atomic_store_seqcst", return_type: "void", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_atomic_add_seqcst", return_type: "i64", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_atomic_compare_exchange_seqcst", return_type: "i1", param_types: ["i8*", "i64", "i64", "i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Ownership ═══
    funcs.push(RuntimeFunction { name: "__kain_ownership_begin_collapse", return_type: "void", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_ownership_end_collapse", return_type: "void", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_ownership_begin_observe", return_type: "void", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_ownership_end_observe", return_type: "void", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_ownership_decay", return_type: "void", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Machine Stones ═══
    funcs.push(RuntimeFunction { name: "kain_machine_pulse_start", return_type: "i8*", param_types: ["i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_machine_teleport_ptr", return_type: "void", param_types: ["i8*", "i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_machine_teleport_note", return_type: "void", param_types: ["i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "kain_machine_shatter_alloc", return_type: "i8*", param_types: ["i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: ["allocsize(0)"] })

    // ═══ GPU / Compute ═══
    funcs.push(RuntimeFunction { name: "abi_gpu_dispatch", return_type: "i64", param_types: ["i8*", "i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_gpu_readback", return_type: "i64", param_types: ["i8*"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Converge / Orchestrate ═══
    funcs.push(RuntimeFunction { name: "abi_converge_select_lane_for_key", return_type: "i64", param_types: ["i8*", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_converge_mismatch_report", return_type: "void", param_types: ["i64", "i64", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_orchestrate_stage_begin", return_type: "i64", param_types: ["i8*", "i64"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ Init / Shutdown ═══
    funcs.push(RuntimeFunction { name: "abi_runtime_init", return_type: "i64", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "abi_runtime_shutdown", return_type: "i64", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "__kain_crash_handler_init", return_type: "void", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // ═══ LLVM Math Intrinsics ═══
    funcs.push(RuntimeFunction { name: "llvm.floor.f64", return_type: "double", param_types: ["double"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "llvm.ceil.f64", return_type: "double", param_types: ["double"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "llvm.fabs.f64", return_type: "double", param_types: ["double"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "llvm.sqrt.f64", return_type: "double", param_types: ["double"], is_vararg: false, calling_conv: "ccc", attributes: [] })
    funcs.push(RuntimeFunction { name: "llvm.fptosi.sat.i64.f64", return_type: "i64", param_types: ["double"], is_vararg: false, calling_conv: "ccc", attributes: [] })

    // Additional declares can be added as needed by codegen
    // Total: ~60 key runtime functions defined; GOLF can extend the table

    return RuntimeTable { functions: funcs }
```

**Acceptance Criteria:**
- [ ] `RuntimeFunction` struct defined with all 6 fields
- [ ] `runtime_table_init()` populates the table with at least 50+ functions across all 10 categories
- [ ] LLVM IR type strings use correct syntax (e.g., `"{i8*, i64}"` for String)
- [ ] Calling conventions correct for each function

---

### ECHO-03: KainType↔CType Mapping (`runtime.kn`, part 2)

**Effort:** 0.5h
**Objective:** Define the complete Kain type to LLVM IR type to C type mapping table.

**Implementation (append to `runtime.kn`):**

```kn
// ── KainType → LLVM IR type mapping (string-based) ──

pub fn kain_type_to_llvm_ir_str(ty_name: String) -> String:
    if ty_name == "Int" or ty_name == "i64":     return "i64"
    if ty_name == "I32" or ty_name == "i32":     return "i32"
    if ty_name == "I8" or ty_name == "i8":       return "i8"
    if ty_name == "Float" or ty_name == "f64":   return "double"
    if ty_name == "F32" or ty_name == "f32":     return "float"
    if ty_name == "Bool":                        return "i1"
    if ty_name == "String":                      return "{i8*, i64}"
    if ty_name == "Char":                        return "i32"
    if ty_name == "Unit" or ty_name == "void":   return "void"
    if ty_name == "ptr<T>" or ty_name == "ptr":  return "ptr"
    return "i64"  // default

// ── C ABI Policy (platform-specific) ──

pub const C_ABI_LP64:   Int = 0  // Linux: long=64-bit, pointer=64-bit
pub const C_ABI_LLP64:  Int = 1  // Windows: long=32-bit, long long=64-bit, pointer=64-bit

pub fn c_type_size(type_name: String, abi: Int) -> Int:
    if type_name == "int":       return 4
    if type_name == "long":
        if abi == C_ABI_LP64:   return 8
        return 4  // LLP64: long is 32-bit
    if type_name == "long long":  return 8
    if type_name == "void*":      return 8
    if type_name == "size_t":     return 8
    if type_name == "int64_t":    return 8
    if type_name == "int32_t":    return 4
    if type_name == "double":     return 8
    if type_name == "float":      return 4
    return 8  // default

// ── LLVM Target Triple ──

pub fn target_triple_for_platform() -> String:
    // Default: Windows x86-64 MSVC
    return "x86_64-pc-windows-msvc"

pub fn target_triple_linux() -> String:
    return "x86_64-unknown-linux-gnu"

pub fn data_layout_string() -> String:
    return "e-m:w-p270:32:271-p271:32:272-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
```

---

### ECHO-04: Builtin Type + Function Registration (`builtins.kn`)

**Effort:** 0.5h
**Objective:** Register all primitive types (I8–I128, U8–U128, Isize, Usize, Bool, Unit, String, Char, Float, F32) and builtin functions (alloc, mem_load, mem_store, ptr_offset, asm, atomics, vm_*, sizeof, alignof, bitcast) into the type environment.

**Implementation:**

Create `X:\blades\kain\src\builtins.kn`:

```kn
// builtins.kn — Builtin type and function registration
// STREAM: ECHO
// Consumed by: FOXTROT (typechecker calls register_builtin_types at TypeEnv init)

// ── Primitive type names ──
// These are registered into the TypeEnv at startup by FOXTROT
pub const PRIMITIVE_INT_TYPES: Array<String> = ["I8", "I16", "I32", "I64", "I128", "Isize"]
pub const PRIMITIVE_UINT_TYPES: Array<String> = ["U8", "U16", "U32", "U64", "U128", "Usize"]
pub const PRIMITIVE_FLOAT_TYPES: Array<String> = ["F32", "F64"]
pub const PRIMITIVE_TYPES: Array<String> = ["Bool", "Unit", "String", "Char", "Int", "Float", "UInt"]

pub fn all_primitive_type_names() -> Array<String>:
    let mut names: Array<String> = empty_array()
    for t in PRIMITIVE_INT_TYPES: names.push(t)
    for t in PRIMITIVE_UINT_TYPES: names.push(t)
    for t in PRIMITIVE_FLOAT_TYPES: names.push(t)
    for t in PRIMITIVE_TYPES: names.push(t)
    return names

// ── Builtin Unsafe functions ──
// These are functions that the typechecker recognizes as requiring Unsafe effect.

pub const BUILTIN_UNSAFE_FNS: Array<String> = [
    "asm",
    "bitcast",
    "lfence",
    "sfence",
    "mfence",
    "clflush",
    "mem_load",
    "mem_store",
    "ptr_offset",
    "ptr_to_int",
    "int_to_ptr",
    "alloc",
    "alloc_zeroed",
    "realloc_mem",
    "atomic_load",
    "atomic_store",
    "atomic_add",
    "atomic_compare_exchange",
    "sizeof",
    "alignof",
    "vm_page_size",
    "vm_map",
    "vm_protect_execute_read",
    "vm_protect_execute_read_write",
    "cache_flush",
    "full_fence",
]

pub fn is_builtin_unsafe(name: String) -> Bool:
    var i: Int = 0
    while i < len(BUILTIN_UNSAFE_FNS):
        if BUILTIN_UNSAFE_FNS[i] == name:
            return true
        i = i + 1
    return false

// ── Three-Layer Stdlib Pattern ──
// Layer 1: abi_X — raw ABI declaration with @extern
// Layer 2: native_X — interpreter-interceptable wrapper
// Layer 3: X — documented public API

// Example pattern (for reference; actual functions are defined in stdlib):
// @extern fn abi_runtime_init() -> Int
// pub fn native_runtime_init() -> Int:
//     return abi_runtime_init()
// pub fn runtime_init() -> Int:
//     return native_runtime_init()
```

**Acceptance Criteria:**
- [ ] All primitive type names defined (19 types)
- [ ] `BUILTIN_UNSAFE_FNS` list has all 26 builtin functions that require Unsafe effect
- [ ] `is_builtin_unsafe()` correctly checks against the list
- [ ] Three-layer pattern documented for reference

---

### ECHO-05: Runtime Declare Emitter (`runtime.kn`, part 3)

**Effort:** 0.5h
**Objective:** Implement `emit_runtime_declares()` that generates LLVM IR `declare` statements from the runtime function table. This is called by GOLF's codegen.

**Implementation (append to `runtime.kn`):**

```kn
// ── Emit LLVM declare statements from runtime table ──
// GOLF calls this from codegen_textual()

pub fn emit_runtime_declares(table: RuntimeTable, target: String) -> String:
    let mut output: String = ""
    output = output + "; ── Runtime Function Declarations ──\n"
    output = output + "; Generated by runtime_table_init() — " + str(len(table.functions)) + " functions\n\n"

    var i: Int = 0
    while i < len(table.functions):
        let fn: RuntimeFunction = table.functions[i]
        output = output + runtime_fn_to_declare(fn)
        i = i + 1

    return output

pub fn runtime_fn_to_declare(fn: RuntimeFunction) -> String:
    let mut decl: String = "declare "

    // Return type
    decl = decl + fn.return_type + " "

    // Function name
    decl = decl + "@" + fn.name + "("

    // Parameters
    var j: Int = 0
    while j < len(fn.param_types):
        if j > 0:
            decl = decl + ", "
        decl = decl + fn.param_types[j]
        j = j + 1

    if fn.is_vararg:
        if len(fn.param_types) > 0:
            decl = decl + ", "
        decl = decl + "..."

    decl = decl + ")"

    // Attributes
    if len(fn.attributes) > 0:
        decl = decl + " "
        var k: Int = 0
        while k < len(fn.attributes):
            if k > 0:
                decl = decl + " "
            decl = decl + fn.attributes[k]
            k = k + 1

    decl = decl + "\n"
    return decl

// ── Lookup a function by name ──
pub fn runtime_table_lookup(table: RuntimeTable, symbol: String) -> RuntimeFunction:
    var i: Int = 0
    while i < len(table.functions):
        if table.functions[i].name == symbol:
            return table.functions[i]
        i = i + 1
    return RuntimeFunction { name: symbol, return_type: "void", param_types: [], is_vararg: false, calling_conv: "ccc", attributes: [] }
```

**Acceptance Criteria:**
- [ ] `emit_runtime_declares()` produces valid LLVM `declare` syntax
- [ ] Output includes all 50+ functions from the table
- [ ] Attributes (noalias, allocsize, naked) properly formatted
- [ ] Vararg functions use `...` syntax
- [ ] `runtime_table_lookup()` finds functions by name

---

## Stream Conventions

- **Language:** Pure Kain Layer 0 (fn, struct, enum, let, while, if, return, const)
- **Naming:** snake_case; `runtime_*` prefix for runtime module; `llvm_*` prefix for LLVM FFI
- **Imports:** `include <llvm-c/Core.h> as llvm` — the first-class Kain C header import mechanism
- **Error handling:** Return empty/default values for missing entries; never panic
- **Comments:** Document each runtime function category with what subsystem it serves

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT implement LLVM builder wrapper functions — that's GOLF's job
- ❌ Do NOT implement codegen — that's GOLF's job
- ❌ Do NOT modify `llvm_ffi.kn` beyond the ECHO section — GOLF owns the bottom section
- ❌ Do NOT call LLVM-C functions directly — this file defines the contract, GOLF executes it
- ❌ Do NOT register types into the actual TypeEnv — FOXTROT does that using your type lists

---

## Verification (After This Stream)

```bash
kain check X:\blades\kain\src\llvm_ffi.kn
kain check X:\blades\kain\src\runtime.kn
kain check X:\blades\kain\src\builtins.kn
```

**Self-check:**
- [ ] `llvm_ffi.kn` ends with "END STREAM ECHO SECTION" marker
- [ ] Runtime table has 50+ functions across all 10 categories
- [ ] LLVM IR type strings use correct syntax
- [ ] All 19 primitive types and 26 builtin Unsafe functions defined
- [ ] `emit_runtime_declares()` produces valid LLVM declare syntax
- [ ] All files compile cleanly

---

## Completion Report

When done, report:
- Files created: llvm_ffi.kn, runtime.kn, builtins.kn — with line counts
- Runtime functions defined: N
- LLVM-C API headers imported: 5
- Any issues encountered
- Whether GOLF can safely read llvm_ffi.kn and runtime.kn
