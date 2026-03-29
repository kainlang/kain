# Build Log - FluidFlow 
 
**Build Date**: Sun 03 29 2026 03:02 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0 (build 1773794355)
🚀 Building UE5 Plugin: FluidFlow
📍 Plugin directory: .

📚 Loaded stdlib from: M:\Code\Kain\stdlib\ue5
📁 Source files: 14 (stdlib: 12, user: 2)
   📚 Stdlib files:
      1. actor.kn
      2. common.kn
      3. components.kn
      4. gameplay.kn
      5. materials.kn
      6. math.kn
      7. particles.kn
      8. patterns.kn
      9. shaders.kn
      10. skeletal_mesh.kn
      11. utilities.kn
      12. world.kn
   📝 User files:
      1. FluidDynamics.kn
      2. FluidDynamicsGraph.kn

🔍 Validating source files...
   ✓ actor.kn validated
   ✓ common.kn validated
   ✓ components.kn validated
   ✓ gameplay.kn validated
   ✓ materials.kn validated
   ✓ math.kn validated
   ✓ particles.kn validated
   ✓ patterns.kn validated
   ✓ shaders.kn validated
   ✓ skeletal_mesh.kn validated
   ✓ utilities.kn validated
   ✓ world.kn validated
   ✓ FluidDynamics.kn validated
Runtime error: 3 parse error(s) found:

❌ Parse error in M:\Code\Kain\unreal_plugins\FluidFlow\FluidDynamicsGraph.kn:
   FluidDynamicsGraph.kn:630:14
   |
630 |     component.active_pipeline_id = pipeline_id
   |              ^
   |
   Expected identifier, got '.'

❌ Parse error in M:\Code\Kain\unreal_plugins\FluidFlow\FluidDynamicsGraph.kn:
   FluidDynamicsGraph.kn:631:14
   |
631 |     component.is_pipeline_running = true
   |              ^
   |
   Expected identifier, got '.'

❌ Parse error in M:\Code\Kain\unreal_plugins\FluidFlow\FluidDynamicsGraph.kn:
   FluidDynamicsGraph.kn:635:14
   |
635 |     component.is_pipeline_running = false
   |              ^
   |
   Expected identifier, got '.'
```
