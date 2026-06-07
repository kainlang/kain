# crates/semantic — Semantic Diagnostic Coprocessor

The semantic crate is Kain's data-driven error intelligence layer. It sits between the compiler's error output and the user's screen, classifying failure modes, ranking repairs, generating explanations, and suppressing noise — all without requiring a GPU or ML runtime on the hot path.

This document is guidance-first: how to work inside this crate, how to grow the error corpus, how the CUDA kernels fit, and how everything wires into the compiler.

---

## Architecture at a Glance

```
┌─────────────────────────────────────────────────────────────────────┐
│                        COMPILER (crates/core)                       │
│                                                                     │
│  parser.rs / types.rs                                               │
│    ├─ emits DiagnosticReport (code, message, span, labels)          │
│    ├─ builds DiagnosticSemanticPacket (AST context, visible symbols,│
│    │   scope matches, flags, candidate repairs)                     │
│    └─ calls enrich_report(report, packet)                           │
│              │                                                      │
│              ▼                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                    crates/semantic (this crate)                      │
│                                                                     │
│  lib.rs::analyze(packet)                                            │
│    ├─ expert.rs  — Lane A: pure-Rust rule engine (sub-ms)           │
│    │   ├─ classify_failure() — maps code + context → FailureMode    │
│    │   ├─ rank_repairs()     — scores repair candidates             │
│    │   └─ generate_explanation() — produces human-readable text     │
│    │                                                                │
│    └─ pack.rs    — Lane B: sidecar pack reranker                    │
│        ├─ loads frozen manifest.json + prototypes.bin + reranker.i8 │
│        ├─ reranks expert output with int8 dot-product scoring       │
│        └─ can load CUDA-forged v2 packs (cuda-forged-pack feature)  │
│                                                                     │
│  Output: SemanticAnalysisReport → DiagnosticSemanticSummary         │
│    ├─ failure_mode (Typo, OwnershipViolation, ShaderStageMismatch…) │
│    ├─ ranked_repairs with scores and replacement text               │
│    ├─ dynamic_explanation                                           │
│    └─ cascade_probability                                           │
│              │                                                      │
│              ▼                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                    crates/error (diagnostic engine)                  │
│                                                                     │
│  DiagnosticReport now carries .semantic_summary field               │
│  Renderers (terminal, JSON) display enriched diagnostics            │
│  Fix-its are auto-generated for typo-mode repairs                   │
└─────────────────────────────────────────────────────────────────────┘
```

### The Packet Contract

The compiler fills a `DiagnosticSemanticPacket` (defined in `crates/error/src/report.rs`) with everything it already knows at the error site:

| Field | What the compiler puts here |
|-------|---------------------------|
| `code` | The diagnostic code (e.g. `KAIN-TYPE-0002`) |
| `phase` | Which compiler phase errored (parser, typechecker, etc.) |
| `primary_text` | The offending source text at the error span |
| `source_window` | 3-5 lines of source around the error |
| `ast_node_path` | Ancestor path from root to error node |
| `visible_symbols` | Symbols in scope at the error site |
| `visible_imports` | Import paths visible at the error site |
| `nearest_scope_matches` | Spelling-close scope matches `(name, edit_distance)` |
| `contextual_flags` | Boolean flags like `in_converge_block`, `in_shader`, `in_actor` |
| `candidate_repairs` | Deterministic repairs the compiler already generated |
| `downstream_codes` | Codes of diagnostics that may be cascades |

The semantic crate never re-discovers language truth. It consumes what the compiler already extracted.

### Where enrich_report Gets Called

Four call sites in the compiler wire semantic enrichment into diagnostics:

- **`crates/core/src/parser.rs`** — `enrich_parser_report()` wraps parser errors
- **`crates/core/src/types.rs`** — `enrich_type_report()` wraps typechecker errors
- **`crates/gpu/src/codegen_spirv.rs`** — wraps SPIR-V codegen errors
- **`crates/gpu/src/codegen_ptx.rs`** — wraps PTX codegen errors

