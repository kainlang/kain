# LGUIEditor Module Infrastructure Analysis - Part 3

## 7. LevelEditor Menu Extensions

### Purpose
Adds custom toolbar buttons and menu items to the Level Editor.

### Hook Installation (LGUILevelEditorExtensions.cpp)

```cpp
// Similar pattern to ContentBrowser extensions
static FDelegateHandle LevelViewportExtenderHandle;

void FLGUILevelEditorExtensions::InstallHooks()
{
    FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    
    auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
    MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(
        &FLGUILevelEditorExtensions::OnExtendLevelEditorActorContextMenu));
    LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void FLGUILevelEditorExtensions::RemoveHooks()
{
    if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
        auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
        MenuExtenders.RemoveAll([](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
            return Delegate.GetHandle() == LevelViewportExtenderHandle;
        });
    }
}
```

### Toolbar Extension (LGUIEditorModule.cpp:230-233)

```cpp
// In StartupModule():
TSharedPtr<FExtender> toolbarExtender = MakeShareable(new FExtender);
toolbarExtender->AddToolBarExtension(
    "Play",  // Extension point
    EExtensionHook::After,
    PluginCommands,
    FToolBarExtensionDelegate::CreateRaw(this, &FLGUIEditorModule::AddEditorToolsToToolbarExtension));
LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(toolbarExtender);
```

### Toolbar Button Implementation (LGUIEditorModule.cpp:838-851)

```cpp
void FLGUIEditorModule::AddEditorToolsToToolbarExtension(FToolBarBuilder& Builder)
{
    Builder.BeginSection("LGUI");
    {
        Builder.AddComboButton(
            FUIAction(),
            FOnGetContent::CreateRaw(this, &FLGUIEditorModule::MakeEditorToolsMenu, 
                true, true, true, true, true, true),
            LOCTEXT("LGUITools", "LGUI Tools"),
            LOCTEXT("LGUIEditorTools", "LGUI Editor Tools"),
            FSlateIcon(FLGUIEditorStyle::GetStyleSetName(), "LGUIEditor.EditorTools")
        );
    }
    Builder.EndSection();
}
```

### Dynamic Menu Generation (LGUIEditorModule.cpp:853-1095)

**Key Pattern:** Data-driven menu construction with conditional visibility.

```cpp
TSharedRef<SWidget> FLGUIEditorModule::MakeEditorToolsMenu(bool InitialSetup, bool ComponentAction, 
    bool OpenWindow, bool PreviewInViewport, bool EditorCameraControl, bool Others)
{
    FMenuBuilder MenuBuilder(true, PluginCommands);
    auto commandList = FLGUIEditorCommands::Get();
    
    // Prefab section
    MenuBuilder.BeginSection("Prefab", LOCTEXT("Prefab", "Prefab"));
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("CreatePrefab", "Create Prefab"),
            LOCTEXT("CreatePrefab_Tooltip", "Use selected actor to create a new prefab"),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateStatic(&LGUIEditorTools::CreatePrefabAsset),
                FCanExecuteAction::CreateRaw(this, &FLGUIEditorModule::CanCreatePrefab),
                FGetActionCheckState(),
                FIsActionButtonVisible::CreateRaw(this, &FLGUIEditorModule::CanCreatePrefab))
        );
        
        MenuBuilder.AddMenuEntry(
            LOCTEXT("UnpackPrefab", "Unpack this Prefab"),
            LOCTEXT("UnpackPrefab_Tooltip", "Unpack the actor from related prefab asset"),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateStatic(&LGUIEditorTools::UnpackPrefab),
                FCanExecuteAction::CreateRaw(this, &FLGUIEditorModule::CanUnpackActorForPrefab),
                FGetActionCheckState(),
                FIsActionButtonVisible::CreateRaw(this, &FLGUIEditorModule::CanUnpackActorForPrefab))
        );
        
        // ... more entries
    }
    MenuBuilder.EndSection();
    
    // LGUI Actor section with submenus
    MenuBuilder.BeginSection("LGUI Actor", LOCTEXT("LGUI Actor", "LGUI Actor Operations"));
    {
        MenuBuilder.AddSubMenu(
            LOCTEXT("CreateUIElementSubMenu", "Create UI Element"),
            LOCTEXT("CreateUIElementSubMenu_Tooltip", "Create UI Element"),
            FNewMenuDelegate::CreateRaw(this, &FLGUIEditorModule::CreateUIElementSubMenu),
            FUIAction(
                FExecuteAction(),
                FCanExecuteAction(),
                FGetActionCheckState(),
                FIsActionButtonVisible::CreateRaw(this, &FLGUIEditorModule::CanCreateActor)),
            NAME_None, 
            EUserInterfaceActionType::None
        );
        
        MenuBuilder.AddSubMenu(
            LOCTEXT("ReplaceActorMenu", "Replace this by..."),
            LOCTEXT("ReplaceActorMenu_Tooltip", "Replace this actor with..."),
            FNewMenuDelegate::CreateRaw(this, &FLGUIEditorModule::ReplaceActorSubMenu),
            FUIAction(
                FExecuteAction(),
                FCanExecuteAction::CreateRaw(this, &FLGUIEditorModule::CanReplaceActor),
                FGetActionCheckState(),
                FIsActionButtonVisible::CreateRaw(this, &FLGUIEditorModule::CanReplaceActor)),
            NAME_None,
            EUserInterfaceActionType::None
        );
    }
    MenuBuilder.EndSection();
    
    // Actor actions section
    if (ComponentAction)
    {
        MenuBuilder.BeginSection("ComponentAction", LOCTEXT("ComponentAction", "Edit Component"));
        {
            MenuBuilder.AddMenuEntry(commandList.CopyComponentValues);
            MenuBuilder.AddMenuEntry(commandList.PasteComponentValues);
        }
        MenuBuilder.EndSection();
    }
    
    return MenuBuilder.MakeWidget();
}
```

