# kain-core AGENT Notes

## Purpose
- Owns Kain frontend compilation stages (lexing, parsing, type checking) and interpreter-side runtime primitives used by higher crates.
- Exposes a single `compile(source, target)` orchestration path in `src/lib.rs` that stitches stdlib injection, lexer, parser, comptime evaluation, and type checking.

## Component Map
- `src/lib.rs`
  - Module surface and re-exports used by downstream crates.
  - `CompileTarget` enum controls backend target intent and stdlib selection.
- `src/parser.rs`
  - Indentation-aware parser with bounded multi-error accumulation (`MAX_ERRORS = 50`).
  - Enforces a broad reserved-keyword policy spanning Kain, HLSL, C++, and UE5 macro namespaces.
- `src/types.rs` + `src/effects.rs`
  - Type/effect analysis consumed by compile pipeline and monomorphization.
- `src/monomorphize.rs`
  - Generic function/struct/impl instantiation and async lowering to state-machine forms.
  - Tracks instantiated symbols to avoid duplicate expansions.
- `src/runtime.rs`
  - Interpreter value model (`Value`) including actor refs, closures, futures, result flow-control wrappers, and JSX nodes.
  - Python interop conversion (`py_to_value`) and actor messaging primitives.
- `src/low_level_memory.rs` + `src/low_level_abi.rs` + `src/low_level_memory_metadata.rs`
  - ABI-aware struct layout modeling, bitfield packing, alignment policies, and memory diagnostics.
- `src/language_features.rs`
  - Data-driven capability registry controlling parser/runtime feature gates via declarative specs.
- `src/diagnostic_registry.rs`
  - Stable diagnostic code registry (`KAIN-*-0001` family + memory-specific diagnostics).

## Data Flow and Ownership
1. `compile()` prepends stdlib text based on target.
2. `Lexer::tokenize()` converts source to tokens.
3. `Parser::parse()` builds AST with span mapping and multi-error recovery bounds.
4. `comptime::eval_program()` mutates AST before type analysis.
5. `types::check()` returns typed program for downstream codegen crates.
6. Lower-level passes (`monomorphize`, memory lowering) operate on typed items where requested.

## Extension Points
- Add language features by extending `LANGUAGE_CAPABILITY_SPECS` instead of scattering ad-hoc booleans.
- Add diagnostics through `DiagnosticCode`/`DIAGNOSTIC_SPECS` so errors stay machine-indexable.
- Add compile targets by extending `CompileTarget` and stdlib loading contract in `compile()`.
- Extend low-level memory behavior using metadata attributes rather than hardcoded special-cases.

## Known Risks and Edge Cases
- Reserved keyword table drift can create parse/runtime mismatch if new backend keyword sets are added without parser updates.
- `compile()` currently returns placeholder output text; backend handoff contracts must stay aligned with `cli`/codegen crates.
- Monomorphization name mangling and instantiation cache collisions are a latent risk for complex generic signatures.
- Runtime `Value` variants are broad; missing match arms in interpreter helpers can surface as runtime-only defects.
- ABI/layout fallback sizing in low-level memory logic can hide target-specific discrepancies when unknown user types are encountered.

## Validation Commands
- `cargo check -p kain-core --all-targets`
- `cargo test -p kain-core --tests`
- `cargo test -p kain-core test_parser_error_quality -- --nocapture`

## Cross-Repo Touchpoints
- Consumers in `M:/Code/Kain/crates/cli` and other backend crates rely on `kain-core` AST/type/runtime contracts.
- K_OS integration points that invoke Kain parsing/type flows should treat `kain-core` diagnostics and capability flags as source of truth.