Each call site builds a packet with the compiler's context at that span, then calls `kain_semantic::enrich_report(report, &packet)`. The enriched report flows back through the normal diagnostic rendering pipeline.

---

## The Error Corpus

The error corpus at `error_corpus/` is the training data for the semantic coprocessor. Each `.kn` file is an intentionally broken piece of Kain with metadata annotations that teach the expert engine and sidecar pack how to classify, explain, and repair that class of failure.

### Fixture Format

Every corpus fixture follows this contract:

```kn
// ERROR: Human-readable summary of the intended failure
// @expected_code: KAIN-TYPE-0002
// @expected_mode: Typo
// @expected_repair: println
fn main() -> Int:
    let result = prntln("hello")
    return 0
```

**Required annotations** (top-of-file comments):

| Annotation | Purpose | Example values |
|------------|---------|---------------|
| `@expected_code` | The diagnostic code the compiler actually emits | `KAIN-TYPE-0002`, `KAIN-SHADER-0002`, `KAIN-BORROW-0004` |
| `@expected_mode` | The failure mode the expert engine should classify | `Typo`, `OwnershipViolation`, `ShaderStageMismatch`, `ConvergeMismatch`, `EntangleViolation`, `ActorMessageMismatch`, `ParserDelimiterDamage`, `MissingSurface`, `GenericUnknown` |
| `@expected_repair` | The shortest stable repair token or action id | `println`, `remove_decay`, `switch_stage`, `match_spec_lane` |

**How build.rs uses these**: At compile time, `build.rs` scans every `.kn` file in `error_corpus/`, extracts the three annotations via regex, derives `primary_text` (the first call symbol for typo/interop modes, or structural tokens like `cells`/`orchestrate`), and bakes everything into a static `ERROR_CORPUS_CASES` array. The expert engine queries this array at runtime for exact golden-case matches.

### Failure Modes

The `FailureMode` enum in `src/lib.rs` defines the full taxonomy:

| Mode | What it covers |
|------|---------------|
| `Typo` | Unknown identifier that's spelling-close to a visible symbol |
| `MissingImport` | Symbol exists in stdlib/external but isn't imported |
| `MissingSurface` | World declaration missing a required surface |
| `OwnershipViolation` | collapse/observe/decay lifecycle violation |
| `ShaderStageMismatch` | Using compute-only builtins in vertex/fragment or vice versa |
| `ShaderHostBoundary` | Calling host-only functions from inside a shader |
| `ShaderResourceContract` | Wrong uniform/storage buffer binding or type |
| `CudaKernelContract` | CUDA kernel launch parameter mismatch |
| `PythonInteropBoundary` | Python import or call boundary error |
| `CAbiBoundary` | C include/import boundary error |
| `WorldDeclarationError` | Malformed world declaration |
| `ActorMessageMismatch` | Actor message type or arity mismatch |
| `ParserDelimiterDamage` | Mismatched/unclosed delimiters |
| `ConvergeMismatch` | Type mismatch inside a converge fast lane |
| `EntangleViolation` | Type mismatch in entangle declaration |
| `GenericUnknown` | Fallback when no better classification applies |

### How to Add New Error Fixtures

#### Quick Manual Path

1. Write a `.kn` file that produces exactly one compiler error when you run `kain check` on it.

2. Run `kain check your_file.kn --target llvm` and record the actual emitted diagnostic code.

3. Add the three annotation comments at the top:

```kn
// @expected_code: KAIN-TYPE-0002
// @expected_mode: Typo
// @expected_repair: println
```

4. Verify the fixture actually fails with the expected code:

```powershell
python X:\crates\semantic\scripts\verify_error_corpus.py --changed
```

5. Rebuild the crate so `build.rs` bakes the new case into the corpus:

```powershell
cargo test -p kain-semantic test_error_corpus_cases
```

#### Batch Authoring Path (Recommended for Growth)

The batch system automates generation, verification, promotion, and baking.