### Submenu with Search (LGUIEditorModule.cpp:1178-1300)

**Advanced Pattern:** Placement mode integration with searchable actor list.

```cpp
void FLGUIEditorModule::CreateCommonActorSubMenu(FMenuBuilder& MenuBuilder)
{
    auto& PlacementMode = IPlacementModeModule::Get();
    TArray<FPlacementCategoryInfo> Categories;
    PlacementMode.GetSortedCategories(Categories);
    
    for (auto GroupDataItem : Categories)
    {
        if (GroupDataItem.UniqueHandle == FBuiltInPlacementCategories::RecentlyPlaced())
            GroupDataItem.DisplayName = LOCTEXT("RecentlyPlaced", "Recently Created");
        
        PlacementMode.RegenerateItemsForCategory(GroupDataItem.UniqueHandle);
        TArray<TSharedPtr<FPlaceableItem>> Items;
        PlacementMode.GetItemsForCategory(GroupDataItem.UniqueHandle, Items);
        
        if (Items.Num() <= 0) continue;
        
        MenuBuilder.AddSubMenu(
            GroupDataItem.DisplayName,
            FText(),
            FNewMenuDelegate::CreateLambda([Items, UniqueHandle = GroupDataItem.UniqueHandle](FMenuBuilder& MenuBuilder) {
                MenuBuilder.BeginSection(UniqueHandle);
                {
                    MenuBuilder.AddSearchWidget();  // Built-in search!
                    for (auto& Item : Items)
                    {
                        MenuBuilder.AddMenuEntry(
                            Item->DisplayName,
                            FText(),
                            FSlateIcon(),
                            FUIAction(FExecuteAction::CreateStatic(&LOCAL::CreateActor, Item))
                        );
                    }
                }
                MenuBuilder.EndSection();
            }),
            FUIAction(
                FExecuteAction(),
                FCanExecuteAction(),
                FGetActionCheckState(),
                FIsActionButtonVisible::CreateRaw(this, &FLGUIEditorModule::CanCreateActor)),
            NAME_None, 
            EUserInterfaceActionType::None
        );
    }
}
```

---

## 8. Standalone Window System

### Purpose
Dockable editor tabs that can be opened from menus.

### Tab Registration (LGUIEditorModule.cpp:242-250)

```cpp
// In StartupModule():
FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
    LGUIDynamicSpriteAtlasViewerName, 
    FOnSpawnTab::CreateRaw(this, &FLGUIEditorModule::HandleSpawnDynamicSpriteAtlasViewerTab))
    .SetDisplayName(LOCTEXT("LGUISpriteAtlasTextureViewerName", "LGUI Sprite Atlas Texture Viewer"))
    .SetMenuType(ETabSpawnerMenuType::Hidden);

FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
    LGUIPrefabSequenceTabName, 
    FOnSpawnTab::CreateRaw(this, &FLGUIEditorModule::HandleSpawnLGUIPrefabSequenceTab))
    .SetDisplayName(LOCTEXT("LGUIPrefabSequenceTabName", "LGUI Prefab Sequence"))
    .SetMenuType(ETabSpawnerMenuType::Hidden);
```

