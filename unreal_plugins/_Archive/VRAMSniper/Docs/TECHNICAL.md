# VRAM Sniper - Technical Implementation Guide

## Architecture Overview

VRAM Sniper is built using the KAIN language and compiles to production-ready UE5 C++. The plugin consists of several interconnected systems:

### Core Systems

1. **VRAM Analysis System** (`VRAMAnalyzer` actor)
   - Enumerates all UTexture2D assets via FAssetRegistryModule
   - Calculates VRAM footprint per texture
   - Detects 8 common texture issues
   - Tracks total VRAM usage across project

2. **Texture Optimization System** (`TextureOptimizerComponent`)
   - Batch optimization of flagged textures
   - Applies compression settings
   - Enables mipmaps and streaming
   - Sets appropriate TextureGroups

3. **UI System** (Slate + Details + Viewport + Toolbar)
   - Premium dark-mode dashboard
   - Real-time progress tracking
   - Sortable/filterable asset list
   - VRAM distribution pie chart
   - Dangerous assets warnings

4. **Reporting System**
   - CSV export for spreadsheet analysis
   - JSON export for automation
   - Per-texture detailed analysis

## VRAM Calculation Algorithm

### Formula

```
VRAM (bytes) = Width × Height × BytesPerPixel × MipChainMultiplier
VRAM (MB) = VRAM (bytes) / (1024 × 1024)
```

### Bytes Per Pixel by Compression Format

| Format | Bits/Pixel | Bytes/Pixel | Use Case |
|--------|-----------|-------------|----------|
| DXT1 (BC1) | 4 | 0.5 | Opaque textures, no alpha |
| DXT5 (BC3) | 8 | 1.0 | Textures with alpha |
| BC4 | 4 | 0.5 | Grayscale (heightmaps, masks) |
| BC5 | 8 | 1.0 | Normal maps (2-channel) |
| BC6H | 8 | 1.0 | HDR textures |
| BC7 | 8 | 1.0 | High-quality RGB/RGBA |
| ASTC 4×4 | 8 | 1.0 | Mobile, highest quality |
| ASTC 6×6 | 4.44 | 0.56 | Mobile, balanced |
| ASTC 8×8 | 2 | 0.25 | Mobile, performance |
| ASTC 12×12 | 0.89 | 0.11 | Mobile, lowest quality |
| Uncompressed | 32 | 4.0 | No compression (RGBA8) |

### Mipmap Chain Overhead

A full mipmap chain adds approximately 33% overhead:
- Level 0: 100% (full resolution)
- Level 1: 25% (half resolution)
- Level 2: 6.25% (quarter resolution)
- Level 3+: ~1.75% (remaining levels)
- **Total: ~133% of base size**

Implementation:
```cpp
if (MipLevels > 1) {
    TotalBytes *= 1.33f;
}
```

## Issue Detection System

### Issue Types and Detection Logic

#### 1. NoMipmaps (Flag: 0x01)
**Detection:**
```cpp
if (MipLevels <= 1) {
    IssueFlags |= 0x01;
}
```
**Impact:** Aliasing, shimmering, poor visual quality at distance  
**Fix:** Enable mipmap generation via `MipGenSettings = TMGS_FromTextureGroup`

#### 2. Uncompressed (Flag: 0x02)
**Detection:**
```cpp
if (Format == TextureCompressionFormat::Uncompressed) {
    IssueFlags |= 0x02;
}
```
**Impact:** 4-8× higher VRAM usage than compressed  
**Fix:** Apply appropriate compression format (BC7, DXT5, etc.)

#### 3. WrongCompression (Flag: 0x04)
**Detection:**
```cpp
// Example: Normal map not using BC5
if (Category == TextureCategory::Normalmap && Format != BC5) {
    IssueFlags |= 0x04;
}
```
**Impact:** Suboptimal quality or VRAM usage  
**Fix:** Apply optimal compression for texture type

#### 4. NonPowerOfTwo (Flag: 0x08)
**Detection:**
```cpp
bool IsPowerOfTwo(int32 Value) {
    return (Value > 0) && ((Value & (Value - 1)) == 0);
}

if (!IsPowerOfTwo(Width) || !IsPowerOfTwo(Height)) {
    IssueFlags |= 0x08;
}
```
**Impact:** May not compress properly, GPU inefficiency  
**Fix:** Resize to nearest power-of-two (512, 1024, 2048, 4096)