**1. Create a batch spec** at `batches/<your_batch>.toml`:

```toml
[batch]
name = "my_error_batch"
prefix = "my_batch"
interview_error_family = "A"    # A=mixed, B=one deep family, C=live weak spots
interview_count = "A"           # A=24, B=12, C=30
interview_authoring = "A"       # A=hybrid, B=manual, C=automation-heavy
interview_error_system = "A"    # A=inspect first, B=corpus only, C=audit after
interview_examples = "A"        # A=cases_v2+corpus, B=user-provided, C=fresh from skills

[[cases]]
shape = "type_typo"
count = 3
start_index = 1

[[cases]]
shape = "wrong_arg_count"
count = 2
start_index = 10

[[cases]]
shape = "converge_fast_lane_drift"
count = 2
start_index = 20
```

Available shapes are defined in `templates/error_case_templates.toml`. Each shape is a template with knobs you can override.

**2. Run the batch pipeline**:

```powershell
python X:\crates\semantic\scripts\error_batch.py `
    --batch X:\crates\semantic\batches\my_error_batch.toml `
    --write-stage `
    --verify `
    --promote `
    --bake `
    --overwrite
```

The pipeline:
- `--write-stage`: generates staged `.kn` files from the batch spec + templates
- `--verify`: runs `kain check` on each fixture, records actual emitted codes
- `--rejects-duplicates`: skips fixtures that match existing corpus entries
- `--promote`: moves passing fixtures into `error_corpus/generated/<batch>/`
- `--bake`: runs `cargo test -p kain-semantic test_error_corpus_cases` to prove the cases are baked

**3. Report**: the script tells you how many cases passed, which failed, and what was promoted.

#### Authoring Style

Write fixtures like tiny pieces of believable Kain, not synthetic garbage:

- One intended primary failure per fixture
- Compact, readable snippets with one interesting mistake
- Vary the semantic setting — don't make 30 identical typos with renamed variables
- Keep comments short and useful
- Let the broken code have a little prose quality

Good fixture:
```kn
// @expected_code: KAIN-TYPE-0002
// @expected_mode: Typo
// @expected_repair: println
fn greet(name: String) -> Void:
    prntln("Hello, " + name)

fn main() -> Int:
    greet("world")
    return 0
```

Bad fixture (machine-spammed, no texture):
```kn
// @expected_code: KAIN-TYPE-0002
// @expected_mode: Typo
// @expected_repair: println
fn main() -> Int:
    let x1 = prntln("a")
    let x2 = prntln("b")
    let x3 = prntln("c")
    return 0
```

#### Adding a New Failure Family

If the failure mode you need doesn't exist in `FailureMode`:

1. Add a variant to the `FailureMode` enum in `src/lib.rs`
2. Add the `as_key()` mapping
3. Add classification logic in `src/expert.rs` → `classify_failure()`
4. Add the golden-case mapping in `failure_mode_from_golden_case()`
5. Add explanation text in `generate_explanation()`
6. Author corpus fixtures for the new family
7. Run `cargo test -p kain-semantic test_error_corpus_cases`

---

## The CUDA Kernels

The offline oracle pipeline has five Kain-authored CUDA compute shaders in `src/`. These run during the forge step, not on the compiler hot path.

### Kernel Inventory

| File | Purpose |
|------|---------|
| `search_kernel.kn` | Fused semantic score + top-k. The crown jewel. Scores chunks via warp-parallel bit-overlap and finds top-k in one kernel launch. |
| `error_kernel.kn` | Diagnostic-specialized search. Adds lane-aware prefiltering and lane/code/repair consensus reduction on top of the fused score+top-k. |
| `repair_kernel.kn` | Repair-oriented beam search. Blends embedding matches, lane overlap, policy overlap, error-code anchors, and repair-code bonuses. |
| `transformer_kernel.kn` | GPT-2-style transformer inference (4 layers, 6 heads, dim=384). Replaces hash-based embeddings with learned representations. |
| `training_kernel.kn` | Backward pass + AdamW optimizer for the transformer. Cross-entropy loss, matmul/layernorm/gelu backward, parameter updates. |

