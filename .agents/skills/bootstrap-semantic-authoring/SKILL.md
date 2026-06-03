---
name: bootstrap-semantic-authoring
description: >-
  Use when authoring, expanding, validating, or repairing Kain semantic error
  corpus fixtures under `crates/semantic/error_corpus`, including batches of
  intentionally broken `.kn` files, expected diagnostic metadata,
  semantic-pack prototype coverage, helper scripts in `crates/semantic/scripts`,
  and follow-on changes to `crates/semantic`, `crates/error`,
  `crates/core`, or diagnostic coprocessor wiring needed to make the new
  corpus cases meaningful. Co-use `bootstrap-semantic` for oracle/pipeline
  changes and `lang-semantics` or `lang-systems` when writing Kain fixtures.
---

# Bootstrap Semantic Authoring

Use this skill to grow `X:\crates\semantic\error_corpus` as the cheapest high-leverage way to improve Kain diagnostics. The corpus is not a pile of random broken files: `crates/semantic/build.rs` scans annotated fixtures, bakes them into `corpus_db`, and `crates/semantic/src/expert.rs` plus `pack.rs` use them to classify failure modes, rank repairs, preserve `primary_text`/`source_window`, and enrich compiler reports.

## Co-Trigger Order

1. Use `lang-semantics` for general Kain syntax, semantic constructs, and source anchors.
2. Use `lang-systems` when fixtures involve actors, effects, ownership, raw memory, machine lanes, or unsafe code.
3. Use `lang-gpu`, `lang-python`, or `lang-c-abi` when the batch covers shaders/CUDA, Python interop, or native/import boundaries.
4. Use `bootstrap-semantic` last when the batch changes the oracle, semantic pack, failure taxonomy, expert rules, generated corpus DB, CUDA kernels, or compiler-facing diagnostic behavior.

Useful repo-local examples:

- `crates/semantic/error_corpus/*.kn`: the fixture format and metadata contract.
- `benchmark/cases_v2/*.kn`: golden authored Kain packs for fresh semantic shapes; mine these for realistic feature pressure before intentionally breaking them.
- `library_of_kain/semantics.kn` and `library_of_kain/actor_ownership_backpressure.kn`: compact semantic and systems vocabulary.

## Required Authoring Interview

Before creating or editing corpus fixtures, interview the user for clarity unless they already gave explicit answers in the prompt. Use A/B/C choices so the user can answer quickly. Keep the recommended path first, but tailor labels to the requested corpus lane.

Ask these five questions:

1. Which error family should this batch focus on?
   - A. `Mixed semantic batch`: covers parser/type, semantic surfaces, systems, and interop/GPU so the pack learns broad shape.
   - B. `One deep family`: focuses on one owner such as ownership, actors, shader contracts, Python, C ABI, or world/entangle.
   - C. `Live weak spots`: inspect current diagnostics first, then target undercovered or generic-error areas.
2. How many error fixtures should I author?
   - A. `24 fixtures`: default corpus-growth batch, enough for several families without making review painful.
   - B. `12 fixtures`: focused patch for one family or a quick validation pass.
   - C. `30 fixtures`: max normal batch when automation and broad coverage are desired.
3. Should I automate generation or write manually?
   - A. `Hybrid`: fill a batch spec for `error_batch.py`, let the script scaffold and verify, then hand-tune only the tricky cases.
   - B. `Manual`: write every fixture by hand for maximum semantic intent and fewer template artifacts.
   - C. `Automation-heavy`: extend the shape templates or helper scripts first, then generate most of the batch.
4. Should I inspect and possibly extend the current error system too?
   - A. `Yes, inspect first`: check emitted codes, expert rules, pack behavior, and update error-system wiring if needed.
   - B. `Corpus only`: add fixtures that match existing emitted diagnostic codes and modes.
   - C. `Audit after`: author fixtures first, then review failures to decide whether `crates/error`, `crates/core`, or `crates/semantic` needs code.
5. Where should I draw examples from?
   - A. `cases_v2 + existing corpus`: mine `benchmark/cases_v2` for realistic Kain, then break one thing per fixture.
   - B. `Specific user-provided examples`: use paths/snippets the user supplies as donor shapes.
   - C. `Fresh Kain from skills`: use `lang-semantics`/`lang-systems` first and invent new donor shapes with minimal scavenging.

After the user answers, restate the chosen batch shape in one sentence and execute. If the user gives no answer but explicitly asks for immediate execution, choose A/A/A/A/A and say that assumption before proceeding.

## Batch Target

Author 20-30 new fixtures per run unless the user asks for a smaller surgical patch. Spread them across at least 3 failure families so the pack learns shape, not just string variants.

Good batch mix:

- 5-8 parser/type basics: missing delimiters, reserved identifiers, unknown symbols, wrong arity, return/type mismatches.
- 4-6 semantic surfaces: `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `pulse`, `teleport`.
- 4-6 systems surfaces: effects, actors, ownership, raw memory, async/await, unsafe boundaries.
- 4-6 interop/GPU surfaces when relevant: shader resource contracts, CUDA/PTX constraints, Python imports, C ABI includes.

Avoid only cloning the same typo. Keep each case short enough to localize the primary error, but rich enough that semantic recovery has a real clue.

## Fixture Contract

Every corpus fixture should be an intentionally broken `.kn` file with top comments like:

```kn
// ERROR: Human summary of the intended failure
// @expected_code: KAIN-TYPE-0002
// @expected_mode: Typo
// @expected_repair: println
fn main() -> Int:
    let value = prntln("hello")
    return 0