#### 5. ExcessiveResolution (Flag: 0x10)
**Detection:**
```cpp
if (Width > 4096 || Height > 4096) {
    IssueFlags |= 0x10;
}
```
**Impact:** Wasted VRAM, longer load times  
**Fix:** Downscale to appropriate resolution for use case

#### 6. NoTextureGroup (Flag: 0x20)
**Detection:**
```cpp
if (TextureGroup.IsEmpty() || TextureGroup == "TEXTUREGROUP_World") {
    IssueFlags |= 0x20;
}
```
**Impact:** Incorrect LOD bias, wrong compression settings  
**Fix:** Assign appropriate TextureGroup (UI, Character, World, etc.)

#### 7. WrongLODGroup (Flag: 0x40)
**Detection:**
```cpp
// Example: UI texture with World LOD settings
if (Category == TextureCategory::UI && LODGroup != "TEXTUREGROUP_UI") {
    IssueFlags |= 0x40;
}
```
**Impact:** Incorrect mipmap bias, quality issues  
**Fix:** Set correct LOD group for texture category

#### 8. NoStreaming (Flag: 0x80)
**Detection:**
```cpp
if (NeverStream == true) {
    IssueFlags |= 0x80;
}
```
**Impact:** All mips loaded at once, VRAM spikes, hitches  
**Fix:** Enable texture streaming (`NeverStream = false`)

## Optimization Actions

### 1. Enable Mipmaps

**C++ Implementation:**
```cpp
void EnableMipmaps(UTexture2D* Texture)
{
    if (Texture)
    {
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->PostEditChange();
        Texture->MarkPackageDirty();
    }
}
```

**KAIN Blueprint Function:**
```kain
@blueprint
fn EnableTextureMipmaps(texture_path: String):
    # Load texture asset
    # Set MipGenSettings
    # Save changes
    println("Enabled mipmaps for: {texture_path}")
```

### 2. Apply Compression

**Optimal Compression Selection:**
```cpp
TextureCompressionSettings GetOptimalCompression(TextureCategory Category, bool bHasAlpha)
{
    switch (Category)
    {
        case TextureCategory::UI:
            return TC_BC7; // High quality for UI
        
        case TextureCategory::Normalmap:
            return TC_Normalmap; // BC5 for normals
        
        case TextureCategory::HDR:
            return TC_HDR; // BC6H for HDR
        
        case TextureCategory::Character:
            return bHasAlpha ? TC_BC7 : TC_Default; // BC7 or DXT1
        
        default:
            return bHasAlpha ? TC_Default : TC_BC7; // DXT5 or BC7
    }
}
```

**C++ Implementation:**
```cpp
void ApplyCompression(UTexture2D* Texture, TextureCompressionSettings Compression)
{
    if (Texture)
    {
        Texture->CompressionSettings = Compression;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
    }
}
```

### 3. Set Texture Group

**C++ Implementation:**
```cpp
void SetTextureGroup(UTexture2D* Texture, TextureGroup Group)
{
    if (Texture)
    {
        Texture->LODGroup = Group;
        Texture->PostEditChange();
        Texture->MarkPackageDirty();
    }
}
```

**Texture Group Mapping:**
```cpp
TextureGroup GetTextureGroupForCategory(TextureCategory Category)
{
    switch (Category)
    {
        case TextureCategory::UI:
            return TEXTUREGROUP_UI;
        
        case TextureCategory::Character:
            return TEXTUREGROUP_Character;
        
        case TextureCategory::World:
            return TEXTUREGROUP_World;
        
        case TextureCategory::Effects:
            return TEXTUREGROUP_Effects;
        
        case TextureCategory::Lightmap:
            return TEXTUREGROUP_Lightmap;
        
        case TextureCategory::Normalmap:
            return TEXTUREGROUP_WorldNormalMap;
        
        default:
            return TEXTUREGROUP_World;
    }
}
```

### 4. Enable Streaming

**C++ Implementation:**
```cpp
void EnableStreaming(UTexture2D* Texture)
{
    if (Texture)
    {
        Texture->NeverStream = false;
        Texture->PostEditChange();
        Texture->MarkPackageDirty();
    }
}
```

## Asset Registry Integration

### Enumerating All Textures