### Hybrid Architecture

The pipeline has two lanes that run at different times:

**Offline (forge time — `kain run src/main.kn -- forge`)**:
1. `indexer.kn` scans repo source files into chunks
2. `embedding.kn` produces packed u8 feature vectors per chunk
3. `search_kernel.kn` or `error_kernel.kn` runs fused score+top-k on GPU
4. `transformer_kernel.kn` optionally replaces hash embeddings with learned ones
5. `training_kernel.kn` trains the transformer on the corpus
6. Results are written as binary artifacts: `manifest.json`, `prototypes.bin`, `reranker.i8`

**Online (compiler hot path — `kain check`)**:
1. The compiler builds a `DiagnosticSemanticPacket`
2. `expert.rs` runs pure-Rust rules (Lane A) — sub-millisecond, no GPU
3. `pack.rs` optionally loads a frozen sidecar pack and reranks (Lane B)
4. Output is a `SemanticAnalysisReport` folded into the `DiagnosticReport`

The CUDA kernels never run during compilation. They forge the pack offline; the compiler reads it through a deterministic CPU int8 reranker.

### Building CUDA Artifacts

Each kernel gets its own output directory to avoid residency JSON collisions:

```powershell
cd X:\crates\semantic

kain gpu-artifacts src\search_kernel.kn --output .kain\oracle\gpu\search_kernel\search_kernel --target cuda
kain gpu-artifacts src\error_kernel.kn --output .kain\oracle\gpu\error_kernel\error_kernel --target cuda
kain gpu-artifacts src\repair_kernel.kn --output .kain\oracle\gpu\repair_kernel\repair_kernel --target cuda
kain gpu-artifacts src\transformer_kernel.kn --output .kain\oracle\gpu\transformer\transformer --target cuda
kain gpu-artifacts src\training_kernel.kn --output .kain\oracle\gpu\training\training --target cuda
```

### Pack Lanes

Control which pack the compiler loads via `KAIN_SEMANTIC_LANE`:

| Value | Behavior |
|-------|----------|
| `auto` (default) | Uses CPU pack; if `cuda-forged-pack` feature is enabled and a v2 pack exists, uses that |
| `cpu` | Always uses the default CPU-forged pack (schema v1) |
| `cuda_forged` | Loads a CUDA-forged pack (schema v2) if available |

---

## Build-Time Corpus Indexing

`build.rs` is the backbone. At `cargo build` time it:

1. **Scans** `.kn` files from four roots:
   - `symbol_corpus/` — known-good Kain symbols
   - `error_corpus/` — annotated error fixtures (this is where your new cases go)
   - `stdlib/` — Kain standard library modules
   - `smoketest/src/` — smoke test sources
   - Plus anything in `KAIN_CORPUS_PATH` env var

2. **Extracts** via regex: function names, struct/enum/actor/world/trait/shader/law/patch/converge/orchestrate/pulse/shatter declarations, import paths, include aliases, Python imports.

3. **Parses** error corpus annotations (`@expected_code`, `@expected_mode`, `@expected_repair`) and derives `primary_text`.

4. **Codegens** `$OUT_DIR/corpus_data.rs` containing:
   - `CORPUS_SYMBOLS` — static array of all known symbols with metadata
   - `CORPUS_IMPORTS` — static array of all known import paths
   - `ERROR_CORPUS_CASES` — static array of annotated error fixtures

5. Runtime code in `src/corpus_db.rs` includes this generated file and provides zero-allocation query functions:
   - `find_nearest_symbols(typo, max_results)` — Jaro-Winkler similarity search
   - `find_import_for_symbol(symbol)` — import path lookup
   - `find_error_corpus_case(code, source_window, primary_text)` — golden-case exact match

---

## Kain Source Files in src/

