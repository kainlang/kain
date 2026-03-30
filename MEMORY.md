# MEMORY.md

## 2026-03-29 - selfhost auto-repair docs added

Documented the new Kain self-host auto-repair lane so future agents can find the architecture, doctor-facing usage, and the safe-vs-dangerous guardrails in one place.

What changed:

- Added `docs/pipeline/SELFHOST_REPAIR.md` with:
  - architecture notes for `crates/kain-selfhost`
  - repair lane flow and report locations
  - doctor-first usage guidance
  - guardrails for syntax repair vs semantic rewriting
  - phased roadmap for the feature
- Linked the repair doc from:
  - `docs/pipeline/README.md`
  - root `README.md`
  - `ARCHITECTURE.md`

Notes:

- The docs deliberately frame repair as a bounded copy/validate loop, not a source-mutating rewrite system.
- `kain doctor` is treated as the provenance check before entering the repair lane.

## 2026-03-29 - kain-repair foundation introduced

Created a new workspace crate, `crates/kain-repair`, as the first dedicated repair lane for parser-hostile Kain source.

What landed:

- New public API shape:
  - `RepairInput`
  - `RepairProfile`
  - `RepairMode` (`Check`, `Suggest`, `ApplySafe`, `ApplyAggressive`)
  - `FixKind`
  - `AppliedFix`
  - `RepairResult`
  - `repair_text`
  - `repair_text_with_input`
  - `suggest_fixes`
- Deterministic repair passes implemented in the first cut:
  - normalize CRLF/CR to LF
  - trim trailing spaces and tabs
  - collapse excessive blank-line runs
  - ensure a final newline
  - append a block-comment closer when unterminated comments are detected

Notes / constraints:

- The crate is intentionally conservative and source-text only for now; it does not depend on `kain-core` yet.
- The workspace root now includes `crates/kain-repair` as a member.
- No CLI wiring yet; this crate is meant to become the repair engine consumed by `doctor` later.

Recommended next step:

- Wire this crate into `doctor`/CLI diagnostics once the API settles, and add parser-aware repair heuristics only where they remain deterministic and safe.


## 2026-03-29 - kain-repair foundation introduced

Created a new workspace crate, `crates/kain-repair`, as the first dedicated repair lane for parser-hostile Kain source.

What landed:

- New public API shape:
  - `RepairInput`
  - `RepairProfile`
  - `RepairMode` (`Check`, `Suggest`, `ApplySafe`, `ApplyAggressive`)
  - `FixKind`
  - `AppliedFix`
  - `RepairResult`
  - `repair_text`
  - `repair_text_with_input`
  - `suggest_fixes`
- Deterministic repair passes implemented in the first cut:
  - normalize CRLF/CR to LF
  - trim trailing spaces and tabs
  - collapse excessive blank-line runs
  - ensure a final newline
  - append a block-comment closer when unterminated comments are detected

Notes / constraints:

- The crate is intentionally conservative and source-text only for now; it does not depend on `kain-core` yet.
- The workspace root now includes `crates/kain-repair` as a member.
- No CLI wiring yet; this crate is meant to become the repair engine consumed by `doctor` later.

Recommended next step:

- Wire this crate into `doctor`/CLI diagnostics once the API settles, and add parser-aware repair heuristics only where they remain deterministic and safe.
