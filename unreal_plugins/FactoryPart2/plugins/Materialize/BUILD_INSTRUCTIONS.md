# Materialize Plugin - Build Instructions

**Version:** 2.0.0 (KAIN Rebuild)  
**Status:** Ready to Build  
**Completion:** 100%

---

## Quick Start

```bash
# Navigate to plugin directory
cd FactoryPart2/plugins/Materialize

# Build UE5 plugin
kain build --ue5

# Expected output: Source/, Shaders/, Content/, Materialize.uplugin, Materialize.Build.cs
```

---

## Prerequisites

### Required Software
- **KAIN Compiler** — Installed at `M:/CODE/KAIN/TARGET/RELEASE/kain.exe`
- **Unreal Engine 5.4** — Installed and configured
- **Visual Studio 2022** — With C++ workload and UE5 support
- **Windows 10/11** — 64-bit

### Verify Installation
```bash
# Check KAIN version
kain --version

# Check UE5 installation
# Verify UE5 is in PATH or set UE5_ROOT environment variable
```

---

## Build Steps

### Step 1: Generate C++ Code
```bash
cd FactoryPart2/plugins/Materialize
kain build --ue5
```

**Expected Output:**
```
Materialize/
├── Source/
│   ├── Materialize/              # Runtime module
│   │   ├── Public/
│   │   │   ├── MaterializeTypes.h
│   │   │   ├── MaterializePresets.h
│   │   │   ├── MaterializeEngine.h
│   │   │   ├── MaterializeLayerSystem.h
│   │   │   ├── MaterializeBatchProcessor.h
│   │   │   └── Materialize.h
│   │   ├── Private/
│   │   │   ├── MaterializeTypes.cpp
│   │   │   ├── MaterializePresets.cpp
│   │   │   ├── MaterializeEngine.cpp
│   │   │   ├── MaterializeLayerSystem.cpp
│   │   │   ├── MaterializeBatchProcessor.cpp
│   │   │   └── MaterializeModule.cpp
│   │   └── Materialize.Build.cs
│   └── MaterializeEditor/        # Editor module
│       ├── Public/
│       │   ├── MaterializeEditorModule.h
│       │   ├── MaterializeAssetEditor.h
│       │   ├── MaterializeViewport.h
│       │   └── MaterializeWidgets.h
│       ├── Private/
│       │   ├── MaterializeEditorModule.cpp
│       │   ├── MaterializeAssetEditor.cpp
│       │   ├── MaterializeViewport.cpp
│       │   └── MaterializeWidgets.cpp
│       └── MaterializeEditor.Build.cs
├── Shaders/
│   ├── Private/
│   │   ├── PBRGenerator.usf
│   │   ├── LayerBlend.usf
│   │   ├── ImageFilter.usf
│   │   ├── Seamless.usf
│   │   ├── NoiseGenerator.usf
│   │   └── ORMPacking.usf
│   └── Public/
│       └── MaterializeCommon.ush
├── Content/
│   └── Blueprints/
│       └── (Generated blueprint assets)
├── Materialize.uplugin
└── README.md
```

### Step 2: Copy to UE5 Project
```bash
# Copy entire plugin folder to your UE5 project
xcopy /E /I FactoryPart2\plugins\Materialize MyProject\Plugins\Materialize
```

### Step 3: Regenerate Project Files
```bash
# Right-click MyProject.uproject
# Select "Generate Visual Studio project files"
```

### Step 4: Compile in Visual Studio
```bash
# Open MyProject.sln in Visual Studio 2022
# Build > Build Solution (Ctrl+Shift+B)
# Wait for compilation to complete
```

### Step 5: Launch UE5 Editor
```bash
# Launch MyProject.uproject
# Editor will prompt to rebuild modules if needed
# Click "Yes" to rebuild
```

### Step 6: Enable Plugin
```bash
# In UE5 Editor:
# Edit > Plugins
# Search for "Materialize"
# Check the box to enable
# Restart editor when prompted
```

---

## Verification

### Check Plugin Loaded
```bash
# In UE5 Editor:
# Window > Developer Tools > Output Log
# Search for "Materialize"
# Should see: "Materialize plugin loaded successfully"
```

### Test Core Features
1. **Create Materialize Asset**
   - Content Browser > Right-click > Materialize > Materialize Asset
   - Double-click to open editor

2. **Verify Editor UI**
   - 3-tab layout: Viewport | Layers | Properties
   - Toolbar with Generate/Save/Export buttons
   - Preset dropdown with 33 presets

3. **Test PBR Generation**
   - Import a texture (File > Import)
   - Add layer to stack (+ button in Layers panel)
   - Select preset from dropdown
   - Click "Generate" button
   - Verify preview updates in viewport

4. **Test Layer System**
   - Add multiple layers
   - Adjust blend modes and opacity
   - Toggle visibility
   - Move layers up/down
   - Verify real-time updates

