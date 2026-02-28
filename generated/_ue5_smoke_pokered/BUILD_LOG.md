# Build Log - PokeredFirmwareSmoke 
 
**Build Date**: Sat 02 28 2026 06:48 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0 (build 1772209264)
🚀 Building UE5 Plugin: PokeredFirmwareSmoke
📍 Plugin directory: .

📚 Loaded stdlib from: M:\Code\Kain\stdlib\ue5
📁 Source files: 13 (stdlib: 12, user: 1)
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
      1. sm64_all.kn

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
   ✓ sm64_all.kn validated

   ℹ️  Stdlib merge: 409 total → 0 kept (409 pruned by tree-shake, 0 shadowed by user code)
🔍 Type checking merged program...
Runtime error: ❌ Type error in merged program: Type error at Span { start: 0, end: 0 }: actor.kn:1:1: Item type not yet supported in type checker
```
