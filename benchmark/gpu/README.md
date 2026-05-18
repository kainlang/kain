# GPU Benchmark Lane

This lane is intentionally separate from `benchmark/cases` and `benchmark/run.py`.
General benchmark rows measure language/runtime pressure. This folder measures
shader and GPU pipeline artifacts: SPIR-V density, Vulkan validation, dispatcher
wall time, readback correctness, and optional hardware telemetry emitted by the
dispatchers.

Run it from the repo root:

```powershell
python benchmark/run_gpu.py --list
python benchmark/run_gpu.py --case vec3_storage_copy --languages kain,cpp --runs 3 --warmups 1
python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --runs 3 --warmups 1
python benchmark/run_spirv.py --case vec3_storage_copy --languages kain,cpp --runs 3 --warmups 1
```

Reports are written to:

- `benchmark/latest_gpu.md`
- `benchmark/out/reports/latest_gpu.llm.md`
- `benchmark/out/reports/latest_gpu.json`
- timestamped `benchmark/out/reports/<stamp>.gpu.llm.md`
- timestamped `benchmark/out/reports/<stamp>.gpu.json`

## Contract

Each case lives under `benchmark/gpu/cases/<case_id>/` and is declared in
`benchmark/gpu/gpu_cases.json`.

Python owns orchestration and bytecode-level profiling. Dispatchers own device
truth. A dispatcher can emit:

```text
benchmark/out/build/gpu/<case_id>/<language>/<language>.telemetry.json
```

The runner also sets these environment variables for dispatchers:

- `KAIN_GPU_CASE_ID`
- `KAIN_GPU_LANGUAGE`
- `KAIN_GPU_SHADER_SPV`
- `KAIN_GPU_ENTRY_POINT`
- `KAIN_GPU_WORK_ITEMS`
- `KAIN_GPU_WIDTH`
- `KAIN_GPU_TELEMETRY_PATH`

Cases and per-language entries may also provide a `runner_env` map in
`gpu_cases.json`. The runner stringifies and merges those values into the
dispatcher environment after the common `KAIN_GPU_*` keys. This is the clean
hook for round counts, tolerances, gains, or other case-local host knobs.

Telemetry JSON is intentionally loose so C++, Rust, and Kain hosts can evolve
without Python parsing Vulkan internals. The report recognizes keys such as
`mismatch_count`, `checksum`, `register_count`, `binary_size`, `vgpr_count`,
`sgpr_count`, `spill_count`, `spills`, `duration_ns`, `execution_duration_ns`,
`rounds`, and `max_abs_error`, while preserving the whole sidecar in JSON.

## Static SPIR-V Profiling

If `spirv-dis` is available, the runner uses disassembly lines and opcode text.
If it is not available, the runner still parses the binary SPIR-V word stream
and counts instructions and selected opcode ids. If `spirv-val` is available,
the lane validates generated modules against `vulkan1.3`.

## First Runtime Row

`vec3_storage_copy` compares a Kain compute shader against a GLSL/C++ reference
shader. Both artifacts execute through the same C++ Vulkan dispatcher, descriptor
layout, host-visible buffers, timestamp query, readback verifier, and telemetry
sidecar. This keeps the comparison focused on generated SPIR-V while proving the
runtime result with matching checksums and zero mismatches.

## Golden Runtime Row

`semantic_ping_pong` is the first "golden SPIR-V" showcase row. It uses a much
richer compute shader with nested branches, loops, trig-heavy vector math, and
a 12-round ping-pong rebound schedule across three storage buffers. The same
C++ Vulkan host runs both the Kain SPIR-V and the GLSL/C++ reference SPIR-V,
then checks the final state against a CPU oracle and emits rounds, max error,
register count, binary size, and accumulated GPU timestamp duration.