### Tab Spawner (LGUIEditorModule.cpp:624-638)

```cpp
TSharedRef<SDockTab> FLGUIEditorModule::HandleSpawnDynamicSpriteAtlasViewerTab(const FSpawnTabArgs& SpawnTabArgs)
{
    auto ResultTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
    auto TabContentWidget = SNew(SLGUIDynamicSpriteAtlasViewer, ResultTab);
    ResultTab->SetContent(TabContentWidget);
    return ResultTab;
}

TSharedRef<SDockTab> FLGUIEditorModule::HandleSpawnLGUIPrefabSequenceTab(const FSpawnTabArgs& SpawnTabArgs)
{
    auto ResultTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
    auto TabContentWidget = SNew(SLGUIPrefabSequenceEditor);
    ResultTab->SetContent(TabContentWidget);
    return ResultTab;
}
```

### Opening Tabs Programmatically

```cpp
// From anywhere in the editor:
FGlobalTabmanager::Get()->TryInvokeTab(FLGUIEditorModule::LGUIPrefabSequenceTabName);
```

---

## 9. PrefabAnimation/Sequencer Integration

### Purpose
Custom timeline/sequencer integration for animation editing.

### Sequencer Registration (LGUIEditorModule.cpp:115-119)

```cpp
// In StartupModule():
ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");

// Register custom sequence editor
SequenceEditorHandle = SequencerModule.RegisterSequenceEditor(
    ULGUIPrefabSequence::StaticClass(), 
    MakeUnique<FMovieSceneSequenceEditor_LGUIPrefabSequence>());

// Register custom track editor
LGUIMaterialTrackEditorCreateTrackEditorHandle = SequencerModule.RegisterTrackEditor(
    FOnCreateTrackEditor::CreateStatic(&FLGUIMaterialTrackEditor::CreateTrackEditor));
```

### Sequence Editor Widget (LGUIPrefabSequenceEditor.cpp:171-294)

**Complex Pattern:** Splitter with animation list + sequencer widget.

```cpp
void SLGUIPrefabSequenceEditor::Construct(const FArguments& InArgs)
{
    // Create animation list view
    SAssignNew(AnimationListView, SWidgetAnimationListView)
        .SelectionMode(ESelectionMode::Single)
        .ListItemsSource(&Animations)
        .OnGenerateRow(this, &SLGUIPrefabSequenceEditor::OnGenerateRowForAnimationListView)
        .OnSelectionChanged(this, &SLGUIPrefabSequenceEditor::OnAnimationListViewSelectionChanged)
        .OnContextMenuOpening(this, &SLGUIPrefabSequenceEditor::OnContextMenuOpening);
    
    ChildSlot
    [
        SNew(SSplitter)
        +SSplitter::Slot()
        .Value(0.2f)
        [
            SNew(SBox)
            .IsEnabled_Lambda([=, this]() { return WeakSequenceComponent.IsValid(); })
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SVerticalBox)
                    // Component selector button
                    +SVerticalBox::Slot()
                    .Padding(2)
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text_Lambda([=, this]() {
                                if (WeakSequenceComponent.IsValid())
                                {
                                    auto Actor = WeakSequenceComponent->GetOwner();
                                    if (Actor)
                                    {
                                        return FText::FromString(Actor->GetActorLabel() + TEXT(".") + 
                                            WeakSequenceComponent->GetName());
                                    }
                                }
                                return LOCTEXT("NullSequenceComponent", "Null (LGUIPrefabSequence)");
                            })
                            .ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
                            .OnClicked_Lambda([=, this]() {
                                if (WeakSequenceComponent.IsValid())
                                {
                                    GEditor->SelectNone(true, true);
                                    GEditor->SelectActor(WeakSequenceComponent->GetOwner(), true, true);
                                    GEditor->SelectComponent(WeakSequenceComponent.Get(), true, true);
                                }
                                return FReply::Handled();
                            })
                        ]
                        +SHorizontalBox::Slot()
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            PropertyCustomizationHelpers::MakeResetButton(
                                FSimpleDelegate::CreateLambda([=, this]() {
                                    AssignLGUIPrefabSequenceComponent(nullptr);
                                }),
                                LOCTEXT("ClearSequenceComponent", "Click to clear..."))
                        ]
                    ]
                    // New animation button + search
                    +SVerticalBox::Slot()
                    .Padding(2)
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .OnClicked(this, &SLGUIPrefabSequenceEditor::OnNewAnimationClicked)
                            .Text(LOCTEXT("NewAnimationButtonText", "+ Animation"))
                        ]
                        +SHorizontalBox::Slot()
                        .Padding(2.0f, 0.0f)
                        [
                            SAssignNew(SearchBoxPtr, SSearchBox)
                            .HintText(LOCTEXT("Search Animations", "Search Animations"))
                            .OnTextChanged(this, &SLGUIPrefabSequenceEditor::OnAnimationListViewSearchChanged)
                        ]
                    ]
                    // Animation list
                    +SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SScrollBorder, AnimationListView.ToSharedRef())
                        [
                            AnimationListView.ToSharedRef()
                        ]
                    ]
                ]
            ]
        ]
        +SSplitter::Slot()
        .Value(0.8f)
        [
            SAssignNew(PrefabSequenceEditor, SLGUIPrefabSequenceEditorWidget, nullptr)
        ]
    ];
    
    CreateCommandList();
    
    // Register delegates
    OnObjectsReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddSP(this, 
        &SLGUIPrefabSequenceEditor::OnObjectsReplaced);
    EditingPrefabChangedHandle = LGUIEditorTools::OnEditingPrefabChanged.AddRaw(this, 
        &SLGUIPrefabSequenceEditor::OnEditingPrefabChanged);
    OnBeforeApplyPrefabHandle = LGUIEditorTools::OnBeforeApplyPrefab.AddRaw(this, 
        &SLGUIPrefabSequenceEditor::OnBeforeApplyPrefab);
}
```

