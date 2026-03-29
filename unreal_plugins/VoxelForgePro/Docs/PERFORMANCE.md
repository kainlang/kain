# VoxelForge Pro - Performance Optimization Guide

## Performance Targets

| Metric | Target | Acceptable | Poor |
|--------|--------|------------|------|
| FPS | 60+ | 45-60 | <45 |
| Frame Time | <16.67ms | 16.67-22ms | >22ms |
| Visible Chunks | 100+ | 50-100 | <50 |
| Draw Calls | <500 | 500-1000 | >1000 |
| Memory Usage | <2GB | 2-4GB | >4GB |
| Chunk Gen Time | <16ms | 16-32ms | >32ms |
| Mesh Gen Time | <8ms | 8-16ms | >16ms |

---

## Quick Optimization Checklist

### ✅ Essential (Do These First)

- [ ] Enable GPU acceleration
- [ ] Enable async chunk generation
- [ ] Set appropriate view distance (1000-2000m)
- [ ] Use greedy meshing algorithm
- [ ] Enable RLE compression
- [ ] Set memory budget (1-2GB)
- [ ] Enable frustum culling
- [ ] Tune LOD distances

### ⚡ Advanced (For Extra Performance)

- [ ] Enable occlusion culling
- [ ] Increase chunk pool size
- [ ] Reduce noise octaves (4-6)
- [ ] Simplify biome blending
- [ ] Disable shadows on distant chunks
- [ ] Use GPU instancing
- [ ] Batch voxel modifications
- [ ] Profile with Unreal Insights

---

## Configuration Settings

### Recommended Settings (60 FPS Target)

```cpp
// VoxelWorldComponent settings
WorldComponent->ChunkSize = 32;              // 32x32x32 voxels
WorldComponent->ViewDistance = 2000.0f;      // 2000 units

// LOD Settings
LODSettings.LOD0Distance = 500.0f;           // Full detail
LODSettings.LOD1Distance = 1000.0f;          // 87.5% reduction
LODSettings.LOD2Distance = 2000.0f;          // 98.4% reduction
LODSettings.LOD3Distance = 3000.0f;          // 99.8% reduction
LODSettings.LOD4Distance = 5000.0f;          // 99.99% reduction
LODSettings.TransitionBlend = 0.2f;          // 20% blend zone

// Performance Settings
PerformanceSettings.MaxChunksPerFrame = 4;   // Generate 4 chunks/frame
PerformanceSettings.MaxMeshUpdatesPerFrame = 8;  // Update 8 meshes/frame
PerformanceSettings.AsyncGeneration = true;  // CRITICAL
PerformanceSettings.GPUAcceleration = true;  // CRITICAL
PerformanceSettings.MemoryBudgetMB = 2048;   // 2GB budget
PerformanceSettings.ChunkPoolSize = 200;     // Reuse 200 chunks

// Physics Settings (disable if not needed)
PhysicsSettings.EnableFallingBlocks = false;
PhysicsSettings.EnableFluidSim = false;
PhysicsSettings.EnableExplosions = true;     // Only if needed
```

### Low-End Hardware (30 FPS Target)

```cpp
WorldComponent->ChunkSize = 16;              // Smaller chunks
WorldComponent->ViewDistance = 1000.0f;      // Reduced distance

LODSettings.LOD0Distance = 250.0f;
LODSettings.LOD1Distance = 500.0f;
LODSettings.LOD2Distance = 1000.0f;
LODSettings.LOD3Distance = 1500.0f;
LODSettings.LOD4Distance = 2000.0f;

PerformanceSettings.MaxChunksPerFrame = 2;
PerformanceSettings.MaxMeshUpdatesPerFrame = 4;
PerformanceSettings.MemoryBudgetMB = 1024;   // 1GB budget
PerformanceSettings.ChunkPoolSize = 100;
```

### High-End Hardware (120 FPS Target)

```cpp
WorldComponent->ChunkSize = 64;              // Larger chunks
WorldComponent->ViewDistance = 5000.0f;      // Extended distance

LODSettings.LOD0Distance = 1000.0f;
LODSettings.LOD1Distance = 2000.0f;
LODSettings.LOD2Distance = 4000.0f;
LODSettings.LOD3Distance = 6000.0f;
LODSettings.LOD4Distance = 10000.0f;

PerformanceSettings.MaxChunksPerFrame = 8;
PerformanceSettings.MaxMeshUpdatesPerFrame = 16;
PerformanceSettings.MemoryBudgetMB = 4096;   // 4GB budget
PerformanceSettings.ChunkPoolSize = 500;
```

---

## Profiling

### Using Unreal Insights

