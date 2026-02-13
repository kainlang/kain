# UE5 Editor Crate - 37 Compilation Errors Fix Plan

**Status**: Ready to fix
**Total Errors**: 37 (5 unique error types)
**Crate**: `kain/crates/ue5-editor`

## Error Summary

### Error Type Breakdown:
1. **E0412** - Missing `Argument` type (5 errors)
2. **E0026** - Type::Array field mismatch (2 errors)  
3. **E0599** - Type::Map doesn't exist (1 error)
4. **E0408** - Pattern binding issue (1 error)
5. **E0502** - Borrow checker issues (28 errors)

---

## SPLIT WORK PLAN

### 🔧 YOUR HALF (Agent - 19 errors)

#### Task 1: Fix Missing `Argument` Type (5 errors)
**Files**: `kain/crates/ue5-editor/src/editor/slate.rs`

**Problem**: Code references `kain_core::ast::Argument` which doesn't exist in the new AST.

**Locations**:
- Line 217: `fn generate_slot(&mut self, args: &[kain_core::ast::Argument])`
- Line 241: `fn generate_slot_property(&mut self, method: &str, args: &[kain_core::ast::Argument])`
- Line 246: `fn generate_widget_property(&mut self, method: &str, args: &[kain_core::ast::Argument])`
- Line 278: `fn format_args(&self, args: &[kain_core::ast::Argument]) -> String`

**Solution**: 
1. Check what the actual argument type is in `kain_core::ast` (likely `CallArg` or similar)
2. Replace all `Argument` references with the correct type
3. Update any field access patterns if the struct changed

**Commands**:
```bash
# Find the correct type
rg "pub struct.*Arg" kain/crates/kain-core/src/ast.rs

# Check usage patterns
rg "Argument" kain/crates/ue5-editor/src/editor/slate.rs
```

---

#### Task 2: Fix Type::Array Field Name (2 errors)
**Files**: `kain/crates/ue5-editor/src/editor/slate.rs`

**Problem**: Code uses `Type::Array { element, .. }` but the field is named differently.

**Locations**:
- Line 360: `if let Type::Array { element, .. } = &field.ty {`
- Line 385: `Type::Array { element, .. } => {`

**Solution**:
1. Check the actual Type::Array definition in kain_core
2. Replace `element` with correct field name (likely `inner` or `elem_type`)

**Commands**:
```bash
# Find the correct field name
rg "Array.*\{" kain/crates/kain-core/src/ast.rs -A 2
```

---

#### Task 3: Fix Type::Map Missing Variant (1 error)
**Files**: `kain/crates/ue5-editor/src/editor/reactive.rs`

**Problem**: Code references `Type::Map { .. }` which doesn't exist.

**Location**:
- Line 143: `Type::Map { .. } |`

**Solution**:
1. Check if Map was removed or renamed in the new AST
2. Either remove this pattern match arm or replace with correct variant
3. May need to handle this case differently

---

#### Task 4: Fix Pattern Binding Issue (1 error)
**Files**: `kain/crates/ue5-editor/src/editor/reactive.rs`

**Problem**: Variable `name` not bound in all patterns.

**Location**:
- Lines 142-144:
```rust
Type::Array { .. } |
Type::Map { .. } |
Type::Named { name, .. } if name.contains("Brush") || name.contains("Style")
```

**Solution**:
Restructure the pattern match to bind `name` in all arms or handle separately:
```rust
Type::Array { .. } => false,
Type::Map { .. } => false,
Type::Named { name, .. } if name.contains("Brush") || name.contains("Style") => true,
```

---

#### Task 5: Fix 10 Borrow Checker Errors in style.rs (Lines 73-156)
**Files**: `kain/crates/ue5-editor/src/editor/style.rs`

**Problem**: Immutable borrow of `self.resources` conflicts with mutable borrow of `self` in `push_line()`.

**Locations** (10 errors):
- Line 76-77: `self.push_line(&format!("static const FSlateBrush* Get{}Brush()", ...))`
- Line 80-81: `self.push_line(&format!("static FSlateFontInfo Get{}Font()", ...))`
- Line 84-85: `self.push_line(&format!("static FSlateColor Get{}Color()", ...))`
- Line 88-89: `self.push_line(&format!("static FSlateSound Get{}Sound()", ...))`
- Line 129-132: `self.push_line(&format!("StyleInstance->Set(...)"))`
- Line 134-137: `self.push_line(&format!("StyleInstance->Set(...)"))`
- Line 141-144: `self.push_line(&format!("StyleInstance->Set(...)"))`
- Line 147-150: `self.push_line(&format!("StyleInstance->Set(...)"))`
- Line 153-156: `self.push_line(&format!("StyleInstance->Set(...)"))`

