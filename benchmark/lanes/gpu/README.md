# GPU Benchmark Lane

This lane stays separate from main multi-language rows because it validates shader/GPU truth (SPIR-V density, validation, dispatcher timing, telemetry sidecars).

## Run

```powershell
python benchmark/bench.py suite gpu -- --list
python benchmark/bench.py suite gpu -- --case vec3_storage_copy --languages kain,cpp --runs 3 --warmups 1
python benchmark/bench.py suite gpu -- --case semantic_ping_pong --languages kain,cpp --runs 3 --warmups 1
python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --runs 3 --warmups 1
python benchmark/run_spirv.py --case vec3_storage_copy --languages kain,cpp --runs 3 --warmups 1
python benchmark/lanes/gpu/run_cuda.py --list
python benchmark/lanes/gpu/run_cuda.py --case cuda_warp_reduce_sum --runs 3 --warmups 1
```

## Reports

- `benchmark/out/snapshots/latest_gpu.md`
- `benchmark/out/reports/latest_gpu.llm.md`
- `benchmark/out/reports/latest_gpu.json`
- timestamped `benchmark/out/reports/<stamp>.gpu.llm.md`
- timestamped `benchmark/out/reports/<stamp>.gpu.json`
- CUDA/PTX gauntlet: `benchmark/out/reports/latest_cuda_gpu.json`
- CUDA/PTX gauntlet: `benchmark/out/reports/latest_cuda_gpu.llm.md`

## Contract

- Cases live under `benchmark/lanes/gpu/cases/<case_id>/`.
- Catalog file: `benchmark/lanes/gpu/gpu_cases.json`.
- Optional hardware sidecar path:

```text
benchmark/out/build/gpu/<case_id>/<language>/<language>.telemetry.json
```

Common dispatcher env vars:

- `KAIN_GPU_CASE_ID`
- `KAIN_GPU_LANGUAGE`
- `KAIN_GPU_SHADER_SPV`
- `KAIN_GPU_ENTRY_POINT`
- `KAIN_GPU_WORK_ITEMS`
- `KAIN_GPU_WIDTH`
- `KAIN_GPU_TELEMETRY_PATH`

## CUDA/PTX Gauntlet

`cuda_cases.json` is the CUDA-native sibling to the Vulkan/SPIR-V lane. It uses
`kain build <shader> --target cuda`, reads the emitted build report, and copies
the PTX into `benchmark/out/build/gpu-cuda/<case>/kain/` for artifact-density
checks. The first cases cover warp reduction, packed u8 embedding gather, and an
attention-score/GEMV tile; add GEMM, softmax, layernorm, RoPE, KV-cache, top-k,
and reduce-max rows here as their Kain PTX lowering becomes real.
