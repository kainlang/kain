# VoxelForge Pro - Complete API Reference

## Blueprint Function Library

All functions are accessible from Blueprint and C++.

---

## World Management

### CreateVoxelWorld

```cpp
bool CreateVoxelWorld(int32 Seed, int32 ChunkSize, float ViewDistance)
```

**Description:** Initialize a new voxel world with specified parameters.

**Parameters:**
- `Seed` - Random seed for procedural generation (0-2147483647)
- `ChunkSize` - Size of each chunk in voxels (16, 32, or 64)
- `ViewDistance` - Maximum distance to load chunks (100-5000 units)

**Returns:** `true` if world created successfully

**Example (Blueprint):**
```
CreateVoxelWorld(12345, 32, 2000.0) → Success
```

**Example (C++):**
```cpp
bool success = UVoxelForgeFunctionLibrary::CreateVoxelWorld(12345, 32, 2000.0f);
```

---

### GetVoxelAt

```cpp
int32 GetVoxelAt(AVoxelWorld* World, FVoxelCoord Coord)
```

**Description:** Query the material ID of a voxel at specific coordinates.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Coord` - Voxel coordinate (X, Y, Z)

**Returns:** Material ID (0 = air, 1+ = material)

**Example:**
```cpp
FVoxelCoord coord = {100, 50, 25};
int32 materialID = UVoxelForgeFunctionLibrary::GetVoxelAt(World, coord);
```

---

### SetVoxelAt

```cpp
bool SetVoxelAt(AVoxelWorld* World, FVoxelCoord Coord, int32 MaterialID)
```

**Description:** Place or remove a voxel at specific coordinates.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Coord` - Voxel coordinate (X, Y, Z)
- `MaterialID` - Material to place (0 = remove, 1+ = material)

**Returns:** `true` if voxel was modified

**Example:**
```cpp
FVoxelCoord coord = {100, 50, 25};
bool success = UVoxelForgeFunctionLibrary::SetVoxelAt(World, coord, 1); // Place stone
```

---

### RaycastVoxel

```cpp
FVoxelRaycastHit RaycastVoxel(AVoxelWorld* World, FVector Start, FVector Direction, float MaxDistance)
```

**Description:** Perform a raycast against the voxel world.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Start` - Ray start position (world space)
- `Direction` - Ray direction (normalized)
- `MaxDistance` - Maximum ray distance

**Returns:** `FVoxelRaycastHit` struct with hit information

**Example:**
```cpp
FVector start = PlayerCamera->GetComponentLocation();
FVector direction = PlayerCamera->GetForwardVector();
FVoxelRaycastHit hit = UVoxelForgeFunctionLibrary::RaycastVoxel(World, start, direction, 1000.0f);

if (hit.Hit) {
    UE_LOG(LogTemp, Log, TEXT("Hit voxel at %s"), *hit.Position.ToString());
}
```

---

### GenerateChunk

```cpp
bool GenerateChunk(AVoxelWorld* World, FChunkCoord ChunkCoord)
```

**Description:** Generate a specific chunk at given coordinates.

**Parameters:**
- `World` - VoxelWorld actor reference
- `ChunkCoord` - Chunk coordinate (X, Y, Z)

**Returns:** `true` if chunk generation started

---

### UnloadChunk

```cpp
bool UnloadChunk(AVoxelWorld* World, FChunkCoord ChunkCoord)
```

**Description:** Unload a chunk from memory.

**Parameters:**
- `World` - VoxelWorld actor reference
- `ChunkCoord` - Chunk coordinate to unload

**Returns:** `true` if chunk was unloaded

---

### SaveWorld

```cpp
bool SaveWorld(AVoxelWorld* World, FString Filename)
```

**Description:** Save the entire voxel world to disk.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Filename` - Path to save file (relative to Saved/VoxelWorlds/)

**Returns:** `true` if save succeeded

**Example:**
```cpp
bool success = UVoxelForgeFunctionLibrary::SaveWorld(World, TEXT("MyWorld.voxel"));
```

