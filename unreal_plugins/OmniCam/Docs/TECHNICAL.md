# OmniCam - Technical Documentation

**Version:** 1.0.0  
**Last Updated:** 2026-02-19  
**Target Engine:** Unreal Engine 5.4+

---

## Architecture Overview

OmniCam is built using the KAIN language, which compiles to production-ready UE5 C++ code. The plugin consists of 13 major subsystems organized into a modular architecture.

### Component Hierarchy

```
OmniCamEditorModule (IModuleInterface)
├── OmniCamAssetEditor (FAssetEditorToolkit)
│   ├── CameraPiPViewport (SEditorViewport + FEditorViewportClient)
│   ├── CameraPropertiesDetails (IDetailCustomization)
│   └── OmniCamToolbar (FToolBarBuilder)
├── Slate Widgets
│   ├── CameraListWidget (SCompoundWidget)
│   ├── CameraPiPWidget (SCompoundWidget)
│   ├── CameraBulkEditWidget (SCompoundWidget)
│   └── CameraSearchWidget (SCompoundWidget)
└── Runtime Components
    ├── CameraIndexerComponent (UActorComponent)
    └── CameraPreviewComponent (UActorComponent)
```

---

## Core Systems

### 1. Camera Indexing System

**Component:** `CameraIndexerComponent`  
**Purpose:** Discovers and tracks all cameras in the level

**Implementation Details:**
- Uses `UGameplayStatics::GetAllActorsOfClass()` to find `ACineCameraActor` and `ACameraActor`
- Maintains an array of `CameraInfo` structs with metadata
- Supports auto-update mode (scans on level changes)
- Caches last scan time to avoid redundant scans

**Key Functions:**
```cpp
// Generated from KAIN:
UFUNCTION(BlueprintCallable)
TArray<FCameraInfo> GetAllCamerasInLevel();

UFUNCTION(BlueprintCallable)
FCameraInfo SelectCameraByName(const FString& Name);
```

**Performance:**
- O(n) scan complexity where n = number of actors in level
- Typical scan time: <10ms for 1000 actors
- Cached results until invalidation

---

### 2. Picture-in-Picture Preview System

**Component:** `CameraPreviewComponent`  
**Purpose:** Renders live camera view without possessing

**Implementation Details:**
- Uses `USceneCaptureComponent2D` for off-screen rendering
- Supports 4 quality presets (Low/Medium/High/Ultra)
- Renders to `UTextureRenderTarget2D` (640x360 default)
- Updates at 30 FPS to minimize overhead

**Quality Presets:**
| Quality | Resolution | Anti-Aliasing | Post-Processing |
|---------|-----------|---------------|-----------------|
| Low     | 320x180   | None          | Disabled        |
| Medium  | 640x360   | FXAA          | Basic           |
| High    | 1280x720  | TAA           | Full            |
| Ultra   | 1920x1080 | TAA + MSAA 4x | Full + DOF      |

**Rendering Pipeline:**
```
Selected Camera → SceneCaptureComponent2D → RenderTarget → Slate Brush → UI
```

---

### 3. Bulk Editing System

**Component:** `CameraBulkEditWidget` + `CameraPropertiesDetails`  
**Purpose:** Modify multiple cameras simultaneously

**Implementation Details:**
- Iterates over selected cameras
- Calls `Modify()` before property changes (undo/redo support)
- Calls `PostEditChangeProperty()` after changes (triggers updates)
- Supports undo/redo via UE5 transaction system

**Editable Properties:**
- **Focal Length** - 10mm to 200mm (affects FOV)
- **Aperture** - f/1.4 to f/22 (affects DOF)
- **Focus Distance** - 0cm to 10000cm (DOF focus plane)
- **Sensor Width** - 12mm to 36mm (affects FOV calculation)
- **Debug Color** - RGB color for frustum visualization

**Blueprint Functions:**
```cpp
UFUNCTION(BlueprintCallable)
bool BulkSetFocalLength(const TArray<FCameraInfo>& Cameras, float FocalLength);

UFUNCTION(BlueprintCallable)
bool BulkSetAperture(const TArray<FCameraInfo>& Cameras, float Aperture);
```

---

### 4. Slate UI System

**Widgets:** 4 custom SCompoundWidgets  
**Purpose:** Dockable editor interface

#### CameraListWidget
- **Type:** `SListView<TSharedPtr<FCameraInfo>>`
- **Features:** Multi-select, search filtering, sorting
- **Layout:** Vertical list with alternating row colors
- **Interaction:** Click to select, Ctrl+Click for multi-select

