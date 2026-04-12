# Kore

Kore is the first hand-owned language core in this tree. It is the pre-UE5
foundational language layer: syntax, AST, features, typing, runtime, and native
LLVM handoff. It is not a mirror of `crates/`, and it is not the selfhost
pipeline.

## Ownership Contract

- `src/kore` is the canonical owned source tree.
- `src/rust-import` is a generated reference corpus only.
- `src/.legacy` is an archival donor tree only.
- If a choice exists between legacy shape and current language intent, current
  Kore intent wins.
- If a topic belongs to UE5, shader codegen, or pipeline orchestration, it does
  not belong in Kore wave 1.

## Donor Matrix

| Surface | Primary donor | Secondary donor | Kore ownership |
| --- | --- | --- | --- |
| AST and syntax shape | `src/.legacy/src/ast.kn` | `crates/kain-core/src` | Owned here |
| Lexing and parsing rules | `src/.legacy/src/lexer.kn`, `parser_v2.kn` | `crates/kain-core/src` | Owned here |
| Spans, diagnostics, errors | `src/.legacy/src/span.kn`, `diagnostic.kn`, `error.kn` | `crates/kain-core/src` | Owned here |
| Effects and typing | `src/.legacy/src/effects.kn`, `types.kn` | `crates/kain-core/src` | Owned here |
| Comptime and runtime substrate | `src/.legacy/src/korec.kn`, `stdlib.kn` | `crates/kain-core/src` | Owned here |
| Native ABI and memory | legacy runtime donors | current C runtime work | Owned here |
| UE5 / shader / pipeline layers | none | excluded | Not in wave 1 |

## Wave 1 Scope

Wave 1 is the smallest useful owned Kore core:

- `ast.kn`
- `language_features.kn`
- later: lexer, parser, span, diagnostic, error, effects, types, comptime,
  runtime, stdlib, low-level memory, low-level ABI, and `korec`

The current goal is not full language completion. The goal is to establish the
owned syntax model and the contract that lets agents rewrite the core without
guesswork.