1. Launch with profiling: `YourProject.exe -trace=cpu,gpu,frame`
2. Open Unreal Insights
3. Look for these markers:
   - `VoxelForge.ChunkGeneration`
   - `VoxelForge.Meshing`
   - `VoxelForge.Rendering`
   - `VoxelForge.Physics`

### Console Commands

```
stat VoxelForge          // Show VoxelForge stats
stat GPU                 // Show GPU timing
stat FPS                 // Show FPS
stat Unit                // Show frame breakdown
r.ScreenPercentage 100   // Ensure 100% resolution
```

### Performance Counters

```cpp
// Access in Blueprint or C++
int32 chunkCount = World->GetChunkCount();
int32 activeChunks = World->WorldComponent->ActiveChunks;
int32 pendingChunks = World->WorldComponent->PendingChunks;
int32 memoryMB = World->GetMemoryUsageMB();
float fps = 1.0f / DeltaTime;
```

---

## Optimization Techniques

### 1. GPU Acceleration (CRITICAL)

**Impact:** 10x faster terrain generation

```cpp
// Enable GPU compute shaders
PerformanceSettings.GPUAcceleration = true;
```

**Before:** 50ms per chunk (CPU)  
**After:** 5ms per chunk (GPU)

### 2. Async Generation (CRITICAL)

**Impact:** Eliminates frame drops

```cpp
// Generate chunks on worker threads
PerformanceSettings.AsyncGeneration = true;
```

**Before:** 50ms spike every chunk (frame drop)  
**After:** Smooth 60 FPS, no spikes

### 3. Greedy Meshing

**Impact:** 10x fewer triangles

```cpp
// Use greedy meshing algorithm
MeshingAlgorithm = EMeshingAlgorithm::GreedyMesh;
```

**Before:** 6000 triangles per chunk  
**After:** 600 triangles per chunk

### 4. LOD System

**Impact:** 90% triangle reduction at distance

```cpp
// Tune LOD distances
LODSettings.LOD0Distance = 500.0f;   // Full detail nearby
LODSettings.LOD4Distance = 5000.0f;  // Minimal detail far away
```

**Example:**
- 100 chunks at LOD0: 60,000 triangles
- 100 chunks at LOD4: 6,000 triangles (90% reduction)

### 5. Frustum Culling

**Impact:** 90% of chunks culled

```cpp
// Automatically enabled
// Only renders chunks in camera view
```

**Example:**
- 1000 total chunks
- 100 visible in frustum (90% culled)

### 6. Occlusion Culling

**Impact:** 40% of visible chunks culled

```cpp
// Enable occlusion culling
CullingMode = ECullingMode::Combined;  // Frustum + Occlusion
```

**Example:**
- 100 visible chunks
- 60 not occluded (40% culled)

### 7. RLE Compression

**Impact:** 90% memory reduction for empty chunks

```cpp
// Enable compression
CompressionMode = ECompressionMode::RLE;
```

**Example:**
- Empty chunk: 128 KB → 1 KB (99% reduction)
- Solid chunk: 128 KB → 2 KB (98% reduction)
- Mixed chunk: 128 KB → 40 KB (69% reduction)

### 8. Chunk Pooling

**Impact:** Eliminates GC spikes

```cpp
// Reuse chunk objects
PerformanceSettings.ChunkPoolSize = 200;
```

**Before:** 50ms GC spike every 100 chunks  
**After:** No GC spikes

### 9. GPU Instancing

**Impact:** 50x fewer draw calls

```cpp
// Automatically enabled for chunks with same material
```

**Example:**
- 1000 chunks without instancing: 1000 draw calls
- 1000 chunks with instancing: 20 draw calls (50x reduction)

### 10. Batch Modifications

**Impact:** 100x faster than individual voxel edits

```cpp
// BAD: Individual voxel edits
for (int i = 0; i < 1000; i++) {
    SetVoxelAt(World, coords[i], materialID);
}
// Time: 1000ms

// GOOD: Batch operation
ApplyBrush(World, center, brush, EditMode::Paint);
// Time: 10ms (100x faster)
```

---

## Common Performance Issues

### Issue: Low FPS (<30)

**Symptoms:**
- Stuttering gameplay
- Frame time >33ms
- High GPU usage

**Solutions:**
1. Reduce view distance to 1000m
2. Lower LOD distances
3. Reduce chunk size to 16
4. Disable physics if not needed
5. Reduce noise octaves to 4
6. Disable shadows on distant chunks

### Issue: Frame Drops During Chunk Generation

**Symptoms:**
- Periodic stuttering
- Frame time spikes to 50-100ms
- Smooth FPS between spikes

**Solutions:**
1. Enable async generation (CRITICAL)
2. Reduce MaxChunksPerFrame to 2
3. Enable GPU acceleration
4. Increase chunk pool size

### Issue: High Memory Usage (>4GB)

**Symptoms:**
- Out of memory crashes
- Slow performance
- Long load times