| File | Role |
|------|------|
| `main.kn` | Oracle forge host. Commands: `forge`, `health`, `embed`, `search`, `index` |
| `config.kn` | All configuration: paths, model params, GPU settings, rank knobs |
| `indexer.kn` | Streams repo source into packed binary index lanes |
| `embedding.kn` | Feature-hash embeddings (text → packed u8 vectors) |
| `tokenizer.kn` | Byte-level tokenizer for the transformer (text → token IDs) |
| `search_engine.kn` | CUDA search host: stages data, launches kernels, formats results |
| `search_kernel.kn` | Fused score+top-k CUDA compute shader |
| `error_kernel.kn` | Diagnostic-specialized CUDA kernel with lane/code/repair consensus |
| `repair_kernel.kn` | Repair beam search CUDA kernel |
| `transformer_kernel.kn` | GPT-2 inference CUDA kernel (encoder + 4 transformer layers + LM head) |
| `training_kernel.kn` | Backward pass + AdamW optimizer CUDA kernels |
| `serialize.kn` | Binary index reading/writing |
| `types.kn` | Shared data types (Chunk, IndexHeader, SearchResult, etc.) |
| `utils.kn` | Hash helpers, string utils, numeric formatting |
| `chunker.kn` | Source code chunking logic (split files into processable chunks) |

---

## Rust Source Files in src/

| File | Role |
|------|------|
| `lib.rs` | Public API: `SemanticCoprocessor`, `analyze()`, `enrich_report()`, `FailureMode`, `RankedRepair` |
| `expert.rs` | Lane A — pure-Rust rule engine with corpus-backed classification |
| `pack.rs` | Lane B — sidecar pack loader and int8 reranker |
| `packet.rs` | Re-exports `DiagnosticSemanticPacket` from `kain-error` |
| `corpus_db.rs` | Zero-allocation query layer over build-time baked data |

---

## Full Proof Loop

When you change anything in this crate, prove it end-to-end:

### 1. Check all Kain sources compile

```powershell
cd X:\crates\semantic

$env:TMP = 'Z:\_b\tmp'
$env:TEMP = 'Z:\_b\tmp'
$env:TMPDIR = 'Z:\_b\tmp'

kain check src\main.kn --target llvm
kain check src\search_kernel.kn --target cuda
kain check src\error_kernel.kn --target cuda
kain check src\repair_kernel.kn --target cuda
kain check src\transformer_kernel.kn --target cuda
kain check src\training_kernel.kn --target cuda
```

### 2. Build CUDA artifacts

```powershell
kain gpu-artifacts src\search_kernel.kn --output .kain\oracle\gpu\search_kernel\search_kernel --target cuda
kain gpu-artifacts src\error_kernel.kn --output .kain\oracle\gpu\error_kernel\error_kernel --target cuda
kain gpu-artifacts src\repair_kernel.kn --output .kain\oracle\gpu\repair_kernel\repair_kernel --target cuda
kain gpu-artifacts src\transformer_kernel.kn --output .kain\oracle\gpu\transformer\transformer --target cuda
kain gpu-artifacts src\training_kernel.kn --output .kain\oracle\gpu\training\training --target cuda
```

### 3. Forge the oracle

```powershell
New-Item -ItemType Directory -Force X:\crates\semantic\.kain\oracle | Out-Null
rg --files X:\crates\semantic\src X:\crates\error\src X:\crates\core\src X:\crates\check\src X:\crates\driver\src X:\crates\semantic\error_corpus X:\crates\semantic\symbol_corpus > X:\crates\semantic\.kain\oracle\source_manifest.txt
$env:KAIN_SEMANTIC_FILE_MANIFEST = 'X:\crates\semantic\.kain\oracle\source_manifest.txt'

kain run src\main.kn --target llvm -- forge
kain run src\main.kn --target llvm -- health
```

### 4. Prove search works

```powershell
kain run src\main.kn --target llvm -- search kain "unknown identifier prntln expected println" 8
```

Expected: 8 hits with `crates\semantic\error_corpus\type_unknown_identifier.kn` ranked first.

### 5. Prove Rust side compiles and corpus tests pass

