# Parser Fixes Summary

**Date:** 2026-02-19  
**Status:** ✅ Implemented and Tested

---

## Quick Reference

### Issue 1: Nested Generic Types (`>>` Parsing)

**Problem:** `Box<Box<Int>>` failed to parse because `>>` was lexed as a single right-shift token.

**Solution:** Modified `parse_type()` to explicitly check for `Shr` token and treat it as two `>` tokens.

**Files Changed:**
- `crates/kain-core/src/parser.rs` (2 locations: generic args + impl trait args)

**Test:** `test_nested_generic_types()` in `crates/kain-core/tests/monomorphize_test.rs`

---

### Issue 2: Negative Literal Type Inference

**Problem:** `abs(-42)` inferred as `abs_Any` instead of `abs_Int`.

**Solution:** Added `Expr::Unary` handling in `scan_expr()` to preserve operand types for unary minus.

**Files Changed:**
- `crates/kain-core/src/monomorphize.rs` (added Unary case in scan_expr)

**Test:** `test_negative_literal_inference()` in `crates/kain-core/tests/monomorphize_test.rs`

---

## Before vs After

### Nested Generics

```kain
// BEFORE: ❌ Parse error
type Nested = Box<Box<Int>>

// AFTER: ✅ Works
type Nested = Box<Box<Int>>
```

### Literal Inference

```kain
// BEFORE: ❌ Generates abs_Any
let x = abs(-42)

// AFTER: ✅ Generates abs_Int
let x = abs(-42)
```

---

## Verification

```bash
# Check diagnostics (no errors)
cargo check --package kain-core

# Run tests
cargo test --package kain-core test_nested_generic_types
cargo test --package kain-core test_negative_literal_inference
```

---

## Documentation

Full details in `docs/recent/PARSER_FIXES.md`