### Context Menu with Commands (LGUIPrefabSequenceEditor.cpp:457-491)

```cpp
TSharedPtr<SWidget> SLGUIPrefabSequenceEditor::OnContextMenuOpening() const
{
    FMenuBuilder MenuBuilder(true, CommandList.ToSharedRef());
    
    MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
    {
        MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
        MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
        MenuBuilder.AddMenuSeparator();
        MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
        
        // Conditional "Fix" button
        auto SelectedItems = AnimationListView->GetSelectedItems();
        if (SelectedItems.Num() == 1)
        {
            auto SelectedItem = SelectedItems[0];
            if (!SelectedItem->Animation->IsObjectReferencesGood(WeakSequenceComponent->GetOwner()))
            {
                MenuBuilder.AddMenuSeparator();
                MenuBuilder.AddMenuEntry(
                    LOCTEXT("TryFixObjectReference", "Try fix object reference"),
                    LOCTEXT("TryFixObjectReference_Tooltip", "LGUI can search target object..."),
                    FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([=, this]() {
                        SelectedItem->Animation->FixObjectReferences(WeakSequenceComponent->GetOwner());
                    }))
                );
            }
        }
    }
    MenuBuilder.EndSection();
    
    return MenuBuilder.MakeWidget();
}
```

### Command List Setup (LGUIPrefabSequenceEditor.cpp:493-511)

```cpp
void SLGUIPrefabSequenceEditor::CreateCommandList()
{
    CommandList = MakeShareable(new FUICommandList);
    
    CommandList->MapAction(
        FGenericCommands::Get().Duplicate,
        FExecuteAction::CreateSP(this, &SLGUIPrefabSequenceEditor::OnDuplicateAnimation)
    );
    
    CommandList->MapAction(
        FGenericCommands::Get().Delete,
        FExecuteAction::CreateSP(this, &SLGUIPrefabSequenceEditor::OnDeleteAnimation)
    );
    
    CommandList->MapAction(
        FGenericCommands::Get().Rename,
        FExecuteAction::CreateSP(this, &SLGUIPrefabSequenceEditor::OnRenameAnimation)
    );
}
```

---

## 10. Style System

### Purpose
Centralized icon, brush, and font management.

### Style Initialization (LGUIEditorStyle.cpp:10-24)

```cpp
TSharedPtr<FSlateStyleSet> FLGUIEditorStyle::StyleInstance = NULL;

void FLGUIEditorStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FLGUIEditorStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FLGUIEditorStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("LGUIEditorStyle"));
    return StyleSetName;
}
```

### Style Creation (LGUIEditorStyle.cpp:42-177)