---

### LoadWorld

```cpp
bool LoadWorld(AVoxelWorld* World, FString Filename)
```

**Description:** Load a voxel world from disk.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Filename` - Path to save file

**Returns:** `true` if load succeeded

---

## Terrain Editing

### ApplyBrush

```cpp
int32 ApplyBrush(AVoxelWorld* World, FVector Center, FBrushSettings Brush, EEditMode Mode)
```

**Description:** Apply a brush operation to the terrain.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Center` - Brush center position (world space)
- `Brush` - Brush settings (size, strength, falloff, shape)
- `Mode` - Edit mode (Sculpt, Smooth, Flatten, Paint, etc.)

**Returns:** Number of voxels modified

**Example:**
```cpp
FBrushSettings brush;
brush.Size = 10.0f;
brush.Strength = 0.5f;
brush.Falloff = 0.8f;
brush.Shape = EBrushShape::Sphere;

int32 modified = UVoxelForgeFunctionLibrary::ApplyBrush(
    World, 
    FVector(1000, 500, 100), 
    brush, 
    EEditMode::Sculpt
);
```

---

### SculptTerrain

```cpp
bool SculptTerrain(AVoxelWorld* World, FVector Center, float Radius, float Strength)
```

**Description:** Raise or lower terrain at a location.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Center` - Sculpt center (world space)
- `Radius` - Brush radius in voxels
- `Strength` - Sculpt strength (-1.0 to 1.0, negative = lower, positive = raise)

**Returns:** `true` if terrain was modified

---

### SmoothTerrain

```cpp
bool SmoothTerrain(AVoxelWorld* World, FVector Center, float Radius, int32 Iterations)
```

**Description:** Smooth terrain by averaging neighboring voxels.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Center` - Smooth center (world space)
- `Radius` - Brush radius
- `Iterations` - Number of smoothing passes (1-10)

**Returns:** `true` if terrain was smoothed

---

### FlattenTerrain

```cpp
bool FlattenTerrain(AVoxelWorld* World, FVector Center, float Radius, float TargetHeight)
```

**Description:** Flatten terrain to a specific height.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Center` - Flatten center (world space)
- `Radius` - Brush radius
- `TargetHeight` - Target height (world Z coordinate)

**Returns:** `true` if terrain was flattened

---

### PaintMaterial

```cpp
bool PaintMaterial(AVoxelWorld* World, FVector Center, float Radius, int32 MaterialID)
```

**Description:** Paint voxel materials in a radius.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Center` - Paint center (world space)
- `Radius` - Brush radius
- `MaterialID` - Material to paint (1-20)

**Returns:** `true` if materials were painted

---

### CarveCave

```cpp
bool CarveCave(AVoxelWorld* World, FVector Start, FVector End, float Radius)
```

**Description:** Carve a cave tunnel between two points.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Start` - Cave start position (world space)
- `End` - Cave end position (world space)
- `Radius` - Tunnel radius

**Returns:** `true` if cave was carved

**Example:**
```cpp
FVector start = FVector(0, 0, 0);
FVector end = FVector(1000, 500, -200);
bool success = UVoxelForgeFunctionLibrary::CarveCave(World, start, end, 50.0f);
```

---

### PlaceStructure

```cpp
bool PlaceStructure(AVoxelWorld* World, FVector Position, FStructureTemplate Structure)
```

**Description:** Place a pre-built structure at a location.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Position` - Structure position (world space)
- `Structure` - Structure template (from DataTable)

**Returns:** `true` if structure was placed

---

### SpawnTree

```cpp
bool SpawnTree(AVoxelWorld* World, FVector Position, FTreeVariant TreeVariant)
```

