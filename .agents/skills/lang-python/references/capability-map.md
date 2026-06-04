# Python Capability Map

Load this when a task needs exact Python bridge surfaces, runtime seams, or
helper families.

## Core Import And Host Objects

- `import ...` and `from ... import ...` bind live CPython module/object handles.
- Local `.py` and package imports resolve relative to the importing `.kn` file, then ancestor roots and `src/`, then cwd/cwd `src/`, then ambient Python.
- `from pkg import name` tries an attribute first, then a nested `pkg.name` module.
- Host-object field access routes through Python getattr.
- Host-object calls preserve positional args and lower named Kain args to Python kwargs.
- Direct callable host objects support keyword-only Python APIs.

## Runtime Seams

- `crates/core/src/parser.rs`, `ast.rs`, `types.rs`, `runtime.rs`: syntax, binding, type/runtime dispatch, named-arg preservation.
- `crates/python/src/lib.rs`: Kain-facing bridge, conversion, local import resolution, diagnostics, materialization.
- `runtime/native/src/core/python_runtime.c`: embedded interpreter lifetime, tagged handles, umbrella seam.
- `runtime/native/src/core/python_runtime_region.c`: region import/attr caches and fast-call counters.
- `runtime/native/src/core/python_runtime_buffers.c`: buffer views and shared-buffer adoption.
- `runtime/native/src/core/python_runtime_async.c`: async futures and actor callbacks.
- `runtime/native/src/core/python_runtime_gpu.c`: image/tensor/GPU adoption.
- `stdlib/python.kn`, `stdlib/interop.kn`: authored bridge vocabulary.

## Helper Families

Raw/base helpers:

- `py_import`, `py_import_with_context`, `py_import_from_with_context`
- `py_call`, `py_call_raw`, `py_getattr`, `py_getattr_raw`
- `py_setattr`, `py_hasattr`, `py_eval`, `py_eval_raw`, `py_exec`

Region helpers:

- `python_region_begin`, `python_region_end`, `python_region_import`
- `python_region_getattr_raw`, `python_region_call_args`
- `python_region_call_attr_args`, `python_region_call_raw_args`
- `python_region_call_raw_attr`
- `python_region_call_raw_f64_trunc_i64`
- `python_region_call_attr_raw_f64_trunc_i64`
- `python_region_buffer_view`
- cache and telemetry counters such as `python_region_import_cache_hits`, `python_region_attr_cache_hits`, `python_region_call_count`, and `python_region_fast_call_count`

Async and actor helpers:

- `python_call_async`, `python_call_attr_async`
- `python_future_from_awaitable`, `python_future_state`, `python_future_done`
- `python_future_await`, `python_future_cancel`, `python_future_close`
- `python_actor_callback`, `python_actor_callback_callable`
- `python_actor_callback_close`, `python_actor_callback_delivered`

Materialization helpers:

- `python_shared_buffer`, `python_shared_image`
- `python_image`, `python_image_shared`, `python_image_owned`, `python_image_to`
- `python_tensor`, `python_tensor_shared`, `python_tensor_owned`, `python_tensor_to`
- `python_tensor_interop_info`, `python_tensor_shape_dim`, `python_tensor_stride_dim`
- `kain_shared_buffer_from_py`, `kain_shared_image_from_py`
- `kain_image_from_py`, `kain_tensor_from_py`, `kain_geometry_from_py`
- shared/owned variants of image, tensor, and geometry adoption

GPU bridge helpers:

- `python_shared_buffer_gpu`
- `python_gpu_storage_buffer`

## Return Conversion

- `None` becomes Kain `none`.
- `bool`, `int`, `float`, and `str` become Kain scalars.
- `bytes` and `bytearray` become byte-like Kain arrays.
- Python `list` and `tuple` become arrays/tuples.
- String-key `dict` becomes struct-like Kain data.
- NumPy-like arrays may materialize through shape/list metadata.
- Rich objects such as modules, classes, tensors, apps, windows, and sessions stay host objects unless explicitly materialized.

## Diagnostics Expectations

Prefer Kain-shaped errors that name:

- Python package/import name
- symbol or attribute
- call target and whether kwargs were involved
- dtype, shape, contiguity, ownership, or event-loop issue when known
- likely fix, such as install package, alias symbol, use `_raw`, use shared/owned helper, or pump/await event loop
