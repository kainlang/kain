# BulkMatte - Technical Documentation

## Architecture Overview

BulkMatte is built using the KAIN language and compiles to native UE5 C++ code. The plugin consists of:

- **Runtime Module** - Material scanning and parameter editing logic
- **Editor Module** - Slate UI, Details panels, Viewports, Toolbars
- **Asset Editor** - Main editor window combining all UI components
- **Blueprint Functions** - Exposed API for scripting

## Code Structure

### Enums (4 types)
- `ParameterType` - Scalar, Vector, Texture, StaticSwitch
- `FilterMode` - All, Modified, Unmodified, Overridden
- `SortMode` - Name, Type, Value, Parent
- `BulkOperation` - Set, Add, Multiply, Clamp, Reset

### Structs (3 types)
- `MaterialInstanceInfo` - Asset path, parent, parameter count, modified flag
- `ParameterInfo` - Name, type, current/default values, override flag
- `BulkEditOperation` - Operation type, parameter name, value, affected materials

### DataTables (2 types)
- `CommonParameterData` - Common parameter names with typical ranges
- `MaterialPresetData` - Material presets (roughness, metallic, base color, etc.)

### Components (3 types)
- `MaterialScannerComponent` - Indexes Material Instances
- `ParameterEditorComponent` - Manages bulk edit operations
- `FilterManagerComponent` - Handles search and filtering

### Slate Widgets (5 types)
- `MaterialListWidget` - Tree view of materials (left panel)
- `ParameterGridWidget` - Spreadsheet of parameters (center panel)
- `BulkEditControlsWidget` - Master controls (top panel)
- `FilterBarWidget` - Search and filter controls (top bar)
- `PreviewWidget` - Material preview sphere (right panel)

### Details Panel (1 type)
- `BulkMatteSettings` - Plugin settings with sliders, color pickers, buttons

### Viewport (1 type)
- `MaterialPreviewViewport` - 3D preview with rotating sphere

### Toolbar (1 type)
- `BulkMatteToolbar` - Quick action buttons, toggles, dropdowns

### Asset Editor (1 type)
- `BulkMatteAssetEditor` - Main editor window combining all components

### Editor Module (1 type)
- `BulkMatteEditorModule` - Menu entries, toolbar buttons, context menu integration

### Blueprint Functions (10 functions)
- `scan_material_instances()` - Scan folder for Material Instances
- `get_material_parameters()` - Get parameters from a material
- `bulk_set_scalar_parameter()` - Set scalar on multiple materials
- `bulk_set_vector_parameter()` - Set vector on multiple materials
- `bulk_reset_parameter()` - Reset parameter to default
- `export_parameters_to_csv()` - Export to CSV file
- `import_parameters_from_csv()` - Import from CSV file
- `get_parameter_default_value()` - Get default value
- `calculate_parameter_statistics()` - Calculate min/max/average
- `find_materials_by_parent()` - Find materials by parent
- `find_materials_with_parameter()` - Find materials with parameter

## UE5 API Integration

### Material Instance Access
```cpp
// Get Material Instance
UMaterialInstanceConstant* MatInst = Cast<UMaterialInstanceConstant>(Asset);

// Get scalar parameter
float Value;
MatInst->GetScalarParameterValue(FName("Roughness"), Value);

// Set scalar parameter (editor only)
MatInst->SetScalarParameterValueEditorOnly(FName("Roughness"), 0.8f);

// Get vector parameter
FLinearColor Color;
MatInst->GetVectorParameterValue(FName("BaseColor"), Color);

// Set vector parameter (editor only)
MatInst->SetVectorParameterValueEditorOnly(FName("BaseColor"), FLinearColor(1,0,0,1));

// Check if parameter is overridden
bool bOverride = false;
MatInst->IsScalarParameterUsedAsAtlasPosition(FName("Roughness"), bOverride, TSoftObjectPtr<class UCurveLinearColor>());

// Apply changes
MatInst->PostEditChange();
MatInst->MarkPackageDirty();
```

### Asset Registry Scanning
```cpp
// Get Asset Registry
FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

// Find all Material Instances
FARFilter Filter;
Filter.ClassNames.Add(UMaterialInstanceConstant::StaticClass()->GetFName());
Filter.PackagePaths.Add(FName("/Game/Materials"));
Filter.bRecursivePaths = true;

TArray<FAssetData> AssetList;
AssetRegistry.GetAssets(Filter, AssetList);

// Iterate results
for (const FAssetData& Asset : AssetList)
{
    UMaterialInstanceConstant* MatInst = Cast<UMaterialInstanceConstant>(Asset.GetAsset());
    if (MatInst)
    {
        // Process material
    }
}
```

### Undo/Redo Support
```cpp
// Begin transaction
FScopedTransaction Transaction(FText::FromString("Bulk Edit Materials"));

// Modify materials
for (UMaterialInstanceConstant* MatInst : Materials)
{
    MatInst->Modify(); // Mark for undo
    MatInst->SetScalarParameterValueEditorOnly(FName("Roughness"), 0.8f);
    MatInst->PostEditChange();
}

// Transaction automatically ends when FScopedTransaction goes out of scope
```

### Context Menu Extension
```cpp
// Extend Content Browser context menu
FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

TArray<FContentBrowserMenuExtender_SelectedPaths>& MenuExtenders = 
    ContentBrowserModule.GetAllPathViewContextMenuExtenders();

MenuExtenders.Add(FContentBrowserMenuExtender_SelectedPaths::CreateLambda(
    [](const TArray<FString>& SelectedPaths)
    {
        TSharedRef<FExtender> Extender = MakeShared<FExtender>();
        
        Extender->AddMenuExtension(
            "PathContextBulkOperations",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateLambda([SelectedPaths](FMenuBuilder& MenuBuilder)
            {
                MenuBuilder.AddMenuEntry(
                    FText::FromString("Audit Materials"),
                    FText::FromString("Open BulkMatte editor for this folder"),
                    FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([SelectedPaths]()
                    {
                        // Open BulkMatte editor with selected folder
                    }))
                );
            })
        );
        
        return Extender;
    }
));
```

