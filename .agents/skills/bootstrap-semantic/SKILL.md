---
name: bootstrap-semantic
description: >-
  Use when changing compiler, frontend, or selfhost truth for the reusable
  semantic oracle pipeline in `crates/semantic`, `crates/error-semantic`, or
  adjacent diagnostic-coprocessor wiring: offline corpus forging, manifest-fed
  indexing, tokenizer and embedding flow, CUDA search, transformer, training,
  error, or repair kernels, oracle artifact layout, and compiler-facing
  semantic enrichment. Do not use for generic authored `.kn` apps, raw
  `crates/gpu-runtime` substrate work, or non-semantic Bazel plumbing.
---

# Bootstrap Semantic

Use this skill when the semantic oracle itself is the owned truth: how corpus data is forged, how oracle artifacts are laid out, how Kain CUDA kernels are staged and consumed, and how compiler diagnostics pull signal back out.

## Trigger Surface

- `crates/semantic/**` for `src/main.kn`, `build.kn`, `config.kn`, `indexer.kn`, `search_engine.kn`, `tokenizer.kn`, `embedding.kn`, and the CUDA kernels `search_kernel.kn`, `transformer_kernel.kn`, `training_kernel.kn`, `error_kernel.kn`, and `repair_kernel.kn`.
- `crates/error-semantic/**` when compatibility glue or migration shims still touch the older error-only lane.
- `crates/error/**`, `crates/check/**`, `crates/core/**`, `crates/driver/**`, or adjacent Rust consumers when compiler diagnostics, fix-its, or semantic-search ingest depend on the oracle pack and its manifest rather than generic frontend semantics.
- `.kain/oracle/**` only when the change is about semantic artifact shape, staging, or proof outputs rather than incidental local cache noise.

## Boundaries

- Co-trigger `lang-gpu` when authoring or redesigning the Kain CUDA kernels themselves.
- Co-trigger `bootstrap-core` when parser, typechecker, diagnostics rendering, or compiler-owned semantic meaning is the dominant owner and the oracle is only a consumer.
- Co-trigger `runtime-gpu` when `crates/gpu-runtime`, CUDA DLL loading, residency dispatch, or executor substrate behavior must change.
- Co-trigger `lang-projects` when `build.kn` authority, package layout, or project-run wiring is the main deliverable instead of semantic-pipeline truth.
- Co-trigger `tool-build-system` when Bazel launchers, generated BUILD state, cargo-target routing, or repo operator plumbing dominate the work.

## Workflow

1. Treat `crates/semantic` as one pipeline. Do not patch the search kernel, transformer path, or Rust consumer in isolation without checking how forge, health, embed, and search all consume the same artifact family.
2. Keep the lane data-driven. Artifact roots, kernel stems, manifests, and seed knobs belong in `src/config.kn` or environment-driven inputs, not hardcoded paths scattered across host code.
3. Prefer manifest-driven indexing until the native recursive FS lane is fixed. The current reliable forge path reads `KAIN_SEMANTIC_FILE_MANIFEST` instead of walking directories recursively from inside the Kain binary.
4. Keep per-kernel CUDA outputs isolated. `kain gpu-artifacts` writes `kain_compute_residency.json` into the output directory, so shared output roots cause collisions and misleading proofs.
5. Prove liveness in stages: `check`, `gpu-artifacts`, `forge`, `health`, `embed`, then `search`. A green `kain check` does not prove artifact emission or runtime dispatch.
6. Read [references/proof_loop.md](./references/proof_loop.md) when you need the exact command deck, environment variables, or the currently known traps.

## Validation Loop

Use the full command set from [references/proof_loop.md](./references/proof_loop.md). The minimum expected loop is:

```powershell
cd X:\crates\semantic

$env:TMP = 'Z:\_b\tmp'
$env:TEMP = 'Z:\_b\tmp'
$env:TMPDIR = 'Z:\_b\tmp'
$env:KAIN_SEMANTIC_FILE_MANIFEST = 'X:\crates\semantic\.kain\oracle\source_manifest.txt'
$env:KAIN_GPU_RUNTIME_LIBRARY = 'Z:\_b\cargo-target\kain-semantic-gpu-runtime\debug\kain_gpu_runtime.dll'

kain check src\main.kn --target llvm
kain check src\search_kernel.kn --target cuda
kain check src\repair_kernel.kn --target cuda

kain gpu-artifacts src\search_kernel.kn --output .kain\oracle\gpu\search_kernel\search_kernel --target cuda
kain gpu-artifacts src\repair_kernel.kn --output .kain\oracle\gpu\repair_kernel\repair_kernel --target cuda

kain run src\main.kn --target llvm -- forge
kain run src\main.kn --target llvm -- health
kain run src\main.kn --target llvm -- embed kain "unknown identifier prntln expected println"
kain run src\main.kn --target llvm -- search kain "unknown identifier prntln expected println" 8
```

The expected search proof is `8` hits with `crates\semantic\error_corpus\type_unknown_identifier.kn` ranked first. If compiler-facing behavior changed, also run the broken-corpus checks from the reference file.

## Guardrails

- Do not hardcode corpus roots, oracle file paths, or GPU runtime DLL locations in Rust or Kain source when `src/config.kn` or env-driven config can own them.
- Do not trust `X:\.kain\cache\run\llvm\kain_gpu_runtime.dll` after runtime changes; the semantic pipeline already proved that a stale cached DLL can reject the current residency target.
- Do not collapse the semantic lane back into error-only assumptions. The point of `crates/semantic` is to stay reusable across diagnostics and future compiler intelligence surfaces.
- Do not stop at `kain check` when the user asks whether the pipeline works. Real proof here means forge artifacts plus at least one runtime-visible command (`health`, `embed`, `search`, or broken-corpus diagnostics).
- Do not treat the fused CUDA score+top-k lane as the default proof path yet. The current reliable path is CUDA score dispatch plus host top-k metadata reranking; fused and CUDA top-k lanes are env-gated experiments.