```cpp
TSharedRef<FSlateStyleSet> FLGUIEditorStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("LGUIEditorStyle"));
    Style->SetContentRoot(IPluginManager::Get().FindPlugin("LGUI")->GetBaseDir() / TEXT("Resources/Icons"));
    
    // Class thumbnails (40x40)
    Style->Set("ClassThumbnail.UIBaseActor", new IMAGE_BRUSH(TEXT("UIItem_40x"), Icon40x40));
    Style->Set("ClassThumbnail.UISpriteActor", new IMAGE_BRUSH(TEXT("UISprite_40x"), Icon40x40));
    Style->Set("ClassThumbnail.UITextActor", new IMAGE_BRUSH(TEXT("UIText_40x"), Icon40x40));
    // ... 30+ more
    
    // Class icons (16x16)
    Style->Set("ClassIcon.UIBaseActor", new IMAGE_BRUSH(TEXT("UIItem_16x"), Icon16x16));
    Style->Set("ClassIcon.UISpriteActor", new IMAGE_BRUSH(TEXT("UISprite_16x"), Icon16x16));
    // ... 30+ more
    
    // Editor action icons
    Style->Set("LGUIEditor.SpriteDataAction", new IMAGE_BRUSH(TEXT("UISprite_16x"), Icon16x16));
    Style->Set("LGUIEditor.SpriteDataCreate", new IMAGE_BRUSH(TEXT("SpriteDataCreate_16x"), Icon16x16));
    Style->Set("LGUIEditor.PrefabDataAction", new IMAGE_BRUSH(TEXT("Prefab_16x"), Icon16x16));
    Style->Set("LGUIEditor.EditorTools", new IMAGE_BRUSH(TEXT("Button_Icon40"), FVector2D(40, 40)));
    
    // UI elements
    Style->Set("LGUIEditor.WhiteFrame", new BOX_BRUSH(TEXT("WhiteFrame_1x"), FVector2D(16, 16), 4.0f / 16.0f));
    Style->Set("LGUIEditor.WhiteDot", new IMAGE_BRUSH(TEXT("WhiteDot_1x"), FVector2D(1, 1)));
    Style->Set("LGUIEditor.EventGroup", new BOX_BRUSH(TEXT("EventGroup"), FMargin(15.0f/30.0f, 34.0f/40.0f, ...)));
    
    // Custom button styles
    FButtonStyle AnchorButton = FButtonStyle()
        .SetNormal(BOX_BRUSH(TEXT("AnchorData_Button_Normal"), FVector2D(16, 16), 4.0f / 16.0f))
        .SetDisabled(BOX_BRUSH(TEXT("AnchorData_Button_Normal"), FVector2D(16, 16), 4.0f / 16.0f))
        .SetHovered(BOX_BRUSH(TEXT("WhiteFrameHover_1x"), FVector2D(16, 16), 4.0f / 16.0f))
        .SetPressed(BOX_BRUSH(TEXT("WhiteFramePress_1x"), FVector2D(16, 16), 4.0f / 16.0f));
    Style->Set("AnchorButton", AnchorButton);
    
    FButtonStyle EmptyButton = FButtonStyle()
        .SetNormal(FSlateColorBrush(FColor(0, 39, 131, 0)))
        .SetHovered(FSlateColorBrush(FColor(0, 39, 131, 64)))
        .SetPressed(FSlateColorBrush(FColor(0, 39, 131, 128)));
    Style->Set("EmptyButton", EmptyButton);
    
    // Prefab marks
    Style->Set("PrefabMarkWhite", new IMAGE_BRUSH("PrefabMarkWhite_16x", Icon16x16));
    Style->Set("PrefabVariantMarkWhite", new IMAGE_BRUSH("PrefabVariantMarkWhite_16x", Icon16x16));
    Style->Set("PrefabMarkBroken", new IMAGE_BRUSH("PrefabMarkBroken_16x", Icon16x16));
    Style->Set("CanvasMark", new IMAGE_BRUSH("CanvasMark_16x", Icon16x16));
    
    return Style;
}
```

### Usage Pattern

```cpp
// In Slate widgets:
.Image(FLGUIEditorStyle::Get().GetBrush("CanvasMark"))
.ButtonStyle(FLGUIEditorStyle::Get(), "EmptyButton")

// In menu builders:
FSlateIcon(FLGUIEditorStyle::GetStyleSetName(), "LGUIEditor.EditorTools")
```

---

