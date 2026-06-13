# Stream GOLF: LLVM Codegen + CLI Driver

**Stream ID:** GOLF
**Role:** Implement the two-path LLVM codegen (textual .ll emission + LLVM-C API), LLVM builder wrapper functions in llvm_ffi.kn, the CLI driver (DriverSession pipeline, workspace discovery, diagnostics, subcommand tree), and the entry point main.kn
**Effort:** ~14 hours
**Depends On:** Stream FOXTROT (types.kn TypedProgram, ResolvedType), Stream ECHO (llvm_ffi.kn type defs, runtime.kn function table), Stream ALPHA (foundation types), Stream DELTA (AstNode), Stream BRAVO (JIT trampoline), Stream CHARLIE (orchestrator entry points)
**Requirements Covered:** FR-CODEGEN.1–46, FR-CLI.1–23, NFR-O1–O2
**Design Reference:** Research 03, Research 04, Design §§CODEGEN, §§CLI

---

## Context

This is the final integration stream. You implement both LLVM codegen paths (Path A: textual .ll emission via string formatting; Path B: LLVM-C API in-memory IR construction), the LLVM builder wrapper functions that ECHO declared, the DriverSession pipeline that coordinates Resolve→Lex→Parse→Typecheck→Monomorphize→Codegen, the CLI subcommand tree (check, build, run, test, selfhost, fmt, amalgamate, doctor, config, clean), workspace discovery, and the main entry point.

**Critical:** The `llvm_ffi.kn` file already exists from ECHO with the type definitions section. You APPEND your LLVM builder wrapper functions BELOW the `// ═══ END STREAM ECHO SECTION ═══` marker. Do NOT modify anything above this marker.

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\codegen.kn` | Two-path LLVM codegen: textual .ll emitter + LLVM-C API path (~2000 lines) | Integration |
| `X:\blades\kain\src\compiler.kn` | DriverSession pipeline: Resolve→Lex→Parse→Typecheck→Mono→Codegen (~200 lines) | Integration |
| `X:\blades\kain\src\cli.kn` | CLI argument parsing + 10 subcommand handlers (~300 lines) | Integration |
| `X:\blades\kain\src\main.kn` | Entry point: calls CLI dispatcher (~100 lines) | Final binary |
| `X:\blades\kain\src\KAIN.toml` | Workspace config: project metadata, source roots, deps (~30 lines) | Build system |
| `X:\blades\kain\spec\codegen_spec.md` | Codegen test specification (~200 lines) | Integration tests |

### Files to Modify

| File | Region/Function | Change Description | After This Stream |
|------|-----------------|--------------------|--------------------|
| `X:\blades\kain\src\llvm_ffi.kn` | Append AFTER "END STREAM ECHO SECTION" marker | Add LLVM builder wrapper functions (~800 lines): `llvm_context_create()`, `llvm_module_create()`, `llvm_builder_create()`, `llvm_build_add()`, `llvm_build_sub()`, `llvm_build_mul()`, `llvm_build_call()`, `llvm_build_ret()`, `llvm_build_br()`, `llvm_build_cond_br()`, `llvm_build_alloca()`, `llvm_build_store()`, `llvm_build_load()`, `llvm_build_gep()`, `llvm_build_icmp()`, `llvm_build_phi()`, `llvm_verify_module()`, etc. | BRAVO consumes for OrcJIT |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\token.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\lexer.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\parser.kn` | Owned by DELTA (read-only) |
| `X:\blades\kain\src\ast.kn` | Owned by ALPHA+DELTA (read-only) |
| `X:\blades\kain\src\types.kn` | Owned by FOXTROT (read-only) |
| `X:\blades\kain\src\effects.kn` | Owned by FOXTROT (read-only) |
| `X:\blades\kain\src\monomorphize.kn` | Owned by FOXTROT (read-only) |
| `X:\blades\kain\src\jit_metal.kn` | Owned by BRAVO (read-only; use trampoline via import) |
| `X:\blades\kain\src\llvm_ffi.kn` ABOVE the END ECHO marker | Owned by ECHO — DO NOT MODIFY |
| `X:\blades\kain\src\orchestrator.kn` | Owned by CHARLIE (read-only; call orch_* entry points) |

---

## Implementation Tasks

---

### GOLF-01: LLVM Builder Wrapper Functions (append to `llvm_ffi.kn`)

**Effort:** 2h
**Objective:** Append LLVM-C API wrapper functions below ECHO's section in `llvm_ffi.kn`. These are Unsafe Kain functions that wrap LLVM-C calls for Path B codegen.

**Implementation:**

