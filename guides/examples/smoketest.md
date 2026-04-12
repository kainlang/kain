# Smoketest Lanes

`smoketest/` is the proof matrix for Kain's runtime bridges, UI, GPU, importer, and mixed-language workflows. Treat these folders as runnable evidence for the language and runtime, not as generic sample code.

## How To Read It

Start from the lane README that matches the subsystem you are changing, then compare the output against the current guide pages and live CLI behavior. If a lane has a stronger local README or source comment, use that first.

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

## Why It Matters

The smoketest tree is where the docs become executable. If a language or runtime feature is described in `guides/`, there should usually be a lane here that proves the claim end to end. That is especially true for importer output, bridge behavior, native UI packaging, and compiler-owned intent semantics.

## Practical Rule

Use the smoketests as the first stop when a feature works in prose but not in practice. They are the fastest way to see whether the current code path still matches the documented behavior.
