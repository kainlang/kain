# Build Log - BlueprintExample 
 
**Build Date**: Tue 02 24 2026 05:18 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0
🚀 Building UE5 Plugin: BlueprintExample
📍 Plugin directory: .

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
      1. blueprint.kn

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
Runtime error: 16 parse error(s) found:

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:66:1
   |
66 |     
   | ^
   |
   Struct literal syntax is not supported in KAIN. Found 'CapsuleComponent { ... }'.
Use field-by-field assignment instead:

Example:
let obj = CapsuleComponent()
obj.field1 = value1
obj.field2 = value2

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:74:32
   |
74 |     state can_crouch: Bool = true
   |                                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:83:74
   |
83 |     state ability_class: String = "/Game/Abilities/BP_DashAbility.BP_DashAbility"
   |                                                                          ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:91:8
   |
91 |     // LinearColor property (FLinearColor)
   |        ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:244:43
   |
244 |         visual_root.SetRelativeLocation(vec3(0.0, 0.0, 0.0))
   |                                           ^
   |
   Struct literal syntax is not supported in KAIN. Found 'SceneComponent { ... }'.
Use field-by-field assignment instead:

Example:
let obj = SceneComponent()
obj.field1 = value1
obj.field2 = value2

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:245:44
   |
245 |         audio_root.SetRelativeLocation(vec3(100.0, 0.0, 0.0))
   |                                            ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:322:41
   |
322 | // ACTOR 9: Pure Function Calls (Blueprint Pure Nodes)
   |                                         ^
   |
   Struct literal syntax is not supported in KAIN. Found 'SceneComponent { ... }'.
Use field-by-field assignment instead:

Example:
let obj = SceneComponent()
obj.field1 = value1
obj.field2 = value2

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:323:55
   |
323 | // ───────────────────────────────────────────────────────────────────────────
   |                                                       ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:330:31
   |
330 |         // Pure function calls (no exec pins in Kismet)
   |                               ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:335:13
   |
335 | // ───────────────────────────────────────────────────────────────────────────
   |             ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:337:1
   |
337 | // ───────────────────────────────────────────────────────────────────────────
   | ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:399:18
   |
399 | // ✅ Component defaults (struct literal initializers)
   |                  ^
   |
   Struct literal syntax is not supported in KAIN. Found 'Point { ... }'.
Use field-by-field assignment instead:

Example:
let obj = Point()
obj.field1 = value1
obj.field2 = value2

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:414:33
   |
414 | // ✅ Export table generation (Blueprint, Class, CDO, SCS, Components, Functions)
   |                                 ^
   |
   Struct literal syntax is not supported in KAIN. Found 'CapsuleComponent { ... }'.
Use field-by-field assignment instead:

Example:
let obj = CapsuleComponent()
obj.field1 = value1
obj.field2 = value2

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:416:16
   |
416 | // ✅ Component template exports (ComponentTemplate property)
   |                ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:419:33
   |
419 | // ✅ Event function exports (ReceiveBeginPlay, ReceiveTick, custom)
   |                                 ^
   |
   Expected item

❌ Parse error in m:\Code\Factory\Example_Blueprint\blueprint.kn:
   blueprint.kn:422:30
   |
422 | // ✅ Object flags (RF_PUBLIC, RF_STANDALONE, RF_DEFAULT_SUB_OBJECT)
   |                              ^
   |
   Expected item
```
