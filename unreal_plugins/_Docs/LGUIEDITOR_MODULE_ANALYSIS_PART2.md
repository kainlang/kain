# LGUIEditor Module Infrastructure Analysis - Part 2

## 4. ContentBrowser Extensions

### Purpose
Adds custom right-click menu actions to assets in the Content Browser.

### Hook Installation Pattern (LGUIContentBrowserExtensions.cpp:223-239)

```cpp
// Global delegates
FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
FDelegateHandle ContentBrowserExtenderDelegateHandle;

void FLGUIContentBrowserExtensions::InstallHooks()
{
    ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(
        &FLGUIContentBrowserExtensions_Impl::OnExtendContentBrowserAssetSelectionMenu);
    
    TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = 
        FLGUIContentBrowserExtensions_Impl::GetExtenderDelegates();
    CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
    ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FLGUIContentBrowserExtensions::RemoveHooks()
{
    if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
    {
        TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = 
            FLGUIContentBrowserExtensions_Impl::GetExtenderDelegates();
        CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { 
            return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; 
        });
    }
}
```

### Extension Logic (LGUIContentBrowserExtensions.cpp:171-211)

```cpp
static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
    TSharedRef<FExtender> Extender(new FExtender());
    
    // Filter selected assets by type
    TArray<UTexture2D*> Textures;
    TArray<ULGUIPrefab*> Prefabs;
    for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
    {
        const FAssetData& Asset = *AssetIt;
        auto AssetObject = Asset.GetAsset();
        if (auto Texture = Cast<UTexture2D>(AssetObject))
        {
            Textures.Add(Texture);
        }
        else if (auto Prefab = Cast<ULGUIPrefab>(AssetObject))
        {
            Prefabs.Add(Prefab);
        }
    }
    
    // Add submenu for textures
    if (Textures.Num() > 0)
    {
        Extender->AddMenuExtension(
            "GetAssetActions",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateStatic(&FLGUIContentBrowserExtensions_Impl::CreateSpriteActionsSubMenu, Textures));
    }
    
    // Add submenu for prefabs
    if (Prefabs.Num() > 0)
    {
        Extender->AddMenuExtension(
            "GetAssetActions",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateStatic(&FLGUIContentBrowserExtensions_Impl::CreatePrefabActionsSubMenu, Prefabs));
    }
    
    return Extender;
}
```

### Submenu Creation (LGUIContentBrowserExtensions.cpp:35-54)

```cpp
static void CreateSpriteActionsSubMenu(FMenuBuilder& MenuBuilder, TArray<UTexture2D*> SelectedAssets)
{
    MenuBuilder.AddSubMenu(
        LOCTEXT("SpriteActionsSubMenuLabel", "LGUISprite"),
        LOCTEXT("SpriteActionsSubMenuToolTip", "Sprite-related actions for this texture."),
        FNewMenuDelegate::CreateStatic(&FLGUIContentBrowserExtensions_Impl::PopulateSpriteActionsMenu, SelectedAssets),
        false,
        FSlateIcon(FLGUIEditorStyle::GetStyleSetName(), "LGUIEditor.SpriteDataAction")
    );
}
```

### Menu Actions (LGUIContentBrowserExtensions.cpp:56-120)

```cpp
static void PopulateSpriteActionsMenu(FMenuBuilder& MenuBuilder, TArray<UTexture2D*> SelectedAssets)
{
    // Action 1: Create Sprite
    MenuBuilder.AddMenuEntry(
        LOCTEXT("CreateSprite", "Create Sprite"),
        LOCTEXT("CreateSprite_Tooltip", "Create sprites from selected textures"),
        FSlateIcon(LGUIStyleSetName, "LGUIEditor.SpriteDataCreate"),
        FUIAction(FExecuteAction::CreateStatic(&LOCAL::CreateSpritesFromTextures, SelectedAssets)),
        NAME_None,
        EUserInterfaceActionType::Button);
    
    // Action 2: Configure Texture Settings
    MenuBuilder.AddMenuEntry(
        LOCTEXT("ConfigureTextureForSprites", "Apply Sprite Texture Settings"),
        LOCTEXT("ConfigureTextureForSprites_Tooltip", "Set texture for sprite"),
        FSlateIcon(LGUIStyleSetName, "LGUIEditor.SpriteDataSetting"),
        FUIAction(FExecuteAction::CreateStatic(&LOCAL::ConfigureTextureSettingsForSprites, SelectedAssets)),
        NAME_None,
        EUserInterfaceActionType::Button);
}

// Implementation
static void CreateSpritesFromTextures(TArray<UTexture2D*> Textures)
{
    FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    
    TArray<UObject*> ObjectsToSync;
    
    for (auto Texture : Textures)
    {
        // Create factory
        ULGUISpriteDataFactory* SpriteFactory = NewObject<ULGUISpriteDataFactory>();
        SpriteFactory->SpriteTexture = Texture;
        
        // Generate unique name
        FString Name, PackageName;
        const FString DefaultSuffix = TEXT("_Sprite");
        AssetToolsModule.Get().CreateUniqueAssetName(Texture->GetOutermost()->GetName(), DefaultSuffix, 
            /*out*/ PackageName, /*out*/ Name);
        const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
        
        // Create asset
        if (UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, 
            ULGUISpriteData::StaticClass(), SpriteFactory))
        {
            ObjectsToSync.Add(NewAsset);
        }
    }
    
    if (ObjectsToSync.Num() > 0)
    {
        ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
    }
}
```

