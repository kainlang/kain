# Unified Automation Changelog

This is the single accumulated changelog for the Ouroboros V2 self-host automation loop.

Rules:

- Add one entry per turn.
- Keep entries concise and evidence-based.
- Link the turn number, lane, primary outcome, validation signal, and next handoff.
- Do not replace the per-turn report. This file is the compressed cross-turn history.

## Template

### TURN-XXX - lane

- Date:
- Summary:
- Kain changes:
- OuroborosV2 changes:
- Validation:
- Next handoff:

## Entries

### TURN-018 - docs

- Date: 2026-03-12
- Summary: Prioritized the Tier-1 backlog around the current `E0428` duplicate-type blocker family in repaired `kain-core` and anchored it to the exact `phase2-core` proving command.
- Kain changes: none
- OuroborosV2 changes: updated `automation/BACKLOG.md` with a new top Tier-1 repair-facing blocker item; added `automation/reports/TURN-018-docs.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed and still reports `duplicate_type_blockers` with stage2 binary absent
- Next handoff: continue rotation to importer for turn 19; on the next repair lane, implement and validate duplicate-type dedup repair coverage against `kain-core` using `python tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core`

### TURN-017 - validation

- Date: 2026-03-12
- Summary: Added machine-readable duplicate type blocker extraction to workspace status output so `E0428` families are explicit for repair targeting.
- Kain changes: none
- OuroborosV2 changes: updated `scripts/selfhost_workspace_status.ps1` to emit `duplicate_type_blockers`; added `automation/reports/TURN-017-validation.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed after regex hotfix and now includes `duplicate_type_blockers`; `cargo run -p cli --bin kain -- selfhost phase2` failed (`Self-host phase2 failed`, stage2 build exit code 101)
- Next handoff: in the next repair lane, target duplicate-type surface deduplication in repaired `kain-core` (`Span`/`Spanned` and typed-program/type-env clusters), then rerun `phase2-core`

### TURN-016 - repair

- Date: 2026-03-12
- Summary: Added a bounded parser-helper surface injection pass in the repair runner so repaired `kain-core` regains missing parser helper methods before core-check compilation.
- Kain changes: none
- OuroborosV2 changes: updated `tools/selfhost_repair/repair_runner.py` with synthetic `parser_helper_surface_injection`; added `automation/reports/TURN-016-repair.md`
- Validation: `python -m py_compile tools/selfhost_repair/repair_runner.py` passed; `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; first `python tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core` failed at `repair_skip_validation` due `FileExistsError` on `phase2_repaired` copy; direct `python tools/selfhost_repair/repair_runner.py repair --validation skip ...` passed; rerun `python tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core` passed repair step and failed at `core_check` (rc=101); `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_repair_loop.ps1` completed with `validation_success=false`
- Next handoff: on the next repair turn, target the new `E0428` duplicate-type family in repaired `kain-core` (`Span`/`Spanned` and typed-program/type-env clusters) now that parser-helper method-missing failures are no longer the front blocker in core-check output

### TURN-015 - pipeline

- Date: 2026-03-11
- Summary: Added structured stage2 assembly evidence to selfhost phase reports so each stage2 crate records its roundtrip source input and emitted workspace files.
- Kain changes: updated `crates/cli/src/selfhost.rs` to return and record `Stage2WorkspaceCrateEvidence` during stage2 workspace assembly; updated `crates/cli/src/selfhost_report.rs` report schema/markdown rendering with `stage2_workspace_crates`
- OuroborosV2 changes: added `automation/reports/TURN-015-pipeline.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo run -p cli --bin kain -- selfhost phase1` passed; `python tools/selfhost_pipeline/run_pipeline.py run --lane analyze` passed
- Next handoff: on the next repair turn, use `stage2_workspace_crates` provenance in `out/selfhost/phase2/phase2_report.json` while targeting the current parser-helper `E0599` blocker family in repaired `kain-core`

### TURN-014 - importer

- Date: 2026-03-11
- Summary: Added regression coverage for strict unsupported literal/pattern class markers in importer selfhost policy matching and Rust transformer emission tests.
- Kain changes: updated `crates/kain-import/src/rust/selfhost.rs` with class-marker allowlist tests for `unsupported_literal_lowering` and `unsupported_pattern_lowering`; updated `crates/kain-import/src/rust/transformer.rs` with class-emission tests that trigger those fallback diagnostics
- OuroborosV2 changes: added `automation/reports/TURN-014-importer.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo check -p kain-import -p cli -p kain-selfhost -p kain-sys-codegen` passed; `cargo test -p kain-import --test c_abi_corpus -- --nocapture` passed; `cargo test -p kain-import selfhost::tests -- --nocapture` passed; `cargo test -p kain-import transformer::tests::records_unsupported_ -- --nocapture` passed after fixture correction
- Next handoff: continue class-tagging remaining strict diagnostics in importer paths so allowlist/hard-fail policy can move fully to stable `class:<id>` markers

### TURN-013 - importer

- Date: 2026-03-11
- Summary: Added stable strict diagnostic class tags for unsupported fallback lowering families (`expression`, `literal`, `pattern`) and wired selfhost policy classification for the new classes.
- Kain changes: updated `crates/kain-import/src/rust/transformer.rs` to emit `class:unsupported_expr_lowering`, `class:unsupported_literal_lowering`, and `class:unsupported_pattern_lowering` for strict fallback diagnostics; updated `crates/kain-import/src/rust/selfhost.rs` classifier/known class mapping and added one class-marker matching regression test
- OuroborosV2 changes: added `automation/reports/TURN-013-importer.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo check -p kain-import -p cli -p kain-selfhost -p kain-sys-codegen` passed; `cargo test -p kain-import --test c_abi_corpus -- --nocapture` passed; `cargo test -p kain-import selfhost::tests -- --nocapture` passed; exploratory `cargo test -p kain-import records_unsupported_pattern_lowering_class_marker -- --nocapture` failed and the brittle test was removed
- Next handoff: on the next importer turn, add deterministic fixtures that exercise the new literal/pattern fallback class markers so all three new strict classes have direct transformer test coverage

### TURN-012 - docs

- Date: 2026-03-11
- Summary: Corrected control-plane README drift by fixing the protected `legacy` path casing and removing a duplicated `update-changelog` script entry from the file inventory.
- Kain changes: none
- OuroborosV2 changes: updated `automation/README.md`; added `automation/reports/TURN-012-docs.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed
- Next handoff: on the next importer turn, keep strict diagnostic hardening as the main lever while keeping docs/config command inventories drift-free