Open `X:\blades\kain\src\llvm_ffi.kn` and append AFTER:
```
// ═══════════════════════════════════════════════════════════════════════
// END STREAM ECHO SECTION — GOLF appends wrapper functions below this line
// ═══════════════════════════════════════════════════════════════════════
```

Add:

```kn
// ═══════════════════════════════════════════════════════════════════════
// SECTION: STREAM GOLF — LLVM builder wrapper functions
// ═══════════════════════════════════════════════════════════════════════
// All functions annotated with Unsafe effect.
// Opaque LLVM types as ptr<Byte> (see ECHO section above for type aliases).

// ── Context Management ──
pub fn llvm_context_create() -> LLVMContextRef with Unsafe:
    return llvm.LLVMContextCreate()

pub fn llvm_context_dispose(ctx: LLVMContextRef) with Unsafe:
    llvm.LLVMContextDispose(ctx)

// ── Module Management ──
pub fn llvm_module_create(name: String, ctx: LLVMContextRef) -> LLVMModuleRef with Unsafe:
    return llvm.LLVMModuleCreateWithNameInContext(name, ctx)

pub fn llvm_module_dispose(mod: LLVMModuleRef) with Unsafe:
    llvm.LLVMDisposeModule(mod)

// ── Builder ──
pub fn llvm_builder_create(ctx: LLVMContextRef) -> LLVMBuilderRef with Unsafe:
    return llvm.LLVMCreateBuilderInContext(ctx)

pub fn llvm_builder_dispose(builder: LLVMBuilderRef) with Unsafe:
    llvm.LLVMDisposeBuilder(builder)

pub fn llvm_position_at_end(builder: LLVMBuilderRef, bb: LLVMBasicBlockRef) with Unsafe:
    llvm.LLVMPositionBuilderAtEnd(builder, bb)

// ── Types ──
pub fn llvm_int1_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMInt1TypeInContext(ctx)

pub fn llvm_int8_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMInt8TypeInContext(ctx)

pub fn llvm_int32_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMInt32TypeInContext(ctx)

pub fn llvm_int64_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMInt64TypeInContext(ctx)

pub fn llvm_float_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMFloatTypeInContext(ctx)

pub fn llvm_double_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMDoubleTypeInContext(ctx)

pub fn llvm_void_type(ctx: LLVMContextRef) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMVoidTypeInContext(ctx)

pub fn llvm_pointer_type(ctx: LLVMContextRef, address_space: Int) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMPointerTypeInContext(ctx, address_space)

pub fn llvm_struct_type(ctx: LLVMContextRef, element_types: Array<LLVMTypeRef>,
                          packed: Bool) -> LLVMTypeRef with Unsafe:
    let count: Int = len(element_types)
    // ... allocate C array of LLVMTypeRef and call LLVMStructTypeInContext

pub fn llvm_array_type(element_type: LLVMTypeRef, count: Int) -> LLVMTypeRef with Unsafe:
    return llvm.LLVMArrayType2(element_type, count)

pub fn llvm_function_type(ret_type: LLVMTypeRef, param_types: Array<LLVMTypeRef>,
                            is_vararg: Bool) -> LLVMTypeRef with Unsafe:
    let count: Int = len(param_types)
    return llvm.LLVMFunctionType(ret_type, /* C array */, count, if is_vararg: 1 else: 0)

// ── Constants ──
pub fn llvm_const_int(int_type: LLVMTypeRef, value: Int, sign_extend: Bool) -> LLVMValueRef with Unsafe:
    return llvm.LLVMConstInt(int_type, value, if sign_extend: 1 else: 0)

pub fn llvm_const_string(str: String, len: Int, null_terminate: Bool) -> LLVMValueRef with Unsafe:
    return llvm.LLVMConstStringInContext(/* ctx */, str, len, if null_terminate: 1 else: 0)

// ── Functions ──
pub fn llvm_add_function(mod: LLVMModuleRef, name: String,
                           fn_type: LLVMTypeRef) -> LLVMValueRef with Unsafe:
    return llvm.LLVMAddFunction(mod, name, fn_type)

pub fn llvm_append_basic_block(fn: LLVMValueRef, name: String) -> LLVMBasicBlockRef with Unsafe:
    return llvm.LLVMAppendBasicBlockInContext(/* ctx */, fn, name)

// ── Builder Instructions ──
pub fn llvm_build_add(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                        name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildAdd(builder, lhs, rhs, name)

pub fn llvm_build_sub(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                        name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildSub(builder, lhs, rhs, name)

pub fn llvm_build_mul(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                        name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildMul(builder, lhs, rhs, name)

pub fn llvm_build_sdiv(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildSDiv(builder, lhs, rhs, name)

pub fn llvm_build_srem(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildSRem(builder, lhs, rhs, name)

pub fn llvm_build_fadd(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildFAdd(builder, lhs, rhs, name)

pub fn llvm_build_fsub(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildFSub(builder, lhs, rhs, name)

pub fn llvm_build_fmul(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildFMul(builder, lhs, rhs, name)

pub fn llvm_build_fdiv(builder: LLVMBuilderRef, lhs: LLVMValueRef, rhs: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildFDiv(builder, lhs, rhs, name)

pub fn llvm_build_call(builder: LLVMBuilderRef, fn: LLVMValueRef,
                         args: Array<LLVMValueRef>, name: String) -> LLVMValueRef with Unsafe:
    let count: Int = len(args)
    return llvm.LLVMBuildCall2(builder, /* fn_type */, fn, /* C array of args */, count, name)

pub fn llvm_build_ret(builder: LLVMBuilderRef, val: LLVMValueRef) with Unsafe:
    llvm.LLVMBuildRet(builder, val)

pub fn llvm_build_ret_void(builder: LLVMBuilderRef) with Unsafe:
    llvm.LLVMBuildRetVoid(builder)

pub fn llvm_build_br(builder: LLVMBuilderRef, dest: LLVMBasicBlockRef) with Unsafe:
    llvm.LLVMBuildBr(builder, dest)

pub fn llvm_build_cond_br(builder: LLVMBuilderRef, cond: LLVMValueRef,
                            then_bb: LLVMBasicBlockRef, else_bb: LLVMBasicBlockRef) with Unsafe:
    llvm.LLVMBuildCondBr(builder, cond, then_bb, else_bb)

pub fn llvm_build_alloca(builder: LLVMBuilderRef, ty: LLVMTypeRef,
                           name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildAlloca(builder, ty, name)

pub fn llvm_build_store(builder: LLVMBuilderRef, val: LLVMValueRef,
                          ptr: LLVMValueRef) with Unsafe:
    llvm.LLVMBuildStore(builder, val, ptr)

pub fn llvm_build_load(builder: LLVMBuilderRef, ty: LLVMTypeRef, ptr: LLVMValueRef,
                         name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildLoad2(builder, ty, ptr, name)

pub fn llvm_build_gep(builder: LLVMBuilderRef, ptr: LLVMValueRef,
                        indices: Array<LLVMValueRef>, name: String) -> LLVMValueRef with Unsafe:
    let count: Int = len(indices)
    return llvm.LLVMBuildGEP2(builder, /* base_type */, ptr, /* C array */, count, name)

pub fn llvm_build_icmp(builder: LLVMBuilderRef, pred: Int, lhs: LLVMValueRef,
                         rhs: LLVMValueRef, name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildICmp(builder, pred, lhs, rhs, name)

pub fn llvm_build_fcmp(builder: LLVMBuilderRef, pred: Int, lhs: LLVMValueRef,
                         rhs: LLVMValueRef, name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildFCmp(builder, pred, lhs, rhs, name)

pub fn llvm_build_phi(builder: LLVMBuilderRef, ty: LLVMTypeRef,
                        name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildPhi(builder, ty, name)

pub fn llvm_add_incoming(phi: LLVMValueRef, values: Array<LLVMValueRef>,
                           blocks: Array<LLVMBasicBlockRef>, count: Int) with Unsafe:
    llvm.LLVMAddIncoming(phi, /* C arrays */, /* C arrays */, count)

pub fn llvm_build_select(builder: LLVMBuilderRef, cond: LLVMValueRef,
                           then_val: LLVMValueRef, else_val: LLVMValueRef,
                           name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildSelect(builder, cond, then_val, else_val, name)

pub fn llvm_build_bitcast(builder: LLVMBuilderRef, val: LLVMValueRef,
                            dest_type: LLVMTypeRef, name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildBitCast(builder, val, dest_type, name)

pub fn llvm_build_int_to_ptr(builder: LLVMBuilderRef, val: LLVMValueRef,
                               dest_type: LLVMTypeRef, name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildIntToPtr(builder, val, dest_type, name)

pub fn llvm_build_ptr_to_int(builder: LLVMBuilderRef, val: LLVMValueRef,
                               dest_type: LLVMTypeRef, name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildPtrToInt(builder, val, dest_type, name)

pub fn llvm_build_extract_value(builder: LLVMBuilderRef, agg: LLVMValueRef,
                                  index: Int, name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildExtractValue(builder, agg, index, name)

pub fn llvm_build_insert_value(builder: LLVMBuilderRef, agg: LLVMValueRef,
                                 val: LLVMValueRef, index: Int,
                                 name: String) -> LLVMValueRef with Unsafe:
    return llvm.LLVMBuildInsertValue(builder, agg, val, index, name)

// ── Verification ──
pub fn llvm_verify_module(mod: LLVMModuleRef, action: Int) -> Bool with Unsafe:
    let mut err_msg: ptr<Byte> = null_ptr()
    let status: Int = llvm.LLVMVerifyModule(mod, action, /* &err_msg */)
    if status != 0:
        // ... report error message
        return false
    return true

// ── BitWriter ──
pub fn llvm_write_bitcode(mod: LLVMModuleRef, path: String) with Unsafe:
    llvm_bitwriter.LLVMWriteBitcodeToFile(mod, path)

// ── Target ──
pub fn llvm_initialize_native_target() with Unsafe:
    llvm_target.LLVMInitializeNativeTarget()

pub fn llvm_initialize_native_asm_printer() with Unsafe:
    llvm_target.LLVMInitializeNativeAsmPrinter()

// ═══════════════════════════════════════════════════════════════════════
// END STREAM GOLF SECTION — LLVM wrapper functions complete
// ═══════════════════════════════════════════════════════════════════════
```

