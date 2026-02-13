// Copyright 2026 K-Studio. All Rights Reserved.

#include "KClonerEditor.h"
#include "KClonerData.h"
#include "KClonerPreviewScene.h"
#include "SKClonerViewport.h"
#include "SKClonerTimeline.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SWindow.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Editor.h"
#include "Engine/TextureCube.h"
#include "AssetViewerSettings.h"

#define LOCTEXT_NAMESPACE "KClonerEditor"

const FName KClonerEditorAppIdentifier = FName(TEXT("KClonerEditorApp"));
const FName PropertiesTabId = FName(TEXT("KClonerEditor_Properties"));
const FName ViewportTabId = FName(TEXT("KClonerEditor_Viewport"));
const FName TimelineTabId = FName(TEXT("KClonerEditor_Timeline"));

void FKClonerEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_KClonerEditor", "KCloner Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args)
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		
		TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(ClonerData);
		DetailsView->OnFinishedChangingProperties().AddSP(this, &FKClonerEditor::OnPropertiesChanged);

		return SNew(SDockTab)
			[
				DetailsView.ToSharedRef()
			];
	}))
	.SetDisplayName(LOCTEXT("PropertiesTab", "Properties"))
	.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FKClonerEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(TimelineTabId, FOnSpawnTab::CreateSP(this, &FKClonerEditor::SpawnTab_Timeline))
		.SetDisplayName(LOCTEXT("TimelineTab", "Timeline"))
		.SetGroup(WorkspaceMenuCategoryRef);
}

void FKClonerEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(TimelineTabId);
}

#undef LOCTEXT_NAMESPACE
#include "KClonerEditorCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "KClonerBakingUtils.h"
#include "EditorDirectories.h"
#include "DesktopPlatformModule.h"
#include "AssetToolsModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "KClonerEditor"

// ...

void FKClonerEditor::InitKClonerEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UKClonerData* InClonerData)
{
	ClonerData = InClonerData;

	FKClonerEditorCommands::Register();
	const FKClonerEditorCommands& Commands = FKClonerEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.BakeAnim,
		FExecuteAction::CreateSP(this, &FKClonerEditor::OnBakeAnim),
		FCanExecuteAction()
	);

	// SETUP PREVIEW - lights, sky, etc
	FAdvancedPreviewScene::ConstructionValues CVS;
	CVS.bDefaultLighting = true;
	CVS.LightBrightness = 1.0f;
	CVS.SkyBrightness = 1.0f;
	PreviewScene = MakeShareable(new FKClonerPreviewScene(CVS));

	// load  custom sky HDRI 
	// look at
	UTextureCube* KClonerEnvMap = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"), nullptr, LOAD_None, nullptr);

	if (KClonerEnvMap)
	{
		FPreviewSceneProfile Profile;
		Profile.EnvironmentCubeMap = KClonerEnvMap;
		Profile.bShowEnvironment = true;
		Profile.bShowFloor = false;
		Profile.EnvironmentIntensity = 1.0f;
		Profile.SkyLightIntensity = 1.2f;
		PreviewScene->UpdateScene(Profile);
	}

	PreviewScene->SetClonerData(ClonerData);

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_KClonerEditor_Layout_v3")
	->AddArea(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell(true)
		)
		->Split(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab(ViewportTabId, ETabState::OpenedTab)
			)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(PropertiesTabId, ETabState::OpenedTab)
			)
		)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.3f)
			->AddTab(TimelineTabId, ETabState::OpenedTab)
		)
	);

	InitAssetEditor(Mode, InitToolkitHost, KClonerEditorAppIdentifier, StandaloneDefaultLayout, true, true, InClonerData);
	
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FKClonerEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ToolkitCommands,
		FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& Builder)
		{
			Builder.AddToolBarButton(FKClonerEditorCommands::Get().BakeAnim);
		})
	);

	AddToolbarExtender(ToolbarExtender);
}

