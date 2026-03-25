# Concrete Things to Steal from unreal-rust

## 1. Hot Reload via DLL Timestamping ⭐⭐⭐

**Their Implementation:**
```cpp
// RustPlugin.cpp
FString LocalTargetPath = FPaths::Combine(
    *PluginFolderPath(),
    *FString::Printf(TEXT("%s-%i"), *PluginFileName(),
    FDateTime::Now().ToUnixTimestamp())
);

// Copy DLL with timestamp
FPlatformFileManager::Get().GetPlatformFile().CopyFile(*LocalTargetPath, *Path);

// Load new DLL
void* LocalHandle = FPlatformProcess::GetDllHandle(*LocalTargetPath);
```

**Why It Works:**
- Thread Local Storage (TLS) prevents normal DLL unloading
- Timestamped copies force OS to load new DLL
- Old DLLs leak memory but that's acceptable for dev

**How We Implement:**
```rust
// In kain-pro watch mode
fn watch_and_reload(config: &Config) {
    let watcher = notify::watcher(tx, Duration::from_secs(1))?;
    watcher.watch(&config.source_dir, RecursiveMode::Recursive)?;
    
    loop {
        match rx.recv() {
            Ok(event) => {
                // Recompile
                compile_project(config)?;
                
                // Copy to timestamped location
                let timestamp = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
                let output = format!("{}-{}.dll", config.plugin_name, timestamp);
                fs::copy(&compiled_dll, &output)?;
                
                println!("✅ Hot reload ready: {}", output);
            }
        }
    }
}
```

**Benefit:** Instant iteration without editor restart

---

## 2. File Watcher with Debouncing ⭐⭐

**Their Implementation:**
```cpp
// Register directory watcher
IDirectoryWatcher* watcher = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(
    TEXT("DirectoryWatcher")).Get();
    
watcher->RegisterDirectoryChangedCallback_Handle(
    *Plugin.PluginFolderPath(),
    IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FRustPluginModule::OnProjectDirectoryChanged),
    WatcherHandle,
    IDirectoryWatcher::WatchOptions::IgnoreChangesInSubtree
);

void FRustPluginModule::OnProjectDirectoryChanged(const TArray<FFileChangeData>& Data) {
    for (FFileChangeData Changed : Data) {
        if (Name == TEXT("rustplugin") && Ext == *PlatformExtensionName() && ChangedOrAdded) {
            Plugin.TryLoad();
            UE_LOG(LogTemp, Display, TEXT("Hotreload: Rust"));
        }
    }
}
```

**How We Implement:**
```rust
// In kain-pro
use notify::{Watcher, RecursiveMode, watcher};

fn watch_kain_files(path: &Path) -> Result<()> {
    let (tx, rx) = channel();
    let mut watcher = watcher(tx, Duration::from_millis(500))?; // 500ms debounce
    
    watcher.watch(path, RecursiveMode::Recursive)?;
    
    let mut last_compile = Instant::now();
    
    for event in rx {
        // Debounce: only compile if 500ms since last compile
        if last_compile.elapsed() < Duration::from_millis(500) {
            continue;
        }
        
        match event {
            DebouncedEvent::Write(path) | DebouncedEvent::Create(path) => {
                if path.extension() == Some(OsStr::new("kn")) {
                    println!("🔄 Detected change: {:?}", path);
                    compile_and_reload()?;
                    last_compile = Instant::now();
                }
            }
            _ => {}
        }
    }
    Ok(())
}
```

**Benefit:** Automatic recompilation on save

---

## 3. UUID-Based Component Registration ⭐⭐⭐

**Their Pattern:**
```rust
#[derive(Debug, Component)]
#[uuid = "b6addc7d-03b1-4b06-9328-f26c71997ee6"]
#[reflect(editor)]
pub struct PlaySoundOnImpactComponent {
    pub sound: USound,
}
```

**Why It's Brilliant:**
- Stable IDs across recompiles
- Editor can track components by UUID
- No name collisions
- Serialization-friendly

**How We Implement:**
```kain
// In KAIN source
@component
@uuid("b6addc7d-03b1-4b06-9328-f26c71997ee6")
struct PlaySoundOnImpact:
    sound: SoundAsset
```

