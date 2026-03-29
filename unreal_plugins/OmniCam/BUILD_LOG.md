# Build Log - OmniCam 
 
**Build Date**: Tue 02 24 2026 11:39 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0
ℹ️  No KAIN.toml found, using auto-detection...

🔍 Using directory name as plugin name: OmniCam
📁 Found 1 .kn file(s):
   - omnicam.kn

🚀 Building UE5 Plugin: OmniCam
📍 Plugin directory: m:\Code\Factory\OmniCam

📚 Loaded stdlib from: m:\Code\Kain\stdlib\ue5
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
      1. m:\Code\Factory\OmniCam\omnicam.kn

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
Runtime error: 1 parse error(s) found:

❌ Parse error in m:\Code\Factory\OmniCam\omnicam.kn:
   omnicam.kn:89:28
   |
89 |     let info: CameraInfo = CameraInfo {
   |                            ^
   |
   Struct literal syntax is not supported in KAIN. Found 'CameraInfo { ... }'.
Use field-by-field assignment instead:

Example:
let obj = CameraInfo()
obj.field1 = value1
obj.field2 = value2
```