**Acceptance Criteria:**
- [ ] 40+ LLVM wrapper functions appended below ECHO's section
- [ ] All wrappers annotated with `Unsafe` effect
- [ ] All type parameters use `ptr<Byte>` for LLVM opaque types
- [ ] Context, Module, Builder lifecycle functions complete
- [ ] All arithmetic, comparison, control flow, memory, and conversion instructions wrapped
- [ ] Verification and bitcode serialization wrapped
- [ ] Target initialization functions wrapped
- [ ] `kain check llvm_ffi.kn` passes with ECHO + GOLF sections combined

---

### GOLF-02: Path A — Textual LLVM IR Codegen (`codegen.kn`, part 1)

**Effort:** 3h
**Objective:** Implement the textual `.ll` emitter (Path A). This is the primary path — always works, zero LLVM DLL dependency. Format: string building, following the proven pattern from the Rust bootstrap's 21,289-line `codegen_llvm/mod.rs`.

**Implementation:**

Create `X:\blades\kain\src\codegen.kn`:

```kn
// codegen.kn — Two-path LLVM codegen
// STREAM: GOLF

use src::types::{ResolvedType, TypedProgram, TypedItem, MonomorphizedProgram,
                  RT_UNIT, RT_BOOL, RT_INT, RT_FLOAT, RT_STRING, RT_CHAR,
                  RT_ARRAY, RT_SLICE, RT_TUPLE, RT_REF, RT_PTR, RT_OPTION,
                  RT_RESULT, RT_FUTURE, RT_STRUCT, RT_ENUM, RT_FUNCTION,
                  RT_GENERIC, RT_NEVER, RT_UNKNOWN, type_env_get}
use src::runtime::{RuntimeTable, runtime_table_init, emit_runtime_declares,
                    kain_type_to_llvm_ir_str, target_triple_for_platform,
                    data_layout_string}
use src::ast::AstNode

// ── LlvmGenerator state ──
pub struct LlvmGenerator:
    output:          String
    reg_count:       Int
    label_count:     Int
    global_const_count: Int
    locals:          HashMap<String, String>     // var_name → %reg_name
    struct_defs:     HashMap<String, Array<(String, String)>>  // struct fields
    loop_stack:      Array<LoopLabel>
    indent_level:    Int

pub struct LoopLabel:
    continue_label: String
    break_label:    String

// ── Path A: Textual .ll emission ──
pub fn codegen_textual(program: MonomorphizedProgram, target_triple: String,
                        debug: Bool) -> String with IO:
    let mut gen: LlvmGenerator = llvm_gen_new()

    // 1. Module header
    gen.output = gen.output + "; ── Module Header ──\n"
    gen.output = gen.output + "target triple = \"" + target_triple + "\"\n"
    gen.output = gen.output + "target datalayout = \"" + data_layout_string() + "\"\n\n"

    // 2. Runtime function declarations (200+ declare statements)
    let runtime: RuntimeTable = runtime_table_init()
    gen.output = gen.output + emit_runtime_declares(runtime, target_triple)

    // 3. Global string constants
    emit_global_constants(gen, program)

    // 4. Struct type definitions
    for item in program.items:
        if item.kind == /* struct */:
            emit_struct_type_definition(gen, item)

    // 5. Enum type definitions
    for item in program.items:
        if item.kind == /* enum */:
            emit_enum_type_definition(gen, item)

    // 6. Function definitions
    for item in program.items:
        if item.kind == /* function */:
            compile_function_textual(gen, item)

    return gen.output
```