#### CameraPiPWidget
- **Type:** `SViewport` wrapper
- **Features:** Live preview, quality selector, camera info overlay
- **Layout:** Viewport + controls in VBox
- **Rendering:** Binds to `UTextureRenderTarget2D` from preview component

#### CameraBulkEditWidget
- **Type:** Form with sliders and buttons
- **Features:** Property sliders, apply/reset buttons, selection count
- **Layout:** Grid layout with labels and controls
- **Validation:** Clamps values to valid ranges

#### CameraSearchWidget
- **Type:** Search bar + sort dropdown
- **Features:** Text filtering, sort mode selection
- **Layout:** Horizontal box with search + sort controls
- **Performance:** Filters on keystroke with debouncing

---

### 5. Details Panel Customization

**Class:** `CameraPropertiesDetails` (IDetailCustomization)  
**Purpose:** Custom property editor for camera settings

**KAIN Attributes → UE5 Widgets:**
| KAIN Attribute | UE5 Widget | Parameters |
|----------------|-----------|------------|
| `@slider(min: 10.0, max: 200.0)` | `SSpinBox<float>` | Min=10, Max=200, Delta=1 |
| `@color_picker` | `SColorPicker` | RGB mode, no alpha |
| `@button(label: "...")` | `SButton` | Text label, OnClicked delegate |

**Generated Code:**
```cpp
// From KAIN @slider attribute:
DetailBuilder.EditCategory("Camera Properties")
    .AddCustomRow(LOCTEXT("FocalLength", "Focal Length"))
    .NameContent()
    [
        SNew(STextBlock).Text(LOCTEXT("FocalLength", "Focal Length"))
    ]
    .ValueContent()
    [
        SNew(SSpinBox<float>)
            .MinValue(10.0f)
            .MaxValue(200.0f)
            .Value(this, &FCameraPropertiesDetails::GetFocalLength)
            .OnValueChanged(this, &FCameraPropertiesDetails::SetFocalLength)
    ];
```

---

### 6. Viewport System

**Class:** `CameraPiPViewport` (SEditorViewport + FEditorViewportClient)  
**Purpose:** 3D preview viewport for camera view

**Implementation Details:**
- Inherits from `SEditorViewport` for editor integration
- Custom `FEditorViewportClient` for rendering logic
- Supports camera manipulation (orbit, pan, zoom)
- Renders scene actors with camera frustum visualization

**Viewport Client Features:**
- **Scene Actors:** Renders preview mesh (camera frustum)
- **Camera:** Matches selected camera's transform and lens settings
- **Tick:** Updates at editor frame rate (60 FPS typical)
- **Input:** Mouse/keyboard navigation (optional)

**Generated Code:**
```cpp
// From KAIN @viewport attribute:
class SCameraPiPViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SCameraPiPViewport) {}
    SLATE_END_ARGS()
    
    void Construct(const FArguments& InArgs);
    
protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
    
private:
    TSharedPtr<FCameraPiPViewportClient> ViewportClient;
};
```

---

### 7. Toolbar System

**Class:** `OmniCamToolbar` (FToolBarBuilder extension)  
**Purpose:** Quick-access buttons and toggles

**KAIN Attributes → UE5 Commands:**
| KAIN Attribute | UE5 Command Type | Behavior |
|----------------|-----------------|----------|
| `@button(icon: "...", tooltip: "...")` | `FUIAction` | Executes function on click |
| `@toggle(label: "...")` | `FUIAction` with `IsChecked` | Toggles boolean state |
| `@separator` | `FToolBarBuilder::AddSeparator()` | Visual separator |
| `@dropdown(label: "...")` | `FUIAction` with menu | Opens submenu |

**Toolbar Layout:**
```
[Refresh] [Select All] [Deselect All] [Focus] | [Show Frustums] [Show Names] [Auto-Update] | [Sort By ▼]
```

**Generated Code:**
```cpp
// From KAIN @button attribute:
ToolbarBuilder.AddToolBarButton(
    FUIAction(
        FExecuteAction::CreateSP(this, &FOmniCamToolbar::OnRefreshIndex),
        FCanExecuteAction::CreateLambda([]() { return true; })
    ),
    NAME_None,
    LOCTEXT("RefreshIndex", "Refresh Index"),
    LOCTEXT("RefreshIndexTooltip", "Refresh Camera Index"),
    FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Refresh")
);
```

