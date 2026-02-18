# Codegen Bug Fix Session — Bug 2, 4, 5

**Date:** 2026-02-18  
**Status:** ✅ All fixed, all tests green

---

## Proof of Success

```
cargo test → 102 passed; 0 failed
  ue5         : 66 tests
  ue5-editor  : 10 tests
  ue5-shaders : 26 tests

cargo build → exit 0 (warnings only)

kain build --ue5 (ToonShaderz) → exit 0
  ⚡ 12 shaders generated
  ✓ AToonDirector.h / AToonDirector.cpp
  ✓ ToonShaderzBlueprintLibrary.h/.cpp
  ✓ 8 editor items (6 Slate panels, 1 Details, 1 EditorModule)
```

---

## Bug 2 — SListView Wrong Template Argument

**Symptom:** `TSharedPtr<SListView<TSharedPtr<ItemType>>>` member used `ptr_type`
(which was already `TSharedPtr<ItemType>`) as the template argument, producing
`SListView<TSharedPtr<TSharedPtr<ItemType>>>` — double-wrapped.

**Root cause:** `generate_list_view_support` built the `ptr_type` string as
`TSharedPtr<ElementType>` and then fed that directly into the `SListView<…>`
template argument.

**Fix — `slate.rs`:**
- `list_item_types: HashMap<String, String>` added to `SlateGenerator` — stores
  the *raw* element type name (not pointer-wrapped).
- `list_widget_stype(&slate_class) -> String` helper emits
  `SNew(SListView<TSharedPtr<ItemType>>)` using the raw name.
- All 3 `SNew` call sites that branch on `is_list_widget()` now call
  `list_widget_stype()`.

---

## Bug 4 — Include Pollution

### Editor side (`codegen.rs`)

**Symptom:** Every Slate widget header was emitted for every Slate panel, even
when 90% of the widget types were never referenced.

**Fix:**
- Added free functions `collect_callee_names()` / `collect_callee_names_from_block()` /
  `collect_widget_names_from_struct()` that recursively walk the Compose method
  AST and return a `HashSet<String>` of all Call callee names.
- `write_item_header_preamble` gains a `used_widgets: &HashSet<String>` parameter.
- In the `"Slate"` branch: replaced the flat emit-all list with a `cond_include!`
  macro that guards each header behind a name-in-set check.
- Fallback: if the set is empty (no Compose body), emits all headers so existing
  builds never regress.

### Runtime side (`codegen_ue5.rs`)

**Symptom:** The `__BLUEPRINT_LIBRARY_ONLY__` preamble included *every* type's
header, even Slate widget headers, because it iterated `type_to_header`
unconditionally. Filtering was a hardcoded list of plugin-specific class names —
fragile and non-portable.

**Fix:**
- `blueprint_used_types: HashSet<String>` added to `Ue5Gen`.
- Pre-pass for `@blueprint` functions now calls `collect_type_names(&param.ty, …)`
  and `collect_type_names(&ret, …)` to populate the set from actual signature types.
- `collect_type_names(ty: &Type, …)` free fn recursively extracts named KAIN type
  identifiers from any `Type` node (handles generics, tuples, arrays, refs, fns).
- Blueprint-library preamble filters `type_to_header` to only those keys present
  in `blueprint_used_types`. Fallback: if set is empty, includes everything (safe).

---

## Bug 5 — Shader AddPass_ Duplicate Output UAV

**Symptom:** When a compute shader had an output texture uniform, the uniforms
loop added it to `texture_args`, and then the heuristic "append output UAV" block
*also* appended it — resulting in two identical `AddPass_` arguments and a C++
compile error.

**Fix — `codegen_ue5.rs`:**
- Added `has_output_texture: bool` flag tracked through the uniforms loop.
- Heuristic output UAV block is guarded by `!has_output_texture` — only fires
  when the uniforms loop did not already cover an output texture.

---

## Files Modified

| File | Change |
|------|--------|
| `crates/ue5-editor/src/editor/slate.rs` | Bug 2: `list_item_types`, `list_widget_stype()`, 3 SNew call sites |
| `crates/ue5-editor/src/editor/codegen.rs` | Bug 4 (editor): widget name scanner, conditional includes |
| `crates/ue5/src/codegen_ue5.rs` | Bug 4 (runtime): `blueprint_used_types`, `collect_type_names()`; Bug 5: `has_output_texture` guard |