**Generated C++:**
```cpp
// Auto-generate UUID if not specified
UCLASS(meta=(UUID="b6addc7d-03b1-4b06-9328-f26c71997ee6"))
class UPlaySoundOnImpactComponent : public UActorComponent {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* Sound;
    
    // UUID for serialization/hot-reload
    static constexpr FGuid ComponentUUID = 
        FGuid(0xb6addc7d, 0x03b14b06, 0x9328f26c, 0x71997ee6);
};
```

**Benefit:** Stable component identity across hot reloads

---

## 4. Editor Component Reflection ⭐⭐

**Their Pattern:**
```rust
#[reflect(editor)]  // Makes component editable in editor
pub struct CameraComponent {
    pub x: f32,
    pub y: f32,
    #[reflect(skip)]  // Skip this field
    pub mode: CameraMode,
}
```

**Generated Reflection Code:**
```rust
impl ReflectDyn for CameraComponentReflect {
    fn name(&self) -> &'static str { "CameraComponent" }
    fn number_of_fields(&self) -> u32 { 2 }
    
    fn get_field_name(&self, idx: u32) -> Option<&'static str> {
        match idx {
            0 => Some("x"),
            1 => Some("y"),
            _ => None
        }
    }
    
    fn get_field_type(&self, idx: u32) -> Option<ReflectType> {
        match idx {
            0 => Some(ReflectType::Float),
            1 => Some(ReflectType::Float),
            _ => None
        }
    }
}
```

**How We Implement:**
```kain
@component
@editor  // Enable editor reflection
struct CameraComponent:
    @editable
    x: Float
    
    @editable
    y: Float
    
    @transient  // Skip serialization/reflection
    mode: CameraMode
```

**Generated C++:**
```cpp
UCLASS()
class UCameraComponent : public UActorComponent {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float X;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float Y;
    
    UPROPERTY(Transient)  // Not serialized, not in editor
    ECameraMode Mode;
    
    // Reflection metadata for hot reload
    static TArray<FFieldMetadata> GetFieldMetadata() {
        return {
            {"X", EFieldType::Float, offsetof(UCameraComponent, X)},
            {"Y", EFieldType::Float, offsetof(UCameraComponent, Y)},
        };
    }
};
```

**Benefit:** Components editable in UE5 editor with hot reload support

---

## 5. cbindgen for C Header Generation ⭐

**Their Build Script:**
```rust
// build.rs
cbindgen::Builder::new()
    .with_crate(crate_dir)
    .include_item("UnrealBindings")
    .include_item("RustBindings")
    .with_pragma_once(true)
    .generate()
    .expect("Unable to generate bindings")
    .write_to_file("../RustPlugin/Source/RustPlugin/Public/Bindings.h");
```

**How We Use This:**
```rust
// In kain-pro codegen
// Generate C++ headers from KAIN AST
fn generate_cpp_header(ast: &Program) -> String {
    let mut header = String::new();
    header.push_str("#pragma once\n\n");
    
    // Forward declarations
    for type_def in &ast.types {
        header.push_str(&format!("class {};\n", type_def.cpp_name()));
    }
    
    // Full definitions
    for type_def in &ast.types {
        header.push_str(&generate_class_definition(type_def));
    }
    
    header
}
```

**Benefit:** Automated header generation (we already do this!)

---

## 6. Panic Catching for Robustness ⭐

**Their Pattern:**
```rust
pub extern "C" fn tick(dt: f32) -> ResultCode {
    let r = std::panic::catch_unwind(|| unsafe {
        UnrealCore::tick(&mut MODULE.as_mut().unwrap().core, dt);
    });
    match r {
        Ok(_) => ResultCode::Success,
        Err(_) => ResultCode::Panic,
    }
}
```

**How We Apply This:**
```kain
// In KAIN, we can add optional error handling
@blueprint
@safe  // Generate try-catch wrapper
fn calculate_damage(base: Float, armor: Float) -> Float:
    if armor < 0.0:
        error("Armor cannot be negative")
        return 0.0
    return base * (1.0 - armor / 100.0)
```

