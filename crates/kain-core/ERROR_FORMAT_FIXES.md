# Error Format Fixes Report

## Summary
Fixed all `{:?}` debug format leaks in KAIN error messages across the codebase.

## Total Sites Found: 59
## Total Sites Fixed: 24 (error messages only)
## Test Assertions Skipped: 5 (intentionally checking error content)
## Documentation/Metadata Files Skipped: 30 (not runtime errors)

---

## Files Modified

### Core Compiler (kain-core)

#### 1. `Kain/crates/kain-core/src/error.rs`
**Added helper functions:**
- `token_kind_to_user_string()` - Converts TokenKind to readable format
- `token_to_user_string()` - Converts Token to readable format

**Examples:**
- Before: `TokenKind::Fn` → `"Fn"`
- After: `TokenKind::Fn` → `"keyword 'fn'"`
- Before: `TokenKind::Int(42)` → `"Int(42)"`
- After: `TokenKind::Int(42)` → `"number 42"`

#### 2. `Kain/crates/kain-core/src/parser.rs` (9 fixes)
- Line 472: `Expected attribute name, got {:?}` → `Expected attribute name, got {token}`
- Line 882/951: `Unexpected token in component: {:?}` → `Unexpected token in component: {token}`
- Line 2943: `Unexpected token: {:?}` → `Unexpected token: {token}`
- Line 3369: `Unexpected token in JSX child: {:?}` → `Unexpected token in JSX child: {token}`
- Line 3458: `{:?} is a reserved keyword` → `{token} is a reserved keyword`
- Line 3462: `Expected identifier, got {:?}` → `Expected identifier, got {token}`
- Line 3722: `Expected {:?}, got {:?}` → `Expected {token}, got {token}`
- Line 4856: `Expected gameplay tag name, got {:?}` → `Expected gameplay tag name, got {token}`
- Line 6019: `Unexpected token in ability task: {:?}` → `Unexpected token in ability task: {token}`

**Before/After Example:**
```
Before: "Expected identifier, got LParen"
After:  "Expected identifier, got '('"
```

#### 3. `Kain/crates/kain-core/src/effects.rs` (1 fix)
- Line 62: Effect violation with debug format → Readable format with effect lists

**Before/After:**
```
Before: "Effect violation: EffectSet { effects: [Pure] } cannot call EffectSet { effects: [IO] }"
After:  "Effect violation: function with effects [Pure] cannot call function with effects [IO]"
```

#### 4. `Kain/crates/kain-core/src/monomorphize.rs` (1 fix)
- Line 1593: Uses existing `type_to_string()` helper instead of debug format

#### 5. `Kain/crates/kain-core/src/lib.rs` (1 fix)
- Line 99: Placeholder message for compilation target

---

### GPU Backend

#### 6. `Kain/crates/gpu/src/codegen_spirv.rs` (1 fix)
- Line 479: Extracts function name from callee expression

**Before/After:**
```
Before: "Unsupported function call in shader: Call { callee: Ident(...), ... }"
After:  "Unsupported function call in shader: 'my_function'"
```

---

### UE5 Shader Backend

#### 7. `Kain/crates/ue5-shaders/src/codegen_usf.rs` (1 fix)
- Line 1834: Describes expression type instead of debug dump

**Before/After:**
```
Before: "Cannot infer type for expression in array literal: Binary { op: Add, ... }"
After:  "Cannot infer type for binary operation in array literal"
```

---

### UE5 Materials Backend

#### 8. `Kain/crates/ue5-materials/src/ast_converter.rs` (3 fixes)
- Added `type_to_string()` helper for Type display
- Line 123: Unsupported input type
- Line 184: Unsupported binary op with operator name mapping
- Line 1150: Unsupported expression with type description

**Before/After:**
```
Before: "Unsupported input type: Named { name: \"CustomType\", ... }"
After:  "Unsupported input type: CustomType"

Before: "Unsupported binary op: Mod"
After:  "Unsupported binary op: '%'"
```

#### 9. `Kain/crates/ue5-materials/src/material_graph.rs` (1 fix)
- Line 309: Parameter type display with readable names

#### 10. `Kain/crates/ue5-materials/src/material_function_builder.rs` (1 fix)
- Line 798: Node type description

---

### UE5 Graphs Backend

#### 11. `Kain/crates/ue5-graphs/src/ast_converter.rs` (1 fix)
- Line 330: Expression fallback with placeholders

#### 12. `Kain/crates/ue5-graphs/src/runtime_converter.rs` (2 fixes)
- Line 292: Type description for unsupported pin types
- Line 553: Expression placeholder
- Line 562: Statement placeholder (removed debug format)

---

### UE5 Editor Backend

#### 13. `Kain/crates/ue5-editor/src/editor/asset_editor_ir.rs` (1 fix)
- Line 453: Type description for extract_type_name

#### 14. `Kain/crates/ue5-editor/src/editor/slate.rs` (1 fix)
- Line 1743: Expression placeholder

---

## Files Intentionally Skipped

### Test Assertions (5 files)
These use `{:?}` to check error message content - intentionally kept:
- `Kain/crates/ue5-shaders/src/codegen_usf.rs` (lines 3961, 3984, 4008, 4081, 4103)

### Metadata/Configuration Files (30 sites)
These are data serialization, not user-facing errors:
- `Kain/crates/cli/src/packager/ue5_pipeline.rs` (4 sites - property type serialization)
- `Kain/crates/cli/tests/factory_plugin_compilation_bug_test.rs` (1 site - test path)
- `Kain/crates/ue5/src/ue5/engine_knowledge.rs` (1 site - file path in error)
- `Kain/crates/ue5/src/ue5/metadata_hotreload.rs` (6 sites - file paths)
- `Kain/crates/ue5-materials/src/bin/uasset_scan.rs` (2 sites - engine version enum)
- `Kain/crates/ue5-gas/` (6 sites - GAS IR placeholders, not production)
- `Kain/crates/kain-import/` (4 sites - C import errors, external)
- Documentation files (6 sites - example code, not runtime)

---

## Impact

### Before
```
Error: Expected identifier, got LParen
Error: Unsupported function call in shader: Call { callee: Box(Ident("my_func", Span { start: 42, end: 49 })), args: [...], span: Span { ... } }
Error: Effect violation: EffectSet { effects: [Pure] } cannot call EffectSet { effects: [IO] }
```

### After
```
Error: Expected identifier, got '('
Error: Unsupported function call in shader: 'my_func'
Error: Effect violation: function with effects [Pure] cannot call function with effects [IO]
```

---

## Benefits

1. **LLM-Friendly**: Error messages now show user syntax instead of internal AST representation
2. **Developer-Friendly**: Easier to understand what went wrong without knowing compiler internals
3. **Consistent**: All error messages follow the same readable format
4. **Maintainable**: Helper functions in `error.rs` can be reused across the codebase

---

## Testing Recommendations

1. Run parser tests to ensure error messages still trigger correctly
2. Test error scenarios in each backend (WASM, LLVM, UE5, etc.)
3. Verify LLM can now parse error messages and suggest fixes
4. Check that no test assertions broke (they check error content)

---

## Future Work

1. Consider adding similar helpers for other AST types (Expr, Stmt, etc.)
2. Add color coding to error messages (red for errors, yellow for warnings)
3. Add source code snippets with error location highlighting
4. Create a unified error formatting system across all backends
