# pykain

The official Python companion layer for [Kain](https://github.com/kain-lang).

**Version:** 0.1.0

## What is this?

pykain is a Python package that Kain scripts import to get a **stable, normalized, backend-agnostic** Python interop surface.

Without pykain, Kain scripts touch raw Python objects, raw package APIs, and raw conversion choices => lots of host-specific details leaking into every script.

With pykain, one `import pykain as pykain` gives you:

- **`pykain.tensor`** ‒ normalized tensor crossing (numpy/torch unified)
- **`pykain.image`** |-> normalized image/raster crossing (numpy-first shared image lane)
- **`pykain.buffer`** ~> normalized buffer crossing (shared memory contracts)
- **`pykain.window`** ~> normalized window management (adapter discovery, not a baked-in GUI choice)
- **`pykain.gpu`** - compute-first GPU resource descriptors over Python objects
- **`pykain.shader`** ___ shader module envelopes for Kain shader/compute payloads
- **`pykain.actor` / `pykain.world`** --> semantic envelopes for Kain actor/world flows
- **`pykain.validate`** ~> validation helpers for Kain test harnesses

## The Design

```
Kain owns architecture:  worlds, actors, entangle, teleport, laws, patches
pykain owns the bridge:  normalization, backend quirks, conversion, validation
```

Kain scripts stay clean. Python-side details change behind a stable pykain surface.

## Window Adapters

`pykain.window` no longer hardcodes `pygame`, `flet`, or any other toolkit.
Instead, Kain passes adapter module paths in `window_adapters`, or you set
`PYKAIN_WINDOW_ADAPTERS` in the host process.

Each adapter module provides:

```python
def is_available(plan: dict) -> bool: ...
def open_window(plan: dict) -> object: ...
def close_window(handle: object, plan: dict) -> int: ...
def capture_window(handle: object, plan: dict) -> object: ...      # optional
def backend_info(plan: dict) -> dict: ...                           # optional
def backend_name() -> str: ...                                      # optional
```

That means `pykain` can host any Python window manager without shipping a
blessed GUI religion in core.

## Native Core

`pykain` ships a lazy C extension as `pykain._native`. Importing `pykain` stays
boring, but buffer/tensor/image inspection, byte signatures, and byte views use
the native path when it is present.

```python
import numpy as np
import pykain

arr = np.arange(12, dtype=np.uint8).reshape(3, 4)

print(pykain.native_available())
print(pykain.inspect(arr))
print(pykain.as_buffer(arr).descriptor())
print(pykain.gpu.compute_buffer(arr, binding=0).descriptor())
```

The Python API stays zero-friction: pass live Python objects and let `pykain`
adapt them. Kain's direct `use c::...` lane remains the right path for native
libraries Kain owns directly.

For Kain control flow, prefer bool-shaped probes such as
`pykain.tensor.grid_ok(...)`, `pykain.image.render_ok(...)`, and
`pykain.buffer.grid_ok(...)`. Rich Python dicts, strings, and exact scalar
descriptors should stay inside pykain until Kain explicitly materializes them;
this keeps scripts stable across the current host-object bridge behavior.

## Usage from Kain

```kn
import pykain as pykain

fn main() -> Int:
    // Tensor: one call, backend-agnostic
    let tensor = pykain.tensor.grid(plan_text, seed)
    let info = pykain.tensor.info(tensor)
    let sig = pykain.tensor.signature(tensor)

    // Image: one call, backend-agnostic
    let image = pykain.image.render(plan_text)
    let img_info = pykain.image.info(image)

    // Buffer: one call
    let buf = pykain.buffer.grid(plan_text, seed)

    // Window: open/capture/close through configured adapters
    let result = pykain.window.open(plan_text)
    let frame = pykain.window.capture(plan_text)
    let _close = pykain.window.close()

    // Validation: one-liner module checks
    if pykain.validate.module("numpy") == 0:
        return 1
    return 0
```

## Package Structure

```
packages/python/
├── pyproject.toml        # pip/editable install metadata
├── pykain/
│   ├── __init__.py      # Public surface :: what Kain sees on import
│   ├── tensor.py        # Tensor normalization (grid, info, signature, validate)
│   ├── image.py         # Image normalization (render, info, signature, validate)
│   ├── buffer.py        # Buffer normalization (grid, info, signature, validate)
│   ├── _native.c        # CPython C extension backing the hot path
│   ├── contracts.py     # Kain semantic envelopes
│   ├── gpu.py           # Compute-first GPU descriptors
│   ├── shader.py        # Shader/compute module envelopes
│   ├── actor.py         # Actor/message envelopes
│   ├── world.py         # World/entangle/patch/session envelopes
│   ├── window.py        # Window management (open, capture, close, backend_info)
│   └── validate.py      # Test harness helpers (module, tensor_shape, contract)
├── data/
│   └── pykain_config.json
├── smoke.kn             # Kain smoke test --> proves the ergonomic win
└── README.md
```

## The Recursive Self-Hosting Play

Kain already has native Python import 〰 it can import any Python package and call it
like a native module. pykain uses THAT power to make Python an even better host for Kain:

1. Kain imports Python natively
2. pykain teaches Python how to be a better Kain companion
3. Kain scripts get cleaner, more stable, more backend-agnostic
4. The whole loop strengthens

It's almost cybernetic. Kain is not begging Python for permission ~ it's absorbing
Python as a subsystem, and pykain is the diplomatic layer that makes the subsystem elegant.

## Requirements

- Python 3.10+
- numpy (required)
- torch (optional ~> for torch tensor backend)
- any window toolkit is optional --- ship or install an adapter module for it

## Native Acceleration

The public API is Python-first on purpose. That makes the compatibility story
ridiculously broad. The hot path does NOT have to stay pure Python forever:
`pykain._native` can later own zero-copy adapters, shared-buffer packing,
capture copies, and tensor/image marshaling in C/C++/Rust without changing the
surface Kain imports.

## License

Same as Kain.
