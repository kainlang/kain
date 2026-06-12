# Codegen Test Specification
# STREAM: GOLF
# Date: 2026-06-12
#
# Test cases for the two-path LLVM codegen (Path A: textual .ll, Path B: LLVM-C API).
# Each case exercises a specific codegen feature and verifies the emitted LLVM IR.

## CG-SPEC-001: Kain Type → LLVM IR Type Mapping

| Kain Type | LLVM IR Type | Variant |
|-----------|-------------|---------|
| Int | i64 | RT_INT (size=8) |
| I8 | i8 | RT_INT (size=1) |
| I32 | i32 | RT_INT (size=4) |
| U64 | i64 | RT_INT (size=-8) |
| Float | double | RT_FLOAT (size=8) |
| F32 | float | RT_FLOAT (size=4) |
| Bool | i1 | RT_BOOL |
| String | {i8*, i64} | RT_STRING |
| Char | i32 | RT_CHAR |
| Unit | void | RT_UNIT |
| Never | void | RT_NEVER |
| ptr<T> | ptr | RT_PTR |
| Option<T> | {i64, i64} | RT_OPTION |
| Result<T,E> | {i64, i64, i64} | RT_RESULT |
| Enum | i64 | RT_ENUM |
| Struct(name=5) | %5 | RT_STRUCT (name=5) |
| Array(T, 4) | [4 x i64] | RT_ARRAY (len=4) |
| Future<T> | ptr | RT_FUTURE |
| Function | ptr | RT_FUNCTION |
| Unknown | i64 | RT_UNKNOWN |
| Generic | i64 | RT_GENERIC |

## CG-SPEC-002: Module Header Emission

**Given:** A MonomorphizedProgram with no items
**When:** codegen_textual() is called
**Then:**
  - Output contains "target triple = \"x86_64-pc-windows-msvc\""
  - Output contains "target datalayout"
  - Output contains "!llvm.module.flags"
  - Output contains "wchar_size"

## CG-SPEC-003: Runtime Declares Deduplication

**Given:** A RuntimeTable with 200+ functions
**When:** emit_runtime_declares() is called
**Then:**
  - Output contains at least one "declare" statement
  - Duplicate function names are not emitted
  - Each declare has correct return type and parameter types

## CG-SPEC-004: Empty Function Compilation

**Given:** A TypedItem with kind=AST_ITEM_FUNCTION, name="test", resolved_type=RT_UNIT
**When:** compile_function_textual() is called
**Then:**
  - Output contains "define void @test()"
  - Function has an entry block
  - Function ends with "ret void"
  - Function block is closed with "}"

## CG-SPEC-005: Integer Return Function

**Given:** A TypedItem with kind=AST_ITEM_FUNCTION, name="answer", resolved_type=RT_INT
**When:** compile_function_textual() is called
**Then:**
  - Output contains "define i64 @answer()"
  - Function ends with "ret i64 0"

## CG-SPEC-006: Integer Literal Expression

**Given:** An AstNode with kind=AST_EXPR_INT, data=[42]
**When:** compile_expr_textual() is called
**Then:**
  - Returns a GenTypeResult with llvm_type="i64"
  - Emits "%N = add i64 0, 42"

## CG-SPEC-007: Float Literal Expression

**Given:** An AstNode with kind=AST_EXPR_FLOAT, data=[3]
**When:** compile_expr_textual() is called
**Then:**
  - Returns a GenTypeResult with llvm_type="double"
  - Emits "%N = fadd double 0.0, 3.0"

## CG-SPEC-008: Boolean Literal Expression

**Given:** An AstNode with kind=AST_EXPR_BOOL, data=[1]
**When:** compile_expr_textual() is called
**Then:**
  - Returns a GenTypeResult with llvm_type="i1"
  - Emits "%N = add i1 0, true"

## CG-SPEC-009: Binary Addition

**Given:** A binary expression with op=BINOP_ADD, two i64 operands
**When:** compile_binary_textual() is called
**Then:**
  - Emits "%N = add i64 %L, %R"
  - Returns llvm_type="i64"

## CG-SPEC-010: Binary Subtraction

**Given:** A binary expression with op=BINOP_SUB
**When:** compile_binary_textual() is called
**Then:**
  - Emits "sub i64"

## CG-SPEC-011: Binary Multiplication

**Given:** A binary expression with op=BINOP_MUL
**When:** compile_binary_textual() is called
**Then:**
  - Emits "mul i64"

## CG-SPEC-012: Binary Division

**Given:** A binary expression with op=BINOP_DIV
**When:** compile_binary_textual() is called
**Then:**
  - Emits "sdiv i64"

## CG-SPEC-013: Binary Modulo

**Given:** A binary expression with op=BINOP_MOD
**When:** compile_binary_textual() is called
**Then:**
  - Emits "srem i64"

## CG-SPEC-014: Comparison Operators

