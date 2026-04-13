# UE5 Editor GODMODE - Enhancement Plan

## Current State (ue5editor.rs)

### ✅ What Works:
1. **Asset Type Generation** (`@asset_type`)
   - Generates UDataAsset classes
   - Auto-creates AssetTypeActions
   - Blueprint-ready properties

2. **Slate Widget Skeleton** (`@slate`)
   - Generates SCompoundWidget classes
   - SLATE_BEGIN_ARGS/SLATE_END_ARGS
   - Construct() method stub

3. **Editor Module** (`@editor_module`)
   - Generates IModuleInterface implementation
   - StartupModule/ShutdownModule
   - IMPLEMENT_MODULE macro

### ❌ What Needs Work:
1. **Compose() Method Body** - Not generating widget hierarchy
2. **Type Mappings** - MaterialPreset should be UMaterialPreset*
3. **SLATE_EVENT Detection** - Using wrong heuristics
4. **Widget DSL** - Limited support for Slate syntax
5. **UE5.7 Features** - Missing latest APIs

---

## GODMODE Vision: Generate Entire Editors in 20 Lines

### Example 1: Material Preset Editor (20 lines)
```kn
@asset_type
struct MaterialPreset:
    base_color: Color
    roughness: Float
    metallic: Float

@editor("MaterialPreset")
widget MaterialEditor:
    preset: MaterialPreset
    
    layout:
        VerticalBox:
            - Header("Material Properties")
            - PropertyGrid(preset)
            - HorizontalBox:
                - Button("Apply").OnClick(ApplyPreset)
                - Button("Reset").OnClick(ResetPreset)
```

**Generates:**
- Complete asset editor with toolbar
- Property panel with live preview
- Undo/redo support
- Auto-registration in asset registry
- Menu entries and shortcuts

### Example 2: Batch Asset Tool (15 lines)
```kn
@editor_utility
tool BatchTextureImporter:
    @input
    source_folder: String
    
    @input
    compression: TextureCompression
    
    @action("Import All")
    fn ImportTextures():
        for file in GetFiles(source_folder, "*.png"):
            ImportTexture(file, compression)
```

**Generates:**
- Editor utility widget
- File browser integration
- Progress bar
- Batch operation with cancellation
- Auto-adds to Tools menu

### Example 3: Custom Viewport (25 lines)
```kn
@viewport
widget MaterialPreview:
    material: Material
    mesh: StaticMesh
    
    camera:
        position: Vec3(0, -200, 0)
        fov: 90.0
    
    lighting:
        - DirectionalLight(intensity: 5.0)
        - SkyLight(intensity: 1.0)
    
    controls:
        - Orbit(mouse_button: Left)
        - Zoom(mouse_wheel: true)
```

**Generates:**
- Custom viewport with rendering
- Camera controls
- Lighting setup
- Material preview
- Screenshot/export tools

---

## Phase 1: Fix Current Issues (Priority: HIGH)

### 1.1 Fix Compose() Method Body Generation
**Problem:** Looking for last expression, but finding return statement

**Solution:**
```rust
// In gen_construct_body()
if let Some(last_stmt) = func.body.stmts.last() {
    match last_stmt {
        Stmt::Return(Some(expr), _) => {
            // Handle return expression
            self.write_source("ChildSlot");
            self.write_source("[");
            self.push_indent();
            self.gen_slate_expr(expr);
            self.pop_indent();
            self.write_source("];");
        },
        Stmt::Expr(expr) => {
            // Handle bare expression
            // ... existing code ...
        },
        _ => {}
    }
}
```

### 1.2 Fix Type Mappings for Asset References
**Problem:** `MaterialPreset` should be `UMaterialPreset*`

**Solution:**
```rust
fn map_type(&self, ty: &Type) -> String {
    match ty {
        Type::Named { name, .. } => {
            // Check if this is a known asset type
            if self.is_asset_type(name) {
                format!("U{}*", name)
            } else {
                match name.as_str() {
                    // ... existing mappings ...
                }
            }
        }
        // ... rest ...
    }
}
```

### 1.3 Fix SLATE_EVENT vs SLATE_ATTRIBUTE Detection
**Problem:** Using name heuristics (starts with "On")

