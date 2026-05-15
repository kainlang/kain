---
title: Parser-safe enum variant forms in selfhost output
---

# Parser-safe enum variant forms in selfhost output

This note captures two concrete KAIN surface forms that the current parser accepts and that `crates/cli/src/selfhost.rs` must preserve when emitting `.kn` roundtrip sources.

## Source references

- `crates/kain-core/src/parser.rs`
  - expression parsing around `parse_primary`
  - pattern parsing around `parse_pattern`
- Generated bundle examples:
  - `out/selfhost/phase2/kain-core.kn:1454-1463`
  - `out/selfhost/phase2/kain-core.kn:1713-1722`

## Case 1: unit enum variant in expression position

Observed parser-safe form:

```kain
impl Default for TraceType:
    fn default_() -> Self_:
        TraceType__Line
```

Why this is safe:

- The parser accepts a plain identifier expression.
- For unit variants, a fully flattened single identifier is safe in selfhost output.
- This avoids relying on a multi-token enum-variant expression shape when a single identifier already roundtrips correctly.

Emitter rule in `selfhost.rs`:

- `Expr::EnumVariant` with `EnumVariantFields::Unit` emits:
  - `EnumHead__Variant`

Example:

- `TraceType` + `Line` -> `TraceType__Line`

## Case 2: qualified enum variant struct pattern in match arms

Observed parser-safe form:

```kain
fn enhance_error_with_location(error: crate::error::KainError, span_mapper: &SpanMapper, file: &String) -> crate::error::KainError:
    match error:
        crate__error__KainError::Codegen { message: message, span: span } =>
            let loc = span_mapper.span_to_location(span, file)
            crate__error__KainError__codegen_with_location(message, loc.file, loc.line_, loc.col, span)
```

Why this is required:

- `parse_pattern` accepts variant patterns only in the form `Ident::Variant`, optionally followed by tuple or struct fields.
- It does **not** accept an unqualified struct variant pattern like:

```kain
Codegen { message: message, span: span }
```

- It also does not parse a multi-segment enum path before `::`.
- Therefore the enum path must be flattened to a single identifier head, while preserving the final `::Variant` split.

Emitter rule in `selfhost.rs`:

- `Pattern::Variant` with a qualified enum name emits:
  - `FlattenedEnumHead::Variant`

Example:

- `crate::error::KainError` + `Codegen` -> `crate__error__KainError::Codegen`

## Summary

The current selfhost emitter should preserve these two distinct spellings:

- Unit enum variant expressions:
  - `EnumHead__Variant`
- Qualified variant patterns:
  - `FlattenedEnumHead::Variant`

That split matches the parser constraints in `kain-core` and keeps the generated selfhost `.kn` sources parser-safe.