---

## 5. SceneOutliner Customization

### Purpose
Adds custom columns and interactive buttons to the World Outliner.

### Column Registration (LGUIEditorModule.cpp:236-240)

```cpp
// In StartupModule():
FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
FSceneOutlinerColumnInfo ColumnInfo(
    ESceneOutlinerColumnVisibility::Visible, 
    15,  // Priority
    FCreateSceneOutlinerColumn::CreateStatic(&LGUISceneOutliner::FLGUISceneOutlinerInfoColumn::MakeInstance));
SceneOutlinerModule.RegisterDefaultColumnType<LGUISceneOutliner::FLGUISceneOutlinerInfoColumn>(ColumnInfo);
```

### Column Implementation (LGUISceneOutlinerInfoColumn.cpp:24-57)

```cpp
class FLGUISceneOutlinerInfoColumn : public ISceneOutlinerColumn
{
public:
    static TSharedRef<ISceneOutlinerColumn> MakeInstance(ISceneOutliner& SceneOutliner)
    {
        return MakeShareable(new FLGUISceneOutlinerInfoColumn(SceneOutliner));
    }
    
    FLGUISceneOutlinerInfoColumn(ISceneOutliner& InSceneOutliner)
        : WeakSceneOutliner(StaticCastSharedRef<ISceneOutliner>(InSceneOutliner.AsShared()))
    {
    }
    
    // ISceneOutlinerColumn interface
    virtual FName GetColumnID() override { return GetID(); }
    
    static FName GetID()
    {
        static FName LGUIInfoID("LGUI");
        return LGUIInfoID;
    }
    
    virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
    {
        return SHeaderRow::Column(GetColumnID())
            .DefaultLabel(LOCTEXT("LGUIColumeHeader", "LGUI"))
            .DefaultTooltip(LOCTEXT("LGUIColumeHeader_Tooltip", "LGUI functions"))
            .HAlignHeader(EHorizontalAlignment::HAlign_Center);
    }
    
    virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, 
        const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;
    
    virtual void PopulateSearchStrings(const ISceneOutlinerTreeItem& Item, 
        TArray<FString>& OutSearchStrings) const override;
    
    virtual void SortItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems, 
        const EColumnSortMode::Type SortMode) const override;
    
private:
    TWeakPtr<ISceneOutliner> WeakSceneOutliner;
};
```

### Row Widget Construction (LGUISceneOutlinerInfoColumn.cpp:59-184)

**Key Pattern:** Complex Slate widget with overlays, visibility bindings, and interactive buttons.

