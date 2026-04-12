# Effects And Capabilities

Snapshot: April 12, 2026.

Kain tracks semantic permissions and target availability explicitly. This page
is the canonical home for the current effect vocabulary and for the rule that
parser support, runtime support, and target support are separate concerns.

## Effects

The current effect vocabulary includes:

- `Pure`
- `IO`
- `Async`
- `GPU`
- `Reactive`
- `Unsafe`
- `Alloc`
- `Panic`

Effects are not decorative annotations. They tell readers and compiler passes
which operations are expected, which runtime services may be needed, and which
lowering paths are valid for a given declaration or block.

## Capabilities

The live capability registry in `crates/kain-core/src/language_features.rs`
currently gates a small but important set of parser and runtime behaviors, such
as:

- struct literal parsing
- bitwise operator parsing and execution
- shift operator parsing and execution

The important documentation rule is that capability gating is separate from
syntax shape. A feature may parse, typecheck, or lower only when the relevant
capability is enabled, and docs should say which layer owns the gate.

One capability deserves a special note: the current registry marks
`ParserStructLiterals` as enabled by default, but the current unit test
`default_profile_keeps_struct_literals_disabled` still fails. Treat the struct
literal default as unsettled until the code and tests are reconciled. Do not
write docs that claim that default is stable.

## How To Read Capability-Gated Behavior

When a feature seems missing, check the layers in this order:

1. parser support in `crates/kain-core/src/ast.rs`
2. semantic/runtime support in `crates/kain-core/src/runtime.rs`
3. capability gating in `crates/kain-core/src/language_features.rs`
4. target lowering in `crates/kain-driver/src/lib.rs`

That order matters because Kain often supports a concept in the language core
before every target is ready to consume it.

## Related Guide Pages

- `syntax-and-semantics/modules-and-items.md`
- `runtime/effects-io-async-and-patching.md`
- `runtime/runtime-model.md`
- `reference/feature-matrix.md`

## Practical Rule

If you are documenting a feature, always say whether it is:

- a syntax feature
- a runtime feature
- a target feature
- a capability-gated feature

That distinction prevents stale prose from accidentally turning a current
limitation into a permanent language rule.