**Description:** Spawn a procedural tree at a location.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Position` - Tree base position (world space)
- `TreeVariant` - Tree type and parameters (from DataTable)

**Returns:** `true` if tree was spawned

---

## Noise Generation

### SampleNoise3D

```cpp
float SampleNoise3D(FVector Position, FNoiseParams Params)
```

**Description:** Sample 3D noise at a position.

**Parameters:**
- `Position` - Sample position (world space)
- `Params` - Noise parameters (frequency, amplitude, octaves, etc.)

**Returns:** Noise value (-1.0 to 1.0)

---

### GeneratePerlinNoise

```cpp
float GeneratePerlinNoise(FVector Position, float Frequency, float Amplitude)
```

**Description:** Generate Perlin noise at a position.

**Parameters:**
- `Position` - Sample position
- `Frequency` - Noise frequency (0.001-1.0)
- `Amplitude` - Noise amplitude (0.1-10.0)

**Returns:** Noise value

---

### GenerateSimplexNoise

```cpp
float GenerateSimplexNoise(FVector Position, float Frequency, int32 Seed)
```

**Description:** Generate simplex noise (faster than Perlin).

**Parameters:**
- `Position` - Sample position
- `Frequency` - Noise frequency
- `Seed` - Random seed

**Returns:** Noise value

---

### GenerateWorleyNoise

```cpp
float GenerateWorleyNoise(FVector Position, float Frequency, float Jitter)
```

**Description:** Generate Worley/Voronoi noise (cellular patterns).

**Parameters:**
- `Position` - Sample position
- `Frequency` - Cell frequency
- `Jitter` - Cell randomness (0.0-1.0)

**Returns:** Distance to nearest cell

---

### GenerateFractalNoise

```cpp
float GenerateFractalNoise(FVector Position, FNoiseParams Params)
```

**Description:** Generate fractal noise (multiple octaves).

**Parameters:**
- `Position` - Sample position
- `Params` - Noise parameters (frequency, octaves, lacunarity, persistence)

**Returns:** Noise value

---

## Biome System

### CalculateBiome

```cpp
EBiomeType CalculateBiome(AVoxelWorld* World, FVector Position)
```

**Description:** Determine the biome at a world position.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Position` - World position

**Returns:** Biome type enum

---

### BlendBiomes

```cpp
EBiomeType BlendBiomes(AVoxelWorld* World, FVector Position, float BlendDistance)
```

**Description:** Blend multiple biomes at a position.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Position` - World position
- `BlendDistance` - Blend radius

**Returns:** Dominant biome type

---

### GetBiomeInfo

```cpp
FBiomeDefinition GetBiomeInfo(EBiomeType BiomeType)
```

**Description:** Get biome definition from DataTable.

**Parameters:**
- `BiomeType` - Biome type enum

**Returns:** Biome definition struct

---

## Physics

### EnablePhysics

```cpp
bool EnablePhysics(AVoxelWorld* World, FVoxelCoord Coord, EPhysicsType PhysicsType)
```

**Description:** Enable physics for a voxel.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Coord` - Voxel coordinate
- `PhysicsType` - Physics type (Static, Falling, Fluid, Explosive)

**Returns:** `true` if physics enabled

---

### SimulateFallingBlocks

```cpp
int32 SimulateFallingBlocks(AVoxelWorld* World, float DeltaTime)
```

**Description:** Simulate falling block physics (sand, gravel).

**Parameters:**
- `World` - VoxelWorld actor reference
- `DeltaTime` - Time step

**Returns:** Number of blocks that fell

---

### SimulateFluid

```cpp
int32 SimulateFluid(AVoxelWorld* World, float DeltaTime, float Viscosity)
```

**Description:** Simulate fluid flow (water, lava).

**Parameters:**
- `World` - VoxelWorld actor reference
- `DeltaTime` - Time step
- `Viscosity` - Fluid viscosity (0.0-1.0)

**Returns:** Number of fluid voxels updated

---

### TriggerExplosion

```cpp
int32 TriggerExplosion(AVoxelWorld* World, FVector Center, float Radius, float Strength)
```

**Description:** Destroy voxels in an explosion radius.