void FKClonerEditor::OnBakeAnim()
{
	if (!ClonerData || !PreviewScene.IsValid()) return;
	AKClonerActor* ClonerActor = PreviewScene->GetClonerActor();
	if (!ClonerActor) return;

	// Bake Settings - Default values
	float Duration = 10.0f;
	float FrameRate = 30.0f;
	
	// pop up a settings window so the user can pick length/fps
	TSharedRef<SWindow> SettingsWindow = SNew(SWindow)
		.Title(LOCTEXT("BakeSettingsTitle", "Bake Animation Settings"))
		.ClientSize(FVector2D(350, 180))
		.SupportsMinimize(false)
		.SupportsMaximize(false);
	
	TSharedPtr<SNumericEntryBox<float>> DurationBox;
	TSharedPtr<SComboBox<TSharedPtr<float>>> FPSComboBox;
	
	// FPS options
	TArray<TSharedPtr<float>> FPSOptions;
	FPSOptions.Add(MakeShared<float>(15.0f));
	FPSOptions.Add(MakeShared<float>(24.0f));
	FPSOptions.Add(MakeShared<float>(30.0f));
	FPSOptions.Add(MakeShared<float>(60.0f));
	TSharedPtr<float> SelectedFPS = FPSOptions[2]; // Default 30 FPS
	
	bool bConfirmed = false;
	
	SettingsWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DurationLabel", "Duration (seconds):"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				SAssignNew(DurationBox, SNumericEntryBox<float>)
				.Value_Lambda([&Duration]() { return Duration; })
				.OnValueChanged_Lambda([&Duration](float NewValue) { Duration = FMath::Clamp(NewValue, 0.1f, 600.0f); })
				.AllowSpin(true)
				.MinValue(0.1f)
				.MaxValue(600.0f)
				.MinSliderValue(1.0f)
				.MaxSliderValue(60.0f)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FrameRateLabel", "Frame Rate (FPS):"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				SAssignNew(FPSComboBox, SComboBox<TSharedPtr<float>>)
				.OptionsSource(&FPSOptions)
				.InitiallySelectedItem(SelectedFPS)
				.OnSelectionChanged_Lambda([&SelectedFPS](TSharedPtr<float> NewSelection, ESelectInfo::Type) { SelectedFPS = NewSelection; })
				.OnGenerateWidget_Lambda([](TSharedPtr<float> InOption)
				{
					return SNew(STextBlock).Text(FText::AsNumber(*InOption));
				})
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([&SelectedFPS]() { return SelectedFPS.IsValid() ? FText::AsNumber(*SelectedFPS) : FText::FromString(TEXT("30")); })
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelButton", "Cancel"))
				.OnClicked_Lambda([&SettingsWindow]()
				{
					SettingsWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("BakeButton", "Bake"))
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.OnClicked_Lambda([&SettingsWindow, &bConfirmed]()
				{
					bConfirmed = true;
					SettingsWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		]
	);
	
	GEditor->EditorAddModalWindow(SettingsWindow);
	
	if (!bConfirmed) return;
	
	// Get final values
	FrameRate = SelectedFPS.IsValid() ? *SelectedFPS : 30.0f;

	// 1. Ask user for Save Location
	FSaveAssetDialogConfig SaveConfig;
	SaveConfig.DefaultPath = TEXT("/Game");
	SaveConfig.DefaultAssetName = ClonerData->GetName() + TEXT("_SkelMesh");
	SaveConfig.AssetClassNames.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	SaveConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	SaveConfig.DialogTitleOverride = LOCTEXT("SaveSkeletalMesh", "Save Skeletal Mesh");

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SavePath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveConfig);

	if (SavePath.IsEmpty()) return;

	FString PackageName = FPackageName::ObjectPathToPackageName(SavePath);
	FString AssetName = FPaths::GetBaseFilename(PackageName);

	// start the bake process
	// first create the skeletal mesh (one bone per instance)
	USkeletalMesh* SkelMesh = FKClonerBakingUtils::BakeToSkeletalMesh(ClonerActor, PackageName);
	
	if (SkelMesh)
	{
		// then record the animation data for each bone
		FString AnimPackageName = PackageName + TEXT("_Anim");
		UAnimSequence* AnimSeq = FKClonerBakingUtils::BakeToAnimSequence(ClonerActor, SkelMesh, AnimPackageName, Duration, FrameRate);

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Baking Complete! Assets saved."));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to create Skeletal Mesh."));
	}
}

TSharedRef<SDockTab> FKClonerEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	ViewportWidget = SNew(SKClonerViewport, SharedThis(this), PreviewScene);

	return SNew(SDockTab)
		[
			ViewportWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FKClonerEditor::SpawnTab_Timeline(const FSpawnTabArgs& Args)
{
	TimelineWidget = SNew(SKClonerTimeline, PreviewScene);

	return SNew(SDockTab)
		[
			TimelineWidget.ToSharedRef()
		];
}

void FKClonerEditor::OnPropertiesChanged(const FPropertyChangedEvent& Event)
{
	if (PreviewScene.IsValid())
	{
		PreviewScene->SetClonerData(ClonerData);
	}
}

FName FKClonerEditor::GetToolkitFName() const
{
	return FName("KClonerEditor");
}

FText FKClonerEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "KCloner Editor");
}

FText FKClonerEditor::GetToolkitName() const
{
	return FText::FromString(ClonerData->GetName());
}

FText FKClonerEditor::GetToolkitToolTipText() const
{
	return LOCTEXT("ToolTip", "KCloner Editor");
}

FLinearColor FKClonerEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FKClonerEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("KCloner ");
}

#undef LOCTEXT_NAMESPACE