```cpp
const TSharedRef<SWidget> FLGUISceneOutlinerInfoColumn::ConstructRowWidget(
    FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
    AActor* actor = GetActorFromTreeItem(TreeItem);
    if (actor == nullptr || !LGUIEditorTools::IsActorCompatibleWithLGUIToolsMenu(actor))
    {
        return SNew(SBox);
    }
    
    auto bIsRootAgentActor = FLGUIPrefabEditor::ActorIsRootAgent(actor);
    TSharedRef<SLGUISceneOutlinerButton> result = SNew(SLGUISceneOutlinerButton)
        .ButtonStyle(FLGUIEditorStyle::Get(), "EmptyButton")
        .ContentPadding(FMargin(0))
        .HasDownArrow(false)
        .OnComboBoxOpened(FOnComboBoxOpened::CreateLambda([=]() {
            FLGUIEditorModule::Get().OnOutlinerSelectionChange();
        }))
        .Visibility(bIsRootAgentActor ? EVisibility::HitTestInvisible : EVisibility::Visible)
        .ButtonContent()
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot()
            [
                SNew(SOverlay)
                // Canvas icon overlay
                +SOverlay::Slot()
                [
                    SNew(SBox)
                    .WidthOverride(16)
                    .HeightOverride(16)
                    .HAlign(EHorizontalAlignment::HAlign_Center)
                    .VAlign(EVerticalAlignment::VAlign_Center)
                    [
                        SNew(SImage)
                        .Image(FLGUIEditorStyle::Get().GetBrush("CanvasMark"))
                        .Visibility(this, &FLGUISceneOutlinerInfoColumn::GetCanvasIconVisibility, TreeItem)
                        .ColorAndOpacity(this, &FLGUISceneOutlinerInfoColumn::GetDrawcallIconColor, TreeItem)
                        .ToolTipText(LOCTEXT("CanvasMarkTip", "This actor have LGUICanvas..."))
                    ]
                ]
                // Drawcall count overlay
                +SOverlay::Slot()
                [
                    SNew(SBox)
                    .WidthOverride(16)
                    .HeightOverride(16)
                    .HAlign(EHorizontalAlignment::HAlign_Left)
                    .VAlign(EVerticalAlignment::VAlign_Center)
                    [
                        SNew(STextBlock)
                        .ShadowColorAndOpacity(FLinearColor::Black)
                        .ShadowOffset(FVector2D(1, 1))
                        .Text(this, &FLGUISceneOutlinerInfoColumn::GetDrawcallInfo, TreeItem)
                        .ColorAndOpacity(FSlateColor(FLinearColor(FColor::Green)))
                        .Visibility(this, &FLGUISceneOutlinerInfoColumn::GetDrawcallCountVisibility, TreeItem)
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
            ]
            +SHorizontalBox::Slot()
            [
                SNew(SOverlay)
                // Down arrow
                +SOverlay::Slot()
                [
                    SNew(SBox)
                    .Visibility(bIsRootAgentActor ? EVisibility::Hidden : EVisibility::Visible)
                    .WidthOverride(8)
                    .HeightOverride(8)
                    [
                        SNew(SImage)
                        .Visibility(this, &FLGUISceneOutlinerInfoColumn::GetDownArrowVisibility, TreeItem)
                        .Image(FAppStyle::GetBrush("ComboButton.Arrow"))
                    ]
                ]
                // Prefab icon
                +SOverlay::Slot()
                [
                    SNew(SBox)
                    .WidthOverride(16)
                    .HeightOverride(16)
                    [
                        SNew(SImage)
                        .Image(this, &FLGUISceneOutlinerInfoColumn::GetPrefabIconImage, TreeItem)
                        .ColorAndOpacity(this, &FLGUISceneOutlinerInfoColumn::GetPrefabIconColor, TreeItem)
                        .Visibility(this, &FLGUISceneOutlinerInfoColumn::GetPrefabIconVisibility, TreeItem)
                        .ToolTipText(this, &FLGUISceneOutlinerInfoColumn::GetPrefabTooltip, TreeItem)
                    ]
                ]
            ]
        ]
        .MenuContent()
        [
            FLGUIEditorModule::Get().MakeEditorToolsMenu(false, false, false, false, false, false)
        ];
    
    result->_TreeItemActor = actor;
    return result;
}
```

### Dynamic Visibility Bindings (LGUISceneOutlinerInfoColumn.cpp:242-293)

```cpp
EVisibility FLGUISceneOutlinerInfoColumn::GetPrefabIconVisibility(FSceneOutlinerTreeItemRef TreeItem) const
{
    if (AActor* actor = GetActorFromTreeItem(TreeItem))
    {
        if (auto PrefabHelperObject = LGUIEditorTools::GetPrefabHelperObject_WhichManageThisActor(actor))
        {
            if (PrefabHelperObject->IsActorBelongsToSubPrefab(actor))
            {
                return EVisibility::Visible;
            }
            else if (PrefabHelperObject->IsActorBelongsToMissingSubPrefab(actor))
            {
                return EVisibility::Visible;
            }
        }
    }
    return EVisibility::Hidden;
}

EVisibility FLGUISceneOutlinerInfoColumn::GetCanvasIconVisibility(FSceneOutlinerTreeItemRef TreeItem) const
{
    if (AActor* actor = GetActorFromTreeItem(TreeItem))
    {
        return LGUIEditorTools::IsCanvasActor(actor) ? EVisibility::Visible : EVisibility::Hidden;
    }
    return EVisibility::Hidden;
}

FText FLGUISceneOutlinerInfoColumn::GetDrawcallInfo(FSceneOutlinerTreeItemRef TreeItem) const
{
    int drawcallCount = 0;
    if (AActor* actor = GetActorFromTreeItem(TreeItem))
    {
        drawcallCount = LGUIEditorTools::GetDrawcallCount(actor);
    }
    return FText::FromString(FString::Printf(TEXT("%d"), drawcallCount));
}
```

### Custom Sorting (LGUISceneOutlinerInfoColumn.cpp:191-240)