---

### 8. Asset Editor System

**Class:** `OmniCamAssetEditor` (FAssetEditorToolkit)  
**Purpose:** Full-featured asset editor combining all subsystems

**Layout:**
```
┌─────────────────────────────────────────────────────┐
│ Toolbar: [Refresh] [Select All] [Deselect] [Focus] │
├──────────────┬──────────────────────────────────────┤
│              │                                      │
│  Camera List │  PiP Viewport                        │
│  (Left)      │  (Right)                             │
│              │                                      │
├──────────────┴──────────────────────────────────────┤
│  Bulk Edit Panel (Bottom)                           │
│  [Focal Length] [Aperture] [Focus] [Sensor]         │
└─────────────────────────────────────────────────────┘
```

**Tab Management:**
- Uses `FGlobalTabmanager` for docking
- Persists layout across sessions
- Supports multi-window workflows

**Generated Code:**
```cpp
// From KAIN @asset_editor attribute:
class FOmniCamAssetEditor : public FAssetEditorToolkit
{
public:
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    
    virtual FName GetToolkitFName() const override { return FName("OmniCamAssetEditor"); }
    virtual FText GetBaseToolkitName() const override { return LOCTEXT("AppLabel", "OmniCam Asset Editor"); }
    virtual FString GetWorldCentricTabPrefix() const override { return TEXT("OmniCam "); }
    virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); }
    
private:
    TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnTab_CameraList(const FSpawnTabArgs& Args);
};
```

---

### 9. Editor Module System

**Class:** `OmniCamEditorModule` (IModuleInterface)  
**Purpose:** Plugin initialization and menu integration

**KAIN Attributes → UE5 Integration:**
| KAIN Attribute | UE5 System | Result |
|----------------|-----------|--------|
| `@menu_entry(path: "Tools/OmniCam/...")` | `FLevelEditorModule::GetMenuExtensibilityManager()` | Menu item in Tools menu |
| `@toolbar_button(section: "Content", icon: "...")` | `FLevelEditorModule::GetToolBarExtensibilityManager()` | Toolbar button |

**Module Lifecycle:**
```cpp
// Generated from KAIN @editor_module:
class FOmniCamEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
private:
    void RegisterMenuExtensions();
    void RegisterToolbarExtensions();
    void OnOpenCameraManager();
    void OnRefreshCameraIndex();
    
    TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
    TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE(FOmniCamEditorModule, OmniCamEditor)
```

---

## Data Structures

### CameraInfo Struct
```cpp
// Generated from KAIN:
USTRUCT(BlueprintType)
struct FCameraInfo
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite)
    FString Name;
    
    UPROPERTY(BlueprintReadWrite)
    ECameraType Type;
    
    UPROPERTY(BlueprintReadWrite)
    FVector Location;
    
    UPROPERTY(BlueprintReadWrite)
    FVector Rotation;
    
    UPROPERTY(BlueprintReadWrite)
    float FOV;
    
    UPROPERTY(BlueprintReadWrite)
    float FocalLength;
    
    UPROPERTY(BlueprintReadWrite)
    float Aperture;
    
    UPROPERTY(BlueprintReadWrite)
    float SensorWidth;
    
    UPROPERTY(BlueprintReadWrite)
    FVector DebugColor;
};
```

### CameraSelection Struct
```cpp
USTRUCT(BlueprintType)
struct FCameraSelection
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite)
    TArray<FCameraInfo> SelectedCameras;
    
    UPROPERTY(BlueprintReadWrite)
    int32 Count;
};
```

---

## Enums

### CameraType
```cpp
UENUM(BlueprintType)
enum class ECameraType : uint8
{
    Cine        UMETA(DisplayName = "Cine Camera"),
    Standard    UMETA(DisplayName = "Standard Camera"),
    Custom      UMETA(DisplayName = "Custom Camera"),
    _MAX        UMETA(Hidden)
};
```

### ViewportQuality
```cpp
UENUM(BlueprintType)
enum class EViewportQuality : uint8
{
    Low         UMETA(DisplayName = "Low (320x180)"),
    Medium      UMETA(DisplayName = "Medium (640x360)"),
    High        UMETA(DisplayName = "High (1280x720)"),
    Ultra       UMETA(DisplayName = "Ultra (1920x1080)"),
    _MAX        UMETA(Hidden)
};
```