**Solution**:
Collect all strings first, then write them:
```rust
let mut lines = Vec::new();
for (name, resource) in &self.resources {
    match resource.resource_type {
        ResourceType::Brush => {
            lines.push(format!("static const FSlateBrush* Get{}Brush();", 
                self.to_method_name(name)));
        }
        // ... etc
    }
}
// Drop the borrow
for line in lines {
    self.push_line(&line);
}
```

---

### 👤 YOUR HALF (User - 18 errors)

#### Task 6: Fix 18 Borrow Checker Errors in style.rs (Lines 200-241)
**Files**: `kain/crates/ue5-editor/src/editor/style.rs`

**Problem**: Same borrow checker issue in implementation generation loop.

**Locations** (18 errors):
- Line 204-205: `self.push_line(&format!("const FSlateBrush* {}::Get{}Brush()", ...))`
- Line 206: `self.push_line("{")`
- Line 208: `self.push_line(&format!("return StyleInstance->GetBrush(...)"))`
- Line 210: `self.push_line("}")`
- Line 211: `self.push_line("")`
- Line 214-215: `self.push_line(&format!("FSlateFontInfo {}::Get{}Font()", ...))`
- Line 216: `self.push_line("{")`
- Line 218: `self.push_line(&format!("return StyleInstance->GetFontStyle(...)"))`
- Line 220: `self.push_line("}")`
- Line 221: `self.push_line("")`
- Line 224-225: `self.push_line(&format!("FSlateColor {}::Get{}Color()", ...))`
- Line 226: `self.push_line("{")`
- Line 228: `self.push_line(&format!("return StyleInstance->GetSlateColor(...)"))`
- Line 230: `self.push_line("}")`
- Line 231: `self.push_line("")`
- Line 234-235: `self.push_line(&format!("FSlateSound {}::Get{}Sound()", ...))`
- Line 236: `self.push_line("{")`
- Line 238: `self.push_line(&format!("return StyleInstance->GetSound(...)"))`
- Line 240: `self.push_line("}")`
- Line 241: `self.push_line("")`

**Solution**:
Same approach - collect strings first:
```rust
let mut impl_lines = Vec::new();
for (name, resource) in &self.resources {
    let method_name = self.to_method_name(name);
    match resource.resource_type {
        ResourceType::Brush => {
            impl_lines.push(format!("const FSlateBrush* {}::Get{}Brush()", class_name, method_name));
            impl_lines.push("{".to_string());
            impl_lines.push(format!("return StyleInstance->GetBrush(\"{}\");", name));
            impl_lines.push("}".to_string());
            impl_lines.push("".to_string());
        }
        // ... etc for Font, Color, Sound
    }
}
// Drop the borrow
for line in impl_lines {
    self.push_line(&line);
}
```

---

## Verification Steps

After fixes:
```bash
# 1. Build ue5-editor crate
cd kain
cargo build -p ue5-editor --release

# 2. Build entire workspace
cargo build --workspace --release

# 3. Test kain-pro binary
./target/release/kain-pro --version
```

---

## Notes

- The borrow checker errors are all the same pattern - easy to fix once you understand it
- The AST type mismatches need investigation in `kain_core::ast` first
- After fixing, we can delete the old `kain/src/` directory
- Then update steering docs in `.kiro/steering/`

---

## Quick Reference Commands

```bash
# Check AST types
rg "pub enum Type" kain/crates/kain-core/src/ast.rs -A 20

# Check for Argument type
rg "pub struct.*Arg" kain/crates/kain-core/src/ast.rs -A 5

# Build just ue5-editor
cargo build -p ue5-editor --release 2>&1 | head -50

# Count remaining errors
cargo build -p ue5-editor --release 2>&1 | grep "^error\[" | wc -l
```

---

## Success Criteria

✅ All 37 errors resolved
✅ `cargo build --workspace --release` succeeds
✅ `kain-pro` binary compiles
✅ Multi-file UE5 plugin build system works (`kain-pro build --ue5`)