**Generated C++:**
```cpp
UFUNCTION(BlueprintCallable, Category="Combat")
static float CalculateDamage(float Base, float Armor) {
    try {
        if (Armor < 0.0f) {
            UE_LOG(LogTemp, Error, TEXT("Armor cannot be negative"));
            return 0.0f;
        }
        return Base * (1.0f - Armor / 100.0f);
    } catch (const std::exception& e) {
        UE_LOG(LogTemp, Error, TEXT("Exception in CalculateDamage: %s"), 
               UTF8_TO_TCHAR(e.what()));
        return 0.0f;
    }
}
```

**Benefit:** Graceful error handling in Blueprint-callable functions

---

## 7. Component Registration Macro ⭐⭐

**Their Pattern:**
```rust
register_components! {
    CharacterSoundsComponent,
    PlaySoundOnImpactComponent,
    CameraComponent,
    => module
};
```

**Expands to:**
```rust
module.register::<CharacterSoundsComponent>();
module.register::<PlaySoundOnImpactComponent>();
module.register::<CameraComponent>();
```

**How We Apply This:**
```kain
// In KAIN.toml
[ue5]
auto_register = true  # Automatically register all components

# Or explicit:
components = [
    "CharacterSoundsComponent",
    "PlaySoundOnImpactComponent",
    "CameraComponent"
]
```

**Generated C++ (in module startup):**
```cpp
void FMyPluginModule::StartupModule() {
    // Auto-register all components
    RegisterComponent<UCharacterSoundsComponent>();
    RegisterComponent<UPlaySoundOnImpactComponent>();
    RegisterComponent<UCameraComponent>();
}
```

**Benefit:** Automatic component registration (we already do this!)

---

## 8. Editor-Only Components ⭐⭐

**Their Pattern:**
```rust
#[reflect(editor)]  // Only exists in editor, stripped in shipping
pub struct DebugVisualizationComponent {
    pub show_bounds: bool,
    pub show_velocity: bool,
}
```

**How We Implement:**
```kain
@component
@editor_only  // Stripped in shipping builds
struct DebugVisualization:
    show_bounds: Bool
    show_velocity: Bool
```

**Generated C++:**
```cpp
#if WITH_EDITOR
UCLASS()
class UDebugVisualizationComponent : public UActorComponent {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere)
    bool ShowBounds;
    
    UPROPERTY(EditAnywhere)
    bool ShowVelocity;
};
#endif
```

**Benefit:** Debug components don't bloat shipping builds

---

## 9. Sound/Asset References ⭐⭐⭐

**Their Pattern:**
```rust
pub struct PlaySoundOnImpactComponent {
    pub sound: USound,  // Type-safe asset reference
}

// Usage:
play_sound_at_location(
    sound.sound,
    transform.position,
    transform.rotation,
    &SoundSettings::default()
)
```

**How We Implement:**
```kain
@component
struct PlaySoundOnImpact:
    sound: SoundAsset  # Type-safe asset reference
    
@blueprint
fn play_impact_sound(component: PlaySoundOnImpact, location: Vec3):
    play_sound_at_location(component.sound, location)
```

**Generated C++:**
```cpp
UCLASS()
class UPlaySoundOnImpactComponent : public UActorComponent {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    USoundBase* Sound;  // Type-safe!
};

UFUNCTION(BlueprintCallable, Category="Audio")
static void PlayImpactSound(
    UPlaySoundOnImpactComponent* Component,
    FVector Location
) {
    UGameplayStatics::PlaySoundAtLocation(
        Component->GetWorld(),
        Component->Sound,
        Location
    );
}
```

**Benefit:** Type-safe asset references (we should add this!)

---

## 10. Query-Based System Architecture ⭐

**Their Pattern:**
```rust
fn update_camera(
    mut query: Query<(Entity, &ParentComponent, &CameraComponent)>,
    mut spatial_query: Query<&mut TransformComponent>,
) {
    for (entity, parent, camera) in query.iter_mut() {
        // Access parent's transform
        let parent_transform = spatial_query.get(parent.parent)?;
        
        // Update camera position
        let mut camera_transform = spatial_query.get_mut(entity)?;
        camera_transform.position = parent_transform.position + offset;
    }
}
```