Key sub-functions to implement:
- `compile_function_textual(gen, item)` — emit function signature + body
- `compile_expr_textual(gen, expr) -> String` — recursive expression compilation
- `emit_entry_block(gen, fn_name, params)` — alloca locals, store params
- `emit_struct_type_definition(gen, item)` — `%Name = type { T1, T2, ... }`
- `emit_global_constants(gen, program)` — `@.str.0 = private constant ...`

**Kain Type → LLVM IR Type mapping (Path A string):**
```
Int(I64)    → "i64"
Float(F64)  → "double"
Bool        → "i1"
String      → "{i8*, i64}"
Unit        → "void"
ptr<T>      → "ptr"
Struct      → "%Name"
Array(T,N)  → "[N x T_llvm]"
```

**Acceptance Criteria:**
- [ ] `codegen_textual()` produces complete, valid LLVM IR text
- [ ] Module header with target triple and data layout
- [ ] Runtime declares emitted (deduplicated)
- [ ] Struct type definitions before function definitions
- [ ] Function prologue (entry block, alloca locals, store params)
- [ ] Integer literals → `add i64 0, <value>`
- [ ] Binary arithmetic → correct LLVM instruction
- [ ] If/else → cond br + phi node
- [ ] While/for → header/body/exit blocks with loop stack
- [ ] Function call → `call @fn(args)`
- [ ] Return → `ret <ty> <val>`
- [ ] Field access → `getelementptr` + `load`
- [ ] Generated LLVM IR passes `llc` verification

