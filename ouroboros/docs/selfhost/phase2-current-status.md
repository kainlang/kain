# Phase 2 Current Status

Updated: 2026-03-09

## Executive Summary

Phase 2 is not done yet, but it is firmly in the late-stage semantic reduction phase.

The important part:

- `phase1` is effectively complete.
- The self-host pipeline infrastructure exists and works.
- The repaired `phase2-core` lane is real and useful.
- We are no longer blocked by importer wiring or `.kn` parser breakage.
- We are now blocked by a shrinking set of generated stage2 `kain-core` semantic/helper families.

This is no longer “does the self-host pipeline exist?” work.
This is “finish the remaining generated compiler-core semantic cleanup” work.

## Percentages

These are engineering estimates, not formal proofs.

- Rust bootstrap / `stage0` safety: `95%`
- Phase 1 self-host lane: `100%`
- Phase 2 pipeline infrastructure: `92%`
- Phase 2 repaired-lane automation: `88%`
- Phase 2 `kain-core` compile viability: `72%`
- Phase 2 full workspace viability: `58%`
- Full self-hosted `kain.exe` path: `60%`

Short version:

- Infrastructure is mostly there.
- Compiler-core semantics are the remaining hard part.
- We are materially closer than “halfway,” but not at the finish line yet.

## What Exists Now

### Working Systems

- Inventory-driven self-host policy under `ouroboros/docs/selfhost/inventories`
- Repair-rule system under `ouroboros/docs/selfhost/repairs`
- Manifest-driven orchestration in `ouroboros/docs/selfhost/pipeline_manifest.json`
- Pipeline runner in `ouroboros/tools/selfhost_pipeline/run_pipeline.py`
- Repair engine in `ouroboros/tools/selfhost_repair/repair_runner.py`
- Legacy PowerShell core-check helper in `ouroboros/scripts/selfhost_stage2_core_check.ps1`
- Machine-readable workspace status in `ouroboros/scripts/selfhost_workspace_status.py`
- Repaired stage2 workspace root:
  - `ouroboros/out/selfhost/phase2_repaired/stage2_workspace`

### Proven Milestones

- `Rust -> KAIN -> Rust` round-trip exists for the current phase slice.
- `phase2-core` executes through:
  - repair
  - repaired workspace assembly
  - `cargo check -p kain-core`
- Repair rules are now successfully replacing whole helper families, not just tiny line edits.

## Checklist

### Completed

- [x] Phase 1 strict self-host lane
- [x] Self-host inventories loaded and used
- [x] Parser-safe `.kn` bundle emission
- [x] `.roundtrip.rs` artifact generation
- [x] Stage2 workspace assembly
- [x] Manifest-driven pipeline runner
- [x] Repaired-lane workflow
- [x] Fast `phase2-core` loop
- [x] Repair rules now support:
  - multi-line regex replacement
  - `phase2` rules applying to `phase2-core` / `phase2-full`
- [x] Major repaired helper families already in place:
  - low-level storage helper family
  - malformed memory-support scanning family
  - monomorphization bootstrap shell
  - MonoContext derive cleanup
  - MonoContext instantiate / instantiate_struct / instantiate_impl_methods stubs
  - `mangle_types`
  - `unify`
  - `infer_type_args`
  - `substitute_type`
  - `resolved_to_ast_type`
  - `MonoTypeEnv::define`
  - `lower_async_fn`

### In Progress

- [ ] Async/state-machine bootstrap family
- [ ] Scan/typechecker helper family
- [ ] Runtime/render/display bootstrap family
- [ ] Ownership/API drift helper cleanup
- [ ] Repeated green `phase2-core`
- [ ] `phase2-full` green workspace build
- [ ] Self-hosted stage2 `cli` binary / `kain.exe`

### Not Done Yet

- [ ] Full stage2 workspace green
- [ ] Full stage2 `cli` binary build
- [ ] Final documentation of bounded bootstrap-only exceptions

## Current Lane Status

### Pipeline Summary

Latest `phase2-core` summary:
- `ouroboros/out/selfhost/pipeline/phase2-core_summary.json`

Current state from that summary:
- lane: `phase2-core`
- success: `false`
- repaired workspace exists: `yes`
- stage2 binary exists: `no`

### Current Blocker Buckets

From the latest pipeline summary:

- `type_shape_mismatch`: `516`
- `result_option_unit_coercion`: `269`
- `unknown`: `167`
- `memory_lowering_leakage`: `49`
- `spawn_runtime_leakage`: `41`
- `trait_impl_fidelity`: `35`
- `placeholder_none`: `28`
- `parser_helper_leakage`: `21`
- `runtime_helper_transliteration`: `19`
- `span_ownership`: `18`

These counts are useful for trend direction, but the most trustworthy current front is the direct repaired-workspace `cargo check`, not the broad original `stage2_build.log`.

## Current Failing Front

Most recent authoritative compiler check:

- direct `cargo check -p kain-core` in:
  - `ouroboros/out/selfhost/phase2_repaired/stage2_workspace`

The current front is concentrated in these families:

### 1. Async / state-machine bootstrap family

Files/functions:
- `split_at_awaits`
- `generate_state_arm`

Current symptoms:
- `Option<bool>` / `bool` drift
- `Stmt` vs `bool` drift
- indexing `await_points` with `u64` instead of `usize`
- `contains_key` inference fallout

Why it matters:
- this is still leftover generated async lowering/state-machine synthesis
- this family should be treated as one repair target, not many tiny type errors

### 2. Scan / typechecker helper family

Files/functions:
- `scan_function`
- `scan_stmt`
- `scan_expr`

Current symptoms:
- malformed `None`-driven iterator predicates like `.any(None)`
- `Ok(None)` where unit is expected
- `ResolvedType::Ref` / `Ptr` field-name drift (`mutable_` vs `mutable`)
- struct-field collection shape drift

Why it matters:
- this is now the main semantic-analysis family still leaking malformed generated code

### 3. Runtime / render / display bootstrap family

Files/functions:
- `render_to_string`
- `Value::truthy`
- `Display` / `fmt_impl` surface for `VNode` and `Value`

Current symptoms:
- string-joining API misuse
- missing `fmt_impl`
- numeric truthiness drift (`0` vs `0.0`)

Why it matters:
- this is mostly runtime/UI support, not core compiler theory
- it should be repairable with bounded bootstrap-safe replacements

### 4. Ownership / API drift helpers

Files/functions:
- `spec_for_code`
- `with_override`
- `policy_entry`
- `first_unsupported_memory_context*`

Current symptoms:
- clone-vs-move drift
- `mut self` missing
- moved enum/config values reused
- memory-support scan still moves `caps` instead of borrowing

Why it matters:
- these are small but noisy blockers
- they should be fixed as a helper cluster, not ad hoc

## What It Has So Far

### Self-host Architecture

- phase1 bootstrap lane
- phase2 repaired lane
- pipeline manifest
- repair-rule engine
- bootstrap feature policy
- inventories
- probe corpus
- repaired workspace loop

### Artifact Outputs

Current phase2 outputs include:
- `.kn` bundles
- `.probe.rs`
- `.roundtrip.rs`
- stage2 workspace
- stage2 logs
- repair reports
- pipeline summaries

### Repair Strategy Already Working

The repair-rule iteration strategy is working and should remain the default approach.

It has already successfully absorbed these classes:
- malformed storage seed helpers
- malformed memory support scan helpers
- monomorphization shell issues
- various derive / receiver / helper signature drifts

That is why the failure front has kept moving forward instead of stagnating.

## What The Goal Is

The current goal is still:

1. make `phase2-core` pass repeatedly
2. then make `phase2-full` pass
3. then produce a buildable stage2 `cli` binary
4. which becomes the first real self-hosted `kain.exe` lane

This is not “fake a compile.”
This is “make the staged self-host pipeline genuinely compile the self-host slice.”

## Recommended Next Steps

In order:

1. Repair the async/state-machine bootstrap family
   - `split_at_awaits`
   - `generate_state_arm`

2. Repair the scan/typechecker helper family
   - `scan_function`
   - `scan_stmt`
   - `scan_expr`

3. Repair the runtime/render/display bootstrap family
   - `render_to_string`
   - `Value::truthy`
   - missing `fmt_impl` helpers

4. Clean the small ownership/API drift helpers
   - `spec_for_code`
   - `with_override`
   - `policy_entry`
   - borrow `caps` in memory-support scan helpers

5. Rerun `phase2-core`
6. When `phase2-core` is green repeatedly, promote to `phase2-full`

## Bottom Line

This is close enough that the remaining work is clearly structured.

The pipeline is real.
The repairs are real.
The current blockers are no longer broad or mysterious.

If this continues at the current trajectory, the next major milestone is not “understand the problem.”
It is “finish the remaining helper families and get the first green `phase2-core`.”
