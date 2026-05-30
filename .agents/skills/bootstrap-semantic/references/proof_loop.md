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

- `forge` produced `2372` code chunks and `635` Kain chunks.
- The oracle wrote `.kain\oracle\kain_error_oracle.bin`.
- `health` confirmed pack, index, matrix, and all CUDA artifact families.
- `embed` returned a transformer-enabled 384-byte preview.

## Search Dispatch

Build a fresh GPU runtime DLL when runtime dispatch changes or the cached run DLL looks stale:

```powershell
$env:CARGO_TARGET_DIR = 'Z:\_b\cargo-target\kain-semantic-gpu-runtime'
cargo build -p kain-gpu-runtime
$env:KAIN_GPU_RUNTIME_LIBRARY = 'Z:\_b\cargo-target\kain-semantic-gpu-runtime\debug\kain_gpu_runtime.dll'
```

Then run:

```powershell
kain run src\main.kn --target llvm -- search
```

Current known-good behavior:

- The fused CUDA search kernel stages and dispatches successfully.
- The last proof still returned `0` hits for the probe query, so ranking quality needs tuning even though the pipeline is live.

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
- The native recursive filesystem path crashed during forge; stay on the manifest-fed path until `fs_read_dir_paths_text` is fixed on Windows LLVM-native runs.
- Kain-owned process launch for `rg` inside the oracle returned `process_last_status() == -5`; do not rely on the oracle self-spawning the manifest scanner right now.
- `X:\.kain\cache\run\llvm\kain_gpu_runtime.dll` was stale and rejected residency target `ks`; prefer the freshly rebuilt DLL under `Z:\_b\cargo-target\kain-semantic-gpu-runtime`.
