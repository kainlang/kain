# UE5 Editor Pipeline - Current Status & Roadmap

## Executive Summary

The KAIN UE5 Editor pipeline (`kain/src/codegen/ue5editor.rs`) has **foundational infrastructure** in place but needs **significant expansion** to match the game code pipeline's maturity. Currently supports basic Slate widgets, asset types, and editor modules, but lacks advanced features like custom viewports, sequencers, detail customizations, and comprehensive widget composition.

**Goal:** Generate massive "god component" editor tool files with everything from Slate widgets to custom viewports, sequencers, detail panels, and toolbars - all in one shot, just like `ultimate_test.kn` does for game code.

---

## Current Implementation (What Works)

### ✅ Basic Slate Widgets (`@slate`)
**Location:** `kain/src/codegen/ue5editor.rs:184-273`

**Capabilities:**
- Generates `SCompoundWidget` subclasses
- `SLATE_BEGIN_ARGS` / `SLATE_END_ARGS` macros
- `SLATE_ARGUMENT`, `SLATE_ATTRIBUTE`, `SLATE_EVENT` support
- `SLATE_DEFAULT_SLOT` for content
- Basic `Construct()` method generation
- Simple widget tree composition (limited)

**Example:**
```kn
@slate
struct MaterialBrowserWindow:
    WindowTitle: Text
    @argument
    Materials: Array<MaterialData>
    OnMaterialSelected: void
```

**Generates:**
```cpp
class SMaterialBrowserWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMaterialBrowserWindow)
        : _Content()
        , _WindowTitle(FText::GetEmpty())
        , _Materials()
        , _OnMaterialSelected(FSimpleDelegate())
    {}
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_ATTRIBUTE(FText, WindowTitle)
    SLATE_ARGUMENT(TArray<FMaterialData>, Materials)
    SLATE_EVENT(FSimpleDelegate, OnMaterialSelected)
    SLATE_END_ARGS()
    
    void Construct(const FArguments& InArgs);
};
```

**Limitations:**
- Widget composition is **very basic** (only handles simple expressions)
- No support for complex widget hierarchies (SVerticalBox, SHorizontalBox, etc.)
- No support for Slate declarative syntax chaining
- No support for slot configuration (padding, alignment, etc.)
- Missing common widgets: SButton, STextBlock, SImage, SScrollBox, etc.

---

### ✅ Asset Types (`@asset_type`)
**Location:** `kain/src/codegen/ue5editor.rs:443-477`

**Capabilities:**
- Generates `UDataAsset` subclasses
- Generates `FAssetTypeActions_Base` subclasses
- Basic asset type registration
- Blueprint-accessible properties

**Example:**
```kn
@asset_type
struct AbilityData:
    ability_id: Int
    ability_name: String
    cooldown: Float
```

**Generates:**
```cpp
UCLASS(BlueprintType)
class GENERATED_API UAbilityData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    int32 ability_id;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    FString ability_name;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    float cooldown;
};

class FAssetTypeActions_AbilityData : public FAssetTypeActions_Base
{
public:
    virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AbilityData", "AbilityData"); }
    virtual FColor GetTypeColor() const override { return FColor::White; }
    virtual UClass* GetSupportedClass() const override { return UAbilityData::StaticClass(); }
    virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};
```

