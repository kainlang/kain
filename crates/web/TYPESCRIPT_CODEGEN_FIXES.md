# TypeScript Codegen Fixes and Enhancements

## Summary

Fixed 6 critical bugs and added comprehensive enhancements to the TypeScript codegen in `Kain/crates/web/src/codegen_ts.rs`.

## Critical Bugs Fixed

### 1. String Escaping (Line ~465)
**Problem:** Used Rust's `escape_default()` which escapes for Rust, not JavaScript, causing double-escaping issues.

**Fix:** Implemented proper JavaScript string escaping:
```rust
Expr::String(s, _) => {
    let escaped = s
        .replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('\n', "\\n")
        .replace('\r', "\\r")
        .replace('\t', "\\t");
    self.write(&format!("\"{}\"", escaped))
}
```

### 2. JSX Text Escaping (Line ~698)
**Problem:** Single quotes + `escape_default()` caused syntax errors with apostrophes in text nodes.

**Fix:** Proper single-quote escaping for JavaScript:
```rust
JSXNode::Text(text, _) => {
    let escaped = text
        .replace('\\', "\\\\")
        .replace('\'', "\\'");
    self.write(&format!("document.createTextNode('{}')", escaped))
}
```

## Enhancements Added

### 3. Array Method Translation
Added automatic translation of KAIN array methods to JavaScript equivalents:

| KAIN Method | JavaScript Output |
|-------------|-------------------|
| `.len()` | `.length` |
| `.is_empty()` | `.length === 0` |
| `.first()` | `[0]` |
| `.last()` | `[arr.length - 1]` (with IIFE) |
| `.contains(x)` | `.includes(x)` |
| `.push(x)` | `.push(x)` |
| `.pop()` | `.pop()` |
| `.clear()` | `.length = 0` |

### 4. Numeric Type Helpers
Added helper functions at the top of generated output for type coercion:

```typescript
function u8(n: number): number { return n & 0xFF; }
function u16(n: number): number { return n & 0xFFFF; }
function u32(n: number): number { return n >>> 0; }
function i8(n: number): number { return (n << 24) >> 24; }
function i16(n: number): number { return (n << 16) >> 16; }
function i32(n: number): number { return n | 0; }
function f32(n: number): number { return Math.fround(n); }
```

### 5. Component Children Handling
Fixed component prop destructuring to always include `children`:

**Before:**
```typescript
const { title } = props;  // children not available
```

**After:**
```typescript
const { title, children = [] } = props;  // children always available
```

### 6. Comprehensive Test Suite
Added 17 comprehensive tests covering:
- String escaping (newlines, quotes, backslashes, tabs)
- JSX text escaping (apostrophes, backslashes)
- Array method translation (all 8 methods)
- Numeric helper generation
- Component children destructuring (with and without props)

## Test Results

All 17 tests pass:
```
test result: ok. 17 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

## Files Modified

- `Kain/crates/web/src/codegen_ts.rs` - Main implementation file
  - Fixed string escaping (line ~465)
  - Fixed JSX text escaping (line ~698)
  - Added array method translation (line ~520)
  - Added numeric type helpers (line ~90)
  - Fixed component children handling (line ~267)
  - Added 17 comprehensive tests (line ~1100)

## Impact

These fixes resolve critical correctness issues that would have caused:
- Syntax errors in generated TypeScript (string escaping)
- Runtime errors with apostrophes in JSX text
- Missing functionality for array operations
- Inability to use component children
- Lack of numeric type coercion helpers

The enhancements significantly improve the usability and correctness of the TypeScript codegen target.
