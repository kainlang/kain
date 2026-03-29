# VoxelForge Pro - Build Instructions

## Prerequisites

1. **KAIN Compiler** - Ensure `kain.exe` is in your PATH
2. **Unreal Engine 5.4+** - Installed and configured
3. **Visual Studio 2022** - With C++ game development workload

---

## Quick Build

### Windows

```batch
# Run the build script
Build5.4.bat
```

This will:
1. Clean previous build
2. Compile voxelforge.kn to C++
3. Generate plugin structure
4. Verify output

### Manual Build

```batch
# Clean
rmdir /s /q VoxelForgePro

# Build
kain build --ue5

# Verify
dir VoxelForgePro\Source\VoxelForgePro\Public\*.h
dir VoxelForgePro\Source\VoxelForgePro\Private\*.cpp
dir VoxelForgePro\Shaders\*.usf
```

---

## Installation

### Step 1: Copy Plugin

```
Copy VoxelForgePro folder to:
YourProject/Plugins/VoxelForgePro/
```

### Step 2: Regenerate Project Files

```
Right-click YourProject.uproject
→ Generate Visual Studio project files
```

### Step 3: Compile

```
Open YourProject.sln in Visual Studio
Build → Build Solution (Ctrl+Shift+B)
```

### Step 4: Enable Plugin

```
Launch Unreal Editor
Edit → Plugins
Search "VoxelForge Pro"
Check "Enabled"
Restart Editor
```

---

## Verification

### Check Plugin Loaded

```
Window → VoxelForge → Open VoxelForge Editor
```

If menu appears, plugin is loaded successfully!

### Test in Level

1. Drag `VoxelWorld` actor into level
2. Set World Seed, Chunk Size, View Distance
3. Play in editor
4. World should generate automatically

---

## Expected Output

### File Count

- **Headers:** 50+ .h files
- **Source:** 50+ .cpp files
- **Shaders:** 19+ .usf files
- **Total Lines:** ~15,000 lines of generated C++

### Generated Files

```
VoxelForgePro/
├── VoxelForgePro.uplugin
├── Resources/
│   └── Icon128.png
├── Source/
│   └── VoxelForgePro/
│       ├── VoxelForgePro.Build.cs
│       ├── Public/
│       │   ├── VoxelWorld.h
│       │   ├── VoxelChunk.h
│       │   ├── VoxelPlayer.h
│       │   ├── VoxelWorldComponent.h
│       │   ├── VoxelChunkComponent.h
│       │   ├── VoxelPlayerComponent.h
│       │   ├── VoxelPhysicsComponent.h
│       │   ├── VoxelLightingComponent.h
│       │   ├── BiomeGeneratorComponent.h
│       │   ├── StructureSpawnerComponent.h
│       │   ├── ChunkStreamerComponent.h
│       │   ├── VoxelCollisionComponent.h
│       │   ├── VoxelNetworkComponent.h
│       │   ├── VoxelForgeFunctionLibrary.h
│       │   ├── VoxelForgeTypes.h
│       │   ├── VoxelToolPalette.h
│       │   ├── BrushSettingsPanel.h
│       │   ├── MaterialPicker.h
│       │   ├── BiomePainter.h
│       │   ├── NoisePreview.h
│       │   ├── ChunkDebugger.h
│       │   ├── PerformanceMonitor.h
│       │   ├── GenerationSettings.h
│       │   ├── StructureLibrary.h
│       │   ├── VoxelInspector.h
│       │   ├── WorldSettings.h
│       │   ├── ExportImporter.h
│       │   ├── TerrainPresets.h
│       │   ├── VoxelWorldDetails.h
│       │   ├── VoxelChunkDetails.h
│       │   ├── VoxelMaterialDetails.h
│       │   ├── BiomeDetails.h
│       │   ├── BrushDetails.h
│       │   ├── VoxelEditorViewport.h
│       │   ├── NoisePreviewViewport.h
│       │   ├── StructurePreviewViewport.h
│       │   ├── VoxelEditingToolbar.h
│       │   ├── VoxelGenerationToolbar.h
│       │   ├── VoxelForgeAssetEditor.h
│       │   └── VoxelForgeEditorModule.h
│       └── Private/
│           ├── VoxelWorld.cpp
│           ├── VoxelChunk.cpp
│           ├── VoxelPlayer.cpp
│           ├── VoxelStructure.cpp
│           ├── VoxelProjectile.cpp
│           ├── VoxelForgeFunctionLibrary.cpp
│           └── ... (50+ more .cpp files)
└── Shaders/
    ├── PerlinNoise3D.usf
    ├── SimplexNoise3D.usf
    ├── WorleyNoise3D.usf
    ├── FractalNoise.usf
    ├── BiomeBlending.usf
    ├── GreedyMeshing.usf
    ├── MarchingCubes.usf
    ├── NormalCalculation.usf
    ├── AmbientOcclusion.usf
    ├── VoxelPhysics.usf
    ├── FluidSimulation.usf
    ├── LightPropagation.usf
    ├── ShadowCasting.usf
    ├── ChunkCulling.usf
    ├── LODGeneration.usf
    ├── CompressionRLE.usf
    ├── VoxelExplosion.usf
    ├── VoxelGrowth.usf
    └── VoxelErosion.usf
```