**How We Could Apply This:**
```kain
// KAIN doesn't need ECS, but we can generate efficient iteration
@system
fn update_camera():
    # Iterate all actors with CameraComponent
    for camera in get_all<CameraComponent>():
        if camera.parent:
            parent_pos = camera.parent.get_position()
            camera.set_position(parent_pos + camera.offset)
```

**Generated C++:**
```cpp
void UpdateCamera() {
    // Efficient iteration over components
    for (TObjectIterator<UCameraComponent> It; It; ++It) {
        UCameraComponent* Camera = *It;
        if (Camera->Parent) {
            FVector ParentPos = Camera->Parent->GetActorLocation();
            Camera->GetOwner()->SetActorLocation(ParentPos + Camera->Offset);
        }
    }
}
```

**Benefit:** Efficient component iteration patterns

---

## Implementation Priority

### Week 1: Hot Reload Foundation
1. ✅ Add file watcher to `kain-pro` (notify crate)
2. ✅ Implement DLL timestamping
3. ✅ Add `-w` watch mode flag
4. ✅ Test hot reload in UE5

### Week 2: UUID System
1. ✅ Add `@uuid` attribute support
2. ✅ Auto-generate UUIDs if not specified
3. ✅ Emit UUID metadata in generated C++
4. ✅ Use UUIDs for hot reload tracking

### Week 3: Editor Reflection
1. ✅ Add `@editor` attribute
2. ✅ Generate reflection metadata
3. ✅ Support `@editable` and `@transient`
4. ✅ Test in UE5 editor

### Week 4: Asset References
1. ✅ Add asset reference types (SoundAsset, TextureAsset, etc.)
2. ✅ Generate type-safe UPROPERTY declarations
3. ✅ Add asset picker support in editor
4. ✅ Test asset loading

---

## Code to Add to KAIN

### 1. Watch Mode in kain-pro
```rust
// src/main.rs
#[derive(Parser)]
struct Args {
    #[arg(short, long)]
    watch: bool,
}

fn main() {
    let args = Args::parse();
    
    if args.watch {
        watch_and_reload(&config)?;
    } else {
        compile_once(&config)?;
    }
}
```

### 2. UUID Attribute
```rust
// src/parser.rs
#[derive(Debug, Clone)]
pub enum Attribute {
    Component,
    DataTable,
    Blueprint,
    Replicated,
    Uuid(String),  // NEW!
    Editor,        // NEW!
}
```

### 3. Asset Reference Types
```rust
// src/types.rs
pub enum Type {
    // ... existing types
    AssetReference(AssetType),  // NEW!
}

pub enum AssetType {
    Sound,
    Texture,
    Material,
    StaticMesh,
    SkeletalMesh,
    Animation,
    Particle,
}
```

### 4. Reflection Metadata Codegen
```rust
// src/codegen/ue5.rs
fn generate_reflection_metadata(component: &Struct) -> String {
    format!(r#"
    static TArray<FFieldMetadata> GetFieldMetadata() {{
        return {{
            {}
        }};
    }}
    "#, component.fields.iter().map(|f| {
        format!(r#"{{"{}",  EFieldType::{}, offsetof({}, {})}}"#,
            f.name, f.type_name(), component.cpp_name(), f.cpp_name())
    }).collect::<Vec<_>>().join(",\n"))
}
```

---

## Summary: What to Steal

**Must Have (Week 1-2):**
1. ✅ Hot reload via DLL timestamping
2. ✅ File watcher with debouncing
3. ✅ UUID-based component tracking

**Should Have (Week 3-4):**
4. ✅ Editor reflection metadata
5. ✅ Asset reference types
6. ✅ Editor-only components

**Nice to Have (Future):**
7. ⚠️ Panic catching (less relevant for compile-time)
8. ⚠️ Query-based iteration (UE5 has TObjectIterator)
9. ⚠️ Custom Blueprint nodes (advanced)

**Don't Need:**
- ❌ FFI bridge (we generate native C++)
- ❌ ECS runtime (UE5 has its own)
- ❌ Bevy integration (different architecture)

**The Big Win: Hot Reload**

This is their killer feature and we should absolutely steal it. Combined with our compile-time codegen, we get:
- Instant iteration (hot reload)
- Zero runtime overhead (native C++)
- Production-ready output (no FFI)

**Best of both worlds!** 🚀
