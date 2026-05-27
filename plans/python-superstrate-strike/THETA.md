# THETA

## Lane

GPU / DLPack / device-memory handoff.

## Mission

Make Kain the clean router between Python tensor ecosystems and Kain GPU execution.

This lane is where Python ML frameworks stop being "host libraries we call" and start becoming tensor/device capability providers inside Kain.

## Owns

- `crates/interop/`
- `crates/gpu-runtime/`
- `crates/gpu/`
- `runtime/native/src/core/python_runtime_gpu.c`
- `runtime/native/src/core/python_runtime.c` only for cross-seam glue
- `stdlib/interop.kn`
- `stdlib/gpu.kn`
- GPU-interop proofs, demos, and focused benchmarks

## Do Not Own

- baseline region tuning
- generic async scheduler policy
- non-GPU shared-buffer default policy except where the contract overlaps device ownership

## Deliverables

1. Define the first truthful GPU-facing Python contract:
   - `python_shared_buffer_gpu(...)` or equivalent
   - device / host / shape / dtype / stride truth
2. Add a DLPack import/export lane where feasible.
3. Allow a Python tensor/buffer to become a Kain GPU binding without an intermediate host-owned byte clone when the underlying API supports it.
4. If full device-memory export is too broad for pass one, still land the contract and one working CPU->GPU fast path that preserves the zero-copy shape as far as the hardware/API allow.

## Design Direction

- DLPack and the CUDA Array Interface are the right mental model.
- Do not invent a fake "GPU zero-copy" claim if the hardware/API path still copies.
- Truthful metadata beats hype.
- Host-visible staging copies are acceptable only when honestly marked as such.

## Proof Obligations

- descriptor size / binding range math
- device/host ownership state transitions
- release ordering when Python and GPU handles both reference the same underlying resource
- any offset/stride shape calculations used for imported tensors

## Benchmark Duties

At minimum add:

- Python tensor -> Kain GPU binding setup cost
- buffer readback or checksum verification row
- one comparison row versus the older copy-shaped path if it exists

## Smoke Target

One authored Kain demo that:

- imports a Python tensor source
- hands it into a Kain GPU-facing contract
- runs a tiny shader/compute/binding flow
- verifies the result on the Kain side

Stretch goal:

- export a Kain-owned GPU result back into a Python tensor view without a redundant host copy

## Exit Criteria

- one real Python-to-Kain GPU handoff path works
- metadata is truthful
- benchmark row exists
- smoke demo exits `0`

## 2026-05-27 Pass Status

### Files touched

- `runtime/native/src/core/python_runtime.c`
- `runtime/native/src/core/python_runtime_async.c`
- `runtime/native/src/core/z3/proofs/native-python-tensor-byte-length-checked-mul-stays-in-int64-range.yaml`
- `stdlib/interop.kn`
- `stdlib/gpu.kn`
- `stdlib/python.kn`
- `benchmark/cases_v2/python_interop.kn`

### New public surfaces

- `std::python::python_tensor_interop_info(target)`
- `std::python::python_shared_buffer_gpu(target, policy)`
- `std::python::python_gpu_buffer(target, policy)`
- `std::python::python_gpu_storage_buffer(target, debug_name)`
- `std::python::python_gpu_uniform_buffer(target, debug_name)`

### What landed

- Native tensor metadata capture now carries `device`, `device_kind`, `device_ordinal`, `device_pointer`, `device_type_code`, `host_accessible`, `writable`, `is_contiguous`, `dlpack_capable`, `cuda_array_interface_version`, `shape`, `strides`, `element_count`, and `byte_length`.
- Tensor virtual attrs now route through one dedicated dispatch seam, so authored Kain can read GPU-facing tensor truth through `kain_tensor_info(...)` without adding a new runtime builtin.
- `std::python` now exposes a truthful Python-tensor-to-`GpuBuffer` contract and a thin CPU shared-buffer-to-`GpuBuffer` wrapper without touching Alpha's shared-buffer adoption internals.
- `std::gpu::gpu_buffer_descriptor(...)` now preserves the contract and device metadata instead of collapsing imported Python tensors down to generic host-buffer fields.
- `benchmark/cases_v2/python_interop.kn` now has a Theta smoke row, `python_gpu_tensor_contract`, backed by a fake CUDA-array-interface + DLPack-style Python object so the contract can be exercised on machines without a real GPU.

### Proof and benchmark artifacts

- Proof file added:
  - `runtime/native/src/core/z3/proofs/native-python-tensor-byte-length-checked-mul-stays-in-int64-range.yaml`
- Benchmark/demo row added:
  - `python_gpu_tensor_contract`
- Benchmark expectation refreshed:
  - `python_raw_tensor_workflow` now expects the richer tensor metadata lane, including nonzero `element_count` and `byte_length`.

### Known compromise

- This pass lands the truthful contract and authored GPU-facing descriptor lane first. It does not yet wire a native executor import path that consumes DLPack or CUDA Array Interface handles all the way into runtime-owned GPU memory objects.
- The CPU zero-copy shared-buffer adoption path remains Alpha-owned. Theta only wraps that surface at `std::python::python_shared_buffer_gpu(...)` and does not change the underlying adoption policy.