### Slate UI Layout
```cpp
// Main editor layout
TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)
            
            // Left panel - Material list
            + SSplitter::Slot()
            .Value(0.25f)
            [
                SNew(SBorder)
                [
                    SNew(SMaterialListWidget)
                ]
            ]
            
            // Center panel - Parameter grid
            + SSplitter::Slot()
            .Value(0.5f)
            [
                SNew(SVerticalBox)
                
                // Filter bar
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SFilterBarWidget)
                ]
                
                // Bulk controls
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBulkEditControlsWidget)
                ]
                
                // Parameter grid
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SParameterGridWidget)
                ]
            ]
            
            // Right panel - Preview
            + SSplitter::Slot()
            .Value(0.25f)
            [
                SNew(SBorder)
                [
                    SNew(SMaterialPreviewViewport)
                ]
            ]
        ];
}
```

## Performance Considerations

### Material Scanning
- Use `IAssetRegistry::GetAssets()` with filters (fast)
- Avoid loading all assets into memory (use `FAssetData`)
- Batch operations in chunks of 100-200 materials
- Show progress bar for long operations

### Parameter Access
- Cache parameter names to avoid repeated lookups
- Use `GetScalarParameterValue()` instead of loading full material
- Only load materials when actually editing
- Unload materials after editing to free memory

### UI Responsiveness
- Use `SListView` with virtualization for large lists
- Update grid in batches (don't refresh entire grid)
- Debounce search input (wait 300ms before filtering)
- Use background threads for scanning (not UI thread)

### Undo/Redo
- Limit undo stack to 50 entries (configurable)
- Clear undo stack when scanning new materials
- Use `FScopedTransaction` for atomic operations
- Don't store full material copies (only changed parameters)

## CSV Format

### Export Format
```csv
MaterialPath,ParameterName,ParameterType,CurrentValue,DefaultValue,IsOverridden
/Game/Materials/M_Metal_Inst,Roughness,Scalar,0.8,0.5,true
/Game/Materials/M_Metal_Inst,Metallic,Scalar,1.0,0.0,true
/Game/Materials/M_Metal_Inst,BaseColor,Vector,"(1.0,0.5,0.0,1.0)","(1.0,1.0,1.0,1.0)",true
/Game/Materials/M_Wood_Inst,Roughness,Scalar,0.9,0.5,true
```

### Import Format
Same as export. Only rows with `IsOverridden=true` are applied.

## Error Handling

### Material Not Found
- Show warning in UI
- Skip material in bulk operations
- Log to Output Log

### Parameter Not Found
- Show warning in UI
- Skip parameter in bulk operations
- Suggest similar parameter names

### Invalid Value
- Clamp to valid range
- Show warning in UI
- Allow user to override

### Permission Denied
- Check if material is read-only
- Show error dialog
- Skip material in bulk operations

## Testing Checklist

- [ ] Scan 1000+ materials without crash
- [ ] Bulk edit 200+ materials successfully
- [ ] Undo/redo works correctly
- [ ] CSV export/import preserves values
- [ ] Context menu appears in Content Browser
- [ ] Preview updates in real-time
- [ ] Filtering works correctly
- [ ] Sorting works correctly
- [ ] No memory leaks after long sessions
- [ ] UI remains responsive during operations

## Known Limitations

1. **Texture Parameters** - Can only view, not bulk edit (UE5 limitation)
2. **Static Switch Parameters** - Require material recompilation (slow)
3. **Material Functions** - Not supported (only Material Instances)
4. **Nested Parameters** - Only top-level parameters shown
5. **Large Projects** - Scanning 10,000+ materials may take 30+ seconds

## Future Enhancements

1. **Texture Swapping** - Bulk replace textures across materials
2. **Parameter Presets** - Save/load parameter sets
3. **Material Comparison** - Side-by-side material comparison
4. **Batch Rename** - Rename parameters across materials
5. **Parameter Templates** - Apply parameter templates to new materials
6. **History View** - See parameter change history
7. **Material Validation** - Check for common issues (missing textures, etc.)
8. **Performance Profiling** - Identify expensive materials

## Build Information

- **KAIN Version:** 1.0.0
- **UE5 Version:** 5.4+
- **Platforms:** Win64, Linux, Mac
- **Module Type:** Runtime + Editor
- **Dependencies:** Core, CoreUObject, Engine, Slate, SlateCore, UnrealEd, PropertyEditor, AssetRegistry, ContentBrowser

## Generated Code Statistics

- **Header Files:** ~15 files
- **Source Files:** ~15 files
- **Total Lines:** ~8,000 lines of C++
- **Compile Time:** ~2 minutes (clean build)
- **Binary Size:** ~500 KB

## Debugging Tips

### Enable Verbose Logging
```cpp
UE_LOG(LogBulkMatte, Verbose, TEXT("Scanning folder: %s"), *FolderPath);
```

### Check Asset Registry State
```cpp
if (!AssetRegistry.IsLoadingAssets())
{
    // Safe to scan
}
```

### Verify Material Instance Type
```cpp
if (MatInst->IsA<UMaterialInstanceConstant>())
{
    // Correct type
}
```

### Check Parameter Existence
```cpp
FMaterialParameterInfo ParamInfo(FName("Roughness"));
float Value;
if (MatInst->GetScalarParameterValue(ParamInfo, Value))
{
    // Parameter exists
}
```

## Support

For technical support, see README.md or contact support email.