```

Required:

- `@expected_code`: canonical diagnostic code, normally from `crates/error/src/code.rs` or the live CLI output.
- `@expected_mode`: semantic failure mode expected by `expert.rs`, such as `Typo`, `OwnershipViolation`, `EntangleViolation`, `ConvergeMismatch`, `ShaderHostBoundary`, `ShaderResourceContract`, `CudaKernelContract`, `PythonInteropBoundary`, `CAbiBoundary`, `ActorMessageMismatch`, `ParserDelimiterDamage`, `MissingSurface`, or `GenericUnknown`.
- `@expected_repair`: shortest stable repair token or action. Use identifiers for typo/import cases, and action ids like `remove_decay`, `add_surface`, `fix_binding`, or `remove_host_call` for structural cases.

The build script derives:

- `source_window`: the whole file text.
- `primary_text`: first bad call symbol for typo/interoperability modes, special structural tokens like `cells`, `Master.val`, or `orchestrate`, otherwise the expected repair text.

If a new family needs different `primary_text` derivation or failure classification, update `crates/semantic/build.rs`, `crates/semantic/src/expert.rs`, and possibly `crates/semantic/src/pack.rs`; then validate with `cargo test -p kain-semantic test_error_corpus_cases`.

## Authoring Flow

1. Read `GLOSSARY.MD`, `CATALOG.MD`, and search `MEMORY.md` for the specific failure family.
2. Inspect existing fixtures with:

```powershell
rg -n "@expected_code|@expected_mode|@expected_repair|ERROR:" X:\crates\semantic\error_corpus
```

3. Generate or handwrite a batch. The spec-driven batch flow is the default because it is safer for cheap models:

```powershell
python X:\crates\semantic\scripts\error_batch.py --batch X:\crates\semantic\batches\example_error_batch.toml --write-stage --verify
```

4. Prefer editing the batch spec and reusable templates before hand-editing raw `.kn` files. If hand edits are needed, keep each case to one intended primary failure and prefer donor shapes from `benchmark/cases_v2` or a real compiler sharp edge.
5. Verify the files are actually errors and that expected codes show up:

```powershell
python X:\crates\semantic\scripts\verify_error_corpus.py --changed
```

6. Prove the baked semantic pipeline:

```powershell
cargo test -p kain-semantic test_error_corpus_cases
cargo test -p kain-semantic sidecar_pack
```

7. If corpus growth changes compiler-facing behavior, run a focused CLI proof:

```powershell
kain check X:\crates\semantic\error_corpus\<fixture>.kn --target llvm
```

8. If the oracle index or sidecar pack must see the new cases, use `bootstrap-semantic` and run the semantic forge/search proof loop from that skill.

## Automation Surface

Agents may create or extend Python scripts under `X:\crates\semantic\scripts` whenever automation makes corpus growth faster or safer. Keep scripts path-configurable, avoid hardcoded machine-only output paths when an argument or environment value can own them, and make them usable from repo root and `crates/semantic`.

Current scripts:

- `error_batch.py`: canonical spec-driven lane. It loads trusted shapes from `crates/semantic/templates/error_case_templates.toml`, requires stored A/B/C interview answers in the batch spec, generates staged fixtures, verifies live `kain check` output, rewrites metadata with emitted codes/modes, rejects duplicates, optionally promotes only passing cases into `error_corpus/generated/<batch>`, and can bake the semantic cargo/Bazel gates.
- `generate_error_corpus.py`: older lightweight scaffold for manual authoring. Keep it for quick sketches, but prefer `error_batch.py` for real corpus growth and for weaker models.
- `verify_error_corpus.py`: parse corpus metadata, run `kain check` against annotated fixtures, report pass/fail, and optionally run semantic Rust tests to prove the cases are baked.

Batch-spec contract:

- Store interview choices under `[batch]` as `interview_error_family`, `interview_count`, `interview_authoring`, `interview_error_system`, and `interview_examples`.
- Use only `A`, `B`, or `C` so future agents can recover why the batch was authored.
- Keep cheap models working in `crates/semantic/batches/*.toml` plus `crates/semantic/templates/error_case_templates.toml` whenever possible; only graduate them to hand-editing staged `.kn` files when the template lane is too weak.

## Error-System Wiring

Corpus-only changes are enough when the compiler already emits the expected code and `expert.rs` already maps the failure family. Add code when any of these are true:

- The CLI emits no code, the wrong code, or a generic code where a stable family code exists: update `crates/error/src/code.rs`, `builder.rs`, registry/explain data, and the compiler site in `crates/core` or the relevant owner.
- `expert.rs` classifies the packet as the wrong failure mode: update family detection, packet flags, repair ranking, and tests.
- `pack.rs` reranking picks a wrong-family prototype: tighten score gates, exact-code/exact-mode requirements, or symbol-family checks.
- The fixture needs different generated metadata: update `build.rs` derivation and prove `test_error_corpus_cases`.
- CUDA/oracle forge/search behavior changes: co-use `bootstrap-semantic` and run the full oracle proof loop.

## Guardrails

- Do not add compile-valid files as error fixtures unless they are explicitly runner/support files and unannotated.
- Do not leave annotated files unverified. A fixture that does not fail is poison.
- Do not fake `@expected_code`; first run `kain check` and record the real emitted code, then adjust compiler/error-system code only when the emitted code is wrong.
- Do not let one generated template produce 30 identical typo cases. Change symbols, features, phases, and repair shapes.
- Do not stop at file count. The win is richer diagnostics, semantic pack coverage, and lower-effort future repair intelligence.
