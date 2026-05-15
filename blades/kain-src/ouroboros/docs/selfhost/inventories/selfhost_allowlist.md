# Self-Host Phase 1 Allowlist

Initial slice: `kain-core, kain-import`

## Acceptable diagnostics in phase 1

- Unsupported attribute macros may be preserved as inert metadata if they do not affect semantic lowering.
- Trait impls may be imported lossy if methods are preserved and trait identity is recorded for diagnostics.
- Clap/thiserror/test/cfg style attribute macros may be preserved without full execution in phase 1.
- Directly lowerable self-host macros in the required list must not remain as preserved macro calls in strict mode.
- Non-required macros outside the initial slice may remain preserved temporarily if they do not affect semantic correctness.

## Phase 1 required direct lowering

- `vec`
- `matches`
- `format`
- `write`
- `writeln`

## Immediate hard fail conditions

- panic!/todo!/unimplemented!/unreachable! survive into imported self-host output without explicit lowering policy.
- Trait object (dyn) usage requires semantics we cannot represent and is silently erased.
- A crate outside the initial slice is made mandatory for phase-1 bootstrap.
- A phase1-required direct-lowering macro remains preserved in strict self-host mode.
- Macro expansion changes control flow or data layout and is imported as plain text without metadata.

## Macro policy

### lower_directly

- `eprint`
- `eprintln`
- `format`
- `matches`
- `print`
- `println`
- `vec`
- `write`
- `writeln`

### preserve

- `arg`
- `cfg`
- `command`
- `derive`
- `derive::ClapParser`
- `derive::Clone`
- `derive::Copy`
- `derive::Debug`
- `derive::Default`
- `derive::Deserialize`
- `derive::Eq`
- `derive::Error`
- `derive::Hash`
- `derive::Logos`
- `derive::Ord`
- `derive::Parser`
- `derive::PartialEq`
- `derive::PartialOrd`
- `derive::Serialize`
- `derive::Subcommand`
- `derive::clap::Subcommand`
- `error`
- `from`
- `test`

### reject

- `assert`
- `assert_eq`
- `debug_assert`
- `panic`
- `unreachable`

Trait-object usage count across initial scan: **3**