---

### GOLF-03: Path B — LLVM-C API Codegen (`codegen.kn`, part 2)

**Effort:** 2h
**Objective:** Implement Path B using the LLVM-C API wrappers from `llvm_ffi.kn`. This constructs LLVM IR in-memory for JIT compilation.

Key functions:
- `codegen_llvm_c(program, ctx, module) -> LLVMModuleRef`
- `compile_function_llvm_c(gen, item, ctx, module, builder) -> LLVMValueRef`
- `compile_expr_llvm_c(gen, expr, ctx, builder, locals) -> LLVMValueRef`

Path B uses the same compilation logic as Path A but calls LLVM-C API functions instead of formatting strings. The structure mirrors Path A for verifiability.

---

### GOLF-04: Untagging + @extern Call Marshaling (`codegen.kn`, part 3)

**Effort:** 1h
**Objective:** Implement tagged integer stripping for @extern calls and String↔C string marshaling.

Key patterns:
- `@extern` calls: strip tag `(val >> 3)` before C ABI call
- Untag return: `(val << 3) | 1` on return
- String→C: `extractvalue {i8*, i64} %str, 0` to get data pointer
- C→String: `string_new` + `strlen` to materialize owned String

---

### GOLF-05: DriverSession Pipeline (`compiler.kn`)

**Effort:** 1.5h
**Objective:** Implement the `DriverSession` that coordinates the full compile pipeline.

**Implementation:**

Create `X:\blades\kain\src\compiler.kn`:

```kn
// compiler.kn — DriverSession pipeline
// STREAM: GOLF

pub struct DriverSession:
    source:         String
    file_path:      String
    tokens:         Array<Token>
    ast:            Array<AstNode>
    typed:          TypedProgram
    mono:           MonomorphizedProgram
    diagnostics:    DiagnosticBag
    config:         BuildConfig
    progress_phase: Int

pub const PHASE_RESOLVE:   Int = 0
pub const PHASE_LEX:       Int = 1
pub const PHASE_PARSE:     Int = 2
pub const PHASE_COMPTIME:  Int = 3
pub const PHASE_TYPECHECK: Int = 4
pub const PHASE_MONO:      Int = 5
pub const PHASE_CODEGEN:   Int = 6

pub fn driver_session_new() -> DriverSession:
    return DriverSession {
        source: "",
        file_path: "",
        tokens: empty_array(),
        ast: empty_array(),
        typed: TypedProgram { items: empty_array(), env: type_env_new(), errors: diag_bag_new() },
        mono: MonomorphizedProgram { items: empty_array() },
        diagnostics: diag_bag_new(),
        config: build_config_default(),
        progress_phase: 0,
    }

pub fn driver_session_compile(session: *mut DriverSession, source: String,
                               source_path: String, target: String) -> CompileResult with IO:
    session.source = source
    session.file_path = source_path

    // Phase 1: Lex
    emit_progress("Lex")
    let raw_tokens: Array<Token> = lexer_tokenize_all(source, source_path)
    session.tokens = indent_process(raw_tokens)
    if session.diagnostics.has_errors():
        return compile_result_error(session.diagnostics)

    // Phase 2: Parse
    emit_progress("Parse")
    let parser: ParserState = parser_new(session.tokens, source_path)
    let program: AstProgram = parse(parser)
    session.ast = program.nodes
    if parser.errors.has_errors():
        session.diagnostics = parser.errors
        return compile_result_error(session.diagnostics)

    // Phase 3: Typecheck
    emit_progress("Typecheck")
    let mut env: TypeEnv = type_env_new()
    let typed: TypedProgram = typecheck(env, program)
    session.typed = typed
    if typed.errors.has_errors():
        session.diagnostics = typed.errors
        return compile_result_error(session.diagnostics)

    // Phase 4: Monomorphize
    emit_progress("Monomorphize")
    session.mono = monomorphize(env, typed)

    // Phase 5: Codegen
    emit_progress("Codegen")
    if target == "llvm":
        let llvm_text: String = codegen_textual(session.mono,
            target_triple_for_platform(), false)
        return CompileResult { success: true, output: llvm_text }
    elif target == "jit":
        // JIT execution path
        let result: Int = 0  // TODO: wire through BRAVO's jit.kn
        return CompileResult { success: true, exit_code: result }

    return CompileResult { success: false, output: "" }

pub fn emit_progress(phase_name: String) with IO:
    // NFR-O1: emit progress event
    println("[kainc] " + phase_name + "...")

pub fn driver_session_check(session: *mut DriverSession, source: String,
                              source_path: String) -> CheckResult with IO:
    // Simplified pipeline: lex → parse → typecheck only (no codegen)
    // Returns diagnostics without running monomorphization or codegen
    return CheckResult { diagnostics: session.diagnostics }
```