**Solution:**
```rust
// Check field type, not name
let is_event = matches!(field.ty, Type::Named { name, .. } if self.is_delegate_type(name));

if is_event {
    self.write_header(&format!("SLATE_EVENT({}, {})", ty_str, name));
} else {
    let is_argument = field.attributes.iter().any(|a| a.name == "argument");
    if is_argument {
        self.write_header(&format!("SLATE_ARGUMENT({}, {})", ty_str, name));
    } else {
        self.write_header(&format!("SLATE_ATTRIBUTE({}, {})", ty_str, name));
    }
}
```

---

## Phase 2: Enhanced Slate DSL (Priority: HIGH)

### 2.1 Widget Hierarchy Generation
Support full Slate syntax:
```kn
VerticalBox:
    - TextBlock("Title").Font(TitleFont)
    - HorizontalBox:
        - Button("OK").OnClick(OnOK)
        - Button("Cancel").OnClick(OnCancel)
    - PropertyGrid(object)
```

**Generates:**
```cpp
ChildSlot
[
    SNew(SVerticalBox)
    + SVerticalBox::Slot()
    [
        SNew(STextBlock)
        .Text(FText::FromString("Title"))
        .Font(TitleFont)
    ]
    + SVerticalBox::Slot()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        [
            SNew(SButton)
            .OnClicked(this, &SMyWidget::OnOK)
        ]
        + SHorizontalBox::Slot()
        [
            SNew(SButton)
            .OnClicked(this, &SMyWidget::OnCancel)
        ]
    ]
    + SVerticalBox::Slot()
    [
        SNew(SPropertyGrid)
        .Object(object)
    ]
];
```

### 2.2 Common Widget Patterns
Auto-generate common patterns:

**Property Panel:**
```kn
PropertyPanel(object)
```
→ Generates full IDetailsView setup

**Toolbar:**
```kn
Toolbar:
    - Command("Save", icon: "Save", shortcut: "Ctrl+S")
    - Command("Load", icon: "Load", shortcut: "Ctrl+O")
    - Separator
    - Command("Export", icon: "Export")
```
→ Generates FToolBarBuilder with commands

**Menu:**
```kn
Menu("File"):
    - Action("New", OnNew)
    - Action("Open", OnOpen)
    - Separator
    - Action("Exit", OnExit)
```
→ Generates FMenuBuilder with actions

---

## Phase 3: Asset Editor Framework (Priority: MEDIUM)

### 3.1 Full Asset Editor Generation
```kn
@asset_editor("MaterialPreset")
editor MaterialPresetEditor:
    @toolbar
    commands:
        - Save
        - Load
        - Export
    
    @layout
    main:
        Splitter(horizontal):
            - PropertyPanel(asset).Weight(0.3)
            - Viewport(preview).Weight(0.7)
    
    @preview
    viewport:
        mesh: DefaultSphere
        lighting: ThreePoint
```

**Generates:**
- Complete FAssetEditorToolkit implementation
- Toolbar with commands
- Dockable tabs
- Property panel
- Preview viewport
- Undo/redo integration
- Auto-registration

### 3.2 Asset Importer/Exporter
```kn
@importer("MaterialPreset", extensions: ["mpreset", "json"])
importer MaterialPresetImporter:
    fn Import(file: String) -> MaterialPreset:
        let json = ReadFile(file)
        return ParseJSON(json)
    
    fn CanImport(file: String) -> Bool:
        return file.EndsWith(".mpreset")
```

**Generates:**
- UFactory implementation
- Import dialog
- File type registration
- Asset creation

---

## Phase 4: Editor Utilities (Priority: MEDIUM)

### 4.1 Editor Utility Widgets (Blutility)
```kn
@editor_utility
widget BatchRenamer:
    @input
    prefix: String
    
    @input
    suffix: String
    
    @action("Rename Selected")
    fn RenameAssets():
        for asset in GetSelectedAssets():
            asset.Rename(prefix + asset.Name + suffix)
```

**Generates:**
- UEditorUtilityWidget
- Auto-adds to Tools menu
- Progress bar
- Undo support

