# KAIN Standard Library & The Babel Layer

## The Vision: "Godmode v3" - Zero-Overhead UE5 Scripting

KAIN is not just a transpiler—it's a **semantic bridge** between clean, readable code and optimal Unreal Engine C++. The Standard Library + Babel Layer is what makes this possible.

## What Is The Babel Layer?

The **Babel Layer** is a dedicated resolver in the KAIN compiler (`resolver.rs`) that maps KAIN function calls to their native Unreal C++ counterparts with **zero runtime overhead**.

### Example Transformations

```kain
// KAIN (what you write)
let pos = GetActorLocation(self)
let lerped = Lerp(a, b, 0.5)
PrintToScreen("Hello", 5.0, vec4(1,1,1,1))
SpawnActor<Bullet>(GetWorld(), location, rotation)
```

```cpp
// Generated C++ (optimal, native)
FVector pos = this->GetActorLocation();
float lerped = FMath::Lerp(a, b, 0.5f);
GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor(255,255,255,255), TEXT("Hello"));
GetWorld()->SpawnActor<ABullet>(location, rotation);
```

**No wrapper classes. No virtual calls. No overhead. Pure translation.**

## The Standard Library Structure

```
kain/stdlib/ue5/
├── math.kn          # FMath, vector operations, interpolation
├── actor.kn         # Actor lifecycle, transform, hierarchy
├── world.kn         # World queries, time, spawning
├── physics.kn       # Physics simulation, forces, impulses
├── audio.kn         # Sound playback, attenuation
├── input.kn         # Input handling, EnhancedInput
├── animation.kn     # Animation playback, montages
├── ai.kn            # AI movement, behavior
├── collision.kn     # Line traces, shape traces
├── niagara.kn       # Particle systems, VFX
├── materials.kn     # Dynamic materials, parameters
├── camera.kn        # Camera control, view targets
├── networking.kn    # Replication, authority checks
├── ui.kn            # UMG widgets, HUD
└── save.kn          # Save/load game data
```

### How It Works

1. **Automatic Injection**: The compiler scans `stdlib/ue5/` **before** compiling your code
2. **@extern Definitions**: Functions marked `@extern` are declarations only—no implementation
3. **Babel Resolution**: The resolver maps these to native UE5 calls during codegen
4. **Type Safety**: The compiler validates calls at compile-time using the stdlib signatures

## Core Principles

### 1. Zero-Cost Abstraction
Every KAIN stdlib call compiles to **optimal native code**. No wrappers, no indirection.

### 2. Semantic Understanding
The Babel Layer understands **context**:
- `GetActorLocation(self)` → `this->GetActorLocation()`
- `GetActorLocation(other_actor)` → `other_actor->GetActorLocation()`
- `Lerp(a, b, t)` → `FMath::Lerp(a, b, t)`

### 3. Invisible Magic
No imports needed. The stdlib is **always there**. Feels like a native language feature.

### 4. Type-Aware Resolution
The resolver knows UE5 types and can make intelligent decisions:
```kain
// KAIN
my_vector.Normalize()

// C++ (knows Vec3 → FVector)
my_vector.GetSafeNormal()
```

## Implementation Architecture

### Phase 1: Standard Library Definitions

**stdlib/ue5/math.kn**
```kain
@extern
fn Lerp(a: Float, b: Float, alpha: Float) -> Float

@extern
fn Clamp(value: Float, min: Float, max: Float) -> Float

@extern
fn Sin(value: Float) -> Float

@extern
fn Cos(value: Float) -> Float

@extern
fn Sqrt(value: Float) -> Float

@extern
fn VectorLength(vec: Vec3) -> Float

@extern
fn VectorNormalize(vec: Vec3) -> Vec3

@extern
fn VectorDot(a: Vec3, b: Vec3) -> Float

@extern
fn VectorCross(a: Vec3, b: Vec3) -> Vec3

@extern
fn FInterpTo(current: Float, target: Float, delta_time: Float, interp_speed: Float) -> Float

@extern
fn VInterpTo(current: Vec3, target: Vec3, delta_time: Float, interp_speed: Float) -> Vec3

@extern
fn RInterpTo(current: Rotator, target: Rotator, delta_time: Float, interp_speed: Float) -> Rotator
```

