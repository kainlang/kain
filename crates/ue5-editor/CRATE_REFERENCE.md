# UE5 Editor Codegen Crate Reference

> **Last Updated:** 2026-02-20  
> **Purpose:** Complete reference for the `ue5-editor` crate - generates Unreal Engine 5 editor UI and tooling  
> **Status:** Production-ready - 10 tests passing, comprehensive Slate/Details/Viewport/Toolbar/Asset Editor generation

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Slate Widget System](#slate-widget-system)
4. [Details Panel System](#details-panel-system)
5. [Viewport System](#viewport-system)
6. [Toolbar System](#toolbar-system)
7. [Asset Editor System](#asset-editor-system)
8. [Editor Module System](#editor-module-system)
9. [Data Asset Writer](#data-asset-writer)
10. [File Structure](#file-structure)
11. [Examples](#examples)

---

## Overview

The `ue5-editor` crate is the **editor UI code generator** for the KAIN compiler. It transforms KAIN AST into production-ready Unreal Engine 5 editor extensions with Slate UI, custom viewports, detail panels, toolbars, and complete asset editors.

### What It Generates

- **Slate Widgets** - SCompoundWidget subclasses with SLATE_BEGIN_ARGS/SLATE_END_ARGS
- **Details Panels** - IDetailCustomization subclasses with property overrides (@slider, @color_picker, @button)
- **Viewports** - SEditorViewport + FEditorViewportClient pairs with 3D preview
- **Toolbars** - FToolBarBuilder extensions with buttons, toggles, separators
- **Asset Editors** - FAssetEditorToolkit subclasses wiring viewport + details + toolbar
- **Editor Modules** - IModuleInterface with menu entries and toolbar buttons
- **Data Assets** - UDataAsset subclasses for configuration

### Key Features

- **Smart Slot Awareness** - Automatically detects VBox/HBox/Overlay and generates correct slot syntax
- **Delegate Bridging** - Converts KAIN lambdas to Slate delegates (CreateSP, CreateLambda)
- **Widget Composition** - Nested widget trees with proper parent-child relationships
- **Property Customization** - Metadata-driven detail panel layouts
- **Shader Brush Integration** - FSlateBrush with shader material support
- **List View Support** - TListView/TileView with item generation
- **Reactive Updates** - State changes trigger UI refresh

---

## Architecture

### Entry Points

```rust
// Main entry point - generates all editor items
pub fn generate(program: &TypedProgram, plugin_name: &str, copyright: Option<&str>) -> KainResult<Ue5EditorOutput>

// With runtime context (includes EngineKnowledge)
pub fn generate_with_context(program: &TypedProgram, plugin_name: &str, runtime_context: Option<Ue5Context>, copyright: Option<&str>) -> KainResult<Ue5EditorOutput>

// Per-item generation (modular output)
pub fn generate_per_item(program: &TypedProgram, plugin_name: &str, copyright: Option<&str>) -> KainResult<Vec<EditorItem>>
```

### Output Structure

```rust
pub struct Ue5EditorOutput {
    pub header: String,              // .h file content
    pub source: String,              // .cpp file content
}

pub struct EditorItem {
    pub name: String,                // Item name (e.g., "HealthBar")
    pub kind: String,                // "slate", "details", "viewport", etc.
    pub header: String,              // .h content
    pub source: String,              // .cpp content
}
```

### Core Components

1. **SlateGenerator** - Slate widget tree → SNew() chains
2. **DetailsGenerator** - IDetailCustomization with property overrides
3. **ViewportGenerator** - SEditorViewport + FEditorViewportClient
4. **ToolbarGenerator** - FToolBarBuilder with commands
5. **AssetEditorGenerator** - FAssetEditorToolkit orchestration
6. **EditorModuleGenerator** - IModuleInterface with registration

### Compilation Flow

```
TypedProgram → Ue5EditorGen
    ↓
Scan for editor attributes:
    - @slate → SlateGenerator
    - @details → DetailsGenerator
    - @viewport → ViewportGenerator
    - @toolbar → ToolbarGenerator
    - @asset_editor → AssetEditorGenerator
    - @editor_module → EditorModuleGenerator
    ↓
Per-Item Codegen:
    - Generate .h (class declaration)
    - Generate .cpp (implementation)
    ↓
Ue5EditorOutput { header, source }
```

---

## Slate Widget System

Slate is UE5's immediate-mode UI framework. KAIN generates SCompoundWidget subclasses with declarative widget trees.

### Basic Slate Widget

**KAIN:**
```kain
@slate
struct HealthBar:
    @property
    current_health: Float
    
    @property
    max_health: Float
    
    fn construct() -> Widget:
        return VBox(
            Text("Health: {current_health}/{max_health}"),
            ProgressBar(
                percent: current_health / max_health,
                fill_color: color("red")
            )
        )
```

**Generated C++:**
```cpp
class SHealthBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHealthBar)
        : _current_health(0.0f)
        , _max_health(100.0f)
    {}
        SLATE_ARGUMENT(float, current_health)
        SLATE_ARGUMENT(float, max_health)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    float current_health;
    float max_health;
};

void SHealthBar::Construct(const FArguments& InArgs)
{
    current_health = InArgs._current_health;
    max_health = InArgs._max_health;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("Health: %.1f/%.1f"), current_health, max_health)))
        ]
        + SVerticalBox::Slot()
        [
            SNew(SProgressBar)
            .Percent(current_health / max_health)
            .FillColorAndOpacity(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f))
        ]
    ];
}
```

### Supported Widgets

| KAIN Widget | Slate Class | Purpose |
|-------------|-------------|---------|
| `VBox` | `SVerticalBox` | Vertical layout |
| `HBox` | `SHorizontalBox` | Horizontal layout |
| `Overlay` | `SOverlay` | Layered widgets |
| `Text` | `STextBlock` | Static text |
| `EditableText` | `SEditableTextBox` | Text input |
| `Button` | `SButton` | Clickable button |
| `CheckBox` | `SCheckBox` | Toggle checkbox |
| `Slider` | `SSlider` | Numeric slider |
| `ProgressBar` | `SProgressBar` | Progress indicator |
| `Image` | `SImage` | Image display |
| `Spacer` | `SSpacer` | Empty space |
| `Separator` | `SSeparator` | Visual divider |
| `ScrollBox` | `SScrollBox` | Scrollable container |
| `Border` | `SBorder` | Bordered container |
| `Canvas` | `SCanvas` | Absolute positioning |
| `GridPanel` | `SGridPanel` | Grid layout |
| `UniformGridPanel` | `SUniformGridPanel` | Uniform grid |
| `WrapBox` | `SWrapBox` | Wrapping layout |
| `Splitter` | `SSplitter` | Resizable split |
| `ListView` | `SListView` | List of items |
| `TileView` | `STileView` | Tile grid |
| `TreeView` | `STreeView` | Hierarchical tree |
| `ComboBox` | `SComboBox` | Dropdown menu |
| `SpinBox` | `SSpinBox` | Numeric spinner |
| `ColorBlock` | `SColorBlock` | Color display |
| `ColorPicker` | `SColorPicker` | Color selector |

### Widget Properties

Widgets support method-chaining for properties:

**KAIN:**
```kain
Button(
    text: "Click Me",
    on_clicked: handle_click,
    tooltip: "This is a button",
    enabled: is_enabled
)
```

**Generated C++:**
```cpp
SNew(SButton)
.Text(FText::FromString(TEXT("Click Me")))
.OnClicked(this, &SMyWidget::handle_click)
.ToolTipText(FText::FromString(TEXT("This is a button")))
.IsEnabled(is_enabled)
```

### Slot Configuration

Containers like VBox/HBox support slot properties:

**KAIN:**
```kain
VBox(
    Text("Header")
        .padding(10)
        .h_align(HAlign::Center),
    
    Button("Action")
        .fill_height(1.0)
        .v_align(VAlign::Bottom)
)
```

**Generated C++:**
```cpp
SNew(SVerticalBox)
+ SVerticalBox::Slot()
.Padding(10.0f)
.HAlign(HAlign_Center)
[
    SNew(STextBlock)
    .Text(FText::FromString(TEXT("Header")))
]
+ SVerticalBox::Slot()
.FillHeight(1.0f)
.VAlign(VAlign_Bottom)
[
    SNew(SButton)
    .Text(FText::FromString(TEXT("Action")))
]
```

### Delegate Binding

KAIN lambdas are converted to Slate delegates:

**KAIN:**
```kain
@slate
struct Counter:
    state count: Int = 0
    
    fn increment():
        count = count + 1
    
    fn construct() -> Widget:
        return VBox(
            Text("Count: {count}"),
            Button(
                text: "Increment",
                on_clicked: || increment()
            )
        )
```

**Generated C++:**
```cpp
SNew(SButton)
.Text(FText::FromString(TEXT("Increment")))
.OnClicked_Lambda([this]() {
    increment();
    return FReply::Handled();
})
```

### List View Support

KAIN generates TListView with item generation:

**KAIN:**
```kain
@slate
struct ItemList:
    @list_data
    items: Array<String>
    
    fn construct() -> Widget:
        return ListView(
            items: items,
            on_generate_row: |item| Text(item)
        )
```

**Generated C++:**
```cpp
SNew(SListView<TSharedPtr<FString>>)
.ListItemsSource(&items)
.OnGenerateRow(this, &SItemList::OnGenerateRow)

TSharedRef<ITableRow> SItemList::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
    [
        SNew(STextBlock)
        .Text(FText::FromString(*Item))
    ];
}
```

---

## Details Panel System

Details panels customize property display in the UE5 editor. KAIN generates IDetailCustomization subclasses.

### Basic Details Panel

**KAIN:**
```kain
@details
struct WeaponDetails:
    @slider(min: 0.0, max: 100.0)
    damage: Float
    
    @color_picker
    glow_color: Vec3
    
    @button(label: "Test Fire")
    fn on_test_fire():
        println("Weapon fired!")
```

**Generated C++:**
```cpp
class FWeaponDetailsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply OnTestFire();
};

void FWeaponDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Weapon"));

    // Slider for damage
    TSharedPtr<IPropertyHandle> DamageProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWeapon, damage));
    Category.AddProperty(DamageProperty)
        .CustomWidget()
        .NameContent()
        [
            DamageProperty->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            SNew(SSpinBox<float>)
            .MinValue(0.0f)
            .MaxValue(100.0f)
            .Value(this, &FWeaponDetailsCustomization::GetDamageValue)
            .OnValueChanged(this, &FWeaponDetailsCustomization::SetDamageValue)
        ];

    // Color picker for glow_color
    TSharedPtr<IPropertyHandle> GlowColorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWeapon, glow_color));
    Category.AddProperty(GlowColorProperty)
        .CustomWidget()
        .NameContent()
        [
            GlowColorProperty->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            SNew(SColorPicker)
            .TargetColorAttribute(this, &FWeaponDetailsCustomization::GetGlowColor)
            .OnColorCommitted(this, &FWeaponDetailsCustomization::SetGlowColor)
        ];

    // Button for test fire
    Category.AddCustomRow(FText::FromString(TEXT("Test Fire")))
        .NameContent()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Test Fire")))
        ]
        .ValueContent()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Test Fire")))
            .OnClicked(this, &FWeaponDetailsCustomization::OnTestFire)
        ];
}
```

### Supported Property Overrides

| KAIN Attribute | Widget | Purpose |
|----------------|--------|---------|
| `@slider(min, max)` | `SSpinBox` | Numeric slider with range |
| `@color_picker` | `SColorPicker` | Color selection |
| `@button(label)` | `SButton` | Action button |
| `@dropdown(options)` | `SComboBox` | Dropdown menu |
| `@text_box` | `SEditableTextBox` | Text input |
| `@checkbox` | `SCheckBox` | Boolean toggle |
| `@file_picker` | `SFilePathPicker` | File browser |
| `@asset_picker(class)` | `SObjectPropertyEntryBox` | Asset reference |
| `@multiline_text` | `SMultiLineEditableTextBox` | Multi-line text |
| `@vector_input` | `SVectorInputBox` | Vector3 input |
| `@rotator_input` | `SRotatorInputBox` | Rotator input |

### Category Grouping

Properties are automatically grouped by category:

**KAIN:**
```kain
@details
struct CharacterDetails:
    @category("Combat")
    @slider(min: 0, max: 1000)
    health: Float
    
    @category("Combat")
    @slider(min: 0, max: 100)
    armor: Float
    
    @category("Movement")
    @slider(min: 0, max: 1000)
    speed: Float
```

**Generated C++:**
```cpp
IDetailCategoryBuilder& CombatCategory = DetailBuilder.EditCategory(TEXT("Combat"));
CombatCategory.AddProperty(HealthProperty);
CombatCategory.AddProperty(ArmorProperty);

IDetailCategoryBuilder& MovementCategory = DetailBuilder.EditCategory(TEXT("Movement"));
MovementCategory.AddProperty(SpeedProperty);
```

### Visibility Conditions

Properties can be conditionally visible:

**KAIN:**
```kain
@details
struct EffectDetails:
    enable_glow: Bool
    
    @visible_if(enable_glow)
    @color_picker
    glow_color: Vec3
```

**Generated C++:**
```cpp
TSharedPtr<IPropertyHandle> GlowColorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UEffect, glow_color));
GlowColorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FEffectDetailsCustomization::OnEnableGlowChanged));

// Visibility check
EVisibility FEffectDetailsCustomization::GetGlowColorVisibility() const
{
    return enable_glow ? EVisibility::Visible : EVisibility::Collapsed;
}
```

---

## Viewport System

Viewports provide 3D preview rendering in the editor. KAIN generates SEditorViewport + FEditorViewportClient pairs.

### Basic Viewport

**KAIN:**
```kain
@viewport
struct WeaponPreview:
    @scene_actor
    weapon_mesh: StaticMeshComponent
    
    @camera
    preview_camera: CameraComponent
    
    fn on_viewport_tick(delta: Float):
        // Rotate weapon for preview
        weapon_mesh.AddLocalRotation(vec3(0, delta * 45, 0))
```

**Generated C++:**
```cpp
class SWeaponPreviewViewport : public SEditorViewport
{
public:
    void Construct(const FArguments& InArgs);

protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
    TSharedPtr<FWeaponPreviewViewportClient> ViewportClient;
};

class FWeaponPreviewViewportClient : public FEditorViewportClient
{
public:
    FWeaponPreviewViewportClient(FPreviewScene& InPreviewScene);

    virtual void Tick(float DeltaSeconds) override;

private:
    UStaticMeshComponent* weapon_mesh;
    UCameraComponent* preview_camera;
};

void FWeaponPreviewViewportClient::Tick(float DeltaSeconds)
{
    FEditorViewportClient::Tick(DeltaSeconds);

    // User code
    weapon_mesh->AddLocalRotation(FRotator(0, DeltaSeconds * 45.0f, 0));
}
```

### Viewport Features

- **Scene Actors** - `@scene_actor` spawns actors in preview scene
- **Camera Control** - `@camera` sets viewport camera
- **Tick Handler** - `on_viewport_tick()` for animation
- **Input Handling** - Mouse/keyboard events
- **Gizmo Support** - Transform widgets
- **Grid Display** - Optional grid overlay

---

## Toolbar System

Toolbars provide quick actions in the editor. KAIN generates FToolBarBuilder extensions.

### Basic Toolbar

**KAIN:**
```kain
@toolbar
struct WeaponTools:
    @button(icon: "Icons.Weapon", tooltip: "Create Weapon")
    fn on_create_weapon():
        println("Creating weapon...")
    
    @toggle(label: "Show Grid")
    fn on_toggle_grid(enabled: Bool):
        println("Grid: {enabled}")
    
    @separator
    
    @dropdown(label: "Quality")
    fn on_quality_changed(value: String):
        println("Quality: {value}")
```

**Generated C++:**
```cpp
void FWeaponToolsCommands::RegisterCommands()
{
    UI_COMMAND(CreateWeapon, "Create Weapon", "Create Weapon", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(ToggleGrid, "Show Grid", "Show Grid", EUserInterfaceActionType::ToggleButton, FInputChord());
}

void FWeaponToolsToolbar::BuildToolbar(FToolBarBuilder& ToolbarBuilder)
{
    ToolbarBuilder.AddToolBarButton(
        FWeaponToolsCommands::Get().CreateWeapon,
        NAME_None,
        FText::FromString(TEXT("Create Weapon")),
        FText::FromString(TEXT("Create Weapon")),
        FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Weapon")
    );

    ToolbarBuilder.AddToolBarButton(
        FWeaponToolsCommands::Get().ToggleGrid,
        NAME_None,
        FText::FromString(TEXT("Show Grid")),
        FText::FromString(TEXT("Show Grid"))
    );

    ToolbarBuilder.AddSeparator();

    ToolbarBuilder.AddComboButton(
        FUIAction(),
        FOnGetContent::CreateSP(this, &FWeaponToolsToolbar::GenerateQualityMenu),
        FText::FromString(TEXT("Quality")),
        FText::FromString(TEXT("Quality"))
    );
}
```

---

## Asset Editor System

Asset editors combine viewport + details + toolbar into a complete editing experience. KAIN generates FAssetEditorToolkit subclasses.

### Basic Asset Editor

**KAIN:**
```kain
@asset_editor
struct WeaponEditor:
    @viewport
    preview: WeaponPreview
    
    @details
    properties: WeaponDetails
    
    @toolbar
    tools: WeaponTools
    
    fn on_asset_opened(asset: WeaponAsset):
        preview.weapon_mesh.SetStaticMesh(asset.mesh)
        properties.damage = asset.damage
```

**Generated C++:**
```cpp
class FWeaponEditorToolkit : public FAssetEditorToolkit
{
public:
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

    void InitEditor(const TArray<UObject*>& InObjects);

private:
    TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

    TSharedPtr<SWeaponPreviewViewport> ViewportWidget;
    TSharedPtr<IDetailsView> DetailsWidget;
    UWeaponAsset* EditingAsset;
};

void FWeaponEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    InTabManager->RegisterTabSpawner("Viewport", FOnSpawnTab::CreateSP(this, &FWeaponEditorToolkit::SpawnTab_Viewport));
    InTabManager->RegisterTabSpawner("Details", FOnSpawnTab::CreateSP(this, &FWeaponEditorToolkit::SpawnTab_Details));
}

TSharedRef<SDockTab> FWeaponEditorToolkit::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .Label(FText::FromString(TEXT("Viewport")))
        [
            ViewportWidget.ToSharedRef()
        ];
}
```

---

## Editor Module System

Editor modules register menu entries and toolbar buttons. KAIN generates IModuleInterface subclasses.

### Basic Editor Module

**KAIN:**
```kain
@editor_module
struct WeaponEditorModule:
    @menu_entry(path: "Tools/Weapons", label: "Open Weapon Editor")
    fn on_open_editor():
        println("Opening weapon editor...")
    
    @toolbar_button(section: "Content", icon: "Icons.Weapon")
    fn on_quick_create():
        println("Quick creating weapon...")
```

**Generated C++:**
```cpp
class FWeaponEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OnOpenEditor();
    void OnQuickCreate();

    TSharedPtr<FUICommandList> PluginCommands;
};

IMPLEMENT_MODULE(FWeaponEditorModule, WeaponEditor)

void FWeaponEditorModule::StartupModule()
{
    RegisterMenus();
}

void FWeaponEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
        FToolMenuSection& Section = Menu->FindOrAddSection("Weapons");
        Section.AddMenuEntry(
            "OpenWeaponEditor",
            FText::FromString(TEXT("Open Weapon Editor")),
            FText::FromString(TEXT("Open Weapon Editor")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FWeaponEditorModule::OnOpenEditor))
        );
    }

    {
        UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
        FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Content");
        Section.AddEntry(FToolMenuEntry::InitToolBarButton(
            "QuickCreate",
            FUIAction(FExecuteAction::CreateRaw(this, &FWeaponEditorModule::OnQuickCreate)),
            FText::FromString(TEXT("Quick Create")),
            FText::FromString(TEXT("Quick Create")),
            FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Weapon")
        ));
    }
}
```

---

## Data Asset Writer

The data asset writer generates UDataAsset subclasses for configuration.

**KAIN:**
```kain
@data_asset
struct GameConfig:
    difficulty: Float = 1.0
    max_players: Int = 4
    enable_pvp: Bool = false
```

**Generated C++:**
```cpp
UCLASS()
class GAME_API UGameConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float difficulty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 max_players;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool enable_pvp;

    UGameConfig()
        : difficulty(1.0f)
        , max_players(4)
        , enable_pvp(false)
    {}
};
```

---

## File Structure

```
crates/ue5-editor/
├── src/
│   ├── lib.rs                    # Public API exports
│   ├── data_asset_writer.rs     # UDataAsset generation
│   └── editor/
│       ├── mod.rs                # Module exports
│       ├── codegen.rs            # Main orchestrator (1400+ lines)
│       ├── slate.rs              # Slate widget generation (1700+ lines)
│       ├── details.rs            # Details panel generation (600+ lines)
│       ├── viewport.rs           # Viewport generation
│       ├── assets.rs             # Asset editor generation
│       ├── style.rs              # Slate style helpers
│       └── reactive.rs           # Reactive state management
├── tests/                        # Integration tests (10 passing)
├── Cargo.toml
└── CRATE_REFERENCE.md            # This file
```

---

## Examples

### Example 1: Complete System Health Dashboard

**KAIN:**
```kain
@slate
struct SystemHealthDashboard:
    state cpu_usage: Float = 0.0
    state memory_usage: Float = 0.0
    state disk_usage: Float = 0.0
    
    fn construct() -> Widget:
        return VBox(
            Text("System Health Dashboard")
                .font_size(24)
                .h_align(HAlign::Center),
            
            Separator(),
            
            HBox(
                VBox(
                    Text("CPU Usage"),
                    ProgressBar(
                        percent: cpu_usage / 100.0,
                        fill_color: color("blue")
                    ),
                    Text("{cpu_usage}%")
                ).fill_width(1.0),
                
                VBox(
                    Text("Memory Usage"),
                    ProgressBar(
                        percent: memory_usage / 100.0,
                        fill_color: color("green")
                    ),
                    Text("{memory_usage}%")
                ).fill_width(1.0),
                
                VBox(
                    Text("Disk Usage"),
                    ProgressBar(
                        percent: disk_usage / 100.0,
                        fill_color: color("orange")
                    ),
                    Text("{disk_usage}%")
                ).fill_width(1.0)
            )
        )
```

This generates a complete Slate widget with nested layouts, progress bars, and reactive state.

---

## Summary

The ue5-editor crate provides comprehensive editor UI generation for KAIN, transforming declarative widget trees into production-ready Slate code with full UE5 integration. It handles all the complexity of Slate's immediate-mode API, delegate binding, slot configuration, and widget composition, allowing developers to focus on UI logic rather than boilerplate.