**Parameters:**
- `World` - VoxelWorld actor reference
- `Center` - Explosion center (world space)
- `Radius` - Explosion radius
- `Strength` - Destruction strength (0.0-1.0)

**Returns:** Number of voxels destroyed

---

## Utilities

### WorldToVoxelCoord

```cpp
FVoxelCoord WorldToVoxelCoord(FVector WorldPosition, int32 ChunkSize)
```

**Description:** Convert world position to voxel coordinate.

**Parameters:**
- `WorldPosition` - Position in world space
- `ChunkSize` - Chunk size (32)

**Returns:** Voxel coordinate

---

### VoxelToWorldCoord

```cpp
FVector VoxelToWorldCoord(FVoxelCoord VoxelCoord, int32 ChunkSize)
```

**Description:** Convert voxel coordinate to world position.

**Parameters:**
- `VoxelCoord` - Voxel coordinate
- `ChunkSize` - Chunk size

**Returns:** World position

---

### IsChunkLoaded

```cpp
bool IsChunkLoaded(AVoxelWorld* World, FChunkCoord ChunkCoord)
```

**Description:** Check if a chunk is currently loaded.

**Parameters:**
- `World` - VoxelWorld actor reference
- `ChunkCoord` - Chunk coordinate

**Returns:** `true` if chunk is loaded

---

### GetLoadedChunks

```cpp
TArray<FChunkCoord> GetLoadedChunks(AVoxelWorld* World)
```

**Description:** Get list of all loaded chunks.

**Parameters:**
- `World` - VoxelWorld actor reference

**Returns:** Array of chunk coordinates

---

### GetWorldStatistics

```cpp
FString GetWorldStatistics(AVoxelWorld* World)
```

**Description:** Get performance statistics as formatted string.

**Parameters:**
- `World` - VoxelWorld actor reference

**Returns:** Statistics string (chunk count, memory usage, FPS, etc.)

**Example Output:**
```
Chunks: 156 loaded, 23 generating
Memory: 104 MB (15% of budget)
FPS: 62.3 (16.04ms per frame)
Draw Calls: 312
Triangles: 1,245,678
```

---

## C++ API

### AVoxelWorld Class

```cpp
class VOXELFORGE_API AVoxelWorld : public AActor
{
public:
    // Components
    UPROPERTY(Replicated)
    UVoxelWorldComponent* WorldComponent;
    
    UPROPERTY(Replicated)
    UChunkStreamerComponent* ChunkStreamer;
    
    UPROPERTY(Replicated)
    UBiomeGeneratorComponent* BiomeGenerator;
    
    // Methods
    UFUNCTION(BlueprintCallable)
    void BeginPlay();
    
    UFUNCTION(BlueprintCallable)
    void Tick(float DeltaTime);
    
    UFUNCTION(Server, Reliable)
    void Server_SetVoxel(FVoxelCoord Coord, int32 MaterialID);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_VoxelChanged(FVoxelCoord Coord, int32 MaterialID);
};
```

### UVoxelWorldComponent Class

```cpp
class VOXELFORGE_API UVoxelWorldComponent : public UActorComponent
{
public:
    UPROPERTY(Replicated, EditDefaultsOnly)
    int32 WorldSeed;
    
    UPROPERTY(Replicated, EditDefaultsOnly)
    int32 ChunkSize;
    
    UPROPERTY(EditDefaultsOnly)
    float ViewDistance;
    
    UPROPERTY(EditDefaultsOnly)
    FLODSettings LODSettings;
    
    UPROPERTY(EditDefaultsOnly)
    FPerformanceSettings PerformanceSettings;
    
    UPROPERTY(Transient)
    int32 ActiveChunks;
    
    UPROPERTY(Transient)
    int32 PendingChunks;
};
```

---

## Data Structures

### FVoxelCoord

```cpp
struct FVoxelCoord
{
    int32 X;
    int32 Y;
    int32 Z;
};
```

### FChunkCoord