### TURN-011 - validation

- Date: 2026-03-11
- Summary: Exposed phase-1 inventory input evidence in workspace status output so validation runs surface manifest-drift inputs (key/path/byte-size/existence) in one command.
- Kain changes: none
- OuroborosV2 changes: updated `scripts/selfhost_workspace_status.ps1` to emit `phase1_inventory_evidence`; added `automation/reports/TURN-011-validation.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed and now includes `phase1_inventory_evidence`; `cargo run -p cli --bin kain -- selfhost phase2` failed with `Self-host phase2 failed` and `stage2_build_exit_code: 101` (`out/selfhost/phase2/stage2_workspace/stage2_build.log`)
- Next handoff: in repair lane, target the current `E0599` parser-method family in `out/selfhost/phase2/stage2_workspace/crates/kain-core/src/lib.rs`; keep validation status output as the one-command triage entrypoint

### TURN-010 - repair

- Date: 2026-03-11
- Summary: Added a targeted parser-impl closure balancing pass in the repair runner to move phase2-core past the unclosed-delimiter front blocker and expose the next parser-method-resolution blocker family.
- Kain changes: none
- OuroborosV2 changes: updated `tools/selfhost_repair/repair_runner.py` with `parser_impl_fragment_closure_balance`; added `automation/reports/TURN-010-repair.md`
- Validation: `python -m py_compile tools/selfhost_repair/repair_runner.py` passed; `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `python tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core` first failed in `repair_skip_validation` due to concurrent `phase2_repaired` copy race (`FileExistsError`), sequential rerun passed repair step and failed at `core_check` (rc=101) with front `E0599` parser method-resolution errors; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_repair_loop.ps1` completed with `validation_success=false`
- Next handoff: in repair lane, add a bounded follow-up transform that keeps parser helper methods attached to `impl Parser` after closure balancing and verify with `phase2-core` rerun

### TURN-009 - pipeline

- Date: 2026-03-11
- Summary: Added per-file inventory input evidence to self-host phase reports so phase runs record exact inventory keys, paths, and byte sizes for reproducibility and drift triage.
- Kain changes: updated `crates/cli/src/selfhost.rs` to centralize inventory file specs, capture inventory metadata, and include `inventory_inputs` in phase reports; updated `crates/cli/src/selfhost_report.rs` report schema/markdown rendering for `inventory_inputs`
- OuroborosV2 changes: added `automation/reports/TURN-009-pipeline.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo run -p cli --bin kain -- selfhost phase1` passed; `python tools/selfhost_pipeline/run_pipeline.py run --lane analyze` passed
- Next handoff: wire `inventory_inputs` into `scripts/selfhost_workspace_status.ps1` so inventory drift evidence is visible in one status command without opening raw phase reports

### TURN-008 - importer

- Date: 2026-03-11
- Summary: Added stable strict diagnostic class tagging for trait/impl lowering (`class:trait_surface_lowering`) and wired selfhost allowlist matching for the new class.
- Kain changes: updated `crates/kain-import/src/rust/transformer.rs` to emit `trait_surface_lowering` class markers for trait/impl lossy surfaces; updated `crates/kain-import/src/rust/selfhost.rs` classifier/known classes and added matching regression tests
- OuroborosV2 changes: added `automation/reports/TURN-008-importer.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo check -p kain-import -p cli -p kain-selfhost -p kain-sys-codegen` passed; `cargo test -p kain-import --test c_abi_corpus -- --nocapture` passed; `cargo test -p kain-import selfhost::tests -- --nocapture` passed; `cargo test -p kain-import records_trait_surface_lowering_class_marker -- --nocapture` passed
- Next handoff: keep expanding stable class tags for remaining strict diagnostics (`unsupported expression/pattern/literal`) so policy can retire heuristic message matching further

### TURN-007 - importer

- Date: 2026-03-11
- Summary: Emitted stable strict diagnostic class markers from the Rust transformer and updated selfhost classification to parse inline `class:<id>` markers before heuristic text matching.
- Kain changes: updated `crates/kain-import/src/rust/transformer.rs` to emit class-tagged strict diagnostics for dyn-trait lowering, external `mod` declarations, macro direct-lowering misses, and macro-policy rejections; updated `crates/kain-import/src/rust/selfhost.rs` to parse inline class markers and added one class-marker matching regression test
- OuroborosV2 changes: added `automation/reports/TURN-007-importer.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo check -p kain-import -p cli -p kain-selfhost -p kain-sys-codegen` passed; `cargo test -p kain-import --test c_abi_corpus -- --nocapture` passed; `cargo test -p kain-import selfhost::tests -- --nocapture` passed; `cargo test -p kain-import records_external_mod_decl_class_marker -- --nocapture` passed
- Next handoff: continue importer hardening by propagating explicit class tags to remaining strict diagnostics so allowlist/hard-fail policy can retire heuristic message matching over time

### TURN-006 - docs

- Date: 2026-03-11
- Summary: Corrected validation command inventory drift by replacing stale `phase2 --build-stage2 false` syntax with the current CLI-compatible `phase2` command.
- Kain changes: none
- OuroborosV2 changes: updated `automation/config/pipeline.config.json` validation lane command; added `automation/reports/TURN-006-docs.md`
- Validation: `node automation/scripts/next-turn.mjs` passed; `cargo run -p cli --bin kain -- selfhost phase2 --help` passed; `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed
- Next handoff: on the next validation turn, run `cargo run -p cli --bin kain -- selfhost phase2` directly to capture fresh runtime evidence using the corrected command inventory

