# Semantic Oracle Proof Loop

Use this reference when you need the exact command deck for the current `crates/semantic` pipeline.

## Scope

- Working directory: `X:\crates\semantic`
- Preferred binary: use the repo `kain` launcher if it is fresh; if launcher behavior looks stale, swap in the current Bazel-built `kain.exe`.
- Pipeline owner: `crates/semantic`

## Environment

Set these before `kain run` proofs so Clang and MSVC stay off the crowded `F:` temp path:

```powershell
$env:TMP = 'Z:\_b\tmp'
$env:TEMP = 'Z:\_b\tmp'
$env:TMPDIR = 'Z:\_b\tmp'
```

Generate a manifest outside the Kain binary and point the oracle at it:

```powershell
New-Item -ItemType Directory -Force X:\crates\semantic\.kain\oracle | Out-Null
rg --files X:\crates\semantic\src X:\crates\error\src X:\crates\core\src X:\crates\check\src X:\crates\driver\src X:\crates\semantic\error_corpus X:\crates\semantic\symbol_corpus > X:\crates\semantic\.kain\oracle\source_manifest.txt
$env:KAIN_SEMANTIC_FILE_MANIFEST = 'X:\crates\semantic\.kain\oracle\source_manifest.txt'
```

## Check Loop

```powershell
cd X:\crates\semantic

kain check src\main.kn --target llvm
kain check src\tokenizer.kn --target llvm
kain check src\search_kernel.kn --target cuda
kain check src\transformer_kernel.kn --target cuda
kain check src\training_kernel.kn --target cuda
kain check src\error_kernel.kn --target cuda
kain check src\repair_kernel.kn --target cuda
kain check build.kn --target llvm
```

## CUDA Artifact Loop

Keep each kernel in its own output directory because `gpu-artifacts` writes `kain_compute_residency.json` into the output directory.

```powershell
kain gpu-artifacts src\search_kernel.kn --output .kain\oracle\gpu\search_kernel\search_kernel --target cuda
kain gpu-artifacts src\transformer_kernel.kn --output .kain\oracle\gpu\transformer\transformer --target cuda
kain gpu-artifacts src\training_kernel.kn --output .kain\oracle\gpu\training\training --target cuda
kain gpu-artifacts src\error_kernel.kn --output .kain\oracle\gpu\error_kernel\error_kernel --target cuda
kain gpu-artifacts src\repair_kernel.kn --output .kain\oracle\gpu\repair_kernel\repair_kernel --target cuda
```

## Oracle Commands

```powershell
kain run src\main.kn --target llvm -- forge
kain run src\main.kn --target llvm -- health
kain run src\main.kn --target llvm -- health-json
kain run src\main.kn --target llvm -- embed kain "unknown identifier prntln expected println"
```

Expected proof from the last known good run:

- `forge` produced `2372` code chunks and `640` Kain chunks.
- The oracle wrote `.kain\oracle\kain_error_oracle.bin`.
- `health` confirmed pack, index, matrix, and all CUDA artifact families.
- `embed` returned a transformer-enabled 384-byte preview.
- `search kain "unknown identifier prntln expected println" 8` returned `8` hits and ranked `crates\semantic\error_corpus\type_unknown_identifier.kn` first.

## Search Dispatch

Build a fresh GPU runtime DLL when runtime dispatch changes or the cached run DLL looks stale:

```powershell
$env:CARGO_TARGET_DIR = 'Z:\_b\cargo-target\kain-semantic-gpu-runtime'
cargo build -p kain-gpu-runtime
$env:KAIN_GPU_RUNTIME_LIBRARY = 'Z:\_b\cargo-target\kain-semantic-gpu-runtime\debug\kain_gpu_runtime.dll'
```

Then run:

```powershell
Remove-Item Env:\KAIN_SEMANTIC_FUSED_RANK_ENABLED -ErrorAction SilentlyContinue
Remove-Item Env:\KAIN_SEMANTIC_CUDA_TOPK_ENABLED -ErrorAction SilentlyContinue
Remove-Item Env:\KAIN_SEMANTIC_QUERY_TRANSFORMER_SEED_MASK -ErrorAction SilentlyContinue
kain run src\main.kn --target llvm -- search kain "unknown identifier prntln expected println" 8
```

Current known-good behavior:

- The CUDA score kernel stages and dispatches successfully.
- The default rank path reads the CUDA score payload and performs host top-k reranking with metadata bonuses.
- The probe query returns `8` hits and ranks `type_unknown_identifier.kn` first.
- The fused score+top-k kernel and the CUDA top-k kernel are still available, but opt-in and experimental: set `KAIN_SEMANTIC_FUSED_RANK_ENABLED=1` and/or `KAIN_SEMANTIC_CUDA_TOPK_ENABLED=1` only when validating those lanes directly.

## Sidecar Pack / Compiler Consumer

The shipped compiler path is CPU-only and data-driven. Generate a frozen sidecar pack from the baked corpus, then point `kain check` at it:

```powershell
$env:TMP = 'Z:\_b\tmp'
$env:TEMP = 'Z:\_b\tmp'
$env:TMPDIR = 'Z:\_b\tmp'
$env:CARGO_BUILD_JOBS = '1'
cargo run -p kain-semantic --example write_semantic_pack --target-dir Z:\_b\cargo-target\semantic-hybrid -- X:\crates\semantic\.kain\oracle\sempack\current

$env:KAIN_SEMANTIC_PACK_PATH = 'X:\crates\semantic\.kain\oracle\sempack\current'
Remove-Item Env:\KAIN_SEMANTIC_PACK_DISABLE -ErrorAction SilentlyContinue
kain check X:\crates\semantic\error_corpus\type_unknown_identifier.kn --target llvm --json X:\crates\semantic\.kain\reports\semantic-pack-type.json
```

Expected structured diagnostic proof from `semantic-pack-type.json`:

- `files[0].diagnostic.diagnostics[0].semantic.backend == "pack_cpu_rerank"`
- `pack_schema_version == "kain.semantic.pack.v1:1"`
- `failure_mode == "typo"`
- top repair replacement text is `println`

Fallback proof:

```powershell
$env:KAIN_SEMANTIC_PACK_DISABLE = '1'
kain check X:\crates\semantic\error_corpus\type_unknown_identifier.kn --target llvm --json X:\crates\semantic\.kain\reports\semantic-pack-disabled-type.json
```

Expected fallback structured diagnostic proof:

- `semantic.backend == "fallback_rules"`
- `pack_schema_version == null`
- user-facing explanation and fix-it still point `prntln -> println`

## Ranking Knobs

The search ranker is intentionally data-driven. Prefer these config/env knobs over hardcoded scoring tweaks:

- `KAIN_SEMANTIC_FUSED_RANK_ENABLED`: enable the experimental fused CUDA score+top-k lane.
- `KAIN_SEMANTIC_CUDA_TOPK_ENABLED`: enable the experimental CUDA top-k lane after the score kernel.
- `KAIN_SEMANTIC_QUERY_LEXICAL_BLEND`: keep lexical bitpack signal blended with transformer seed output.
- `KAIN_SEMANTIC_QUERY_TRANSFORMER_SEED_MASK`: mask transformer seed bits before blending; default `0` keeps untrained transformer noise from scrambling lexical exact matches.
- `KAIN_SEMANTIC_RANK_POPCOUNT_SCALE`: weight bit-overlap popcount in CUDA scores.
- `KAIN_SEMANTIC_RANK_BITS_PER_BYTE`: expected max per byte for score normalization.
- `KAIN_SEMANTIC_RANK_EXACT_BONUS`: bonus for exact byte matches between query and chunk embeddings.
- `KAIN_SEMANTIC_RANK_ERROR_CORPUS_BIAS`: corpus prior for known broken/error examples.
- `KAIN_SEMANTIC_RANK_META_BONUS`: enable path/symbol/kind token reranking.
- `KAIN_SEMANTIC_RANK_PATH_TOKEN_BONUS`: bonus when query tokens appear in chunk paths.
- `KAIN_SEMANTIC_RANK_SYMBOL_TOKEN_BONUS`: bonus when query tokens appear in symbol names.
- `KAIN_SEMANTIC_RANK_KIND_TOKEN_BONUS`: bonus when query tokens appear in chunk kinds.
- `KAIN_SEMANTIC_PACK_PATH`: CPU compiler-side sidecar pack root containing `manifest.json`, `prototypes.bin`, and `reranker.i8`.
- `KAIN_SEMANTIC_PACK_DISABLE`: force the compiler semantic coprocessor back to the fallback rules path.

## Broken Corpus Proof

Run the real compiler checks directly against the broken examples:

```powershell
kain check X:\crates\semantic\error_corpus\type_unknown_identifier.kn --target llvm
kain check X:\crates\semantic\error_corpus\parse_missing_colon.kn --target llvm
kain check X:\crates\semantic\error_corpus\world_missing_surface.kn --target llvm
```

The current proof expectation is enriched diagnostics, notes, and fix-its rather than a successful compile.

## Known Traps

- `kain check` can pass while `kain gpu-artifacts` fails. PTX artifact emission is stricter than a frontend-only compile.
- Run `kain` launcher commands serially on Windows. Parallel checks/runs can race the shared stamp replacement path before Kain itself starts.
- The native recursive filesystem path crashed during forge; stay on the manifest-fed path until `fs_read_dir_paths_text` is fixed on Windows LLVM-native runs.
- Kain-owned process launch for `rg` inside the oracle returned `process_last_status() == -5`; do not rely on the oracle self-spawning the manifest scanner right now.
- `X:\.kain\cache\run\llvm\kain_gpu_runtime.dll` was stale and rejected residency target `ks`; prefer the freshly rebuilt DLL under `Z:\_b\cargo-target\kain-semantic-gpu-runtime`.
- Some small authored Kain helper shapes passed `kain check` but failed during LLVM run with invalid PHI IR. In this lane, avoid empty `return` in `Unit` helpers, avoid inline `if` expressions for values that can be spelled as explicit `var` assignments, and prefer `text_tokenize_whitespace` plus direct streaming over accumulating `Array<String>` tokens in tiny helpers.
- Error corpus typo extraction must skip declarations like `fn main()`. If it treats the declaration name as the primary bad symbol, the sidecar pack can learn a bogus prototype. Keep corpus files annotated with `@expected_code`, `@expected_mode`, and `@expected_repair` when the broken code should become pack truth.
