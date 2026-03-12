# kain-selfhost - Agent Notes

## Purpose
`kain-selfhost` defines the typed data contracts for the self-hosting repair/validation lane. The crate is intentionally dependency-light (`serde`, `serde_json`) and models:
- lane/step execution plans (`SelfHostLane`, `SelfHostStep`, `StepKind`)
- repair rule and taxonomy payloads (`RepairRule`, `TaxonomyBucket`)
- blocker classification (`BlockerBucket`)
- preflight/report outputs (`StructuralPreflightReport`, `SelfHostLaneSummary`)
- artifact/path contracts (`ArtifactContract`, `SelfHostPaths`)

Primary source:
- [`M:/Code/Kain/crates/kain-selfhost/src/lib.rs`](M:/Code/Kain/crates/kain-selfhost/src/lib.rs)

## Data Flow And Ownership
1. Orchestrators deserialize lane configs and rule/taxonomy data into `kain-selfhost` structs.
2. Preflight and execution stages produce typed status (`PreflightFailure`, `StepExecutionSummary`).
3. Reporting consolidates run outcomes into `SelfHostLaneSummary` for downstream automation and dashboards.

Ownership boundaries:
- This crate owns schemas and enums, not execution side effects.
- Command execution, FS mutation, and retry behavior are interpreted by callers from declarative fields like `command_template`, `retry_policy`, and artifact expectations.

## Extension Points
- Add new step runtimes via `StepKind` (for example, dedicated Rust executor mode).
- Expand `BlockerBucket` and `TaxonomyBucket` to improve routing granularity.
- Evolve `RepairRule.scope` for finer crate/file targeting without changing caller APIs.
- Version external JSON payloads with additive fields first to avoid deserialization breaks.

## Known Risks / Edge Cases
- `target_kind`, `severity`, `retry_policy`, and `failure_policy` are free-form strings; callers must validate allowed values.
- `command_template` execution is caller-defined; sanitize interpolation inputs to avoid command injection.
- Path fields are plain strings and may be relative/invalid per environment; normalize/validate before use.
- `confidence: Option<f64>` has no enforced range in schema; enforce bounds in writer/consumer layers.

## Validation Commands
From [`M:/Code/Kain`](M:/Code/Kain):

```powershell
cargo check -p kain-selfhost
cargo test -p kain-selfhost
```
