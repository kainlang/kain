# Vulkain Examples

`mesh-scene` is the first inline C bitcode Vulkain example. The scene data is authored in Kain, then lowered through `use c::vulkain_bridge` into the Vulkan command stream:

```powershell
.\examples\mesh-scene\run.ps1
```

The run compiles the GLSL shaders, validates SPIR-V, builds the inline C-backed Kain executable, opens a Win32/Vulkan window, and writes `.kain/run/vulkain_mesh_scene_report.txt`.

`std-math-bounce-game` drives a procedural raytraced bouncing-cube game state from Kain. The Kain source owns the `std::input` WASD bindings, synthetic validation input, `std::math` physics, ray/AABB/triangle probes, quaternions, procedural noise, and render parameters before handing the frame to Vulkain:

```powershell
.\examples\std-math-bounce-game\run.ps1
```