**Limitations:**
- No custom asset actions (Open, Edit, Reimport, etc.)
- No asset factory generation (can't create new assets in editor)
- No thumbnail rendering
- No asset context menu customization

---

### ✅ Editor Modules (`@editor_module`)
**Location:** `kain/src/codegen/ue5editor.rs:405-441`

**Capabilities:**
- Generates `IModuleInterface` subclasses
- `StartupModule()` / `ShutdownModule()` methods
- Basic module registration with `IMPLEMENT_MODULE`

**Example:**
```kn
@editor_module
struct MaterialLibraryEditor:
    EditorVersion: String
```

**Generates:**
```cpp
class FMaterialLibraryEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

void FMaterialLibraryEditorModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("MaterialLibraryEditor has started!"));
}

void FMaterialLibraryEditorModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MaterialLibraryEditor has shut down!"));
}

IMPLEMENT_MODULE(FMaterialLibraryEditorModule, MaterialLibraryEditor)
```

**Limitations:**
- No menu/toolbar registration
- No asset type registration in module
- No editor mode registration
- No detail customization registration
- No custom placement mode registration

---

## Missing Features (What's Needed)

### ❌ Custom Viewports (`@viewport`)
**Status:** Not implemented

**What's Needed:**
- Generate `SEditorViewport` subclasses
- Generate `FEditorViewportClient` subclasses
- Camera controls (orbit, pan, zoom)
- Viewport rendering (preview meshes, materials, particles)
- Viewport overlays (stats, debug info)
- Input handling (mouse, keyboard)

**Example Target:**
```kn
@viewport
struct MaterialPreviewViewport:
    @argument
    PreviewMesh: StaticMesh
    @argument
    PreviewMaterial: Material
    CameraDistance: Float
    CameraRotation: Vec2
    ShowGrid: Bool
    ShowStats: Bool
```

**Should Generate:**
```cpp
class SMaterialPreviewViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SMaterialPreviewViewport) {}
        SLATE_ARGUMENT(UStaticMesh*, PreviewMesh)
        SLATE_ARGUMENT(UMaterial*, PreviewMaterial)
    SLATE_END_ARGS()
    
    void Construct(const FArguments& InArgs);
    
protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
    
private:
    TSharedPtr<FMaterialPreviewViewportClient> ViewportClient;
    UStaticMesh* PreviewMesh;
    UMaterial* PreviewMaterial;
    float CameraDistance;
    FVector2D CameraRotation;
    bool bShowGrid;
    bool bShowStats;
};

class FMaterialPreviewViewportClient : public FEditorViewportClient
{
public:
    FMaterialPreviewViewportClient(/* params */);
    
    virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad) override;
    
private:
    UStaticMesh* PreviewMesh;
    UMaterial* PreviewMaterial;
    FPreviewScene PreviewScene;
};
```

---

### ❌ Sequencer Integration (`@sequencer`)
**Status:** Not implemented

**What's Needed:**
- Generate `ISequencerModule` integration
- Generate custom sequencer tracks
- Generate custom sequencer sections
- Timeline UI integration
- Keyframe editing
- Track evaluation

**Example Target:**
```kn
@sequencer
struct AbilitySequencerTrack:
    TrackName: String
    TrackColor: Color
    SupportedTypes: Array<String>
```

---

### ❌ Detail Customizations (`@details`)
**Status:** Not implemented

**What's Needed:**
- Generate `IDetailCustomization` subclasses
- Custom property layouts
- Custom property widgets
- Property visibility control
- Property validation
- Custom categories and sections

**Example Target:**
```kn
@details
struct WeaponStatsCustomization:
    TargetClass: String = "WeaponStats"
    CustomCategories: Array<String>
```

**Should Generate:**
```cpp
class FWeaponStatsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
    
private:
    void CustomizeWeaponCategory(IDetailLayoutBuilder& DetailBuilder);
    void CustomizeCombatCategory(IDetailLayoutBuilder& DetailBuilder);
};
```

---

### ❌ Toolbars & Menus (`@toolbar`, `@menu`)
**Status:** Not implemented

**What's Needed:**
- Generate toolbar button registration
- Generate menu command registration
- Generate command lists
- Icon/style integration
- Keyboard shortcuts

**Example Target:**
```kn
@toolbar
struct MaterialEditorToolbar:
    Buttons: Array<ToolbarButton>
    
struct ToolbarButton:
    Label: String
    Icon: String
    Tooltip: String
    Command: String
```

---

### ❌ Property Editors (`@property_editor`)
**Status:** Not implemented

**What's Needed:**
- Generate `IPropertyTypeCustomization` subclasses
- Custom property widgets (color pickers, sliders, etc.)
- Property metadata handling
- Property change notifications

---

### ❌ Asset Factories (`@asset_factory`)
**Status:** Not implemented

**What's Needed:**
- Generate `UFactory` subclasses
- Asset creation dialogs
- Import/export support
- Asset initialization

---

### ❌ Editor Modes (`@editor_mode`)
**Status:** Not implemented

**What's Needed:**
- Generate `FEdMode` subclasses
- Mode toolbar integration
- Mode-specific input handling
- Mode-specific rendering

---

### ❌ Placement Mode Extensions (`@placement`)
**Status:** Not implemented

**What's Needed:**
- Generate placement mode category registration
- Custom placeable actors
- Placement preview rendering

---

### ❌ Advanced Widget Composition
**Status:** Partially implemented (very basic)

**What's Needed:**
- Full Slate declarative syntax support
- Widget hierarchy generation from KAIN expressions
- Slot configuration (padding, alignment, fill, auto)
- Common widget library (SButton, STextBlock, SImage, etc.)
- Layout widgets (SVerticalBox, SHorizontalBox, SGridPanel, etc.)
- Container widgets (SScrollBox, SBorder, SOverlay, etc.)

**Example Target:**
```kn
@slate
struct MaterialBrowserWindow:
    WindowTitle: Text
    
    fn Compose():
        return SVerticalBox()
            .Add(
                SHorizontalBox()
                    .Add(STextBlock().Text("Search:"))
                    .Add(SEditableTextBox().HintText("Filter materials..."))
            )
            .Add(
                SScrollBox()
                    .Add(MaterialThumbnailGrid())
            )
            .Add(
                SHorizontalBox()
                    .Add(SButton().Text("Import").OnClicked(OnImportClicked))
                    .Add(SButton().Text("Export").OnClicked(OnExportClicked))
            )
```

**Should Generate:**
```cpp
void SMaterialBrowserWindow::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)
        +SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(FText::FromString("Search:"))
            ]
            +SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SEditableTextBox)
                .HintText(FText::FromString("Filter materials..."))
            ]
        ]
        +SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)
            +SScrollBox::Slot()
            [
                SNew(SMaterialThumbnailGrid)
            ]
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot()
            [
                SNew(SButton)
                .Text(FText::FromString("Import"))
                .OnClicked(this, &SMaterialBrowserWindow::OnImportClicked)
            ]
            +SHorizontalBox::Slot()
            [
                SNew(SButton)
                .Text(FText::FromString("Export"))
                .OnClicked(this, &SMaterialBrowserWindow::OnExportClicked)
            ]
        ]
    ];
}
```

---

## Comparison: Game Code vs Editor Code Pipeline

| Feature | Game Code (`ue5.rs`) | Editor Code (`ue5editor.rs`) | Status |
|---------|---------------------|------------------------------|--------|
| **Actors** | ✅ Full support | N/A | Complete |
| **Components** | ✅ Full support | N/A | Complete |
| **Structs** | ✅ Full support | ✅ Basic support | Partial |
| **Enums** | ✅ Full support | ✅ Basic support | Partial |
| **Delegates** | ✅ Full support | ⚠️ Limited support | Partial |
| **RPCs** | ✅ Full support | N/A | Complete |
| **Replication** | ✅ Full support | N/A | Complete |
| **Shaders** | ✅ Full support | N/A | Complete |
| **Blueprint Functions** | ✅ Full support | N/A | Complete |
| **Slate Widgets** | N/A | ⚠️ Basic support | **Needs Work** |
| **Asset Types** | N/A | ⚠️ Basic support | **Needs Work** |
| **Editor Modules** | N/A | ⚠️ Basic support | **Needs Work** |
| **Viewports** | N/A | ❌ Not implemented | **Missing** |
| **Sequencers** | N/A | ❌ Not implemented | **Missing** |
| **Detail Customizations** | N/A | ❌ Not implemented | **Missing** |
| **Toolbars/Menus** | N/A | ❌ Not implemented | **Missing** |
| **Property Editors** | N/A | ❌ Not implemented | **Missing** |
| **Asset Factories** | N/A | ❌ Not implemented | **Missing** |
| **Editor Modes** | N/A | ❌ Not implemented | **Missing** |

**Maturity Score:**
- Game Code Pipeline: **95%** (production-ready)
- Editor Code Pipeline: **25%** (early prototype)

---

## Roadmap to Parity

### Phase 1: Complete Slate Widget System (2-3 days)
**Priority:** HIGH

**Tasks:**
1. Implement full widget composition from KAIN expressions
2. Add support for all common Slate widgets:
   - Layout: `SVerticalBox`, `SHorizontalBox`, `SGridPanel`, `SUniformGridPanel`
   - Containers: `SScrollBox`, `SBorder`, `SOverlay`, `SSplitter`
   - Controls: `SButton`, `SCheckBox`, `SComboBox`, `SEditableTextBox`, `SSlider`
   - Display: `STextBlock`, `SImage`, `SProgressBar`, `SThrobber`
3. Add slot configuration support (padding, alignment, fill, auto)
4. Add style/brush support
5. Add event handler generation (OnClicked, OnValueChanged, etc.)
6. Test with complex widget hierarchies

**Deliverable:** Generate production-ready Slate UIs from KAIN code

---

### Phase 2: Custom Viewports (2-3 days)
**Priority:** HIGH

**Tasks:**
1. Implement `@viewport` attribute
2. Generate `SEditorViewport` subclasses
3. Generate `FEditorViewportClient` subclasses
4. Add camera control generation (orbit, pan, zoom)
5. Add preview scene setup
6. Add input handling
7. Add viewport overlay support
8. Test with material/mesh preview viewports

**Deliverable:** Generate custom editor viewports from KAIN code

---

### Phase 3: Detail Customizations (2 days)
**Priority:** MEDIUM

**Tasks:**
1. Implement `@details` attribute
2. Generate `IDetailCustomization` subclasses
3. Add custom property layout generation
4. Add property visibility control
5. Add custom category/section generation
6. Test with complex property layouts

**Deliverable:** Generate detail panel customizations from KAIN code

---

### Phase 4: Asset System Completion (2 days)
**Priority:** MEDIUM

**Tasks:**
1. Implement `@asset_factory` attribute
2. Generate `UFactory` subclasses
3. Add asset creation dialog generation
4. Add import/export support
5. Enhance `@asset_type` with custom actions
6. Add thumbnail rendering support
7. Test with custom asset types

**Deliverable:** Complete asset creation/editing pipeline

---

### Phase 5: Toolbars & Menus (1-2 days)
**Priority:** MEDIUM

**Tasks:**
1. Implement `@toolbar` and `@menu` attributes
2. Generate toolbar button registration
3. Generate menu command registration
4. Add command list generation
5. Add icon/style integration
6. Add keyboard shortcut support
7. Test with custom editor toolbars

**Deliverable:** Generate editor toolbars and menus from KAIN code

---

### Phase 6: Advanced Features (3-4 days)
**Priority:** LOW (nice-to-have)

**Tasks:**
1. Implement `@sequencer` attribute (custom sequencer tracks)
2. Implement `@property_editor` attribute (custom property widgets)
3. Implement `@editor_mode` attribute (custom editor modes)
4. Implement `@placement` attribute (placement mode extensions)
5. Add graph editor support (for node-based editors)
6. Test with complex editor tools

**Deliverable:** Full-featured editor tool generation

---

## Implementation Strategy

### 1. Extend `Ue5EditorGen` struct
Add new methods for each feature:
```rust
impl Ue5EditorGen {
    fn gen_viewport(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_detail_customization(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_toolbar(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_menu(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_asset_factory(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_sequencer_track(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_property_editor(&mut self, st: &TypedStruct) { /* ... */ }
    fn gen_editor_mode(&mut self, st: &TypedStruct) { /* ... */ }
}
```

### 2. Enhance Widget Composition
Rewrite `gen_slate_expr` to handle full Slate declarative syntax:
```rust
fn gen_slate_expr(&mut self, expr: &Expr) {
    match expr {
        Expr::Call { callee, args, .. } => {
            // SNew(SVerticalBox)
            let widget_type = self.extract_widget_type(callee);
            self.write_source(&format!("SNew({})", widget_type));
        }
        Expr::MethodCall { receiver, method, args, .. } => {
            match method.as_str() {
                "Add" => {
                    // +SVerticalBox::Slot()
                    let parent_type = self.infer_parent_type(receiver);
                    self.write_source(&format!("+{}::Slot()", parent_type));
                    self.gen_slot_config(args);
                    self.write_source("[");
                    self.push_indent();
                    self.gen_slate_expr(&args[0].value);
                    self.pop_indent();
                    self.write_source("]");
                }
                _ => {
                    // .Text(...), .OnClicked(...), etc.
                    self.gen_slate_expr(receiver);
                    self.write_source(&format!(".{}({})", method, self.gen_args(args)));
                }
            }
        }
        _ => { /* ... */ }
    }
}
```

### 3. Add Template Support
Create Jinja2 templates for complex editor code:
```
kain/src/ue5/templates/
├── viewport_header.jinja
├── viewport_source.jinja
├── detail_customization.jinja
├── asset_factory.jinja
├── toolbar_registration.jinja
└── menu_registration.jinja
```

### 4. Extend `Ue5Context`
Add editor-specific tracking:
```rust
pub struct Ue5Context {
    // Existing fields...
    
    // Editor-specific
    pub viewports: HashMap<String, String>,
    pub detail_customizations: HashMap<String, String>,
    pub asset_factories: HashMap<String, String>,
    pub toolbars: Vec<String>,
    pub menus: Vec<String>,
}
```

---

## Testing Strategy

### Test Files Needed:
1. `testing/editor/simple_slate.kn` - Basic Slate widgets
2. `testing/editor/complex_slate.kn` - Complex widget hierarchies
3. `testing/editor/viewport_test.kn` - Custom viewports
4. `testing/editor/details_test.kn` - Detail customizations
5. `testing/editor/asset_test.kn` - Asset types + factories
6. `testing/editor/toolbar_test.kn` - Toolbars and menus
7. `testing/editor/ultimate_editor.kn` - **GOD COMPONENT** (everything)

### Ultimate Editor Test (Target):
```kn
# Ultimate Editor Tool - Everything in One File
# Generates a complete editor plugin with:
# - Custom asset types
# - Slate UI windows
# - Custom viewports
# - Detail customizations
# - Toolbars and menus
# - Asset factories
# - Editor modes

@asset_type
struct GameplayAbility:
    ability_id: Int
    ability_name: String
    cooldown: Float
    damage: Float
    range: Float

@asset_factory
struct GameplayAbilityFactory:
    TargetClass: String = "GameplayAbility"
    DefaultCooldown: Float = 5.0

@slate
struct AbilityEditorWindow:
    WindowTitle: Text = "Ability Editor"
    
    fn Compose():
        return SVerticalBox()
            .Add(AbilityToolbar())
            .Add(
                SHorizontalBox()
                    .Add(AbilityListPanel())
                    .Add(AbilityDetailsPanel())
                    .Add(AbilityPreviewViewport())
            )
            .Add(AbilityStatusBar())

@viewport
struct AbilityPreviewViewport:
    @argument
    PreviewAbility: GameplayAbility
    CameraDistance: Float = 500.0
    ShowGrid: Bool = true

@details
struct GameplayAbilityCustomization:
    TargetClass: String = "GameplayAbility"

@toolbar
struct AbilityEditorToolbar:
    Buttons: Array<ToolbarButton>

@menu
struct AbilityEditorMenu:
    MenuName: String = "Ability"
    Commands: Array<MenuCommand>

@editor_module
struct AbilityEditorSuite:
    ModuleVersion: String = "1.0.0"
```

**Expected Output:**
- 15-20 generated files
- 50,000+ lines of C++
- Production-ready editor plugin
- Zero manual edits required

---

## Success Criteria

### Phase 1 Complete When:
- ✅ Can generate complex Slate UIs with 10+ widget types
- ✅ Widget hierarchies match UE5 declarative syntax exactly
- ✅ All slot configurations work (padding, alignment, fill)
- ✅ Event handlers generate correctly
- ✅ Test file compiles and runs in UE5 Editor

### Phase 2 Complete When:
- ✅ Can generate custom viewports with preview scenes
- ✅ Camera controls work (orbit, pan, zoom)
- ✅ Viewport rendering works (meshes, materials)
- ✅ Input handling works
- ✅ Test viewport compiles and runs in UE5 Editor

### Phase 3 Complete When:
- ✅ Can generate detail customizations
- ✅ Custom property layouts work
- ✅ Property visibility control works
- ✅ Test customization compiles and runs in UE5 Editor

### Phase 4 Complete When:
- ✅ Can generate asset factories
- ✅ Asset creation works in editor
- ✅ Import/export works
- ✅ Test asset type works end-to-end

### Phase 5 Complete When:
- ✅ Can generate toolbars and menus
- ✅ Toolbar buttons work
- ✅ Menu commands work
- ✅ Test toolbar/menu compiles and runs

### Phase 6 Complete When:
- ✅ All advanced features implemented
- ✅ Ultimate editor test generates complete plugin
- ✅ Plugin compiles and runs in UE5 Editor
- ✅ All features work end-to-end

---

## Conclusion

The UE5 Editor pipeline is **25% complete** compared to the game code pipeline. To reach parity and enable "god component" editor tool generation, we need:

1. **Complete Slate widget system** (highest priority)
2. **Custom viewports** (high priority)
3. **Detail customizations** (medium priority)
4. **Asset system completion** (medium priority)
5. **Toolbars & menus** (medium priority)
6. **Advanced features** (low priority, nice-to-have)

**Estimated Time:** 12-17 days of focused development

**Payoff:** Generate complete, production-ready UE5 editor plugins from KAIN code with zero manual edits - matching the game code pipeline's 10-30x speed advantage.

**Next Steps:**
1. Start with Phase 1 (Slate widgets) - highest ROI
2. Create test files for each phase
3. Iterate until tests pass
4. Move to next phase

This will enable the **marketplace domination strategy** for editor tools, not just game code.