**C++ Implementation:**
```cpp
void ScanAllTextures(TArray<FAssetData>& OutTextureAssets)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // Get all UTexture2D assets
    AssetRegistry.GetAssetsByClass(UTexture2D::StaticClass()->GetFName(), OutTextureAssets);
    
    UE_LOG(LogTemp, Log, TEXT("Found %d textures in project"), OutTextureAssets.Num());
}
```

### Extracting Texture Properties

**C++ Implementation:**
```cpp
void AnalyzeTexture(const FAssetData& AssetData, FTextureAnalysisData& OutAnalysis)
{
    // Load texture (lightweight, doesn't load pixel data)
    UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset());
    if (!Texture)
        return;
    
    // Extract properties
    OutAnalysis.AssetPath = AssetData.ObjectPath.ToString();
    OutAnalysis.AssetName = AssetData.AssetName.ToString();
    OutAnalysis.Width = Texture->GetSizeX();
    OutAnalysis.Height = Texture->GetSizeY();
    OutAnalysis.Format = GetCompressionFormat(Texture->CompressionSettings);
    OutAnalysis.MipLevels = Texture->GetNumMips();
    OutAnalysis.TextureGroup = Texture->LODGroup;
    OutAnalysis.IsStreaming = !Texture->NeverStream;
    
    // Calculate VRAM
    OutAnalysis.VRAM_MB = CalculateTextureVRAM(
        OutAnalysis.Width,
        OutAnalysis.Height,
        OutAnalysis.Format,
        OutAnalysis.MipLevels
    );
    
    // Detect issues
    OutAnalysis.IssueFlags = DetectTextureIssues(
        OutAnalysis.Width,
        OutAnalysis.Height,
        OutAnalysis.Format,
        OutAnalysis.MipLevels,
        OutAnalysis.IsStreaming
    );
    
    OutAnalysis.HasIssues = (OutAnalysis.IssueFlags != 0);
}
```

## UI Implementation

### Dashboard Layout