```powershell
cargo test -p kain-semantic test_error_corpus_cases
cargo test -p kain-semantic sidecar_pack
```

### 6. Prove compiler integration

```powershell
kain check X:\crates\semantic\error_corpus\type_unknown_identifier.kn --target llvm
```

The output should show the enriched diagnostic with failure mode, explanation, and ranked repairs.

---

## Environment Variables Reference

| Variable | Purpose |
|----------|---------|
| `KAIN_SEMANTIC_FILE_MANIFEST` | Path to a file list for the oracle forge (avoids recursive FS walk) |
| `KAIN_SEMANTIC_PACK_PATH` | Path to a semantic sidecar pack directory |
| `KAIN_SEMANTIC_CUDA_PACK_PATH` | Path to a CUDA-forged semantic pack |
| `KAIN_SEMANTIC_LANE` | Pack lane selector: `auto`, `cpu`, `cuda_forged` |
| `KAIN_SEMANTIC_PACK_DISABLE` | Set to disable sidecar pack (expert-only mode) |
| `KAIN_SEMANTIC_PACK_STRICT` | Set to make pack loading strict (fail on missing files) |
| `KAIN_GPU_RUNTIME_LIBRARY` | Path to the GPU runtime DLL for CUDA dispatch |
| `KAIN_CORPUS_PATH` | Extra paths to scan for corpus symbols (path-separated) |

---

## Adding a New Diagnostic Code

If the compiler doesn't emit the code your fixture needs:

1. Add the code constant to `crates/error/src/code.rs`:

```rust
pub const TypeMyNewError: Self = Self::new("KAIN-TYPE-0099");
```

2. Add the spec to the appropriate TOML file in `crates/error/specs/`.

3. Emit the code from the compiler site in `crates/core/` or whichever crate owns the error.

4. Add classification logic in `crates/semantic/src/expert.rs` for the new code.

5. Author corpus fixtures using the new code.

6. Run the full proof loop above.

---

## Quick Reference: Common Tasks

| Task | Command |
|------|---------|
| Check all Kain sources | `kain check src\main.kn --target llvm` |
| Verify error corpus fixtures | `python scripts\verify_error_corpus.py --changed` |
| Run a batch | `python scripts\error_batch.py --batch batches\my_batch.toml --write-stage --verify --promote --bake` |
| Run corpus tests | `cargo test -p kain-semantic test_error_corpus_cases` |
| Run pack tests | `cargo test -p kain-semantic sidecar_pack` |
| Forge oracle | `kain run src\main.kn --target llvm -- forge` |
| Health check | `kain run src\main.kn --target llvm -- health` |
| Search test | `kain run src\main.kn --target llvm -- search kain "error message" 8` |
| Generate sidecar pack | `cargo run -p kain-semantic --features cuda-forged-pack --example write_semantic_pack` |
| Search existing corpus fixtures | `rg -n "@expected_code\|@expected_mode\|@expected_repair\|ERROR:" error_corpus` |

---

## Guardrails

- **Don't hardcode paths.** Use `config.kn` or environment variables for corpus roots, oracle paths, and GPU runtime locations.
- **Don't trust stale cached DLLs.** After runtime changes, rebuild the GPU runtime DLL and point `KAIN_GPU_RUNTIME_LIBRARY` at it.
- **Don't stop at `kain check`.** A green check doesn't prove artifact emission, runtime dispatch, or pack-backed semantic provenance. Run the full loop.
- **Don't add valid files as error fixtures.** Every annotated fixture must produce a compiler error. Fixtures that don't fail are poison.
- **Don't fake `@expected_code`.** Run `kain check` first, record the real emitted code, then adjust compiler/error-system code only when the emitted code is wrong.
- **Don't make 30 identical typos.** Vary symbols, features, phases, and repair shapes across your batch.
- **Don't let the sidecar pack accept wrong-family hits.** If `pack.rs` reranking picks a wrong-family prototype, tighten score gates or exact-code/exact-mode requirements.
