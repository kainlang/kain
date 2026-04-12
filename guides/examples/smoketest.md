# Smoketest Lanes

`smoketest/` is the proof matrix for Kain's runtime bridges, UI, GPU, and
mixed-language workflows.

## Major Lanes

| Lane | What It Proves |
| --- | --- |
| `3D/` | Viewport, sculpt, material, and fast 3D runtime behavior |
| `UI/` | Native UI, layout, shader canvas, and desktop app behavior |
| `c_ffi/` | C bridge behavior and shared payload contracts |
| `cargo/` | Rust crate and workspace import / build flows |
| `cargo_node/` | Cargo + Node orchestration |
| `node/` | Node-only runtime bridges |
| `python/` | Python bridge lanes and helper modules |
| `py_node/` and `py_cargo_node/` | Hybrid Python/Node/Cargo workflows |
| `fabric/` | Fabric manifest orchestration |
| `allinone/` | Broad regression harness that replays the full mixed runtime stack |
| `compiler_owned_intent/` | `law`, `patch`, `converge`, `world`, `orchestrate` smoke coverage |

## How To Read It

Each smoke folder should be treated as a runnable proof, not as generic sample
code. The README in each lane explains run steps and expected outputs.

## Practical Rule

If a feature matters enough to document, it should usually also have a smoke
lane that proves it works.
