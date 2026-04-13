# Kain Self-Host Auto-Repair

This doc covers the repair lane around `kain-selfhost`, the profile-driven source repair crate, and the rules for keeping repair bounded.

## What the repair lane is

The repair feature is not a general code rewriter. It is a staged, data-driven loop for copied self-host outputs and other parser-hostile source slices:

1. collect the current self-host inventory and failure evidence
2. generate a lane summary and repair report
3. apply a selected repair profile to copied artifacts only
4. validate the repaired copy against the next check/build step
5. record which repair families were hit and which blockers remain

The source tree is never the repair target. The repaired workspace is the disposable proving ground.

## Architecture notes

### `crates/kain-selfhost`

`kain-selfhost` owns the typed contracts for the lane. It is deliberately small and dependency-light. The crate models:

- `SelfHostLane` / `SelfHostStep` / `StepKind` for declarative lane execution
- `RepairRule` / `RuleScope` / `MatchType` for rule-driven repairs
- `Taxonomy` / `TaxonomyBucket` / `BlockerBucket` for failure classification
- `ArtifactContract` / `ArtifactExpectation` for file and output expectations
- `StructuralPreflightReport` / `SelfHostLaneSummary` / `StepExecutionSummary` for reporting
- `SelfHostPaths` for resolved roots and report locations

That crate is the schema. Execution and filesystem mutation still live in the CLI and orchestration layers.

### `crates/kain-repair`

`kain-repair` is the actual repair engine used by `kain doctor` and the repair-oriented CLI flow. It is profile-driven and intentionally conservative.

Current profile knobs:

- `normalize_line_endings`
- `trim_trailing_whitespace`
- `collapse_extra_blank_lines`
- `fix_unterminated_block_comments`
- `normalize_indentation`
- `rewrite_reserved_identifiers`
- `normalize_self_constructor_syntax`
- `rewrite_inline_initializers`
- `normalize_namespace_paths`
- `reconstruct_parser_safe_blocks`
- `ensure_final_newline`

The default profile enables all of the above. That makes the lane useful for ugly parser inputs without turning it into a semantic editor.

The repair engine also separates mode from profile:

- `Check` / `DryRun`-style behavior reports what would change
- `Suggest` emits the fix list without writing
- `ApplySafe` writes only the conservative repairs
- `ApplyAggressive` allows the few broader recovery steps that still stay local to source shape

### CLI flow

The `kain selfhost phase2` lane is the current repair-oriented workflow. It is the main entrypoint when you want to exercise the repair system against real self-host outputs.

Typical flow:

- import or lower the selected self-host slice
- emit phase2 bundles and round-trip Rust where configured
- assemble the stage2 workspace
- build or check the stage2 workspace
- classify failures into buckets
- apply explicit repair rules to the repaired copy
- write repair reports and validation logs

The `kain doctor` command is the fastest way to inspect the active compiler build before using the repair lane. Use it to confirm the executable, build stamp, git SHA, supported targets, and environment wiring before you trust repair output.

## Doctor repair mode usage

There is no magic hidden mode here. The doctor step is the triage front door.

Use it like this:

```powershell
kain doctor
kain doctor --repair path\to\file.kn --dry-run
kain doctor --repair path\to\file.kn --suggest
kain doctor --repair path\to\file.kn --write
```

Recommended operator pattern:

1. run `kain doctor` first
2. verify the binary and build provenance are the one you intended to test
3. run `kain doctor --repair <file> --dry-run` to see the repair surface without mutating anything
4. use `--suggest` when you want the fix list but do not want a write path at all
5. use `--write` only when the repair is narrow, explainable, and already matched to a known profile
6. inspect the generated report and validation log in the repaired output root before promoting anything upstream

If the repaired lane regresses, the doctor output tells you whether you were testing the wrong binary, the wrong workspace, or the wrong target surface. It does not fix the problem for you. It just keeps the trapdoor visible.

## Guardrails

### Safe repair: syntax and structure

Safe repairs are the ones that preserve meaning while restoring compilability or lane shape:

- line-ending normalization and trailing whitespace cleanup
- missing final newline repair
- unterminated block-comment closure
- indentation normalization when the structure is already obvious
- reserved-identifier rewrites that are mechanically derived from parser rules
- constructor-syntax normalization and other parser-safe shape fixes
- namespace-path normalization when separators are clearly malformed
- block reconstruction that only restores the shape the parser already implied

These repairs should be narrow, explicit, and easy to explain in a report.

### Dangerous repair: semantic rewriting

Do not let the repair lane become a silent semantic editor.

Avoid repairs that:

- change algorithmic behavior without direct evidence
- rewrite ownership, lifetimes, or control flow beyond the local failure seam
- guess at missing business logic
- promote a temporary workaround into canonical source truth
- mutate the original generated source instead of the repaired copy
- infer new APIs, new data flow, or hidden intent from the parser failure alone

If the repair requires interpretation, it belongs in source generation, importer logic, or a targeted upstream fix, not in blind auto-repair.

### Practical rule

If a human could not explain the repair in one sentence using the failure log and the emitted diff, it is probably too broad.

## Output locations

The lane writes its evidence into the repaired root, typically under `out/selfhost/phase2_repaired`:

- `phase2_repair_report.json`
- `phase2_repair_report.md`
- `stage2_workspace/`
- `stage2_workspace/stage2_build.log` or `stage2_repair_build.log`
- `front_errors.json` / `front_errors.md` when produced by the pipeline wrapper

Keep those outputs disposable. They are iteration artifacts, not project source.

## Usage patterns

Use the repair lane in three distinct ways:

- **Triage**: `kain doctor --repair <file> --dry-run` to see what the engine would touch
- **Inspection**: `kain doctor --repair <file> --suggest` to enumerate applied fixes without writing
- **Recovery**: `kain doctor --repair <file> --write` when the source is already known to be parser-hostile but semantically obvious

Do not use repair as a substitute for emitter fixes, importer fixes, or language design work. It is a containment tool, not a policy layer.

## Phased roadmap

### Phase 1: bounded syntax recovery

Goal: keep the lane honest and narrow.

- classify recurring blocker families into stable buckets
- keep repair profiles syntactic and traceable
- preserve one-file/one-family style transforms where possible
- report every hit as explicit evidence

### Phase 2: profile consolidation

Goal: reduce duplicated repair behavior without widening semantics.

- merge overlapping fix families where they share the same parser failure seam
- separate safe defaults from explicitly aggressive recovery
- make profile selections easy to explain from CLI output and repair reports
- keep validation against repaired copies only

### Phase 3: upstream promotion

Goal: stop depending on repair for patterns that are really generator bugs.

- move stable mechanical fixes into the emitter/importer that created them
- keep auto-repair as a temporary bridge, not the canonical solution
- delete rules that only exist to mask one-off historical drift

### Phase 4: semantic hardening

Goal: let the lane reject ambiguous repairs instead of guessing.

- require stronger evidence before applying broad transforms
- distinguish safe syntax recovery from risky semantic normalization
- add explicit deny rules for patterns that should fail fast
- preserve a human review path for anything that changes meaning

## Bottom line

The repair lane is useful because it is strict. It only stays useful if it remains boring about meaning and ruthless about structure.