```cpp
void FLGUISceneOutlinerInfoColumn::SortItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems, 
    const EColumnSortMode::Type SortMode) const
{
    if (SortMode == EColumnSortMode::None) return;
    
    OutItems.Sort([this, SortMode](FSceneOutlinerTreeItemPtr A, FSceneOutlinerTreeItemPtr B)
    {
        AActor* ActorA = GetActorFromTreeItem(A.ToSharedRef());
        AActor* ActorB = GetActorFromTreeItem(B.ToSharedRef());
        
        if (ActorA != nullptr && ActorB != nullptr)
        {
            UUIItem* UIItemA = Cast<UUIItem>(ActorA->GetRootComponent());
            UUIItem* UIItemB = Cast<UUIItem>(ActorB->GetRootComponent());
            
            if (UIItemA != nullptr && UIItemB != nullptr)
            {
                if (UIItemA->GetHierarchyIndex() == UIItemB->GetHierarchyIndex())
                {
                    return ActorA->GetActorLabel().Compare(ActorB->GetActorLabel()) > 0;
                }
                else
                {
                    return UIItemA->GetHierarchyIndex() > UIItemB->GetHierarchyIndex();
                }
            }
        }
        
        // Fallback to string comparison
        auto AStr = SceneOutliner::FNumericStringWrapper(A->GetDisplayString());
        auto BStr = SceneOutliner::FNumericStringWrapper(B->GetDisplayString());
        bool result = AStr > BStr;
        
        return SortMode == EColumnSortMode::Ascending ? !result : result;
    });
}
```

---

## 6. Thumbnail Renderer Subsystem

### Purpose
Custom thumbnail rendering for asset types in Content Browser.

### Registration (LGUIEditorModule.cpp:368-372)

```cpp
// In StartupModule():
UThumbnailManager::Get().RegisterCustomRenderer(ULGUIPrefab::StaticClass(), 
    ULGUIPrefabThumbnailRenderer::StaticClass());
UThumbnailManager::Get().RegisterCustomRenderer(ULGUISpriteData::StaticClass(), 
    ULGUISpriteThumbnailRenderer::StaticClass());
UThumbnailManager::Get().RegisterCustomRenderer(ULGUISpriteData_BaseObject::StaticClass(), 
    ULGUISpriteDataBaseObjectThumbnailRenderer::StaticClass());
```

### Renderer Implementation (LGUIPrefabThumbnailRenderer.cpp)

```cpp
class ULGUIPrefabThumbnailRenderer : public UThumbnailRenderer
{
public:
    ULGUIPrefabThumbnailRenderer();
    
    // UThumbnailRenderer interface
    virtual bool CanVisualizeAsset(UObject* Object) override;
    virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, 
        FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
    virtual void BeginDestroy() override;
    
private:
    ThumbnailSceneCache<FLGUIPrefabThumbnailScene> ThumbnailScenes;
};

bool ULGUIPrefabThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
    return Cast<ULGUIPrefab>(Object) != nullptr;
}

void ULGUIPrefabThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, 
    FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
    auto prefab = Cast<ULGUIPrefab>(Object);
    if (IsValid(prefab))
    {
        // Get or create thumbnail scene
        TSharedRef<FLGUIPrefabThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(prefab->GetPathName());
        ThumbnailScene->SetPrefab(prefab);
        
        if (!ThumbnailScene->IsValidForVisualization())
            return;
        
        // Setup scene view family
        FSceneViewFamilyContext ViewFamily(
            FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), 
                FEngineShowFlags(ESFIM_Game))
            .SetTime(UThumbnailRenderer::GetTime()));
        
        ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
        ViewFamily.EngineShowFlags.MotionBlur = 0;
        
        // Create view and render
        auto View = ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height);
        RenderViewFamily(Canvas, &ViewFamily, View);
        
        // Draw overlay icon
        static FString LGUIBasePath = IPluginManager::Get().FindPlugin(TEXT("LGUI"))->GetBaseDir();
        LGUIEditorUtils::DrawThumbnailIcon(
            LGUIBasePath + (prefab->GetIsPrefabVariant() ? 
                TEXT("/Resources/Icons/PrefabVariant_40x.png") : 
                TEXT("/Resources/Icons/Prefab_40x.png")),
            X, Y, Width, Height, Canvas);
    }
}

void ULGUIPrefabThumbnailRenderer::BeginDestroy()
{
    ThumbnailScenes.Clear();
    Super::BeginDestroy();
}
```

### Thumbnail Scene (LGUIPrefabThumbnailScene.cpp)

**Pattern:** Manages a preview world for rendering the asset.

```cpp
class FLGUIPrefabThumbnailScene : public FThumbnailPreviewScene
{
public:
    FLGUIPrefabThumbnailScene();
    
    void SetPrefab(ULGUIPrefab* Prefab);
    bool IsValidForVisualization() const;
    FSceneView* CreateView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY);
    
private:
    TWeakObjectPtr<ULGUIPrefab> CurrentPrefab;
    AActor* PreviewActor;
};
```

---

