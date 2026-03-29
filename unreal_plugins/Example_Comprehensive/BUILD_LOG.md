# Build Log - ComprehensiveTest 
 
**Build Date**: Tue 02 24 2026 05:38 
**Status**: FAILED 
**Error Type**: KAIN COMPILATION 
 
## KAIN Compilation Errors 
 
```
 KAIN Compiler v0.1.0
🚀 Building UE5 Plugin: ComprehensiveTest
📍 Plugin directory: Plugins

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
      1. src/main.kn

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
Runtime error: 10 parse error(s) found:

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:88:43
   |
88 | type OnHealthChanged = delegate(new_health: Float)
   |                                           ^
   |
   Expected Comma, got Colon

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:89:40
   |
89 | type OnDamageReceived = delegate(damage: Float, damage_type: DamageType)
   |                                        ^
   |
   Expected Comma, got Colon

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:90:37
   |
90 | type OnItemPickup = delegate(item_id: Int, quantity: Int)
   |                                     ^
   |
   Expected Comma, got Colon

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:228:24
   |
228 |     state target: Actor
   |                        ^
   |
   Expected Eq, got Newline("\n    ")

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:258:1
   |
258 | 
   | ^
   |
   Dedent

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:284:47
   |
284 | shader compute NoiseGenerator(thread_id: Vec3):
   |                                               ^
   |
   Expected Arrow, got Colon

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:313:17
   |
313 |     on_refresh: void
   |                 ^
   |
   Identifier 'void' conflicts with reserved keyword. Please choose a different name.
Reserved keywords include KAIN keywords (fn, let, struct, etc.), HLSL keywords (cbuffer, register, etc.), C++ keywords (class, virtual, etc.), and UE5 macros (UCLASS, UPROPERTY, etc.)

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:317:29
   |
317 |         return VerticalBox()
   |                             ^
   |
   Indent

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:390:5
   |
390 |     idle:
   |     ^
   |
   Expected Struct, got Ident("idle")

❌ Parse error in m:\Code\Factory\Example_Comprehensive\src/main.kn:
   main.kn:397:5
   |
397 |     attacking:
   |     ^
   |
   Expected item
```