### SortMode
```cpp
UENUM(BlueprintType)
enum class ESortMode : uint8
{
    Name        UMETA(DisplayName = "Sort by Name"),
    Type        UMETA(DisplayName = "Sort by Type"),
    Location    UMETA(DisplayName = "Sort by Location"),
    FOV         UMETA(DisplayName = "Sort by FOV"),
    _MAX        UMETA(Hidden)
};
```

---

## Performance Characteristics

### Memory Usage
- **Base Plugin:** ~2 MB (compiled binaries)
- **Runtime Overhead:** ~500 KB (component instances)
- **Per-Camera:** ~200 bytes (CameraInfo struct)
- **PiP Render Target:** 640x360x4 bytes = ~900 KB (Medium quality)

### CPU Usage
- **Idle:** <0.1% (no cameras selected)
- **PiP Active:** ~2-5% (30 FPS rendering)
- **Bulk Edit:** <1% (one-time operation)
- **Camera Scan:** <0.5% (triggered manually or on level change)

### Scalability
- **Tested with:** 100+ cameras in a single level
- **UI Responsiveness:** <16ms frame time (60 FPS)
- **Scan Time:** O(n) where n = actor count, ~10ms for 1000 actors
- **PiP Rendering:** Independent of camera count (renders one at a time)

---

## Build System

### KAIN Compilation
```bash
cd Factory/OmniCam
kain build --ue5
```

**Output Files:**
- `Source/OmniCam/Public/*.h` - Header files
- `Source/OmniCam/Private/*.cpp` - Implementation files
- `OmniCam.uplugin` - Plugin descriptor
- `Source/OmniCam/OmniCam.Build.cs` - Build configuration

### UE5 Compilation
```bash
# Method 1: Quick build (KAIN only)
Build5.4.bat

# Method 2: Full build (KAIN + UE5 packaging)
FULLBUILD.bat
```

**Dependencies:**
- Core, CoreUObject, Engine (runtime)
- Slate, SlateCore (UI)
- UnrealEd, PropertyEditor, EditorStyle (editor)
- LevelEditor, InputCore (editor integration)
- CinematicCamera (camera support)
- Projects (plugin system)

---

## Extension Points

### Adding Custom Camera Types
1. Add new enum value to `CameraType`
2. Update `get_all_cameras_in_level()` to detect new type
3. Rebuild plugin

### Adding New Properties
1. Add field to `CameraInfo` struct
2. Add slider/picker to `CameraPropertiesDetails`
3. Add bulk edit function to `CameraBulkEditWidget`
4. Rebuild plugin

### Custom Viewport Rendering
1. Modify `CameraPiPViewport::on_viewport_tick()`
2. Add custom rendering logic
3. Rebuild plugin

---

## Known Limitations

1. **PiP Performance:** High/Ultra quality may impact editor performance on low-end GPUs
2. **Camera Detection:** Only detects `ACineCameraActor` and `ACameraActor` (not custom camera classes)
3. **Undo/Redo:** Bulk edits create one transaction per camera (may clutter undo history)
4. **Multi-Level:** Only scans currently loaded level (not sub-levels)

---

## Future Enhancements

### Planned Features
- **Camera Sequencer Integration** - Import/export camera sequences
- **Preset System** - Save/load camera property presets
- **Camera Paths** - Visualize and edit camera movement paths
- **Multi-Level Support** - Scan all sub-levels simultaneously
- **Custom Camera Types** - Support for user-defined camera classes
- **Batch Export** - Export camera data to JSON/CSV

### Performance Improvements
- **Incremental Scanning** - Only scan changed actors
- **LOD System** - Reduce PiP quality when viewport is small
- **Async Rendering** - Offload PiP rendering to background thread

---

## Debugging

### Enable Verbose Logging
Edit `KAIN.toml`:
```toml
[debug]
verbose_logging = true
```

### Common Issues

**Issue:** Camera list is empty  
**Solution:** Click "Refresh Index" or enable "Auto-Update"

**Issue:** PiP preview is black  
**Solution:** Increase quality setting or check camera is not inside geometry

**Issue:** Bulk edit not applying  
**Solution:** Ensure cameras are selected and not locked in World Outliner

---

## Contact

For technical support or bug reports:
- Email: dev@kainfactory.com
- GitHub: [github.com/kainfactory/omnicam](https://github.com/kainfactory/omnicam)
- Discord: [KAIN Factory Community](https://discord.gg/kainfactory)

---

**Generated by KAIN Compiler v1.0.0**  
**Target Engine: Unreal Engine 5.4+**