### TURN-005 - validation

- Date: 2026-03-11
- Summary: Added explicit validation evidence fields in workspace status output so core-check/build failures and front blockers are immediately actionable.
- Kain changes: none
- OuroborosV2 changes: updated `scripts/selfhost_workspace_status.ps1` to emit `phase2_core_check`, `phase2_build_evidence`, and `front_blocker`; added `automation/reports/TURN-005-validation.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo run -p cli --bin kain -- selfhost phase2 --build-stage2 false` failed (CLI arg contract drift: unexpected argument `false`); fallback `cargo check -p cli` passed
- Next handoff: align validation lane `phase2` command syntax in `automation/config/pipeline.config.json` with current CLI flags and re-run phase2 validation command without fallback

### TURN-004 - repair

- Date: 2026-03-11
- Summary: Added panic-macro lowering coverage to repair taxonomy/rules so recurrent `panic!("...".to_string())` drift is explicitly classified and repair-addressable.
- Kain changes: none
- OuroborosV2 changes: updated `docs/selfhost/repairs/repair_rules.json` with `panic_literal_to_string_fix`; updated `docs/selfhost/repairs/error_taxonomy.json` with `panic_macro_lowering`; added `automation/reports/TURN-004-repair.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `python tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core` failed at `core_check` (return code 101, unclosed delimiter front); `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_repair_loop.ps1` completed with `validation_success=false`; JSON parse checks for both modified repair docs passed
- Next handoff: target the current unclosed-delimiter parser cluster in `out/selfhost/phase2_repaired/stage2_workspace/crates/kain-core/src/lib.rs` with a bounded repair-family rule so phase2-core can progress past the first front blocker

### TURN-003 - pipeline

- Date: 2026-03-11
- Summary: Added explicit stage-2 build evidence to selfhost phase reports by wiring build log path and process exit code into JSON/markdown output and CLI summary.
- Kain changes: updated `crates/cli/src/selfhost.rs` to capture `stage2_build_log_path` and `stage2_build_exit_code` from stage2 cargo build; updated `crates/cli/src/selfhost_report.rs` to serialize/render those fields
- OuroborosV2 changes: added `automation/reports/TURN-003-pipeline.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo run -p cli --bin kain -- selfhost phase1` passed; `python tools/selfhost_pipeline/run_pipeline.py run --lane analyze` passed
- Next handoff: wire the new report fields into `scripts/selfhost_workspace_status.ps1` and/or pipeline summary readers so stage2 failure triage can consume log path and exit code directly

### TURN-002 - importer

- Date: 2026-03-11
- Summary: Added class-aware strict diagnostic policy matching so allowlist/hard-fail rules can key on stable classes (`class:<id>`) or mapped natural-language policy markers.
- Kain changes: updated `crates/kain-import/src/rust/selfhost.rs` with diagnostic classification and marker-class matching helpers; added 3 importer unit tests for class marker behavior (6 selfhost tests total)
- OuroborosV2 changes: added `automation/reports/TURN-002-importer.md`
- Validation: `python tools/selfhost_pipeline/run_pipeline.py list` passed; `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/selfhost_workspace_status.ps1` passed; `cargo check -p kain-import -p cli -p kain-selfhost -p kain-sys-codegen` passed; `cargo test -p kain-import --test c_abi_corpus -- --nocapture` passed; `cargo test -p kain-import selfhost::tests -- --nocapture` passed (6 tests)
- Next handoff: add explicit diagnostic class emission at transformer diagnostic creation sites so importer policy matching can eventually rely on typed tags instead of heuristic message classification

### TURN-001 - importer

- Date: 2026-03-11
- Summary: Wired strict self-host diagnostic filtering to use allowlist policy (`phase1_acceptable_diagnostics` and `hard_fail_conditions`) with explicit precedence and unit coverage.
- Kain changes: updated `crates/kain-import/src/rust/selfhost.rs` to enforce allowlist-driven diagnostic gating and added 3 unit tests under `rust::selfhost::tests`
- OuroborosV2 changes: added `automation/reports/TURN-001-importer.md`
- Validation: `cargo check -p kain-import -p cli -p kain-selfhost -p kain-sys-codegen` passed; `cargo test -p kain-import --test c_abi_corpus -- --nocapture` passed; `cargo test -p kain-import selfhost::tests -- --nocapture` passed (3 tests)
- Next handoff: continue importer hardening by emitting/consuming stable diagnostic classes in `kain-import` so policy matching can move from substring matching to typed codes

### TURN-000 - bootstrap

- Date: 2026-03-11
- Summary: Seeded the hourly self-host automation control plane for Ouroboros V2 and mapped the split between control-plane docs and live Kain implementation.
- Kain changes: none
- OuroborosV2 changes: added automation config, blueprint, logic map, prompt, scripts, and report template
- Validation: automation scripts parsed and the existing selfhost pipeline runner still listed its lanes
- Next handoff: start with the importer lane and harden `crates/kain-import/src/rust/selfhost.rs`