**Acceptance Criteria:**
- [ ] DriverSession executes full pipeline: Lex → Parse → Typecheck → Monomorphize → Codegen
- [ ] Progress events emitted at each phase (NFR-O1)
- [ ] Error bail-out at each phase (don't proceed if errors exist)
- [ ] `driver_session_check()` does lex→parse→typecheck only
- [ ] Caching stubs: frontend/checked cache for incremental compilation

---

### GOLF-06: CLI Argument Parsing + Subcommand Tree (`cli.kn`)

**Effort:** 1.5h
**Objective:** Implement CLI argument parsing and 10 subcommand handlers.

**Implementation:**

Create `X:\blades\kain\src\cli.kn`:

```kn
// cli.kn — CLI argument parsing + subcommand tree
// STREAM: GOLF

pub const SUBCMD_CHECK:    Int = 0
pub const SUBCMD_BUILD:    Int = 1
pub const SUBCMD_RUN:      Int = 2
pub const SUBCMD_TEST:     Int = 3
pub const SUBCMD_SELFHOST: Int = 4
pub const SUBCMD_FMT:      Int = 5
pub const SUBCMD_AMALGAMATE: Int = 6
pub const SUBCMD_DOCTOR:   Int = 7
pub const SUBCMD_CONFIG:   Int = 8
pub const SUBCMD_CLEAN:    Int = 9
pub const SUBCMD_HELP:     Int = 10

pub struct CliConfig:
    subcommand:      Int
    input_path:      String
    target:          String
    profile:         String
    json_output:     Bool
    json_out_path:   String
    verbose:         Bool
    debug_info:      Bool
    verify_ouroboros: Bool
    stage:           String

pub fn parse_args(args: Array<String>) -> CliConfig:
    let mut config: CliConfig = CliConfig {
        subcommand: SUBCMD_HELP,
        input_path: ".",
        target: "llvm",
        profile: "debug",
        json_output: false,
        json_out_path: "",
        verbose: false,
        debug_info: false,
        verify_ouroboros: false,
        stage: "",
    }

    if len(args) < 2:
        return config

    let subcmd: String = args[1]
    if subcmd == "check":
        config.subcommand = SUBCMD_CHECK
        if len(args) > 2:
            config.input_path = args[2]
    elif subcmd == "build":
        config.subcommand = SUBCMD_BUILD
        // ... parse build-specific flags
    elif subcmd == "run":
        config.subcommand = SUBCMD_RUN
    elif subcmd == "test":
        config.subcommand = SUBCMD_TEST
    elif subcmd == "selfhost":
        config.subcommand = SUBCMD_SELFHOST
    elif subcmd == "fmt":
        config.subcommand = SUBCMD_FMT
    elif subcmd == "amalgamate":
        config.subcommand = SUBCMD_AMALGAMATE
    elif subcmd == "doctor":
        config.subcommand = SUBCMD_DOCTOR
    elif subcmd == "config":
        config.subcommand = SUBCMD_CONFIG
    elif subcmd == "clean":
        config.subcommand = SUBCMD_CLEAN

    // Parse flags: --target, --json, --debug, --verify-ouroboros, etc.
    var i: Int = 2
    while i < len(args):
        let arg: String = args[i]
        if arg == "--target" and i + 1 < len(args):
            i = i + 1
            config.target = args[i]
        elif arg == "--json":
            config.json_output = true
        elif arg == "--debug" or arg == "-g":
            config.debug_info = true
        elif arg == "--verify-ouroboros":
            config.verify_ouroboros = true
        elif arg == "--stage" and i + 1 < len(args):
            i = i + 1
            config.stage = args[i]
        elif arg == "--profile" and i + 1 < len(args):
            i = i + 1
            config.profile = args[i]
        elif arg == "-v" or arg == "--verbose":
            config.verbose = true
        i = i + 1

    return config

// ── Subcommand dispatch ──
pub fn run_subcommand(config: CliConfig) -> Int with IO:
    if config.subcommand == SUBCMD_HELP:
        print_help()
        return 0
    elif config.subcommand == SUBCMD_CHECK:
        return orch_check_cli(config.input_path)  // delegates to CHARLIE's orchestrator
    elif config.subcommand == SUBCMD_BUILD:
        return orch_build_cli(config.input_path, config.target, config.profile, config.stage)
    elif config.subcommand == SUBCMD_RUN:
        return orch_run_cli(config.input_path)
    elif config.subcommand == SUBCMD_TEST:
        return orch_test_cli(config.input_path)
    elif config.subcommand == SUBCMD_SELFHOST:
        return orch_selfhost_cli(config.verify_ouroboros)
    elif config.subcommand == SUBCMD_FMT:
        println("fmt: not yet implemented")
        return 0
    elif config.subcommand == SUBCMD_DOCTOR:
        println("kainc " + VERSION)
        println("Target: " + target_triple_for_platform())
        return 0
    elif config.subcommand == SUBCMD_CLEAN:
        println("clean: not yet implemented")
        return 0

    return 0
```

**Acceptance Criteria:**
- [ ] All 10 subcommands dispatch correctly
- [ ] `--target`, `--json`, `--debug`, `--stage`, `--profile`, `-v`, `--verify-ouroboros` flags parsed
- [ ] `check` delegates to orchestrator
- [ ] `build` delegates to orchestrator with target/profile/stage
- [ ] Help text displayed for `--help` or no args

---

### GOLF-07: Workspace Discovery (`compiler.kn`, append)

**Effort:** 0.5h
**Objective:** Implement `discover_workspace()` that ascends directories looking for KAIN.toml/build.kn/.git.

```kn
pub fn discover_workspace(start_path: String) -> String:
    // Ascend directory tree looking for workspace anchors
    // Returns workspace root path or "" if not found
    var current: String = start_path
    loop:
        if fs_exists(current + "/KAIN.toml"): return current
        if fs_exists(current + "/kain.toml"):  return current
        if fs_exists(current + "/build.kn"):   return current
        if fs_exists(current + "/platform.kn"): return current
        if fs_exists(current + "/.git"):       return current
        let parent: String = fs_parent(current)
        if parent == current:  // reached filesystem root
            break
        current = parent
    return ""
```

---

### GOLF-08: Entry Point (`main.kn`)

**Effort:** 0.5h
**Objective:** Wire the `main()` entry point that calls CLI argument parsing and subcommand dispatch.

Create `X:\blades\kain\src\main.kn`:

```kn
// main.kn — Entry point for the self-host compiler
// STREAM: GOLF

pub fn main() -> Int with IO:
    let args: Array<String> = os_args()
    let config: CliConfig = parse_args(args)
    return run_subcommand(config)
```

---

### GOLF-09: KAIN.toml Workspace Config

**Effort:** 0.25h
**Objective:** Create the workspace configuration file.

Create `X:\blades\kain\src\KAIN.toml`:

```toml
[package]
name = "kainc"
version = "0.1.0"
description = "Kain Self-Host Compiler"
authors = ["Kain Compiler Team"]

[build]
entry = "src/main.kn"
source_root = "src/"
output = "kainc"
target = "llvm"
profile = "debug"

[dependencies]
stdlib = ["std::text", "std::machine", "std::markscript", "std::fs", "std::collections"]

[source_order]
# Determines concatenation order for combined source (ouroboros)
files = [
    "token.kn",
    "error.kn",
    "span.kn",
    "ast.kn",
    "lexer.kn",
    "builtins.kn",
    "runtime.kn",
    "llvm_ffi.kn",
    "jit_metal.kn",
    "jit_x86.kn",
    "jit_orc.kn",
    "jit_cache.kn",
    "jit.kn",
    "parser.kn",
    "types.kn",
    "effects.kn",
    "monomorphize.kn",
    "codegen.kn",
    "orchestrator.kn",
    "compiler.kn",
    "cli.kn",
    "main.kn",
]
```

---

### GOLF-10: Codegen Test Specification (`spec/codegen_spec.md`)

**Effort:** 0.5h
**Objective:** Write test cases for codegen.

Create `X:\blades\kain\spec\codegen_spec.md` with test cases for:
- Kain type → LLVM IR type mapping (all 20 variants)
- Function compilation (empty, int return, arithmetic)
- Control flow (if/else, while, for, match)
- Struct literal compilation
- Field access via GEP
- Function calls
- @extern call untagging
- Runtime declares deduplication

---

## Stream Conventions

- **Language:** Pure Kain Layer 0 with Unsafe effect for LLVM-C wrappers, IO effect for CLI
- **Naming:** snake_case; `codegen_*` for codegen; `driver_*` for DriverSession; `llvm_*` for FFI wrappers
- **Imports:** Import from all subsystems — LEX, PARSE, TYPE, RUNTIME, JIT, ORCH
- **Error handling:** DriverSession accumulates errors and bails at each phase. CLI formats and displays.
- **Comments:** Document LLVM IR patterns with the corresponding LLVM instruction
- **Testing:** Test-driven — write spec first for each codegen feature

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT modify ALPHA's files (token.kn, error.kn, span.kn, lexer.kn)
- ❌ Do NOT modify DELTA's parser.kn or the ALPHA section of ast.kn
- ❌ Do NOT modify FOXTROT's types.kn, effects.kn, monomorphize.kn
- ❌ Do NOT modify ECHO's section of llvm_ffi.kn (above the END marker)
- ❌ Do NOT modify CHARLIE's orchestrator.kn — call `orch_*_cli()` entry points
- ❌ Do NOT implement the Rust bootstrap bridge — this is pure Kain

---

## Verification (After This Stream)

```bash
# Check all files
kain check X:\blades\kain\src\llvm_ffi.kn
kain check X:\blades\kain\src\codegen.kn
kain check X:\blades\kain\src\compiler.kn
kain check X:\blades\kain\src\cli.kn
kain check X:\blades\kain\src\main.kn

# Full workspace check (all files)
kain check X:\blades\kain\src\

# Build the compiler
kain build X:\blades\kain\src\ --target llvm

# Run integration test
kain test X:\blades\kain\spec\codegen_spec.md
```

**Self-check:**
- [ ] All 6 files created/modified
- [ ] llvm_ffi.kn has 40+ builder wrapper functions below ECHO's section
- [ ] codegen.kn Path A produces valid LLVM IR for a simple function
- [ ] DriverSession pipeline executes all 7 phases
- [ ] CLI parses all 10 subcommands correctly
- [ ] main.kn wires everything together
- [ ] KAIN.toml defines workspace + source order for ouroboros
- [ ] Full `kain check src/` passes with zero errors

---

## Integration Gate: Ouroboros Verification

After all streams complete and integration passes, the final acceptance test is:

```bash
# Stage 1: Compile kainc with the Rust bootstrap compiler
kain build src/ --target llvm
clang out/*.o -lkain_runtime -o kainc.exe

# Stage 2: Use the stage-1 binary to compile itself
./kainc.exe build src/ --target llvm

# Stage 3: Compare output
diff out/stage1.ll out/stage2.ll

# Expected: "OUROBOROS VERIFIED" — byte-identical LLVM IR
```

---

## Completion Report

When done, report:
- Files created: codegen.kn, compiler.kn, cli.kn, main.kn, KAIN.toml, spec/codegen_spec.md — with line counts
- Files modified: llvm_ffi.kn (GOLF section appended) — line count of new section
- LLVM-C wrapper functions: N implemented
- Codegen Path A: all expression kinds covered
- CLI subcommands: 10/10 dispatched
- Integration status: can `kain check src/` pass?
- Any issues encountered
- Ouroboros verification status
