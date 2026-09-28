# shaderlib artifacts (proof)

One `.kn` file in, SPIR-V + HLSL + WGSL + Rust host + reflection + residency out.
Built 2026-09-28 with `kain gpu-artifacts <file> -o artifacts/<name>/<name> --target all`.

## Layout

```
artifacts/
  gpu_showcase/   12 stages in one module (compute, mesh, task, raygen, closesthit, miss, anyhit, intersection, callable, vertex, fragment, indirect compute)
  ocean/          fragment ray-traced ocean (OceanFragment)
  blackhole/      fragment Schwarzschild raymarcher (BlackHoleFragment)
  supermotion/    compute + vertex + fragment + indirect compute (full pipeline sample)
```

Each folder holds its own `.spv`, `.shader_bundle.json`, `.reflect.json`, `.gpu.rs`,
derived text (`*.derived.hlsl`, `*.derived.wgsl` where emitted), and its own
`kain_compute_residency.json` + staging `.bin`s (kept per-folder so nothing clobbers).

## What each source emitted

| Source | SPIR-V | HLSL | WGSL | PTX | Residency |
|--------|--------|------|------|-----|-----------|
| gpu_showcase.kn | yes (23,848 B) | no | no | no | yes, 2 compute kernels |
| ocean.kn | yes (41,272 B) | yes (OceanFragment) | no | no | n/a (fragment only) |
| blackhole.kn | yes (22,480 B) | yes (BlackHoleFragment) | no | no | n/a (fragment only) |
| supermotion_v2.kn | yes (60,152 B) | yes | yes | no | yes, 2 compute kernels |

All four SPIR-V binaries carry magic `03 02 23 07`.

Why no WGSL/PTX on ocean + blackhole: fragment-only raymarchers lower to the
HLSL path in this snapshot. WGSL appears once vertex + compute join the module
(see supermotion). PTX is not emitted for any of these four in this snapshot;
the PTX backend exists (`--target cuda`, `derived_ptx` in the bundle schema)
but these shaders do not trigger it. USF is not a `gpu-artifacts` target at
all: USF is the UE5 import/codegen path (`kain inject`, `crates/ue5-shaders`),
not the SPIR-V artifact path. HLSL here is the DX feed for that chain.

## Validation (honest)

- `spirv-val --target-env vulkan1.3` on ocean.spv: PASS
- `spirv-val --target-env vulkan1.3` on blackhole.spv: PASS
- `spirv-val --target-env vulkan1.3` on gpu_showcase.spv: reports
  `Capability MeshShadingEXT is not allowed by Vulkan 1.3` (expected, mesh + task
  stages need VK_EXT_mesh_shader; other 10 entry points are core Vulkan 1.3)
- `spirv-val --target-env vulkan1.3` on supermotion.spv: reports
  `All OpVariable instructions in a function must be the first instructions in
  the first block` at line 757 (codegen bug, filed as found; HLSL + WGSL text
  for the same source generates fine)

## Reproduce

```
kain gpu-artifacts blades/shaderlib/gpu_showcase.kn -o blades/shaderlib/artifacts/gpu_showcase/gpu_showcase --target all
kain gpu-artifacts blades/shaderlib/ocean.kn -o blades/shaderlib/artifacts/ocean/ocean --target all
kain gpu-artifacts blades/shaderlib/blackhole.kn -o blades/shaderlib/artifacts/blackhole/blackhole --target all
kain gpu-artifacts blades/shaderlib/supermotion_v2.kn -o blades/shaderlib/artifacts/supermotion/supermotion --target all
spirv-val --target-env vulkan1.3 blades/shaderlib/artifacts/*/*.spv
```

Note: the repo gitignores `*.spv`, `*.json`, `*.bin`, `*.hlsl`, `*.wgsl`, `*.ptx`
by default. These files are force-added as Reddit proof.