```
┌─────────────────────────────────────────────────────────────┐
│ VRAM Sniper Dashboard                          [Scan] [Fix] │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│ │ Total VRAM  │ │  Textures   │ │   Issues    │           │
│ │  2.4 GB     │ │    1,247    │ │     83      │           │
│ └─────────────┘ └─────────────┘ └─────────────┘           │
├─────────────────────────────────────────────────────────────┤
│ ┌───────────────────────────┐ ┌─────────────────────────┐ │
│ │ Asset List                │ │ VRAM Distribution       │ │
│ │ ┌─────────────────────┐   │ │                         │ │
│ │ │ Name | Res | VRAM   │   │ │      [Pie Chart]        │ │
│ │ │ tex1 |4096| 21.3 MB │   │ │                         │ │
│ │ │ tex2 |2048|  5.3 MB │   │ │  UI: 30%                │ │
│ │ │ tex3 |2048|  5.3 MB │   │ │  World: 45%             │ │
│ │ │ ...                 │   │ │  Character: 20%         │ │
│ │ └─────────────────────┘   │ │  Effects: 5%            │ │
│ └───────────────────────────┘ └─────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ ⚠️ Dangerous Assets (High VRAM Waste)                      │
│ ┌─────────────────────────────────────────────────────────┐│
│ │ • uncompressed_4k.png - 64 MB (No compression)          ││
│ │ • huge_ui_texture.tga - 42 MB (Excessive resolution)    ││
│ │ • character_diffuse.bmp - 32 MB (No mipmaps)            ││
│ └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### Slate Widget Hierarchy

```
VRAMDashboard (SCompoundWidget)
├── SVerticalBox
│   ├── Header (SHorizontalBox)
│   │   ├── Title (STextBlock)
│   │   ├── Spacer
│   │   ├── ScanButton (SButton)
│   │   └── FixButton (SButton)
│   ├── StatsPanel (SHorizontalBox)
│   │   ├── TotalVRAMCard (SBorder)
│   │   ├── TotalTexturesCard (SBorder)
│   │   └── IssuesCard (SBorder)
│   ├── MainContent (SHorizontalBox)
│   │   ├── LeftPanel (SVerticalBox)
│   │   │   ├── FilterBar (SHorizontalBox)
│   │   │   └── TextureListWidget (SListView)
│   │   └── RightPanel (SVerticalBox)
│   │       ├── VRAMPieChart (SCanvas)
│   │       └── TexturePreview (SImage)
│   └── DangerousAssetsPanel (SBorder)
│       └── DangerousAssetsList (SListView)
```

## Performance Optimization

### Async Scanning

Scan textures asynchronously to avoid blocking the UI:

```cpp
void AsyncScanTextures()
{
    // Run on background thread
    Async(EAsyncExecution::ThreadPool, [this]()
    {
        TArray<FAssetData> TextureAssets;
        ScanAllTextures(TextureAssets);
        
        int32 ProcessedCount = 0;
        float TotalVRAM = 0.0f;
        int32 IssueCount = 0;
        
        for (const FAssetData& AssetData : TextureAssets)
        {
            FTextureAnalysisData Analysis;
            AnalyzeTexture(AssetData, Analysis);
            
            TotalVRAM += Analysis.VRAM_MB;
            if (Analysis.HasIssues)
                IssueCount++;
            
            ProcessedCount++;
            
            // Update UI every 100 textures
            if (ProcessedCount % 100 == 0)
            {
                float Progress = (float)ProcessedCount / TextureAssets.Num() * 100.0f;
                
                // Update on game thread
                AsyncTask(ENamedThreads::GameThread, [=]()
                {
                    UpdateScanProgress(Progress, ProcessedCount, TotalVRAM, IssueCount);
                });
            }
        }
        
        // Scan complete
        AsyncTask(ENamedThreads::GameThread, [=]()
        {
            CompleteScan(ProcessedCount, TotalVRAM, IssueCount);
        });
    });
}
```

### Batch Optimization

Optimize textures in batches to avoid editor freezing:

```cpp
void BatchOptimizeTextures(const TArray<FTextureAnalysisData>& Textures)
{
    const int32 BatchSize = 10;
    int32 CurrentBatch = 0;
    
    // Process in batches
    while (CurrentBatch < Textures.Num())
    {
        int32 BatchEnd = FMath::Min(CurrentBatch + BatchSize, Textures.Num());
        
        for (int32 i = CurrentBatch; i < BatchEnd; i++)
        {
            OptimizeTexture(Textures[i]);
        }
        
        CurrentBatch = BatchEnd;
        
        // Update progress
        float Progress = (float)CurrentBatch / Textures.Num() * 100.0f;
        UpdateOptimizationProgress(Progress, CurrentBatch, CalculateVRAMSaved());
        
        // Yield to prevent freezing
        FPlatformProcess::Sleep(0.01f);
    }
}
```

## Testing Checklist

- [ ] Scan detects all UTexture2D assets in project
- [ ] VRAM calculation matches actual memory usage
- [ ] All 8 issue types detected correctly
- [ ] Optimization fixes issues without breaking textures
- [ ] UI remains responsive during scan/optimization
- [ ] CSV/JSON export contains correct data
- [ ] Thumbnail previews load correctly
- [ ] Pie chart displays accurate percentages
- [ ] Dangerous assets panel shows critical issues
- [ ] Progress bars update smoothly
- [ ] Undo/redo works for texture modifications
- [ ] Plugin loads without errors in UE5.3+
- [ ] No memory leaks during long scanning sessions
- [ ] Works with 10,000+ texture projects

## Known Limitations

1. **Texture Streaming Pool:** Does not analyze streaming pool size or budget
2. **Material Textures:** Does not track which materials use which textures
3. **Blueprint References:** Does not detect unused textures referenced in Blueprints
4. **Virtual Textures:** Limited support for virtual texture analysis
5. **Render Targets:** Does not analyze dynamic render target VRAM usage

## Future Enhancements

1. **Real-time VRAM Monitoring:** Track VRAM usage during PIE
2. **Material Analysis:** Show which materials use each texture
3. **Unused Texture Detection:** Find textures not referenced anywhere
4. **Duplicate Detection:** Find identical textures with different names
5. **Texture Atlas Generation:** Combine small textures into atlases
6. **Virtual Texture Support:** Analyze virtual texture streaming
7. **Per-Level Analysis:** Break down VRAM by level
8. **Budget Enforcement:** Set VRAM budgets and enforce limits
9. **CI/CD Integration:** Command-line scanning for build pipelines
10. **Marketplace Integration:** One-click publish to UE Marketplace

---

**Built with KAIN** - Production-ready UE5 plugins from high-level code