---

## Troubleshooting

### Build Failed

**Error:** `kain.exe not found`
- **Solution:** Add KAIN compiler to PATH

**Error:** `Parse error in voxelforge.kn`
- **Solution:** Check KAIN syntax, run `kain check voxelforge.kn`

**Error:** `Failed to generate C++`
- **Solution:** Check KAIN.toml configuration

### Compilation Failed

**Error:** `Cannot open include file`
- **Solution:** Regenerate Visual Studio project files

**Error:** `Unresolved external symbol`
- **Solution:** Clean and rebuild solution

**Error:** `Module not found`
- **Solution:** Check VoxelForgePro.Build.cs dependencies

### Plugin Not Loading

**Error:** Plugin not in list
- **Solution:** Ensure plugin is in Plugins/ folder

**Error:** "Plugin is not compatible"
- **Solution:** Check engine version (requires 5.4+)

**Error:** "Failed to load module"
- **Solution:** Check compilation succeeded, no errors

---

## Advanced Build Options

### Debug Build

```batch
kain build --ue5 --debug
```

Generates debug symbols and verbose logging.

### Release Build

```batch
kain build --ue5 --release
```

Optimized build for distribution.

### Custom Output Directory

```batch
kain build --ue5 --output MyCustomFolder
```

---

## Performance Verification

### After Installation

1. Create new level
2. Add VoxelWorld actor
3. Set View Distance to 2000
4. Play in editor
5. Check stats:
   - `stat FPS` - Should be 60+
   - `stat VoxelForge` - Check chunk count, memory
   - `stat GPU` - Check GPU timing

### Expected Performance

- **FPS:** 60+ with 100 chunks visible
- **Frame Time:** <16.67ms
- **Memory:** <2GB
- **Draw Calls:** <500

---

## Next Steps

1. Read README.md for feature overview
2. Read TECHNICAL.md for architecture details
3. Read API_REFERENCE.md for Blueprint/C++ API
4. Read PERFORMANCE.md for optimization guide
5. Check Examples/ folder for sample projects

---

## Support

If you encounter issues:

1. Check KAIN compiler version (latest)
2. Check Unreal Engine version (5.4+)
3. Verify all prerequisites installed
4. Check build logs for errors
5. Consult documentation

---

## Success!

If you see:
- ✅ Plugin loads in editor
- ✅ VoxelForge menu appears
- ✅ VoxelWorld generates terrain
- ✅ 60 FPS performance

**Congratulations! VoxelForge Pro is ready to use!**

Start building your voxel game with the most powerful voxel engine for Unreal Engine 5.