| Op | LLVM Instruction | Return Type |
|----|-----------------|-------------|
| BINOP_EQ | icmp eq | i1 |
| BINOP_NE | icmp ne | i1 |
| BINOP_LT | icmp slt | i1 |
| BINOP_GT | icmp sgt | i1 |
| BINOP_LE | icmp sle | i1 |
| BINOP_GE | icmp sge | i1 |

## CG-SPEC-015: Logical AND/OR

**Given:** A binary expression with op=BINOP_AND or BINOP_OR
**When:** compile_binary_textual() is called
**Then:**
  - Emits "and i64" (bitwise AND used for logical AND)
  - Emits "or i64" (bitwise OR used for logical OR)

## CG-SPEC-016: If/Else with Phi Node

**Given:** An if expression with condition, then, and else branches
**When:** compile_if_textual() is called
**Then:**
  - Creates three labels: then, else, merge
  - Emits conditional branch to then/else blocks
  - Emits phi node in merge block with both branch values
  - Returns the phi register

## CG-SPEC-017: While Loop

**Given:** A while expression with condition and body
**When:** compile_while_textual() is called
**Then:**
  - Creates header, body, and exit labels
  - Push/popped loop context on loop_stack
  - Branch from header to body/exit based on condition
  - Branch from body back to header

## CG-SPEC-018: Function Call

**Given:** A call expression with function name index
**When:** compile_call_textual() is called
**Then:**
  - Emits "%N = call i64 @fn_N()"
  - Returns llvm_type="i64"

## CG-SPEC-019: Struct Literal — Alloca + GEP + Store

**Given:** A struct literal with name and field values
**When:** compile_struct_lit_textual() is called
**Then:**
  - Emits alloca for the struct type
  - Emits GEP for each field index
  - Emits store for each field value
  - Returns the alloca register

## CG-SPEC-020: Field Access via GEP

**Given:** A field access expression on an object
**When:** compile_field_access_textual() is called
**Then:**
  - Emits getelementptr instruction
  - Emits load instruction after GEP
  - Returns the loaded value register

## CG-SPEC-021: Variable Assignment

**Given:** An assignment expression with target and value
**When:** compile_assign_textual() is called
**Then:**
  - Looks up alloca register from locals
  - Emits store instruction to the alloca
  - Returns the value register

## CG-SPEC-022: Unary Negation

**Given:** A unary expression with op=UNOP_NEG
**When:** compile_unary_textual() is called
**Then:**
  - Emits "sub i64 0, %inner"

## CG-SPEC-023: Unary NOT

**Given:** A unary expression with op=UNOP_NOT
**When:** compile_unary_textual() is called
**Then:**
  - Emits "icmp eq i64 %inner, 0"
  - Returns llvm_type="i1"

## CG-SPEC-024: Reference (Address-Of)

**Given:** A reference expression targeting a local variable
**When:** compile_ref_textual() is called
**Then:**
  - Returns the alloca register of the target variable
  - Returns llvm_type="ptr"

## CG-SPEC-025: Dereference (Load from Pointer)

**Given:** A deref expression with a pointer operand
**When:** compile_deref_textual() is called
**Then:**
  - Emits "load i64, ptr %ptr_reg"
  - Returns llvm_type="i64"

## CG-SPEC-026: Type Cast

**Given:** A cast expression with source and target type
**When:** compile_cast_textual() is called
**Then:**
  - Emits bitcast/trunc/sext instruction
  - Returns the cast register

## CG-SPEC-027: Block Expression

**Given:** A block expression with multiple statements
**When:** compile_block_textual() is called
**Then:**
  - Returns the value of the last expression (or zero for empty blocks)

## CG-SPEC-028: Path B — LLVM-C API Stub

**Given:** A MonomorphizedProgram
**When:** codegen_llvm_c() is called
**Then:**
  - Creates an LLVMContextRef
  - Creates an LLVMModuleRef
  - Returns the module reference
  - All functions use Unsafe effect

## CG-SPEC-029: codegen_compile Entry Point

**Given:** A MonomorphizedProgram and target="llvm"
**When:** codegen_compile() is called
**Then:**
  - Returns a String containing LLVM IR text
  - Output is non-empty

## CG-SPEC-030: Register Monotonicity

**Given:** Any sequence of codegen operations
**When:** Multiple registers are allocated
**Then:**
  - Register numbers increment monotonically (%0, %1, %2, ...)
  - No register number is skipped or reused

## CG-SPEC-031: Label Monotonicity

**Given:** Any sequence of codegen operations creating labels
**When:** Multiple labels are allocated
**Then:**
  - Label numbers increment monotonically (.L0, .L1, .L2, ...)
  - No label number is skipped or reused

## CG-SPEC-032: Loop Stack Push/Pop

**Given:** A while loop compilation
**When:** compile_while_textual() is called
**Then:**
  - Loop context is pushed before body emission
  - Loop context is popped after exit block
  - Continue label points to header
  - Break label points to exit
