# Python Facades And Patterns

Load this when designing authored Kain over real Python packages.

## Direct Imported Host Object

Use this when the package object model is the API:

```kn
import pyglet as pyglet

fn pump(window: Any) -> Int:
    let _events = window.dispatch_events()
    let _flip = window.flip()
    return 0
```

Kain owns lifecycle, cadence, state, reports, actors/worlds, and shutdown.
Python owns the package internals.

## Sibling `.py` Helper

Use this when Python syntax is substantial, callback-heavy, or decorator-heavy.

```text
src/
  main.kn
  tools/
    __init__.py
    mcp_helper.py
```

```kn
import tools.mcp_helper as mcp_helper

fn boot(name: String) -> Any:
    return mcp_helper.create_server(name)
```

Prefer a real helper file over hundreds of lines of string-built Python.

## Tiny Bootstrap

Use `python_exec` only for short helper definitions that are awkward as attr
calls:

- decorator registration such as FastMCP
- small benchmark factories
- narrow callback glue

Do not hide an entire app, GUI, or package implementation inside one giant
`python_exec` string.

## Facade Shape

Write facades as Kain policy wrappers over Python packages:

```kn
import numpy as np

fn normalized_grid(width: Int, height: Int) -> Any:
    let xs = np.linspace(start = 0.0, stop = 1.0, num = width)
    let ys = np.linspace(start = 0.0, stop = 1.0, num = height)
    return np.meshgrid(xs, ys)
```

Keep facades:

- thin
- package-specific
- honest about host objects versus Kain-owned data
- explicit about dependency names and version-sensitive behavior
- small enough that app code stops spelling bridge calls everywhere

Do not fake static typing for all of Python.

## Canonical Facade Families

Start facade patterns around:

- NumPy: arrays, dtype, shape, buffer adoption, region hot loops
- Torch: CPU/GPU tensors, device metadata, DLPack/CUDA array interface
- Pandas: CSV/Parquet/dataframe boundaries, dtype policy, export shape
- PIL/Pillow: image load/save, pixel formats, shared/owned image adoption
- Qt/PySide/Tkinter/Flet/Pygame/Pyglet: windows, event loops, widgets
- FastMCP: decorator-heavy server setup with Kain-owned orchestration
- sounddevice/pyqtgraph/OpenCV/DCC packages: real ecosystem capability with Kain policy

## Project-Owned Dependency Truth

For serious packages, record:

- pip/distribution package name
- Python import name
- version range or exact version when behavior matters
- platform/GPU/native wheel constraints
- local helper roots
- install/fix text for diagnostics

If a facade package manager exists later, make it install Kain facade modules
and dependency metadata. It should not replace natural `import ...`; it should
make repeated package usage canonical.

## Event Loop And Callback Pattern

For GUI/tool/evented packages:

```text
Python package event source
  -> Python object/callback records compact state
  -> Kain facade polls or awaits
  -> Kain actor/world owns policy
```

Prefer:

- explicit tick/pump/await functions
- compact status records
- one facade per package/session
- Kain-owned teardown

Avoid:

- Python callbacks mutating Kain state directly from foreign control flow
- hidden event-loop ownership
- raw host-object calls scattered across unrelated app files

## Ownership Pattern

Stay host-object when the next operation is another Python method call.

Materialize when Kain must own:

- indexing and mutation
- lifetime and teardown
- shared-buffer synchronization
- export to GPU/runtime/native lanes
- proof-visible ownership metadata

Use shared mode when mutation must reflect into the Python owner. Use owned mode
when deterministic detached Kain mutation matters. Use auto mode only when the
facade inspects and reports the chosen ownership.
