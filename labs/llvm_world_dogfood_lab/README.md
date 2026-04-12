# LLVM World Dogfood Lab

This lab is the canonical LLVM-facing dogfood app for the current Kain pipeline.
It stays in authored Kain while exercising the compiler-owned world, patch,
converge, orchestrate, actor mailbox, and native UI + viewport lanes.

What it does:

- drives a world singleton through patch transactions
- sends named payload messages through an actor mailbox
- runs a converge step and an orchestrated pipeline in the LLVM lane
- renders a dense native UI shell with an inspector rail, graph, timeline, and viewport

Layout:

- `src/`
  Kain source for the lab entrypoint.
- `generated/`
  LLVM output, the linked executable, and runtime sidecars from `build.sh`.

Run:

```bash
./build.sh
./run.sh
```

`build.sh` emits `generated/llvm_world_dogfood_lab.ll`, the linked executable,
and the runtime contract / realtime app sidecars.

`run.sh` auto-detects the current Wayland/X11 session when needed and launches
the linked binary from `generated/`.

Notes:

- The viewport uses the existing `magma_terraces` scene so the lab has a real
  native 3D surface without needing an extra asset bundle.
- The lab keeps to current LLVM-safe source shapes: named actor payloads,
  compiler-owned world patches, converge/orchestrate stages, arrays, loops, and
  JSX expressions.