### 4.2 Editor Modes
```kn
@editor_mode("MaterialPainting")
mode MaterialPaintMode:
    @tool("Paint")
    paint_tool: PaintTool
    
    @tool("Erase")
    erase_tool: EraseTool
    
    @panel
    settings:
        brush_size: Float
        opacity: Float
```

**Generates:**
- FEdMode implementation
- Tool palette
- Settings panel
- Viewport rendering
- Input handling

---

## Phase 5: UE5.7 Features (Priority: LOW)

### 5.1 Enhanced Editor Subsystems
```kn
@editor_subsystem
subsystem MaterialLibrary:
    @cache
    presets: Array<MaterialPreset>
    
    fn GetPreset(name: String) -> MaterialPreset:
        return presets.Find(name)
    
    fn SavePreset(preset: MaterialPreset):
        presets.Add(preset)
```

**Generates:**
- UEditorSubsystem implementation
- Singleton access
- Persistent storage
- Event broadcasting

### 5.2 Asset Definition System
```kn
@asset_definition
define MaterialPreset:
    category: "Materials"
    color: Color(255, 128, 0)
    icon: "MaterialPreset.Icon"
    actions:
        - "Edit"
        - "Duplicate"
        - "Export"
```

**Generates:**
- Asset definition registration
- Context menu actions
- Thumbnail rendering
- Asset browser integration

---

## Phase 6: Advanced Features (Priority: LOW)

### 6.1 Custom Details Panels
```kn
@details_customization("MaterialPreset")
details MaterialPresetDetails:
    @category("Base")
    base_properties:
        - base_color
        - roughness
        - metallic
    
    @category("Advanced")
    advanced:
        - emissive
        - opacity
    
    @custom_widget("Preview")
    preview:
        MaterialPreview(asset)
```

**Generates:**
- IDetailCustomization implementation
- Custom property layout
- Custom widgets
- Live preview

### 6.2 Validation Rules
```kn
@validator("MaterialPreset")
validator MaterialPresetValidator:
    fn Validate(asset: MaterialPreset) -> ValidationResult:
        if asset.roughness < 0.0 or asset.roughness > 1.0:
            return Error("Roughness must be 0-1")
        
        if asset.metallic < 0.0 or asset.metallic > 1.0:
            return Error("Metallic must be 0-1")
        
        return Success()
```

**Generates:**
- Data validation
- Editor warnings
- Asset health checks

---

## Implementation Priority

### Week 1: Fix Current Issues
- [ ] Fix Compose() method body generation
- [ ] Fix type mappings for asset references
- [ ] Fix SLATE_EVENT detection
- [ ] Test with simple editor

### Week 2: Enhanced Slate DSL
- [ ] Widget hierarchy generation
- [ ] Common widget patterns (PropertyPanel, Toolbar, Menu)
- [ ] Slot configuration (padding, alignment, etc.)
- [ ] Test with complex UI

### Week 3: Asset Editor Framework
- [ ] Full asset editor generation
- [ ] Toolbar/menu integration
- [ ] Dockable tabs
- [ ] Test with MaterialPreset editor

### Week 4: Editor Utilities
- [ ] Editor utility widgets
- [ ] Batch operations
- [ ] Progress bars
- [ ] Test with batch tools

### Week 5: Polish & Documentation
- [ ] UE5.7 compatibility
- [ ] Documentation
- [ ] Examples
- [ ] Steering docs

---

## Success Metrics

### Code Reduction:
- **Traditional:** 2,000-5,000 lines for asset editor
- **KAIN:** 20-50 lines
- **Reduction:** 100x smaller!

### Development Speed:
- **Traditional:** 40-80 hours for asset editor
- **KAIN:** 1-2 hours
- **Speedup:** 40x faster!

### Quality:
- ✅ Compiler-verified
- ✅ Type-safe
- ✅ Follows UE5 conventions
- ✅ Production-ready

---

## Next Steps

1. **Review this plan** - Adjust priorities
2. **Fix Phase 1 issues** - Get basic editor working
3. **Implement Phase 2** - Enhanced Slate DSL
4. **Test with real editor** - MaterialPreset editor
5. **Iterate** - Add features based on feedback

**This will make KAIN the ULTIMATE tool for UE5 editor development!** 🚀