**stdlib/ue5/actor.kn**
```kain
@extern
fn GetActorLocation(actor: Actor) -> Vec3

@extern
fn GetActorRotation(actor: Actor) -> Rotator

@extern
fn SetActorLocation(actor: Actor, location: Vec3, sweep: Bool = false) -> Bool

@extern
fn SetActorRotation(actor: Actor, rotation: Rotator)

@extern
fn GetActorForwardVector(actor: Actor) -> Vec3

@extern
fn GetActorRightVector(actor: Actor) -> Vec3

@extern
fn GetActorUpVector(actor: Actor) -> Vec3

@extern
fn DestroyActor(actor: Actor)

@extern
fn SetActorHiddenInGame(actor: Actor, hidden: Bool)

@extern
fn SetActorEnableCollision(actor: Actor, enable: Bool)

@extern
fn AttachToActor(actor: Actor, parent: Actor, socket_name: String = "")

@extern
fn DetachFromActor(actor: Actor)
```

**stdlib/ue5/world.kn**
```kain
@extern
fn GetWorld() -> World

@extern
fn GetWorldDeltaSeconds() -> Float

@extern
fn GetWorldTimeSeconds() -> Float

@extern
fn SpawnActor<T>(world: World, location: Vec3, rotation: Rotator = Rotator()) -> T

@extern
fn SpawnActorDeferred<T>(world: World, location: Vec3, rotation: Rotator = Rotator()) -> T
```

**stdlib/ue5/physics.kn**
```kain
@extern
fn AddImpulse(component: PrimitiveComponent, impulse: Vec3, bone_name: String = "")

@extern
fn AddForce(component: PrimitiveComponent, force: Vec3, bone_name: String = "")

@extern
fn SetSimulatePhysics(component: PrimitiveComponent, simulate: Bool)

@extern
fn SetEnableGravity(component: PrimitiveComponent, enable: Bool)

@extern
fn SetPhysicsLinearVelocity(component: PrimitiveComponent, velocity: Vec3)

@extern
fn GetPhysicsLinearVelocity(component: PrimitiveComponent) -> Vec3

@extern
fn SetPhysicsAngularVelocityInDegrees(component: PrimitiveComponent, velocity: Vec3)
```

**stdlib/ue5/audio.kn**
```kain
@extern
fn PlaySoundAtLocation(world: World, sound: SoundBase, location: Vec3, volume: Float = 1.0, pitch: Float = 1.0)

@extern
fn PlaySound2D(world: World, sound: SoundBase, volume: Float = 1.0, pitch: Float = 1.0)

@extern
fn SpawnSoundAttached(sound: SoundBase, attach_to: SceneComponent, socket_name: String = "", volume: Float = 1.0) -> AudioComponent
```

**stdlib/ue5/input.kn**
```kain
@extern
fn GetInputAxisValue(controller: PlayerController, axis_name: String) -> Float

@extern
fn IsActionPressed(controller: PlayerController, action_name: String) -> Bool

@extern
fn IsActionReleased(controller: PlayerController, action_name: String) -> Bool

@extern
fn GetController() -> PlayerController

@extern
fn GetPlayerController(world: World, player_index: Int = 0) -> PlayerController
```

**stdlib/ue5/niagara.kn**
```kain
@extern
fn SpawnSystemAtLocation(world: World, system: NiagaraSystem, location: Vec3, rotation: Rotator = Rotator(), scale: Vec3 = vec3(1,1,1)) -> NiagaraComponent

@extern
fn SpawnSystemAttached(system: NiagaraSystem, attach_to: SceneComponent, socket_name: String = "", location: Vec3 = vec3(0,0,0), rotation: Rotator = Rotator(), scale: Vec3 = vec3(1,1,1)) -> NiagaraComponent

@extern
fn SetNiagaraVariableFloat(component: NiagaraComponent, name: String, value: Float)

@extern
fn SetNiagaraVariableVec3(component: NiagaraComponent, name: String, value: Vec3)
```

### Phase 2: Babel Resolver Patterns