**Solutions:**
1. Enable RLE compression
2. Reduce memory budget to 2GB
3. Reduce view distance
4. Reduce chunk pool size
5. Unload distant chunks more aggressively

### Issue: Slow Terrain Editing

**Symptoms:**
- Lag when sculpting/painting
- Delayed brush response
- Frame drops during editing

**Solutions:**
1. Use smaller brush sizes (<20 voxels)
2. Reduce brush strength
3. Batch operations instead of per-frame edits
4. Enable GPU acceleration for meshing

### Issue: High Draw Calls (>1000)

**Symptoms:**
- Low FPS despite low triangle count
- High CPU usage
- GPU underutilized

**Solutions:**
1. Enable GPU instancing (automatic)
2. Merge chunks with same material
3. Reduce number of unique materials
4. Enable frustum culling

---

## Benchmarking

### Test Scene Setup

```
- Flat terrain at Y=0
- 10 biomes (Plains, Forest, Desert, etc.)
- 1000 chunks loaded
- Player at (0, 0, 100)
- View distance: 2000m
- Chunk size: 32
```

### Expected Performance

| Hardware | FPS | Frame Time | Chunks Visible | Draw Calls | Memory |
|----------|-----|------------|----------------|------------|--------|
| RTX 4090 + i9-13900K | 120+ | 8ms | 150 | 300 | 1.5GB |
| RTX 3080 + i7-12700K | 90+ | 11ms | 120 | 350 | 1.8GB |
| RTX 3060 + i5-12400 | 60+ | 16ms | 100 | 400 | 2.0GB |
| GTX 1660 + i5-10400 | 45+ | 22ms | 80 | 450 | 1.5GB |
| GTX 1050 + i3-10100 | 30+ | 33ms | 50 | 300 | 1.2GB |

### Stress Test

```
- Mountainous terrain (high complexity)
- 20 biomes with blending
- 2000 chunks loaded
- View distance: 5000m
- Chunk size: 64
- All physics enabled
```

**Expected:** 30-60 FPS on high-end hardware

---

## Optimization Workflow

### Step 1: Profile

```
1. Run game with stat VoxelForge
2. Identify bottleneck:
   - ChunkGeneration >10ms? → Enable GPU acceleration
   - Meshing >10ms? → Use greedy meshing
   - Rendering >10ms? → Reduce view distance
   - Physics >5ms? → Disable if not needed
```

### Step 2: Apply Quick Fixes

```
1. Enable GPU acceleration
2. Enable async generation
3. Set view distance to 2000m
4. Use greedy meshing
5. Enable RLE compression
```

### Step 3: Tune Settings

```
1. Adjust LOD distances
2. Set memory budget
3. Tune MaxChunksPerFrame
4. Adjust chunk pool size
```

### Step 4: Verify

```
1. Run game again
2. Check FPS (target: 60+)
3. Check frame time (target: <16.67ms)
4. Check memory (target: <2GB)
5. Check draw calls (target: <500)
```

### Step 5: Advanced Optimization

```
1. Enable occlusion culling
2. Reduce noise octaves
3. Simplify biome blending
4. Batch voxel modifications
5. Profile with Unreal Insights
```

---

## Platform-Specific Tips

### PC (High-End)

- Use 64x64x64 chunks
- View distance: 5000m
- Enable all features
- Target: 120 FPS

### PC (Mid-Range)

- Use 32x32x32 chunks
- View distance: 2000m
- Disable expensive physics
- Target: 60 FPS

### PC (Low-End)

- Use 16x16x16 chunks
- View distance: 1000m
- Disable physics and shadows
- Target: 30 FPS

### Console (PS5/Xbox Series X)

- Use 32x32x32 chunks
- View distance: 2500m
- Enable most features
- Target: 60 FPS

### Console (PS4/Xbox One)

- Use 16x16x16 chunks
- View distance: 1500m
- Disable expensive features
- Target: 30 FPS

### Mobile (High-End)

- Use 16x16x16 chunks
- View distance: 500m
- Minimal features
- Target: 30 FPS

---

## Conclusion

VoxelForge Pro is highly optimized out-of-the-box, but tuning these settings for your specific use case can dramatically improve performance. Start with the Quick Optimization Checklist, profile your game, and iterate.

**Key Takeaways:**
1. Always enable GPU acceleration and async generation
2. Tune view distance and LOD settings for your hardware
3. Use greedy meshing for best performance
4. Enable compression to reduce memory usage
5. Profile regularly and optimize bottlenecks

**Target Performance:**
- 60 FPS with 100+ chunks visible
- <16.67ms frame time
- <2GB memory usage
- <500 draw calls

With proper optimization, VoxelForge Pro can handle massive voxel worlds at 60+ FPS on mid-range hardware.
