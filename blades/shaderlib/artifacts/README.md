# gpu_showcase.kn - GPU Artifacts (Proof)

Source: `blades/shaderlib/gpu_showcase.kn`
Built: 2026-09-28 with `kain gpu-artifacts`

Build command:
```
kain gpu-artifacts "blades/shaderlib/gpu_showcase.kn" -o "blades/shaderlib/artifacts/gpu_showcase" --target all
```

Result: 10 files, 12 shader stages in one SPIR-V module (23,848 bytes, magic `03 02 23 07`).

## File map

| File | What it is |
|------|------------|
| `gpu_showcase.spv` | Canonical SPIR-V binary, all 12 entry points |
| `gpu_showcase.shader_bundle.json` | Runtime catalog, includes SPIR-V bytes_hex + module name |
| `gpu_showcase.reflect.json` | Per-shader reflection: stage, inputs, bindings @N, output type |
| `gpu_showcase.gpu.rs` | Rust host wrappers (ShaderDesc + BindingDesc per kernel) |
| `kain_compute_residency.json` | Compute plan: workgroup + dispatch + tensor bindings for the 2 compute kernels |
| `kain_compute_residency_*.bin` | 4-byte staging payloads per compute binding (src/dst/pad, indirect_buf/workload_size) |

## Shaders in this module (from reflect.json)

1. InceptionKernel - compute, workgroup(32,1,1), subgroup(32), spec constants
2. MandelbulbMesh - mesh (procedural fractal geometry)
3. MandelbulbCull - task (meshlet culling)
4. UniversalRayGen - ray_gen (Hopf fibration color)
5. MandelbulbHit - closest_hit (orbit-trap color)
6. CosmicMicrowaveBackground - miss (CMB dipole + starfield)
7. FractalDensityTest - any_hit (density threshold)
8. TesseractIntersection - intersection (4D hypercube projection)
9. FractalUtility - callable (Julia param blend)
10. RasterFallback - vertex (warp distortion fallback)
11. ProceduralReality - fragment (4D Julia set, 256-iter loop)
12. IndirectController - compute, workgroup(1,1,1), GPU-driven dispatch writer

Compute kernels with residency entries:
- `shader::InceptionKernel::compute` - workgroup [32,1,1], dispatch [128,1,1]
- `shader::IndirectController::compute` - workgroup [1,1,1], dispatch [1,1,1]

## Validation

- SPIR-V magic: `03 02 23 07` OK, 23,848 bytes
- `spirv-val --target-env vulkan1.3 gpu_showcase.spv` reports:
  `Capability MeshShadingEXT is not allowed by Vulkan 1.3 specification (or requires extension)`
  This is expected: the mesh + task stages need `VK_EXT_mesh_shader`. The remaining 10 execution models validate under core Vulkan 1.3.
- No derived HLSL / WGSL / PTX text for this file: the mesh, task, and ray-tracing stages (raygen, closesthit, miss, anyhit, intersection, callable) are SPIR-V native and have no HLSL/WGSL lowering in this snapshot. Compute + vertex + fragment stages are covered by the SPIR-V path.

## Why this is here

Reddit r/graphicsprogramming proof thread: one `.kn` file in, LLVM host + SPIR-V + residency sidecars out. These files are force-added (the repo gitignores `*.spv`, `*.json`, `*.bin` by default).