**resolver.rs** (conceptual structure)
```rust
impl Resolver {
    fn resolve_function_call(&self, name: &str, args: &[Expr]) -> String {
        match name {
            // Actor methods (self -> this->)
            "GetActorLocation" | "GetActorRotation" | "SetActorLocation" 
            | "SetActorRotation" | "GetActorForwardVector" | "GetActorRightVector" 
            | "GetActorUpVector" | "DestroyActor" | "SetActorHiddenInGame" 
            | "SetActorEnableCollision" | "AttachToActor" | "DetachFromActor"
            => self.resolve_actor_method(name, args),
            
            // FMath functions
            "Lerp" | "Clamp" | "Sin" | "Cos" | "Sqrt" | "Pow" | "Abs" 
            | "Floor" | "Ceil" | "Round" | "FInterpTo" | "RInterpTo"
            => format!("FMath::{}({})", name, self.gen_args(args)),
            
            // UKismetMathLibrary (complex math)
            "VectorLength" | "VectorNormalize" | "VectorDot" | "VectorCross"
            | "VInterpTo" | "RotatorFromAxisAndAngle"
            => format!("UKismetMathLibrary::{}({})", name, self.gen_args(args)),
            
            // World functions (need GetWorld())
            "SpawnActor" | "SpawnActorDeferred" | "GetWorldDeltaSeconds" 
            | "GetWorldTimeSeconds" | "LineTraceSingle" | "SphereTraceSingle"
            => self.resolve_world_function(name, args),
            
            // GEngine functions
            "PrintToScreen" => self.generate_print_to_screen(args),
            
            // Component functions
            "AddImpulse" | "AddForce" | "SetSimulatePhysics" | "SetEnableGravity"
            | "SetPhysicsLinearVelocity" | "GetPhysicsLinearVelocity"
            => self.resolve_component_method(name, args),
            
            // Audio functions
            "PlaySoundAtLocation" | "PlaySound2D" | "SpawnSoundAttached"
            => format!("UGameplayStatics::{}({})", name, self.gen_args(args)),
            
            // Niagara functions
            "SpawnSystemAtLocation" | "SpawnSystemAttached"
            => format!("UNiagaraFunctionLibrary::{}({})", name, self.gen_args(args)),
            
            // Input functions
            "GetInputAxisValue" | "IsActionPressed" | "IsActionReleased"
            => self.resolve_input_function(name, args),
            
            _ => format!("{}({})", name, self.gen_args(args)) // Fallback
        }
    }
    
    fn resolve_actor_method(&self, name: &str, args: &[Expr]) -> String {
        if let Some(Expr::Ident(id, _)) = args.first() {
            if id == "self" {
                // self -> this->
                return format!("this->{}({})", name, self.gen_args(&args[1..]));
            }
        }
        // other_actor -> other_actor->
        format!("{}->{}({})", self.gen_expr(&args[0]), name, self.gen_args(&args[1..]))
    }
    
    fn resolve_world_function(&self, name: &str, args: &[Expr]) -> String {
        // Check if first arg is GetWorld() call
        if let Some(Expr::Call { callee, .. }) = args.first() {
            if let Expr::Ident(id, _) = &**callee {
                if id == "GetWorld" {
                    return format!("GetWorld()->{}({})", name, self.gen_args(&args[1..]));
                }
            }
        }
        format!("GetWorld()->{}({})", name, self.gen_args(args))
    }
    
    fn generate_print_to_screen(&self, args: &[Expr]) -> String {
        // PrintToScreen(message, duration, color)
        // -> GEngine->AddOnScreenDebugMessage(-1, duration, color, TEXT(message))
        let message = self.gen_expr(&args[0]);
        let duration = if args.len() > 1 { self.gen_expr(&args[1]) } else { "5.0f".to_string() };
        let color = if args.len() > 2 { 
            self.gen_expr(&args[2]) 
        } else { 
            "FColor::White".to_string() 
        };
        
        format!(
            "if (GEngine) {{ GEngine->AddOnScreenDebugMessage(-1, {}, {}, TEXT(\"{}\")); }}",
            duration, color, message
        )
    }
}
```

### Phase 3: Automatic Include Detection

**In Ue5Context:**
```rust
pub fn use_stdlib_module(&mut self, module: &str) {
    match module {
        "math" => {
            self.add_include("Kismet/KismetMathLibrary.h");
        }
        "physics" => {
            self.use_feature("Physics");
            self.add_include("PhysicsEngine/PhysicsHandleComponent.h");
        }
        "audio" => {
            self.add_include("Kismet/GameplayStatics.h");
            self.add_include("Components/AudioComponent.h");
        }
        "niagara" => {
            self.use_feature("Niagara");
            self.build_file.add_dependency_for_feature("Niagara");
            self.add_include("NiagaraFunctionLibrary.h");
            self.add_include("NiagaraComponent.h");
        }
        "input" => {
            self.use_feature("EnhancedInput");
            self.add_include("EnhancedInputComponent.h");
            self.add_include("EnhancedInputSubsystems.h");
        }
        "ui" => {
            self.use_feature("UMG");
            self.add_include("Blueprint/UserWidget.h");
            self.add_include("Blueprint/WidgetBlueprintLibrary.h");
        }
        _ => {}
    }
}
```

## Real-World Example: Complete FPS Character