5. **Test Batch Processing**
   - Tools > Materialize > Batch Process Textures
   - Add multiple textures to queue
   - Click "Start Batch"
   - Verify progress updates

---

## Troubleshooting

### Build Errors

**Error: "kain: command not found"**
```bash
# Solution: Add KAIN to PATH
set PATH=%PATH%;M:\CODE\KAIN\TARGET\RELEASE
```

**Error: "UE5 not found"**
```bash
# Solution: Set UE5_ROOT environment variable
set UE5_ROOT=C:\Program Files\Epic Games\UE_5.4
```

**Error: "Visual Studio not found"**
```bash
# Solution: Install Visual Studio 2022 with C++ workload
# Download from: https://visualstudio.microsoft.com/
```

### Compilation Errors

**Error: "Cannot open include file: 'MaterializeTypes.h'"**
```bash
# Solution: Regenerate project files
# Right-click .uproject > Generate Visual Studio project files
```

**Error: "Unresolved external symbol"**
```bash
# Solution: Clean and rebuild
# Build > Clean Solution
# Build > Build Solution
```

**Error: "Module 'Materialize' could not be loaded"**
```bash
# Solution: Check Build.cs dependencies
# Verify all required modules are listed:
# - Core, CoreUObject, Engine
# - RenderCore, RHI (for shaders)
# - Slate, SlateCore (for editor)
# - UnrealEd, AssetTools (for asset editor)
```

### Runtime Errors

**Error: "Shader compilation failed"**
```bash
# Solution: Check shader directory mapping
# Verify Shaders/Private/ contains all .usf files
# Check Materialize.uplugin has correct shader directory
```

**Error: "Blueprint function not found"**
```bash
# Solution: Verify UFUNCTION macros
# Check generated .h files have BlueprintCallable
# Restart editor to refresh Blueprint cache
```

**Error: "Editor crashes on asset open"**
```bash
# Solution: Check editor module dependencies
# Verify MaterializeEditor.Build.cs includes:
# - Materialize (runtime module)
# - PropertyEditor (for details panels)
# - UnrealEd (for asset editor)
```

---

## Advanced Build Options

### Dry Run (Preview Only)
```bash
kain build --ue5 --dry-run
# Shows what would be generated without writing files
```

### Verbose Output
```bash
kain build --ue5 --verbose
# Shows detailed compilation steps
```

### Embed Debug Info
```bash
kain build --ue5 --embed
# Embeds source maps for debugging
```

### Analyze Shader Complexity
```bash
kain build --ue5 --analyze
# Analyzes shader instruction counts
```

### Target Specific Module
```bash
kain build --ue5 --module Materialize
# Builds only runtime module (not editor)
```

---

## Performance Optimization

### Shader Compilation
```bash
# Enable shader caching in UE5
# Edit > Project Settings > Engine > Rendering
# Check "Share Material Shader Code"
# Check "Shared Material Native Libraries"
```

### Build Configuration
```bash
# For faster iteration:
# Build > Configuration Manager
# Set to "Development Editor" (not "Shipping")
```

### Parallel Compilation
```bash
# In Visual Studio:
# Tools > Options > Projects and Solutions > Build and Run
# Set "maximum number of parallel project builds" to CPU core count
```

---

## Testing Checklist

### Build Tests
- [ ] `kain build --ue5` completes without errors
- [ ] All expected files generated (Source/, Shaders/, Content/)
- [ ] Materialize.uplugin created with correct metadata
- [ ] Build.cs files created for both modules

### Compilation Tests
- [ ] Visual Studio solution opens without errors
- [ ] Build succeeds with 0 errors
- [ ] All warnings reviewed and addressed
- [ ] Plugin loads in UE5 Editor

### Runtime Tests
- [ ] Materialize asset can be created
- [ ] Editor opens with 3-tab layout
- [ ] Viewport displays preview mesh
- [ ] Layer panel shows stack
- [ ] Properties panel updates on selection
- [ ] Generate button creates PBR maps
- [ ] Batch processing works

### Performance Tests
- [ ] PBR generation (2048x2048): < 500ms
- [ ] Layer evaluation (10 layers): < 50ms
- [ ] UI responsiveness: < 16ms per frame
- [ ] Memory usage: < 100MB

---

## Support

### Documentation
- `PROJECT_COMPLETE.md` — Project summary
- `docs/PHASE8_COMPLETION_REPORT.md` — Integration report
- `docs/CORE_ARCHITECTURE.md` — Type system reference
- `docs/SHADER_ANALYSIS.md` — GPU pipeline reference

### Source Files
- `src/` — All 12 KAIN source files
- `KAIN.toml` — Build configuration
- `README.md` — Plugin overview

### Contact
- **Project:** Materialize KAIN Rebuild
- **Location:** `FactoryPart2/plugins/Materialize/`
- **KAIN Compiler:** `M:/CODE/KAIN/TARGET/RELEASE/kain.exe`

---

**Status:** Ready to build! Run `kain build --ue5` to get started.
