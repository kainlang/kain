---
name: lang-interop
description: Use only as a legacy compatibility router when old notes, prompts, or skills still mention `lang-interop`. Its job is to choose `lang-python`, `lang-c-abi`, or both; do not use it as the primary field manual for new work.
---

# Lang Interop

`lang-interop` is no longer the main field manual. It exists so old notes and
older prompts do not strand agents after the split.

For new work:

- use `lang-python` for first-class Python `import ...`, local `.py` resolution, `std::python`, host objects, and shared/owned Python materialization
- use `lang-c-abi` for `use c::...`, `use rust::...`, `kain import-c`, `kain import platform`, DLLs, platform packages, handles, callbacks, strings, buffers, and native bridge metadata
- load both when one feature genuinely spans Python and native ABI work

## Quick Routing

Choose the first skill by center of gravity:

| If the task says | Read first |
| --- | --- |
| `import numpy`, `import fastmcp`, local `.py`, `std::python`, `python_*`, `kain_*_from_py` | `lang-python` |
| `use c::...`, `[c_ffi]`, `kain import-c`, DLL, platform SDK, `use rust::...`, host bridge | `lang-c-abi` |
| Python package plus native wrapper or DLL/package lock plumbing | `lang-python` and `lang-c-abi` |

## Mixed Boundary Rule

If the user is:

- calling Python packages that themselves depend on native loaders
- exposing a Kain facade that combines Python host objects with C-ABI package state
- moving shared buffers/images/tensors between Python and a native runtime

then read both skills and keep the split honest:

- `lang-python` owns Python import semantics, local resolution, host-object rules, and materialization
- `lang-c-abi` owns native ABI shape, package metadata, loader contracts, handles, callbacks, and bridge tiers

## Anti-Pattern

Do not keep adding new depth here. Put reusable Python doctrine into
`lang-python` and reusable native-ABI doctrine into `lang-c-abi`.