```kain
actor FPSCharacter:
    state health: Float = 100.0
    state max_health: Float = 100.0
    state ammo: Int = 30
    state weapon_mesh: StaticMeshComponent = StaticMeshComponent()
    state camera: CameraComponent = CameraComponent()
    state muzzle_flash: NiagaraSystem
    state gunshot_sound: SoundBase
    
    on BeginPlay():
        // Setup weapon
        weapon_mesh = CreateDefaultSubobject<StaticMeshComponent>("Weapon")
        weapon_mesh.AttachToComponent(GetMesh(), "hand_r_socket")
        
        // Setup camera
        camera = CreateDefaultSubobject<CameraComponent>("Camera")
        camera.AttachToComponent(GetRootComponent(), "")
        camera.SetRelativeLocation(vec3(0, 0, 64))
    
    on Tick(dt: Float):
        HandleMovement(dt)
        HandleLooking(dt)
        HandleShooting()
        
        // Health regeneration
        if health < max_health:
            health = FInterpTo(health, max_health, dt, 2.0)
    
    fn HandleMovement(dt: Float):
        let controller = GetController()
        let move_forward = GetInputAxisValue(controller, "MoveForward")
        let move_right = GetInputAxisValue(controller, "MoveRight")
        
        if move_forward != 0.0:
            let forward = GetActorForwardVector(self)
            AddMovementInput(forward * move_forward)
        
        if move_right != 0.0:
            let right = GetActorRightVector(self)
            AddMovementInput(right * move_right)
    
    fn HandleLooking(dt: Float):
        let controller = GetController()
        let look_up = GetInputAxisValue(controller, "LookUp")
        let look_right = GetInputAxisValue(controller, "LookRight")
        
        AddControllerPitchInput(look_up * -1.0)
        AddControllerYawInput(look_right)
    
    fn HandleShooting():
        let controller = GetController()
        if IsActionPressed(controller, "Fire") and ammo > 0:
            Shoot()
    
    fn Shoot():
        ammo = ammo - 1
        
        // Muzzle flash VFX
        let muzzle_location = weapon_mesh.GetSocketLocation("Muzzle")
        SpawnSystemAtLocation(GetWorld(), muzzle_flash, muzzle_location)
        
        // Gunshot audio
        PlaySoundAtLocation(GetWorld(), gunshot_sound, muzzle_location, 1.0, 1.0)
        
        // Line trace for hit detection
        let start = camera.GetComponentLocation()
        let forward = camera.GetForwardVector()
        let end = start + (forward * 10000.0)
        
        var hit: HitResult
        if LineTraceSingle(GetWorld(), start, end, &hit):
            // Hit something!
            let impact_location = hit.Location
            
            // Spawn impact effect
            SpawnSystemAtLocation(GetWorld(), impact_effect, impact_location)
            
            // Apply damage if hit an enemy
            if hit.Actor.IsA<Enemy>():
                hit.Actor.TakeDamage(25.0)
    
    fn TakeDamage(amount: Float):
        health = Clamp(health - amount, 0.0, max_health)
        
        if health <= 0.0:
            Die()
    
    fn Die():
        PrintToScreen("You Died!", 5.0, vec4(1, 0, 0, 1))
        DestroyActor(self)
```

**This is 100% KAIN. Zero C++. Compiles to optimal native code.**

## Development Roadmap

### Phase 1: Core Systems (Week 1)
- ✅ Math (FMath, interpolation)
- ✅ Actor (transform, lifecycle)
- ✅ World (time, spawning)
- 🔥 Physics (forces, impulses, simulation)
- 🔥 Audio (sound playback)

### Phase 2: Gameplay Systems (Week 2)
- Input (axis values, actions, EnhancedInput)
- Animation (montages, blend spaces)
- AI (movement, pathfinding)
- Collision (line traces, shape traces)

### Phase 3: Visual Effects (Week 3)
- Niagara (particle systems, parameters)
- Materials (dynamic instances, parameters)
- Camera (view targets, camera shakes)

### Phase 4: Advanced Systems (Week 4)
- Networking (authority, replication)
- UI (UMG widgets, HUD)
- Save/Load (game saves, persistence)
- Async (timers, delays, coroutines)

## Success Metrics

### Developer Experience
- **Zero imports needed** - stdlib is always available
- **IntelliSense works** - IDE knows all stdlib functions
- **Compile-time validation** - catch errors before runtime
- **Readable code** - looks like Python, runs like C++

### Performance
- **Zero overhead** - direct translation to native calls
- **No wrappers** - no virtual calls, no indirection
- **Optimal codegen** - same performance as hand-written C++

### Ecosystem
- **Marketplace-ready** - plugins compile to production quality
- **Team-friendly** - junior devs can be productive immediately
- **Maintainable** - clear, readable, self-documenting code

## The "Godmode v3" Promise

**Write games in KAIN. Ship them as native UE5 C++. No compromises.**

This is the transition from "Coder" to "Architect." You think in gameplay systems, not memory management. You write clean logic, the compiler handles the rest.

**This is what makes KAIN a true UE5 DSL.**