```cpp
struct FChunkCoord
{
    int32 X;
    int32 Y;
    int32 Z;
};
```

### FBrushSettings

```cpp
struct FBrushSettings
{
    float Size;              // 1-100
    float Strength;          // 0-1
    float Falloff;           // 0-1
    EBrushShape Shape;       // Sphere, Cube, Cylinder, Cone
    bool SymmetryX;
    bool SymmetryY;
    bool SymmetryZ;
    bool Invert;
};
```

### FNoiseParams

```cpp
struct FNoiseParams
{
    float Frequency;         // 0.001-1.0
    float Amplitude;         // 0.1-10.0
    int32 Octaves;           // 1-8
    float Lacunarity;        // 1.5-3.0
    float Persistence;       // 0.3-0.7
    int32 Seed;
    ENoiseType NoiseType;    // Perlin, Simplex, Worley, Fractal
};
```

---

## Enums

### EBiomeType

```cpp
enum class EBiomeType : uint8
{
    Plains,
    Forest,
    Desert,
    Mountains,
    Ocean,
    Tundra,
    Jungle,
    Swamp,
    Beach,
    Savanna,
    Cave,
    Underground
};
```

### EEditMode

```cpp
enum class EEditMode : uint8
{
    Sculpt,
    Smooth,
    Flatten,
    Paint,
    Erosion,
    Noise,
    Stamp,
    Cave,
    Carve,
    Grow
};
```

### EMaterialType

```cpp
enum class EMaterialType : uint8
{
    Stone,
    Dirt,
    Grass,
    Sand,
    Gravel,
    Water,
    Lava,
    Wood,
    Leaves,
    Snow,
    Ice,
    Clay,
    IronOre,
    GoldOre,
    DiamondOre,
    Glass,
    Brick,
    Concrete,
    Metal,
    Obsidian
};
```

---

## Events

### OnChunkGenerated

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkGenerated, FChunkCoord, ChunkCoord);
```

**Description:** Fired when a chunk finishes generating.

### OnVoxelChanged

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoxelChanged, FVoxelCoord, Coord, int32, MaterialID);
```

**Description:** Fired when a voxel is placed or removed.

### OnWorldSaved

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldSaved, FString, Filename);
```

**Description:** Fired when world save completes.

---

## Performance Tips

1. **Batch Operations** - Use `ApplyBrush` instead of multiple `SetVoxelAt` calls
2. **Async Generation** - Enable `PerformanceSettings.AsyncGeneration = true`
3. **GPU Acceleration** - Enable `PerformanceSettings.GPUAcceleration = true`
4. **LOD Distances** - Tune `LODSettings` for your view distance
5. **Memory Budget** - Set `PerformanceSettings.MemoryBudgetMB` appropriately
6. **Chunk Pooling** - Increase `PerformanceSettings.ChunkPoolSize` for less GC

---

## Complete Example

```cpp
// Create voxel world
AVoxelWorld* World = GetWorld()->SpawnActor<AVoxelWorld>();
World->WorldComponent->WorldSeed = 12345;
World->WorldComponent->ChunkSize = 32;
World->WorldComponent->ViewDistance = 2000.0f;

// Generate terrain
UVoxelForgeFunctionLibrary::GenerateTerrain(
    World, 
    EBiomeType::Plains, 
    DefaultNoiseParams
);

// Sculpt terrain
FBrushSettings brush;
brush.Size = 20.0f;
brush.Strength = 0.8f;
brush.Shape = EBrushShape::Sphere;

UVoxelForgeFunctionLibrary::SculptTerrain(
    World,
    FVector(1000, 500, 100),
    brush.Size,
    brush.Strength
);

// Place tree
FTreeVariant oakTree = GetTreeVariant(TEXT("Oak"));
UVoxelForgeFunctionLibrary::SpawnTree(
    World,
    FVector(1050, 550, 100),
    oakTree
);

// Save world
UVoxelForgeFunctionLibrary::SaveWorld(World, TEXT("MyWorld.voxel"));
```
